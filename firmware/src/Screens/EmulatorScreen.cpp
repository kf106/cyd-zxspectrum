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
#ifndef CYD_NO_EMULATOR_MENU
#include "PokeScreen.h"
#include "SaveSnapshotScreen.h"
#endif
#ifdef CYD_TOUCH_KEYBOARD
#include "CydMenuScreen.h"
#include "../Files/ISettings.h"
#endif
#include "EmulatorScreen/Renderer.h"
#ifdef CYD_TOUCH_KEYBOARD
#include "../Input/CydTouchKeyboard.h"
#include "../Input/CydTouchDriver.h"
#endif
#include "EmulatorScreen/Machine.h"
#include "EmulatorScreen/GameLoader.h"
#include "fonts/GillSans_15_vlw.h"

const std::vector<std::string> tap_extensions = {".tap", ".tzx"};
const std::vector<std::string> no_sd_card_error = {"No SD Card", "Insert an SD Card", "to load games"};
const std::vector<std::string> no_files_error = {"No games found", "on the SD Card", "add Z80 or SNA files"};

void EmulatorScreen::triggerLoadTape()
{
  if (isLoading || m_navigationStack == nullptr) {
    return;
  }
  machine->pause();
  renderer->pause();
  if (!m_files->isAvailable())
  {
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
#ifdef CYD_SKIP_AUTO_TAPE_LOADER
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
    m_tft.fillScreen(TFT_RED);
    m_tft.loadFont(GillSans_15_vlw);
    m_tft.setTextColor(TFT_WHITE, TFT_RED);
    m_tft.drawCenterString("Out of memory", m_tft.height() / 2 - 20);
    m_tft.drawCenterString("for 48K emulator", m_tft.height() / 2 + 10);
    return;
  }
  m_tft.fillScreen(TFT_BLACK);
  auto bl = BusyLight();
  machine->setup(model);
  renderer->start();
  Serial.println("Machine setup complete");
  // Run a few frames synchronously and paint once so we are not stuck on the
  // ILI9341 boot fill while the display task waits for the first triggerDraw.
  ZXSpectrum *speccy = machine->getMachine();
  for (int i = 0; i < 150; i++)
  {
    speccy->runForFrame(nullptr, nullptr);
  }
  uint32_t screenSum = 0;
  for (int i = 0; i < 6912; i++)
  {
    screenSum += speccy->mem.currentScreen->data[i];
  }
  Serial.printf("Boot frames complete, screen checksum %u, drawing\n", (unsigned)screenSum);
  renderer->pause();
  renderer->drawFrameSync(speccy->mem.currentScreen->data, speccy->borderColors);
#ifdef CYD_TOUCH_KEYBOARD
  if (m_cydTouchKeyboard != nullptr)
  {
    m_cydTouchKeyboard->invalidateOverlay();
  }
  renderer->drawFrameSync(speccy->mem.currentScreen->data, speccy->borderColors);
#endif
  renderer->resume();
  Serial.println("First screen draw complete");
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
  machine->start(audioFile);
  m_emulatorStarted = true;
}

void EmulatorScreen::didAppear()
{
#ifdef CYD_TOUCH_KEYBOARD
  if (renderer != nullptr)
  {
    renderer->setCydKeyboardOverlayEnabled(true);
  }
  if (m_cydTouchKeyboard != nullptr)
  {
    m_cydTouchKeyboard->invalidateOverlay();
  }
#endif
  if (m_deferResume)
  {
    m_deferResume = false;
  }
  else if (m_emulatorStarted)
  {
    resume();
#ifdef CYD_TOUCH_KEYBOARD
    refreshCydKeyboard();
#endif
  }
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
#ifndef CYD_NO_EMULATOR_MENU
  if (!renderer->isShowingTimeTravel)
#endif
  {
    machine->updateKey(key, state);
  }
}

void EmulatorScreen::willDisappear()
{
  pause();
}

void EmulatorScreen::finishGameLoad()
{
  ZXSpectrum *speccy = machine != nullptr ? machine->getMachine() : nullptr;
  if (speccy == nullptr || renderer == nullptr)
  {
    return;
  }
  renderer->setIsLoading(false);
  renderer->setLoadProgress(0);
  renderer->invalidateFramebufferCache();
  renderer->pause();
  machine->pause();
  for (int i = 0; i < 50; i++)
  {
    speccy->runForFrame(nullptr, nullptr);
  }
  for (int i = 0; i < 8; i++)
  {
    speckey[i] = 0xFF;
  }
#ifdef CYD_TOUCH_KEYBOARD
  renderer->setCydKeyboardOverlayEnabled(true);
  if (m_navigationStack != nullptr)
  {
    m_navigationStack->setCydKeyboardEnabled(true);
  }
  if (m_cydTouchKeyboard != nullptr)
  {
    m_cydTouchKeyboard->invalidateOverlay();
  }
  renderer->drawFrameSync(speccy->mem.currentScreen->data, speccy->borderColors);
  renderer->resume();
#else
  renderer->drawFrameSync(speccy->mem.currentScreen->data, speccy->borderColors);
  renderer->resume();
#endif
  machine->resume();
  isLoading = false;
}

