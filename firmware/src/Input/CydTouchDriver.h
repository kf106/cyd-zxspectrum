#pragma once

#include <Arduino.h>

class Display;

/** Persisted plate limits (maps to cyd-lords-of-midnight plate_touch_cal_t). */
struct CydTouchCalibration
{
  uint16_t rawXMin = 200;
  uint16_t rawXMax = 3700;
  uint16_t rawYMin = 200;
  uint16_t rawYMax = 3700;
  bool swapXY = true;
  bool valid = false;
};

namespace CydTouch
{
void init();
void setCalibration(const CydTouchCalibration &cal);
const CydTouchCalibration &calibration();
bool readScreen(int16_t &x, int16_t &y);
} // namespace CydTouch
