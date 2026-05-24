#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include "Screen.h"
#include "../TFT/Display.h"
#include "../Emulator/spectrum.h"
#include "fonts/GillSans_25_vlw.h"
#include "fonts/GillSans_15_vlw.h"
#include "images/rainbow_image.h"

#ifdef CYD_TOUCH_KEYBOARD
#include "../Input/CydTouchDriver.h"
#endif

class ScrollingList;

template <class ItemT>
class PickerScreen : public Screen
{
private:
  std::vector<ItemT> m_items;
  std::string searchPrefix;
  std::string title;
  int lastSearchPrefix = 0;
  int m_selectedItem = 0;
  int m_lastPageDrawn = -1;
#ifdef CYD_TOUCH_KEYBOARD
  int m_firstVisibleItem = 0;
  bool m_touchActive = false;
  int16_t m_touchX = 0;
  int16_t m_touchY = 0;

  static const int CYD_PICKER_HEADER_H = 18;
  static const int CYD_PICKER_ROW_H = 25;
  static const int CYD_PICKER_LIST_Y = 25;
  static const int CYD_PICKER_BACK_W = 56;
  static const int CYD_PICKER_SCROLL_W = 28;
  static const int CYD_PICKER_LIST_RIGHT = 320 - 24 - 28;

  int linesPerPage() const
  {
    return (m_tft.height() - CYD_PICKER_LIST_Y) / CYD_PICKER_ROW_H;
  }

  bool pointInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) const
  {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
  }

  bool hitBack(int16_t x, int16_t y) const
  {
    return pointInRect(x, y, 0, 0, CYD_PICKER_BACK_W, CYD_PICKER_HEADER_H);
  }

  bool hitScrollUp(int16_t x, int16_t y) const
  {
    return m_firstVisibleItem > 0 &&
           pointInRect(x, y, CYD_PICKER_LIST_RIGHT, CYD_PICKER_LIST_Y, CYD_PICKER_SCROLL_W, CYD_PICKER_ROW_H);
  }

  bool hitScrollDown(int16_t x, int16_t y) const
  {
    const int pageSize = linesPerPage();
    return m_firstVisibleItem + pageSize < (int)m_items.size() &&
           pointInRect(x, y, CYD_PICKER_LIST_RIGHT, CYD_PICKER_LIST_Y + CYD_PICKER_ROW_H, CYD_PICKER_SCROLL_W, CYD_PICKER_ROW_H);
  }

  int hitVisibleRow(int16_t x, int16_t y) const
  {
    if (x < 0 || x >= CYD_PICKER_LIST_RIGHT)
    {
      return -1;
    }
    const int relY = y - CYD_PICKER_LIST_Y;
    if (relY < 0)
    {
      return -1;
    }
    const int row = relY / CYD_PICKER_ROW_H;
    const int pageSize = linesPerPage();
    if (row < 0 || row >= pageSize)
    {
      return -1;
    }
    const int itemIndex = m_firstVisibleItem + row;
    if (itemIndex >= (int)m_items.size())
    {
      return -1;
    }
    return itemIndex;
  }
#endif

  bool starts_with(const std::string& str, const std::string& prefix) {
      return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
  }
public:
  PickerScreen(
      std::string title,
      Display &tft,
      HDMIDisplay *hdmiDisplay,
      AudioOutput *audioOutput,
      IFiles *files
  ) : title(title), Screen(tft, hdmiDisplay, audioOutput, files)
  {
  }

  void setItems(std::vector<ItemT> items)
  {
    m_items = items;
    m_selectedItem = 0;
    m_lastPageDrawn = -1;
#ifdef CYD_TOUCH_KEYBOARD
    m_firstVisibleItem = 0;
#endif
    updateDisplay();
  }


  void didAppear() override
  {
    m_lastPageDrawn=-1;
#ifdef CYD_TOUCH_KEYBOARD
    m_touchActive = false;
    m_firstVisibleItem = 0;
    if (m_navigationStack != nullptr)
    {
      m_navigationStack->setCydKeyboardEnabled(false);
    }
#endif
    updateDisplay();
  }

  void willDisappear() override
  {
#ifdef CYD_TOUCH_KEYBOARD
    if (m_navigationStack != nullptr)
    {
      m_navigationStack->setCydKeyboardEnabled(true);
    }
#endif
  }

