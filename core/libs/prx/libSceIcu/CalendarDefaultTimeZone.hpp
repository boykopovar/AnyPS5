#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI ucal_getDefaultTimeZone_nid_postfix(char16_t*, std::int32_t, std::int32_t*);
}