void EmulatorScreen::loadGameFile(const char *path)
{
  if (path == nullptr || machine == nullptr)
  {
    return;
  }

  std::string ext = path;
  size_t dot = ext.find_last_of('.');
  if (dot != std::string::npos)
  {
    ext = ext.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
  }
  if (ext == ".tap" || ext == ".tzx")
  {
    isLoading = true;
    m_tft.startWrite();
    m_tft.fillScreen(TFT_BLACK);
    m_tft.endWrite();
    pause();
    renderer->waitForIdle();
    ZXSpectrum *speccy = machine->getMachine();
    if (speccy != nullptr)
    {
      // LOAD "" needs copyright/BASIC with an empty line — reset if a game was running.
      speccy->reset_spectrum(speccy->z80Regs);
      speccy->resetBorderTimeline();
      for (int i = 0; i < 8; i++)
      {
        speckey[i] = 0xFF;
      }
    }
    machine->startLoading();
    loadTape(path);
    finishGameLoad();
    return;
  }

  pause();
  renderer->waitForIdle();
  {
    auto bl = BusyLight();
    Load(machine->getMachine(), path);
  }
  finishGameLoad();
}

void EmulatorScreen::pressKey(SpecKeys key)
{
#ifdef CYD_TOUCH_KEYBOARD
  if (key == SPECKEY_MENU)
  {
    m_menuRequested = true;
    m_menuOpenDelay = 3;
    return;
  }
#endif
#ifndef CYD_NO_EMULATOR_MENU
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
#ifdef CYD_NO_TIME_TRAVEL
    if (key == SPECKEY_2) {
#else
    if (key == SPECKEY_1) {
      renderer->isShowingMenu = false;
      renderer->isShowingTimeTravel = true;
      machine->startTimeTravel();
      renderer->forceRedraw();
    } else if (key == SPECKEY_2) {
#endif
      renderer->isShowingMenu = false;
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
#else
  (void)key;
#endif
}

#ifdef CYD_TOUCH_KEYBOARD
void EmulatorScreen::openMenuIfRequested()
{
  if (m_menuOpenDelay > 0)
  {
    m_menuOpenDelay--;
    return;
  }
  if (!m_menuRequested)
  {
    return;
  }
  m_menuRequested = false;
  pause();
  renderer->setCydKeyboardOverlayEnabled(false);
  renderer->waitForIdle();
  m_navigationStack->push(new CydMenuScreen(
      m_tft,
      m_hdmiDisplay,
      m_audioOutput,
      m_files,
      this,
      machine,
      m_cydSettings,
      m_cydTouchKeyboard));
}

void EmulatorScreen::setCydHandedness(bool rightHanded)
{
  renderer->setCydHandedness(rightHanded);
}

bool EmulatorScreen::usesCydTouch() const
{
  return m_cydTouchKeyboard != nullptr;
}

void EmulatorScreen::pollCydTouch()
{
  if (m_cydTouchKeyboard != nullptr)
  {
    m_cydTouchKeyboard->pollTouchInput();
  }
}

void EmulatorScreen::tickEmulation()
{
  if (m_emulatorStarted && machine != nullptr)
  {
    machine->tickMainLoop();
  }
}

void EmulatorScreen::setCydTouchKeyboard(CydTouchKeyboard *keyboard, ISettings *settings)
{
  m_cydTouchKeyboard = keyboard;
  m_cydSettings = settings;
  if (settings != nullptr)
  {
    renderer->setCydHandedness(settings->isCydRightHanded());
    CydTouch::setCalibration(settings->getCydTouchCalibration());
  }
  renderer->setCydTouchKeyboard(keyboard);
}

void EmulatorScreen::refreshCydKeyboard()
{
  if (renderer == nullptr)
  {
    return;
  }
  renderer->pause();
  renderer->setCydKeyboardOverlayEnabled(true);
  if (m_cydTouchKeyboard != nullptr)
  {
    m_cydTouchKeyboard->invalidateOverlay();
  }
  if (machine != nullptr && machine->getMachine() != nullptr)
  {
    ZXSpectrum *speccy = machine->getMachine();
    renderer->drawFrameSync(speccy->mem.currentScreen->data, speccy->borderColors);
  }
  else
  {
    renderer->drawFrameSync();
  }
  renderer->resume();
}
#endif

void EmulatorScreen::loadTape(std::string filename)
{
  isLoading = true;
#ifdef CYD_TOUCH_KEYBOARD
  renderer->setCydKeyboardOverlayEnabled(false);
  if (m_navigationStack != nullptr)
  {
    m_navigationStack->setCydKeyboardEnabled(false);
  }
#endif
  renderer->resume();
  renderer->setIsLoading(true);
  renderer->invalidateFramebufferCache();
  renderer->setNeedsRedraw();
  gameLoader->loadTape(filename);
}
