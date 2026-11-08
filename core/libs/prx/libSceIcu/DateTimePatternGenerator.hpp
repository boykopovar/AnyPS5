#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateTimePatternGenerator;

extern "C" {
UDateTimePatternGenerator* APS5_VABI udatpg_open_nid_postfix(const char* locale, std::int32_t* status);
}
