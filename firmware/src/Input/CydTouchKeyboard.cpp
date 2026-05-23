#include "CydTouchKeyboard.h"
#include "CydTouchDriver.h"
#include "../TFT/Display.h"
#include "../BootLog.h"
#include "../Screens/fonts/GillSans_15_vlw.h"

#ifndef CYD_SPECTRUM_TOP
#define CYD_SPECTRUM_TOP 8
#endif

static const int CYD_SCREEN_W = 320;
static bool s_rightHanded = false;

static const int CYD_KEY_W = 32;
static const int CYD_KEY_H = 26;
static const int CYD_MODIFIER_KEY_W = 32;
static const int CYD_MODIFIER_KEY_H = 32;
static const int CYD_SPECTRUM_X = (320 - 256) / 2;
static const int CYD_SPECTRUM_Y = CYD_SPECTRUM_TOP;
static const int CYD_BOTTOM_ROW_Y = 240 - CYD_KEY_H;
static const int CYD_LEFT_KEY_X = 0;
static const int CYD_LEFT_MOD_H = CYD_KEY_H;
// Left column (bottom to top): Sym, Caps (26px), then R4–R1 (32px).
static const int CYD_SYM_Y = CYD_BOTTOM_ROW_Y - CYD_LEFT_MOD_H;
static const int CYD_CAPS_Y = CYD_SYM_Y - CYD_LEFT_MOD_H;

static int16_t cydRowSelectY(int rowSlotFromCaps)
{
  return (int16_t)(CYD_CAPS_Y - (rowSlotFromCaps + 1) * CYD_MODIFIER_KEY_H);
}

static const int CYD_BOTTOM_KEY_INDEX = 0;
static const int CYD_CAPS_KEY_INDEX = 10;
static const int CYD_SYM_KEY_INDEX = 11;
static const int CYD_ROW_SELECT_INDEX = 12;
static const int CYD_ROW_HIT_PAD_X = 6;
static const int CYD_ROW_HIT_PAD_TOP = 14;
static const int CYD_ROW_HIT_PAD_BOTTOM = 4;
static const int CYD_LEFT_STRIP_PLAYFIELD_X = CYD_MODIFIER_KEY_W + 8;

static const int CYD_KEY_COUNT = 16;
static CydKeyDef s_keys[CYD_KEY_COUNT];

static const CydKeyRowDef kKeyRows[CydTouchKeyboard::ROW_COUNT] = {
    {{{"1", SPECKEY_1},
      {"2", SPECKEY_2},
      {"3", SPECKEY_3},
      {"4", SPECKEY_4},
      {"5", SPECKEY_5},
      {"6", SPECKEY_6},
      {"7", SPECKEY_7},
      {"8", SPECKEY_8},
      {"9", SPECKEY_9},
      {"0", SPECKEY_0}}},
    {{{"Q", SPECKEY_Q},
      {"W", SPECKEY_W},
      {"E", SPECKEY_E},
      {"R", SPECKEY_R},
      {"T", SPECKEY_T},
      {"Y", SPECKEY_Y},
      {"U", SPECKEY_U},
      {"I", SPECKEY_I},
      {"O", SPECKEY_O},
      {"P", SPECKEY_P}}},
    {{{"A", SPECKEY_A},
      {"S", SPECKEY_S},
      {"D", SPECKEY_D},
      {"F", SPECKEY_F},
      {"G", SPECKEY_G},
      {"H", SPECKEY_H},
      {"J", SPECKEY_J},
      {"K", SPECKEY_K},
      {"L", SPECKEY_L},
      {"Ent", SPECKEY_ENTER}}},
    {{{"Del", SPECKEY_0, true},
      {"Z", SPECKEY_Z},
      {"X", SPECKEY_X},
      {"C", SPECKEY_C},
      {"V", SPECKEY_V},
      {"B", SPECKEY_B},
      {"N", SPECKEY_N},
      {"M", SPECKEY_M},
      {"Spc", SPECKEY_SPACE},
      {"Spc", SPECKEY_SPACE}}},
};

static int16_t mirrorKeyX(int16_t x, int16_t w)
{
  if (!s_rightHanded)
  {
    return x;
  }
  return (int16_t)(CYD_SCREEN_W - x - w);
}

