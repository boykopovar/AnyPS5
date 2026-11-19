/*-
 * Copyright (c) 2005-2011 David Schultz <das@FreeBSD.ORG>
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
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
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
#include <type_traits>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ExtendedPrecisionParts.hpp"

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
        value = JoinExtended(1ull << 63, static_cast<std::uint16_t>(sign | 0x7fff));
        return Infinite;
    }
    if (biased > 0) {
        value = JoinExtended(mantissa, static_cast<std::uint16_t>(sign | biased));
        return Finite;
    }
    const long long rightShift = 1 - biased;
    if (rightShift >= 65) {
        value = 0.0L;
        return Zero;
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
    constexpr long double inputLimit = 29490.0L;
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
    if (x > inputLimit) {
        x = std::copysign(std::numeric_limits<long double>::infinity(), multiplier);
        RaiseLibcError(FE_OVERFLOW);
        return Infinite;
    }
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
    x *= multiplier;
    if (std::isinf(x)) {
        x = std::copysign(std::numeric_limits<long double>::infinity(), multiplier);
        RaiseLibcError(FE_OVERFLOW);
        return Infinite;
    }
    const short result = ScaleExtended(x, n + scale);
    if (result == Infinite) {
        RaiseLibcError(FE_OVERFLOW);
        return Infinite;
    }
    if (result == Zero) {
        RaiseLibcError(FE_UNDERFLOW);
        return Zero;
    }
    if (result == Denormal) {
        const int raised = std::fetestexcept(FE_ALL_EXCEPT);
        std::fesetexceptflag(&entryFlags, FE_ALL_EXCEPT);
        errno = entryErrno;
        if ((raised & FE_INEXACT) != 0) std::feraiseexcept(FE_INEXACT);
        return Finite;
    }
    return result;
}

template<typename TFloat>
short TestFloat(TFloat value) {
    using TBits = std::conditional_t<sizeof(TFloat) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;
    static_assert(sizeof(TFloat) == sizeof(TBits) && std::numeric_limits<TFloat>::is_iec559);
    constexpr TBits infinity = std::bit_cast<TBits>(std::numeric_limits<TFloat>::infinity());
    constexpr TBits smallestNormal = std::bit_cast<TBits>(std::numeric_limits<TFloat>::min());
    const TBits magnitude = std::bit_cast<TBits>(value) & (~TBits{0} >> 1);
    if (magnitude > infinity) return NotANumber;
    if (magnitude == infinity) return Infinite;
    if (magnitude == 0) return Zero;
    if (magnitude < smallestNormal) return Denormal;
    return Finite;
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

constexpr std::size_t ExtendedBytes = 10;

long double LoadGuestExtended(const long double* value, const char* message) {
    long double host = 0.0L;
    if (!GuestCopyIn_nid_no_patch(&host, value, ExtendedBytes)) throw std::invalid_argument(message);
    return host;
}

void StoreGuestExtended(long double* value, const long double& host, const char* message) {
    if (!GuestCopyOut_nid_no_patch(value, &host, ExtendedBytes)) throw std::invalid_argument(message);
}

template<typename T, typename Step>
T EvaluateGuestPolynomial(const T* table, int degree, const char* message, Step step) {
    std::array<T, 32> chunk;
    const std::size_t count = static_cast<std::size_t>(std::max(degree, 0)) + 1;
    T result{};
    for (std::size_t start = 0; start < count; start += chunk.size()) {
        const std::size_t size = std::min(chunk.size(), count - start);
        if (!GuestCopyIn_nid_no_patch(chunk.data(), table + start, size * sizeof(T))) throw std::invalid_argument(message);
        for (std::size_t index = 0; index < size; ++index) result = start + index == 0 ? chunk[0] : step(result, chunk[index]);
    }
    return result;
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

template<typename TBits, int FractionBits, int ExponentBits>
short ScaleFieldBits(void* value, std::int64_t exponent) {
    constexpr TBits Sign = TBits{1} << (FractionBits + ExponentBits);
    constexpr TBits Hidden = TBits{1} << FractionBits;
    constexpr TBits FractionMask = Hidden - 1;
    constexpr std::int64_t Maximum = (std::int64_t{1} << ExponentBits) - 1;
    TBits bits;
    std::memcpy(&bits, value, sizeof(bits));
    const TBits sign = bits & Sign;
    TBits fraction = bits & FractionMask;
    std::int64_t field = static_cast<std::int64_t>((bits >> FractionBits) & Maximum);
    if (field == Maximum) return fraction != 0 ? NotANumber : Infinite;
    if (field == 0) {
        if (fraction == 0) return Zero;
        const int shift = FractionBits + 1 - static_cast<int>(std::bit_width(fraction));
        fraction = (fraction << shift) & FractionMask;
        field = 1 - shift;
    }
    if (exponent > 0 && Maximum - field <= exponent) {
        bits = sign | (static_cast<TBits>(Maximum) << FractionBits);
        std::memcpy(value, &bits, sizeof(bits));
        return Infinite;
    }
    if (-field < exponent) {
        bits = sign | (static_cast<TBits>(exponent + field) << FractionBits) | fraction;
        std::memcpy(value, &bits, sizeof(bits));
        return Finite;
    }
    const auto shifted = static_cast<std::int64_t>(static_cast<std::uint64_t>(exponent) + static_cast<std::uint64_t>(field - 1));
    if (shifted < -(FractionBits + 1) || shifted >= 0) {
        std::memcpy(value, &sign, sizeof(sign));
        return Zero;
    }
    const int drop = static_cast<int>(-shifted);
    const TBits significand = Hidden | fraction;
    TBits result = significand >> drop;
    const TBits lost = significand & ((TBits{1} << drop) - 1);
    const TBits half = TBits{1} << (drop - 1);
    if (lost > half || (lost == half && (result & 1) != 0)) ++result;
    bits = sign | result;
    std::memcpy(value, &bits, sizeof(bits));
    return result != 0 ? Finite : Zero;
}

template<typename TBits, int FractionBits, int ExponentBits>
short UnscaleFieldBits(short* exponent, void* value) {
    constexpr TBits Sign = TBits{1} << (FractionBits + ExponentBits);
    constexpr TBits FractionMask = (TBits{1} << FractionBits) - 1;
    constexpr int Maximum = (1 << ExponentBits) - 1;
    constexpr int Half = (1 << (ExponentBits - 1)) - 2;
    TBits bits;
    std::memcpy(&bits, value, sizeof(bits));
    TBits fraction = bits & FractionMask;
    int field = static_cast<int>((bits >> FractionBits) & Maximum);
    if (field == Maximum) {
        *exponent = 0;
        return fraction != 0 ? NotANumber : Infinite;
    }
    if (field == 0) {
        if (fraction == 0) {
            *exponent = 0;
            return Zero;
        }
        const int shift = FractionBits + 1 - static_cast<int>(std::bit_width(fraction));
        fraction = (fraction << shift) & FractionMask;
        field = 1 - shift;
    }
    bits = (bits & Sign) | (static_cast<TBits>(Half) << FractionBits) | fraction;
    std::memcpy(value, &bits, sizeof(bits));
    *exponent = static_cast<short>(field - Half);
    return Finite;
}

template<typename TFloat>
TFloat ExpUnderflow(TFloat y) { return std::copysign(std::numeric_limits<TFloat>::min(), y) * std::numeric_limits<TFloat>::min(); }

template<typename TFloat>
TFloat ExpOverflow(TFloat y) { return std::copysign(std::numeric_limits<TFloat>::max(), y) * std::numeric_limits<TFloat>::max(); }

template<typename TFloat>
struct ExpConstants;

template<>
struct ExpConstants<double> {
    using Bits = std::uint64_t;
    static constexpr double limit = 1842;
    static constexpr double inverseLog = 1.4426950408889634;
    static constexpr double highLog = -0.693145751953125;
    static constexpr double lowLog = -1.4286068203094173e-06;
    static constexpr double tiny = 0x1p-54;
    static constexpr double half = 0.5;
};

template<>
struct ExpConstants<float> {
    using Bits = std::uint32_t;
    static constexpr float limit = 229.0f;
    static constexpr float inverseLog = 1.4426950216293335f;
    static constexpr float highLog = -0.693145751953125f;
    static constexpr float lowLog = -1.428606765330187e-06f;
    static constexpr float tiny = 0x1p-25f;
    static constexpr float half = 0.5f;
};

template<typename TFloat>
TFloat ExpProduct(TFloat y, TFloat ratio) {
    using Bits = typename ExpConstants<TFloat>::Bits;
    constexpr Bits magnitude = std::numeric_limits<Bits>::max() >> 1;
    constexpr Bits infinity = std::bit_cast<Bits>(std::numeric_limits<TFloat>::infinity());
    constexpr Bits quiet = Bits{1} << (std::numeric_limits<TFloat>::digits - 2);
    if constexpr (sizeof(TFloat) == sizeof(float)) {
        const Bits ratioBits = std::bit_cast<Bits>(ratio);
        if ((ratioBits & magnitude) > infinity) return std::bit_cast<TFloat>(static_cast<Bits>(ratioBits | quiet));
    }
    const TFloat product = y * ratio;
    const Bits bits = std::bit_cast<Bits>(y);
    if ((bits & magnitude) > infinity) return std::bit_cast<TFloat>(static_cast<Bits>(bits | quiet));
    return product;
}

template<typename TFloat, typename TScale>
short VersionedExp(TFloat* value, TFloat y, short scale, TScale scaleBits) {
    constexpr bool single = sizeof(TFloat) == sizeof(float);
    if (y == 0) {
        *value = y;
        return 0;
    }
    TFloat x = *value;
    using Constants = ExpConstants<TFloat>;
    if (-Constants::limit > x) {
        *value = ExpUnderflow(y);
        return 0;
    }
    if (x > Constants::limit) {
        *value = ExpOverflow(y);
        return 1;
    }
    const TFloat scaled = x * Constants::inverseLog;
    const TFloat rounded = scaled + (scaled < 0 ? -Constants::half : Constants::half);
    int whole;
    if constexpr (single) whole = _mm_cvtt_ss2si(_mm_set_ss(rounded));
    else whole = _mm_cvttsd_si32(_mm_set_sd(rounded));
    const TFloat count = static_cast<TFloat>(static_cast<short>(whole));
    x = std::fma(count, Constants::highLog, x);
    x = std::fma(count, Constants::lowLog, x);
    TFloat result = y;
    if (!(x > -Constants::tiny && Constants::tiny > x)) {
        const TFloat square = x * x;
        TFloat even;
        TFloat odd;
        if constexpr (single) {
            even = std::fma(square, 12.015174865722656f, 120.18228912353516f);
            odd = x * (square + 60.09114456176758f);
        } else {
            even = std::fma(square, std::fma(square, 30.01511290683317, 3362.7215441655303), 30265.401893609498);
            odd = x * std::fma(square, square + 420.3023598491063, 15132.700946804747);
        }
        TFloat ratio = (even + odd) / (even - odd);
        ratio += ratio;
        result = ExpProduct(y, ratio);
        whole = static_cast<int>(static_cast<unsigned>(whole) - 1u);
    }
    *value = result;
    const short code = scaleBits(value, static_cast<std::int64_t>(static_cast<short>(whole)) + scale);
    if (code == 1) {
        *value = ExpOverflow(y);
        return 1;
    }
    if (code == 0) {
        *value = ExpUnderflow(y);
        return 0;
    }
    return code;
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

bool NanBits(double value) { return (std::bit_cast<std::uint64_t>(value) & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }

double QuietNan(double value) { return std::bit_cast<double>(std::bit_cast<std::uint64_t>(value) | 0x0008000000000000ull); }

double OperandOrderFma(double accumulator, double x, double addend) {
    const double fused = std::fma(accumulator, x, addend);
    if (!NanBits(fused)) return fused;
    if (NanBits(x)) return QuietNan(x);
    if (NanBits(accumulator)) return QuietNan(accumulator);
    if (NanBits(addend)) return QuietNan(addend);
    return fused;
}

template<typename TFloat, typename TCoefficient, typename TLogarithm>
TFloat LanczosLogGamma(TFloat x, TLogarithm logarithm, TCoefficient series, TCoefficient root) {
    static constexpr TCoefficient Coefficients[] = {
        static_cast<TCoefficient>(76.18009172947146),
        static_cast<TCoefficient>(-86.50532032941678),
        static_cast<TCoefficient>(24.01409824083091),
        static_cast<TCoefficient>(-1.23173972450155),
        static_cast<TCoefficient>(0.001208650973866179),
        static_cast<TCoefficient>(-5.395239384953e-06),
    };
    const TFloat shifted = x + static_cast<TFloat>(5.5);
    const TFloat logShifted = logarithm(shifted);
    TFloat term = x + 1;
    TFloat sum = Coefficients[0] / term + series;
    for (int index = 1; index < 6; ++index) {
        term += 1;
        sum += Coefficients[index] / term;
    }
    const TFloat half = x + static_cast<TFloat>(0.5);
    TFloat rest;
    if constexpr (sizeof(TFloat) > sizeof(double)) rest = shifted - half * logShifted;
    else rest = std::fma(-half, logShifted, shifted);
    return logarithm(sum * root / x) - rest;
}

/* Adapted from FreeBSD release/9.0.0 lib/msun/src/s_fmal.c: https://github.com/freebsd/freebsd-src/blob/release/9.0.0/lib/msun/src/s_fmal.c */
struct ExtendedPair {
    long double hi;
    long double lo;
};

