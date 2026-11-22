#pragma once

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVdecwrapCreateDecoder(const void* config, const void* resource, void* decoder);
int APS5_VABI sceVdecwrapDeleteDecoder(void* decoder);
int APS5_VABI sceVdecwrapFlushDecodeOutput(void* decoder, void* output);
int APS5_VABI sceVdecwrapGetDecodeOutput(void* decoder, void* output);
int APS5_VABI sceVdecwrapMapDirectMemory(void* decoder, const void* memory);
int APS5_VABI sceVdecwrapMapMemory(void* decoder, std::uint64_t argument2, void* argument3);
int APS5_VABI sceVdecwrapQueryDecoderMemoryInfo(const void* config, void* info);
int APS5_VABI sceVdecwrapQueryFrameBufferInfo(const void* config, void* info);
int APS5_VABI sceVdecwrapResetDecoder(void* decoder, const void* param);
int APS5_VABI sceVdecwrapSetDecodeInput(void* decoder, const void* input, void* output);
int APS5_VABI sceVdecwrapSyncDecode(void* decoder, void* argument2);

}
