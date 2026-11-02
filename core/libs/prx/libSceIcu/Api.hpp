#pragma once

#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
std::int32_t APS5_VABI u_countChar32_nid_postfix(const std::uint16_t*, std::int32_t);
std::int8_t APS5_VABI u_strHasMoreChar32Than_nid_postfix(const std::uint16_t*, std::int32_t, std::int32_t);
std::uint16_t* APS5_VABI u_strchr32_nid_postfix(const std::uint16_t*, std::int32_t);
std::uint16_t* APS5_VABI u_strrchr32_nid_postfix(const std::uint16_t*, std::int32_t);
std::uint16_t* APS5_VABI u_memchr32_nid_postfix(const std::uint16_t*, std::int32_t, std::int32_t);
}