ExtendedPair AddExact(long double a, long double b) {
    ExtendedPair result;
    result.hi = a + b;
    const long double shift = result.hi - a;
    result.lo = (a - (result.hi - shift)) + (b - shift);
    return result;
}

ExtendedPair MultiplyExact(long double a, long double b) {
    constexpr long double Split = 0x1p32L + 1.0L;
    long double p = a * Split;
    long double ha = a - p;
    ha += p;
    const long double la = a - ha;
    p = b * Split;
    long double hb = b - p;
    hb += p;
    const long double lb = b - hb;
    p = ha * hb;
    const long double q = ha * lb + la * hb;
    ExtendedPair result;
    result.hi = p + q;
    result.lo = p - result.hi + q + la * lb;
    return result;
}

long double FmaProductDominates(long double x, long double y, long double z, int rounding) {
    std::feraiseexcept(FE_INEXACT);
    constexpr long double Infinity = std::numeric_limits<long double>::infinity();
    const auto stepped = [&](long double toward) {
        std::fenv_t environment;
        std::feholdexcept(&environment);
        volatile long double product = x * y;
        long double result = product;
        if (!std::fetestexcept(FE_INEXACT)) result = std::nextafter(result, toward);
        std::feupdateenv(&environment);
        return result;
    };
    switch (rounding) {
    case FE_TONEAREST:
        return x * y;
    case FE_TOWARDZERO:
        return ((x > 0.0L) != (y < 0.0L)) != (z < 0.0L) ? x * y : stepped(0.0L);
    case FE_DOWNWARD:
        return z > 0.0L ? x * y : stepped(-Infinity);
    default:
        return z < 0.0L ? x * y : stepped(Infinity);
    }
}

