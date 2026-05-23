#pragma once

class Display;
class ISettings;

namespace CydCalibration
{
bool runIfNeeded(Display &tft, ISettings &settings);
}
