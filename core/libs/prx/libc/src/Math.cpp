#include <mutex>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cfenv>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <xmmintrin.h>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"

namespace {

template<typename TFloat>
std::int64_t RoundToInt64(TFloat value) {
    const TFloat rounded = std::round(value);
    if (!(rounded >= static_cast<TFloat>(INT64_MIN) && rounded < -static_cast<TFloat>(INT64_MIN))) {
        std::feraiseexcept(FE_INVALID);
        return INT64_MIN;
    }
    return static_cast<std::int64_t>(rounded);
}

float SinhfExpm1(float x) {
    constexpr float Ln2High = 6.9313812256e-01f, Ln2Low = 9.0580006145e-06f, InverseLn2 = 1.4426950216e+00f;
    constexpr float Q1 = -3.3333212137e-2f, Q2 = 1.5807170421e-3f;
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x);
    int k = 0;
    float c = 0.0f;
    if (magnitude > 0x3eb17218u) {
        float high;
        float low;
        if (magnitude < 0x3f851592u) {
            high = x - Ln2High;
            low = Ln2Low;
            k = 1;
        } else {
            k = static_cast<int>(InverseLn2 * x + 0.5f);
            const float t = static_cast<float>(k);
            high = x - t * Ln2High;
            low = t * Ln2Low;
        }
        x = high - low;
        c = (high - x) - low;
    }
    const float halfX = 0.5f * x;
    const float halfSquare = x * halfX;
    const float r1 = 1.0f + halfSquare * (Q1 + halfSquare * Q2);
    const float t = 3.0f - r1 * halfX;
    float e = halfSquare * ((r1 - t) / (6.0f - x * t));
    if (k == 0) return x - (x * e - halfSquare);
    e = x * (e - c) - c;
    e -= halfSquare;
    if (k == 1) {
        if (x < -0.25f) return -2.0f * (e - (x + 0.5f));
        return 1.0f + 2.0f * (x - e);
    }
    const float twoPowK = std::bit_cast<float>(static_cast<std::uint32_t>(0x7f + k) << 23);
    const float oneMinusTwoPowMinusK = std::bit_cast<float>(0x3f800000u - (0x1000000u >> k));
    return (oneMinusTwoPowMinusK - (e - x)) * twoPowK;
}

float SinhfExp(float x) {
    constexpr float Ln2High = 6.9314575195e-01f, Ln2Low = 1.4286067653e-06f, InverseLn2 = 1.4426950216e+00f;
    constexpr float P1 = 1.6666625440e-1f, P2 = -2.7667332906e-3f;
    const int k = static_cast<int>(InverseLn2 * x + 0.5f);
    const float kFloat = static_cast<float>(k);
    const float high = x - kFloat * Ln2High;
    const float low = kFloat * Ln2Low;
    x = high - low;
    const float t = x * x;
    const float c = x - t * (P1 + t * P2);
    const float y = 1.0f - ((low - (x * c) / (2.0f - c)) - high);
    if (k == 128) return y * 2.0f * 0x1p127f;
    return y * std::bit_cast<float>(static_cast<std::uint32_t>(0x7f + k) << 23);
}

float AtanhfLog1p(float x) {
    constexpr float Ln2High = 6.9313812256e-01f, Ln2Low = 9.0580006145e-06f;
    constexpr float Lp1 = 6.6666668653e-01f, Lp2 = 4.0000000596e-01f, Lp3 = 2.8571429849e-01f, Lp4 = 2.2222198546e-01f;
    constexpr float Lp5 = 1.8183572590e-01f, Lp6 = 1.5313838422e-01f, Lp7 = 1.4798198640e-01f;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x);
    if (bits < 0x38000000u) return bits < 0x33800000u ? x : x - x * x * 0.5f;
    int k = 0;
    float f = x;
    float c = 0.0f;
    std::uint32_t mantissa = 1;
    if (bits >= 0x3ed413d0u) {
        const float u = 1.0f + x;
        const std::uint32_t uBits = std::bit_cast<std::uint32_t>(u);
        k = static_cast<int>(uBits >> 23) - 127;
        c = (k > 0 ? 1.0f - (u - x) : x - (u - 1.0f)) / u;
        mantissa = uBits & 0x007fffffu;
        if (mantissa < 0x3504f4u) {
            f = std::bit_cast<float>(mantissa | 0x3f800000u) - 1.0f;
        } else {
            ++k;
            f = std::bit_cast<float>(mantissa | 0x3f000000u) - 1.0f;
            mantissa = (0x00800000u - mantissa) >> 2;
        }
    }
    const float halfSquare = 0.5f * f * f;
    const float kFloat = static_cast<float>(k);
    if (mantissa == 0) {
        if (f == 0.0f) return kFloat * Ln2High + (c + kFloat * Ln2Low);
        const float r = halfSquare * (1.0f - 0.66666666666666666f * f);
        return kFloat * Ln2High - ((r - (kFloat * Ln2Low + c)) - f);
    }
    const float s = f / (2.0f + f);
    const float z = s * s;
    const float r = z * (Lp1 + z * (Lp2 + z * (Lp3 + z * (Lp4 + z * (Lp5 + z * (Lp6 + z * Lp7))))));
    if (k == 0) return f - (halfSquare - s * (halfSquare + r));
    return kFloat * Ln2High - ((halfSquare - (s * (halfSquare + r) + (kFloat * Ln2Low + c))) - f);
}

static_assert(std::numeric_limits<long double>::digits == 64, "guest long double is the x87 80-bit format");

struct ExtendedParts {
    std::uint64_t mantissa;
    std::uint16_t signExponent;
};

template <typename TValue, typename THypot>
TValue Hypot3(TValue x, TValue y, TValue z, THypot hypot) {
    if (std::isnan(x)) return x;
    if (std::isnan(y)) return y;
    if (std::isnan(z)) return z;
    if (std::isinf(x) || std::isinf(y) || std::isinf(z)) return std::numeric_limits<TValue>::infinity();
    if (x == 0) return hypot(y, z);
    if (y == 0) return hypot(x, z);
    if (z == 0) return hypot(x, y);
    x = std::fabs(x);
    y = std::fabs(y);
    z = std::fabs(z);
    if (z >= x && z >= y) return hypot(z, hypot(x, y));
    if (y >= x && y >= z) return hypot(y, hypot(x, z));
    return hypot(x, hypot(y, z));
}

ExtendedParts SplitExtended(long double value) {
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(value));
    ExtendedParts parts;
    std::memcpy(&parts.mantissa, bytes, sizeof(parts.mantissa));
    std::memcpy(&parts.signExponent, bytes + sizeof(parts.mantissa), sizeof(parts.signExponent));
    return parts;
}

long double ClearLowMantissaWord(long double value) {
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(value));
    std::memset(bytes, 0, sizeof(std::uint32_t));
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

