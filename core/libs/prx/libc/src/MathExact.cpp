#include <algorithm>
#include <bit>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "prx/libc/include/General.hpp"

namespace {

class ErrnoGuard {
public:
    ErrnoGuard() : saved(errno) {}
    ~ErrnoGuard() { errno = saved; }
    ErrnoGuard(const ErrnoGuard&) = delete;
    ErrnoGuard& operator=(const ErrnoGuard&) = delete;

private:
    int saved;
};

struct ExtendedBits {
    std::uint64_t mantissa;
    std::uint16_t signExponent;
};

ExtendedBits Bits(const long double& value) {
    ExtendedBits bits;
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(bytes));
    std::memcpy(&bits.mantissa, bytes, sizeof(bits.mantissa));
    std::memcpy(&bits.signExponent, bytes + 8, sizeof(bits.signExponent));
    return bits;
}

constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;

short TestExtended(const ExtendedBits& bits) {
    const int exponent = bits.signExponent & 0x7fff;
    const std::uint64_t fraction = bits.mantissa & 0x7fffffffffffffffull;
    if (exponent == 0x7fff) return fraction != 0 ? NotANumber : Infinite;
    if (exponent != 0 || (bits.mantissa >> 63) != 0) return Finite;
    return bits.mantissa != 0 ? Denormal : Zero;
}

template<typename TFloat>
short TestFloat(TFloat value) {
    switch (std::fpclassify(value)) {
    case FP_NAN:
        return NotANumber;
    case FP_INFINITE:
        return Infinite;
    case FP_ZERO:
        return Zero;
    case FP_SUBNORMAL:
        return Denormal;
    default:
        return Finite;
    }
}

template <typename T>
T Remainder(T x, T y) {
    const bool domain = !std::isnan(x) && !std::isnan(y) && (std::isinf(x) || y == T(0));
    T result;
    {
        ErrnoGuard guard;
        result = std::remainder(x, y);
    }
    if (domain) errno = EDOM;
    return result;
}

int Exponent(std::int64_t exponent) {
    return static_cast<int>(std::clamp<std::int64_t>(exponent, INT_MIN, INT_MAX));
}

void StoreExtended(long double* value, const ExtendedBits& bits) {
    unsigned char bytes[sizeof(long double)] = {};
    std::memcpy(bytes, value, sizeof(bytes));
    std::memcpy(bytes, &bits.mantissa, sizeof(bits.mantissa));
    std::memcpy(bytes + 8, &bits.signExponent, sizeof(bits.signExponent));
    std::memcpy(value, bytes, sizeof(bytes));
}

bool ExtendedNan(const ExtendedBits& bits) {
    return (bits.signExponent & 0x7fff) == 0x7fff && (bits.mantissa & 0x7fffffffffffffffull) != 0;
}

long double ExtendedValue(const ExtendedBits& bits) {
    const int exponent = bits.signExponent & 0x7fff;
    const long double magnitude = std::ldexp(static_cast<long double>(bits.mantissa), (exponent == 0 ? 1 : exponent) - 16383 - 63);
    return (bits.signExponent & 0x8000) != 0 ? -magnitude : magnitude;
}

template<typename TBits, int FractionBits, int ExponentBits>
short DropFraction(void* value, short scale) {
    constexpr TBits Sign = TBits{1} << (FractionBits + ExponentBits);
    constexpr TBits FractionMask = (TBits{1} << FractionBits) - 1;
    constexpr int Maximum = (1 << ExponentBits) - 1;
    constexpr int Bias = (1 << (ExponentBits - 1)) - 2;
    TBits bits;
    std::memcpy(&bits, value, sizeof(bits));
    const int field = static_cast<int>((bits >> FractionBits) & Maximum);
    if (field == Maximum) return (bits & FractionMask) != 0 ? NotANumber : Infinite;
    if ((bits & ~Sign) == 0) return Zero;
    const short drop = static_cast<short>(Bias + FractionBits + 1 - field - scale);
    if (drop <= 0) return Zero;
    if (drop > FractionBits) {
        bits &= Sign;
        std::memcpy(value, &bits, sizeof(bits));
        return Finite;
    }
    const TBits mask = (TBits{1} << drop) - 1;
    const TBits lost = bits & mask;
    bits &= ~mask;
    std::memcpy(value, &bits, sizeof(bits));
    return lost != 0 ? Finite : Zero;
}

template<typename TFloat>
short ResultCode(TFloat value) {
    if (std::isinf(value)) return Infinite;
    return value == 0 ? Zero : Finite;
}

