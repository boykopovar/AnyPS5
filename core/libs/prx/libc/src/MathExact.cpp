#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
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

long double JoinExtended(std::uint64_t mantissa, std::uint16_t signExponent) {
    unsigned char bytes[sizeof(long double)]{};
    std::memcpy(bytes, &mantissa, sizeof(mantissa));
    std::memcpy(bytes + sizeof(mantissa), &signExponent, sizeof(signExponent));
    long double value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;

short TestExtended(const ExtendedBits& bits) {
    const int exponent = bits.signExponent & 0x7fff;
    const std::uint64_t fraction = bits.mantissa & 0x7fffffffffffffffull;
    if (exponent == 0x7fff) return fraction != 0 ? NotANumber : Infinite;
    if ((bits.mantissa >> 63) != 0) return Finite;
    return exponent != 0 || bits.mantissa != 0 ? Denormal : Zero;
}

void RaiseLibcError(int exceptions) {
    if ((exceptions & (FE_OVERFLOW | FE_UNDERFLOW)) != 0) exceptions |= FE_INEXACT;
    std::feraiseexcept(exceptions);
    const bool range = (exceptions & FE_INVALID) == 0 &&
        (exceptions & (FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW)) != 0;
    errno = range ? ERANGE : EDOM;
}

bool OverflowToInfinity(bool negative) {
    const int mode = std::fegetround();
    return mode == FE_UPWARD ? !negative : mode == FE_DOWNWARD ? negative : mode == FE_TOWARDZERO ? false : true;
}

short OverflowScaled(long double& value, long double multiplier) {
    const bool toInfinity = OverflowToInfinity(std::signbit(multiplier));
    value = std::copysign(toInfinity ? std::numeric_limits<long double>::infinity() : std::numeric_limits<long double>::max(), multiplier);
    RaiseLibcError(FE_OVERFLOW);
    return toInfinity ? Infinite : Finite;
}

short ScaleExtended(long double& value, long long exponent) {
    const ExtendedBits parts = Bits(value);
    const std::uint16_t sign = parts.signExponent & 0x8000;
    if ((parts.signExponent & 0x7fff) == 0x7fff) {
        return (parts.mantissa << 1) == 0 ? Infinite : NotANumber;
    }
    if (parts.mantissa == 0) {
        value = JoinExtended(0, sign);
        return Zero;
    }
    long long biased = std::max<long long>(parts.signExponent & 0x7fff, 1);
    const int shift = std::countl_zero(parts.mantissa);
    const std::uint64_t mantissa = parts.mantissa << shift;
    biased += exponent - shift;
    if (biased >= 0x7fff) {
        const bool negative = sign != 0;
        const bool toInfinity = OverflowToInfinity(negative);
        value = toInfinity
            ? JoinExtended(1ull << 63, static_cast<std::uint16_t>(sign | 0x7fff))
            : JoinExtended(~0ull, static_cast<std::uint16_t>(sign | 0x7ffe));
        RaiseLibcError(FE_OVERFLOW);
        return toInfinity ? Infinite : Finite;
    }
    if (biased > 0) {
        value = JoinExtended(mantissa, static_cast<std::uint16_t>(sign | biased));
        return Finite;
    }
    const long long rightShift = 1 - biased;
    if (rightShift >= 65) {
        const bool negative = sign != 0;
        const int mode = std::fegetround();
        const bool away = (mode == FE_UPWARD && !negative) || (mode == FE_DOWNWARD && negative);
        value = JoinExtended(away ? 1 : 0, sign);
        return away ? Denormal : Zero;
    }
    std::uint64_t kept = rightShift == 64 ? 0 : mantissa >> rightShift;
    const std::uint64_t lost = rightShift == 64 ? mantissa : mantissa << (64 - rightShift);
    bool roundUp = false;
    switch (std::fegetround()) {
    case FE_UPWARD:
        roundUp = lost != 0 && sign == 0;
        break;
    case FE_DOWNWARD:
        roundUp = lost != 0 && sign != 0;
        break;
    case FE_TOWARDZERO:
        break;
    default:
        roundUp = lost > 1ull << 63 || (lost == 1ull << 63 && (kept & 1) != 0);
        break;
    }
    if (roundUp) ++kept;
    value = JoinExtended(kept, static_cast<std::uint16_t>(sign | (kept >> 63)));
    return TestExtended(Bits(value));
}

short FinishExpScale(long double& value, long long exponent) {
    const short result = ScaleExtended(value, exponent);
    if (result == Infinite) RaiseLibcError(FE_OVERFLOW);
    if (result == Zero || result == Denormal) RaiseLibcError(FE_UNDERFLOW);
    return result;
}

short UnderflowScaled(long double& value, long double multiplier) {
    value = std::copysign(0.0L, multiplier);
    return Zero;
}

short ExpScaled(long double& x, long long scale, long double multiplier) {
    std::fexcept_t entryFlags;
    std::fegetexceptflag(&entryFlags, FE_ALL_EXCEPT);
    const int entryErrno = errno;
    auto finalize = [&](long double value, short kind) -> short {
        x = value;
        return kind;
    };
    constexpr long double inverseLn2 = 0xb.8aa3b295c17f0bcp-3L;
    constexpr long double ln2Low = -0xe.8e7bcd5e4f1d9ccp-35L;
    constexpr long double tiny = 0x1p-66L;
    constexpr double ln2High = -0x1.62e42fecp-1;
    constexpr long double p0 = 0xc.99bcb9bdeb30f49p-23L;
    constexpr long double p1 = 0xa.578826a9b3aafa0p-13L;
    constexpr long double p2 = 0xe.8b94728735d9363p-7L;
    constexpr long double q0 = 0x8.45a5963c196b530p-17L;
    constexpr long double q1 = 0xf.83a71f722cba3c4p-10L;
    constexpr long double inputLimit = 50000.0L;
    if (std::isinf(x)) {
        if (x > 0.0L) {
            if (multiplier == 0.0L) {
                x = std::numeric_limits<long double>::quiet_NaN();
                RaiseLibcError(FE_INVALID);
                return NotANumber;
            }
            if (std::isinf(multiplier)) x = multiplier;
            else x = std::copysign(std::numeric_limits<long double>::infinity(), multiplier);
            return Infinite;
        }
        if (std::isinf(multiplier)) {
            x = std::numeric_limits<long double>::quiet_NaN();
            RaiseLibcError(FE_INVALID);
            return NotANumber;
        }
        return finalize(std::copysign(0.0L, multiplier), Zero);
    }
    if (std::isinf(multiplier)) {
        x = multiplier;
        return Infinite;
    }
    if (x == 0.0L) {
        if (scale == 0) return finalize(multiplier, Finite);
        long double result = multiplier;
        std::fexcept_t productFlags;
        std::fegetexceptflag(&productFlags, FE_ALL_EXCEPT);
        const short resultKind = ScaleExtended(result, scale);
        x = result;
        if (resultKind == Infinite) {
            RaiseLibcError(FE_OVERFLOW);
            return Infinite;
        }
        if (resultKind == Zero) {
            RaiseLibcError(FE_UNDERFLOW);
            return Zero;
        }
        if (resultKind == Denormal) {
            std::fesetexceptflag(&productFlags, FE_ALL_EXCEPT);
            errno = entryErrno;
            return Finite;
        }
        return resultKind;
    }
    if (scale == 0 && x == 1.0L && multiplier == 1.0L) {
        x = 0xa.df85458a2bb4a9bp-2L;
        return Finite;
    }
    if (x < -inputLimit) return finalize(std::copysign(0.0L, multiplier), Zero);
    if (x > inputLimit) return OverflowScaled(x, multiplier);
    const long double g = x * inverseLn2;
    if (std::isnan(g)) {
        x = g;
        return NotANumber;
    }
    const long long n = static_cast<long long>(g + (g < 0.0L ? -0.5L : 0.5L));
    const long double nn = static_cast<long double>(n);
    x += nn * static_cast<long double>(ln2High);
    x += nn * ln2Low;
    if (x > -tiny && x < tiny) {
        x = 1.0L;
    } else {
        const long double w = x * x;
        const long double even = ((p0 * w + p1) * w + p2) * w;
        const long double odd = x * ((q0 * w + q1) * w + 0.5L);
        x = ((even + odd) + 1.0L) / ((even - odd) + 1.0L);
    }
    int multiplierExponent = 0;
    const long double multiplierMantissa = std::frexp(multiplier, &multiplierExponent);
    x *= multiplierMantissa;
    const long long finalShift = n + scale + multiplierExponent;
    short result = ScaleExtended(x, finalShift);
    if (result == Infinite) {
        RaiseLibcError(FE_OVERFLOW);
        return Infinite;
    }
    if (result == Zero) {
        RaiseLibcError(FE_UNDERFLOW);
        return Zero;
    }
    if (result == Denormal) {
        std::fesetexceptflag(&entryFlags, FE_ALL_EXCEPT);
        errno = entryErrno;
        return Finite;
    }
    return result;
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
float APS5_VABI nearbyintf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::rintf(x); }

std::int64_t APS5_VABI lrint_nid_postfix(double x) { return _mm_cvtsd_si64(_mm_set_sd(x)); }

std::int64_t APS5_VABI lrintf_nid_postfix(float x) { return _mm_cvtss_si64(_mm_set_ss(x)); }

long long APS5_VABI llrint_nid_postfix(double x) { return _mm_cvtsd_si64(_mm_set_sd(x)); }

long long APS5_VABI llrintf_nid_postfix(float x) { return _mm_cvtss_si64(_mm_set_ss(x)); }

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
    const int exponent = bits.signExponent & 0x7fff;
    if (exponent == 0) return bits.mantissa != 0 ? Denormal : Zero;
    return exponent != 0x7fff ? Finite : TestExtended(bits);
}
int APS5_VABI __isinfl_nid_postfix(long double x) { return TestExtended(Bits(x)) == Infinite ? 1 : 0; }
int APS5_VABI __isnanl_nid_postfix(long double x) { return TestExtended(Bits(x)) == NotANumber ? 1 : 0; }

