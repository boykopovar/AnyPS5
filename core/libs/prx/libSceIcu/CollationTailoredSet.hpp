#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCollator;
struct USet;

extern "C" {
USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator* coll, std::int32_t* status);
}
