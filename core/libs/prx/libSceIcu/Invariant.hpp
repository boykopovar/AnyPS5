#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
char* APS5_VABI u_austrcpy_nid_postfix(char*, const char16_t*);
char* APS5_VABI u_austrncpy_nid_postfix(char*, const char16_t*, std::int32_t);
void APS5_VABI u_charsToUChars_nid_postfix(const char*, char16_t*, std::int32_t);
void APS5_VABI u_UCharsToChars_nid_postfix(const char16_t*, char*, std::int32_t);
}
