#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceScreenShotSetOverlayImage(const char* path, int offsetX, int offsetY);
int APS5_VABI sceScreenShotSetOverlayImageWithOrigin(const char* path, int offsetX, int offsetY, int origin);
int APS5_VABI sceScreenShotDisable();
int APS5_VABI sceScreenShotEnable();
int APS5_VABI sceScreenShotIsDisabled(int id, int* isDisabled);
int APS5_VABI sceScreenShotDisableNotification();
int APS5_VABI sceScreenShotEnableNotification();
int APS5_VABI sceScreenShotCapture();
int APS5_VABI sceScreenShotGetAppInfo();
int APS5_VABI sceScreenShotGetDrcParam();
int APS5_VABI sceScreenShotIsVshScreenCaptureDisabled();
int APS5_VABI sceScreenShotSetDrcParam();
int APS5_VABI sceScreenShotSetParam();

}
