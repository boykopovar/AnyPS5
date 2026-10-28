#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int8_t APS5_VABI u_isISOControl_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_forDigit_nid_postfix(std::int32_t, std::int8_t);
}
