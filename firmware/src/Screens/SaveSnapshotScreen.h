#pragma once

#include "Screen.h"
#include "../TFT/TFTDisplay.h"
#include "fonts/GillSans_25_vlw.h"
#include "fonts/GillSans_15_vlw.h"
#include "../Emulator/spectrum.h"
#include "../Emulator/snaps.h"

#ifdef CYD_TOUCH_KEYBOARD
#include "../Input/CydTouchDriver.h"
#include <sys/stat.h>
#endif

class IFiles;

class SaveSnapshotScreen : public Screen
{
private:
  ZXSpectrum *machine = nullptr;
  std::string filename = "";
#ifdef CYD_TOUCH_KEYBOARD
  bool m_touchActive = false;
  int16_t m_touchX = 0;
  int16_t m_touchY = 0;

  static const int CYD_SNAP_BTN_W = 120;
  static const int CYD_SNAP_BTN_H = 36;
  static const int CYD_SNAP_BTN_Y = 188;

  bool pointInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) const
  {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
  }

  bool hitSave(int16_t x, int16_t y) const
  {
    return pointInRect(x, y, 24, CYD_SNAP_BTN_Y, CYD_SNAP_BTN_W, CYD_SNAP_BTN_H);
  }

  bool hitCancel(int16_t x, int16_t y) const
  {
    return pointInRect(x, y, 176, CYD_SNAP_BTN_Y, CYD_SNAP_BTN_W, CYD_SNAP_BTN_H);
  }

  std::string nextSnapshotName()
  {
    for (int i = 1; i < 1000; i++)
    {
      char name[16];
      snprintf(name, sizeof(name), "SNAP%03d", i);
      std::string path = m_files->getPath("/snapshots") + "/" + name + ".Z80";
      struct stat st;
      if (stat(path.c_str(), &st) != 0)
      {
        return name;
      }
    }
    return "SNAP999";
  }

  void saveSnapshot()
  {
    if (filename.empty())
    {
      playErrorBeep();
      return;
    }
    auto bl = BusyLight();
    drawBusy();
    std::string fname = m_files->getPath("/snapshots") + "/" + filename + ".Z80";
    Z80FileWriter writer(machine, fname.c_str());
    writer.saveZ80();
    playSuccessBeep();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    m_navigationStack->pop();
  }
#endif

public:
  SaveSnapshotScreen(
      Display &tft,
      HDMIDisplay *hdmiDisplay,
      AudioOutput *audioOutput,
      ZXSpectrum *machine,
      IFiles *files) : machine(machine), Screen(tft, hdmiDisplay, audioOutput, files)
  {
  }

  void didAppear() override
  {
#ifdef CYD_TOUCH_KEYBOARD
    m_touchActive = false;
    filename = nextSnapshotName();
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

    if (hitSave(m_touchX, m_touchY))
    {
      playKeyClick();
      saveSnapshot();
      return;
    }

    if (hitCancel(m_touchX, m_touchY))
    {
      playKeyClick();
      m_navigationStack->pop();
      return;
    }

    playErrorBeep();
  }
