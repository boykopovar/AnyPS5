#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateFormat;
struct UFieldPosition;

extern "C" {
std::int32_t APS5_VABI udat_format_nid_postfix(const UDateFormat* format, double dateToFormat, char16_t* result, std::int32_t resultLength, UFieldPosition* position, std::int32_t* status);
}
