#pragma once

#include <string>
#include "Screen.h"
class Display;
class AudioOutput;
class ZXSpectrum;
class TouchKeyboard;
class Machine;
class GameLoader;
class Renderer;
class CydTouchKeyboard;
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
      resume();
    }
    void loadTape(std::string filename);
    Renderer *getRenderer() { return renderer; }
#ifdef CYD_TOUCH_KEYBOARD
    void setCydHandedness(bool rightHanded);
    void setCydTouchKeyboard(CydTouchKeyboard *keyboard, bool rightHanded);
#endif
};
