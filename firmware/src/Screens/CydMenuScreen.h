#pragma once

#include <vector>
#include "Screen.h"
#include "../Files/Files.h"

class ISettings;
class EmulatorScreen;
class CydTouchKeyboard;
class Machine;

class CydMenuScreen : public Screen
{
public:
  CydMenuScreen(
      Display &tft,
      HDMIDisplay *hdmiDisplay,
      AudioOutput *audioOutput,
      IFiles *files,
      EmulatorScreen *emulatorScreen,
      Machine *machine,
      ISettings *settings,
      CydTouchKeyboard *touchKeyboard);

  void didAppear() override;
  void willDisappear() override;
  bool usesCydTouch() const override { return true; }
  void pollCydTouch() override;

private:
  enum class Action
  {
    TouchSetup,
    Volume,
    SaveSnapshot,
    LoadSnapshot,
    LoadGame,
    Poke,
    Exit,
    VolumeDown,
    VolumeUp,
    VolumeBack,
    VolumeSet,
  };

  struct MenuRow
  {
    const char *label;
    bool enabled;
    Action action;
  };

  EmulatorScreen *m_emulatorScreen = nullptr;
  Machine *m_machine = nullptr;
  ISettings *m_settings = nullptr;
  CydTouchKeyboard *m_touchKeyboard = nullptr;
  bool m_volumeMode = false;
  bool m_touchActive = false;
  int16_t m_touchX = 0;
  int16_t m_touchY = 0;
  int m_rowCount = 0;
  int m_volumeSetLevel = 0;
  MenuRow m_rows[8] = {};

  void buildMainRows();
  void draw();
  void drawVolume();
  bool hitRow(int rowIndex, int16_t x, int16_t y) const;
  int rowIndexAt(int16_t x, int16_t y) const;
  bool handleVolumeTouch(int16_t x, int16_t y);
  void runAction(Action action);
  void exitMenu();

  void showAlphabetPicker(
      const char *title,
      const char *path,
      const std::vector<std::string> &extensions,
      const std::vector<std::string> &noSdError,
      const std::vector<std::string> &noFilesError);
};
