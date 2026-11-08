#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCollator;

extern "C" {
std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* coll, std::int32_t attr, std::int32_t* status);
}
