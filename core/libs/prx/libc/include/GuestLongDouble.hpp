#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTLONGDOUBLE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTLONGDOUBLE_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

#ifdef ANYPS5_GUEST_LIFTED
#include "prx/libc/include/GuestCpu.hpp"

extern "C" {
void GuestCpuExtendedAdd_nid_no_patch(const void* left, const void* right, void* result);
void GuestCpuExtendedSubtract_nid_no_patch(const void* left, const void* right, void* result);
void GuestCpuExtendedMultiply_nid_no_patch(const void* left, const void* right, void* result);
void GuestCpuExtendedDivide_nid_no_patch(const void* left, const void* right, void* result);
void GuestCpuExtendedSquareRoot_nid_no_patch(const void* value, void* result);
void GuestCpuExtendedRoundToIntegral_nid_no_patch(const void* value, void* result);
double GuestCpuExtendedToDouble_nid_no_patch(const void* value);
float GuestCpuExtendedToFloat_nid_no_patch(const void* value);
std::int64_t GuestCpuExtendedToInt64_nid_no_patch(const void* value);
#if LDBL_MANT_DIG == 113
void GuestCpuStoreExtended_nid_no_patch(const long double* host, void* guest);
#endif
}
#endif

namespace LibcDetail {

inline constexpr std::size_t GuestLongDoubleBytes = 16;

enum class IntegralRounding { TowardZero, Downward, Upward, HalfAwayFromZero };

#ifdef ANYPS5_GUEST_LIFTED

class alignas(16) GuestLongDouble {
public:
    constexpr GuestLongDouble() = default;
    constexpr GuestLongDouble(int value) : GuestLongDouble(static_cast<long long>(value)) {}
    constexpr GuestLongDouble(long value) : GuestLongDouble(static_cast<long long>(value)) {}
    constexpr GuestLongDouble(long long value)
        : GuestLongDouble(FromInteger(value < 0, value < 0 ? 0ull - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value))) {}
    constexpr GuestLongDouble(unsigned value) : GuestLongDouble(FromInteger(false, value)) {}
    constexpr GuestLongDouble(unsigned long value) : GuestLongDouble(FromInteger(false, value)) {}
    constexpr GuestLongDouble(unsigned long long value) : GuestLongDouble(FromInteger(false, value)) {}
    constexpr GuestLongDouble(float value) : GuestLongDouble(static_cast<double>(value)) {}
    constexpr GuestLongDouble(double value) : GuestLongDouble(FromDouble(std::bit_cast<std::uint64_t>(value))) {}

    static constexpr GuestLongDouble FromBits(const std::uint16_t signExponent, const std::uint64_t significand) {
        GuestLongDouble value;
        value._signExponent = signExponent;
        value._significand = significand;
        return value;
    }

    constexpr std::uint16_t SignExponent() const { return _signExponent; }
    constexpr std::uint64_t Significand() const { return _significand; }
    constexpr int Exponent() const { return _signExponent & 0x7fff; }
    constexpr bool Negative() const { return (_signExponent & 0x8000) != 0; }
    constexpr bool Nan() const { return Exponent() == 0x7fff && (_significand << 1) != 0; }
    constexpr bool Infinite() const { return Exponent() == 0x7fff && (_significand << 1) == 0; }
    constexpr bool Zero() const { return _significand == 0 && Exponent() != 0x7fff; }

    explicit operator double() const { return GuestCpuExtendedToDouble_nid_no_patch(this); }
    explicit operator float() const { return GuestCpuExtendedToFloat_nid_no_patch(this); }

    template<typename TInteger>
        requires std::is_integral_v<TInteger>
    explicit operator TInteger() const {
        if constexpr (std::is_unsigned_v<TInteger> && sizeof(TInteger) == sizeof(std::uint64_t)) {
            constexpr auto Half = FromBits(0x403e, 0x8000000000000000ull);
            if (!(*this < Half)) {
                const auto shifted = *this - Half;
                return static_cast<TInteger>(GuestCpuExtendedToInt64_nid_no_patch(&shifted)) + (TInteger {1} << 63);
            }
        }
        return static_cast<TInteger>(GuestCpuExtendedToInt64_nid_no_patch(this));
    }

    friend GuestLongDouble operator+(const GuestLongDouble& left, const GuestLongDouble& right) {
        GuestLongDouble result;
        GuestCpuExtendedAdd_nid_no_patch(&left, &right, &result);
        return result;
    }

