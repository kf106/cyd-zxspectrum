#pragma once

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <esp_system.h>

#ifdef BOOT_DEBUG

inline const char *resetReasonString(esp_reset_reason_t reason)
{
  switch (reason)
  {
  case ESP_RST_UNKNOWN:
    return "UNKNOWN";
  case ESP_RST_POWERON:
    return "POWERON";
  case ESP_RST_EXT:
    return "EXT";
  case ESP_RST_SW:
    return "SW";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
    return "INT_WDT";
  case ESP_RST_TASK_WDT:
    return "TASK_WDT";
  case ESP_RST_WDT:
    return "WDT";
  case ESP_RST_DEEPSLEEP:
    return "DEEPSLEEP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "?";
  }
}

inline void bootLog(const char *tag, const char *msg)
{
  Serial.printf("[BOOT %8lu][%-8s] %s (heap=%u)\n", millis(), tag, msg, ESP.getFreeHeap());
  Serial.flush();
}

inline void bootLogf(const char *tag, const char *fmt, ...)
{
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  bootLog(tag, buf);
}

inline void bootLogResetReason()
{
  bootLogf("main", "reset reason: %s", resetReasonString(esp_reset_reason()));
}

#else

inline void bootLog(const char *, const char *) {}
inline void bootLogf(const char *, const char *, ...) {}
inline void bootLogResetReason() {}

#endif