constexpr long double AsinNumeratorCoefficients[] = {
    1.66666666666666666631e-01L, -4.16313987993683104320e-01L, 3.69068046323246813704e-01L, -1.36213932016738603108e-01L,
    1.78324189708471965733e-02L, -2.19216428382605211588e-04L, -7.10526623669075243183e-06L,
};
constexpr long double AsinDenominatorCoefficients[] = {
    -2.94788392796209867269e+00L, 3.27309890266528636716e+00L, -1.68285799854822427013e+00L, 3.90699412641738801874e-01L,
    -3.14365703596053263322e-02L,
};
constexpr long double HalfPiHigh = 1.57079632679489661926e+00L;
constexpr long double HalfPiLow = -2.50827880633416601173e-20L;
constexpr long double QuarterPiHigh = 7.85398163397448309628e-01L;

long double AsinNumerator(long double t) {
    const auto& p = AsinNumeratorCoefficients;
    return t * (p[0] + t * (p[1] + t * (p[2] + t * (p[3] + t * (p[4] + t * (p[5] + t * p[6]))))));
}

long double AsinDenominator(long double t) {
    const auto& q = AsinDenominatorCoefficients;
    return 1.0L + t * (q[0] + t * (q[1] + t * (q[2] + t * (q[3] + t * q[4]))));
}

template<typename T>
bool NanWithoutException(T value) {
    if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return (bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    } else if constexpr (sizeof(T) == sizeof(std::uint64_t)) {
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return (bits & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000);
    } else {
        static_assert(std::numeric_limits<T>::digits == 64);
        std::uint64_t significand;
        std::uint16_t signAndExponent;
        std::memcpy(&significand, &value, sizeof(significand));
        std::memcpy(&signAndExponent, reinterpret_cast<const unsigned char*>(&value) + sizeof(significand), sizeof(signAndExponent));
        return (signAndExponent & 0x7fff) == 0x7fff && (significand & UINT64_C(0x7fffffffffffffff)) != 0;
    }
}

template<typename T>
T GuestFmin(T x, T y) {
    if (NanWithoutException(x))
        return y;
    if (NanWithoutException(y))
        return x;
    if (std::signbit(x) != std::signbit(y))
        return std::signbit(x) ? x : y;
    return x < y ? x : y;
}

template<typename T>
T GuestFmax(T x, T y) {
    if (NanWithoutException(x))
        return y;
    if (NanWithoutException(y))
        return x;
    if (std::signbit(x) != std::signbit(y))
        return std::signbit(x) ? y : x;
    return x < y ? y : x;
}

float GuestBesselSecondKindZero() {
    return -std::numeric_limits<float>::infinity();
}

float ScaledHyperbolic(float x, float y, bool sine) {
    const auto xBits = std::bit_cast<std::uint32_t>(x);
    const auto xMagnitude = xBits & 0x7fffffffu;
    const auto yMagnitude = std::bit_cast<std::uint32_t>(y) & 0x7fffffffu;
    constexpr std::uint32_t infinity = 0x7f800000u;
    if (xMagnitude > infinity) return x;
    if (yMagnitude > infinity) return y;
    if ((xMagnitude == infinity && yMagnitude == 0) || (sine && xMagnitude == 0 && yMagnitude == infinity)) {
        errno = EDOM;
        std::feraiseexcept(FE_INVALID);
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (xMagnitude == infinity) return sine ? x * y : std::copysign(std::numeric_limits<float>::infinity(), y);
    if (xMagnitude == 0 || yMagnitude == 0 || yMagnitude == infinity) return sine ? x * y : y;
    constexpr double limit = 2.0 * std::numeric_limits<float>::max_exponent;
    const float magnitude = std::bit_cast<float>(xMagnitude);
    const double argument = magnitude < limit ? static_cast<double>(magnitude) : limit;
    const double value = static_cast<double>(y) * (sine ? std::sinh(argument) : std::cosh(argument));
    const bool exponential = magnitude >= (sine ? 7.7f : 8.675f);
    float result;
    if (exponential && std::fabs(value) >= 0x1p128) {
        result = std::copysign(std::numeric_limits<float>::infinity(), y);
    } else {
        result = static_cast<float>(value);
    }
    if (exponential && std::isinf(result)) {
        errno = ERANGE;
        std::feraiseexcept(FE_OVERFLOW | FE_INEXACT);
    }
    return sine && (xBits & 0x80000000u) != 0 ? -result : result;
}

}

