#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
char16_t* APS5_VABI u_memmove_nid_postfix(char16_t*, const char16_t*, std::int32_t);
char16_t* APS5_VABI u_memset_nid_postfix(char16_t*, char16_t, std::int32_t);
char16_t* APS5_VABI u_memchr_nid_postfix(const char16_t*, char16_t, std::int32_t);
char16_t* APS5_VABI u_memrchr_nid_postfix(const char16_t*, char16_t, std::int32_t);
char16_t* APS5_VABI u_strchr_nid_postfix(const char16_t*, char16_t);
char16_t* APS5_VABI u_strrchr_nid_postfix(const char16_t*, char16_t);
char16_t* APS5_VABI u_strFindFirst_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t);
char16_t* APS5_VABI u_strFindLast_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t);
std::int32_t APS5_VABI u_strcmpCodePointOrder_nid_postfix(const char16_t*, const char16_t*);
std::int32_t APS5_VABI u_strncmpCodePointOrder_nid_postfix(const char16_t*, const char16_t*, std::int32_t);
}
