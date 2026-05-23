#include "CydTouchDriver.h"
#include "cyd_touch/plate_touch.h"
#include "esp_log.h"

static const char *TAG = "CydTouch";
static const uint16_t CYD_SCREEN_W = 320;
static const uint16_t CYD_SCREEN_H = 240;

static CydTouchCalibration s_cal;
static bool s_inited = false;
static bool s_touch_ready = false;

static plate_touch_cal_t toPlateCal(const CydTouchCalibration &cal)
{
  plate_touch_cal_t plate;
  plate.py_at_scr_x_min = cal.rawYMin;
  plate.py_at_scr_x_max = cal.rawYMax;
  plate.px_at_scr_y_min = cal.rawXMin;
  plate.px_at_scr_y_max = cal.rawXMax;
  plate.valid = cal.valid;
  return plate;
}

static CydTouchCalibration fromPlateCal(const plate_touch_cal_t &plate)
{
  CydTouchCalibration cal;
  cal.rawYMin = plate.py_at_scr_x_min;
  cal.rawYMax = plate.py_at_scr_x_max;
  cal.rawXMin = plate.px_at_scr_y_min;
  cal.rawXMax = plate.px_at_scr_y_max;
  cal.swapXY = true;
  cal.valid = plate.valid;
  return cal;
}

void CydTouch::init()
{
  if (s_touch_ready)
  {
    return;
  }
  const esp_err_t err = plate_touch_init(CYD_SCREEN_W, CYD_SCREEN_H);
  s_inited = true;
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "plate_touch_init failed: %s", esp_err_to_name(err));
    s_touch_ready = false;
    return;
  }
  s_touch_ready = true;
  ESP_LOGI(TAG, "touch ready");
}

bool CydTouch::isReady()
{
  return s_touch_ready;
}

void CydTouch::setCalibration(const CydTouchCalibration &cal)
{
  s_cal = cal;
  plate_touch_cal_t plate = toPlateCal(cal);
  plate_touch_set_cal(&plate);
}

const CydTouchCalibration &CydTouch::calibration()
{
  plate_touch_cal_t plate;
  plate_touch_get_cal(&plate);
  s_cal = fromPlateCal(plate);
  return s_cal;
}

bool CydTouch::readScreen(int16_t &x, int16_t &y)
{
  if (!s_inited)
  {
    init();
  }
  if (!s_touch_ready)
  {
    return false;
  }
  int sx = 0;
  int sy = 0;
  if (!plate_touch_try_read(&sx, &sy))
  {
    return false;
  }
  x = (int16_t)sx;
  y = (int16_t)sy;
  return true;
}
