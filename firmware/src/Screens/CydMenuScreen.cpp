#include "CydMenuScreen.h"
#include "AlphabetPicker.h"
#include "CydCalibration.h"
#include "EmulatorScreen.h"
#include "EmulatorScreen/Machine.h"
#include "ErrorScreen.h"
#include "GameFilePickerScreen.h"
#include "NavigationStack.h"
#include "PokeScreen.h"
#include "SaveSnapshotScreen.h"
#include "../Files/ISettings.h"
#include "../Input/CydTouchDriver.h"
#include "../Input/CydTouchKeyboard.h"
#include "fonts/GillSans_15_vlw.h"
#include "fonts/GillSans_25_vlw.h"
#include <vector>

static const int CYD_MENU_W = 320;
static const int CYD_MENU_H = 240;
static const int CYD_MENU_TITLE_Y = 12;
static const int CYD_MENU_FIRST_ROW_Y = 42;
static const int CYD_MENU_ROW_H = 30;
static const int CYD_MENU_ROW_GAP = 10;
static const int CYD_MENU_MARKER_R = 12;
static const int CYD_MENU_MARKER_SIZE = CYD_MENU_MARKER_R * 2 + 1;
static const int CYD_MENU_COL_X[2] = {6, 162};
static const int CYD_MENU_COL_W = 152;
static const int CYD_MENU_TEXT_OFFSET = CYD_MENU_MARKER_SIZE + 8;

static const int CYD_VOL_BACK_ROW_Y = 168;
static const int CYD_VOL_BAR_Y = 108;
static const int CYD_VOL_BAR_H = 32;
static const int CYD_VOL_BLOCK_COUNT = 10;
static const int CYD_VOL_BLOCK_GAP = 3;
static const int CYD_VOL_TRI_W = 22;
static const int CYD_VOL_TRI_PAD = 10;
static const int CYD_VOL_SIDE_MARGIN = 16;

struct CydVolLayout
{
  int barY;
  int barH;
  int blocksX;
  int blocksRight;
  int blockW;
  int blockGap;
  int leftTriX;
  int rightTriX;
  int triW;
  int triH;
};

static CydVolLayout cydVolLayout()
{
  CydVolLayout l{};
  l.barY = CYD_VOL_BAR_Y;
  l.barH = CYD_VOL_BAR_H;
  l.blockGap = CYD_VOL_BLOCK_GAP;
  l.triW = CYD_VOL_TRI_W;
  l.triH = CYD_VOL_BAR_H;
  l.leftTriX = CYD_VOL_SIDE_MARGIN;
  l.rightTriX = CYD_MENU_W - CYD_VOL_SIDE_MARGIN - l.triW;
  l.blocksX = l.leftTriX + l.triW + CYD_VOL_TRI_PAD;
  l.blocksRight = l.rightTriX - CYD_VOL_TRI_PAD;
  const int blocksW = l.blocksRight - l.blocksX;
  l.blockW = (blocksW - l.blockGap * (CYD_VOL_BLOCK_COUNT - 1)) / CYD_VOL_BLOCK_COUNT;
  return l;
}

static void drawFilledTriangle(Display &tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t fill, uint16_t outline)
{
  const Point pts[3] = {{x0, y0}, {x1, y1}, {x2, y2}};
  tft.drawFilledPolygon(std::vector<Point>(pts, pts + 3), fill);
  tft.drawPolygon(std::vector<Point>(pts, pts + 3), outline);
}

