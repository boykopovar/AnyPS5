#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateTimePatternGenerator;

extern "C" {
std::int32_t APS5_VABI udatpg_getSkeleton_nid_postfix(UDateTimePatternGenerator* dtpg, const char16_t* pattern, std::int32_t length,
                                                     char16_t* skeleton, std::int32_t capacity, std::int32_t* errorCode);
}
