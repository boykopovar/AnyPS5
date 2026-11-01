#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSessionSignalingCrossplayInitialize(void);
int APS5_VABI sceNpSessionSignalingCrossplayTerminate(void);
int APS5_VABI sceNpSessionSignalingCrossplayCreateContext(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown00(void);
int APS5_VABI sceNpSessionSignalingCrossplayDestroyContext(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown01(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown02(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown03(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown04(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown05(void);
int APS5_VABI sceNpSessionSignalingCrossplayGetMemoryInfo(void);

}
