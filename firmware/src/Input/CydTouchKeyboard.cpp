#include "CydTouchKeyboard.h"
#include "CydKeyboardImages.h"
#include "CydKeyboardTheme.h"
#include "CydTouchDriver.h"
#include "../CydLayout.h"
#include "../TFT/Display.h"
#include "cyd_touch/plate_touch.h"

static const int CYD_SCREEN_W = 320;
static bool s_rightHanded = false;

static const int CYD_KEY_W = 32;
static const int CYD_BOTTOM_ROW_H = CYD_KEYBOARD_ROW_H;
static const int CYD_BOTTOM_ROW_Y = CYD_KEYBOARD_ROW_Y;
static const int CYD_MODIFIER_KEY_W = 32;
static const int CYD_MODIFIER_KEY_H = 32;
static const int CYD_MENU_KEY_W = 32;
static const int CYD_MENU_KEY_H = 32;
static const int CYD_MENU_KEY_Y = 2;
static const int CYD_MODIFIER_COLUMN_X_LEFT = 0;
static const int CYD_MODIFIER_COLUMN_X_RIGHT = CYD_SCREEN_W - CYD_MODIFIER_KEY_W;
static const int CYD_LEFT_MOD_H = CYD_BOTTOM_ROW_H;
// Modifier column (bottom to top): bottom row (26px), Sym, Caps (26px), R4–R1 (32px), Menu (32px).
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
static const int CYD_MENU_KEY_INDEX = 16;
static const int CYD_ROW_HIT_PAD_X = 6;
static const int CYD_ROW_HIT_PAD_TOP = 14;
static const int CYD_ROW_HIT_PAD_BOTTOM = 4;
static const int CYD_LEFT_STRIP_PLAYFIELD_X = CYD_MODIFIER_KEY_W + 8;

static const int CYD_KEY_COUNT = 17;
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

// R3 (row index 2) for left-handed: Enter on the left, letters shifted one slot right.
static const CydKeyRowDef kKeyRow3Left = {{{"Ent", SPECKEY_ENTER},
                                            {"A", SPECKEY_A},
                                            {"S", SPECKEY_S},
                                            {"D", SPECKEY_D},
                                            {"F", SPECKEY_F},
                                            {"G", SPECKEY_G},
                                            {"H", SPECKEY_H},
                                            {"J", SPECKEY_J},
                                            {"K", SPECKEY_K},
                                            {"L", SPECKEY_L}}};

static constexpr int8_t CYD_ROW_R3 = 2;

static const CydKeyRowDef &bottomRowDef(int8_t row, bool rightHanded)
{
  if (row == CYD_ROW_R3 && !rightHanded)
  {
    return kKeyRow3Left;
  }
  return kKeyRows[row];
}

static int16_t modifierColumnX(bool rightHanded)
{
  return rightHanded ? (int16_t)CYD_MODIFIER_COLUMN_X_RIGHT : (int16_t)CYD_MODIFIER_COLUMN_X_LEFT;
}

