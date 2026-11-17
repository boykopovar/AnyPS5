#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
char16_t* APS5_VABI u_strcpy_nid_postfix(char16_t*, const char16_t*);
char16_t* APS5_VABI u_strncpy_nid_postfix(char16_t*, const char16_t*, std::int32_t);
char16_t* APS5_VABI u_strcat_nid_postfix(char16_t*, const char16_t*);
char16_t* APS5_VABI u_strncat_nid_postfix(char16_t*, const char16_t*, std::int32_t);
char16_t* APS5_VABI u_memcpy_nid_postfix(char16_t*, const char16_t*, std::int32_t);
}