template<typename TFloat>
short ScaleBinary(TFloat* value, std::int64_t exponent) {
    if (std::isnan(*value)) return NotANumber;
    if (std::isinf(*value)) return Infinite;
    if (*value == 0) return Zero;
    ErrnoGuard guard;
    *value = std::ldexp(*value, static_cast<int>(std::clamp<std::int64_t>(exponent, -100000, 100000)));
    return ResultCode(*value);
}

template<typename TFloat>
short UnscaleBinary(short* exponent, TFloat* value) {
    *exponent = 0;
    if (std::isnan(*value)) return NotANumber;
    if (std::isinf(*value)) return Infinite;
    if (*value == 0) return Zero;
    int unscaled = 0;
    *value = std::frexp(*value, &unscaled);
    *exponent = static_cast<short>(unscaled);
    return Finite;
}

template<typename TBits, int FractionBits>
short NormalizeFraction(void* value) {
    constexpr TBits FractionMask = (TBits{1} << FractionBits) - 1;
    constexpr TBits Sign = TBits{1} << (sizeof(TBits) * CHAR_BIT - 1);
    TBits bits;
    std::memcpy(&bits, value, sizeof(bits));
    TBits fraction = bits & FractionMask;
    short exponent = 1;
    if (fraction != 0) {
        const int shift = FractionBits + 1 - static_cast<int>(std::bit_width(fraction));
        exponent = static_cast<short>(1 - shift);
        fraction = (fraction << shift) & FractionMask;
    }
    bits = (bits & Sign) | fraction;
    std::memcpy(value, &bits, sizeof(bits));
    return exponent;
}

template<typename TFloat>
int Compare(TFloat x, TFloat y) {
    if (x < y) return 1;
    if (x > y) return 4;
    return 2;
}

template<typename TFloat>
TFloat FusedPolynomial(TFloat x, const TFloat* table, int degree) {
    TFloat result = table[0];
    for (int index = 1; index <= degree; ++index) result = std::fma(result, x, table[index]);
    return result;
}

}

extern "C" {

long double APS5_VABI copysignl_nid_postfix(long double x, long double y) { return std::copysign(x, y); }
long double APS5_VABI fdiml_nid_postfix(long double x, long double y) {
    if (std::isnan(x)) return x;
    if (std::isnan(y)) return y;
    return x > y ? x - y : 0.0L;
}

long double APS5_VABI fmodl_nid_postfix(long double x, long double y) {
    if (!std::isfinite(x) || y == 0.0L || std::isnan(y)) {
        errno = EDOM;
        return (x * y) / (x * y);
    }
    ErrnoGuard guard;
    return std::fmod(x, y);
}

long double APS5_VABI remainderl_nid_postfix(long double x, long double y) {
    return Remainder(x, y);
}

long double APS5_VABI remquol_nid_postfix(long double x, long double y, int* quotient) {
    const bool domain = !std::isnan(x) && !std::isnan(y) && (std::isinf(x) || y == 0.0L);
    int bits = 0;
    long double result;
    {
        ErrnoGuard guard;
        result = std::remquo(x, y, &bits);
    }
    if (domain) errno = EDOM;
    if (!std::isnan(result)) *quotient = bits < 0 ? -(-bits & 7) : (bits & 7);
    return result;
}

long double APS5_VABI logbl_nid_postfix(long double x) {
    ErrnoGuard guard;
    return std::logb(x);
}

int APS5_VABI ilogbl_nid_postfix(long double x) {
    if ((Bits(x).signExponent & 0x7fff) == 0x7fff) return INT_MAX;
    ErrnoGuard guard;
    return std::ilogb(x);
}

long double APS5_VABI rintl_nid_postfix(long double x) { return std::rint(x); }
long double APS5_VABI roundl_nid_postfix(long double x) { return std::isfinite(x) ? std::round(x) : x; }

std::int64_t APS5_VABI lroundl_nid_postfix(long double x) {
    ErrnoGuard guard;
    return std::llround(x);
}

std::int64_t APS5_VABI lrintl_nid_postfix(long double x) {
    ErrnoGuard guard;
    return std::llrint(x);
}

long long APS5_VABI llrintl_nid_postfix(long double x) {
    ErrnoGuard guard;
    return std::llrint(x);
}

long double APS5_VABI sqrtl_nid_postfix(long double x) {
    return std::sqrt(x);
}

long double APS5_VABI modfl_nid_postfix(long double x, long double* integral) {
    if (std::isnan(x)) {
        *integral = x;
        return x;
    }
    return std::modf(x, integral);
}
long double APS5_VABI nanl_nid_postfix(const char*) { return std::numeric_limits<long double>::quiet_NaN(); }

long double APS5_VABI scalblnl_nid_postfix(long double x, std::int64_t exponent) {
    ErrnoGuard guard;
    return std::scalbn(x, static_cast<int>(exponent));
}

long double APS5_VABI fmal_nid_postfix(long double x, long double y, long double z) {
    if (x == 0.0L || y == 0.0L) return x * y + z;
    if (z == 0.0L) return x * y;
    if (!std::isfinite(x) || !std::isfinite(y)) return x * y + z;
    if (!std::isfinite(z)) return z;
    int xExponent = 0, yExponent = 0, zExponent = 0;
    std::frexp(x, &xExponent);
    std::frexp(y, &yExponent);
    std::frexp(z, &zExponent);
    if (xExponent + yExponent - zExponent > 2 * std::numeric_limits<long double>::digits) return x * y;
    ErrnoGuard guard;
    return std::fma(x, y, z);
}

double APS5_VABI rint_nid_postfix(double x) { return std::isnan(x) ? x + x : std::rint(x); }
float APS5_VABI rintf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::rint(x); }