static void initKeyLayout()
{
  static bool initialized = false;
  if (initialized)
  {
    return;
  }
  initialized = true;
  for (int i = 0; i < CydTouchKeyboard::BOTTOM_KEY_COUNT; i++)
  {
    s_keys[CYD_BOTTOM_KEY_INDEX + i] = {
        mirrorKeyX((int16_t)(i * CYD_KEY_W), (int16_t)CYD_KEY_W),
        (int16_t)CYD_BOTTOM_ROW_Y,
        (int16_t)CYD_KEY_W,
        (int16_t)CYD_KEY_H,
        SPECKEY_NONE,
        ""};
  }
  s_keys[CYD_SYM_KEY_INDEX] = {
      mirrorKeyX((int16_t)CYD_LEFT_KEY_X, (int16_t)CYD_MODIFIER_KEY_W),
      (int16_t)CYD_SYM_Y,
      (int16_t)CYD_MODIFIER_KEY_W,
      (int16_t)CYD_LEFT_MOD_H,
      SPECKEY_SYMB,
      "Sym"};
  s_keys[CYD_CAPS_KEY_INDEX] = {
      mirrorKeyX((int16_t)CYD_LEFT_KEY_X, (int16_t)CYD_MODIFIER_KEY_W),
      (int16_t)CYD_CAPS_Y,
      (int16_t)CYD_MODIFIER_KEY_W,
      (int16_t)CYD_LEFT_MOD_H,
      SPECKEY_SHIFT,
      "Caps"};
  const char *rowLabels[CydTouchKeyboard::ROW_COUNT] = {"R1", "R2", "R3", "R4"};
  // Above Caps: R4 (nearest Caps) through R1 (top).
  for (int slot = 0; slot < CydTouchKeyboard::ROW_COUNT; slot++)
  {
    int8_t rowIndex = (int8_t)(CydTouchKeyboard::ROW_COUNT - 1 - slot);
    s_keys[CYD_ROW_SELECT_INDEX + slot] = {
        mirrorKeyX((int16_t)CYD_LEFT_KEY_X, (int16_t)CYD_MODIFIER_KEY_W),
        cydRowSelectY(slot),
        (int16_t)CYD_MODIFIER_KEY_W,
        (int16_t)CYD_MODIFIER_KEY_H,
        SPECKEY_NONE,
        rowLabels[rowIndex],
        rowIndex};
  }
}

CydTouchKeyboard::CydTouchKeyboard(KeyEventType keyEvent, bool rightHanded) : m_keyEvent(keyEvent)
{
  s_rightHanded = rightHanded;
  initKeyLayout();
  m_keys = s_keys;
  m_keyCount = CYD_KEY_COUNT;
  selectRow(0);
}

void CydTouchKeyboard::start()
{
  CydTouch::init();
  bootLog("touch", "CYD plate touch (LoM cal) init");
  xTaskCreatePinnedToCore(keyboardTask, "cydTouchKb", 4096, this, 1, nullptr, 0);
}

bool CydTouchKeyboard::isModifierKey(SpecKeys key) const
{
  return key == SPECKEY_SHIFT || key == SPECKEY_SYMB;
}

bool CydTouchKeyboard::isRowSelectKey(const CydKeyDef &key) const
{
  return key.rowSelectIndex >= 0;
}

void CydTouchKeyboard::selectRow(int8_t row)
{
  if (row < 0 || row >= ROW_COUNT)
  {
    return;
  }
  m_rowSelectLatched = row;
  const CydKeyRowDef &rowDef = kKeyRows[row];
  for (int i = 0; i < BOTTOM_KEY_COUNT; i++)
  {
    const CydBottomKeySlot &slot = rowDef.keys[i];
    CydKeyDef &key = m_keys[CYD_BOTTOM_KEY_INDEX + i];
    key.key = slot.key;
    key.label = slot.label;
    key.withCapShift = slot.withCapShift;
  }
  m_bottomRowVisualDirty = true;
  m_rowSelectVisualDirty = true;
  m_modifierVisualDirty = true;
}

size_t CydTouchKeyboard::indexForKey(SpecKeys key) const
{
  for (size_t i = 0; i < m_keyCount; i++)
  {
    if (m_keys[i].key == key)
    {
      return i;
    }
  }
  return m_keyCount;
}

