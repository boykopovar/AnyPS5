#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI u_charDigitValue_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_digit_nid_postfix(std::int32_t, std::int8_t);
std::int8_t APS5_VABI u_isdigit_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isxdigit_nid_postfix(std::int32_t);
}
