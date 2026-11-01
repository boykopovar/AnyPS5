// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI u_terminateUChars_nid_postfix(char16_t*, std::int32_t, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_terminateWChars_nid_postfix(std::int32_t*, std::int32_t, std::int32_t, std::int32_t*);
char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t);
char16_t* APS5_VABI u_uastrcpy_nid_postfix(char16_t*, const char*);
char16_t* APS5_VABI u_uastrncpy_nid_postfix(char16_t*, const char*, std::int32_t);
}
