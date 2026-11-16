#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI u_tolower_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_toupper_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_totitle_nid_postfix(std::int32_t);
std::int32_t APS5_VABI ublock_getCode_nid_postfix(std::int32_t);
}