extern "C" {

std::lldiv_t APS5_VABI lldiv_nid_postfix(long long numerator, long long denominator) {
    return std::lldiv(numerator, denominator);
}

std::lldiv_t APS5_VABI ldiv_nid_postfix(std::int64_t numerator, std::int64_t denominator) {
    return std::lldiv(numerator, denominator);
}

std::lldiv_t APS5_VABI imaxdiv_nid_postfix(std::int64_t numerator, std::int64_t denominator) {
    return std::lldiv(numerator, denominator);
}

std::int64_t APS5_VABI labs_nid_postfix(std::int64_t x) {
    return x < 0 ? static_cast<std::int64_t>(0u - static_cast<std::uint64_t>(x)) : x;
}

std::int64_t APS5_VABI llabs_nid_postfix(std::int64_t x) {
    return labs_nid_postfix(x);
}

std::int64_t APS5_VABI imaxabs_nid_postfix(std::int64_t x) {
    return labs_nid_postfix(x);
}

float APS5_VABI fmodf_nid_postfix(float x, float y) {
    const int saved_errno = errno;
    const float result = std::fmod(x, y);
    errno = saved_errno;
    return result;
}
float APS5_VABI asinf_nid_postfix(float x) {
    const int saved_errno = errno;
    const float result = std::asin(x);
    errno = saved_errno;
    return result;
}
float APS5_VABI acosf_nid_postfix(float x) {
    if (std::isnan(x)) {
        return std::bit_cast<float>(std::bit_cast<std::uint32_t>(x) | 0x00400000u);
    }
    if (std::fabs(x) > 1.0f) {
        return std::bit_cast<float>(0xffc00000u);
    }
    const int saved_errno = errno;
    const float result = std::acos(x);
    errno = saved_errno;
    return result;
}
float APS5_VABI atan2f_nid_postfix(float y, float x) { return std::atan2(y, x); }
float APS5_VABI hypotf_nid_postfix(float x, float y) { return static_cast<float>(std::hypot(static_cast<double>(x), static_cast<double>(y))); }
double APS5_VABI hypot_nid_postfix(double x, double y) {
    const std::uint64_t hx = std::bit_cast<std::uint64_t>(x);
    const std::uint64_t hy = std::bit_cast<std::uint64_t>(y);
    const std::uint64_t ix = hx & 0x7fffffffffffffffull;
    const std::uint64_t iy = hy & 0x7fffffffffffffffull;
    if (ix == 0x7ff0000000000000ull || iy == 0x7ff0000000000000ull) {
        return std::numeric_limits<double>::infinity();
    }
    return std::hypot(x, y);
}
float APS5_VABI tanf_nid_postfix(float x) { return std::tan(x); }
float APS5_VABI log10f_nid_postfix(float x) { return std::log10(x); }
float APS5_VABI logbf_nid_postfix(float x) { return std::logb(x); }
double APS5_VABI exp2_nid_postfix(double x) {
    int saved_errno = errno;
    double result = std::exp2(x);
    errno = saved_errno;
    return result;
}
double APS5_VABI ldexp_nid_postfix(double x, int exponent) { return std::ldexp(x, exponent); }
double APS5_VABI scalbn_nid_postfix(double x, int exponent) { return std::scalbn(x, exponent); }
float APS5_VABI scalbnf_nid_postfix(float x, int exponent) { return std::scalbn(x, exponent); }
long double APS5_VABI scalbnl_nid_postfix(long double x, int exponent) { return std::scalbn(x, exponent); }
float APS5_VABI significandf_nid_postfix(float x) { return x == 0.0f || !std::isfinite(x) ? x : std::scalbn(x, -std::ilogb(x)); }
double APS5_VABI significand_nid_postfix(double x) { return x == 0.0 || !std::isfinite(x) ? x : std::scalbn(x, -std::ilogb(x)); }
double APS5_VABI frexp_nid_postfix(double x, int* exponent) { return std::frexp(x, exponent); }
float APS5_VABI frexpf_nid_postfix(float x, int* exponent) { return std::frexp(x, exponent); }
// Guest long is 64-bit, including on Windows where native long is 32-bit.
std::int64_t APS5_VABI lround_nid_postfix(double x) { return RoundToInt64(x); }
std::int64_t APS5_VABI lroundf_nid_postfix(float x) { return RoundToInt64(x); }
std::int64_t APS5_VABI llround_nid_postfix(double x) { return RoundToInt64(x); }
std::int64_t APS5_VABI llroundf_nid_postfix(float x) { return RoundToInt64(x); }
std::int64_t APS5_VABI llroundl_nid_postfix(long double x) { return RoundToInt64(x); }
int APS5_VABI __isfinitef_nid_postfix(float x) { return std::isfinite(x) ? 1 : 0; }
int APS5_VABI __isnormal_nid_postfix(double x) { return std::isnormal(x) ? 1 : 0; }
int APS5_VABI __isnormalf_nid_postfix(float x) { return std::isnormal(x) ? 1 : 0; }
int APS5_VABI __isinff_nid_postfix(float x) { return std::isinf(x) ? 1 : 0; }
float APS5_VABI nearbyintf_nid_postfix(float x) { return std::nearbyintf(x); }
long double APS5_VABI nearbyintl_nid_postfix(long double x) { return std::nearbyint(x); }
double APS5_VABI nearbyint_nid_postfix(double x) { return std::nearbyint(x); }

double APS5_VABI cbrt_nid_postfix(double x) { return std::cbrt(x); }
double APS5_VABI asin_nid_postfix(double x) {
    if (std::isgreater(x, 1.0) || std::isless(x, -1.0)) {
        std::feraiseexcept(FE_INVALID);
        return -std::numeric_limits<double>::quiet_NaN();
    }
    return std::asin(x);
}
double APS5_VABI acos_nid_postfix(double x) { return std::acos(x); }
double APS5_VABI sqrt_nid_postfix(double x) { return std::sqrt(x); }
float APS5_VABI sqrtf_nid_postfix(float x) { return std::sqrt(x); }
double APS5_VABI fabs_nid_postfix(double x) { return std::fabs(x); }
float APS5_VABI fabsf_nid_postfix(float x) { return std::fabs(x); }
long double APS5_VABI fabsl_nid_postfix(long double x) { return std::fabs(x); }
double APS5_VABI copysign_nid_postfix(double x, double y) {
    return std::copysign(x, y);
}
float APS5_VABI copysignf_nid_postfix(float x, float y) {
    return std::copysign(x, y);
}
int APS5_VABI abs_nid_postfix(int x) { return x < 0 ? static_cast<int>(0u - static_cast<unsigned>(x)) : x; }
double APS5_VABI trunc_nid_postfix(double x) { return std::trunc(x); }
double APS5_VABI ceil_nid_postfix(double x) { return std::ceil(x); }
double APS5_VABI floor_nid_postfix(double x) { return std::floor(x); }
float APS5_VABI truncf_nid_postfix(float x) { return std::trunc(x); }
float APS5_VABI ceilf_nid_postfix(float x) { return std::ceil(x); }
float APS5_VABI floorf_nid_postfix(float x) { return std::floor(x); }
double APS5_VABI fma_nid_postfix(double x, double y, double z) { return std::fma(x, y, z); }
float APS5_VABI fmaf_nid_postfix(float x, float y, float z) { return std::fma(x, y, z); }
double APS5_VABI fmin_nid_postfix(double x, double y) { return GuestFmin(x, y); }
float APS5_VABI fminf_nid_postfix(float x, float y) { return GuestFmin(x, y); }
long double APS5_VABI fminl_nid_postfix(long double x, long double y) { return GuestFmin(x, y); }
double APS5_VABI fmax_nid_postfix(double x, double y) { return GuestFmax(x, y); }
float APS5_VABI fmaxf_nid_postfix(float x, float y) { return GuestFmax(x, y); }
long double APS5_VABI fmaxl_nid_postfix(long double x, long double y) { return GuestFmax(x, y); }
double APS5_VABI fdim_nid_postfix(double x, double y) { return std::fdim(x, y); }
float APS5_VABI fdimf_nid_postfix(float x, float y) { return std::fdim(x, y); }
long double APS5_VABI acosl_nid_postfix(long double x) { return std::acos(x); }
long double APS5_VABI acoshl_nid_postfix(long double x) { return std::acosh(x); }
long double APS5_VABI asinhl_nid_postfix(long double x) { return std::asinh(x); }
long double APS5_VABI atanhl_nid_postfix(long double x) { return std::atanh(x); }
long double APS5_VABI atan2l_nid_postfix(long double y, long double x) { return std::atan2(y, x); }
long double APS5_VABI cbrtl_nid_postfix(long double x) { return std::cbrt(x); }
long double APS5_VABI erfl_nid_postfix(long double x) { return std::erf(x); }
long double APS5_VABI erfcl_nid_postfix(long double x) { return std::erfc(x); }
long double APS5_VABI expm1l_nid_postfix(long double x) { return std::expm1(x); }
long double APS5_VABI log1pl_nid_postfix(long double x) { return std::log1p(x); }
long double APS5_VABI log2l_nid_postfix(long double x) { return std::log2(x); }
long double APS5_VABI lgammal_nid_postfix(long double x) { return std::lgamma(x); }
long double APS5_VABI tgammal_nid_postfix(long double x) { return std::tgamma(x); }
float APS5_VABI tgammaf_nid_postfix(float x) { return static_cast<float>(std::tgamma(static_cast<double>(x))); }
long double APS5_VABI hypotl_nid_postfix(long double x, long double y) {
    if (std::isinf(x) || std::isinf(y)) return std::numeric_limits<long double>::infinity();
    return std::hypot(x, y);
}
float APS5_VABI hypot3f_nid_postfix(float x, float y, float z) { return Hypot3(x, y, z, hypotf_nid_postfix); }
double APS5_VABI hypot3_nid_postfix(double x, double y, double z) { return Hypot3(x, y, z, hypot_nid_postfix); }
long double APS5_VABI hypot3l_nid_postfix(long double x, long double y, long double z) { return Hypot3(x, y, z, hypotl_nid_postfix); }
float APS5_VABI j0f_nid_postfix(float x) { return static_cast<float>(::j0(x)); }
float APS5_VABI j1f_nid_postfix(float x) { return static_cast<float>(::j1(x)); }
float APS5_VABI jnf_nid_postfix(int order, float x) { return static_cast<float>(::jn(order, x)); }
float APS5_VABI y0f_nid_postfix(float x) { return x == 0.0f ? GuestBesselSecondKindZero() : static_cast<float>(::y0(x)); }
float APS5_VABI y1f_nid_postfix(float x) { return x == 0.0f ? GuestBesselSecondKindZero() : static_cast<float>(::y1(x)); }
float APS5_VABI ynf_nid_postfix(int order, float x) { return x == 0.0f ? GuestBesselSecondKindZero() : static_cast<float>(::yn(order, x)); }

long double APS5_VABI asinl_nid_postfix(long double x) {
    const ExtendedParts parts = SplitExtended(x);
    const int exponent = parts.signExponent & 0x7fff;
    if (exponent >= 0x3fff) {
        if (exponent == 0x3fff && (parts.mantissa & 0x7fffffffffffffffull) == 0) return x * HalfPiHigh + x * HalfPiLow;
        std::feraiseexcept(FE_INVALID);
        errno = EDOM;
        return (x - x) / (x - x);
    }
    if (exponent < 0x3ffe) {
        if (exponent < 0x3fff - 32 && 1.0e300L + x > 1.0L) return x;
        const long double t = x * x;
        return x + x * (AsinNumerator(t) / AsinDenominator(t));
    }
    const long double t = (1.0L - std::fabs(x)) * 0.5L;
    const long double p = AsinNumerator(t);
    const long double q = AsinDenominator(t);
    const long double s = std::sqrt(t);
    long double result;
    if ((parts.mantissa >> 32) >= 0xf3333333u) {
        result = HalfPiHigh - (2.0L * (s + s * (p / q)) - HalfPiLow);
    } else {
        const long double w = ClearLowMantissaWord(s);
        const long double c = (t - w * w) / (s + w);
        const long double r = p / q;
        result = QuarterPiHigh - ((2.0L * s * r - (HalfPiLow - 2.0L * c)) - (QuarterPiHigh - 2.0L * w));
    }
    return (parts.signExponent & 0x8000) != 0 ? -result : result;
}

long double APS5_VABI frexpl_nid_postfix(long double x, int* exponent) {
    if (!std::isfinite(x)) return x;
    return std::frexp(x, exponent);
}
double APS5_VABI exp_nid_postfix(double x) { return std::exp(x); }
double APS5_VABI atan_nid_postfix(double x) { return std::atan(x); }
double APS5_VABI tan_nid_postfix(double x) { return std::tan(x); }
double APS5_VABI log2_nid_postfix(double x) { return std::log2(x); }
double APS5_VABI log_nid_postfix(double x) { return std::log(x); }
double APS5_VABI log1p_nid_postfix(double x) { return std::log1p(x); }
float APS5_VABI log1pf_nid_postfix(float x) { return std::log1p(x); }
static double Expm1(double x) {
    const double tiny = 1.0e-300;
    const double o_threshold = 7.09782712893383973096e+02;
    const double ln2_hi = 6.93147180369123816490e-01;
    const double ln2_lo = 1.90821492927058770002e-10;
    const double invln2 = 1.44269504088896338700e+00;
    const double Q1 = -3.33333333333331316428e-02;
    const double Q2 = 1.58730158725481460165e-03;
    const double Q3 = -7.93650757867487942473e-05;
    const double Q4 = 4.00821782732936239552e-06;
    const double Q5 = -2.01099218183624371326e-07;
    static volatile double huge = 1.0e+300;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::uint32_t hx = static_cast<std::uint32_t>(bits >> 32);
    const std::uint32_t xsb = hx & 0x80000000u;
    hx &= 0x7fffffff;
    if (hx >= 0x4043687a) {
        if (hx >= 0x40862e42) {
            if (hx >= 0x7ff00000) {
                if (((hx & 0xfffff) | static_cast<std::uint32_t>(bits)) != 0) return x + x;
                return xsb == 0 ? x : -1.0;
            }
            if (x > o_threshold) return huge * huge;
        }
        if (xsb != 0 && x + tiny < 0.0) return tiny - 1.0;
    }
    double c = 0.0;
    std::int32_t k;
    if (hx > 0x3fd62e42) {
        double hi;
        double lo;
        if (hx < 0x3ff0a2b2) {
            if (xsb == 0) {
                hi = x - ln2_hi;
                lo = ln2_lo;
                k = 1;
            } else {
                hi = x + ln2_hi;
                lo = -ln2_lo;
                k = -1;
            }
        } else {
            k = static_cast<std::int32_t>(invln2 * x + (xsb == 0 ? 0.5 : -0.5));
            const double t = k;
            hi = x - t * ln2_hi;
            lo = t * ln2_lo;
        }
        x = hi - lo;
        c = (hi - x) - lo;
    } else if (hx < 0x3c900000) {
        const double t = huge + x;
        return x - (t - (huge + x));
    } else {
        k = 0;
    }
    const double hfx = 0.5 * x;
    const double hxs = x * hfx;
    const double r1 = 1.0 + hxs * (Q1 + hxs * (Q2 + hxs * (Q3 + hxs * (Q4 + hxs * Q5))));
    double t = 3.0 - r1 * hfx;
    double e = hxs * ((r1 - t) / (6.0 - x * t));
    if (k == 0) return x - (x * e - hxs);
    const double twopk = std::bit_cast<double>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(0x3ff + k) << 20) << 32);
    e = (x * (e - c) - c);
    e -= hxs;
    if (k == -1) return 0.5 * (x - e) - 0.5;
    if (k == 1) {
        if (x < -0.25) return -2.0 * (e - (x + 0.5));
        return 1.0 + 2.0 * (x - e);
    }
    double y;
    if (k <= -2 || k > 56) {
        y = 1.0 - (e - x);
        if (k == 1024) {
            y = y * 2.0 * 0x1p1023;
        } else {
            y = y * twopk;
        }
        return y - 1.0;
    }
    if (k < 20) {
        t = std::bit_cast<double>(static_cast<std::uint64_t>(0x3ff00000u - (0x200000u >> k)) << 32);
        y = t - (e - x);
        y = y * twopk;
    } else {
        t = std::bit_cast<double>(static_cast<std::uint64_t>(static_cast<std::uint32_t>((0x3ff - k) << 20)) << 32);
        y = x - (e + t);
        y += 1.0;
        y = y * twopk;
    }
    return y;
}

