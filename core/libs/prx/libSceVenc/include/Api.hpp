#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVencCreateEncoder();
int APS5_VABI sceVencDeleteEncoder();
int APS5_VABI sceVencGetConfig();
int APS5_VABI sceVencMapMemory();
int APS5_VABI sceVencQueryHeader();
int APS5_VABI sceVencQueryMemorySize();
int APS5_VABI sceVencQueryPreset();
int APS5_VABI sceVencSetConfig();
int APS5_VABI sceVencSetDecodedFrame();
int APS5_VABI sceVencSetPictureConfig();
int APS5_VABI sceVencSetRateControlConfig();
int APS5_VABI sceVencSetReferenceFrameInvalidationConfig();
int APS5_VABI sceVencStartEncode();
int APS5_VABI sceVencStopEncode();

}
