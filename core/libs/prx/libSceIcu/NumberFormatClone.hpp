#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UNumberFormat;

extern "C" {
UNumberFormat* APS5_VABI unum_clone_nid_postfix(const UNumberFormat* fmt, std::int32_t* status);
}
