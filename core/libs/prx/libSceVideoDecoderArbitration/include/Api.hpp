#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoDecoderArbitrationAcceptEvent(std::uint32_t event);
int APS5_VABI sceVideoDecoderArbitrationEnable(std::uint64_t reserved, void* callback);
int APS5_VABI sceVideoDecoderArbitrationEnableSuspendMode();
int APS5_VABI sceVideoDecoderArbitrationInitialize(const void* param);

}
