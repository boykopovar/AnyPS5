#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UListFormatter;

extern "C" {
std::int32_t APS5_VABI ulistfmt_format_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[],
                                                   const std::int32_t* stringLengths, std::int32_t stringCount,
                                                   char16_t* result, std::int32_t resultCapacity, std::int32_t* status);
}
