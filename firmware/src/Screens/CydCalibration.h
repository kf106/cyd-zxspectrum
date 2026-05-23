#pragma once

class Display;
class ISettings;

namespace CydCalibration
{
bool runIfNeeded(Display &tft, ISettings &settings);
void runRecalibration(Display &tft, ISettings &settings);
bool runHandednessSelection(Display &tft, ISettings &settings);
}
