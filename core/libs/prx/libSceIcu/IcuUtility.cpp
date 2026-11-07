// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <limits>
#include <stdexcept>

extern "C" {

std::int8_t APS5_VABI _ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(std::int32_t character) {
    return character < 0x20 || character > 0x7e;
}

std::int32_t APS5_VABI _ZN6icu_6811ICU_Utility17parseAsciiIntegerERKNS_13UnicodeStringERi_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t* position) {
    if (!position) throw std::invalid_argument("ICU_Utility: null position");
    std::int32_t result = 0;
    while (*position < _ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(text)) {
        const auto unit = _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(text, *position);
        if (unit < u'0' || unit > u'9') break;
        const auto next = static_cast<std::int64_t>(result) * 10 + unit - u'0';
        if (next > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("ICU_Utility: integer overflow");
        result = static_cast<std::int32_t>(next);
        ++*position;
    }
    return result;
}

}