double APS5_VABI expm1_nid_postfix(double x) {
    const double result = Expm1(x);
    if (std::isinf(result) && std::isfinite(x)) {
        errno = ERANGE;
    }
    return result;
}
float APS5_VABI expm1f_nid_postfix(float x) { return std::expm1(x); }
float APS5_VABI erff_nid_postfix(float x) { return std::erf(x); }
double APS5_VABI erf_nid_postfix(double x) { return std::erf(x); }
double APS5_VABI erfc_nid_postfix(double x) { return std::erfc(x); }
float APS5_VABI erfcf_nid_postfix(float x) { return std::erfc(x); }
int signgam_nid_postfix = 0;

double APS5_VABI lgamma_nid_postfix(double x) {
    const double whole = std::floor(x);
    if (x == 0) signgam_nid_postfix = std::signbit(x) ? -1 : 1;
    else signgam_nid_postfix = x < 0 && whole != x && std::fmod(whole, 2.0) != 0 ? -1 : 1;
    const std::uint64_t magnitude = std::bit_cast<std::uint64_t>(x) & 0x7fffffffffffffffull;
    if (magnitude != 0 && magnitude < 0x3c70000000000000ull) return -std::log(std::fabs(x));
    return std::lgamma(x);
}

float APS5_VABI sinf_nid_postfix(float x) { return std::sin(x); }
float APS5_VABI cosf_nid_postfix(float x) { return std::cos(x); }

