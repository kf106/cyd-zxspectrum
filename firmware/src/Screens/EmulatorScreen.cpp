#include <Arduino.h>
#include "../TFT/TFTDisplay.h"
#include "../Emulator/spectrum.h"
#include "../Emulator/snaps.h"
#include "../AudioOutput/AudioOutput.h"
#include "../Files/Files.h"
#include "ErrorScreen.h"
#include "EmulatorScreen.h"
#include "AlphabetPicker.h"
#include "GameFilePickerScreen.h"
#include "NavigationStack.h"
#include "PokeScreen.h"
#include "SaveSnapshotScreen.h"
#include "EmulatorScreen/Renderer.h"
#include "EmulatorScreen/Machine.h"
#include "EmulatorScreen/GameLoader.h"
#include "../BootLog.h"
#include "fonts/GillSans_15_vlw.h"

const std::vector<std::string> tap_extensions = {".tap", ".tzx"};
const std::vector<std::string> no_sd_card_error = {"No SD Card", "Insert an SD Card", "to load games"};
const std::vector<std::string> no_files_error = {"No games found", "on the SD Card", "add Z80 or SNA files"};

void EmulatorScreen::triggerLoadTape()
{
  bootLog("emu", "triggerLoadTape called");
  if (isLoading || m_navigationStack == nullptr) {
    bootLogf("emu", "triggerLoadTape aborted (loading=%d nav=%p)", isLoading, m_navigationStack);
    return;
  }
  bootLog("emu", "pausing machine+renderer for tape UI");
  machine->pause();
  renderer->pause();
  if (!m_files->isAvailable())
  {
    bootLog("emu", "no storage — showing error screen");
    ErrorScreen *errorScreen = new ErrorScreen(
        no_sd_card_error,
        m_tft,
        m_hdmiDisplay,
        m_audioOutput,
        m_files);
    m_navigationStack->push(errorScreen);
    return;
  }
  drawBusy();
  FileLetterCountVector fileLetterCounts = m_files->getFileLetters("/", tap_extensions);
  if (fileLetterCounts.size() == 0)
  {
    ErrorScreen *errorScreen = new ErrorScreen(
        no_files_error,
        m_tft,
        m_hdmiDisplay,
        m_audioOutput,
        m_files);
    m_navigationStack->push(errorScreen);
    return;
  }
  AlphabetPicker<GameFilePickerScreen> *alphabetPicker = new AlphabetPicker<GameFilePickerScreen>("Select Tape File", m_files, m_tft, m_hdmiDisplay, m_audioOutput, "/", tap_extensions);
  alphabetPicker->setItems(fileLetterCounts);
  m_navigationStack->push(alphabetPicker);
}

EmulatorScreen::EmulatorScreen(Display &tft, HDMIDisplay *hdmiDisplay, AudioOutput *audioOutput, IFiles *files)
    : Screen(tft, hdmiDisplay, audioOutput, files)
{
  renderer = new Renderer(tft, audioOutput, hdmiDisplay);
  machine = new Machine(renderer, audioOutput, [&]()
                        {
    bootLog("emu", "ROM tape-loader address hit");
#ifdef CYD_SKIP_AUTO_TAPE_LOADER
    bootLog("emu", "auto tape UI disabled on CYD");
#else
    triggerLoadTape();
#endif
  });
  gameLoader = new GameLoader(machine, renderer, audioOutput);
}

bool EmulatorScreen::isMachineReady() const
{
  ZXSpectrum *speccy = machine != nullptr ? machine->getMachine() : nullptr;
  return speccy != nullptr && speccy->z80Regs != nullptr && speccy->mem.banks[5] != nullptr &&
         speccy->mem.banks[5]->ok() && speccy->mem.banks[2] != nullptr && speccy->mem.banks[2]->ok() &&
         speccy->mem.banks[0] != nullptr && speccy->mem.banks[0]->ok() && speccy->mem.rom[0] != nullptr &&
         speccy->mem.rom[0]->ok();
}

