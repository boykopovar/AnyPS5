#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateFormat;

extern "C" {
UDateFormat* APS5_VABI udat_open_nid_postfix(std::int32_t, std::int32_t, const char*, const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*);
}
