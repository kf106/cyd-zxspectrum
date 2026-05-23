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
static const int CYD_DOT_RADIUS = 8;

static Display *s_calTft = nullptr;
static int s_calStep = 0;
static const char *s_calColourName = "";

static void drawFilledDot(Display &tft, int16_t cx, int16_t cy, uint16_t color)
{
  tft.fillRect(cx - CYD_DOT_RADIUS, cy - CYD_DOT_RADIUS, CYD_DOT_RADIUS * 2 + 1, CYD_DOT_RADIUS * 2 + 1, color);
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

static void cornerCenter(int step_index, int16_t &cx, int16_t &cy)
{
  const int r = CYD_DOT_RADIUS;
  switch (step_index)
  {
  case 0:
    cx = CYD_SCREEN_W - 1 - r;
    cy = r;
    break;
  case 1:
    cx = r;
    cy = r;
    break;
  case 2:
    cx = CYD_SCREEN_W - 1 - r;
    cy = CYD_SCREEN_H - 1 - r;
    break;
  default:
    cx = r;
    cy = CYD_SCREEN_H - 1 - r;
    break;
  }
}

static void paintCalScreen(void)
{
  if (s_calTft == nullptr)
  {
    return;
  }
  Display &tft = *s_calTft;
  int16_t dotX = 0;
  int16_t dotY = 0;
  cornerCenter(s_calStep, dotX, dotY);

  tft.startWrite();
  tft.fillScreen(TFT_BLACK);

  drawFilledDot(tft, dotX, dotY, colorForCorner(s_calColourName));

  tft.loadFont(GillSans_25_vlw);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  Point titleSize = tft.measureString("CALIBRATING");
  tft.drawString("CALIBRATING", (CYD_SCREEN_W - titleSize.x) / 2, 56);

  char prompt[48];
  snprintf(prompt, sizeof(prompt), "Touch the %s dot", s_calColourName);
  tft.loadFont(GillSans_15_vlw);
  Point promptSize = tft.measureString(prompt);
  tft.setTextColor(colorForCorner(s_calColourName), TFT_BLACK);
  tft.drawString(prompt, (CYD_SCREEN_W - promptSize.x) / 2, 88);

  tft.endWrite();
}

static void calStepUi(int step_index, const char *colour_name, uint16_t colour_rgb565)
{
  (void)colour_rgb565;
  s_calStep = step_index;
  s_calColourName = colour_name;
  paintCalScreen();
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

  plate_touch_wait_finger_up();

  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  tft.loadFont(GillSans_25_vlw);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  Point titleSize = tft.measureString("Handedness");
  tft.drawString("Handedness", (CYD_SCREEN_W - titleSize.x) / 2, 48);
  tft.loadFont(GillSans_15_vlw);
  const char *hint = "Tap Left or Right";
  Point hintSize = tft.measureString(hint);
  tft.drawString(hint, (CYD_SCREEN_W - hintSize.x) / 2, 82);
  drawButton(tft, buttons[0], false);
  drawButton(tft, buttons[1], false);
  tft.endWrite();

  bool selected = false;
  bool rightHanded = false;

  while (!selected)
  {
    int16_t x = 0;
    int16_t y = 0;
    if (CydTouch::readScreen(x, y))
    {
      for (int i = 0; i < 2; i++)
      {
        const HandednessButton &b = buttons[i];
        if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h)
        {
          rightHanded = b.rightHanded;
          selected = true;
          tft.startWrite();
          drawButton(tft, buttons[i], true);
          tft.endWrite();
          break;
        }
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }

  plate_touch_wait_finger_up();

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