void EmulatorScreen::run(std::string filename, models_enum model)
{
  if (!isMachineReady())
  {
    bootLog("emu", "FATAL: cannot run — Spectrum memory not ready");
    m_tft.fillScreen(TFT_RED);
    m_tft.loadFont(GillSans_15_vlw);
    m_tft.setTextColor(TFT_WHITE, TFT_RED);
    m_tft.drawCenterString("Out of memory", m_tft.height() / 2 - 20);
    m_tft.drawCenterString("for 48K emulator", m_tft.height() / 2 + 10);
    return;
  }
  bootLog("emu", "run: fillScreen black");
  m_tft.fillScreen(TFT_BLACK);
  bootLog("emu", "run: renderer->start");
  renderer->start();
  auto bl = BusyLight();
  bootLog("emu", "run: machine->setup");
  machine->setup(model);
  // Run a few frames synchronously and paint once so we are not stuck on the
  // ILI9341 boot fill while the display task waits for the first triggerDraw.
  ZXSpectrum *speccy = machine->getMachine();
  bootLog("emu", "run: priming Z80 frames");
  for (int i = 0; i < 50; i++)
  {
    speccy->runForFrame(nullptr, nullptr);
  }
  bootLog("emu", "run: forceRedraw");
  renderer->forceRedraw(speccy->mem.currentScreen->data, speccy->borderColors);
  bootLog("emu", "run: forceRedraw done");
  if (filename.size() > 0)
  {
    // check for tap or tpz files
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    if (ext == "tap" || ext == "tzx")
    {
      machine->startLoading();
      gameLoader->loadTape(filename.c_str());
    }
    else
    {
      // generic loading of z80 and sna files
      Load(machine->getMachine(), filename.c_str());
    }
  }
  // audioFile = fopen("/fs/audio.raw", "wb");
  bootLog("emu", "run: machine->start");
  machine->start(audioFile);
  bootLog("emu", "run: machine task started");
}

void EmulatorScreen::pause()
{
  renderer->pause();
  machine->pause();
}

void EmulatorScreen::resume()
{
  renderer->resume();
  machine->resume();
}

void EmulatorScreen::updateKey(SpecKeys key, uint8_t state)
{
  // TODO audio capture
  // if (key == SPECKEY_0)
  // {
  //   if (audioFile)
  //   {
  //     machine->pause();
  //     vTaskDelay(1000 / portTICK_PERIOD_MS);
  //     fclose(audioFile);
  //     audioFile = NULL;
  //     Serial.printf("Audio file closed\n");
  //   }
  // }
  if (!renderer->isShowingTimeTravel)
  {
    machine->updateKey(key, state);
  }
}

void EmulatorScreen::pressKey(SpecKeys key)
{
  if (key == SPECKEY_MENU)
  {
    if (renderer->isShowingMenu) {
      renderer->isShowingMenu = false;
      renderer->forceRedraw();
      machine->resume();
    } else {
      machine->pause();
      renderer->isShowingMenu = true;
    }
  }
  if (renderer->isShowingTimeTravel)
  {
    if (key == SPECKEY_ENTER)
    {
      machine->stopTimeTravel();
      renderer->isShowingTimeTravel = false;
      renderer->forceRedraw();
      machine->resume();
    }
    else
    {
      if (key == SPECKEY_5) {
        machine->stepBack();
        renderer->forceRedraw();
      }
      if (key == SPECKEY_8) {
        machine->stepForward();
        renderer->forceRedraw();
      }
    }
  } else if (renderer->isShowingMenu) 
  {
    if (key == SPECKEY_1) {
      renderer->isShowingMenu = false;
      renderer->isShowingTimeTravel = true;
      machine->startTimeTravel();
      renderer->forceRedraw();
    }
    else if (key == SPECKEY_2) {
      renderer->isShowingMenu = false;
      // show the save snapshot UI
      m_navigationStack->push(new SaveSnapshotScreen(m_tft, m_hdmiDisplay, m_audioOutput, machine->getMachine(), m_files));
    } else if (key == SPECKEY_P) {
      renderer->isShowingMenu = false;
      m_navigationStack->push(new PokeScreen(m_tft, m_hdmiDisplay, m_audioOutput, machine->getMachine()));
    } else if (key == SPECKEY_SPACE || key == SPECKEY_ENTER) {
      renderer->isShowingMenu = false;
      machine->resume();
      renderer->forceRedraw();
    } else if (key == SPECKEY_5) {
    m_audioOutput->volumeDown();
      renderer->forceRedraw();
    } else if (key == SPECKEY_8) {
      m_audioOutput->volumeUp();
      renderer->forceRedraw();
    }
  }
}

void EmulatorScreen::loadTape(std::string filename)
{
  isLoading = true;
  renderer->resume();
  renderer->setNeedsRedraw();
  gameLoader->loadTape(filename);
  renderer->setIsLoading(false);
  renderer->setNeedsRedraw();
  machine->resume();
  isLoading = false;
}
