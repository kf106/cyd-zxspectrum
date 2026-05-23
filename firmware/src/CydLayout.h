#pragma once

#if defined(CYD_EMULATOR_W) && defined(CYD_TOUCH_KEYBOARD)

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
#ifndef CYD_SPECTRUM_BORDER_TOP
#define CYD_SPECTRUM_BORDER_TOP 2
#endif
#ifndef CYD_SPECTRUM_BORDER_BOTTOM
#define CYD_SPECTRUM_BORDER_BOTTOM 10
#endif
#ifndef CYD_COMMAND_ROW_H
#define CYD_COMMAND_ROW_H 8
#endif

#define CYD_SPECTRUM_W 256
#define CYD_SPECTRUM_H 192
#define CYD_EMULATOR_H (CYD_SPECTRUM_BORDER_TOP + CYD_SPECTRUM_H + CYD_SPECTRUM_BORDER_BOTTOM)

inline int cydEmulatorOriginX(bool rightHanded)
{
  return rightHanded ? CYD_EMULATOR_X_RIGHT : CYD_EMULATOR_X_LEFT;
}

inline int cydSpectrumSideBorder(bool rightHanded)
{
  return (CYD_EMULATOR_W - CYD_SPECTRUM_W) / 2;
}

inline int cydSpectrumOriginX(bool rightHanded)
{
  return cydEmulatorOriginX(rightHanded) + cydSpectrumSideBorder(rightHanded);
}

inline int cydSpectrumOriginY()
{
  return CYD_EMULATOR_Y + CYD_SPECTRUM_BORDER_TOP;
}

#endif
