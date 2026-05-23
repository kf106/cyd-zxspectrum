/*
 * Plate-ADC corner calibration and span mapping (cyd-lords-of-midnight touch.c).
 * Bit-bang XPT2046 via plate_touch_hw.c (CYD touch pins; SD uses VSPI on other pins).
 */
#include "plate_touch.h"
#include "plate_touch_hw.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "plate_touch";

#define PLATE_ADC_MAX 4095U

#define PLATE_DEFAULT_RED_PX 16
#define PLATE_DEFAULT_RED_PY 282
#define PLATE_DEFAULT_GREEN_PX 21
#define PLATE_DEFAULT_GREEN_PY 13
#define PLATE_DEFAULT_MAGENTA_PX 224
#define PLATE_DEFAULT_MAGENTA_PY 283
#define PLATE_DEFAULT_CYAN_PX 226
#define PLATE_DEFAULT_CYAN_PY 16

static uint16_t s_screen_w;
static uint16_t s_screen_h;
static plate_touch_cal_t s_plate_cal;
static plate_touch_cal_step_cb_t s_cal_step_cb;
static plate_touch_idle_cb_t s_idle_cb;

static int s_cal_capture_corner = -1;
static uint32_t s_cal_cap_px;
static uint32_t s_cal_cap_py;
static uint32_t s_cal_cap_n;

typedef struct
{
  const char *colour;
  uint16_t plate_x;
  uint16_t plate_y;
} plate_cal_sample_t;

static void pump_idle(void)
{
  if (s_idle_cb != NULL)
  {
    s_idle_cb();
  }
}

static uint16_t plate_span_map(uint16_t v, uint16_t in_lo, uint16_t in_hi, uint16_t out_max)
{
  uint32_t lo = (uint32_t)(in_lo < in_hi ? in_lo : in_hi);
  uint32_t hi = (uint32_t)(in_lo < in_hi ? in_hi : in_lo);
  if (hi <= lo)
  {
    return 0;
  }
  uint32_t vv = (uint32_t)v;
  if (vv < lo)
  {
    vv = lo;
  }
  if (vv > hi)
  {
    vv = hi;
  }
  return (uint16_t)((vv - lo) * (uint32_t)out_max / (hi - lo));
}

static void plate_cal_apply_from_corners(const plate_cal_sample_t *pts)
{
  s_plate_cal.py_at_scr_x_min = pts[1].plate_y;
  s_plate_cal.py_at_scr_x_max = pts[0].plate_y;
  s_plate_cal.px_at_scr_y_min = pts[0].plate_x;
  s_plate_cal.px_at_scr_y_max = pts[2].plate_x;
  s_plate_cal.valid = true;

  ESP_LOGI(TAG, "Plate cal: horiz plate y %u (scr x~0) .. %u (scr x~%u)",
           (unsigned)s_plate_cal.py_at_scr_x_min, (unsigned)s_plate_cal.py_at_scr_x_max,
           (unsigned)(s_screen_w - 1));
  ESP_LOGI(TAG, "Plate cal: vert  plate x %u (scr y~0) .. %u (scr y~%u)",
           (unsigned)s_plate_cal.px_at_scr_y_min, (unsigned)s_plate_cal.px_at_scr_y_max,
           (unsigned)(s_screen_h - 1));
}

static void plate_cal_use_defaults(void)
{
  static const plate_cal_sample_t k_defaults[4] = {
      {"RED", PLATE_DEFAULT_RED_PX, PLATE_DEFAULT_RED_PY},
      {"GREEN", PLATE_DEFAULT_GREEN_PX, PLATE_DEFAULT_GREEN_PY},
      {"MAGENTA", PLATE_DEFAULT_MAGENTA_PX, PLATE_DEFAULT_MAGENTA_PY},
      {"CYAN", PLATE_DEFAULT_CYAN_PX, PLATE_DEFAULT_CYAN_PY},
  };
  plate_cal_apply_from_corners(k_defaults);
  ESP_LOGI(TAG, "Plate cal: factory defaults (run interactive cal to replace)");
}

static void map_plate_to_screen(uint16_t plate_x, uint16_t plate_y, int *screen_x, int *screen_y)
{
  const uint16_t out_w = s_screen_w - 1;
  const uint16_t out_h = s_screen_h - 1;
  int sx;
  int sy;

  if (s_plate_cal.valid)
  {
    sx = (int)plate_span_map(plate_y, s_plate_cal.py_at_scr_x_min, s_plate_cal.py_at_scr_x_max, out_w);
    sy = (int)plate_span_map(plate_x, s_plate_cal.px_at_scr_y_min, s_plate_cal.px_at_scr_y_max, out_h);
  }
  else
  {
    sx = (int)plate_span_map(plate_y, 0, PLATE_ADC_MAX, out_w);
    sy = (int)plate_span_map(plate_x, 0, PLATE_ADC_MAX, out_h);
  }

  if (sx < 0)
  {
    sx = 0;
  }
  if (sy < 0)
  {
    sy = 0;
  }
  if (sx >= (int)s_screen_w)
  {
    sx = (int)s_screen_w - 1;
  }
  if (sy >= (int)s_screen_h)
  {
    sy = (int)s_screen_h - 1;
  }
  *screen_x = sx;
  *screen_y = sy;
}

