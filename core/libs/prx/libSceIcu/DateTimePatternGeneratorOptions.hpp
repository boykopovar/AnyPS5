#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateTimePatternGenerator;

enum UDateTimePatternMatchOptions : std::int32_t {
    UDATPG_MATCH_NO_OPTIONS = 0,
    UDATPG_MATCH_HOUR_FIELD_LENGTH = 1 << 11,
    UDATPG_MATCH_MINUTE_FIELD_LENGTH = 1 << 12,
    UDATPG_MATCH_SECOND_FIELD_LENGTH = 1 << 13,
    UDATPG_MATCH_ALL_FIELDS_LENGTH = (1 << 16) - 1
};

extern "C" {
std::int32_t APS5_VABI udatpg_getBestPatternWithOptions_nid_postfix(
    UDateTimePatternGenerator* dtpg,
    const char16_t* skeleton,
    std::int32_t length,
    UDateTimePatternMatchOptions options,
    char16_t* bestPattern,
    std::int32_t capacity,
    std::int32_t* pErrorCode);
}
