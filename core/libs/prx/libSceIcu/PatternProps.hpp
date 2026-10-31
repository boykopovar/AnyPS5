// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "UnicodeString.hpp"

extern "C" {
std::int8_t APS5_VABI _ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(std::int32_t);
std::int8_t APS5_VABI _ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(std::int32_t);
std::int8_t APS5_VABI _ZN6icu_6812PatternProps20isSyntaxOrWhiteSpaceEi_nid_postfix(std::int32_t);
std::int8_t APS5_VABI _ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(const char16_t*, std::int32_t);
const char16_t* APS5_VABI _ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(const char16_t*, std::int32_t);
const char16_t* APS5_VABI _ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(const char16_t*, std::int32_t);
const char16_t* APS5_VABI _ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(const char16_t*, std::int32_t*);
std::int32_t APS5_VABI _ZN6icu_6812PatternProps14skipWhiteSpaceERKNS_13UnicodeStringEi_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t);
}
