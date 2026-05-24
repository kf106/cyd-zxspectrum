#include "./Machine.h"
#include "./Renderer.h"
#include "../../AudioOutput/AudioOutput.h"
#include <esp_heap_caps.h>
void runnerTask(void *pvParameter)
{
  Machine *machine = (Machine *)pvParameter;
  machine->runEmulator();
}

void Machine::runEmulator() {
  Serial.println("Z80 runner task running");
  unsigned long lastTime = millis();
  bool romLoadCallbackFired = false;
  while (1)
  {
    if (isRunning)
    {
      cycleCount += machine->runForFrame(audioOutput, audioFile);
      renderer->triggerDraw(machine->mem.currentScreen->data, machine->borderColors);
      unsigned long currentTime = millis();
      unsigned long elapsed = currentTime - lastTime;
      if (elapsed > 1000)
      {
        lastTime = currentTime;
        float cycles = cycleCount / (elapsed * 1000.0);
        float fps = renderer->getFrameCount() / (elapsed / 1000.0);
        Serial.printf("Executed at %.3FMHz cycles, frame rate=%.2f\n", cycles, fps);
        renderer->resetFrameCount();
        cycleCount = 0;
#ifndef CYD_NO_EMULATOR_MENU
        if (timeTravel->isEnabled()) {
          timeTravel->record(machine);
        }
#endif
        Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
        Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
      }
      if (machine->romLoadingRoutineHit)
      {
        if (!romLoadCallbackFired)
        {
          romLoadCallbackFired = true;
          romLoadingRoutineHitCallback();
        }
      }
      else
      {
        romLoadCallbackFired = false;
      }
      if (m_touchPollCallback)
      {
        m_touchPollCallback();
      }
    }
    else
    {
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
}


Machine::Machine(Renderer *renderer, AudioOutput *audioOutput, std::function<void()> romLoadingRoutineHitCallback)
: renderer(renderer), audioOutput(audioOutput), romLoadingRoutineHitCallback(romLoadingRoutineHitCallback) {
  machine = new ZXSpectrum();
#ifndef CYD_NO_EMULATOR_MENU
  timeTravel = new TimeTravel();
#endif
}

void Machine::updateKey(SpecKeys key, uint8_t state) {
  if (machine != nullptr) {
    machine->updateKey(key, state);
  }
}

void Machine::setup(models_enum model) {
  Serial.println("Setting up machine");
  machine->reset();
  machine->init_spectrum(model);
  machine->reset_spectrum(machine->z80Regs);
  ensureRunnerTask();
}

bool Machine::ensureRunnerTask() {
  if (m_runnerTask != nullptr)
  {
    return true;
  }
  static const uint32_t kStacks[] = {4096, 5120, 6144, 3072};
  for (uint32_t stack : kStacks)
  {
    Serial.printf("Creating Z80 runner (stack %u, free %u, largest %u)\n",
                  (unsigned)stack, (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    BaseType_t created = xTaskCreatePinnedToCore(runnerTask, "z80Runner", stack, this, 4, &m_runnerTask, 0);
    if (created == pdPASS)
    {
      Serial.printf("Z80 runner task created (stack %u)\n", (unsigned)stack);
      return true;
    }
  }
  Serial.println("Z80 runner task unavailable — will emulate on main loop");
  m_useMainLoopFallback = true;
  return false;
}

void Machine::start(FILE *audioFile) {
  this->audioFile = audioFile;
  if (!m_useMainLoopFallback)
  {
    ensureRunnerTask();
  }
  isRunning = true;
  if (m_runnerTask != nullptr)
  {
    Serial.println("Z80 runner started");
  }
  else
  {
    Serial.println("Z80 emulation on main loop");
  }
}

void Machine::tickMainLoop() {
  if (!m_useMainLoopFallback || !isRunning || m_runnerTask != nullptr)
  {
    return;
  }
  cycleCount += machine->runForFrame(audioOutput, audioFile);
  renderer->triggerDraw(machine->mem.currentScreen->data, machine->borderColors);
}

void Machine::tapKey(SpecKeys key)
{
  machine->updateKey(key, 1);
  for (int i = 0; i < 10; i++)
  {
    machine->runForFrame(nullptr, nullptr);
  }
  machine->updateKey(key, 0);
  for (int i = 0; i < 10; i++)
  {
    machine->runForFrame(nullptr, nullptr);
  }
}


void Machine::startLoading()
{
  for (int i = 0; i < 200; i++)
  {
    machine->runForFrame(nullptr, nullptr);
  }
  renderer->triggerDraw(machine->mem.currentScreen->data, machine->borderColors);
  // TODO load screenshot...
  if (machine->hwopt.hw_model == SPECMDL_48K)
  {
    tapKey(SPECKEY_J);
    machine->updateKey(SPECKEY_SYMB, 1);
    tapKey(SPECKEY_P);
    tapKey(SPECKEY_P);
    machine->updateKey(SPECKEY_SYMB, 0);
    tapKey(SPECKEY_ENTER);
  }
  else
  {
    // 128K the tape loader is first in the menu
    tapKey(SPECKEY_ENTER);
  }
  renderer->triggerDraw(machine->mem.currentScreen->data, machine->borderColors);
}