#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI u_strspn_nid_postfix(const char16_t* text, const char16_t* set);
std::int32_t APS5_VABI u_strcspn_nid_postfix(const char16_t* text, const char16_t* set);
char16_t* APS5_VABI u_strpbrk_nid_postfix(const char16_t* text, const char16_t* set);
char16_t* APS5_VABI u_strtok_r_nid_postfix(char16_t* source, const char16_t* delimiters, char16_t** state);
char16_t* APS5_VABI u_memrchr32_nid_postfix(const char16_t* text, std::int32_t value, std::int32_t count);
}
