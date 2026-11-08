#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCollator;

enum UColAttribute : std::int32_t {
    UCOL_FRENCH_COLLATION,
    UCOL_ALTERNATE_HANDLING,
    UCOL_CASE_FIRST,
    UCOL_CASE_LEVEL,
    UCOL_NORMALIZATION_MODE,
    UCOL_DECOMPOSITION_MODE = UCOL_NORMALIZATION_MODE,
    UCOL_STRENGTH,
    UCOL_HIRAGANA_QUATERNARY_MODE,
    UCOL_NUMERIC_COLLATION,
    UCOL_ATTRIBUTE_COUNT
};

enum UColAttributeValue : std::int32_t {
    UCOL_DEFAULT = -1,
    UCOL_PRIMARY = 0,
    UCOL_SECONDARY = 1,
    UCOL_TERTIARY = 2,
    UCOL_DEFAULT_STRENGTH = UCOL_TERTIARY,
    UCOL_CE_STRENGTH_LIMIT,
    UCOL_QUATERNARY = 3,
    UCOL_IDENTICAL = 15,
    UCOL_STRENGTH_LIMIT,
    UCOL_OFF = 16,
    UCOL_ON = 17,
    UCOL_SHIFTED = 20,
    UCOL_NON_IGNORABLE = 21,
    UCOL_LOWER_FIRST = 24,
    UCOL_UPPER_FIRST = 25,
    UCOL_ATTRIBUTE_VALUE_COUNT
};

extern "C" {
void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* coll, UColAttribute attr, UColAttributeValue value, std::int32_t* status);
}
