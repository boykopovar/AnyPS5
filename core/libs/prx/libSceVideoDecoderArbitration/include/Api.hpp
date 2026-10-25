#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoDecoderArbitrationAcceptEvent();
int APS5_VABI sceVideoDecoderArbitrationEnable();
int APS5_VABI sceVideoDecoderArbitrationEnableSuspendMode();
int APS5_VABI sceVideoDecoderArbitrationInitialize();

}
