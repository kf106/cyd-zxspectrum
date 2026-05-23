#pragma once

#if defined(CYD_EMULATOR_W) && defined(CYD_TOUCH_KEYBOARD)

#ifndef CYD_DISPLAY_W
#define CYD_DISPLAY_W 320
#endif
#ifndef CYD_DISPLAY_H
#define CYD_DISPLAY_H 240
#endif
#ifndef CYD_KEYBOARD_ROW_H
#define CYD_KEYBOARD_ROW_H 26
#endif

/* Viewport: border strip + 256×192 drawing area (Spectrum screen pixels). */
#ifndef CYD_EMULATOR_X_RIGHT
#define CYD_EMULATOR_X_RIGHT 0
#endif
#ifndef CYD_EMULATOR_X_LEFT
#define CYD_EMULATOR_X_LEFT 32
#endif
#ifndef CYD_EMULATOR_Y
#define CYD_EMULATOR_Y 0
#endif
#ifndef CYD_EMULATOR_W
#define CYD_EMULATOR_W 288
#endif
#ifndef CYD_EMULATOR_H
#define CYD_EMULATOR_H 214
#endif

#ifndef CYD_SPECTRUM_BORDER_TOP
#define CYD_SPECTRUM_BORDER_TOP 10
#endif
#ifndef CYD_SPECTRUM_BORDER_BOTTOM
#define CYD_SPECTRUM_BORDER_BOTTOM 12
#endif
#ifndef CYD_SPECTRUM_SIDE_BORDER
#define CYD_SPECTRUM_SIDE_BORDER ((CYD_EMULATOR_W - CYD_SPECTRUM_W) / 2)
#endif
#ifndef CYD_COMMAND_ROW_H
#define CYD_COMMAND_ROW_H 8
#endif

#define CYD_KEYBOARD_ROW_Y (CYD_EMULATOR_Y + CYD_EMULATOR_H)

#define CYD_SPECTRUM_W 256
#define CYD_SPECTRUM_H 192

#if (CYD_SPECTRUM_BORDER_TOP + CYD_SPECTRUM_H + CYD_SPECTRUM_BORDER_BOTTOM) != CYD_EMULATOR_H
#error "CYD viewport height must be border_top + 192 + border_bottom"
#endif
#if (CYD_SPECTRUM_SIDE_BORDER * 2 + CYD_SPECTRUM_W) != CYD_EMULATOR_W
#error "CYD viewport width must be side_border + 256 + side_border"
#endif

inline int cydEmulatorOriginX(bool rightHanded)
{
  return rightHanded ? CYD_EMULATOR_X_RIGHT : CYD_EMULATOR_X_LEFT;
}

inline int cydSpectrumSideBorder(bool rightHanded)
{
  (void)rightHanded;
  return CYD_SPECTRUM_SIDE_BORDER;
}

inline int cydSpectrumOriginX(bool rightHanded)
{
  return cydEmulatorOriginX(rightHanded) + CYD_SPECTRUM_SIDE_BORDER;
}

inline int cydSpectrumOriginY()
{
  return CYD_EMULATOR_Y + CYD_SPECTRUM_BORDER_TOP;
}

#endif
