#include "./Machine.h"
#include "./Renderer.h"
#include "../../AudioOutput/AudioOutput.h"
#include "../../BootLog.h"

void runnerTask(void *pvParameter)
{
  Machine *machine = (Machine *)pvParameter;
  machine->runEmulator();
}

void Machine::runEmulator() {
  unsigned long lastTime = millis();
  bool romLoadCallbackFired = false;
  uint32_t frameNum = 0;
  bootLog("z80", "emulator task running");
  while (1)
  {
    if (isRunning)
    {
      cycleCount += machine->runForFrame(audioOutput, audioFile);
      renderer->triggerDraw(machine->mem.currentScreen->data, machine->borderColors);
      if (frameNum < 5)
      {
        bootLogf("z80", "frame %lu drawn", frameNum);
      }
      frameNum++;
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
          bootLog("z80", "ROM loader entered — firing callback");
          romLoadingRoutineHitCallback();
        }
      }
      else
      {
        romLoadCallbackFired = false;
      }
    }
    else
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}


Machine::Machine(Renderer *renderer, AudioOutput *audioOutput, std::function<void()> romLoadingRoutineHitCallback)
: renderer(renderer), audioOutput(audioOutput), romLoadingRoutineHitCallback(romLoadingRoutineHitCallback) {
  bootLog("z80", "Machine ctor: allocate ZXSpectrum");
  machine = new ZXSpectrum();
  if (machine == nullptr || machine->z80Regs == nullptr || machine->mem.banks[5] == nullptr ||
      !machine->mem.banks[5]->ok())
  {
    bootLog("z80", "FATAL: ZXSpectrum memory not ready");
  }
  else
  {
    bootLogf("z80", "ZXSpectrum OK static RAM (heap=%u)", ESP.getFreeHeap());
  }
#ifndef CYD_NO_EMULATOR_MENU
  timeTravel = new TimeTravel();
#endif
}

void Machine::updateKey(SpecKeys key, uint8_t state) {
  if (isRunning) {
    machine->updateKey(key, state);
  }
}

void Machine::setup(models_enum model) {
  Serial.println("Setting up machine");
  machine->reset();
  machine->init_spectrum(model);
  machine->reset_spectrum(machine->z80Regs);
}

void Machine::start(FILE *audioFile) {
  bootLog("z80", "Machine::start — creating runner task");
  this->audioFile = audioFile;
  isRunning = true;
  BaseType_t ok = xTaskCreatePinnedToCore(runnerTask, "z80Runner", 8192, this, 5, NULL, 0);
  bootLogf("z80", "runner task create %s", ok == pdPASS ? "OK" : "FAILED");
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