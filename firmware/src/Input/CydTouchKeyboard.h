#pragma once

#include <Arduino.h>
#include <functional>
#include "../Emulator/keyboard_defs.h"

class Display;

struct CydBottomKeySlot
{
  const char *label;
  SpecKeys key;
  bool withCapShift = false;
};

struct CydKeyRowDef
{
  static constexpr int BOTTOM_KEY_COUNT = 10;
  CydBottomKeySlot keys[BOTTOM_KEY_COUNT];
};

struct CydKeyDef
{
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  SpecKeys key;
  const char *label;
  int8_t rowSelectIndex = -1;
  bool withCapShift = false;
};

// Resistive touch overlay for the Cheap Yellow Display (XPT2046).
class CydTouchKeyboard
{
public:
  static constexpr int ROW_COUNT = 4;
  static constexpr int BOTTOM_KEY_COUNT = CydKeyRowDef::BOTTOM_KEY_COUNT;

  using KeyEventType = std::function<void(SpecKeys key, bool isPressed)>;
  using KeyPressType = std::function<void(SpecKeys key)>;

  CydTouchKeyboard(KeyEventType keyEvent, bool rightHanded = false, KeyPressType pressEvent = nullptr);
  void start();
  void drawOverlayIfNeeded(Display &tft);
  static void fillRectAvoidingKeys(Display &tft, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

private:
  KeyEventType m_keyEvent;
  KeyPressType m_pressKeyEvent;
  CydKeyDef *m_keys = nullptr;
  size_t m_keyCount = 0;

  bool m_capShiftLatched = false;
  bool m_symShiftLatched = false;
  bool m_extendedModePending = false;
  bool m_capShiftComboActive = false;
  SpecKeys m_activeKey = SPECKEY_NONE;
  int8_t m_activeRowSelect = -1;
  volatile int8_t m_rowSelectLatched = 0;
  volatile int16_t m_highlightIndex = -1;
  int16_t m_lastDrawnHighlight = -1;
  bool m_overlayDrawn = false;
  volatile bool m_modifierVisualDirty = false;
  volatile bool m_bottomRowVisualDirty = false;
  volatile bool m_rowSelectVisualDirty = false;

  static void keyboardTask(void *arg);
  void drawKey(Display &tft, size_t index) const;
  void drawAllKeys(Display &tft);
  void redrawBottomAndRowSelect(Display &tft);
  void redrawRowSelectKeys(Display &tft);
  void pollTouch();
  bool readTouch(int16_t &x, int16_t &y);
  bool isInPlayfield(int16_t x, int16_t y) const;
  int hitTest(int16_t x, int16_t y) const;
  void pressKeyAt(int index);
  void releaseActiveKey();
  void latchCapShift();
  void latchSymShift();
  void cancelCapShift();
  void cancelSymShift();
  void releaseLatchedModifiers();
  void enterExtendedMode();
  void cancelExtendedMode();
  void selectRow(int8_t row);
  bool isModifierKey(SpecKeys key) const;
  bool isRowSelectKey(const CydKeyDef &key) const;
  bool hitKeyRect(const CydKeyDef &k, int16_t x, int16_t y) const;
  size_t indexForKey(SpecKeys key) const;
};
