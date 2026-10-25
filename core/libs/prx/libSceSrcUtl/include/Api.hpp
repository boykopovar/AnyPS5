#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSrcUtilityFinalize();
int APS5_VABI sceSrcUtilityFlush();
int APS5_VABI sceSrcUtilityGetDelaySamples();
int APS5_VABI sceSrcUtilityGetHandleSize();
int APS5_VABI sceSrcUtilityGetPriority();
int APS5_VABI sceSrcUtilityInitialize();
int APS5_VABI sceSrcUtilityInitializeEx();
int APS5_VABI sceSrcUtilityResample();
int APS5_VABI sceSrcUtilitySetPriority();

}