long double FmaFarApart(long double x, long double y, long double z, int rounding) {
    std::feraiseexcept(FE_INEXACT);
    if (!std::isnormal(z)) std::feraiseexcept(FE_UNDERFLOW);
    constexpr long double Infinity = std::numeric_limits<long double>::infinity();
    const bool productPositive = (x > 0.0L) == (y > 0.0L);
    switch (rounding) {
    case FE_TONEAREST:
        return z;
    case FE_TOWARDZERO:
        return productPositive == (z > 0.0L) ? z : std::nextafter(z, 0.0L);
    case FE_DOWNWARD:
        return productPositive ? z : std::nextafter(z, -Infinity);
    default:
        return productPositive ? std::nextafter(z, Infinity) : z;
    }
}

long double FmaScaled(long double xs, long double ys, long double zs, int spread, int rounding) {
    std::fesetround(FE_TONEAREST);
    volatile long double vxs = xs;
    const ExtendedPair xy = MultiplyExact(vxs, ys);
    const ExtendedPair sum = AddExact(xy.hi, zs);
    volatile long double rest = sum.lo + xy.lo;
    const ExtendedBits bits = Bits(sum.hi);
    const unsigned exponent = sum.hi == 0.0L ? 0x80000000u : static_cast<unsigned>((bits.signExponent & 0x7fff) - 16383);
    if (static_cast<int>(static_cast<unsigned>(spread) + exponent) > -16383) {
        volatile long double high = sum.hi;
        std::fesetround(rounding);
        return std::ldexp(high + rest, spread);
    }
    const long double floor = std::ldexp(std::copysign(0x1p-16382L, sum.hi), -spread);
    const ExtendedPair lifted = AddExact(sum.hi, floor);
    volatile long double high = lifted.hi;
    volatile long double low = lifted.lo + rest;
    volatile long double anchor = floor;
    std::fesetround(rounding);
    return std::ldexp((high + low) - anchor, spread);
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
    if (!std::isnan(result)) {
        const int low = bits < 0 ? -(-bits & 7) : (bits & 7);
        if (!GuestCopyOut_nid_no_patch(quotient, &low, sizeof(low))) throw std::invalid_argument("remquol: unwritable quotient");
    }
    if (domain) errno = EDOM;
    return result;
}

