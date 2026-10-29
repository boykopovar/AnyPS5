#include "prx/libc/include/Api.hpp"
#include <mutex>
#include <array>
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

volatile float AcosfPio2Low = 7.5497894159e-08f;

float AcosfRational(float z) {
    constexpr float P0 = 1.6666586697e-01f, P1 = -4.2743422091e-02f, P2 = -8.6563630030e-03f, Q1 = -7.0662963390e-01f;
    return z * (P0 + z * (P1 + z * P2)) / (1.0f + z * Q1);
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

float GuestTgammaf(float x) {
    const int savedErrno = errno;
    const float result = static_cast<float>(std::tgamma(static_cast<double>(x)));
    errno = std::isfinite(x) && x < 0.0f && std::trunc(x) == x ? EDOM : savedErrno;
    if (std::isnan(result) && !std::isnan(x)) return std::bit_cast<float>(0xffc00000u);
    if (result != 0.0f || !std::isfinite(x) || x >= 0.0f || std::trunc(x) == x) return result;
    const double interval = std::floor(-static_cast<double>(x));
    const bool negative = std::fmod(interval, 2.0) == 0.0;
    return std::copysign(0.0f, negative ? -1.0f : 1.0f);
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
static float AsinfRational(float t) {
    const float p = t * (1.6666586697e-01f + t * (-4.2743422091e-02f + t * -8.6563630030e-03f));
    const float q = 1.0f + t * -7.0662963390e-01f;
    return p / q;
}

float APS5_VABI asinf_nid_postfix(float x) {
    const double pio2 = 1.570796326794896558e+00;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x3f800000) {
        if (ix == 0x3f800000) {
            return static_cast<float>(x * pio2);
        }
        _Feraise_nid_postfix(0x01);
        return (x - x) / (x - x);
    }
    if (ix < 0x3f000000) {
        if (ix < 0x39800000) {
            return x;
        }
        return x + x * AsinfRational(x * x);
    }
    const float t = (1.0f - std::fabs(x)) * 0.5f;
    const double s = std::sqrt(static_cast<double>(t));
    const float result = static_cast<float>(pio2 - 2.0 * (s + s * AsinfRational(t)));
    return hx > 0 ? result : -result;
}
float APS5_VABI acosf_nid_postfix(float x) {
    if (std::isnan(x)) {
        _Feraise_nid_postfix(0x01);
        return std::bit_cast<float>(std::bit_cast<std::uint32_t>(x) | 0x00400000u);
    }
    if (std::fabs(x) > 1.0f) {
        _Feraise_nid_postfix(0x01);
        return std::bit_cast<float>(0xffc00000u);
    }
    constexpr float Pi = 3.1415925026e+00f, Pio2High = 1.5707962513e+00f;
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (magnitude == 0x3f800000u) return std::signbit(x) ? Pi + 2.0f * AcosfPio2Low : 0.0f;
    if (magnitude < 0x3f000000u) {
        if (magnitude <= 0x32800000u) return Pio2High + AcosfPio2Low;
        return Pio2High - (x - (AcosfPio2Low - x * AcosfRational(x * x)));
    }
    if (std::signbit(x)) {
        const float z = (1.0f + x) * 0.5f;
        const float s = std::sqrt(z);
        return Pi - 2.0f * (s + (AcosfRational(z) * s - AcosfPio2Low));
    }
    const float z = (1.0f - x) * 0.5f;
    const float s = std::sqrt(z);
    const float high = std::bit_cast<float>(std::bit_cast<std::uint32_t>(s) & 0xfffff000u);
    const float correction = (z - high * high) / (s + high);
    return 2.0f * (high + (AcosfRational(z) * s + correction));
}
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
float APS5_VABI log10f_nid_postfix(float x) { return std::log10(x); }
float APS5_VABI logbf_nid_postfix(float x) {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x) & 0x7fffffffu;
    if (bits == 0) {
        return -1.0f / std::fabs(x);
    }
    if (bits >= 0x7f800000u) {
        return x * x;
    }
    if (bits < 0x00800000u) {
        const std::uint32_t scaled = std::bit_cast<std::uint32_t>(x * 0x1p25f) & 0x7fffffffu;
        return static_cast<float>(static_cast<int>(scaled >> 23) - 127 - 25);
    }
    return static_cast<float>(static_cast<int>(bits >> 23) - 127);
}
float APS5_VABI exp2f_nid_postfix(float x) {
    static constexpr double exp2ft[16] = {
        0x1.6a09e667f3bcdp-1, 0x1.7a11473eb0187p-1, 0x1.8ace5422aa0dbp-1, 0x1.9c49182a3f090p-1,
        0x1.ae89f995ad3adp-1, 0x1.c199bdd85529cp-1, 0x1.d5818dcfba487p-1, 0x1.ea4afa2a490dap-1,
        0x1.0000000000000p+0, 0x1.0b5586cf9890fp+0, 0x1.172b83c7d517bp+0, 0x1.2387a6e756238p+0,
        0x1.306fe0a31b715p+0, 0x1.3dea64c123422p+0, 0x1.4bfdad5362a27p+0, 0x1.5ab07dd485429p+0,
    };
    static volatile float huge = 0x1p100f;
    static volatile float twom100 = 0x1p-100f;
    const float redux = 0x1.8p23f / 16;
    const float P1 = 0x1.62e430p-1f;
    const float P2 = 0x1.ebfbe0p-3f;
    const float P3 = 0x1.c6b348p-5f;
    const float P4 = 0x1.3b2c9cp-7f;
    const std::uint32_t hx = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t ix = hx & 0x7fffffff;
    if (ix >= 0x43000000) {
        if (ix >= 0x7f800000) {
            return (ix & 0x7fffff) != 0 || (hx & 0x80000000) == 0 ? x + x : 0.0f;
        }
        if (x >= 0x1.0p7f) {
            errno = ERANGE;
            return huge * huge;
        }
        if (x <= -0x1.2cp7f) {
            errno = ERANGE;
            return twom100 * twom100;
        }
    } else if (ix <= 0x33000000) {
        return 1.0f + x;
    }
    volatile float rounded = x + redux;
    float t = rounded;
    std::uint32_t i0 = std::bit_cast<std::uint32_t>(t) + 8;
    const std::int32_t k = static_cast<std::int32_t>(i0 >> 4) << 20;
    i0 &= 15;
    t -= redux;
    const double z = x - t;
    const double twopk = std::bit_cast<double>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(0x3ff00000 + k)) << 32);
    const double u = exp2ft[i0] * z;
    const double tv = exp2ft[i0] + u * (P1 + z * P2) + u * (z * z) * (P3 + z * P4);
    return static_cast<float>(tv * twopk);
}
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
float APS5_VABI frexpf_nid_postfix(float x, int* exponent) {
    auto bits = std::bit_cast<std::uint32_t>(x);
    auto magnitude = bits & 0x7fffffffu;
    *exponent = 0;
    if (magnitude == 0 || magnitude >= 0x7f800000u) return x;
    int adjustment = 0;
    if (magnitude < 0x00800000u) {
        volatile float subnormal = x;
        x = subnormal * 0x1p25f;
        bits = std::bit_cast<std::uint32_t>(x);
        magnitude = bits & 0x7fffffffu;
        adjustment = -25;
    }
    *exponent = static_cast<int>(magnitude >> 23) - 126 + adjustment;
    return std::bit_cast<float>((bits & 0x807fffffu) | 0x3f000000u);
}
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
float APS5_VABI nearbyintf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::nearbyintf(x); }
long double APS5_VABI nearbyintl_nid_postfix(long double x) { return std::nearbyint(x); }
double APS5_VABI nearbyint_nid_postfix(double x) { return std::isnan(x) ? x + x : std::nearbyint(x); }