static void layoutKeyGeometry(bool rightHanded)
{
  s_rightHanded = rightHanded;
  const int16_t modX = modifierColumnX(rightHanded);

  for (int i = 0; i < CydTouchKeyboard::BOTTOM_KEY_COUNT; i++)
  {
    CydKeyDef &key = s_keys[CYD_BOTTOM_KEY_INDEX + i];
    key.x = (int16_t)(i * CYD_KEY_W);
    key.y = (int16_t)CYD_BOTTOM_ROW_Y;
    key.w = (int16_t)CYD_KEY_W;
    key.h = (int16_t)CYD_BOTTOM_ROW_H;
  }
  s_keys[CYD_SYM_KEY_INDEX] = {
      modX,
      (int16_t)CYD_SYM_Y,
      (int16_t)CYD_MODIFIER_KEY_W,
      (int16_t)CYD_LEFT_MOD_H,
      SPECKEY_SYMB,
      "Sym"};
  s_keys[CYD_CAPS_KEY_INDEX] = {
      modX,
      (int16_t)CYD_CAPS_Y,
      (int16_t)CYD_MODIFIER_KEY_W,
      (int16_t)CYD_LEFT_MOD_H,
      SPECKEY_SHIFT,
      "Caps"};
  s_keys[CYD_MENU_KEY_INDEX] = {
      modX,
      (int16_t)CYD_MENU_KEY_Y,
      (int16_t)CYD_MENU_KEY_W,
      (int16_t)CYD_MENU_KEY_H,
      SPECKEY_MENU,
      "Menu"};
  const char *rowLabels[CydTouchKeyboard::ROW_COUNT] = {"R1", "R2", "R3", "R4"};
  for (int slot = 0; slot < CydTouchKeyboard::ROW_COUNT; slot++)
  {
    int8_t rowIndex = (int8_t)(CydTouchKeyboard::ROW_COUNT - 1 - slot);
    s_keys[CYD_ROW_SELECT_INDEX + slot] = {
        modX,
        cydRowSelectY(slot),
        (int16_t)CYD_MODIFIER_KEY_W,
        (int16_t)CYD_MODIFIER_KEY_H,
        SPECKEY_NONE,
        rowLabels[rowIndex],
        rowIndex};
  }
}

static void layoutKeys(bool rightHanded)
{
  layoutKeyGeometry(rightHanded);
}

CydTouchKeyboard::CydTouchKeyboard(KeyEventType keyEvent, bool rightHanded, KeyPressType pressEvent)
    : m_keyEvent(keyEvent), m_pressKeyEvent(pressEvent)
{
  layoutKeys(rightHanded);
  m_keys = s_keys;
  m_keyCount = CYD_KEY_COUNT;
  selectRow(0);
}

void CydTouchKeyboard::setRightHanded(bool rightHanded)
{
  layoutKeys(rightHanded);
  m_highlightIndex = -1;
  m_lastDrawnHighlight = -1;
  m_overlayDrawn = false;
  m_modifierVisualDirty = true;
  m_bottomRowVisualDirty = true;
  m_rowSelectVisualDirty = true;
  selectRow(m_rowSelectLatched);
}

void CydTouchKeyboard::setEnabled(bool enabled)
{
  if (!enabled)
  {
    if (m_activeKey != SPECKEY_NONE || m_activeRowSelect >= 0)
    {
      releaseActiveKey();
    }
    releaseLatchedModifiers();
  }
  m_enabled = enabled;
}

void CydTouchKeyboard::invalidateOverlay()
{
  m_overlayDrawn = false;
  m_modifierVisualDirty = true;
  m_bottomRowVisualDirty = true;
  m_rowSelectVisualDirty = true;
  m_lastDrawnHighlight = -1;
}

void CydTouchKeyboard::pollTouchInput()
{
  pollTouch();
}

