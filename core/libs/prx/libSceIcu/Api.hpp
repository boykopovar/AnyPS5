#pragma once

#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

std::int32_t APS5_VABI u_strlen_nid_postfix(const char16_t*);
std::int32_t APS5_VABI u_strcmp_nid_postfix(const char16_t*, const char16_t*);
std::int32_t APS5_VABI u_strncmp_nid_postfix(const char16_t*, const char16_t*, std::int32_t);
std::int32_t APS5_VABI u_memcmp_nid_postfix(const char16_t*, const char16_t*, std::int32_t);
std::int32_t APS5_VABI u_memcmpCodePointOrder_nid_postfix(const char16_t*, const char16_t*, std::int32_t);

}