#endif

  void pressKey(SpecKeys key) 
  {
    switch (key)
    {
    case JOYK_FIRE:
    case SPECKEY_ENTER:
    if (filename.length() > 0) {
#ifdef CYD_TOUCH_KEYBOARD
        saveSnapshot();
#else
        auto bl = BusyLight();
        drawBusy();
        std::string fname = m_files->getPath("/snapshots") + "/" + filename + ".Z80";
        Z80FileWriter writer(machine, fname.c_str());
        writer.saveZ80();
        playSuccessBeep();
        vTaskDelay(500 / portTICK_PERIOD_MS);
        m_navigationStack->pop();
#endif
        return;
    }
    case SPECKEY_DEL:
      if (filename.length() > 0)
      {
        Serial.println("Delete last character");
        filename = filename.substr(0, filename.length() - 1);
        updateDisplay();
        playKeyClick();
        return;
      }
      else
      {
        playErrorBeep();
      }
      break;
    case SPECKEY_BREAK:
      Serial.println("Cancel");
      m_navigationStack->pop();
      return;
    default:
      // does the speckey map onto a letter - look in the mapping table
      if (specKeyToLetterMap().find(key) != specKeyToLetterMap().end())
      {
        char letter = specKeyToLetterMap().at(key);
        // only allow alphnum and not "."
        if (isalpha(letter) || isdigit(letter))
        {
          // maximum length of 8
          if (filename.length() < 8)
          {
            // make upper case
            letter = toupper(letter);
            filename += letter;
            playKeyClick();
            updateDisplay();
            return;
          }
          else
          {
            playErrorBeep();
          }
        }
      }
    }
  };
  

  void updateDisplay()
  {
    const int yMargin = 100;
    const int xMargin = 76;
    m_tft.loadFont(GillSans_25_vlw);
    m_tft.startWrite();
    m_tft.fillRect(xMargin/2, yMargin/2, m_tft.width() - xMargin, m_tft.height() - yMargin, TFT_BLACK);
    m_tft.drawRect(xMargin/2, yMargin/2, m_tft.width() - xMargin, m_tft.height() - yMargin, TFT_WHITE);
    m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
#ifdef CYD_TOUCH_KEYBOARD
    auto size = m_tft.measureString("Save snapshot?");
    m_tft.drawString("Save snapshot?", (m_tft.width() - size.x)/2, yMargin/2 + 15);
#else
    auto size = m_tft.measureString("Enter Filename");
    m_tft.drawString("Enter Filename", (m_tft.width() - size.x)/2, yMargin/2 + 15);
#endif

    int centerX = m_tft.width() / 2;
    int centerY = m_tft.height() / 2;

    // filename textbox
    m_tft.fillRect(centerX - 4*25, centerY-20, 8*25, 40, 0x2020);
    m_tft.drawRect(centerX - 4*25, centerY-20, 8*25, 40, TFT_WHITE);

    m_tft.setTextColor(TFT_WHITE, 0x2020);
    auto textSize = m_tft.measureString(filename.c_str());
    m_tft.drawString(filename.c_str(), centerX - textSize.x/2, centerY - textSize.y/2);

#ifdef CYD_TOUCH_KEYBOARD
    m_tft.loadFont(GillSans_15_vlw);
    auto extSize = m_tft.measureString(".Z80");
    m_tft.drawString(".Z80", centerX + textSize.x/2 + 4, centerY - extSize.y/2);

    m_tft.fillRect(24, CYD_SNAP_BTN_Y, CYD_SNAP_BTN_W, CYD_SNAP_BTN_H, 0x0400);
    m_tft.drawRect(24, CYD_SNAP_BTN_Y, CYD_SNAP_BTN_W, CYD_SNAP_BTN_H, TFT_WHITE);
    m_tft.drawString("Save", 56, CYD_SNAP_BTN_Y + 10);

    m_tft.fillRect(176, CYD_SNAP_BTN_Y, CYD_SNAP_BTN_W, CYD_SNAP_BTN_H, 0x2104);
    m_tft.drawRect(176, CYD_SNAP_BTN_Y, CYD_SNAP_BTN_W, CYD_SNAP_BTN_H, TFT_WHITE);
    m_tft.drawString("Cancel", 196, CYD_SNAP_BTN_Y + 10);
#else
    // draw a cursor on the end of the filename (let's just use a white rectangle)
    m_tft.fillRect(centerX + textSize.x/2, centerY-15, 3, 30, TFT_WHITE);

    m_tft.loadFont(GillSans_15_vlw);
    auto instructionsSize = m_tft.measureString("Press ENTER to save, BREAK to exit");
    m_tft.drawString("Press ENTER to save, BREAK to exit", centerX - instructionsSize.x/2, centerY + 40);
#endif

    m_tft.endWrite();
  }
};
