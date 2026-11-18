#pragma once

#include "SceTypes.hpp"
#include <cstdint>

namespace Icu {
struct DataMemory;
}

extern "C" {
const void* APS5_VABI udata_getRawMemory_nid_postfix(const Icu::DataMemory* data);
const void* APS5_VABI udata_getMemory_nid_postfix(Icu::DataMemory* data);
std::int32_t APS5_VABI udata_getLength_nid_postfix(const Icu::DataMemory* data);
}
