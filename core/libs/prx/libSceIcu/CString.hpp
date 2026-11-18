#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI T_CString_integerToString_nid_postfix(char* buffer, std::int32_t value, std::int32_t radix);
std::int32_t APS5_VABI T_CString_int64ToString_nid_postfix(char* buffer, std::int64_t value, std::uint32_t radix);
std::int32_t APS5_VABI T_CString_stringToInteger_nid_postfix(const char* text, std::int32_t radix);
char* APS5_VABI T_CString_toLowerCase_nid_postfix(char* text);
char* APS5_VABI T_CString_toUpperCase_nid_postfix(char* text);
char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t unit);
std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t* units, std::int32_t length);
std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* chars, std::int32_t length);
std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* chars, std::int32_t length);
}
