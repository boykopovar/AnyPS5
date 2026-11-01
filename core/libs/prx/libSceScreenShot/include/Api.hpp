#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceScreenShotSetOverlayImage(const char* path, int offsetX, int offsetY);
int APS5_VABI sceScreenShotSetOverlayImageWithOrigin(const char* path, int offsetX, int offsetY, int origin);
int APS5_VABI sceScreenShotDisable();
int APS5_VABI sceScreenShotEnable();
int APS5_VABI sceScreenShotIsDisabled(int id, int* isDisabled);
int APS5_VABI sceScreenShotDisableNotification();
int APS5_VABI sceScreenShotEnableNotification();
int APS5_VABI sceScreenShotCapture(std::uint32_t type, void* out, int (*cancel)(void*), void* cancelArg);
int APS5_VABI sceScreenShotGetAppInfo(int id, void* out);
int APS5_VABI sceScreenShotGetDrcParam();
int APS5_VABI sceScreenShotIsVshScreenCaptureDisabled(int* out);
int APS5_VABI sceScreenShotSetDrcParam();
int APS5_VABI sceScreenShotSetParam();

}
