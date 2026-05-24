#pragma once

#include "NavigationStack.h"
#include "Screen.h"
#include "../TFT/TFTDisplay.h"
#include "fonts/GillSans_25_vlw.h"
#include "fonts/GillSans_15_vlw.h"

#ifdef CYD_TOUCH_KEYBOARD
#include "../Input/CydTouchDriver.h"
#endif

class IFiles;

class MessageScreen : public Screen
{
private:
  std::string m_title;
  std::vector<std::string> m_messages;
#ifdef CYD_TOUCH_KEYBOARD
  bool m_touchActive = false;
#endif
public:
  MessageScreen(
      std::string title,
      std::vector<std::string> messages,
      Display &tft,
      HDMIDisplay *hdmiDisplay,
      AudioOutput *audioOutput) : m_title{title}, m_messages{messages}, Screen(tft, hdmiDisplay, audioOutput, nullptr)
  {
  }

  void didAppear() override
  {
    updateDisplay();
  }

  void pressKey(SpecKeys key) override
  {
    m_navigationStack->pop();
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
      return;
    }
    if (!m_touchActive)
    {
      return;
    }
    m_touchActive = false;
    playKeyClick();
    m_navigationStack->pop();
  }
#endif

  void updateDisplay();
};
