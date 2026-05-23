#pragma once

#include <string>
#include "Screen.h"
#include "../BootLog.h"

class Display;
class AudioOutput;
class ZXSpectrum;
class TouchKeyboard;
class Machine;
class GameLoader;
class Renderer;
class IFiles;
class HDMIDisplay;

class EmulatorScreen : public Screen
{
  private:
    Renderer *renderer = nullptr;
    Machine *machine = nullptr;
    GameLoader *gameLoader = nullptr;
    FILE *audioFile = nullptr;
    void triggerLoadTape();
    bool isLoading = false;
    bool isMachineReady() const;
  public:
    EmulatorScreen(Display &tft, HDMIDisplay *hdmiDisplay, AudioOutput *audioOutput, IFiles *files);
    void updateKey(SpecKeys key, uint8_t state);
    void pressKey(SpecKeys key);
    void run(std::string filename, models_enum model);
    void pause();
    void resume();
    void didAppear() {
      bootLog("emu", "didAppear — resume renderer/machine");
      resume();
    }
    void loadTape(std::string filename);
};