void CydTouchKeyboard::start()
{
  CydTouch::init();
  // Touch is polled from the Z80 runner on core 0 (see Machine::runEmulator) so it
  // does not run concurrently with the TFT SPI driver on core 1.
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
  const CydKeyRowDef &rowDef = bottomRowDef(row, s_rightHanded);
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
    if (m_modifierPassThrough)
    {
      m_keyEvent(SPECKEY_SHIFT, false);
      m_modifierPassThrough = false;
      cancelSymShift();
    }
    else if (m_capShiftLatched)
    {
      cancelCapShift();
    }
    else
    {
      latchCapShift();
    }
  }
  else if (m_activeKey == SPECKEY_SYMB)
  {
    if (m_modifierPassThrough)
    {
      m_keyEvent(SPECKEY_SYMB, false);
      m_modifierPassThrough = false;
      cancelCapShift();
    }
    else if (m_symShiftLatched)
    {
      cancelSymShift();
    }
    else
    {
      latchSymShift();
    }
  }
  else if (m_activeKey == SPECKEY_MENU)
  {
    m_keyEvent(SPECKEY_MENU, false);
    if (m_pressKeyEvent)
    {
      m_pressKeyEvent(SPECKEY_MENU);
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
  // Use s_keys geometry only — layoutKeys() clears bottom-row labels/keys.
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
  bool latched = (k.key == SPECKEY_SHIFT && m_capShiftLatched) ||
                 (k.key == SPECKEY_SYMB && m_symShiftLatched) ||
                 (k.rowSelectIndex >= 0 && k.rowSelectIndex == m_rowSelectLatched);

  CydKeyImage img;
  if (cydKeyboardImageForLabel(k.label, index, img))
  {
    tft.fillRect(k.x, k.y, k.w, k.h, TFT_BLACK);
    cydKeyboardDrawImage(tft, k.x, k.y, img);
    if (pressed || latched)
    {
      uint16_t border = pressed ? TFT_WHITE : 0xC618;
      tft.drawRect(k.x, k.y, k.w, k.h, border);
    }
    return;
  }

  uint16_t fill = pressed ? 0x52AA : (latched ? 0x4A69 : 0x3186);
  tft.fillRect(k.x, k.y, k.w, k.h, fill);
  tft.drawRect(k.x, k.y, k.w, k.h, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, fill);
  Point size = tft.measureString(k.label);
  int tx = k.x + (k.w - size.x) / 2;
  int ty = k.y + (k.h - size.y) / 2;
  tft.drawString(k.label, tx, ty);
}

void CydTouchKeyboard::drawAllKeys(Display &tft)
{
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
    if (key == SPECKEY_SHIFT && m_symShiftLatched)
    {
      m_keyEvent(SPECKEY_SHIFT, true);
      m_modifierPassThrough = true;
    }
    else if (key == SPECKEY_SYMB && m_capShiftLatched)
    {
      m_keyEvent(SPECKEY_SYMB, true);
      m_modifierPassThrough = true;
    }
    return;
  }
  if (key == SPECKEY_MENU)
  {
    m_activeKey = SPECKEY_MENU;
    m_keyEvent(SPECKEY_MENU, true);
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
  const int16_t modX = modifierColumnX(s_rightHanded);
  if (s_rightHanded)
  {
    if (x >= modX - CYD_ROW_HIT_PAD_X)
    {
      return false;
    }
  }
  else if (x < modX + CYD_MODIFIER_KEY_W + (CYD_LEFT_STRIP_PLAYFIELD_X - CYD_MODIFIER_KEY_W))
  {
    return false;
  }
  const int specX = cydSpectrumOriginX(s_rightHanded);
  const int specY = cydSpectrumOriginY();
  return x >= specX && x < specX + CYD_SPECTRUM_W && y >= specY && y < specY + CYD_SPECTRUM_H;
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
  if (hitKeyRect(m_keys[CYD_MENU_KEY_INDEX], x, y))
  {
    return CYD_MENU_KEY_INDEX;
  }
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
  if (!m_enabled)
  {
    return;
  }
  int16_t x = 0;
  int16_t y = 0;
  if (!readTouch(x, y))
  {
    cydKeyboardThemeClearPlayfieldLatch();
    if (m_activeKey != SPECKEY_NONE || m_activeRowSelect >= 0)
    {
      if (++m_touchMissReads >= 3)
      {
        releaseActiveKey();
        m_touchMissReads = 0;
      }
    }
    return;
  }
  m_touchMissReads = 0;
  int index = hitTest(x, y);
  if (index < 0)
  {
    if (isInPlayfield(x, y))
    {
      if (cydKeyboardThemeOnPlayfieldTap())
      {
        invalidateOverlay();
      }
      if (m_activeKey != SPECKEY_NONE || m_activeRowSelect >= 0)
      {
        releaseActiveKey();
      }
      return;
    }
    cydKeyboardThemeClearPlayfieldLatch();
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
}

void CydTouchKeyboard::keyboardTask(void *arg)
{
  CydTouchKeyboard *kb = (CydTouchKeyboard *)arg;
  while (true)
  {
    kb->pollTouch();
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}
