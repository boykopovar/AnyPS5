#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVdecwrapCreateDecoder();
int APS5_VABI sceVdecwrapDeleteDecoder();
int APS5_VABI sceVdecwrapFlushDecodeOutput();
int APS5_VABI sceVdecwrapGetDecodeOutput();
int APS5_VABI sceVdecwrapMapDirectMemory();
int APS5_VABI sceVdecwrapMapMemory();
int APS5_VABI sceVdecwrapQueryDecoderMemoryInfo();
int APS5_VABI sceVdecwrapQueryFrameBufferInfo();
int APS5_VABI sceVdecwrapResetDecoder();
int APS5_VABI sceVdecwrapSetDecodeInput();
int APS5_VABI sceVdecwrapSyncDecode();

}
