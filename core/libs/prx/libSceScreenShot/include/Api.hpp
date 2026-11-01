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
int APS5_VABI sceScreenShotCapture(std::uint32_t type, void* result, int (APS5_VABI* cancel)(void*), void* cancelArg);
int APS5_VABI sceScreenShotGetAppInfo(int id, void* info);
int APS5_VABI sceScreenShotGetDrcParam(int id, void* first, std::uint64_t firstCount, void* second, void* third, std::uint64_t thirdCount, void* fourth);
int APS5_VABI sceScreenShotIsVshScreenCaptureDisabled(int* isDisabled);
int APS5_VABI sceScreenShotSetDrcParam(const void* param);
int APS5_VABI sceScreenShotSetParam(const void* param);

}
