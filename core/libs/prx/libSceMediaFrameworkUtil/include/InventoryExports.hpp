#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMediaFwUtilUnregisterNotifySystemVolume();
int APS5_VABI sceMediaFwUtilRegisterNotifySystemVolume();
int APS5_VABI sceMediaFwUtilPreprocessToKillMediaApp();
int APS5_VABI sceMediaFwUtilKillCustomMusicCoreSynchronously();
int APS5_VABI sceMediaFwUtilGetSystemVolume();

}