int APS5_VABI __isnormall_nid_postfix(long double x) {
    const int exponent = Bits(x).signExponent & 0x7fff;
    return exponent != 0 && exponent != 0x7fff ? 1 : 0;
}

short APS5_VABI _LDtest_nid_postfix(const long double* value) { return TestExtended(Bits(*value)); }
short APS5_VABI _LExp_nid_postfix(long double* value, long double y, short offset) {
    if (value == nullptr) throw std::invalid_argument("_LExp: null value");
    const long double x = *value;
    const short xKind = TestExtended(Bits(x));
    const short yKind = (std::isnan(y) ? NotANumber : std::isinf(y) ? Infinite : (y == 0.0L ? Zero : TestExtended(Bits(y))));
    if (xKind == NotANumber) return NotANumber;
    if (yKind == NotANumber) return NotANumber;
    if (xKind == Infinite) {
        if (x > 0.0L) {
            if (yKind == Zero) {
                *value = std::numeric_limits<long double>::quiet_NaN();
                RaiseLibcError(FE_INVALID);
                return NotANumber;
            }
            if (yKind == Infinite) *value = y;
            else *value = std::copysign(std::numeric_limits<long double>::infinity(), y);
            return Infinite;
        }
        if (yKind == Infinite) {
            *value = std::numeric_limits<long double>::quiet_NaN();
            RaiseLibcError(FE_INVALID);
            return NotANumber;
        }
        *value = std::copysign(0.0L, y);
        return Zero;
    }
    if (yKind == Zero) {
        *value = y;
        return Zero;
    }
    if (yKind == Infinite) {
        *value = y;
        return Infinite;
    }
    if (xKind == Zero) {
        if (offset == 0) {
            *value = y;
            return Finite;
        }
        long double result = y;
        std::fexcept_t entryFlags;
        std::fegetexceptflag(&entryFlags, FE_ALL_EXCEPT);
        const int entryErrno = errno;
        const short resultKind = ScaleExtended(result, offset);
        *value = result;
        if (resultKind == Infinite) {
            RaiseLibcError(FE_OVERFLOW);
            return Infinite;
        }
        if (resultKind == Zero) {
            RaiseLibcError(FE_UNDERFLOW);
            return Zero;
        }
        if (resultKind == Denormal) {
            std::fesetexceptflag(&entryFlags, FE_ALL_EXCEPT);
            errno = entryErrno;
            return Finite;
        }
        return resultKind;
    }
    if (offset == 0 && x == 1.0L && y == 1.0L) {
        *value = 0xa.df85458a2bb4a9bp-2L;
        return Finite;
    }
    long double result = x;
    const short resultKind = ExpScaled(result, offset, y);
    *value = result;
    return resultKind;
}
int APS5_VABI _Dclass_nid_postfix(double x) { return TestFloat(x); }
int APS5_VABI _FDclass_nid_postfix(float x) { return TestFloat(x); }
int APS5_VABI _LDclass_nid_postfix(long double x) { return TestExtended(Bits(x)); }
short APS5_VABI _LDsign_nid_postfix(long double x) { return static_cast<short>(Bits(x).signExponent & 0x8000); }

}
