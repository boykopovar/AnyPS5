#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI ucal_getTimeZoneDisplayName_nid_postfix(const void* calendar, std::int32_t displayNameType,
                                                               const char* locale, char16_t* result,
                                                               std::int32_t resultLength, std::int32_t* status);
}
