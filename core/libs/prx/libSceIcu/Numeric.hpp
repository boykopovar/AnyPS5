#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int8_t APS5_VABI uprv_isNaN_nid_postfix(double);
std::int8_t APS5_VABI uprv_isInfinite_nid_postfix(double);
std::int8_t APS5_VABI uprv_isPositiveInfinity_nid_postfix(double);
std::int8_t APS5_VABI uprv_isNegativeInfinity_nid_postfix(double);
double APS5_VABI uprv_getNaN_nid_postfix();
double APS5_VABI uprv_getInfinity_nid_postfix();
double APS5_VABI uprv_floor_nid_postfix(double);
double APS5_VABI uprv_ceil_nid_postfix(double);
double APS5_VABI uprv_round_nid_postfix(double);
double APS5_VABI uprv_fabs_nid_postfix(double);
double APS5_VABI uprv_modf_nid_postfix(double, double*);
double APS5_VABI uprv_fmod_nid_postfix(double, double);
double APS5_VABI uprv_pow_nid_postfix(double, double);
double APS5_VABI uprv_pow10_nid_postfix(std::int32_t);
double APS5_VABI uprv_fmax_nid_postfix(double, double);
double APS5_VABI uprv_fmin_nid_postfix(double, double);
std::int8_t APS5_VABI uprv_add32_overflow_nid_postfix(std::int32_t, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI uprv_mul32_overflow_nid_postfix(std::int32_t, std::int32_t, std::int32_t*);
double APS5_VABI uprv_trunc_nid_postfix(double);
double APS5_VABI uprv_maxMantissa_nid_postfix();
double APS5_VABI uprv_log_nid_postfix(double);
void* APS5_VABI uprv_maximumPtr_nid_postfix(void*);
std::int32_t APS5_VABI uprv_max_nid_postfix(std::int32_t, std::int32_t);
std::int32_t APS5_VABI uprv_min_nid_postfix(std::int32_t, std::int32_t);
}
