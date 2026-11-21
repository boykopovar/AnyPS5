#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace {

void StoreExponent(const char* function, int* exponent, int value) {
    if (!GuestCopyOut_nid_no_patch(exponent, &value, sizeof(value)))
        throw std::invalid_argument(std::string(function) + ": unwritable exponent");
}

template <typename T, typename THypot>
T ScaledHypot(const char* function, T x, T y, int* exponent, THypot hypot) {
    StoreExponent(function, exponent, 0);
    if (std::isnan(x) || std::isnan(y)) return x + y;
    if (std::isinf(x) || std::isinf(y)) return std::fabs(x) + std::fabs(y);
    const T larger = std::fmax(std::fabs(x), std::fabs(y));
    if (larger == T(0)) return T(0);
    int largerExponent = 0;
    std::frexp(larger, &largerExponent);
    int scale = largerExponent / 4 * 2;
    if (scale == 0) scale = 2;
    StoreExponent(function, exponent, scale);
    return hypot(std::ldexp(x, -scale), std::ldexp(y, -scale));
}

}

extern "C" {

float APS5_VABI _FHypot_nid_postfix(float x, float y, int* exponent) {
    return ScaledHypot(__func__, x, y, exponent, hypotf_nid_postfix);
}

double APS5_VABI _Hypot_nid_postfix(double x, double y, int* exponent) {
    return ScaledHypot(__func__, x, y, exponent, hypot_nid_postfix);
}

long double APS5_VABI _LHypot_nid_postfix(long double x, long double y, int* exponent) {
    return ScaledHypot(__func__, x, y, exponent, hypotl_nid_postfix);
}

}