void CydTouchKeyboard::latchCapShift()
{
  if (!m_capShiftLatched)
  {
    m_keyEvent(SPECKEY_SHIFT, true);
    m_capShiftLatched = true;
    m_modifierVisualDirty = true;
  }
}

void CydTouchKeyboard::latchSymShift()
{
  if (!m_symShiftLatched)
  {
    m_keyEvent(SPECKEY_SYMB, true);
    m_symShiftLatched = true;
    m_modifierVisualDirty = true;
  }
}

void CydTouchKeyboard::cancelCapShift()
{
  if (m_capShiftLatched)
  {
    m_keyEvent(SPECKEY_SHIFT, false);
    m_capShiftLatched = false;
    m_modifierVisualDirty = true;
  }
}

void CydTouchKeyboard::cancelSymShift()
{
  if (m_symShiftLatched)
  {
    m_keyEvent(SPECKEY_SYMB, false);
    m_symShiftLatched = false;
    m_modifierVisualDirty = true;
  }
}

void CydTouchKeyboard::releaseLatchedModifiers()
{
  cancelCapShift();
  cancelSymShift();
  m_extendedModePending = false;
}

void CydTouchKeyboard::enterExtendedMode()
{
  latchCapShift();
  latchSymShift();
  m_extendedModePending = true;
  m_modifierVisualDirty = true;
}

void CydTouchKeyboard::cancelExtendedMode()
{
  releaseLatchedModifiers();
}

void CydTouchKeyboard::releaseActiveKey()
{
  if (m_activeRowSelect >= 0)
  {
    m_activeRowSelect = -1;
    m_highlightIndex = -1;
    m_rowSelectVisualDirty = true;
    m_modifierVisualDirty = true;
    return;
  }
  if (m_activeKey == SPECKEY_NONE)
  {
    m_highlightIndex = -1;
    return;
  }
  if (m_activeKey == SPECKEY_SHIFT)
  {
    if (m_extendedModePending)
    {
      cancelExtendedMode();
    }
    else if (m_capShiftLatched)
    {
      cancelCapShift();
    }
    else if (m_symShiftLatched)
    {
      enterExtendedMode();
    }
    else
    {
      latchCapShift();
    }
  }
  else if (m_activeKey == SPECKEY_SYMB)
  {
    if (m_extendedModePending)
    {
      cancelExtendedMode();
    }
    else if (m_symShiftLatched)
    {
      cancelSymShift();
    }
    else if (m_capShiftLatched)
    {
      enterExtendedMode();
    }
    else
    {
      latchSymShift();
    }
  }
  else
  {
    if (m_capShiftComboActive)
    {
      m_keyEvent(SPECKEY_SHIFT, false);
      m_capShiftComboActive = false;
    }
    m_keyEvent(m_activeKey, false);
    releaseLatchedModifiers();
  }
  m_activeKey = SPECKEY_NONE;
  m_highlightIndex = -1;
}

void CydTouchKeyboard::fillRectAvoidingKeys(Display &tft, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  initKeyLayout();
  for (int16_t row = y; row < y + h; row++)
  {
    int16_t spanStart = x;
    for (int16_t col = x; col < x + w; col++)
    {
      bool inKey = false;
      for (size_t i = 0; i < CYD_KEY_COUNT; i++)
      {
        const CydKeyDef &k = s_keys[i];
        if (col >= k.x && col < k.x + k.w && row >= k.y && row < k.y + k.h)
        {
          inKey = true;
          break;
        }
      }
      if (inKey)
      {
        if (col > spanStart)
        {
          tft.fillRect(spanStart, row, col - spanStart, 1, color);
        }
        spanStart = col + 1;
      }
    }
    if (spanStart < x + w)
    {
      tft.fillRect(spanStart, row, x + w - spanStart, 1, color);
    }
  }
}