void APS5_VABI sincosf_nid_postfix(float x, float* sinp, float* cosp) {
    *sinp = std::sin(x);
    *cosp = std::cos(x);
}

double APS5_VABI sin_nid_postfix(double x) { return std::sin(x); }
double APS5_VABI cos_nid_postfix(double x) { return std::cos(x); }
double APS5_VABI sinh_nid_postfix(double x) { return std::sinh(x); }
double APS5_VABI cosh_nid_postfix(double x) { return std::cosh(x); }
float APS5_VABI coshf_nid_postfix(float x) {
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (magnitude >= 0x7f800000u) return x * x;
    const float absolute = std::fabs(x);
    if (magnitude < 0x3eb17218u) {
        const float t = SinhfExpm1(absolute);
        const float w = 1.0f + t;
        if (magnitude < 0x39800000u) return 1.0f;
        return 1.0f + (t * t) / (w + w);
    }
    if (magnitude < 0x41100000u) {
        const float t = SinhfExp(absolute);
        return 0.5f * t + 0.5f / t;
    }
    if (magnitude < 0x42b17217u) return 0.5f * SinhfExp(absolute);
    if (magnitude <= 0x42b2d4fcu) {
        const float w = SinhfExp(0.5f * absolute);
        const float t = 0.5f * w;
        return t * w;
    }
    volatile float huge = 1.0e30f;
    return huge * huge;
}

float APS5_VABI sinhf_nid_postfix(float x) {
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (magnitude >= 0x7f800000u) return x + x;
    const float half = std::signbit(x) ? -0.5f : 0.5f;
    const float absolute = std::fabs(x);
    if (magnitude < 0x41100000u) {
        if (magnitude < 0x39800000u && 1.0e37f + x > 1.0f) return x;
        const float t = SinhfExpm1(absolute);
        if (magnitude < 0x3f800000u) return half * (2.0f * t - t * t / (t + 1.0f));
        return half * (t + t / (t + 1.0f));
    }
    if (magnitude < 0x42b17217u) return half * SinhfExp(absolute);
    if (magnitude <= 0x42b2d4fcu) {
        const float w = SinhfExp(0.5f * absolute);
        return half * w * w;
    }
    errno = ERANGE;
    return x * 1.0e37f;
}

double APS5_VABI asinh_nid_postfix(double x) { return std::asinh(x); }
float APS5_VABI asinhf_nid_postfix(float x) { return std::asinh(x); }
double APS5_VABI acosh_nid_postfix(double x) { return std::acosh(x); }
float APS5_VABI acoshf_nid_postfix(float x) { return std::acosh(x); }
double APS5_VABI atanh_nid_postfix(double x) { return std::atanh(x); }

float APS5_VABI atanhf_nid_postfix(float x) {
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (magnitude > 0x3f800000u) {
        std::feraiseexcept(FE_INVALID);
        errno = EDOM;
        return (x - x) / (x - x);
    }
    if (magnitude == 0x3f800000u) {
        errno = ERANGE;
        return x / 0.0f;
    }
    if (magnitude < 0x31800000u && 1.0e30f + x > 0.0f) return x;
    const float absolute = std::fabs(x);
    const float twice = absolute + absolute;
    const float t = magnitude < 0x3f000000u ? 0.5f * AtanhfLog1p(twice + twice * absolute / (1.0f - absolute))
                                            : 0.5f * AtanhfLog1p(twice / (1.0f - absolute));
    return std::signbit(x) ? -t : t;
}

void APS5_VABI sincos_nid_postfix(double x, double* sinp, double* cosp) {
    *sinp = std::sin(x);
    *cosp = std::cos(x);
}

