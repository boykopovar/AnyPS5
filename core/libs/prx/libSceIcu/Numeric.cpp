#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <bit>
#include <cfloat>
#include <cstdint>
#include <limits>

namespace {

constexpr std::int64_t InfinityBits = 0x7ff0000000000000;
constexpr std::int64_t NaNBits = 0x7ff8000000000000;

std::int64_t Magnitude(double value) {
    return std::bit_cast<std::int64_t>(value) & std::numeric_limits<std::int64_t>::max();
}

bool SignBit(double value) {
    return std::bit_cast<std::int64_t>(value) < 0;
}

}

extern "C" {

std::int8_t APS5_VABI uprv_isNaN_nid_postfix(double number) {
    return Magnitude(number) > InfinityBits;
}

std::int8_t APS5_VABI uprv_isInfinite_nid_postfix(double number) {
    return Magnitude(number) == InfinityBits;
}

std::int8_t APS5_VABI uprv_isPositiveInfinity_nid_postfix(double number) {
    return number > 0 && uprv_isInfinite_nid_postfix(number);
}

std::int8_t APS5_VABI uprv_isNegativeInfinity_nid_postfix(double number) {
    return number < 0 && uprv_isInfinite_nid_postfix(number);
}

double APS5_VABI uprv_getNaN_nid_postfix() {
    return std::bit_cast<double>(NaNBits);
}

double APS5_VABI uprv_getInfinity_nid_postfix() {
    return std::bit_cast<double>(InfinityBits);
}

double APS5_VABI uprv_floor_nid_postfix(double x) {
    return floor_nid_postfix(x);
}

double APS5_VABI uprv_ceil_nid_postfix(double x) {
    return ceil_nid_postfix(x);
}

double APS5_VABI uprv_round_nid_postfix(double x) {
    return uprv_floor_nid_postfix(x + 0.5);
}

double APS5_VABI uprv_fabs_nid_postfix(double x) {
    return fabs_nid_postfix(x);
}

double APS5_VABI uprv_modf_nid_postfix(double x, double* y) {
    if (!y) APS5_INVALID_ARG_EX;
    return modf_nid_postfix(x, y);
}

double APS5_VABI uprv_fmod_nid_postfix(double x, double y) {
    return fmod_nid_postfix(x, y);
}

double APS5_VABI uprv_pow_nid_postfix(double x, double y) {
    return pow_nid_postfix(x, y);
}

double APS5_VABI uprv_pow10_nid_postfix(std::int32_t x) {
    return pow_nid_postfix(10.0, static_cast<double>(x));
}

double APS5_VABI uprv_fmax_nid_postfix(double x, double y) {
    if (uprv_isNaN_nid_postfix(x) || uprv_isNaN_nid_postfix(y)) return uprv_getNaN_nid_postfix();
    if (x == 0.0 && y == 0.0 && SignBit(x)) return y;
    return x > y ? x : y;
}

double APS5_VABI uprv_fmin_nid_postfix(double x, double y) {
    if (uprv_isNaN_nid_postfix(x) || uprv_isNaN_nid_postfix(y)) return uprv_getNaN_nid_postfix();
    if (x == 0.0 && y == 0.0 && SignBit(y)) return y;
    return x > y ? y : x;
}

std::int8_t APS5_VABI uprv_add32_overflow_nid_postfix(std::int32_t a, std::int32_t b, std::int32_t* res) {
    if (!res) APS5_INVALID_ARG_EX;
    const std::int64_t result = static_cast<std::int64_t>(a) + b;
    *res = static_cast<std::int32_t>(result);
    return result != *res;
}

std::int8_t APS5_VABI uprv_mul32_overflow_nid_postfix(std::int32_t a, std::int32_t b, std::int32_t* res) {
    if (!res) APS5_INVALID_ARG_EX;
    const std::int64_t result = static_cast<std::int64_t>(a) * b;
    *res = static_cast<std::int32_t>(result);
    return result != *res;
}

double APS5_VABI uprv_trunc_nid_postfix(double d) {
    if (uprv_isNaN_nid_postfix(d)) return uprv_getNaN_nid_postfix();
    if (uprv_isInfinite_nid_postfix(d)) return uprv_getInfinity_nid_postfix();
    return SignBit(d) ? ceil_nid_postfix(d) : floor_nid_postfix(d);
}

double APS5_VABI uprv_maxMantissa_nid_postfix() {
    return pow_nid_postfix(2.0, DBL_MANT_DIG + 1.0) - 1.0;
}

double APS5_VABI uprv_log_nid_postfix(double d) {
    return log_nid_postfix(d);
}

void* APS5_VABI uprv_maximumPtr_nid_postfix(void* base) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    const std::uintptr_t limit = address + 0x7fffffffu;
    return reinterpret_cast<void*>(limit > address ? limit : std::numeric_limits<std::uintptr_t>::max());
}

std::int32_t APS5_VABI uprv_max_nid_postfix(std::int32_t x, std::int32_t y) {
    return x > y ? x : y;
}

std::int32_t APS5_VABI uprv_min_nid_postfix(std::int32_t x, std::int32_t y) {
    return x > y ? y : x;
}

}