double APS5_VABI cbrt_nid_postfix(double x) {
    const std::uint32_t B1 = 715094163;
    const std::uint32_t B2 = 696219795;
    const double P0 = 1.87595182427177009643;
    const double P1 = -1.88497979543377169875;
    const double P2 = 1.621429720105354466140;
    const double P3 = -0.758397934778766047437;
    const double P4 = 0.145996192886612446982;
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::uint32_t hx = static_cast<std::uint32_t>(bits >> 32);
    const std::uint32_t low = static_cast<std::uint32_t>(bits);
    const std::uint32_t sign = hx & 0x80000000u;
    hx ^= sign;
    if (hx >= 0x7ff00000) {
        return x + x;
    }
    double t;
    if (hx < 0x00100000) {
        if ((hx | low) == 0) {
            return x;
        }
        t = std::bit_cast<double>(std::uint64_t{0x4350000000000000}) * x;
        const std::uint32_t high = static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(t) >> 32);
        t = std::bit_cast<double>(static_cast<std::uint64_t>(sign | ((high & 0x7fffffff) / 3 + B2)) << 32);
    } else {
        t = std::bit_cast<double>(static_cast<std::uint64_t>(sign | (hx / 3 + B1)) << 32);
    }
    double r = (t * t) * (t / x);
    t = t * ((P0 + r * (P1 + r * P2)) + ((r * r) * r) * (P3 + r * P4));
    t = std::bit_cast<double>((std::bit_cast<std::uint64_t>(t) + 0x80000000u) & 0xffffffffc0000000ull);
    const double s = t * t;
    r = x / s;
    const double w = t + t;
    r = (r - t) / (w + r);
    return t + t * r;
}
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
double APS5_VABI trunc_nid_postfix(double x) { return std::isnan(x) ? x + x : std::trunc(x); }
double APS5_VABI ceil_nid_postfix(double x) { return std::isnan(x) ? x + x : std::ceil(x); }
double APS5_VABI floor_nid_postfix(double x) { return std::isnan(x) ? x + x : std::floor(x); }
float APS5_VABI truncf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::trunc(x); }
float APS5_VABI ceilf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::ceil(x); }
float APS5_VABI floorf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::floor(x); }
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
float APS5_VABI tgammaf_nid_postfix(float x) { return GuestTgammaf(x); }
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
double APS5_VABI log1p_nid_postfix(double x) {
    const double ln2_hi = 6.93147180369123816490e-01;
    const double ln2_lo = 1.90821492927058770002e-10;
    const double two54 = 1.80143985094819840000e+16;
    const double Lp1 = 6.666666666666735130e-01;
    const double Lp2 = 3.999999999940941908e-01;
    const double Lp3 = 2.857142874366239149e-01;
    const double Lp4 = 2.222219843214978396e-01;
    const double Lp5 = 1.818357216161805012e-01;
    const double Lp6 = 1.531383769920937332e-01;
    const double Lp7 = 1.479819860511658591e-01;
    static volatile double vzero = 0.0;
    const std::int32_t hx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const std::int32_t ax = hx & 0x7fffffff;
    std::int32_t k = 1;
    std::int32_t hu = 0;
    double f = 0.0;
    double c = 0.0;
    if (hx < 0x3fda827a) {
        if (ax >= 0x3ff00000) {
            if (x == -1.0) {
                return -two54 / vzero;
            }
            errno = EDOM;
            return (x - x) / (x - x);
        }
        if (ax < 0x3e200000) {
            if (two54 + x > 0.0 && ax < 0x3c900000) {
                return x;
            }
            return x - x * x * 0.5;
        }
        if (hx > 0 || hx <= static_cast<std::int32_t>(0xbfd2bec4)) {
            k = 0;
            f = x;
            hu = 1;
        }
    }
    if (hx >= 0x7ff00000) {
        return x + x;
    }
    if (k != 0) {
        double u;
        if (hx < 0x43400000) {
            u = 1.0 + x;
            hu = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(u) >> 32);
            k = (hu >> 20) - 1023;
            c = k > 0 ? 1.0 - (u - x) : x - (u - 1.0);
            c /= u;
        } else {
            u = x;
            hu = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(u) >> 32);
            k = (hu >> 20) - 1023;
            c = 0;
        }
        hu &= 0x000fffff;
        const std::uint64_t ulow = std::bit_cast<std::uint64_t>(u) & 0xffffffffull;
        if (hu < 0x6a09e) {
            u = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(hu | 0x3ff00000)) << 32) | ulow);
        } else {
            k += 1;
            u = std::bit_cast<double>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(hu | 0x3fe00000)) << 32) | ulow);
            hu = (0x00100000 - hu) >> 2;
        }
        f = u - 1.0;
    }
    const double hfsq = 0.5 * f * f;
    if (hu == 0) {
        if (f == 0.0) {
            if (k == 0) {
                return 0.0;
            }
            c += k * ln2_lo;
            return k * ln2_hi + c;
        }
        const double R = hfsq * (1.0 - 0.66666666666666666 * f);
        if (k == 0) {
            return f - R;
        }
        return k * ln2_hi - ((R - (k * ln2_lo + c)) - f);
    }
    const double s = f / (2.0 + f);
    const double z = s * s;
    const double R = z * (Lp1 + z * (Lp2 + z * (Lp3 + z * (Lp4 + z * (Lp5 + z * (Lp6 + z * Lp7))))));
    if (k == 0) {
        return f - (hfsq - s * (hfsq + R));
    }
    return k * ln2_hi - ((hfsq - (s * (hfsq + R) + (k * ln2_lo + c))) - f);
}
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
        y = 1.0 + (x - e);
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

static const std::int32_t TrigfIpio2[] = {
    0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
    0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129, 0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
    0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8, 0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
    0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5, 0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
    0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3, 0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
    0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
};

