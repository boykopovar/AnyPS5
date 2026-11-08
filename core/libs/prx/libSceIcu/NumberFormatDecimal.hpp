#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UFieldPosition;
struct UNumberFormat;

extern "C" {
std::int32_t APS5_VABI unum_formatDecimal_nid_postfix(const UNumberFormat* fmt, const char* number, std::int32_t length,
                                                      char16_t* result, std::int32_t resultLength, UFieldPosition* pos,
                                                      std::int32_t* status);
}