void CydTouchKeyboard::drawKey(Display &tft, size_t index) const
{
  if (index >= m_keyCount)
  {
    return;
  }
  const CydKeyDef &k = m_keys[index];
  bool pressed = m_highlightIndex == (int)index;
  bool latched = (k.key == SPECKEY_SHIFT && (m_capShiftLatched || m_extendedModePending)) ||
                 (k.key == SPECKEY_SYMB && (m_symShiftLatched || m_extendedModePending)) ||
                 (k.rowSelectIndex >= 0 && k.rowSelectIndex == m_rowSelectLatched);
  uint16_t fill = pressed ? 0x52AA : (latched ? 0x4A69 : 0x3186);
  tft.fillRect(k.x, k.y, k.w, k.h, fill);
  tft.drawRect(k.x, k.y, k.w, k.h, 0xFFFF);
  tft.setTextColor(0xFFFF, fill);
  Point size = tft.measureString(k.label);
  int tx = k.x + (k.w - size.x) / 2;
  int ty = k.y + (k.h - size.y) / 2;
  tft.drawString(k.label, tx, ty);
}

void CydTouchKeyboard::drawAllKeys(Display &tft)
{
  static bool fontLoaded = false;
  if (!fontLoaded)
  {
    tft.loadFont(GillSans_15_vlw);
    fontLoaded = true;
  }
  for (size_t i = 0; i < m_keyCount; i++)
  {
    drawKey(tft, i);
  }
}

void CydTouchKeyboard::redrawRowSelectKeys(Display &tft)
{
  for (int r = 0; r < ROW_COUNT; r++)
  {
    drawKey(tft, CYD_ROW_SELECT_INDEX + r);
  }
}

void CydTouchKeyboard::redrawBottomAndRowSelect(Display &tft)
{
  for (int i = 0; i < BOTTOM_KEY_COUNT; i++)
  {
    drawKey(tft, CYD_BOTTOM_KEY_INDEX + i);
  }
  redrawRowSelectKeys(tft);
}

void CydTouchKeyboard::drawOverlayIfNeeded(Display &tft)
{
  if (!m_overlayDrawn)
  {
    drawAllKeys(tft);
    m_overlayDrawn = true;
    m_lastDrawnHighlight = m_highlightIndex;
    m_modifierVisualDirty = false;
    m_bottomRowVisualDirty = false;
    return;
  }
  if (m_bottomRowVisualDirty)
  {
    redrawBottomAndRowSelect(tft);
    m_bottomRowVisualDirty = false;
    m_rowSelectVisualDirty = false;
    m_modifierVisualDirty = false;
  }
  else if (m_rowSelectVisualDirty)
  {
    redrawRowSelectKeys(tft);
    m_rowSelectVisualDirty = false;
  }
  else
  {
    redrawRowSelectKeys(tft);
  }
  if (m_modifierVisualDirty)
  {
    size_t capIdx = indexForKey(SPECKEY_SHIFT);
    size_t symIdx = indexForKey(SPECKEY_SYMB);
    if (capIdx < m_keyCount)
    {
      drawKey(tft, capIdx);
    }
    if (symIdx < m_keyCount)
    {
      drawKey(tft, symIdx);
    }
    m_modifierVisualDirty = false;
  }
  if (m_highlightIndex == m_lastDrawnHighlight)
  {
    return;
  }
  if (m_lastDrawnHighlight >= 0 && (size_t)m_lastDrawnHighlight < m_keyCount)
  {
    drawKey(tft, (size_t)m_lastDrawnHighlight);
  }
  if (m_highlightIndex >= 0 && (size_t)m_highlightIndex < m_keyCount)
  {
    drawKey(tft, (size_t)m_highlightIndex);
  }
  m_lastDrawnHighlight = m_highlightIndex;
}

void CydTouchKeyboard::pressKeyAt(int index)
{
  if (index < 0 || (size_t)index >= m_keyCount)
  {
    return;
  }
  const CydKeyDef &def = m_keys[index];
  m_highlightIndex = index;
  if (isRowSelectKey(def))
  {
    m_activeRowSelect = def.rowSelectIndex;
    m_activeKey = SPECKEY_NONE;
    selectRow(def.rowSelectIndex);
    return;
  }
  SpecKeys key = def.key;
  if (isModifierKey(key))
  {
    m_activeKey = key;
    return;
  }
  m_activeKey = key;
  if (def.withCapShift)
  {
    m_keyEvent(SPECKEY_SHIFT, true);
    m_capShiftComboActive = true;
  }
  m_keyEvent(key, true);
}

