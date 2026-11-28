#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceVideoArbitrationAcceptEvent(std::uint32_t event);
int APS5_VABI sceVideoArbitrationEnable(std::uint64_t reserved, void* callback);
int APS5_VABI sceVideoArbitrationInitialize(const void* param);

}
