#pragma once

#include "SceTypes.hpp"

extern "C" {

char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t c);
char APS5_VABI uprv_toupper_nid_postfix(char c);
char APS5_VABI uprv_asciitolower_nid_postfix(char c);
char APS5_VABI uprv_ebcdictolower_nid_postfix(char c);
std::int32_t APS5_VABI uprv_stricmp_nid_postfix(const char* str1, const char* str2);
std::int32_t APS5_VABI uprv_strnicmp_nid_postfix(const char* str1, const char* str2, std::uint32_t n);
std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char* s, std::int32_t length);
std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t* s, std::int32_t length);

}