std::int64_t APS5_VABI lrint_nid_postfix(double x) {
    ErrnoGuard guard;
    return std::llrint(x);
}

std::int64_t APS5_VABI lrintf_nid_postfix(float x) {
    ErrnoGuard guard;
    return std::llrint(x);
}

long long APS5_VABI llrint_nid_postfix(double x) {
    ErrnoGuard guard;
    return std::llrint(x);
}

long long APS5_VABI llrintf_nid_postfix(float x) {
    ErrnoGuard guard;
    return std::llrint(x);
}

double APS5_VABI drem_nid_postfix(double x, double y) {
    return Remainder(x, y);
}

float APS5_VABI dremf_nid_postfix(float x, float y) {
    return Remainder(x, y);
}

double APS5_VABI scalbln_nid_postfix(double x, std::int64_t exponent) {
    ErrnoGuard guard;
    return std::scalbn(x, Exponent(exponent));
}

float APS5_VABI scalblnf_nid_postfix(float x, std::int64_t exponent) {
    ErrnoGuard guard;
    return std::scalbn(x, Exponent(exponent));
}

double APS5_VABI scalb_nid_postfix(double x, double exponent) {
    ErrnoGuard guard;
    if (std::isnan(x) || std::isnan(exponent)) return x * exponent;
    if (std::isinf(exponent)) return exponent > 0 ? x * exponent : x / -exponent;
    if (std::rint(exponent) != exponent) return (exponent - exponent) / (exponent - exponent);
    if (exponent > 65000.0) return std::scalbn(x, 65000);
    if (exponent < -65000.0) return std::scalbn(x, -65000);
    return std::scalbn(x, static_cast<int>(exponent));
}

float APS5_VABI scalbf_nid_postfix(float x, float exponent) {
    ErrnoGuard guard;
    if (std::isnan(x) || std::isnan(exponent)) return x * exponent;
    if (std::isinf(exponent)) return exponent > 0 ? x * exponent : x / -exponent;
    if (std::rint(exponent) != exponent) return (exponent - exponent) / (exponent - exponent);
    if (exponent > 65000.0f) return std::scalbn(x, 65000);
    if (exponent < -65000.0f) return std::scalbn(x, -65000);
    return std::scalbn(x, static_cast<int>(exponent));
}

int APS5_VABI finite_nid_postfix(double x) { return std::isfinite(x) ? 1 : 0; }
int APS5_VABI finitef_nid_postfix(float x) { return std::isfinite(x) ? 1 : 0; }

int APS5_VABI __fpclassifyl_nid_postfix(long double x) {
    const ExtendedBits bits = Bits(x);
    if ((bits.signExponent & 0x7fff) != 0 || bits.mantissa == 0) return TestExtended(bits);
    return Denormal;
}
int APS5_VABI __isinfl_nid_postfix(long double x) { return TestExtended(Bits(x)) == Infinite ? 1 : 0; }
int APS5_VABI __isnanl_nid_postfix(long double x) { return TestExtended(Bits(x)) == NotANumber ? 1 : 0; }

int APS5_VABI __isnormall_nid_postfix(long double x) {
    const int exponent = Bits(x).signExponent & 0x7fff;
    return exponent != 0 && exponent != 0x7fff ? 1 : 0;
}

short APS5_VABI _LDtest_nid_postfix(const long double* value) { return TestExtended(Bits(*value)); }
int APS5_VABI _Dclass_nid_postfix(double x) { return TestFloat(x); }
int APS5_VABI _FDclass_nid_postfix(float x) { return TestFloat(x); }
int APS5_VABI _LDclass_nid_postfix(long double x) { return TestExtended(Bits(x)); }
short APS5_VABI _LDsign_nid_postfix(long double x) { return static_cast<short>(Bits(x).signExponent & 0x8000); }