bool CydTouchKeyboard::isInPlayfield(int16_t x, int16_t y) const
{
  if (s_rightHanded)
  {
    if (x >= CYD_SCREEN_W - CYD_LEFT_STRIP_PLAYFIELD_X)
    {
      return false;
    }
  }
  else if (x < CYD_LEFT_STRIP_PLAYFIELD_X)
  {
    return false;
  }
  return x >= CYD_SPECTRUM_X && x < CYD_SPECTRUM_X + 256 && y >= CYD_SPECTRUM_Y && y < CYD_SPECTRUM_Y + 192;
}

bool CydTouchKeyboard::hitKeyRect(const CydKeyDef &k, int16_t x, int16_t y) const
{
  int16_t x0 = k.x;
  int16_t x1 = k.x + k.w;
  int16_t y0 = k.y;
  int16_t y1 = k.y + k.h;
  if (k.rowSelectIndex >= 0)
  {
    if (x0 > CYD_ROW_HIT_PAD_X)
    {
      x0 -= CYD_ROW_HIT_PAD_X;
    }
    else
    {
      x0 = 0;
    }
    if (x1 < 320 - CYD_ROW_HIT_PAD_X)
    {
      x1 += CYD_ROW_HIT_PAD_X;
    }
    if (k.rowSelectIndex <= 1)
    {
      y0 -= CYD_ROW_HIT_PAD_TOP;
    }
    if (k.rowSelectIndex == 0)
    {
      y0 -= CYD_ROW_HIT_PAD_TOP;
    }
    if (k.rowSelectIndex == ROW_COUNT - 1)
    {
      y1 += CYD_ROW_HIT_PAD_BOTTOM;
    }
  }
  return x >= x0 && x < x1 && y >= y0 && y < y1;
}

int CydTouchKeyboard::hitTest(int16_t x, int16_t y) const
{
  for (int slot = 0; slot < ROW_COUNT; slot++)
  {
    size_t i = CYD_ROW_SELECT_INDEX + (ROW_COUNT - 1 - slot);
    if (hitKeyRect(m_keys[i], x, y))
    {
      return (int)i;
    }
  }
  for (size_t i = m_keyCount; i-- > 0;)
  {
    if (i >= (size_t)CYD_ROW_SELECT_INDEX && i < (size_t)CYD_ROW_SELECT_INDEX + ROW_COUNT)
    {
      continue;
    }
    if (hitKeyRect(m_keys[i], x, y))
    {
      return (int)i;
    }
  }
  return -1;
}

bool CydTouchKeyboard::readTouch(int16_t &x, int16_t &y)
{
  return CydTouch::readScreen(x, y);
}

void CydTouchKeyboard::pollTouch()
{
  int16_t x = 0;
  int16_t y = 0;
  if (!readTouch(x, y))
  {
    if (m_activeKey != SPECKEY_NONE || m_activeRowSelect >= 0)
    {
      releaseActiveKey();
    }
    return;
  }
  int index = hitTest(x, y);
  if (index < 0)
  {
    if (isInPlayfield(x, y))
    {
      if (m_activeKey != SPECKEY_NONE || m_activeRowSelect >= 0)
      {
        releaseActiveKey();
      }
      return;
    }
    if (m_activeKey != SPECKEY_NONE || m_activeRowSelect >= 0)
    {
      releaseActiveKey();
    }
    return;
  }
  const CydKeyDef &hit = m_keys[index];
  if (m_highlightIndex == index &&
      (m_activeRowSelect == hit.rowSelectIndex ||
       (m_activeRowSelect < 0 && m_activeKey == hit.key)))
  {
    return;
  }
  releaseActiveKey();
  pressKeyAt(index);
#ifdef BOOT_DEBUG
  static uint8_t touchLogCount = 0;
  if (touchLogCount < 12)
  {
    bootLogf("touch", "key %d (%s) at %d,%d", index, m_keys[index].label, x, y);
    touchLogCount++;
  }
#endif
}

void CydTouchKeyboard::keyboardTask(void *arg)
{
  CydTouchKeyboard *kb = (CydTouchKeyboard *)arg;
  bootLog("touch", "CYD touch keyboard task running");
  while (true)
  {
    kb->pollTouch();
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}
