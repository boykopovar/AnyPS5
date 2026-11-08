#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCollator;

extern "C" {
std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator* coll, std::int32_t* dest, std::int32_t destCapacity, std::int32_t* status);
}
