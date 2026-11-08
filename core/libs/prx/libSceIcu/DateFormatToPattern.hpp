#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateFormat;

extern "C" {
std::int32_t APS5_VABI udat_toPattern_nid_postfix(const UDateFormat* fmt, std::int8_t localized, char16_t* result, std::int32_t resultLength, std::int32_t* status);
}