static const double TrigfPio2Parts[] = {
    1.57079625129699707031e+00, 7.54978941586159635335e-08, 5.39030252995776476554e-15, 3.28200341580791294123e-22,
    1.27065575308067607349e-29, 1.22933308981111328932e-36, 2.73370053816464559624e-44, 2.16741683877804819444e-51,
};

static int TrigfKernelRemPio2(const double* x, double* y, int e0) {
    const double two24 = 1.67772160000000000000e+07, twon24 = 5.96046447753906250000e-08;
    std::int32_t jz, jv, jp, jk, carry, n, iq[20], i, j, k, m, q0, ih;
    double z, fw, f[20], fq[20], q[20];
    const std::int32_t jx = 0;
    jk = 3;
    jp = jk;
    jv = (e0 - 3) / 24;
    if (jv < 0) jv = 0;
    q0 = e0 - 24 * (jv + 1);
    j = jv - jx;
    m = jx + jk;
    for (i = 0; i <= m; i++, j++) f[i] = j < 0 ? 0.0 : static_cast<double>(TrigfIpio2[j]);
    for (i = 0; i <= jk; i++) {
        for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
        q[i] = fw;
    }
    jz = jk;
recompute:
    for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
        fw = static_cast<double>(static_cast<std::int32_t>(twon24 * z));
        iq[i] = static_cast<std::int32_t>(z - two24 * fw);
        z = q[j - 1] + fw;
    }
    z = std::scalbn(z, q0);
    z -= 8.0 * std::floor(z * 0.125);
    n = static_cast<std::int32_t>(z);
    z -= static_cast<double>(n);
    ih = 0;
    if (q0 > 0) {
        i = iq[jz - 1] >> (24 - q0);
        n += i;
        iq[jz - 1] -= i << (24 - q0);
        ih = iq[jz - 1] >> (23 - q0);
    } else if (q0 == 0) {
        ih = iq[jz - 1] >> 23;
    } else if (z >= 0.5) {
        ih = 2;
    }
    if (ih > 0) {
        n += 1;
        carry = 0;
        for (i = 0; i < jz; i++) {
            j = iq[i];
            if (carry == 0) {
                if (j != 0) {
                    carry = 1;
                    iq[i] = 0x1000000 - j;
                }
            } else {
                iq[i] = 0xffffff - j;
            }
        }
        if (q0 > 0) {
            switch (q0) {
                case 1: iq[jz - 1] &= 0x7fffff; break;
                case 2: iq[jz - 1] &= 0x3fffff; break;
            }
        }
        if (ih == 2) {
            z = 1.0 - z;
            if (carry != 0) z -= std::scalbn(1.0, q0);
        }
    }
    if (z == 0.0) {
        j = 0;
        for (i = jz - 1; i >= jk; i--) j |= iq[i];
        if (j == 0) {
            for (k = 1; iq[jk - k] == 0; k++) {
            }
            for (i = jz + 1; i <= jz + k; i++) {
                f[jx + i] = static_cast<double>(TrigfIpio2[jv + i]);
                for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
                q[i] = fw;
            }
            jz += k;
            goto recompute;
        }
    }
    if (z == 0.0) {
        jz -= 1;
        q0 -= 24;
        while (iq[jz] == 0) {
            jz--;
            q0 -= 24;
        }
    } else {
        z = std::scalbn(z, -q0);
        if (z >= two24) {
            fw = static_cast<double>(static_cast<std::int32_t>(twon24 * z));
            iq[jz] = static_cast<std::int32_t>(z - two24 * fw);
            jz += 1;
            q0 += 24;
            iq[jz] = static_cast<std::int32_t>(fw);
        } else {
            iq[jz] = static_cast<std::int32_t>(z);
        }
    }
    fw = std::scalbn(1.0, q0);
    for (i = jz; i >= 0; i--) {
        q[i] = fw * static_cast<double>(iq[i]);
        fw *= twon24;
    }
    for (i = jz; i >= 0; i--) {
        for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++) fw += TrigfPio2Parts[k] * q[i + k];
        fq[jz - i] = fw;
    }
    fw = 0.0;
    for (i = jz; i >= 0; i--) fw += fq[i];
    y[0] = ih == 0 ? fw : -fw;
    return n & 7;
}

static int TrigfRemPio2(float x, double* y) {
    const double invpio2 = 6.36619772367581382433e-01, pio2_1 = 1.57079631090164184570e+00, pio2_1t = 1.58932547735281966916e-08;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix < 0x4dc90fdb) {
        volatile double rounded = x * invpio2 + 0x1.8p52;
        double fn = rounded;
        fn = fn - 0x1.8p52;
        const std::int32_t n = static_cast<std::int32_t>(fn);
        const double r = x - fn * pio2_1;
        const double w = fn * pio2_1t;
        *y = r - w;
        return n;
    }
    if (ix >= 0x7f800000) {
        *y = x - x;
        return 0;
    }
    const std::int32_t e0 = (ix >> 23) - 150;
    const float z = std::bit_cast<float>(ix - static_cast<std::int32_t>(static_cast<std::uint32_t>(e0) << 23));
    double tx[1] = {z}, ty[1];
    const int n = TrigfKernelRemPio2(tx, ty, e0);
    if (hx < 0) {
        *y = -ty[0];
        return -n;
    }
    *y = ty[0];
    return n;
}

static float TrigfKernelSin(double x) {
    const double S1 = -0x15555554cbac77.0p-55, S2 = 0x111110896efbb2.0p-59, S3 = -0x1a00f9e2cae774.0p-65, S4 = 0x16cd878c3b46a7.0p-71;
    const double z = x * x, w = z * z, r = S3 + z * S4, s = z * x;
    return static_cast<float>((x + s * (S1 + z * S2)) + s * w * r);
}

static float TrigfKernelCos(double x) {
    const double C0 = -0x1ffffffd0c5e81.0p-54, C1 = 0x155553e1053a42.0p-57, C2 = -0x16c087e80f1e27.0p-62, C3 = 0x199342e0ee5069.0p-68;
    const double z = x * x, w = z * z, r = C2 + z * C3;
    return static_cast<float>(((1.0 + z * C0) + w * C1) + (w * z) * r);
}

