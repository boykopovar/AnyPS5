#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCompanionUtilGetEvent(void* outEvent);
int APS5_VABI sceCompanionUtilGetRemoteOskEvent();
int APS5_VABI sceCompanionUtilInitialize();
int APS5_VABI sceCompanionUtilOptParamInitialize();
int APS5_VABI sceCompanionUtilTerminate();

}
