#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI u_strFoldCase_nid_postfix(char16_t*, std::int32_t, const char16_t*, std::int32_t, std::uint32_t, std::int32_t*);
std::int32_t APS5_VABI u_strToLower_nid_postfix(char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, std::int32_t*);
std::int32_t APS5_VABI u_strToUpper_nid_postfix(char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, std::int32_t*);
std::int32_t APS5_VABI u_strCaseCompare_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::uint32_t, std::int32_t*);
std::int32_t APS5_VABI u_strcasecmp_nid_postfix(const char16_t*, const char16_t*, std::uint32_t);
std::int32_t APS5_VABI u_strncasecmp_nid_postfix(const char16_t*, const char16_t*, std::int32_t, std::uint32_t);
std::int32_t APS5_VABI u_memcasecmp_nid_postfix(const char16_t*, const char16_t*, std::int32_t, std::uint32_t);
void APS5_VABI u_caseInsensitivePrefixMatch_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::uint32_t, std::int32_t*, std::int32_t*, std::int32_t*);
}
