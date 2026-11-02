#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoOutSetFlipMaster(std::int32_t handle, std::int32_t masterHandle);
int APS5_VABI sceVideoOutClearFlipMaster(std::int32_t handle);
int APS5_VABI sceVideoOutCursorEnable(int32_t, int32_t, const void*);
int APS5_VABI sceVideoOutCursorDisable(int32_t, int32_t);
int APS5_VABI sceVideoOutCursorSetImageAddress(int32_t, int32_t, const void*);
int APS5_VABI sceVideoOutCursorSetPosition(int32_t, int32_t, uint32_t, uint32_t);
int APS5_VABI sceVideoOutCursorSet2xMagnify(int32_t, int32_t, bool);
int APS5_VABI sceVideoOutAdjustColor_(int handle, const VideoOutColorSettings* settings, uint32_t settingsSize);
int APS5_VABI sceVideoOutColorSettingsSetGamma_(VideoOutColorSettings* settings, float gamma, uint32_t settingsSize);
int APS5_VABI sceDbgVideoOutAddOutputModeVr60Privilege();
int APS5_VABI sceVideoOutAddVrrStatusFlagsPrivilege();

}