#ifdef CYD_TOUCH_KEYBOARD
  bool usesCydTouch() const override { return true; }

  void pollCydTouch() override
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

    if (hitBack(m_touchX, m_touchY))
    {
      playKeyClick();
      onBack();
      return;
    }

    if (hitScrollUp(m_touchX, m_touchY))
    {
      playKeyClick();
      m_firstVisibleItem = std::max(0, m_firstVisibleItem - linesPerPage());
      m_lastPageDrawn = -1;
      updateDisplay();
      return;
    }

    if (hitScrollDown(m_touchX, m_touchY))
    {
      playKeyClick();
      const int pageSize = linesPerPage();
      m_firstVisibleItem = std::min(std::max(0, (int)m_items.size() - pageSize), m_firstVisibleItem + pageSize);
      m_lastPageDrawn = -1;
      updateDisplay();
      return;
    }

    const int itemIndex = hitVisibleRow(m_touchX, m_touchY);
    if (itemIndex < 0)
    {
      playErrorBeep();
      return;
    }

    playKeyClick();
    onItemSelect(m_items[itemIndex], itemIndex);
  }
#endif

  virtual void onBack() {
    m_navigationStack->pop();
  }
  virtual void onItemSelect(ItemT item, int index) = 0;

  void pressKey(SpecKeys key)
  {
    bool isHandled = false;
    switch (key)
    {
    case JOYK_UP:
    case SPECKEY_7:
      if (m_selectedItem > 0)
      {
        playKeyClick();
        m_selectedItem--;
        updateDisplay();
      }
      isHandled = true;
      break;
    case JOYK_DOWN:
    case SPECKEY_6:
      if (m_selectedItem < (int)m_items.size() - 1)
      {
        playKeyClick();
        m_selectedItem++;
        updateDisplay();
      }
      isHandled = true;
      break;
    case JOYK_LEFT:
    case SPECKEY_5:
    {
      playKeyClick();
      isHandled = true;
      onBack();
      break;
    }
    case JOYK_FIRE:
    case SPECKEY_ENTER:
      playKeyClick();
      onItemSelect(m_items[m_selectedItem], m_selectedItem);
      break;
    }
    if (!isHandled)
    {
      // does the speckey map onto a letter - look in the mapping table
      if (specKeyToLetterMap().find(key) != specKeyToLetterMap().end())
      {
        playKeyClick();
        char letter = specKeyToLetterMap().at(key);
        if (millis() - lastSearchPrefix > 500)
        {
          searchPrefix = "";
        }
        searchPrefix += letter;
        lastSearchPrefix = millis();
        for (int i = 0; i < (int)m_items.size(); i++)
        {
          if (starts_with(m_items[i]->getTitle(), searchPrefix))
          {
            m_selectedItem = i;
            updateDisplay();
            break;
          }
        }
      }
    }
  }

  void updateDisplay()
  {
    m_tft.startWrite();
#ifdef CYD_TOUCH_KEYBOARD
    const int pageSize = linesPerPage();
    const int page = m_firstVisibleItem / std::max(1, pageSize);
    if (page != m_lastPageDrawn)
    {
      m_tft.fillScreen(TFT_BLACK);
      m_lastPageDrawn = page;
    }
    m_tft.loadFont(GillSans_15_vlw);
    m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    m_tft.fillRect(0, 0, CYD_PICKER_BACK_W, CYD_PICKER_HEADER_H, 0x2104);
    m_tft.drawRect(0, 0, CYD_PICKER_BACK_W, CYD_PICKER_HEADER_H, TFT_WHITE);
    m_tft.drawString("Back", 8, 1);
    m_tft.drawString(title.c_str(), CYD_PICKER_BACK_W + 4, 1);
    m_tft.drawFastHLine(0, CYD_PICKER_LIST_Y - 2, m_tft.width() - 1, TFT_WHITE);
    m_tft.loadFont(GillSans_25_vlw);
    for (int i = 0; i < pageSize; i++)
    {
      const int itemIndex = m_firstVisibleItem + i;
      if (itemIndex >= (int)m_items.size())
      {
        break;
      }
      m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
      m_tft.drawString(m_items[itemIndex]->getTitle().c_str(), 5, CYD_PICKER_LIST_Y + i * CYD_PICKER_ROW_H);
    }
    if (m_firstVisibleItem > 0)
    {
      m_tft.fillRect(CYD_PICKER_LIST_RIGHT, CYD_PICKER_LIST_Y, CYD_PICKER_SCROLL_W, CYD_PICKER_ROW_H, 0x2104);
      m_tft.drawRect(CYD_PICKER_LIST_RIGHT, CYD_PICKER_LIST_Y, CYD_PICKER_SCROLL_W, CYD_PICKER_ROW_H, TFT_WHITE);
      m_tft.loadFont(GillSans_15_vlw);
      m_tft.drawString("^", CYD_PICKER_LIST_RIGHT + 10, CYD_PICKER_LIST_Y + 4);
      m_tft.loadFont(GillSans_25_vlw);
    }
    if (m_firstVisibleItem + pageSize < (int)m_items.size())
    {
      m_tft.fillRect(CYD_PICKER_LIST_RIGHT, CYD_PICKER_LIST_Y + CYD_PICKER_ROW_H, CYD_PICKER_SCROLL_W, CYD_PICKER_ROW_H, 0x2104);
      m_tft.drawRect(CYD_PICKER_LIST_RIGHT, CYD_PICKER_LIST_Y + CYD_PICKER_ROW_H, CYD_PICKER_SCROLL_W, CYD_PICKER_ROW_H, TFT_WHITE);
      m_tft.loadFont(GillSans_15_vlw);
      m_tft.drawString("v", CYD_PICKER_LIST_RIGHT + 10, CYD_PICKER_LIST_Y + CYD_PICKER_ROW_H + 4);
      m_tft.loadFont(GillSans_25_vlw);
    }
#else
    int linesPerPage = (m_tft.height() - 10 - 15)/25;
    int page = m_selectedItem / linesPerPage;
    if (page != m_lastPageDrawn)
    {
      m_tft.fillScreen(TFT_BLACK);
      m_lastPageDrawn = page;
    }
    m_tft.loadFont(GillSans_15_vlw);
    m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    m_tft.drawString((title + " - 5: Back, 6: Down, 7: Up, ENTER: Pick").c_str(), 0, 0);
    m_tft.drawFastHLine(0, 15, m_tft.width() - 1, TFT_WHITE);
    m_tft.loadFont(GillSans_25_vlw);
    for (int i = 0; i < linesPerPage; i++)
    {
      int itemIndex = page * linesPerPage + i;
      if (itemIndex >= (int)m_items.size())
      {
        break;
      }
      m_tft.setTextColor(itemIndex == m_selectedItem ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
      m_tft.drawString(m_items[itemIndex]->getTitle().c_str(), 5, 10 + 15 + i * 25);
    }
#endif
    // draw the spectrum flash
    m_tft.setWindow(m_tft.width() - rainbowImageWidth, m_tft.height() - rainbowImageHeight, m_tft.width() - 1, m_tft.height() - 1);
    m_tft.pushPixels((uint16_t *) rainbowImageData, rainbowImageWidth * rainbowImageHeight);
    m_tft.endWrite();
  }
};
