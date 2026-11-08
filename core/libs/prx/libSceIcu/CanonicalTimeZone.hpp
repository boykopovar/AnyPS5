#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI ucal_getCanonicalTimeZoneID_nid_postfix(
    const char16_t* id,
    std::int32_t len,
    char16_t* result,
    std::int32_t resultCapacity,
    std::int8_t* isSystemID,
    std::int32_t* status);
}
