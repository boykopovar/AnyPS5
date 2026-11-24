#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVdecCoreCreateDecoderBid(std::int32_t bid, const void* config, const void* memory, void** decoder);
int APS5_VABI sceVdecCoreCreateDecoderSvp(std::int32_t svpParam, const void* config, const void* memory, void** decoder);
int APS5_VABI sceVdecCoreMapMemory(void* decoder, const void* address, std::size_t size);
int APS5_VABI sceVdecCoreSetDecodeInputSvp(void* decoder, const void* pictureInfo, const void* auInfo, void* userData);

}
