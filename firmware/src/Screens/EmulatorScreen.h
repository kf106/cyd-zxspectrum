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
    bool m_deferResume = false;
  public:
    EmulatorScreen(Display &tft, HDMIDisplay *hdmiDisplay, AudioOutput *audioOutput, IFiles *files);
    void updateKey(SpecKeys key, uint8_t state);
    void pressKey(SpecKeys key);
    void run(std::string filename, models_enum model);
    void pause();
    void resume();
    void didAppear() override;
    void willDisappear() override;
    bool loadTape(std::string filename);
    Renderer *getRenderer() { return renderer; }
    Machine *getMachine() { return machine; }
    void loadGameFile(const char *path);
    void finishGameLoad();
    void setDeferResume(bool defer) { m_deferResume = defer; }
#ifdef CYD_TOUCH_KEYBOARD
    bool usesCydTouch() const override;
    void pollCydTouch() override;
    void setCydHandedness(bool rightHanded);
    void setCydTouchKeyboard(CydTouchKeyboard *keyboard, ISettings *settings);
    void refreshCydKeyboard();
    void openMenuIfRequested();
    void tickEmulation();
#endif
  private:
    bool m_emulatorStarted = false;
#ifdef CYD_TOUCH_KEYBOARD
    CydTouchKeyboard *m_cydTouchKeyboard = nullptr;
    ISettings *m_cydSettings = nullptr;
    volatile bool m_menuRequested = false;
    uint8_t m_menuOpenDelay = 0;
#endif
};