    friend GuestLongDouble operator-(const GuestLongDouble& left, const GuestLongDouble& right) {
        GuestLongDouble result;
        GuestCpuExtendedSubtract_nid_no_patch(&left, &right, &result);
        return result;
    }

    friend GuestLongDouble operator*(const GuestLongDouble& left, const GuestLongDouble& right) {
        GuestLongDouble result;
        GuestCpuExtendedMultiply_nid_no_patch(&left, &right, &result);
        return result;
    }

    friend GuestLongDouble operator/(const GuestLongDouble& left, const GuestLongDouble& right) {
        GuestLongDouble result;
        GuestCpuExtendedDivide_nid_no_patch(&left, &right, &result);
        return result;
    }

    friend constexpr GuestLongDouble operator-(const GuestLongDouble& value) {
        return FromBits(static_cast<std::uint16_t>(value._signExponent ^ 0x8000), value._significand);
    }

    GuestLongDouble& operator+=(const GuestLongDouble& other) { return *this = *this + other; }
    GuestLongDouble& operator-=(const GuestLongDouble& other) { return *this = *this - other; }
    GuestLongDouble& operator*=(const GuestLongDouble& other) { return *this = *this * other; }
    GuestLongDouble& operator/=(const GuestLongDouble& other) { return *this = *this / other; }

    friend constexpr std::partial_ordering operator<=>(const GuestLongDouble& left, const GuestLongDouble& right) {
        if (left.Nan() || right.Nan()) return std::partial_ordering::unordered;
        const auto first = left.Ordered();
        const auto second = right.Ordered();
        return first < second ? std::partial_ordering::less : first > second ? std::partial_ordering::greater : std::partial_ordering::equivalent;
    }

    friend constexpr bool operator==(const GuestLongDouble& left, const GuestLongDouble& right) { return (left <=> right) == 0; }

private:
    std::uint64_t _significand = 0;
    std::uint16_t _signExponent = 0;
    std::uint16_t _padding[3] = {};

    static constexpr GuestLongDouble FromInteger(const bool negative, const unsigned long long magnitude) {
        if (magnitude == 0) return {};
        const int shift = std::countl_zero(magnitude);
        return FromBits(static_cast<std::uint16_t>((negative ? 0x8000 : 0) | (16383 + 63 - shift)), magnitude << shift);
    }

    static constexpr GuestLongDouble FromDouble(const std::uint64_t bits) {
        const auto sign = static_cast<std::uint16_t>((bits >> 48) & 0x8000);
        const auto exponent = static_cast<int>((bits >> 52) & 0x7ff);
        const auto fraction = bits & 0xfffffffffffffull;
        if (exponent == 0x7ff) return FromBits(static_cast<std::uint16_t>(sign | 0x7fff), fraction == 0 ? 0x8000000000000000ull : 0xc000000000000000ull | (fraction << 11));
        if (exponent == 0) {
            if (fraction == 0) return FromBits(sign, 0);
            const int shift = std::countl_zero(fraction);
            return FromBits(static_cast<std::uint16_t>(sign | (15372 - shift)), fraction << shift);
        }
        return FromBits(static_cast<std::uint16_t>(sign | (exponent - 1023 + 16383)), 0x8000000000000000ull | (fraction << 11));
    }

    constexpr __int128 Ordered() const {
        if (_significand == 0) return 0;
        const int exponent = Exponent() == 0 && (_significand >> 63) != 0 ? 1 : Exponent();
        const auto magnitude = static_cast<__int128>(static_cast<unsigned __int128>(exponent) << 64 | _significand);
        return Negative() ? -magnitude : magnitude;
    }
};

static_assert(sizeof(GuestLongDouble) == GuestLongDoubleBytes && std::is_trivially_copyable_v<GuestLongDouble>);

}

