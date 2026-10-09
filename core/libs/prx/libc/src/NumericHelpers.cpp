#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

template<typename TValue>
bool IsNan_nid_no_patch(TValue value) {
    if constexpr (sizeof(TValue) == sizeof(std::uint32_t)) {
        const auto bits = std::bit_cast<std::uint32_t>(value) & UINT32_C(0x7fffffff);
        return bits > UINT32_C(0x7f800000);
    } else {
        const auto bits = std::bit_cast<std::uint64_t>(value) & UINT64_C(0x7fffffffffffffff);
        return bits > UINT64_C(0x7ff0000000000000);
    }
}

template<typename TValue>
TValue Extremum_nid_no_patch(TValue left, TValue right, bool maximum) {
    if (IsNan_nid_no_patch(left)) return right;
    if (IsNan_nid_no_patch(right)) return left;
    if (left == right) {
        if (maximum) return std::signbit(left) ? right : left;
        return std::signbit(left) ? left : right;
    }
    return (maximum ? left > right : left < right) ? left : right;
}

int BoundExponent_nid_no_patch(std::int64_t exponent) {
    return static_cast<int>(std::clamp(exponent, static_cast<std::int64_t>(std::numeric_limits<int>::min()),
                                      static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

template<typename TValue, typename TTarget>
TValue CheckNeighbor_nid_no_patch(TValue from, TTarget to, TValue result) {
    if (from == to || std::isnan(result)) return result;
    if (std::isfinite(from) && std::isinf(result)) {
        errno = ERANGE;
        std::feraiseexcept(FE_OVERFLOW | FE_INEXACT);
    } else if (result == 0 || std::fpclassify(result) == FP_SUBNORMAL) {
        errno = ERANGE;
        std::feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
    }
    return result;
}

}

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
static_assert(sizeof(long double) == 16 && std::numeric_limits<long double>::digits == 64);

extern "C" {

float APS5_VABI sqrtf_nid_postfix(float value) { return std::sqrt(value); }
float APS5_VABI fabsf_nid_postfix(float value) { return std::fabs(value); }
float APS5_VABI ceilf_nid_postfix(float value) { return std::ceil(value); }
float APS5_VABI truncf_nid_postfix(float value) { return std::trunc(value); }
double APS5_VABI nearbyint_nid_postfix(double value) { return std::nearbyint(value); }

double APS5_VABI remquo_nid_postfix(double x, double y, int* quotient) { return std::remquo(x, y, quotient); }
float APS5_VABI remquof_nid_postfix(float x, float y, int* quotient) { return std::remquo(x, y, quotient); }
double APS5_VABI copysign_nid_postfix(double magnitude, double sign) { return std::copysign(magnitude, sign); }
float APS5_VABI copysignf_nid_postfix(float magnitude, float sign) { return std::copysign(magnitude, sign); }

double APS5_VABI nan_nid_postfix(const char* tag) { return std::nan(tag); }
float APS5_VABI nanf_nid_postfix(const char* tag) { return std::nanf(tag); }
long double APS5_VABI nanl_nid_postfix(const char* tag) { return std::nanl(tag); }

double APS5_VABI nextafter_nid_postfix(double from, double to) { return CheckNeighbor_nid_no_patch(from, to, std::nextafter(from, to)); }
double APS5_VABI nexttoward_nid_postfix(double from, long double to) { return CheckNeighbor_nid_no_patch(from, to, std::nexttoward(from, to)); }
float APS5_VABI nexttowardf_nid_postfix(float from, long double to) { return CheckNeighbor_nid_no_patch(from, to, std::nexttoward(from, to)); }

double APS5_VABI fdim_nid_postfix(double x, double y) { return std::fdim(x, y); }
float APS5_VABI fdimf_nid_postfix(float x, float y) { return std::fdim(x, y); }
double APS5_VABI fmax_nid_postfix(double x, double y) { return Extremum_nid_no_patch(x, y, true); }
float APS5_VABI fmaxf_nid_postfix(float x, float y) { return Extremum_nid_no_patch(x, y, true); }
double APS5_VABI fmin_nid_postfix(double x, double y) { return Extremum_nid_no_patch(x, y, false); }
float APS5_VABI fminf_nid_postfix(float x, float y) { return Extremum_nid_no_patch(x, y, false); }
float APS5_VABI fmaf_nid_postfix(float x, float y, float z) { return std::fma(x, y, z); }

double APS5_VABI scalbln_nid_postfix(double value, std::int64_t exponent) { return std::scalbn(value, BoundExponent_nid_no_patch(exponent)); }
float APS5_VABI scalblnf_nid_postfix(float value, std::int64_t exponent) { return std::scalbn(value, BoundExponent_nid_no_patch(exponent)); }

}
