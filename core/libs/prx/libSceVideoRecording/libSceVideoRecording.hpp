#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoRecordingSetInfo_nid_disambig1(std::int32_t info, const void* data, std::size_t size);

}
