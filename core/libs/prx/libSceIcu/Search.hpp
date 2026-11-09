#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
char16_t* APS5_VABI u_strstr_nid_postfix(const char16_t*, const char16_t*);
char16_t* APS5_VABI u_strrstr_nid_postfix(const char16_t*, const char16_t*);
}