float APS5_VABI sinf_nid_postfix(float x) {
    const double s1pio2 = std::bit_cast<double>(0x3FF921FB54442D18ull), s2pio2 = std::bit_cast<double>(0x400921FB54442D18ull);
    const double s3pio2 = std::bit_cast<double>(0x4012D97C7F3321D2ull), s4pio2 = std::bit_cast<double>(0x401921FB54442D18ull);
    double y;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix <= 0x3f490fda) {
        if (ix < 0x39800000)
            if (static_cast<int>(x) == 0) return x;
        return TrigfKernelSin(x);
    }
    if (ix <= 0x407b53d1) {
        if (ix <= 0x4016cbe3) return hx > 0 ? TrigfKernelCos(x - s1pio2) : -TrigfKernelCos(x + s1pio2);
        return TrigfKernelSin((hx > 0 ? s2pio2 : -s2pio2) - x);
    }
    if (ix <= 0x40e231d5) {
        if (ix <= 0x40afeddf) return hx > 0 ? -TrigfKernelCos(x - s3pio2) : TrigfKernelCos(x + s3pio2);
        return TrigfKernelSin(x + (hx > 0 ? -s4pio2 : s4pio2));
    }
    if (ix >= 0x7f800000) {
        if (ix == 0x7f800000) errno = EDOM;
        return x - x;
    }
    const int n = TrigfRemPio2(x, &y);
    switch (n & 3) {
        case 0: return TrigfKernelSin(y);
        case 1: return TrigfKernelCos(y);
        case 2: return TrigfKernelSin(-y);
        default: return -TrigfKernelCos(y);
    }
}

static const double TrigfPio2[5] = {0.0, std::bit_cast<double>(0x3FF921FB54442D18ull), std::bit_cast<double>(0x400921FB54442D18ull),
                                std::bit_cast<double>(0x4012D97C7F3321D2ull), std::bit_cast<double>(0x401921FB54442D18ull)};

float APS5_VABI cosf_nid_postfix(float x) {
    double y;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix <= 0x3f490fda) {
        if (ix < 0x39800000)
            if (static_cast<int>(x) == 0) return 1.0f;
        return TrigfKernelCos(x);
    }
    if (ix <= 0x407b53d1) {
        if (ix > 0x4016cbe3) return -TrigfKernelCos(x + (hx > 0 ? -TrigfPio2[2] : TrigfPio2[2]));
        return hx > 0 ? TrigfKernelSin(TrigfPio2[1] - x) : TrigfKernelSin(x + TrigfPio2[1]);
    }
    if (ix <= 0x40e231d5) {
        if (ix > 0x40afeddf) return TrigfKernelCos(x + (hx > 0 ? -TrigfPio2[4] : TrigfPio2[4]));
        return hx > 0 ? TrigfKernelSin(x - TrigfPio2[3]) : TrigfKernelSin(-TrigfPio2[3] - x);
    }
    if (ix >= 0x7f800000) {
        if (ix == 0x7f800000) errno = EDOM;
        return x - x;
    }
    const int n = TrigfRemPio2(x, &y);
    switch (n & 3) {
        case 0: return TrigfKernelCos(y);
        case 1: return TrigfKernelSin(-y);
        case 2: return -TrigfKernelCos(y);
        default: return TrigfKernelSin(y);
    }
}

static float TrigfKernelTan(double x, int iy) {
    static const double T[] = {0x15554d3418c99f.0p-54, 0x1112fd38999f72.0p-55, 0x1b54c91d865afe.0p-57,
                               0x191df3908c33ce.0p-58, 0x185dadfcecf44e.0p-61, 0x1362b9bf971bcd.0p-59};
    const double z = x * x;
    double r = T[4] + z * T[5];
    const double t = T[2] + z * T[3];
    const double w = z * z;
    const double s = z * x;
    const double u = T[0] + z * T[1];
    r = (x + s * u) + (s * w) * (t + w * r);
    if (iy == 1) return static_cast<float>(r);
    return static_cast<float>(-1.0 / r);
}

float APS5_VABI tanf_nid_postfix(float x) {
    double y;
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix <= 0x3f490fda) {
        if (ix < 0x39800000)
            if (static_cast<int>(x) == 0) return x;
        return TrigfKernelTan(x, 1);
    }
    if (ix <= 0x407b53d1) {
        if (ix <= 0x4016cbe3) return TrigfKernelTan(x + (hx > 0 ? -TrigfPio2[1] : TrigfPio2[1]), -1);
        return TrigfKernelTan(x + (hx > 0 ? -TrigfPio2[2] : TrigfPio2[2]), 1);
    }
    if (ix <= 0x40e231d5) {
        if (ix <= 0x40afeddf) return TrigfKernelTan(x + (hx > 0 ? -TrigfPio2[3] : TrigfPio2[3]), -1);
        return TrigfKernelTan(x + (hx > 0 ? -TrigfPio2[4] : TrigfPio2[4]), 1);
    }
    if (ix >= 0x7f800000) {
        if (ix == 0x7f800000) errno = EDOM;
        return x - x;
    }
    const int n = TrigfRemPio2(x, &y);
    return TrigfKernelTan(y, 1 - ((n & 1) << 1));
}

void APS5_VABI sincosf_nid_postfix(float x, float* sinp, float* cosp) {
    *sinp = sinf_nid_postfix(x);
    *cosp = cosf_nid_postfix(x);
}

double APS5_VABI sin_nid_postfix(double x) { return std::sin(x); }
double APS5_VABI cos_nid_postfix(double x) { return std::cos(x); }
/*-
 * Copyright (c) 2011 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/k_exp.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/k_exp.c */