float APS5_VABI atanf_nid_postfix(float x) {
    static constexpr float atanhi[] = {static_cast<float>(4.6364760399e-01), static_cast<float>(7.8539812565e-01), static_cast<float>(9.8279368877e-01), static_cast<float>(1.5707962513e+00)};
    static constexpr float atanlo[] = {static_cast<float>(5.0121582440e-09), static_cast<float>(3.7748947079e-08), static_cast<float>(3.4473217170e-08), static_cast<float>(7.5497894159e-08)};
    static constexpr float aT[] = {static_cast<float>(3.3333328366e-01), static_cast<float>(-1.9999158382e-01), static_cast<float>(1.4253635705e-01), static_cast<float>(-1.0648017377e-01), static_cast<float>(6.1687607318e-02)};
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x4c800000) {
        if (ix > 0x7f800000) {
            return x + x;
        }
        const volatile float lo = atanlo[3];
        return hx > 0 ? atanhi[3] + lo : -atanhi[3] - lo;
    }
    int id = -1;
    if (ix < 0x3ee00000) {
        if (ix < 0x39800000) {
            return x;
        }
    } else {
        x = std::fabs(x);
        if (ix < 0x3f300000) {
            id = 0;
            x = (2.0f * x - 1.0f) / (2.0f + x);
        } else if (ix < 0x3f980000) {
            id = 1;
            x = (x - 1.0f) / (x + 1.0f);
        } else if (ix < 0x401c0000) {
            id = 2;
            x = (x - 1.5f) / (1.0f + 1.5f * x);
        } else {
            id = 3;
            x = -1.0f / x;
        }
    }
    const float z = x * x;
    const float w = z * z;
    const float s1 = z * (aT[0] + w * (aT[2] + w * aT[4]));
    const float s2 = w * (aT[1] + w * aT[3]);
    if (id < 0) {
        return x - x * (s1 + s2);
    }
    const float result = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
    return hx < 0 ? -result : result;
}
double APS5_VABI atan2_nid_postfix(double y, double x) { return std::atan2(y, x); }
float APS5_VABI powf_nid_postfix(float base, float exp) {
    if (base == 1.0f) return 1.0f;
    return std::pow(base, exp);
}
double APS5_VABI pow_nid_postfix(double base, double exp) { return std::pow(base, exp); }
float APS5_VABI __powisf2_nid_postfix(float base, int exponent) {
    const bool reciprocal = exponent < 0;
    float result = 1.0f;
    unsigned int magnitude = reciprocal ? 0u - static_cast<unsigned int>(exponent) : static_cast<unsigned int>(exponent);
    while (magnitude != 0u) {
        if ((magnitude & 1u) != 0u) result *= base;
        magnitude >>= 1u;
        if (magnitude == 0u) break;
        base *= base;
    }
    return reciprocal ? 1.0f / result : result;
}
double APS5_VABI __powidf2_nid_postfix(double base, int exponent) {
    const bool reciprocal = exponent < 0;
    double result = 1.0;
    unsigned int magnitude = reciprocal ? 0u - static_cast<unsigned int>(exponent) : static_cast<unsigned int>(exponent);
    while (magnitude != 0u) {
        if ((magnitude & 1u) != 0u) result *= base;
        magnitude >>= 1u;
        if (magnitude == 0u) break;
        base *= base;
    }
    return reciprocal ? 1.0 / result : result;
}
float APS5_VABI expf_nid_postfix(float x) { return std::exp(x); }
float APS5_VABI exp2f_nid_postfix(float x) { return std::exp2(x); }
float APS5_VABI logf_nid_postfix(float x) {
    const float ln2_hi = static_cast<float>(6.9313812256e-01);
    const float ln2_lo = static_cast<float>(9.0580006145e-06);
    const float two25 = static_cast<float>(3.355443200e+07);
    const float Lg1 = 0xaaaaaa.0p-24f;
    const float Lg2 = 0xccce13.0p-25f;
    const float Lg3 = 0x91e9ee.0p-25f;
    const float Lg4 = 0xf89e26.0p-26f;
    static volatile float vzero = 0.0f;
    std::int32_t k = 0;
    std::int32_t ix = std::bit_cast<std::int32_t>(x);
    if (ix < 0x00800000) {
        if ((ix & 0x7fffffff) == 0) {
            errno = ERANGE;
            return -two25 / vzero;
        }
        if (ix < 0) {
            errno = EDOM;
            return (x - x) / 0.0f;
        }
        k -= 25;
        x *= two25;
        ix = std::bit_cast<std::int32_t>(x);
    }
    if (ix >= 0x7f800000) {
        return x + x;
    }
    k += (ix >> 23) - 127;
    ix &= 0x007fffff;
    std::int32_t i = (ix + (0x95f64 << 3)) & 0x800000;
    x = std::bit_cast<float>(ix | (i ^ 0x3f800000));
    k += i >> 23;
    const float f = x - 1.0f;
    const float dk = static_cast<float>(k);
    if ((0x007fffff & (0x8000 + ix)) < 0xc000) {
        if (f == 0.0f) {
            return k == 0 ? 0.0f : dk * ln2_hi + dk * ln2_lo;
        }
        const float R = f * f * (0.5f - static_cast<float>(0.33333333333333333) * f);
        return k == 0 ? f - R : dk * ln2_hi - ((R - dk * ln2_lo) - f);
    }
    const float s = f / (2.0f + f);
    const float z = s * s;
    i = ix - (0x6147a << 3);
    const float w = z * z;
    const std::int32_t j = (0x6b851 << 3) - ix;
    const float t1 = w * (Lg2 + w * Lg4);
    const float t2 = z * (Lg1 + w * Lg3);
    i |= j;
    const float R = t2 + t1;
    if (i > 0) {
        const float hfsq = 0.5f * f * f;
        return k == 0 ? f - (hfsq - s * (hfsq + R)) : dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
    }
    return k == 0 ? f - s * (f - R) : dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}
float APS5_VABI log2f_nid_postfix(float x) { return std::log2(x); }
double APS5_VABI log10_nid_postfix(double x) { return std::log10(x); }
float APS5_VABI ldexpf_nid_postfix(float x, int exp) { return std::ldexp(x, exp); }
double APS5_VABI fmod_nid_postfix(double x, double y) {
    int saved_errno = errno;
    double result = std::fmod(x, y);
    errno = saved_errno;
    return result;
}
float APS5_VABI roundf_nid_postfix(float x) { return std::round(x); }
double APS5_VABI round_nid_postfix(double x) { return std::round(x); }
float APS5_VABI cbrtf_nid_postfix(float x) {
    const std::uint32_t B1 = 709958130;
    const std::uint32_t B2 = 642849266;
    std::uint32_t hx = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t sign = hx & 0x80000000u;
    hx ^= sign;
    if (hx >= 0x7f800000u) {
        return x + x;
    }
    float t;
    if (hx < 0x00800000u) {
        if (hx == 0) {
            return x;
        }
        t = 0x1p24f * x;
        const std::uint32_t high = std::bit_cast<std::uint32_t>(t);
        t = std::bit_cast<float>(sign | ((high & 0x7fffffffu) / 3 + B2));
    } else {
        t = std::bit_cast<float>(sign | (hx / 3 + B1));
    }
    double T = t;
    double r = T * T * T;
    T = T * (static_cast<double>(x) + x + r) / (x + r + r);
    r = T * T * T;
    T = T * (static_cast<double>(x) + x + r) / (x + r + r);
    return static_cast<float>(T);
}
static long double PartialRemainder(long double x, long double y, std::uint16_t* status) {
    long double remainder;
    asm("1:\n\tfprem1\n\tfnstsw %%ax\n\ttestw $0x400, %%ax\n\tjnz 1b" : "=t"(remainder), "=a"(*status) : "0"(x), "u"(y));
    return remainder;
}

static void SetRemainderDomainError(long double x, long double y) {
    if (!std::isnan(x) && !std::isnan(y) && (y == 0 || std::isinf(x))) {
        errno = EDOM;
    }
}

static int RemquoQuotient(std::uint16_t status, bool negative) {
    const int quotient = ((status >> 8) & 1) << 2 | ((status >> 14) & 1) << 1 | ((status >> 9) & 1);
    return negative ? -quotient : quotient;
}

