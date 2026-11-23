#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UBreakIterator;
struct UParseError;

namespace Icu {
void CloseBreakIterator_nid_no_patch(void* iterator);
}

extern "C" {
UBreakIterator* APS5_VABI ubrk_openRules_nid_postfix(const char16_t* rules, std::int32_t rulesLength,
    const char16_t* text, std::int32_t textLength, UParseError* parseError, std::int32_t* status);
std::int32_t APS5_VABI ubrk_getRuleStatusVec_nid_postfix(UBreakIterator* iterator, std::int32_t* values,
    std::int32_t capacity, std::int32_t* status);
const char* APS5_VABI ubrk_getLocaleByType_nid_postfix(const UBreakIterator* iterator,
    std::int32_t type, std::int32_t* status);
UBreakIterator* APS5_VABI ubrk_safeClone_nid_postfix(const UBreakIterator* iterator,
    void* stackBuffer, std::int32_t* bufferSize, std::int32_t* status);
}