static bool plate_sample_pressed(uint16_t *plate_x, uint16_t *plate_y)
{
  uint16_t px = 0;
  uint16_t py = 0;
  if (!plate_touch_hw_read_plate(&px, &py))
  {
    return false;
  }

  if (s_cal_capture_corner >= 0)
  {
    s_cal_cap_px += (uint32_t)px;
    s_cal_cap_py += (uint32_t)py;
    s_cal_cap_n++;
  }

  if (plate_x != NULL)
  {
    *plate_x = px;
  }
  if (plate_y != NULL)
  {
    *plate_y = py;
  }
  return true;
}

static void plate_wait_finger_up(void)
{
  int stable = 0;
  while (stable < 6)
  {
    if (!plate_touch_hw_pen_down())
    {
      stable++;
    }
    else
    {
      stable = 0;
    }
    pump_idle();
    vTaskDelay(pdMS_TO_TICKS(25));
  }
  vTaskDelay(pdMS_TO_TICKS(200));
}

static void plate_prompt_one_corner(plate_cal_sample_t *s)
{
  ESP_LOGI(TAG, ">>> Touch the %s dot; hold steady, then release.", s->colour);

  plate_wait_finger_up();

  s_cal_cap_px = 0;
  s_cal_cap_py = 0;
  s_cal_cap_n = 0;
  s_cal_capture_corner = 0;

  uint16_t dummy_x = 0;
  uint16_t dummy_y = 0;
  while (!plate_sample_pressed(&dummy_x, &dummy_y))
  {
    pump_idle();
    vTaskDelay(pdMS_TO_TICKS(25));
  }

  vTaskDelay(pdMS_TO_TICKS(80));

  for (int i = 0; i < 12; i++)
  {
    (void)plate_sample_pressed(&dummy_x, &dummy_y);
    pump_idle();
    vTaskDelay(pdMS_TO_TICKS(15));
  }

  s_cal_capture_corner = -1;

  if (s_cal_cap_n > 0)
  {
    s->plate_x = (uint16_t)(s_cal_cap_px / s_cal_cap_n);
    s->plate_y = (uint16_t)(s_cal_cap_py / s_cal_cap_n);
  }
  else
  {
    ESP_LOGW(TAG, "[%s] no samples — keeping previous plate ADC", s->colour);
  }

  ESP_LOGI(TAG, "[%s] plate vert(x)=%u horiz(y)=%u", s->colour, (unsigned)s->plate_x, (unsigned)s->plate_y);
  plate_wait_finger_up();
}

esp_err_t plate_touch_init(uint16_t screen_w, uint16_t screen_h)
{
  s_screen_w = screen_w;
  s_screen_h = screen_h;
  if (!plate_touch_hw_init())
  {
    return ESP_FAIL;
  }
  plate_cal_use_defaults();
  ESP_LOGI(TAG, "ready %ux%u (LoM plate mapping, bit-bang XPT2046)", (unsigned)screen_w, (unsigned)screen_h);
  return ESP_OK;
}

bool plate_touch_try_read(int *screen_x, int *screen_y)
{
  uint16_t px = 0;
  uint16_t py = 0;
  if (!plate_touch_hw_read_plate(&px, &py))
  {
    return false;
  }
  map_plate_to_screen(px, py, screen_x, screen_y);
  return true;
}

void plate_touch_interactive_calibration(plate_touch_cal_step_cb_t on_step)
{
  static const struct
  {
    const char *name;
    uint16_t rgb565;
  } steps[4] = {
      {"RED", 0xF800},
      {"GREEN", 0x07E0},
      {"MAGENTA", 0xF81F},
      {"CYAN", 0x07FF},
  };

  plate_cal_sample_t pts[4] = {
      {"RED", PLATE_DEFAULT_RED_PX, PLATE_DEFAULT_RED_PY},
      {"GREEN", PLATE_DEFAULT_GREEN_PX, PLATE_DEFAULT_GREEN_PY},
      {"MAGENTA", PLATE_DEFAULT_MAGENTA_PX, PLATE_DEFAULT_MAGENTA_PY},
      {"CYAN", PLATE_DEFAULT_CYAN_PX, PLATE_DEFAULT_CYAN_PY},
  };

  s_cal_step_cb = on_step;
  ESP_LOGI(TAG, "Touch calibration: RED, GREEN, MAGENTA, CYAN");

  for (int i = 0; i < 4; i++)
  {
    pts[i].colour = steps[i].name;
    if (s_cal_step_cb != NULL)
    {
      s_cal_step_cb(i, steps[i].name, steps[i].rgb565);
    }
    plate_prompt_one_corner(&pts[i]);
  }

  s_cal_step_cb = NULL;
  plate_cal_apply_from_corners(pts);
}

void plate_touch_get_cal(plate_touch_cal_t *out)
{
  if (out != NULL)
  {
    *out = s_plate_cal;
  }
}

void plate_touch_set_cal(const plate_touch_cal_t *cal)
{
  if (cal == NULL)
  {
    return;
  }
  s_plate_cal = *cal;
  if (s_plate_cal.valid)
  {
    ESP_LOGI(TAG, "Plate cal loaded from settings");
  }
}

void plate_touch_set_idle_callback(plate_touch_idle_cb_t cb)
{
  s_idle_cb = cb;
}