float APS5_VABI remainderf_nid_postfix(float x, float y) {
    std::uint16_t status;
    const long double remainder = PartialRemainder(x, y, &status);
    SetRemainderDomainError(x, y);
    return static_cast<float>(remainder);
}
float APS5_VABI nextafterf_nid_postfix(float x, float y) { return std::nextafter(x, y); }
double APS5_VABI nextafter_nid_postfix(double x, double y) { return std::nextafter(x, y); }
int APS5_VABI __isfinite_nid_postfix(double x) { return std::isfinite(x) ? 1 : 0; }
int APS5_VABI __isnan_nid_postfix(double x) { return std::isnan(x) ? 1 : 0; }
int APS5_VABI __isinf_nid_postfix(double x) { return std::isinf(x) ? 1 : 0; }
int APS5_VABI __signbit_nid_postfix(double x) { return std::signbit(x) ? 1 : 0; }
float APS5_VABI nanf_nid_postfix(const char* tagp) {
    (void)tagp;
    return std::bit_cast<float>(0x7fc00000u);
}
double APS5_VABI nan_nid_postfix(const char* tagp) {
    (void)tagp;
    return std::bit_cast<double>(0x7ff8000000000000ull);
}

double APS5_VABI modf_nid_postfix(double x, double* integral) { return std::modf(x, integral); }
float APS5_VABI modff_nid_postfix(float x, float* integral) { return std::modf(x, integral); }
double APS5_VABI tanh_nid_postfix(double x) { return std::tanh(x); }
float APS5_VABI tanhf_nid_postfix(float x) { return std::tanh(x); }
long double APS5_VABI atanl_nid_postfix(long double x) { return std::atan(x); }
long double APS5_VABI sinl_nid_postfix(long double x) { return std::sin(x); }
long double APS5_VABI cosl_nid_postfix(long double x) { return std::cos(x); }
long double APS5_VABI tanl_nid_postfix(long double x) { return std::tan(x); }
long double APS5_VABI sinhl_nid_postfix(long double x) { return std::sinh(x); }
long double APS5_VABI coshl_nid_postfix(long double x) { return std::cosh(x); }
long double APS5_VABI tanhl_nid_postfix(long double x) { return std::tanh(x); }
long double APS5_VABI expl_nid_postfix(long double x) { return std::exp(x); }
long double APS5_VABI exp2l_nid_postfix(long double x) { return std::exp2(x); }
long double APS5_VABI logl_nid_postfix(long double x) { return std::log(x); }
long double APS5_VABI log10l_nid_postfix(long double x) { return std::log10(x); }
long double APS5_VABI powl_nid_postfix(long double base, long double exp) { return std::pow(base, exp); }
long double APS5_VABI ldexpl_nid_postfix(long double x, int exp) { return std::ldexp(x, exp); }
long double APS5_VABI truncl_nid_postfix(long double x) { return std::trunc(x); }
long double APS5_VABI ceill_nid_postfix(long double x) { return std::ceil(x); }
long double APS5_VABI floorl_nid_postfix(long double x) { return std::floor(x); }
double APS5_VABI logb_nid_postfix(double x) { return std::logb(x); }
int APS5_VABI ilogb_nid_postfix(double x) {
    if (x == 0.0) {
        return INT_MIN;
    }
    if (!std::isfinite(x)) {
        return INT_MAX;
    }
    return std::ilogb(x);
}
int APS5_VABI ilogbf_nid_postfix(float x) { return ilogb_nid_postfix(static_cast<double>(x)); }
double APS5_VABI remainder_nid_postfix(double x, double y) {
    std::uint16_t status;
    const long double remainder = PartialRemainder(x, y, &status);
    SetRemainderDomainError(x, y);
    return static_cast<double>(remainder);
}
double APS5_VABI remquo_nid_postfix(double x, double y, int* quo) {
    std::uint16_t status;
    const long double remainder = PartialRemainder(x, y, &status);
    *quo = RemquoQuotient(status, std::signbit(x) != std::signbit(y));
    SetRemainderDomainError(x, y);
    return static_cast<double>(remainder);
}
float APS5_VABI remquof_nid_postfix(float x, float y, int* quo) {
    std::uint16_t status;
    const long double remainder = PartialRemainder(x, y, &status);
    *quo = RemquoQuotient(status, std::signbit(x) != std::signbit(y));
    SetRemainderDomainError(x, y);
    return static_cast<float>(remainder);
}
float APS5_VABI _FSinh_nid_postfix(float x, float y) { return ScaledHyperbolic(x, y, true); }
float APS5_VABI _FCosh_nid_postfix(float x, float y) { return ScaledHyperbolic(x, y, false); }

struct alignas(16) LibcFloatConstant { std::uint32_t bits[4]; };
LibcFloatConstant _FInf_nid_postfix {{0x7f800000u, 0, 0, 0}};
LibcFloatConstant _FNan_nid_postfix {{0x7fc00000u, 0, 0, 0}};

short APS5_VABI _FDtest_nid_postfix(const float* value) {
    constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;
    if (value == nullptr) throw std::invalid_argument("_FDtest: null value");
    std::uint32_t bits;
    std::memcpy(&bits, value, sizeof(bits));
    const auto exponent = bits & 0x7f800000u;
    const auto fraction = bits & 0x007fffffu;
    if (exponent == 0x7f800000u) return fraction != 0 ? NotANumber : Infinite;
    if (exponent == 0) return fraction != 0 ? Denormal : Zero;
    return Finite;
}

short APS5_VABI _Dtest_nid_postfix(const double* value) {
    constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;
    if (value == nullptr) throw std::invalid_argument("_Dtest: null value");
    std::uint64_t bits;
    std::memcpy(&bits, value, sizeof(bits));
    const auto exponent = bits & 0x7ff0000000000000u;
    const auto fraction = bits & 0x000fffffffffffffu;
    if (exponent == 0x7ff0000000000000u) return fraction != 0 ? NotANumber : Infinite;
    if (exponent == 0) return fraction != 0 ? Denormal : Zero;
    return Finite;
}

int APS5_VABI __isnanf_nid_postfix(float x) { return std::isnan(x) ? 1 : 0; }
int APS5_VABI isnanf_nid_postfix(float x) { return __isnanf_nid_postfix(x); }
int APS5_VABI isnan_nid_postfix(double x) { return __isnan_nid_postfix(x); }
int APS5_VABI isinf_nid_postfix(double x) { return __isinf_nid_postfix(x); }
int APS5_VABI __fpclassifyf_nid_postfix(float x) {
    constexpr int Infinite = 0x01, NotANumber = 0x02, Normal = 0x04, Subnormal = 0x08, Zero = 0x10;
    std::uint32_t bits;
    std::memcpy(&bits, &x, sizeof(bits));
    const auto exponent = bits & 0x7f800000u;
    const auto fraction = bits & 0x007fffffu;
    if (exponent == 0) return fraction != 0 ? Subnormal : Zero;
    if (exponent == 0x7f800000u) return fraction != 0 ? NotANumber : Infinite;
    return Normal;
}
int APS5_VABI __signbitf_nid_postfix(float x) { return std::signbit(x) ? 1 : 0; }
int APS5_VABI __signbitl_nid_postfix(long double x) {
    static_assert(sizeof(long double) == 16, "guest long double requires x87 extended precision storage");
    return std::signbit(x) ? 1 : 0;
}

