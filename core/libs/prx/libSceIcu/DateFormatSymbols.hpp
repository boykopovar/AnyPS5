#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* fmt, std::int32_t type, std::int32_t index, char16_t* result,
                                                   std::int32_t resultLength, std::int32_t* status);
}