template<>
struct std::numeric_limits<LibcDetail::GuestLongDouble> {
    using Value = LibcDetail::GuestLongDouble;
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr bool is_iec559 = true;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr int digits = 64;
    static constexpr int digits10 = 18;
    static constexpr int max_digits10 = 21;
    static constexpr int radix = 2;
    static constexpr int min_exponent = -16381;
    static constexpr int min_exponent10 = -4931;
    static constexpr int max_exponent = 16384;
    static constexpr int max_exponent10 = 4932;
    static constexpr std::float_round_style round_style = std::round_to_nearest;
    static constexpr Value min() noexcept { return Value::FromBits(0x0001, 0x8000000000000000ull); }
    static constexpr Value lowest() noexcept { return Value::FromBits(0xfffe, 0xffffffffffffffffull); }
    static constexpr Value max() noexcept { return Value::FromBits(0x7ffe, 0xffffffffffffffffull); }
    static constexpr Value epsilon() noexcept { return Value::FromBits(0x3fc0, 0x8000000000000000ull); }
    static constexpr Value round_error() noexcept { return Value::FromBits(0x3ffe, 0x8000000000000000ull); }
    static constexpr Value infinity() noexcept { return Value::FromBits(0x7fff, 0x8000000000000000ull); }
    static constexpr Value quiet_NaN() noexcept { return Value::FromBits(0x7fff, 0xc000000000000000ull); }
    static constexpr Value signaling_NaN() noexcept { return Value::FromBits(0x7fff, 0xa000000000000000ull); }
    static constexpr Value denorm_min() noexcept { return Value::FromBits(0x0000, 0x0000000000000001ull); }
};