static bool pointInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh)
{
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static int volumeBlockIndexAt(int16_t x, int16_t y)
{
  const CydVolLayout l = cydVolLayout();
  if (!pointInRect(x, y, l.blocksX, l.barY, l.blocksRight - l.blocksX, l.barH))
  {
    return -1;
  }
  const int relX = x - l.blocksX;
  const int step = l.blockW + l.blockGap;
  const int index = relX / step;
  if (index < 0 || index >= CYD_VOL_BLOCK_COUNT)
  {
    return -1;
  }
  if ((relX % step) >= l.blockW)
  {
    return -1;
  }
  return index;
}

static int volumeLevelForBlock(int blockIndex)
{
  if (blockIndex < 0)
  {
    return 0;
  }
  if (blockIndex >= CYD_VOL_BLOCK_COUNT)
  {
    return 10;
  }
  return blockIndex + 1;
}

static int menuItemRowY(int itemIndex)
{
  const int row = itemIndex / 2;
  return CYD_MENU_FIRST_ROW_Y + row * (CYD_MENU_ROW_H + CYD_MENU_ROW_GAP);
}

static int menuItemColX(int itemIndex)
{
  return CYD_MENU_COL_X[itemIndex % 2];
}

static void menuItemBounds(int itemIndex, int &x, int &y, int &w, int &h)
{
  x = menuItemColX(itemIndex);
  y = menuItemRowY(itemIndex);
  w = CYD_MENU_COL_W;
  h = CYD_MENU_ROW_H;
}

static void drawMenuMarker(Display &tft, int itemX, int rowY, bool enabled)
{
  const int markerCy = rowY + CYD_MENU_ROW_H / 2;
  const uint16_t markerColor = enabled ? TFT_WHITE : 0x4208;
  tft.fillRect(
      itemX,
      markerCy - CYD_MENU_MARKER_R,
      CYD_MENU_MARKER_SIZE,
      CYD_MENU_MARKER_SIZE,
      markerColor);
}

static const std::vector<std::string> kGameExtensions = {".z80", ".sna", ".tap", ".tzx"};
static const std::vector<std::string> kSnapshotExtensions = {".z80"};

CydMenuScreen::CydMenuScreen(
    Display &tft,
    HDMIDisplay *hdmiDisplay,
    AudioOutput *audioOutput,
    IFiles *files,
    EmulatorScreen *emulatorScreen,
    Machine *machine,
    ISettings *settings,
    CydTouchKeyboard *touchKeyboard)
    : Screen(tft, hdmiDisplay, audioOutput, files),
      m_emulatorScreen(emulatorScreen),
      m_machine(machine),
      m_settings(settings),
      m_touchKeyboard(touchKeyboard)
{
}

void CydMenuScreen::didAppear()
{
  m_volumeMode = false;
  m_touchActive = false;
  buildMainRows();
  if (m_touchKeyboard != nullptr)
  {
    m_touchKeyboard->setEnabled(false);
  }
  draw();
}

void CydMenuScreen::willDisappear()
{
  if (m_touchKeyboard != nullptr)
  {
    m_touchKeyboard->setEnabled(true);
  }
}

void CydMenuScreen::buildMainRows()
{
  const bool sdReady = m_files != nullptr && m_files->isAvailable(StorageType::SD);
  m_rowCount = 0;
  m_rows[m_rowCount++] = {"Recalibrate", true, Action::TouchSetup};
  m_rows[m_rowCount++] = {"Volume", true, Action::Volume};
  m_rows[m_rowCount++] = {"Save snapshot", sdReady, Action::SaveSnapshot};
  m_rows[m_rowCount++] = {"Load snapshot", sdReady, Action::LoadSnapshot};
  m_rows[m_rowCount++] = {"Load game / tape", sdReady, Action::LoadGame};
  m_rows[m_rowCount++] = {"Poke", true, Action::Poke};
  m_rows[m_rowCount++] = {"Exit to emulator", true, Action::Exit};
}

void CydMenuScreen::draw()
{
  m_tft.startWrite();
  m_tft.fillScreen(TFT_BLACK);
  m_tft.loadFont(GillSans_25_vlw);
  m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
  const char *title = m_volumeMode ? "Volume" : "Menu";
  Point titleSize = m_tft.measureString(title);
  m_tft.drawString(title, (CYD_MENU_W - titleSize.x) / 2, CYD_MENU_TITLE_Y);

  m_tft.loadFont(GillSans_15_vlw);
  if (m_volumeMode)
  {
    drawVolume();
    m_tft.endWrite();
    return;
  }

  for (int i = 0; i < m_rowCount; i++)
  {
    const int itemX = menuItemColX(i);
    const int rowY = menuItemRowY(i);
    drawMenuMarker(m_tft, itemX, rowY, m_rows[i].enabled);
    m_tft.setTextColor(m_rows[i].enabled ? TFT_WHITE : 0x8410, TFT_BLACK);
    m_tft.drawString(m_rows[i].label, itemX + CYD_MENU_TEXT_OFFSET, rowY + 7);
  }

  m_tft.endWrite();
}

void CydMenuScreen::drawVolume()
{
  if (m_audioOutput == nullptr)
  {
    return;
  }

  const CydVolLayout l = cydVolLayout();
  const int volume = m_audioOutput->getVolume();
  char level[16];
  snprintf(level, sizeof(level), "Level %d / 10", volume);
  Point levelSize = m_tft.measureString(level);
  m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
  m_tft.drawString(level, (CYD_MENU_W - levelSize.x) / 2, l.barY - 24);

  const uint16_t triFill = 0x3186;
  drawFilledTriangle(
      m_tft,
      l.leftTriX,
      l.barY + l.triH / 2,
      l.leftTriX + l.triW,
      l.barY,
      l.leftTriX + l.triW,
      l.barY + l.triH,
      triFill,
      TFT_WHITE);
  drawFilledTriangle(
      m_tft,
      l.rightTriX + l.triW,
      l.barY + l.triH / 2,
      l.rightTriX,
      l.barY,
      l.rightTriX,
      l.barY + l.triH,
      triFill,
      TFT_WHITE);

  for (int i = 0; i < CYD_VOL_BLOCK_COUNT; i++)
  {
    const int bx = l.blocksX + i * (l.blockW + l.blockGap);
    const uint16_t fill = (volume >= volumeLevelForBlock(i)) ? 0x0340 : 0x3186;
    m_tft.fillRect(bx, l.barY, l.blockW, l.barH, fill);
    m_tft.drawRect(bx, l.barY, l.blockW, l.barH, TFT_WHITE);
  }

  const int backRowY = CYD_VOL_BACK_ROW_Y;
  drawMenuMarker(m_tft, CYD_MENU_COL_X[0], backRowY, true);
  m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
  m_tft.drawString("Back to menu", CYD_MENU_COL_X[0] + CYD_MENU_TEXT_OFFSET, backRowY + 7);
}

static bool volumeBackRowHit(int16_t x, int16_t y)
{
  const int itemX = CYD_MENU_COL_X[0];
  const int rowY = CYD_VOL_BACK_ROW_Y;
  const int hitPad = 6;
  return x >= itemX - hitPad && x < itemX + CYD_MENU_COL_W + hitPad && y >= rowY - hitPad &&
         y < rowY + CYD_MENU_ROW_H + hitPad;
}

bool CydMenuScreen::handleVolumeTouch(int16_t x, int16_t y)
{
  const CydVolLayout l = cydVolLayout();

  if (volumeBackRowHit(x, y))
  {
    playKeyClick();
    runAction(Action::VolumeBack);
    return true;
  }

  if (pointInRect(x, y, l.leftTriX, l.barY, l.triW, l.triH))
  {
    playKeyClick();
    runAction(Action::VolumeDown);
    return true;
  }

  if (pointInRect(x, y, l.rightTriX, l.barY, l.triW, l.triH))
  {
    playKeyClick();
    runAction(Action::VolumeUp);
    return true;
  }

  const int blockIndex = volumeBlockIndexAt(x, y);
  if (blockIndex >= 0)
  {
    playKeyClick();
    m_volumeSetLevel = volumeLevelForBlock(blockIndex);
    runAction(Action::VolumeSet);
    return true;
  }

  return false;
}

bool CydMenuScreen::hitRow(int itemIndex, int16_t x, int16_t y) const
{
  if (itemIndex < 0 || itemIndex >= m_rowCount || m_volumeMode)
  {
    return false;
  }
  int itemX = 0;
  int rowY = 0;
  int w = 0;
  int h = 0;
  menuItemBounds(itemIndex, itemX, rowY, w, h);
  const int hitPad = 6;
  return x >= itemX - hitPad && x < itemX + w + hitPad && y >= rowY - hitPad && y < rowY + h + hitPad;
}

int CydMenuScreen::rowIndexAt(int16_t x, int16_t y) const
{
  if (m_volumeMode)
  {
    return -1;
  }
  for (int i = 0; i < m_rowCount; i++)
  {
    if (hitRow(i, x, y))
    {
      return i;
    }
  }
  return -1;
}

void CydMenuScreen::pollCydTouch()
{
  int16_t x = 0;
  int16_t y = 0;
  const bool touching = CydTouch::readScreen(x, y);

  if (touching)
  {
    m_touchActive = true;
    m_touchX = x;
    m_touchY = y;
    return;
  }

  if (!m_touchActive)
  {
    return;
  }
  m_touchActive = false;

  if (m_volumeMode)
  {
    handleVolumeTouch(m_touchX, m_touchY);
    return;
  }

  const int row = rowIndexAt(m_touchX, m_touchY);
  if (row < 0)
  {
    return;
  }

  if (!m_rows[row].enabled)
  {
    playErrorBeep();
    return;
  }

  playKeyClick();
  runAction(m_rows[row].action);
}

void CydMenuScreen::runAction(Action action)
{
  switch (action)
  {
  case Action::TouchSetup:
    CydCalibration::runRecalibration(m_tft, *m_settings);
    {
      const bool rightHanded = CydCalibration::runHandednessSelection(m_tft, *m_settings);
      if (m_emulatorScreen != nullptr)
      {
        m_emulatorScreen->setCydHandedness(rightHanded);
      }
      if (m_touchKeyboard != nullptr)
      {
        m_touchKeyboard->setRightHanded(rightHanded);
      }
    }
    m_touchActive = false;
    buildMainRows();
    draw();
    break;
  case Action::Volume:
    m_volumeMode = true;
    draw();
    break;
  case Action::SaveSnapshot:
    if (m_machine != nullptr)
    {
      m_navigationStack->push(
          new SaveSnapshotScreen(m_tft, m_hdmiDisplay, m_audioOutput, m_machine->getMachine(), m_files));
    }
    break;
  case Action::LoadSnapshot:
    showAlphabetPicker(
        "Snapshots",
        "/snapshots",
        kSnapshotExtensions,
        {"No SD Card", "Insert an SD Card", "to load snapshots"},
        {"No snapshots found", "on the SD Card", "save snapshots from the menu"});
    break;
  case Action::LoadGame:
    showAlphabetPicker(
        "Games",
        "/",
        kGameExtensions,
        {"No SD Card", "Insert an SD Card", "to load games"},
        {"No games found", "on the SD Card", "add Z80 or SNA files"});
    break;
  case Action::Poke:
    if (m_machine != nullptr)
    {
      m_navigationStack->push(
          new PokeScreen(m_tft, m_hdmiDisplay, m_audioOutput, m_machine->getMachine()));
    }
    break;
  case Action::Exit:
    exitMenu();
    break;
  case Action::VolumeDown:
    if (m_audioOutput != nullptr)
    {
      m_audioOutput->volumeDown();
    }
    draw();
    break;
  case Action::VolumeUp:
    if (m_audioOutput != nullptr)
    {
      m_audioOutput->volumeUp();
    }
    draw();
    break;
  case Action::VolumeBack:
    m_volumeMode = false;
    buildMainRows();
    draw();
    break;
  case Action::VolumeSet:
    if (m_audioOutput != nullptr)
    {
      m_audioOutput->setVolume(m_volumeSetLevel);
    }
    draw();
    break;
  }
}

void CydMenuScreen::exitMenu()
{
  m_navigationStack->pop();
}

void CydMenuScreen::showAlphabetPicker(
    const char *title,
    const char *path,
    const std::vector<std::string> &extensions,
    const std::vector<std::string> &noSdError,
    const std::vector<std::string> &noFilesError)
{
  if (!m_files->isAvailable(StorageType::SD))
  {
    m_navigationStack->push(new ErrorScreen(noSdError, m_tft, m_hdmiDisplay, m_audioOutput, m_files));
    return;
  }
  drawBusy();
  FileLetterCountVector letters = m_files->getFileLetters(path, extensions);
  if (letters.empty())
  {
    m_navigationStack->push(new ErrorScreen(noFilesError, m_tft, m_hdmiDisplay, m_audioOutput, m_files));
    return;
  }
  AlphabetPicker<GameFilePickerScreen> *picker =
      new AlphabetPicker<GameFilePickerScreen>(title, m_files, m_tft, m_hdmiDisplay, m_audioOutput, path, extensions);
  picker->setItems(letters);
  m_navigationStack->push(picker);
}
