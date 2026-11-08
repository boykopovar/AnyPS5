#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UFormattedList;
struct UListFormatter;

extern "C" {
void APS5_VABI ulistfmt_formatStringsToResult_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[],
                                                          const std::int32_t* stringLengths, std::int32_t stringCount,
                                                          UFormattedList* uresult, std::int32_t* status);
}
