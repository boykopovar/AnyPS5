#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVdecCoreCreateDecoderBid();
int APS5_VABI sceVdecCoreCreateDecoderSvp();
int APS5_VABI sceVdecCoreMapMemory();
int APS5_VABI sceVdecCoreSetDecodeInputSvp();

}