short APS5_VABI _Dint_nid_postfix(double* value, short scale) { return DropFraction<std::uint64_t, 52, 11>(value, scale); }
short APS5_VABI _FDint_nid_postfix(float* value, short scale) { return DropFraction<std::uint32_t, 23, 8>(value, scale); }

short APS5_VABI _LDint_nid_postfix(long double* value, short scale) {
    ExtendedBits bits = Bits(*value);
    const int field = bits.signExponent & 0x7fff;
    if (field == 0x7fff) return ExtendedNan(bits) ? NotANumber : Infinite;
    if (bits.mantissa == 0) return Zero;
    const short drop = static_cast<short>(0x3ffe + 64 - field - scale);
    if (drop <= 0) return Zero;
    if (drop >= 64) {
        StoreExtended(value, {0, static_cast<std::uint16_t>(bits.signExponent & 0x8000)});
        return Finite;
    }
    const std::uint64_t mask = (std::uint64_t{1} << drop) - 1;
    const std::uint64_t lost = bits.mantissa & mask;
    bits.mantissa &= ~mask;
    StoreExtended(value, bits);
    return lost != 0 ? Finite : Zero;
}

short APS5_VABI _Dscale_nid_postfix(double* value, std::int64_t exponent) { return ScaleBinary(value, exponent); }
short APS5_VABI _FDscale_nid_postfix(float* value, std::int64_t exponent) { return ScaleBinary(value, exponent); }

short APS5_VABI _LDscale_nid_postfix(long double* value, std::int64_t exponent) {
    const ExtendedBits bits = Bits(*value);
    if ((bits.signExponent & 0x7fff) == 0x7fff) return ExtendedNan(bits) ? NotANumber : Infinite;
    if (bits.mantissa == 0) {
        StoreExtended(value, {0, static_cast<std::uint16_t>(bits.signExponent & 0x8000)});
        return Zero;
    }
    *value = ExtendedValue(bits);
    const short code = ScaleBinary(value, exponent);
    if (code == Zero) *value = 0.0L;
    return code;
}

short APS5_VABI _Dunscale_nid_postfix(short* exponent, double* value) { return UnscaleBinary(exponent, value); }
short APS5_VABI _FDunscale_nid_postfix(short* exponent, float* value) { return UnscaleBinary(exponent, value); }

short APS5_VABI _LDunscale_nid_postfix(short* exponent, long double* value) {
    const ExtendedBits bits = Bits(*value);
    if ((bits.signExponent & 0x7fff) == 0x7fff) {
        *exponent = 0;
        return ExtendedNan(bits) ? NotANumber : Infinite;
    }
    if (bits.mantissa == 0) {
        *exponent = 0;
        return Zero;
    }
    *value = ExtendedValue(bits);
    return UnscaleBinary(exponent, value);
}

short APS5_VABI _Dnorm_nid_postfix(std::uint16_t* words) { return NormalizeFraction<std::uint64_t, 52>(words); }
short APS5_VABI _FDnorm_nid_postfix(std::uint16_t* words) { return NormalizeFraction<std::uint32_t, 23>(words); }

short APS5_VABI _LDnorm_nid_postfix(std::uint16_t* words) {
    std::uint64_t mantissa;
    std::memcpy(&mantissa, words, sizeof(mantissa));
    if (mantissa == 0) throw std::invalid_argument("_LDnorm: a zero significand never returns on the console");
    const int shift = std::countl_zero(mantissa);
    mantissa <<= shift;
    std::memcpy(words, &mantissa, sizeof(mantissa));
    return static_cast<short>(-shift);
}

int APS5_VABI _Fpcomp_nid_postfix(double x, double y) { return std::isnan(x) || std::isnan(y) ? 0 : Compare(x, y); }
int APS5_VABI _FFpcomp_nid_postfix(float x, float y) { return std::isnan(x) || std::isnan(y) ? 0 : Compare(x, y); }
int APS5_VABI _LFpcomp_nid_postfix(long double x, long double y) { return ExtendedNan(Bits(x)) || ExtendedNan(Bits(y)) ? 0 : Compare(x, y); }

double APS5_VABI _Poly_nid_postfix(double x, const double* table, int degree) { return FusedPolynomial(x, table, degree); }
float APS5_VABI _FPoly_nid_postfix(float x, const float* table, int degree) { return FusedPolynomial(x, table, degree); }

long double APS5_VABI _LPoly_nid_postfix(long double x, const long double* table, int degree) {
    long double result = table[0];
    for (int index = 1; index <= degree; ++index) {
        result *= x;
        result += table[index];
    }
    return result;
}

}
