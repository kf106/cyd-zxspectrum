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
class ISettings;
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
    void didAppear() override;
    void willDisappear() override;
    void loadTape(std::string filename);
    Renderer *getRenderer() { return renderer; }
    Machine *getMachine() { return machine; }
    void loadGameFile(const char *path);
#ifdef CYD_TOUCH_KEYBOARD
    void setCydHandedness(bool rightHanded);
    void setCydTouchKeyboard(CydTouchKeyboard *keyboard, ISettings *settings);
    void openMenuIfRequested();
#endif
  private:
#ifdef CYD_TOUCH_KEYBOARD
    CydTouchKeyboard *m_cydTouchKeyboard = nullptr;
    ISettings *m_cydSettings = nullptr;
    volatile bool m_menuRequested = false;
    uint8_t m_menuOpenDelay = 0;
#endif
};
