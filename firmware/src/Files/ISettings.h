#pragma once

#include "../Input/CydTouchDriver.h"

class ISettings {
  public:
    virtual int getVolume() = 0;
    virtual void setVolume(int volume) = 0;

    virtual bool isCydSetupComplete() { return true; }
    virtual void setCydSetupComplete(bool complete) { (void)complete; }
    virtual bool isCydRightHanded() { return false; }
    virtual void setCydRightHanded(bool rightHanded) { (void)rightHanded; }
    virtual CydTouchCalibration getCydTouchCalibration() { return CydTouchCalibration(); }
    virtual void setCydTouchCalibration(const CydTouchCalibration &cal) { (void)cal; }
};