namespace LibcDetail {

#if LDBL_MANT_DIG >= 64
using HostWide = long double;
#else
using HostWide = double;
#endif

#if LDBL_MANT_DIG == 64
inline HostWide ToWide(const GuestLongDouble value) {
    HostWide wide;
    std::memcpy(&wide, &value, sizeof(wide));
    return wide;
}

inline GuestLongDouble FromWide(const HostWide wide) {
    GuestLongDouble value;
    std::memcpy(&value, &wide, 10);
    return value;
}
#elif LDBL_MANT_DIG == 113
inline HostWide ToWide(const GuestLongDouble value) {
    if (value.Nan() || value.Infinite()) {
        const std::uint64_t fraction = value.Significand() & 0x7FFF'FFFF'FFFF'FFFFull;
        const std::uint64_t low = fraction << 49;
        const std::uint64_t high = (fraction >> 15) | (static_cast<std::uint64_t>(value.SignExponent()) << 48);
        HostWide wide;
        std::memcpy(&wide, &low, sizeof(low));
        std::memcpy(reinterpret_cast<unsigned char*>(&wide) + 8, &high, sizeof(high));
        return wide;
    }
    const int exponent = value.Exponent() == 0 ? 1 : value.Exponent();
    const HostWide magnitude = std::ldexp(static_cast<HostWide>(value.Significand()), exponent - 16383 - 63);
    return value.Negative() ? -magnitude : magnitude;
}

inline GuestLongDouble FromWide(const HostWide wide) {
    unsigned char bytes[GuestLongDoubleBytes] = {};
    GuestCpuStoreExtended_nid_no_patch(&wide, bytes);
    GuestLongDouble value;
    std::memcpy(&value, bytes, 10);
    return value;
}
#else
inline HostWide ToWide(const GuestLongDouble value) {
    return static_cast<double>(value);
}

inline GuestLongDouble FromWide(const HostWide wide) {
    return GuestLongDouble(wide);
}
#endif

inline GuestLongDouble GuestLongDoubleLoad(const void* source) {
    GuestLongDouble value;
    std::memcpy(&value, source, 10);
    return value;
}

inline void GuestLongDoubleStore(void* target, const GuestLongDouble value) {
    std::memcpy(target, &value, 10);
}

inline std::array<unsigned char, GuestLongDoubleBytes> GuestLongDoubleImage(const GuestLongDouble value) {
    return std::bit_cast<std::array<unsigned char, GuestLongDoubleBytes>>(value);
}

GuestLongDouble GuestLongDoubleParse(const char* text, char** end);

constexpr bool IsNan(const GuestLongDouble value) { return value.Nan(); }
constexpr bool IsInf(const GuestLongDouble value) { return value.Infinite(); }
constexpr bool IsFinite(const GuestLongDouble value) { return value.Exponent() != 0x7fff; }
constexpr bool SignBit(const GuestLongDouble value) { return value.Negative(); }
constexpr GuestLongDouble Fabs(const GuestLongDouble value) { return GuestLongDouble::FromBits(static_cast<std::uint16_t>(value.SignExponent() & 0x7fff), value.Significand()); }

constexpr GuestLongDouble CopySign(const GuestLongDouble magnitude, const GuestLongDouble sign) {
    return GuestLongDouble::FromBits(static_cast<std::uint16_t>((magnitude.SignExponent() & 0x7fff) | (sign.SignExponent() & 0x8000)), magnitude.Significand());
}

constexpr GuestLongDouble Frexp(const GuestLongDouble value, int* exponent) {
    if (value.Zero() || !IsFinite(value)) {
        *exponent = 0;
        return value;
    }
    int biased = value.Exponent();
    auto significand = value.Significand();
    if (biased == 0) {
        const int shift = std::countl_zero(significand);
        significand <<= shift;
        biased = 1 - shift;
    }
    *exponent = biased - 16382;
    return GuestLongDouble::FromBits(static_cast<std::uint16_t>((value.SignExponent() & 0x8000) | 16382), significand);
}

inline GuestLongDouble Scalbn(const GuestLongDouble value, const long exponent) {
    if (!IsFinite(value) || value.Zero() || exponent == 0) return value;
    int current = 0;
    const auto fraction = Frexp(value, &current);
    const long target = current + exponent;
    const auto sign = static_cast<std::uint16_t>(value.SignExponent() & 0x8000);
    if (target + 16382 >= 1 && target + 16382 <= 0x7ffe) return GuestLongDouble::FromBits(static_cast<std::uint16_t>(sign | (target + 16382)), fraction.Significand());
    const long start = target > 0 ? 16383 : -16381;
    const long rest = std::clamp(target - start, -16382l, 16383l);
    return GuestLongDouble::FromBits(static_cast<std::uint16_t>(sign | (start + 16382)), fraction.Significand()) * GuestLongDouble::FromBits(static_cast<std::uint16_t>(16383 + rest), 0x8000000000000000ull);
}

constexpr int Ilogb(const GuestLongDouble value) {
    if (value.Nan()) return FP_ILOGBNAN;
    if (value.Infinite()) return std::numeric_limits<int>::max();
    if (value.Zero()) return FP_ILOGB0;
    int exponent = 0;
    Frexp(value, &exponent);
    return exponent - 1;
}

inline GuestLongDouble RoundIntegral(const GuestLongDouble value, const IntegralRounding rounding) {
    if (!IsFinite(value) || value.Zero()) return value;
    const int unbiased = value.Exponent() - 16383;
    if (unbiased >= 63) return value;
    const bool negative = value.Negative();
    const auto magnitude = value.Significand();
    const int fractionBits = unbiased < 0 ? 64 : 63 - unbiased;
    const auto whole = fractionBits >= 64 ? 0 : magnitude >> fractionBits;
    const auto fraction = fractionBits >= 64 ? magnitude : magnitude & ((1ull << fractionBits) - 1);
    const bool half = fractionBits <= 64 && fractionBits > 0 && (fractionBits == 64 ? unbiased == -1 : (fraction >> (fractionBits - 1)) != 0);
    bool away = false;
    switch (rounding) {
    case IntegralRounding::TowardZero: away = false; break;
    case IntegralRounding::Downward: away = negative && fraction != 0; break;
    case IntegralRounding::Upward: away = !negative && fraction != 0; break;
    case IntegralRounding::HalfAwayFromZero: away = half; break;
    }
    const GuestLongDouble integral(whole + (away ? 1ull : 0ull));
    return CopySign(integral, value);
}

inline GuestLongDouble Remainder(const GuestLongDouble dividend, const GuestLongDouble divisor, const bool nearest, long long* quotient) {
    *quotient = 0;
    if (dividend.Nan() || divisor.Nan() || dividend.Infinite() || divisor.Zero()) return (dividend * divisor) / (dividend * divisor);
    if (divisor.Infinite() || dividend.Zero()) return dividend;
    int dividendExponent = 0;
    int divisorExponent = 0;
    const auto dividendSignificand = Frexp(Fabs(dividend), &dividendExponent).Significand();
    const auto divisorSignificand = Frexp(Fabs(divisor), &divisorExponent).Significand();
    if (dividendExponent < divisorExponent) {
        if (!nearest || dividendExponent < divisorExponent - 1 || dividendSignificand <= divisorSignificand) return dividend;
        *quotient = SignBit(dividend) != SignBit(divisor) ? -1 : 1;
        return dividend - CopySign(Fabs(divisor), dividend);
    }
    unsigned __int128 remainder = dividendSignificand;
    unsigned long long bits = 0;
    for (int exponent = dividendExponent; exponent >= divisorExponent; --exponent) {
        bits <<= 1;
        if (remainder >= divisorSignificand) {
            remainder -= divisorSignificand;
            bits |= 1;
        }
        if (exponent != divisorExponent) remainder <<= 1;
    }
    auto magnitude = static_cast<unsigned long long>(remainder);
    auto sign = dividend;
    if (nearest && (remainder << 1 > divisorSignificand || (remainder << 1 == divisorSignificand && (bits & 1) != 0))) {
        magnitude = divisorSignificand - magnitude;
        ++bits;
        sign = -dividend;
    }
    *quotient = SignBit(dividend) != SignBit(divisor) ? -static_cast<long long>(bits & 0x7fffffffffffffffull) : static_cast<long long>(bits & 0x7fffffffffffffffull);
    return CopySign(Scalbn(GuestLongDouble(magnitude), divisorExponent - 64), sign);
}

inline GuestLongDouble Sqrt(const GuestLongDouble value) {
    GuestLongDouble result;
    GuestCpuExtendedSquareRoot_nid_no_patch(&value, &result);
    return result;
}

inline GuestLongDouble Rint(const GuestLongDouble value) {
    GuestLongDouble result;
    GuestCpuExtendedRoundToIntegral_nid_no_patch(&value, &result);
    return result;
}

inline GuestLongDouble NearbyInt(const GuestLongDouble value) {
    const auto status = GuestCpu::X87Status();
    const auto result = Rint(value);
    GuestCpu::SetX87Status(status);
    return result;
}

inline GuestLongDouble Fma(const GuestLongDouble left, const GuestLongDouble right, const GuestLongDouble addend) {
    return FromWide(std::fma(ToWide(left), ToWide(right), ToWide(addend)));
}

inline GuestLongDouble Logb(const GuestLongDouble value) {
    if (value.Nan()) return value;
    if (value.Infinite()) return Fabs(value);
    if (value.Zero()) return GuestLongDouble(-1) / Fabs(value);
    return GuestLongDouble(Ilogb(value));
}

inline std::uint64_t NormalizedSignificand(const GuestLongDouble value, int* exponent) {
    return Frexp(Fabs(value), exponent).Significand();
}

inline long long LlRound(const GuestLongDouble value) {
    return static_cast<long long>(RoundIntegral(value, IntegralRounding::HalfAwayFromZero));
}

inline long long LlRint(const GuestLongDouble value) {
    return static_cast<long long>(Rint(value));
}

inline GuestLongDouble Modf(const GuestLongDouble value, GuestLongDouble* whole) {
    if (!IsFinite(value)) {
        *whole = value;
        return value.Nan() ? value : CopySign(GuestLongDouble {}, value);
    }
    *whole = RoundIntegral(value, IntegralRounding::TowardZero);
    return CopySign(value - *whole, value);
}

#else

using GuestLongDouble = long double;
using HostWide = long double;

inline long double Fma(const long double left, const long double right, const long double addend) {
    return std::fma(left, right, addend);
}

static_assert(std::numeric_limits<long double>::digits == 64 || std::numeric_limits<long double>::digits == 113);

inline HostWide ToWide(const GuestLongDouble value) { return value; }
inline GuestLongDouble FromWide(const HostWide value) { return value; }

inline long double GuestLongDoubleLoad(const void* source) {
    long double value;
    static_assert(sizeof(value) == GuestLongDoubleBytes);
    if constexpr (std::numeric_limits<long double>::digits == 64) {
        std::memcpy(&value, source, sizeof(value));
    } else {
        std::uint64_t mantissa;
        std::uint16_t signExponent;
        std::memcpy(&mantissa, source, sizeof(mantissa));
        std::memcpy(&signExponent, static_cast<const unsigned char*>(source) + 8, sizeof(signExponent));
        const std::uint64_t fraction = mantissa & 0x7FFF'FFFF'FFFF'FFFFull;
        const std::uint64_t low = fraction << 49;
        const std::uint64_t high = (fraction >> 15) | (static_cast<std::uint64_t>(signExponent & 0x7FFF) << 48) | (static_cast<std::uint64_t>(signExponent >> 15) << 63);
        std::memcpy(&value, &low, sizeof(low));
        std::memcpy(reinterpret_cast<unsigned char*>(&value) + 8, &high, sizeof(high));
    }
    return value;
}

inline void GuestLongDoubleStore(void* target, long double value) {
    if constexpr (std::numeric_limits<long double>::digits == 64) {
        std::memcpy(target, &value, sizeof(value));
    } else {
        std::uint64_t low;
        std::uint64_t high;
        std::memcpy(&low, &value, sizeof(low));
        std::memcpy(&high, reinterpret_cast<const unsigned char*>(&value) + 8, sizeof(high));
        const auto exponent = static_cast<std::uint16_t>((high >> 48) & 0x7FFF);
        const std::uint64_t fraction = ((high & 0xFFFF'FFFF'FFFFull) << 15) | (low >> 49);
        const std::uint64_t mantissa = fraction | (exponent != 0 ? 0x8000'0000'0000'0000ull : 0);
        const auto signExponent = static_cast<std::uint16_t>(exponent | ((high >> 63) << 15));
        unsigned char bytes[GuestLongDoubleBytes] = {};
        std::memcpy(bytes, &mantissa, sizeof(mantissa));
        std::memcpy(bytes + 8, &signExponent, sizeof(signExponent));
        std::memcpy(target, bytes, sizeof(bytes));
    }
}

inline std::array<unsigned char, GuestLongDoubleBytes> GuestLongDoubleImage(const long double value) {
    std::array<unsigned char, GuestLongDoubleBytes> bytes{};
    GuestLongDoubleStore(bytes.data(), value);
    return bytes;
}

inline long double GuestLongDoubleParse(const char* text, char** end) {
    return std::strtold(text, end);
}

#endif

template<typename T>
    requires std::is_floating_point_v<T>
bool IsNan(const T value) { return std::isnan(value); }

template<typename T>
    requires std::is_floating_point_v<T>
bool IsInf(const T value) { return std::isinf(value); }

template<typename T>
    requires std::is_floating_point_v<T>
bool IsFinite(const T value) { return std::isfinite(value); }

template<typename T>
    requires std::is_floating_point_v<T>
bool SignBit(const T value) { return std::signbit(value); }

template<typename T>
    requires std::is_floating_point_v<T>
T Fabs(const T value) { return std::fabs(value); }

template<typename T>
    requires std::is_floating_point_v<T>
T CopySign(const T magnitude, const T sign) { return std::copysign(magnitude, sign); }

template<typename T>
    requires std::is_floating_point_v<T>
T Frexp(const T value, int* exponent) { return std::frexp(value, exponent); }

template<typename T>
    requires std::is_floating_point_v<T>
T Scalbn(const T value, const long exponent) { return std::scalbln(value, exponent); }

template<typename T>
    requires std::is_floating_point_v<T>
int Ilogb(const T value) { return std::ilogb(value); }

template<typename T>
    requires std::is_floating_point_v<T>
T Sqrt(const T value) { return std::sqrt(value); }

template<typename T>
    requires std::is_floating_point_v<T>
T Rint(const T value) { return std::rint(value); }

template<typename T>
    requires std::is_floating_point_v<T>
T NearbyInt(const T value) { return std::nearbyint(value); }

template<typename T>
    requires std::is_floating_point_v<T>
T Logb(const T value) { return std::logb(value); }

template<typename T>
    requires std::is_floating_point_v<T>
std::uint64_t NormalizedSignificand(const T value, int* exponent) { return static_cast<std::uint64_t>(std::ldexp(std::frexp(std::fabs(value), exponent), 64)); }

template<typename T>
    requires std::is_floating_point_v<T>
long long LlRound(const T value) { return std::llround(value); }

template<typename T>
    requires std::is_floating_point_v<T>
long long LlRint(const T value) { return std::llrint(value); }

template<typename T>
    requires std::is_floating_point_v<T>
T Modf(const T value, T* whole) { return std::modf(value, whole); }

template<typename T>
    requires std::is_floating_point_v<T>
T RoundIntegral(const T value, const IntegralRounding rounding) {
    switch (rounding) {
    case IntegralRounding::TowardZero: return std::trunc(value);
    case IntegralRounding::Downward: return std::floor(value);
    case IntegralRounding::Upward: return std::ceil(value);
    case IntegralRounding::HalfAwayFromZero: return std::round(value);
    }
    return value;
}

template<typename T>
    requires std::is_floating_point_v<T>
T Remainder(const T dividend, const T divisor, const bool nearest, long long* quotient) {
    if (nearest) {
        int bits = 0;
        const T result = std::remquo(dividend, divisor, &bits);
        *quotient = bits;
        return result;
    }
    *quotient = 0;
    return std::fmod(dividend, divisor);
}

}

#ifdef ANYPS5_GUEST_LIFTED
#define APS5_GUEST_LONG_DOUBLE(literal, signExponent, significand) ::LibcDetail::GuestLongDouble::FromBits(signExponent, significand)
#else
#define APS5_GUEST_LONG_DOUBLE(literal, signExponent, significand) literal
#endif


#endif