long double APS5_VABI logbl_nid_postfix(long double x) {
    ErrnoGuard guard;
    return std::logb(x);
}

int APS5_VABI ilogbl_nid_postfix(long double x) {
    const ExtendedBits bits = Bits(x);
    const int field = bits.signExponent & 0x7fff;
    if (field == 0x7fff) return INT_MAX;
    if (field != 0) return field - 16383;
    return bits.mantissa != 0 ? static_cast<int>(std::bit_width(bits.mantissa)) - 16446 : INT_MIN;
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
    const auto bits = Bits(x);
    const int field = bits.signExponent & 0x7fff;
    const bool negative = (bits.signExponent & 0x8000u) != 0 && (field != 0x7fff ? field != 0 || bits.mantissa != 0 : bits.mantissa == 0x8000000000000000ull);
    if (negative) {
        std::fenv_t environment;
        std::fegetenv(&environment);
        const long double result = std::sqrt(x);
        std::fesetenv(&environment);
        std::feraiseexcept(FE_INVALID);
        errno = EDOM;
        return result;
    }
    return std::sqrt(x);
}

/* Adapted from FreeBSD release/11.4.0 lib/msun/src/s_modfl.c: https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/msun/src/s_modfl.c */
long double APS5_VABI modfl_nid_postfix(long double x, long double* integral) {
    const ExtendedBits bits = Bits(x);
    const int exponent = (bits.signExponent & 0x7fff) - 16383;
    const long double zero = JoinExtended(0, static_cast<std::uint16_t>(bits.signExponent & 0x8000));
    long double whole = x;
    long double fraction = zero;
    if (exponent < 0) {
        whole = zero;
        fraction = x;
    } else if (exponent >= 63) {
        if (x != x) fraction = x;
    } else if (const std::uint64_t mask = (std::uint64_t{1} << (63 - exponent)) - 1; (bits.mantissa & mask) != 0) {
        whole = JoinExtended(bits.mantissa & ~mask, bits.signExponent);
        fraction = x - whole;
    }
    if (!GuestCopyOut_nid_no_patch(integral, &whole, sizeof(whole))) throw std::invalid_argument("modfl: unwritable integral");
    return fraction;
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
    ErrnoGuard guard;
    int xExponent = 0, yExponent = 0, zExponent = 0;
    const long double xs = std::frexp(x, &xExponent);
    const long double ys = std::frexp(y, &yExponent);
    const long double zs = std::frexp(z, &zExponent);
    const int rounding = std::fegetround();
    const int spread = xExponent + yExponent - zExponent;
    if (spread > std::numeric_limits<long double>::digits * 2) return FmaProductDominates(x, y, z, rounding);
    if (spread < -std::numeric_limits<long double>::digits) return FmaFarApart(x, y, z, rounding);
    return FmaScaled(xs, ys, std::ldexp(zs, -spread), xExponent + yExponent, rounding);
}