static double SinhLdexpExpHalf(double x) {
    constexpr std::uint32_t k = 1799;
    constexpr double kln2 = 1246.97177782734161156;
    const double expX = exp_nid_postfix(x - kln2);
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(expX);
    const std::uint32_t hx = static_cast<std::uint32_t>(bits >> 32);
    const int expt = -1 + static_cast<int>(hx >> 20) - (0x3ff + 1023) + static_cast<int>(k);
    const std::uint32_t scaledHigh = (hx & 0xfffff) | ((0x3ff + 1023) << 20);
    const double scaled = std::bit_cast<double>((bits & 0xffffffffull) | (static_cast<std::uint64_t>(scaledHigh) << 32));
    return scaled * std::bit_cast<double>(static_cast<std::uint64_t>(0x3ff + expt) << 52);
}
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/e_sinh.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/e_sinh.c */
double APS5_VABI sinh_nid_postfix(double x) {
    const double one = 1.0;
    const double shuge = 1.0e307;
    const std::int32_t jx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const std::int32_t ix = jx & 0x7fffffff;
    if (ix >= 0x7ff00000) return x + x;
    const double h = jx < 0 ? -0.5 : 0.5;
    if (ix < 0x40360000) {
        if (ix < 0x3e300000 && shuge + x > one) return x;
        const double t = Expm1(std::fabs(x));
        if (ix < 0x3ff00000) return h * (2.0 * t - t * t / (t + one));
        return h * (t + t / (t + one));
    }
    if (ix < 0x40862e42) return h * exp_nid_postfix(std::fabs(x));
    if (ix <= 0x408633ce) return h * 2.0 * SinhLdexpExpHalf(std::fabs(x));
    errno = ERANGE;
    return x * shuge;
}
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

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_asinh.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_asinh.c */
double APS5_VABI asinh_nid_postfix(double x) {
    const double one = 1.0;
    const double ln2 = 6.93147180559945286227e-01;
    const double huge = 1.0e300;
    const std::int32_t hx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const std::int32_t ix = hx & 0x7fffffff;
    if (ix >= 0x7ff00000) return x + x;
    if (ix < 0x3e300000 && huge + x > one) return x;
    double w;
    if (ix > 0x41b00000) {
        w = log_nid_postfix(std::fabs(x)) + ln2;
    } else if (ix > 0x40000000) {
        const double t = std::fabs(x);
        w = log_nid_postfix(2.0 * t + one / (std::sqrt(x * x + one) + t));
    } else {
        const double t = x * x;
        w = log1p_nid_postfix(std::fabs(x) + t / (one + std::sqrt(one + t)));
    }
    return hx > 0 ? w : -w;
}
float APS5_VABI asinhf_nid_postfix(float x) { return std::asinh(x); }
double APS5_VABI acosh_nid_postfix(double x) { return std::acosh(x); }
float APS5_VABI acoshf_nid_postfix(float x) {
    constexpr float ln2 = 6.9314718246e-01f;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t magnitude = bits & 0x7fffffffu;
    if ((bits & 0x80000000u) != 0 || magnitude < 0x3f800000u) {
        _Feraise_nid_postfix(0x01);
        errno = EDOM;
        return (x - x) / (x - x);
    }
    if (magnitude >= 0x7f800000u) return x + x;
    if (magnitude == 0x3f800000u) return 0.0f;
    if (magnitude >= 0x4d800000u) return logf_nid_postfix(x) + ln2;
    if (magnitude > 0x40000000u) {
        const float square = x * x;
        return logf_nid_postfix(2.0f * x + (-1.0f / (x + std::sqrt(square - 1.0f))));
    }
    const float t = x - 1.0f;
    return AtanhfLog1p(t + std::sqrt(2.0f * t + t * t));
}
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

float APS5_VABI atan2f_nid_postfix(float y, float x) {
    static volatile float tiny = 1.0e-30f;
    static volatile float pi_lo = static_cast<float>(-8.7422776573e-08);
    const float pi_o_4 = static_cast<float>(7.8539818525e-01);
    const float pi_o_2 = static_cast<float>(1.5707963705e+00);
    const float pi = static_cast<float>(3.1415927410e+00);
    const std::int32_t hx = std::bit_cast<std::int32_t>(x);
    const std::int32_t ix = hx & 0x7fffffff;
    const std::int32_t hy = std::bit_cast<std::int32_t>(y);
    const std::int32_t iy = hy & 0x7fffffff;
    if (iy > 0x7f800000) {
        return y + y;
    }
    if (ix > 0x7f800000) {
        return x + x;
    }
    if (hx == 0x3f800000) {
        return atanf_nid_postfix(y);
    }
    int m = ((hy >> 31) & 1) | ((hx >> 30) & 2);
    if (iy == 0) {
        if (m < 2) {
            return y;
        }
        return m == 2 ? pi + tiny : -pi - tiny;
    }
    if (ix == 0) {
        return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;
    }
    if (ix == 0x7f800000) {
        if (iy == 0x7f800000) {
            switch (m) {
                case 0: return pi_o_4 + tiny;
                case 1: return -pi_o_4 - tiny;
                case 2: return 3.0f * pi_o_4 + tiny;
                default: return -3.0f * pi_o_4 - tiny;
            }
        }
        switch (m) {
            case 0: return 0.0f;
            case 1: return -0.0f;
            case 2: return pi + tiny;
            default: return -pi - tiny;
        }
    }
    if (iy == 0x7f800000) {
        return hy < 0 ? -pi_o_2 - tiny : pi_o_2 + tiny;
    }
    const std::int32_t k = (iy - ix) >> 23;
    float z;
    if (k > 26) {
        z = pi_o_2 + 0.5f * pi_lo;
        m &= 1;
    } else if (k < -26 && hx < 0) {
        z = 0.0f;
    } else {
        z = atanf_nid_postfix(std::fabs(y / x));
    }
    switch (m) {
        case 0: return z;
        case 1: return -z;
        case 2: return pi - (z - pi_lo);
        default: return (z - pi_lo) - pi;
    }
}
double APS5_VABI atan2_nid_postfix(double y, double x) { return std::atan2(y, x); }
static inline std::int32_t PowfWord(float f) { return std::bit_cast<std::int32_t>(f); }
static inline float PowfFloat(std::int32_t w) { return std::bit_cast<float>(w); }

