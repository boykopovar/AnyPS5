#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

std::uint16_t* APS5_VABI _Plsw_nid_postfix(double* value);
std::uint16_t* APS5_VABI _Pmsw_nid_postfix(double* value);
std::uint16_t* APS5_VABI _FPlsw_nid_postfix(float* value);
std::uint16_t* APS5_VABI _FPmsw_nid_postfix(float* value);
std::uint16_t* APS5_VABI _LPlsw_nid_postfix(long double* value);
std::uint16_t* APS5_VABI _LPmsw_nid_postfix(long double* value);

}
