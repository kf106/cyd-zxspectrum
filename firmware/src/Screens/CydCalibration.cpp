#include "CydCalibration.h"
#include <cstring>
#include "../Files/ISettings.h"
#include "../Input/CydTouchDriver.h"
#include "../Input/cyd_touch/plate_touch.h"
#include "../TFT/Display.h"
#include "../Screens/fonts/GillSans_15_vlw.h"
#include "../Screens/fonts/GillSans_25_vlw.h"

static const int CYD_SCREEN_W = 320;
static const int CYD_SCREEN_H = 240;
static const int CYD_DOT_RADIUS = 4;

static Display *s_calTft = nullptr;

static void drawFilledDot(Display &tft, int16_t cx, int16_t cy, uint16_t color)
{
  tft.fillRect(cx - CYD_DOT_RADIUS, cy - CYD_DOT_RADIUS, CYD_DOT_RADIUS * 2 + 1, CYD_DOT_RADIUS * 2 + 1, color);
}

static void drawCornerDots(Display &tft)
{
  const int r = CYD_DOT_RADIUS;
  drawFilledDot(tft, CYD_SCREEN_W - 1 - r, r, Display::color565(255, 0, 0));
  drawFilledDot(tft, r, r, Display::color565(0, 255, 0));
  drawFilledDot(tft, CYD_SCREEN_W - 1 - r, CYD_SCREEN_H - 1 - r, Display::color565(255, 0, 255));
  drawFilledDot(tft, r, CYD_SCREEN_H - 1 - r, Display::color565(0, 255, 255));
}

static uint16_t colorForCorner(const char *name)
{
  if (strcmp(name, "RED") == 0)
  {
    return Display::color565(255, 0, 0);
  }
  if (strcmp(name, "GREEN") == 0)
  {
    return Display::color565(0, 255, 0);
  }
  if (strcmp(name, "MAGENTA") == 0)
  {
    return Display::color565(255, 0, 255);
  }
  return Display::color565(0, 255, 255);
}

static void calStepUi(int step_index, const char *colour_name, uint16_t colour_rgb565)
{
  (void)step_index;
  (void)colour_rgb565;
  if (s_calTft == nullptr)
  {
    return;
  }
  Display &tft = *s_calTft;
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  drawCornerDots(tft);
  tft.loadFont(GillSans_25_vlw);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  Point titleSize = tft.measureString("CALIBRATING");
  tft.drawString("CALIBRATING", (CYD_SCREEN_W - titleSize.x) / 2, 72);

  char prompt[48];
  snprintf(prompt, sizeof(prompt), "Touch the %s dot", colour_name);
  tft.loadFont(GillSans_15_vlw);
  Point promptSize = tft.measureString(prompt);
  tft.setTextColor(colorForCorner(colour_name), TFT_BLACK);
  tft.drawString(prompt, (CYD_SCREEN_W - promptSize.x) / 2, 108);
  tft.endWrite();
}

struct HandednessButton
{
  const char *label;
  bool rightHanded;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

static void drawButton(Display &tft, const HandednessButton &btn, bool pressed)
{
  uint16_t fill = pressed ? 0x52AA : 0x3186;
  tft.fillRect(btn.x, btn.y, btn.w, btn.h, fill);
  tft.drawRect(btn.x, btn.y, btn.w, btn.h, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, fill);
  Point size = tft.measureString(btn.label);
  tft.drawString(btn.label, btn.x + (btn.w - size.x) / 2, btn.y + (btn.h - size.y) / 2);
}

static bool runHandednessSelection(Display &tft, ISettings &settings)
{
  HandednessButton buttons[2] = {
      {"Left", false, 24, 118, 128, 72},
      {"Right", true, 168, 118, 128, 72},
  };

  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  tft.loadFont(GillSans_25_vlw);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  Point titleSize = tft.measureString("Handedness");
  tft.drawString("Handedness", (CYD_SCREEN_W - titleSize.x) / 2, 48);
  tft.loadFont(GillSans_15_vlw);
  const char *hint = "Touch Left or Right";
  Point hintSize = tft.measureString(hint);
  tft.drawString(hint, (CYD_SCREEN_W - hintSize.x) / 2, 82);
  drawButton(tft, buttons[0], false);
  drawButton(tft, buttons[1], false);
  tft.endWrite();

  bool selected = false;
  bool rightHanded = false;
  int active = -1;

  while (!selected)
  {
    int16_t x = 0;
    int16_t y = 0;
    if (CydTouch::readScreen(x, y))
    {
      int hit = -1;
      for (int i = 0; i < 2; i++)
      {
        const HandednessButton &b = buttons[i];
        if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h)
        {
          hit = i;
          break;
        }
      }
      if (hit != active)
      {
        tft.startWrite();
        if (active >= 0)
        {
          drawButton(tft, buttons[active], false);
        }
        active = hit;
        if (active >= 0)
        {
          drawButton(tft, buttons[active], true);
        }
        tft.endWrite();
      }
    }
    else if (active >= 0)
    {
      rightHanded = buttons[active].rightHanded;
      selected = true;
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }

  settings.setCydRightHanded(rightHanded);
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  tft.loadFont(GillSans_15_vlw);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  const char *done = rightHanded ? "Right-handed" : "Left-handed";
  Point doneSize = tft.measureString(done);
  tft.drawString(done, (CYD_SCREEN_W - doneSize.x) / 2, 110);
  tft.endWrite();
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  return rightHanded;
}

bool CydCalibration::runIfNeeded(Display &tft, ISettings &settings)
{
  CydTouch::init();

  if (settings.isCydSetupComplete())
  {
    CydTouch::setCalibration(settings.getCydTouchCalibration());
    return false;
  }

  s_calTft = &tft;
  plate_touch_interactive_calibration(calStepUi);
  s_calTft = nullptr;

  settings.setCydTouchCalibration(CydTouch::calibration());

  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  tft.loadFont(GillSans_25_vlw);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  Point t1 = tft.measureString("CALIBRATION");
  tft.drawString("CALIBRATION", (CYD_SCREEN_W - t1.x) / 2, 88);
  Point t2 = tft.measureString("COMPLETED");
  tft.drawString("COMPLETED", (CYD_SCREEN_W - t2.x) / 2, 118);
  tft.endWrite();
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  runHandednessSelection(tft, settings);
  settings.setCydSetupComplete(true);
  return true;
}