int APS5_VABI __isfinitel_nid_postfix(long double x) {
    static_assert(sizeof(long double) == 16 && std::numeric_limits<long double>::digits == 64 && std::numeric_limits<long double>::max_exponent == 16384 && std::endian::native == std::endian::little, "guest long double requires little-endian x87 extended precision storage");
    std::uint16_t exponent;
    std::memcpy(&exponent, reinterpret_cast<const std::uint8_t*>(&x) + 8, sizeof(exponent));
    return (exponent & 0x7fffu) != 0x7fffu;
}

struct DinkumwareFloatCharacteristics {
    int digits;
    int mantissaDigits;
    int maxExponent10;
    int maxExponent;
    int minExponent10;
    int minExponent;
    alignas(16) std::uint16_t epsilon[8];
    alignas(16) std::uint16_t max[8];
    alignas(16) std::uint16_t min[8];
};
static_assert(sizeof(DinkumwareFloatCharacteristics) == 80 && offsetof(DinkumwareFloatCharacteristics, epsilon) == 32);

DinkumwareFloatCharacteristics _Flt_nid_postfix {6, 24, 38, 128, -37, -125,
    {0, 0x3400}, {0xffff, 0x7f7f}, {0, 0x0080}};
DinkumwareFloatCharacteristics _Dbl_nid_postfix {15, 53, 308, 1024, -307, -1021,
    {0, 0, 0, 0x3cb0}, {0xffff, 0xffff, 0xffff, 0x7fef}, {0, 0, 0, 0x0010}};
DinkumwareFloatCharacteristics _Ldbl_nid_postfix {18, 64, 4932, 16384, -4931, -16381,
    {0, 0, 0, 0x8000, 0x3fc0}, {0xffff, 0xffff, 0xffff, 0xffff, 0x7ffe}, {0, 0, 0, 0x8000, 0x0001}};

static std::mutex g_randLock;
static std::uint32_t g_randState = 1;

static int NextRand_nid_no_patch(std::uint32_t& state) {
    const std::int64_t x = static_cast<std::int64_t>(state % 0x7ffffffeu) + 1;
    std::int64_t next = 16807 * (x % 127773) - 2836 * (x / 127773);
    if (next < 0) next += 0x7fffffff;
    state = static_cast<std::uint32_t>(next - 1);
    return static_cast<int>(next - 1);
}

int APS5_VABI rand_nid_postfix() {
    std::lock_guard lock(g_randLock);
    return NextRand_nid_no_patch(g_randState);
}

int APS5_VABI rand_r_nid_postfix(unsigned int* state) {
    if (state == nullptr) throw std::invalid_argument("rand_r: null state");
    std::uint32_t value = *state;
    const int result = NextRand_nid_no_patch(value);
    *state = value;
    return result;
}

void APS5_VABI srand_nid_postfix(unsigned int seed) {
    std::lock_guard lock(g_randLock);
    g_randState = seed;
}

int APS5_VABI __fpclassifyd_nid_postfix(double value) {
    return SonyFpclassifyd(value);
}

double APS5_VABI _Sinh_nid_postfix(double x, double y) {
    return SonySinh(x, y);
}

double APS5_VABI _Cosh_nid_postfix(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return y * std::cosh(x);
    if (y == 0.0) return std::copysign(0.0, y);
    constexpr double Ln2High = 0x1.62e42fefa0000p-1;
    constexpr double Ln2Low = 0x1.cf79abc9e3b3ap-40;
    constexpr double InverseLn2 = 0x1.71547652b82fep+0;
    const double magnitude = std::abs(x);
    if (magnitude <= (std::numeric_limits<double>::max_exponent - 1) * (Ln2High + Ln2Low))
        return y * std::cosh(x);
    int scaleExponent;
    const double scale = std::frexp(y, &scaleExponent);
    constexpr int MaximumExponentSpan = std::numeric_limits<double>::max_exponent -
        std::numeric_limits<double>::min_exponent + std::numeric_limits<double>::digits + 4;
    if (magnitude > MaximumExponentSpan * (Ln2High + Ln2Low))
        return std::scalbn(scale, INT_MAX);
    const int exponent = static_cast<int>(magnitude * InverseLn2);
    const double reduced = std::fma(-exponent, Ln2Low, std::fma(-exponent, Ln2High, magnitude));
    return std::scalbn(scale * (0.5 * std::exp(reduced)), scaleExponent + exponent);
}

int APS5_VABI _Fltrounds_nid_postfix(void) {
    return SonyFltrounds();
}

struct GuestFloatingPointEnvironment {
    std::uint32_t control;
    std::uint32_t status;
    std::uint32_t tag;
    unsigned char other[16];
    std::uint32_t mxcsr;
};
static_assert(sizeof(GuestFloatingPointEnvironment) == 32 && offsetof(GuestFloatingPointEnvironment, mxcsr) == 28);

int APS5_VABI fegetenv_nid_postfix(GuestFloatingPointEnvironment* environment) {
    if (environment == nullptr) throw std::invalid_argument("fegetenv: null environment");
    asm volatile("fnstenv %0\n\tfldcw %0" : "+m"(*environment));
    environment->mxcsr = _mm_getcsr();
    return 0;
}

union SonyDconst {
    unsigned short _Word[8];
    float _Float;
    double _Double;
    long double _Long_double;
};

static_assert(std::endian::native == std::endian::little);
SonyDconst _Inf_nid_postfix {{0, 0, 0, 0x7ff0, 0, 0, 0, 0}};
SonyDconst _Denorm_nid_postfix {{1, 0, 0, 0, 0, 0, 0, 0}};
SonyDconst _Hugeval_nid_postfix {{0, 0, 0, 0x7ff0, 0, 0, 0, 0}};
SonyDconst _Nan_nid_postfix {{0, 0, 0, 0x7ff8, 0, 0, 0, 0}};
SonyDconst _Snan_nid_postfix {{1, 0, 0, 0x7ff0, 0, 0, 0, 0}};
LibcFloatConstant _FDenorm_nid_postfix {{0x00000001u, 0, 0, 0}};
LibcFloatConstant _FSnan_nid_postfix {{0x7f800001u, 0, 0, 0}};
SonyDconst _LDenorm_nid_postfix {{1, 0, 0, 0, 0, 0, 0, 0}};
SonyDconst _LInf_nid_postfix {{0, 0, 0, 0x8000, 0x7fff, 0, 0, 0}};
SonyDconst _LNan_nid_postfix {{0, 0, 0, 0xc000, 0x7fff, 0, 0, 0}};
SonyDconst _LSnan_nid_postfix {{1, 0, 0, 0x8000, 0x7fff, 0, 0, 0}};
}
