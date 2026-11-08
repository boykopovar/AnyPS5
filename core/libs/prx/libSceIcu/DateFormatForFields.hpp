#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, void* fpositer, std::int32_t* status);
}