double APS5_VABI rint_nid_postfix(double x) { return std::isnan(x) ? x + x : std::rint(x); }
float APS5_VABI rintf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::rint(x); }
float APS5_VABI nearbyintf_nid_postfix(float x) { return std::isnan(x) ? x + x : std::rintf(x); }
double APS5_VABI nearbyint_nid_postfix(double x) { return std::isnan(x) ? x + x : std::rint(x); }

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

short APS5_VABI _LDtest_nid_postfix(const long double* value) {
    return TestExtended(Bits(LoadGuestExtended(value, "_LDtest: unreadable value")));
}

}

namespace {

short ExtendedExp(long double* value, long double y, std::int64_t offset) {
    offset = std::clamp<std::int64_t>(offset, -200000, 200000);
    const long double x = *value;
    const short xKind = TestExtended(Bits(x));
    const short yKind = (std::isnan(y) ? NotANumber : std::isinf(y) ? Infinite : (y == 0.0L ? Zero : TestExtended(Bits(y))));
    if (xKind == NotANumber) return NotANumber;
    if (yKind == NotANumber) {
        *value = y;
        return NotANumber;
    }
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
            const int raised = std::fetestexcept(FE_ALL_EXCEPT);
            std::fesetexceptflag(&entryFlags, FE_ALL_EXCEPT);
            errno = entryErrno;
            if ((raised & FE_INEXACT) != 0) std::feraiseexcept(FE_INEXACT);
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

}

extern "C" {

short APS5_VABI _LExp_nid_postfix(long double* value, long double y, std::int64_t offset) {
    if (value == nullptr) throw std::invalid_argument("_LExp: null value");
    long double host = LoadGuestExtended(value, "_LExp: unreadable value");
    const bool stores = TestExtended(Bits(host)) != NotANumber;
    const short kind = ExtendedExp(&host, y, offset);
    if (stores) StoreGuestExtended(value, host, "_LExp: unwritable value");
    return kind;
}

long double APS5_VABI expl_nid_postfix(long double x) {
    const short kind = TestExtended(Bits(x));
    if (kind == Zero) return 1.0L;
    if (kind == Infinite) return std::signbit(x) ? 0.0L : x;
    if (kind != NotANumber) ExtendedExp(&x, 1.0L, 0);
    return x;
}
int APS5_VABI _Dclass_nid_postfix(double x) { return TestFloat(x); }
int APS5_VABI _FDclass_nid_postfix(float x) { return TestFloat(x); }
int APS5_VABI _LDclass_nid_postfix(long double x) { return TestExtended(Bits(x)); }
short APS5_VABI _LDsign_nid_postfix(long double x) { return static_cast<short>(Bits(x).signExponent & 0x8000); }

short APS5_VABI _Dint_nid_postfix(double* value, short scale) { return DropFraction<std::uint64_t, 52, 11>(value, scale); }
short APS5_VABI _FDint_nid_postfix(float* value, short scale) { return DropFraction<std::uint32_t, 23, 8>(value, scale); }

}

namespace {

using LibcDetail::ExtendedStep;
using LibcDetail::ScaleExtendedBits;
using LibcDetail::TruncateExtended;
using LibcDetail::UnscaleExtended;

using PowerLogarithm = std::array<long double, 3>;

const std::array<PowerLogarithm, 257>& PowerLogarithms() {
    static const std::array<PowerLogarithm, 257> table = [] {
        std::array<PowerLogarithm, 257> built{};
        for (int index = 0; index <= 256; ++index) {
            unsigned __int128 scaled = 0;
            if (index == 0) {
                scaled = static_cast<unsigned __int128>(1) << 95;
            } else if (index != 256) {
                unsigned __int128 value = static_cast<unsigned __int128>(256 + index) << 118;
                unsigned __int128 digits = 0;
                for (int digit = 0; digit < 95; ++digit) {
                    const auto high = static_cast<std::uint64_t>(value >> 64);
                    const auto low = static_cast<std::uint64_t>(value);
                    value = ((static_cast<unsigned __int128>(high) * high) << 2) + ((static_cast<unsigned __int128>(high) * low) >> 61) +
                            ((static_cast<unsigned __int128>(low) * low) >> 126);
                    digits <<= 1;
                    if ((value >> 127) != 0) {
                        digits |= 1;
                        value >>= 1;
                    }
                }
                scaled = (static_cast<unsigned __int128>(1) << 95) - digits - 1;
            }
            for (int part = 0; part < 3; ++part) {
                const std::uint64_t piece = static_cast<std::uint32_t>(scaled >> (64 - 32 * part));
                const int shift = std::countl_zero(piece);
                built[index][part] = piece == 0 ? JoinExtended(0, 0x8000) : JoinExtended(piece << shift, static_cast<std::uint16_t>(0x8000 | (0x3fff + 32 - shift - 32 * part)));
            }
        }
        return built;
    }();
    return table;
}

long double PowerExtended(long double x, long double y) {
    static constexpr long double Log2EParts[] = {0xb8aa3b29p-31L, 0xb82fe176p-64L, 0xbe87fed0p-95L, 0xd23a7d10p-128L};
    constexpr long double SquareRootOfHalf = 0xb504f333f9de6484p-64L;
    constexpr long double Log2E = 0xb8aa3b295c17f0bcp-63L;
    constexpr long double Ln2 = 0xb17217f7d1cf79acp-64L;
    const long double infinity = std::numeric_limits<long double>::infinity();
    const auto negative = [](long double value) { return (Bits(value).signExponent & 0x8000) != 0; };
    if (y == 1.0L) return x;
    long double fraction = x;
    long double whole = y;
    short exponent = 0;
    const short xKind = UnscaleExtended(exponent, fraction).code;
    const short yKind = TruncateExtended(whole, 0).code;
    const auto odd = [&whole] { return TruncateExtended(whole, -1).code < 0; };
    if (yKind == Zero && y == 0.0L) return 1.0L;
    if (xKind < 0 && exponent == 1 && (fraction == 0.5L || (yKind == Infinite && fraction == -0.5L))) return 1.0L;
    if (!(xKind < 0 && yKind <= 0)) {
        if (xKind == NotANumber) return fraction;
        if (yKind == NotANumber) return y;
        if (xKind == Infinite) {
            if (!negative(fraction)) return negative(y) ? 0.0L : infinity;
            if (negative(y)) return yKind == Zero && odd() ? -0.0L : 0.0L;
            return yKind == Zero && odd() ? -infinity : infinity;
        }
        if (yKind == Infinite) return negative(y) == (exponent <= 0) ? infinity : 0.0L;
        if (negative(y)) {
            _Feraise_nid_postfix(0x04);
            return yKind == Zero && odd() && negative(fraction) ? -infinity : infinity;
        }
        return yKind == Zero && odd() && negative(fraction) ? -0.0L : 0.0L;
    }
    if (negative(fraction) && yKind < 0) {
        _Feraise_nid_postfix(0x01);
        return std::numeric_limits<long double>::quiet_NaN();
    }
    bool negate = false;
    if (!(fraction > 0.0L)) {
        fraction = -fraction;
        negate = TruncateExtended(whole, -1).code != 0;
    }
    const std::array<PowerLogarithm, 257>& logarithms = PowerLogarithms();
    long double base = fraction;
    long double centre = 1.0L;
    int index = 0;
    if (exponent == 1 && SquareRootOfHalf > fraction) {
        base = fraction + fraction;
    } else {
        index = static_cast<int>(fraction * 512.0L + 0.5L) - 256;
        centre = 0.5L + static_cast<long double>(index) * 0x1p-9L;
    }
    const long double offset = base - centre;
    const long double ratio = offset / (base + centre);
    const long double square = ratio * ratio;
    long double series = LibcDetail::LogPolynomial[0];
    for (std::size_t term = 1; term < std::size(LibcDetail::LogPolynomial); ++term) series = series * square + LibcDetail::LogPolynomial[term];
    const long double gap = 1.0L - centre;
    const long double correction = (series * square - offset) + (gap + gap);
    const long double scaledCorrection = correction * ratio;
    const long double logarithm = scaledCorrection + offset;
    const long double leading = logarithms[index][0] + static_cast<long double>(exponent);
    const long double product = (Log2E * logarithm + leading) * y;
    long double result;
    std::int64_t shift = 0;
    if (-32768.0L > product) {
        _Feraise_nid_postfix(0x10);
        return negate ? -0.0L : 0.0L;
    }
    if (product >= -16.0L && 16.0L >= product && y > -40.0L && 40.0L > y) {
        shift = static_cast<std::int64_t>(product + (0.0L > product ? -0.5L : 0.5L));
        const long double scaled = logarithm * y;
        const long double tail = (logarithms[index][1] + logarithms[index][2]) * y * Ln2 + scaled;
        result = (y * leading - static_cast<long double>(shift)) * Ln2 + tail;
    } else if (product > 32768.0L) {
        _Feraise_nid_postfix(0x08);
        return negate ? -infinity : infinity;
    } else {
        std::array<long double, 4> sum{}, factor{}, total{}, term{};
        std::array<long double, 2> low{};
        LibcDetail::SetWide(sum.data(), 4, offset);
        LibcDetail::SetWide(low.data(), 2, scaledCorrection);
        LibcDetail::AddWide(sum.data(), 4, low.data(), 2);
        if (sum[0] == 0.0L) {
            LibcDetail::SetWide(total.data(), 4, 0.0L);
        } else {
            std::copy_n(Log2EParts, 4, total.begin());
            LibcDetail::MultiplyHalf(total.data(), 4, sum[0]);
            for (int part = 1; part < 4 && sum[part] != 0.0L; ++part) {
                std::copy_n(Log2EParts, 4, term.begin());
                LibcDetail::MultiplyHalf(term.data(), 4, sum[part]);
                LibcDetail::AddWide(total.data(), 4, term.data(), 4);
            }
        }
        LibcDetail::AddWide(total.data(), 4, logarithms[index].data(), 3);
        LibcDetail::AddHalf(total.data(), 4, static_cast<long double>(exponent));
        LibcDetail::SetWide(factor.data(), 2, y);
        std::array<long double, 4> scaled = total;
        LibcDetail::MultiplyHalf(scaled.data(), 4, factor[0]);
        if (factor[1] != 0.0L) {
            term = total;
            LibcDetail::MultiplyHalf(term.data(), 4, factor[1]);
            LibcDetail::AddWide(scaled.data(), 4, term.data(), 4);
        }
        long double integral = scaled[0];
        if (scaled[0] != 0.0L && scaled[1] != 0.0L) integral = (scaled[1] + scaled[2]) + scaled[0];
        TruncateExtended(integral, 0);
        LibcDetail::AddHalf(scaled.data(), 4, -integral);
        result = LibcDetail::GetWide<long double>(scaled.data(), 4) * Ln2;
        shift = static_cast<std::int64_t>(integral);
    }
    const short state = ExtendedExp(&result, 1.0L, shift);
    if (state == Zero || (state == Infinite && 0.0L > result)) {
        result = 0.0L;
        _Feraise_nid_postfix(0x10);
    } else if (state == Infinite) {
        result = infinity;
        _Feraise_nid_postfix(0x08);
    }
    return negate ? -result : result;
}

}

extern "C" {

short APS5_VABI _LDint_nid_postfix(long double* value, short scale) {
    long double host = LoadGuestExtended(value, "_LDint: unreadable value");
    const ExtendedStep step = TruncateExtended(host, scale);
    if (step.stored) StoreGuestExtended(value, host, "_LDint: unwritable value");
    return step.code;
}

long double APS5_VABI powl_nid_postfix(long double base, long double exp) { return PowerExtended(base, exp); }

short APS5_VABI _Dscale_nid_postfix(double* value, std::int64_t exponent) { return ScaleBinary(value, exponent); }
short APS5_VABI _FDscale_nid_postfix(float* value, std::int64_t exponent) { return ScaleBinary(value, exponent); }

short APS5_VABI _LDscale_nid_postfix(long double* value, std::int64_t exponent) {
    long double host = LoadGuestExtended(value, "_LDscale: unreadable value");
    const ExtendedStep step = ScaleExtendedBits(host, exponent);
    if (step.stored) StoreGuestExtended(value, host, "_LDscale: unwritable value");
    return step.code;
}

short APS5_VABI _Dunscale_nid_postfix(short* exponent, double* value) { return UnscaleBinary(exponent, value); }
short APS5_VABI _FDunscale_nid_postfix(short* exponent, float* value) { return UnscaleBinary(exponent, value); }

short APS5_VABI _LDunscale_nid_postfix(short* exponent, long double* value) {
    long double host = LoadGuestExtended(value, "_LDunscale: unreadable value");
    short scaled = 0;
    const ExtendedStep step = UnscaleExtended(scaled, host);
    if (!GuestCopyOut_nid_no_patch(exponent, &scaled, sizeof(scaled))) throw std::invalid_argument("_LDunscale: unwritable exponent");
    if (step.stored) StoreGuestExtended(value, host, "_LDunscale: unwritable value");
    return step.code;
}

short APS5_VABI _ZNSt8__sce_v27_DscaleEPdl_nid_postfix(double* value, std::int64_t exponent) { return ScaleFieldBits<std::uint64_t, 52, 11>(value, exponent); }
short APS5_VABI _ZNSt8__sce_v28_FDscaleEPfl_nid_postfix(float* value, std::int64_t exponent) { return ScaleFieldBits<std::uint32_t, 23, 8>(value, exponent); }
short APS5_VABI _ZNSt8__sce_v28_LDscaleEPel_nid_postfix(double* value, std::int64_t exponent) { return ScaleFieldBits<std::uint64_t, 52, 11>(value, exponent); }
short APS5_VABI _ZNSt8__sce_v29_DunscaleEPsPd_nid_postfix(short* exponent, double* value) { return UnscaleFieldBits<std::uint64_t, 52, 11>(exponent, value); }
short APS5_VABI _ZNSt8__sce_v210_FDunscaleEPsPf_nid_postfix(short* exponent, float* value) { return UnscaleFieldBits<std::uint32_t, 23, 8>(exponent, value); }
short APS5_VABI _ZNSt8__sce_v210_LDunscaleEPsPe_nid_postfix(short* exponent, double* value) { return UnscaleFieldBits<std::uint64_t, 52, 11>(exponent, value); }
short APS5_VABI _ZNSt8__sce_v24_ExpEPdds_nid_postfix(double* value, double y, short scale) {
    return VersionedExp(value, y, scale, _ZNSt8__sce_v27_DscaleEPdl_nid_postfix);
}
short APS5_VABI _ZNSt8__sce_v25_FExpEPffs_nid_postfix(float* value, float y, short scale) {
    return VersionedExp(value, y, scale, _ZNSt8__sce_v28_FDscaleEPfl_nid_postfix);
}

short APS5_VABI _Dnorm_nid_postfix(std::uint16_t* words) { return NormalizeFraction<std::uint64_t, 52>(words); }
short APS5_VABI _FDnorm_nid_postfix(std::uint16_t* words) { return NormalizeFraction<std::uint32_t, 23>(words); }

short APS5_VABI _LDnorm_nid_postfix(std::uint16_t* words) {
    std::uint64_t mantissa;
    if (!GuestCopyIn_nid_no_patch(&mantissa, words, sizeof(mantissa))) throw std::invalid_argument("_LDnorm: unreadable significand");
    if (mantissa == 0) throw std::invalid_argument("_LDnorm: a zero significand never returns on the console");
    const int shift = std::countl_zero(mantissa);
    mantissa <<= shift;
    if (!GuestCopyOut_nid_no_patch(words, &mantissa, sizeof(mantissa))) throw std::invalid_argument("_LDnorm: unwritable significand");
    return static_cast<short>(-shift);
}

short APS5_VABI _ZNSt8__sce_v26_DtestEPd_nid_postfix(double* value) { return _Dtest_nid_postfix(value); }
short APS5_VABI _ZNSt8__sce_v27_FDtestEPf_nid_postfix(float* value) { return _FDtest_nid_postfix(value); }
short APS5_VABI _ZNSt8__sce_v27_LDtestEPe_nid_postfix(double* value) { return _Dtest_nid_postfix(value); }
std::uint16_t* APS5_VABI _ZNSt8__sce_v25_PmswEPd_nid_postfix(double* value) { return reinterpret_cast<std::uint16_t*>(value) + 3; }
std::uint16_t* APS5_VABI _ZNSt8__sce_v26_FPmswEPf_nid_postfix(float* value) { return reinterpret_cast<std::uint16_t*>(value) + 1; }
std::uint16_t* APS5_VABI _ZNSt8__sce_v26_LPmswEPe_nid_postfix(double* value) { return reinterpret_cast<std::uint16_t*>(value) + 3; }
short APS5_VABI _ZNSt8__sce_v25_DintEPds_nid_postfix(double* value, short scale) { return _Dint_nid_postfix(value, scale); }
short APS5_VABI _ZNSt8__sce_v26_FDintEPfs_nid_postfix(float* value, short scale) { return _FDint_nid_postfix(value, scale); }
short APS5_VABI _ZNSt8__sce_v26_LDintEPes_nid_postfix(double* value, short scale) { return _Dint_nid_postfix(value, scale); }
short APS5_VABI _ZNSt8__sce_v26_DnormEPNS_5_DvalE_nid_postfix(std::uint16_t* words) { return _Dnorm_nid_postfix(words); }
short APS5_VABI _ZNSt8__sce_v27_FDnormEPNS_5_FvalE_nid_postfix(std::uint16_t* words) { return _FDnorm_nid_postfix(words); }

int APS5_VABI _Fpcomp_nid_postfix(double x, double y) { return std::isnan(x) || std::isnan(y) ? 0 : Compare(x, y); }
int APS5_VABI _FFpcomp_nid_postfix(float x, float y) { return std::isnan(x) || std::isnan(y) ? 0 : Compare(x, y); }
int APS5_VABI _LFpcomp_nid_postfix(long double x, long double y) { return ExtendedNan(Bits(x)) || ExtendedNan(Bits(y)) ? 0 : Compare(x, y); }

double APS5_VABI _Poly_nid_postfix(double x, const double* table, int degree) { return FusedPolynomial(x, table, degree); }
float APS5_VABI _FPoly_nid_postfix(float x, const float* table, int degree) { return FusedPolynomial(x, table, degree); }

long double APS5_VABI _LPoly_nid_postfix(long double x, const long double* table, int degree) {
    return EvaluateGuestPolynomial(table, degree, "_LPoly: unreadable coefficient table", [x](long double result, long double coefficient) {
        result *= x;
        result += coefficient;
        return result;
    });
}

double APS5_VABI _ZNSt8__sce_v25_PolyEdPKdi_nid_postfix(double x, const double* table, int degree) {
    return EvaluateGuestPolynomial(table, degree, "_Poly: unreadable coefficient table", [x](double result, double coefficient) {
        return OperandOrderFma(result, x, coefficient);
    });
}
float APS5_VABI _ZNSt8__sce_v28_XLgammaEf_nid_postfix(float x) { return LanczosLogGamma<float, float>(x, logf_nid_postfix, 1.0f, 2.5066282746310007f); }
double APS5_VABI _ZNSt8__sce_v28_XLgammaEd_nid_postfix(double x) { return LanczosLogGamma<double, double>(x, log_nid_postfix, 1.000000000190015, 2.5066282746310007); }

long double APS5_VABI _ZNSt8__sce_v28_XLgammaEe_nid_postfix(long double x) {
    return LanczosLogGamma<long double, double>(x, logl_nid_postfix, 1.000000000190015, 2.5066282746310007);
}

}
