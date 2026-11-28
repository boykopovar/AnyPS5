#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

constexpr int NegateFlag = 0x8;
constexpr int QuadrantFlags = 0x6;
constexpr double HalfPiHigh = 1.57079632679489655800e+00;
constexpr double HalfPiLow = 6.12323399573676603587e-17;
constexpr long double HalfPiLong = 1.57079632679489661923132169163975144L;

bool KnownAtanArguments(long double x, int flags) {
    return (flags & ~(NegateFlag | QuadrantFlags)) == 0 && x >= 0.0L && x <= 1.0L;
}

double AtanDouble(double x, int flags) {
    const double angle = atan_nid_postfix(x);
    const int quadrant = (flags & QuadrantFlags) >> 1;
    const double sign = quadrant == 1 || quadrant == 3 ? -1.0 : 1.0;
    const double high = quadrant == 0 ? 0.0 : quadrant == 3 ? 2.0 * HalfPiHigh : HalfPiHigh;
    const double low = quadrant == 0 ? 0.0 : quadrant == 3 ? 2.0 * HalfPiLow : HalfPiLow;
    const double result = quadrant == 0 ? angle : high + (sign * angle + low);
    return (flags & NegateFlag) != 0 ? -result : result;
}

constexpr int GuestInvalid = 0x01;
constexpr int GuestOverflow = 0x08;

template <typename T>
T Pi() {
    if constexpr (std::is_same_v<T, long double>) return 3.14159265358979323846264338327950288L;
    else return static_cast<T>(3.14159265358979323846);
}

short Unscale(short* exponent, float* value) {
    return _FDunscale_nid_postfix(exponent, value);
}

short Unscale(short* exponent, double* value) {
    return _Dunscale_nid_postfix(exponent, value);
}

short Unscale(short* exponent, long double* value) {
    return _LDunscale_nid_postfix(exponent, value);
}

short Scale(float* value, std::int64_t exponent) {
    return _FDscale_nid_postfix(value, exponent);
}

short Scale(double* value, std::int64_t exponent) {
    return _Dscale_nid_postfix(value, exponent);
}

short Scale(long double* value, std::int64_t exponent) {
    return _LDscale_nid_postfix(value, exponent);
}

template <typename T>
unsigned int ReduceHalfTurns(T& x) {
    const T even = std::trunc(x / 2) * 2;
    if (even != 0) x -= even;
    unsigned int quadrants = static_cast<unsigned int>(static_cast<int>(x + x));
    const T halves = std::trunc(x + x) / 2;
    if (halves != 0) x -= halves;
    if (x > static_cast<T>(0.25)) {
        x -= static_cast<T>(0.5);
        ++quadrants;
    } else if (x < static_cast<T>(-0.25)) {
        x += static_cast<T>(0.5);
        --quadrants;
    }
    T radians = x * Pi<T>();
    if (radians == 0 && x != 0) {
        short exponent = 0;
        T fraction = x;
        Unscale(&exponent, &fraction);
        radians = fraction * Pi<T>();
        Scale(&radians, exponent);
    }
    x = radians;
    return quadrants;
}

template <typename T, typename TTangent>
T Tangent(T x, int quadrants, TTangent tangent) {
    if (std::isnan(x) || x == 0) return x;
    if (std::isinf(x)) {
        _Feraise_nid_postfix(GuestInvalid);
        return x - x;
    }
    if ((quadrants & 1) == 0) return tangent(x);
    const bool invert = (ReduceHalfTurns(x) & 1u) != 0;
    const bool negate = x < 0;
    if (negate) x = -x;
    T result = tangent(x);
    if (invert) {
        result = -1 / result;
        if (std::isinf(result)) _Feraise_nid_postfix(GuestOverflow);
    }
    return negate ? -result : result;
}

template <typename T, typename TSine, typename TCosine>
T SineCosine(T x, T* cosine, TSine sine, TCosine cosineOf, const char* caller) {
    T sineValue = x;
    T cosineValue = x;
    if (x == 0) {
        cosineValue = 1;
    } else if (std::isinf(x)) {
        sineValue = x - x;
        cosineValue = sineValue;
        _Feraise_nid_postfix(GuestInvalid);
    } else if (!std::isnan(x)) {
        sineValue = sine(x);
        cosineValue = cosineOf(x);
    }
    constexpr std::size_t bytes = std::is_same_v<T, long double> ? 10 : sizeof(T);
    if (cosine == nullptr || !GuestCopyOut_nid_no_patch(cosine, &cosineValue, bytes)) throw std::invalid_argument(std::string(caller) + ": an unwritable cosine pointer");
    return sineValue;
}


}

extern "C" {

double APS5_VABI _Atan_nid_postfix(double x, int flags) {
    if (!KnownAtanArguments(x, flags)) NotImplemented_nid_no_patch("_Atan: an argument outside [0, 1] or flags other than 0x2, 0x4 and 0x8");
    return AtanDouble(x, flags);
}

float APS5_VABI _FAtan_nid_postfix(float x, int flags) {
    if (!KnownAtanArguments(x, flags)) NotImplemented_nid_no_patch("_FAtan: an argument outside [0, 1] or flags other than 0x2, 0x4 and 0x8");
    return static_cast<float>(AtanDouble(x, flags));
}

long double APS5_VABI _LAtan_nid_postfix(long double x, int flags) {
    if (!KnownAtanArguments(x, flags)) NotImplemented_nid_no_patch("_LAtan: an argument outside [0, 1] or flags other than 0x2, 0x4 and 0x8");
    const long double angle = atanl_nid_postfix(x);
    const int quadrant = (flags & QuadrantFlags) >> 1;
    long double result = angle;
    if (quadrant == 1) result = HalfPiLong - angle;
    if (quadrant == 2) result = HalfPiLong + angle;
    if (quadrant == 3) result = 2.0L * HalfPiLong - angle;
    return (flags & NegateFlag) != 0 ? -result : result;
}

double APS5_VABI _Tan_nid_postfix(double x, int quadrants) {
    return Tangent(x, quadrants, tan_nid_postfix);
}

float APS5_VABI _FTan_nid_postfix(float x, int quadrants) {
    return Tangent(x, quadrants, tanf_nid_postfix);
}

long double APS5_VABI _LTan_nid_postfix(long double x, int quadrants) {
    return Tangent(x, quadrants, tanl_nid_postfix);
}

double APS5_VABI _Sincos_nid_postfix(double x, double* cosine) {
    return SineCosine(x, cosine, sin_nid_postfix, cos_nid_postfix, "_Sincos");
}

float APS5_VABI _FSincos_nid_postfix(float x, float* cosine) {
    return SineCosine(x, cosine, sinf_nid_postfix, cosf_nid_postfix, "_FSincos");
}

long double APS5_VABI _LSincos_nid_postfix(long double x, long double* cosine) {
    return SineCosine(x, cosine, sinl_nid_postfix, cosl_nid_postfix, "_LSincos");
}

}
