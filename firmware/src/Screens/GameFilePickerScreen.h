#pragma once

#include <algorithm>
#include "PickerScreen.h"
#include "../Files/Files.h"
#include "../Emulator/snaps.h"
#include "EmulatorScreen.h"

class GameFilePickerScreen : public PickerScreen<FileInfoPtr>
{
  public:
      GameFilePickerScreen(Display &tft, HDMIDisplay *hdmiDisplay, AudioOutput *audioOutput, IFiles *files)
      : PickerScreen("Games", tft, hdmiDisplay, audioOutput, files) {}
      void onItemSelect(FileInfoPtr item, int index) {
        Serial.printf("Selected %s\n", item->getPath().c_str());
        // locate the emaulator screen if it's on the stack already
        EmulatorScreen *emulatorScreen = nullptr;
        for (int i = 0; i < m_navigationStack->stack.size(); i++) {
          emulatorScreen = dynamic_cast<EmulatorScreen *>(m_navigationStack->stack.at(i));
          if (emulatorScreen != nullptr) {
            break;
          }
        }
        // if we found the emulator screen and if we're loading a tape file then we've been triggered due to the user starting the
        // load routine from the emulator screen. We just need to tell the emulator screen to load the tape file and pop back to it
        std::string ext = item->getExtension();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (emulatorScreen != nullptr &&
            (ext == ".tap" || ext == ".tzx" || ext == ".z80" || ext == ".sna"))
        {
          Serial.println("Loading file into existing emulator screen");
          NavigationStack *navStack = m_navigationStack;
          emulatorScreen->setDeferResume(true);
          while (navStack->stack.size() > 1 && navStack->stack.back() != emulatorScreen)
          {
            navStack->pop();
          }
          emulatorScreen->loadGameFile(item->getPath().c_str());
          return;
        }
        Serial.println("Starting new emulator screen");
        drawBusy();
        emulatorScreen = new EmulatorScreen(m_tft, m_hdmiDisplay, m_audioOutput, m_files);
        // TODO - we should pick the machine to run on - 48k or 128k
        // there's no way to know from the file name or the file contents
        emulatorScreen->run(item->getPath(), models_enum::SPECMDL_48K);
        m_navigationStack->push(emulatorScreen);
      }
      void onBack() {
        m_navigationStack->pop();
      }
};