float APS5_VABI powf_nid_postfix(float x, float y) {
    static const float bp[] = {1.0f, 1.5f}, dp_h[] = {0.0f, static_cast<float>(5.84960938e-01)}, dp_l[] = {0.0f, static_cast<float>(1.56322085e-06)};
    const float zero = 0.0f, half = 0.5f, qrtr = 0.25f, thrd = static_cast<float>(3.33333343e-01), one = 1.0f, two = 2.0f, two24 = 16777216.0f;
    const float huge = 1.0e30f;
    static const volatile float tiny = 1.0e-30f;
    const float L1 = static_cast<float>(6.0000002384e-01), L2 = static_cast<float>(4.2857143283e-01), L3 = static_cast<float>(3.3333334327e-01);
    const float L4 = static_cast<float>(2.7272811532e-01), L5 = static_cast<float>(2.3066075146e-01), L6 = static_cast<float>(2.0697501302e-01);
    const float P1 = static_cast<float>(1.6666667163e-01), P2 = static_cast<float>(-2.7777778450e-03), P3 = static_cast<float>(6.6137559770e-05);
    const float P4 = static_cast<float>(-1.6533901999e-06), P5 = static_cast<float>(4.1381369442e-08);
    const float lg2 = static_cast<float>(6.9314718246e-01), lg2_h = static_cast<float>(6.93145752e-01), lg2_l = static_cast<float>(1.42860654e-06);
    const float ovt = static_cast<float>(4.2995665694e-08), cp = static_cast<float>(9.6179670095e-01), cp_h = static_cast<float>(9.6191406250e-01);
    const float cp_l = static_cast<float>(-1.1736857402e-04), ivln2 = static_cast<float>(1.4426950216e+00), ivln2_h = static_cast<float>(1.4426879883e+00);
    const float ivln2_l = static_cast<float>(7.0526075433e-06);
    float z, ax, z_h, z_l, p_h, p_l, y1, t1, t2, r, s, sn, t, u, v, w;
    std::int32_t i, j, k, yisint, n, is;
    const std::int32_t hx = PowfWord(x), hy = PowfWord(y);
    std::int32_t ix = hx & 0x7fffffff;
    const std::int32_t iy = hy & 0x7fffffff;
    if (iy == 0) return one;
    if (ix > 0x7f800000 || iy > 0x7f800000) {
        if ((ix > 0x7f800000 && (ix & 0x00400000) == 0) ||
            (iy > 0x7f800000 && (iy & 0x00400000) == 0)) std::feraiseexcept(FE_INVALID);
        return PowfFloat((ix > 0x7f800000 ? hx : hy) | 0x00400000);
    }
    yisint = 0;
    if (hx < 0) {
        if (iy >= 0x4b800000) yisint = 2;
        else if (iy >= 0x3f800000) {
            k = (iy >> 23) - 0x7f;
            j = iy >> (23 - k);
            if ((j << (23 - k)) == iy) yisint = 2 - (j & 1);
        }
    }
    if (iy == 0x7f800000) {
        if (ix == 0x3f800000) return y - y;
        else if (ix > 0x3f800000) return hy >= 0 ? y : zero;
        else return hy < 0 ? -y : zero;
    }
    if (iy == 0x3f800000) return hy < 0 ? one / x : x;
    if (hy == 0x40000000) return x * x;
    if (hy == 0x3f000000) {
        if (hx >= 0) return std::sqrt(x);
    }
    ax = std::fabs(x);
    if (ix == 0x7f800000 || ix == 0 || ix == 0x3f800000) {
        z = ax;
        if (hy < 0) z = one / z;
        if (hx < 0) {
            if (((ix - 0x3f800000) | yisint) == 0) {
                errno = EDOM;
                z = (z - z) / (z - z);
            }
            else if (yisint == 1) z = -z;
        }
        return z;
    }
    n = static_cast<std::int32_t>((static_cast<std::uint32_t>(hx) >> 31) - 1);
    if ((n | yisint) == 0) {
        errno = EDOM;
        return (x - x) / (x - x);
    }
    sn = one;
    if ((n | (yisint - 1)) == 0) sn = -one;
    if (iy > 0x4d000000) {
        if (ix < 0x3f7ffff8) return hy < 0 ? sn * huge * huge : sn * tiny * tiny;
        if (ix > 0x3f800007) return hy > 0 ? sn * huge * huge : sn * tiny * tiny;
        t = ax - 1;
        w = (t * t) * (half - t * (thrd - t * qrtr));
        u = ivln2_h * t;
        v = t * ivln2_l - w * ivln2;
        t1 = u + v;
        is = PowfWord(t1);
        t1 = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        t2 = v - (t1 - u);
    } else {
        float s2, s_h, s_l, t_h, t_l;
        n = 0;
        if (ix < 0x00800000) { ax *= two24; n -= 24; ix = PowfWord(ax); }
        n += (ix >> 23) - 0x7f;
        j = ix & 0x007fffff;
        ix = j | 0x3f800000;
        if (j <= 0x1cc471) k = 0;
        else if (j < 0x5db3d7) k = 1;
        else { k = 0; n += 1; ix -= 0x00800000; }
        ax = PowfFloat(ix);
        u = ax - bp[k];
        v = one / (ax + bp[k]);
        s = u * v;
        s_h = s;
        is = PowfWord(s_h);
        s_h = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        is = ((ix >> 1) & static_cast<std::int32_t>(0xfffff000)) | 0x20000000;
        t_h = PowfFloat(is + 0x00400000 + (k << 21));
        t_l = ax - (t_h - bp[k]);
        s_l = v * ((u - s_h * t_h) - s_h * t_l);
        s2 = s * s;
        r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
        r += s_l * (s_h + s);
        s2 = s_h * s_h;
        t_h = 3 + s2 + r;
        is = PowfWord(t_h);
        t_h = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        t_l = r - ((t_h - 3) - s2);
        u = s_h * t_h;
        v = s_l * t_h + t_l * s;
        p_h = u + v;
        is = PowfWord(p_h);
        p_h = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        p_l = v - (p_h - u);
        z_h = cp_h * p_h;
        z_l = cp_l * p_h + p_l * cp + dp_l[k];
        t = static_cast<float>(n);
        t1 = (((z_h + z_l) + dp_h[k]) + t);
        is = PowfWord(t1);
        t1 = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
        t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
    }
    is = PowfWord(y);
    y1 = PowfFloat(is & static_cast<std::int32_t>(0xfffff000));
    p_l = (y - y1) * t1 + y * t2;
    p_h = y1 * t1;
    z = p_l + p_h;
    j = PowfWord(z);
    if (j > 0x43000000 || (j == 0x43000000 && p_l + ovt > z - p_h)) {
        errno = ERANGE;
        return sn * huge * huge;
    }
    if ((j & 0x7fffffff) > 0x43160000 || (static_cast<std::uint32_t>(j) == 0xc3160000u && p_l <= z - p_h)) {
        errno = ERANGE;
        return sn * tiny * tiny;
    }
    i = j & 0x7fffffff;
    k = (i >> 23) - 0x7f;
    n = 0;
    if (i > 0x3f000000) {
        n = j + (0x00800000 >> (k + 1));
        k = ((n & 0x7fffffff) >> 23) - 0x7f;
        t = PowfFloat(n & ~(0x007fffff >> k));
        n = ((n & 0x007fffff) | 0x00800000) >> (23 - k);
        if (j < 0) n = -n;
        p_h -= t;
    }
    t = p_l + p_h;
    is = PowfWord(t);
    t = PowfFloat(is & static_cast<std::int32_t>(0xffff8000));
    u = t * lg2_h;
    v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
    z = u + v;
    w = v - (z - u);
    t = z * z;
    t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    r = (z * t1) / (t1 - two) - (w + z * w);
    z = one - (r - z);
    j = PowfWord(z);
    j += static_cast<std::int32_t>(static_cast<std::uint32_t>(n) << 23);
    if ((j >> 23) <= 0) z = std::scalbn(z, n);
    else z = PowfFloat(j);
    return sn * z;
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
float APS5_VABI remainderf_nid_postfix(float x, float y) { return std::remainder(x, y); }

float APS5_VABI nextafterf_nid_postfix(float x, float y) {
    const std::uint32_t ux = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t uy = std::bit_cast<std::uint32_t>(y);
    if ((ux & 0x7fffffffu) > 0x7f800000u || (uy & 0x7fffffffu) > 0x7f800000u) {
        volatile float sum = x + y;
        (void)sum;
        return std::bit_cast<float>(((ux & 0x7fffffffu) > 0x7f800000u ? ux : uy) | 0x00400000u);
    }
    if (x == y) {
        return y;
    }
    if ((ux & 0x7fffffffu) == 0) {
        const volatile float smallest = std::bit_cast<float>((uy & 0x80000000u) | 1u);
        volatile float product = smallest * smallest;
        return product == smallest ? product : smallest;
    }
    const bool towardZero = ((ux ^ uy) & 0x80000000u) != 0 || ux > uy;
    const std::uint32_t next = towardZero ? ux - 1 : ux + 1;
    const std::uint32_t exponent = next & 0x7f800000u;
    if (exponent == 0x7f800000u) {
        _Feraise_nid_postfix(0x08);
        return x + x;
    }
    if (exponent == 0) {
        volatile float product = x * x;
        if (product != x) _Feraise_nid_postfix(0x10);
    }
    return std::bit_cast<float>(next);
}
double APS5_VABI nextafter_nid_postfix(double x, double y) {
    const std::uint64_t ux = std::bit_cast<std::uint64_t>(x);
    const std::uint64_t uy = std::bit_cast<std::uint64_t>(y);
    if ((ux & 0x7fffffffffffffffull) > 0x7ff0000000000000ull || (uy & 0x7fffffffffffffffull) > 0x7ff0000000000000ull) {
        volatile double sum = x + y;
        (void)sum;
        return std::bit_cast<double>(((ux & 0x7fffffffffffffffull) > 0x7ff0000000000000ull ? ux : uy) | 0x0008000000000000ull);
    }
    if (x == y) {
        return y;
    }
    if ((ux & 0x7fffffffffffffffull) == 0) {
        const volatile double smallest = std::bit_cast<double>((uy & 0x8000000000000000ull) | 1u);
        volatile double product = smallest * smallest;
        return product == smallest ? product : smallest;
    }
    const bool towardZero = ((ux ^ uy) & 0x8000000000000000ull) != 0 || ux > uy;
    const std::uint64_t next = towardZero ? ux - 1 : ux + 1;
    const std::uint64_t exponent = next & 0x7ff0000000000000ull;
    if (exponent == 0x7ff0000000000000ull) {
        _Feraise_nid_postfix(0x08);
        return x + x;
    }
    if (exponent == 0) {
        volatile double product = x * x;
        if (product != x) _Feraise_nid_postfix(0x10);
    }
    return std::bit_cast<double>(next);
}
int APS5_VABI __isfinite_nid_postfix(double x) { return std::isfinite(x) ? 1 : 0; }
int APS5_VABI __isnan_nid_postfix(double x) { return std::isnan(x) ? 1 : 0; }
int APS5_VABI __isinf_nid_postfix(double x) { return std::isinf(x) ? 1 : 0; }
int APS5_VABI __signbit_nid_postfix(double x) { return std::signbit(x) ? 0x8000 : 0; }
float APS5_VABI nanf_nid_postfix(const char* tagp) {
    (void)tagp;
    return std::bit_cast<float>(0x7fc00000u);
}
double APS5_VABI nan_nid_postfix(const char* tagp) {
    (void)tagp;
    return std::bit_cast<double>(0x7ff8000000000000ull);
}

double APS5_VABI modf_nid_postfix(double x, double* integral) {
    const auto bits = std::bit_cast<std::uint64_t>(x);
    const auto exponent = (bits >> 52u) & 0x7ffull;
    const auto zero = std::bit_cast<double>(bits & 0x8000000000000000ull);
    if (exponent < 1023u) {
        *integral = zero;
        return x;
    }
    if (exponent >= 1075u) {
        *integral = x;
        return exponent == 0x7ffu && (bits & 0xfffffffffffffull) != 0 ? x : zero;
    }
    const auto mask = (std::uint64_t{1} << (1075u - exponent)) - 1u;
    *integral = std::bit_cast<double>(bits & ~mask);
    return (bits & mask) == 0 ? zero : x - *integral;
}
float APS5_VABI modff_nid_postfix(float x, float* integral) {
    const auto bits = std::bit_cast<std::uint32_t>(x);
    const auto exponent = (bits >> 23u) & 0xffu;
    const auto zero = std::bit_cast<float>(bits & 0x80000000u);
    if (exponent < 127u) {
        *integral = zero;
        return x;
    }
    if (exponent >= 150u) {
        *integral = x;
        if (exponent == 0xffu && (bits & 0x7fffffu) != 0) {
            volatile int unordered = _mm_ucomieq_ss(_mm_set_ss(x), _mm_setzero_ps());
            (void)unordered;
            return x;
        }
        return zero;
    }
    const auto mask = (std::uint32_t{1} << (150u - exponent)) - 1u;
    *integral = std::bit_cast<float>(bits & ~mask);
    return (bits & mask) == 0 ? zero : x - *integral;
}
double APS5_VABI tanh_nid_postfix(double x) {
    static volatile double tiny = 1.0e-300;
    const double huge = 1.0e300;
    const std::int32_t jx = static_cast<std::int32_t>(std::bit_cast<std::uint64_t>(x) >> 32);
    const std::int32_t ix = jx & 0x7fffffff;
    if (ix >= 0x7ff00000) {
        return jx >= 0 ? 1.0 / x + 1.0 : 1.0 / x - 1.0;
    }
    double z;
    if (ix < 0x40360000) {
        if (ix < 0x3e300000 && huge + x > 1.0) {
            return x;
        }
        if (ix >= 0x3ff00000) {
            const double t = Expm1(2.0 * std::fabs(x));
            z = 1.0 - 2.0 / (t + 2.0);
        } else {
            const double t = Expm1(-2.0 * std::fabs(x));
            z = -t / (t + 2.0);
        }
    } else {
        z = 1.0 - tiny;
    }
    return jx >= 0 ? z : -z;
}
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
double APS5_VABI logb_nid_postfix(double x) {
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(x) & 0x7fffffffffffffffull;
    if (bits == 0) {
        return -1.0 / std::fabs(x);
    }
    if (bits >= 0x7ff0000000000000ull) {
        return x * x;
    }
    if (bits < 0x0010000000000000ull) {
        const std::uint64_t scaled = std::bit_cast<std::uint64_t>(x * 0x1p54) & 0x7fffffffffffffffull;
        return static_cast<double>(static_cast<int>(scaled >> 52) - 1023 - 54);
    }
    return static_cast<double>(static_cast<int>(bits >> 52) - 1023);
}
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
double APS5_VABI remainder_nid_postfix(double x, double y) { return std::remainder(x, y); }
double APS5_VABI remquo_nid_postfix(double x, double y, int* quo) { return std::remquo(x, y, quo); }
float APS5_VABI remquof_nid_postfix(float x, float y, int* quo) { return std::remquo(x, y, quo); }
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
int APS5_VABI __fpclassifyf_nid_postfix(float x) { return _FDtest_nid_postfix(&x); }
int APS5_VABI __signbitf_nid_postfix(float x) { return std::signbit(x) ? 0x8000 : 0; }
int APS5_VABI __signbitl_nid_postfix(long double x) {
    static_assert(sizeof(long double) == 16, "guest long double requires x87 extended precision storage");
    return std::signbit(x) ? 0x8000 : 0;
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
static std::uint64_t g_randState = 1;

static std::uint32_t MinimalStandardNext(std::int32_t state) {
    if (state == 0) return 0x2ae1b767u;
    const auto product = static_cast<std::uint64_t>(static_cast<std::int64_t>(state)) * 16807u;
    const auto high = static_cast<std::uint32_t>(product >> 31u);
    const auto sum = (static_cast<std::uint32_t>(product) & 0x7fffffffu) + high;
    if (sum < 0x7fffffffu) return sum;
    if (sum == 0x7fffffffu) return 0;
    return static_cast<std::int32_t>(high) <= 0 ? (sum & 0x7fffffffu) - 1u : (sum & 0x7fffffffu) + 1u;
}

int APS5_VABI rand_nid_postfix() {
    std::lock_guard lock(g_randLock);
    g_randState = g_randState * 0x5851f42d4c957f2dull + 1u;
    return static_cast<int>((g_randState >> 32u) & 0x3fffffffu);
}

int APS5_VABI rand_r_nid_postfix(unsigned int* state) {
    if (state == nullptr) throw std::invalid_argument("rand_r: null state");
    *state = MinimalStandardNext(static_cast<std::int32_t>(*state));
    return static_cast<int>(*state);
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

long double APS5_VABI _LCosh_nid_postfix(long double x, long double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return y * std::cosh(x);
    if (y == 0.0L) return std::copysign(0.0L, y);
    constexpr long double Ln2High = 0x1.62e42fefa0000p-1L;
    constexpr long double Ln2Low = 0x1.cf79abc9e3b398p-40L;
    constexpr long double InverseLn2 = 0x1.71547652b82fe178p+0L;
    const long double magnitude = std::abs(x);
    if (magnitude <= (std::numeric_limits<long double>::max_exponent - 1) * (Ln2High + Ln2Low))
        return y * std::cosh(x);
    int scaleExponent;
    const long double scale = std::frexp(y, &scaleExponent);
    constexpr int MaximumExponentSpan = std::numeric_limits<long double>::max_exponent -
        std::numeric_limits<long double>::min_exponent + std::numeric_limits<long double>::digits + 4;
    if (magnitude > MaximumExponentSpan * (Ln2High + Ln2Low))
        return std::scalbn(scale, INT_MAX);
    const int exponent = static_cast<int>(magnitude * InverseLn2);
    const long double reduced = std::fma(-exponent, Ln2Low, std::fma(-exponent, Ln2High, magnitude));
    return std::scalbn(scale * (0.5L * std::exp(reduced)), scaleExponent + exponent);
}

long double APS5_VABI _LSinh_nid_postfix(long double x, long double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return y * std::sinh(x);
    if (y == 0.0L) {
        const bool negative = std::signbit(x) != std::signbit(y);
        return std::copysign(0.0L, negative ? -1.0L : 1.0L);
    }
    constexpr long double Ln2High = 0x1.62e42fefa0000p-1L;
    constexpr long double Ln2Low = 0x1.cf79abc9e3b398p-40L;
    constexpr long double InverseLn2 = 0x1.71547652b82fe178p+0L;
    const long double magnitude = std::abs(x);
    if (magnitude <= (std::numeric_limits<long double>::max_exponent - 1) * (Ln2High + Ln2Low))
        return y * std::sinh(x);
    int scaleExponent;
    const long double scale = std::frexp(y, &scaleExponent);
    constexpr int MaximumExponentSpan = std::numeric_limits<long double>::max_exponent -
        std::numeric_limits<long double>::min_exponent + std::numeric_limits<long double>::digits + 4;
    const long double sign = std::copysign(1.0L, x);
    if (magnitude > MaximumExponentSpan * (Ln2High + Ln2Low))
        return sign * std::scalbn(scale, INT_MAX);
    const int exponent = static_cast<int>(magnitude * InverseLn2);
    const long double reduced = std::fma(-exponent, Ln2Low, std::fma(-exponent, Ln2High, magnitude));
    return sign * std::scalbn(scale * (0.5L * std::exp(reduced)), scaleExponent + exponent);
}

int APS5_VABI _Fltrounds_nid_postfix(void) {
    return SonyFltrounds();
}

static_assert(sizeof(GuestFloatingPointEnvironment) == 32 && alignof(GuestFloatingPointEnvironment) == 4 && offsetof(GuestFloatingPointEnvironment, mxcsr) == 28);

int APS5_VABI feupdateenv_nid_postfix(const GuestFloatingPointEnvironment* environment) {
    if (environment == nullptr) throw std::invalid_argument("feupdateenv: null environment");
    const auto saved = *environment;
    alignas(16) std::array<std::uint32_t, 128> state;
    asm volatile("fxsave %0" : "=m"(state));
    const auto supported = state[7] != 0 ? state[7] : 0xffbfu;
    if ((saved.mxcsr & ~supported) != 0) throw std::invalid_argument("feupdateenv: unsupported MXCSR bits");
    std::uint16_t status;
    asm volatile("fnstsw %0" : "=am"(status));
    const auto pending = (status | _mm_getcsr()) & 0x3f;
    asm volatile("fldenv %0" : : "m"(saved) : "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
    _mm_setcsr(saved.mxcsr);
    GuestFloatingPointEnvironment raised;
    asm volatile("fnstenv %0" : "=m"(raised));
    raised.status |= pending;
    const auto mxcsr = _mm_getcsr() | pending;
    asm volatile("fldenv %0" : : "m"(raised) : "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
    _mm_setcsr(mxcsr);
    asm volatile("fwait");
    return 0;
}

int APS5_VABI fegetenv_nid_postfix(GuestFloatingPointEnvironment* environment) {
    if (environment == nullptr) throw std::invalid_argument("fegetenv: null environment");
    asm volatile("fnstenv %0\n\tfldcw %0" : "+m"(*environment));
    environment->mxcsr = _mm_getcsr();
    return 0;
}

int APS5_VABI feholdexcept_nid_postfix(GuestFloatingPointEnvironment* environment) {
    if (environment == nullptr) throw std::invalid_argument("feholdexcept: null environment");
    const auto csr = _mm_getcsr();
    asm volatile("fnstenv %0\n\tfnclex" : "=m"(*environment));
    environment->mxcsr = csr;
    _mm_setcsr((csr & ~0x3fu) | 0x1f80u);
    return 0;
}

const GuestFloatingPointEnvironment __fe_dfl_env_nid_postfix {0xffff037fu, 0xffff0000u, 0xffffffffu, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff}, 0x9fc0u};

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
