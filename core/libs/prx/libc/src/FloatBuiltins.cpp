#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "prx/libc/include/General.hpp"

namespace {

constexpr std::uint32_t DefaultMxcsr = 0x1f80;

template<typename TFloat>
struct FloatFormat;

template<>
struct FloatFormat<float> {
    using Rep = std::uint32_t;
    static constexpr int SignificandBits = 23;
    static constexpr int Bias = 127;
    static constexpr int MaxBiased = 0xff;
};

template<>
struct FloatFormat<double> {
    using Rep = std::uint64_t;
    static constexpr int SignificandBits = 52;
    static constexpr int Bias = 1023;
    static constexpr int MaxBiased = 0x7ff;
};

template<typename TFloat>
struct Decomposed {
    bool negative;
    int exponent;
    typename FloatFormat<TFloat>::Rep significand;
};

template<typename TFloat>
Decomposed<TFloat> Decompose(TFloat value) {
    using Format = FloatFormat<TFloat>;
    using Rep = typename Format::Rep;
    const Rep bits = std::bit_cast<Rep>(value);
    const Rep implicitBit = static_cast<Rep>(1) << Format::SignificandBits;
    const int biased = static_cast<int>((bits >> Format::SignificandBits) & Format::MaxBiased);
    return {(bits >> (sizeof(Rep) * 8 - 1)) != 0, biased - Format::Bias, (bits & (implicitBit - 1)) | implicitBit};
}

struct ExtendedBits {
    std::uint64_t mantissa;
    bool negative;
    int exponent;
};

ExtendedBits DecomposeExtended(long double value) {
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(bytes));
    std::uint64_t mantissa;
    std::uint16_t signExponent;
    std::memcpy(&mantissa, bytes, sizeof(mantissa));
    std::memcpy(&signExponent, bytes + 8, sizeof(signExponent));
    return {mantissa, (signExponent & 0x8000u) != 0, static_cast<int>(signExponent & 0x7fffu) - 16383};
}

long double ComposeExtended(bool negative, int biased, std::uint64_t mantissa) {
    unsigned char bytes[sizeof(long double)] = {};
    const std::uint16_t signExponent = static_cast<std::uint16_t>((negative ? 0x8000u : 0u) | static_cast<unsigned>(biased));
    std::memcpy(bytes, &mantissa, sizeof(mantissa));
    std::memcpy(bytes + 8, &signExponent, sizeof(signExponent));
    long double value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

template<typename TShift>
constexpr int ShiftMask = static_cast<int>(sizeof(TShift) * 8 - 1);

template<typename TShift, typename TFloat>
TShift ShiftSignificand(const Decomposed<TFloat>& parts) {
    constexpr int significandBits = FloatFormat<TFloat>::SignificandBits;
    const TShift significand = parts.significand;
    if (parts.exponent < significandBits) return significand >> (significandBits - parts.exponent);
    return significand << ((parts.exponent - significandBits) & ShiftMask<TShift>);
}

template<typename TResult, typename TShift, typename TFloat>
TResult FixSigned(TFloat value) {
    const Decomposed<TFloat> parts = Decompose(value);
    if (parts.exponent < 0) return 0;
    const TShift magnitude = ShiftSignificand<TShift>(parts);
    return static_cast<TResult>(parts.negative ? static_cast<TShift>(0) - magnitude : magnitude);
}

template<typename TResult, typename TFloat>
TResult FixUnsigned(TFloat value) {
    const Decomposed<TFloat> parts = Decompose(value);
    if (parts.negative || parts.exponent < 0) return 0;
    return ShiftSignificand<TResult>(parts);
}

template<typename TResult>
TResult FixExtendedSigned(long double value) {
    const ExtendedBits parts = DecomposeExtended(value);
    if (parts.exponent < 0) return 0;
    TResult magnitude;
    if constexpr (sizeof(TResult) == 16) {
        const Uint128 mantissa = parts.mantissa;
        magnitude = static_cast<TResult>(parts.exponent < 64 ? mantissa >> (63 - parts.exponent)
                                                             : mantissa << ((parts.exponent - 63) & 127));
    } else {
        magnitude = static_cast<TResult>(parts.mantissa >> ((63 - parts.exponent) & 63));
    }
    using TUnsigned = std::conditional_t<sizeof(TResult) == 16, Uint128, std::uint64_t>;
    const TUnsigned bits = static_cast<TUnsigned>(magnitude);
    return static_cast<TResult>(parts.negative ? static_cast<TUnsigned>(0) - bits : bits);
}

template<typename TResult>
TResult FixExtendedUnsigned(long double value) {
    const ExtendedBits parts = DecomposeExtended(value);
    if (parts.negative || parts.exponent < 0) return 0;
    if constexpr (sizeof(TResult) == 16) {
        const Uint128 mantissa = parts.mantissa;
        return parts.exponent < 64 ? mantissa >> (63 - parts.exponent) : mantissa << ((parts.exponent - 63) & 127);
    } else if constexpr (sizeof(TResult) == 8) {
        return parts.mantissa >> ((63 - parts.exponent) & 63);
    } else {
        return static_cast<std::uint32_t>(parts.mantissa >> 32) >> ((31 - parts.exponent) & 31);
    }
}

int HighestBit(Uint128 value) {
    const std::uint64_t high = static_cast<std::uint64_t>(value >> 64);
    return high != 0 ? 127 - std::countl_zero(high) : 63 - std::countl_zero(static_cast<std::uint64_t>(value));
}

template<typename TFloat>
TFloat RoundToFloat(bool negative, Uint128 magnitude) {
    using Format = FloatFormat<TFloat>;
    using Rep = typename Format::Rep;
    const Rep sign = negative ? static_cast<Rep>(1) << (sizeof(Rep) * 8 - 1) : 0;
    if (magnitude == 0) return std::bit_cast<TFloat>(sign);
    int exponent = HighestBit(magnitude);
    Uint128 significand;
    if (exponent <= Format::SignificandBits) {
        significand = magnitude << (Format::SignificandBits - exponent);
    } else {
        const int shift = exponent - Format::SignificandBits;
        significand = magnitude >> shift;
        const Uint128 rest = magnitude & ((static_cast<Uint128>(1) << shift) - 1);
        const Uint128 half = static_cast<Uint128>(1) << (shift - 1);
        if (rest > half || (rest == half && (significand & 1) != 0)) ++significand;
        if (significand >> (Format::SignificandBits + 1) != 0) {
            significand >>= 1;
            ++exponent;
        }
    }
    const int biased = exponent + Format::Bias;
    if (biased >= Format::MaxBiased) return std::bit_cast<TFloat>(static_cast<Rep>(sign | (static_cast<Rep>(Format::MaxBiased) << Format::SignificandBits)));
    const Rep fraction = static_cast<Rep>(significand) & ((static_cast<Rep>(1) << Format::SignificandBits) - 1);
    return std::bit_cast<TFloat>(static_cast<Rep>(sign | (static_cast<Rep>(biased) << Format::SignificandBits) | fraction));
}

long double RoundToExtended(bool negative, Uint128 magnitude) {
    if (magnitude == 0) return ComposeExtended(negative, 0, 0);
    int exponent = HighestBit(magnitude);
    Uint128 significand;
    if (exponent <= 63) {
        significand = magnitude << (63 - exponent);
    } else {
        const int shift = exponent - 63;
        significand = magnitude >> shift;
        const Uint128 rest = magnitude & ((static_cast<Uint128>(1) << shift) - 1);
        const Uint128 half = static_cast<Uint128>(1) << (shift - 1);
        if (rest > half || (rest == half && (significand & 1) != 0)) ++significand;
        if (significand >> 64 != 0) {
            significand >>= 1;
            ++exponent;
        }
    }
    return ComposeExtended(negative, exponent + 16383, static_cast<std::uint64_t>(significand));
}

Uint128 Magnitude(Int128 value) {
    return value < 0 ? static_cast<Uint128>(0) - static_cast<Uint128>(value) : static_cast<Uint128>(value);
}

template<typename TFloat>
bool IsNan(TFloat value) {
    using Format = FloatFormat<TFloat>;
    using Rep = typename Format::Rep;
    const Rep magnitude = std::bit_cast<Rep>(value) & (~static_cast<Rep>(0) >> 1);
    return magnitude > (static_cast<Rep>(Format::MaxBiased) << Format::SignificandBits);
}

template<typename TFloat>
TFloat QuietNan() {
    using Format = FloatFormat<TFloat>;
    using Rep = typename Format::Rep;
    return std::bit_cast<TFloat>(static_cast<Rep>((static_cast<Rep>(Format::MaxBiased) << Format::SignificandBits)
        | (static_cast<Rep>(1) << (Format::SignificandBits - 1))));
}

template<typename TFloat>
TFloat Negate(TFloat value) {
    using Rep = typename FloatFormat<TFloat>::Rep;
    return std::bit_cast<TFloat>(static_cast<Rep>(std::bit_cast<Rep>(value) ^ (static_cast<Rep>(1) << (sizeof(Rep) * 8 - 1))));
}

template<typename TFloat>
TFloat GeneratedNan(TFloat result, TFloat a, TFloat b) {
    return IsNan(result) && !IsNan(a) && !IsNan(b) ? QuietNan<TFloat>() : result;
}

double AddDouble(double a, double b) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\taddsd %[b], %[a]\n\tldmxcsr %[saved]"
                 : [a] "+x"(a), [saved] "+m"(saved) : [b] "x"(b), [standard] "m"(DefaultMxcsr));
    return a;
}

double MultiplyDouble(double a, double b) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\tmulsd %[b], %[a]\n\tldmxcsr %[saved]"
                 : [a] "+x"(a), [saved] "+m"(saved) : [b] "x"(b), [standard] "m"(DefaultMxcsr));
    return a;
}

double DivideDouble(double a, double b) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\tdivsd %[b], %[a]\n\tldmxcsr %[saved]"
                 : [a] "+x"(a), [saved] "+m"(saved) : [b] "x"(b), [standard] "m"(DefaultMxcsr));
    return a;
}

float AddFloat(float a, float b) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\taddss %[b], %[a]\n\tldmxcsr %[saved]"
                 : [a] "+x"(a), [saved] "+m"(saved) : [b] "x"(b), [standard] "m"(DefaultMxcsr));
    return a;
}

float MultiplyFloat(float a, float b) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\tmulss %[b], %[a]\n\tldmxcsr %[saved]"
                 : [a] "+x"(a), [saved] "+m"(saved) : [b] "x"(b), [standard] "m"(DefaultMxcsr));
    return a;
}

float DivideFloat(float a, float b) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\tdivss %[b], %[a]\n\tldmxcsr %[saved]"
                 : [a] "+x"(a), [saved] "+m"(saved) : [b] "x"(b), [standard] "m"(DefaultMxcsr));
    return a;
}

constexpr std::uint32_t TowardZeroMxcsr = DefaultMxcsr | 0x6000;

double DivideDoubleTowardZero(double a, double b) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\tdivsd %[b], %[a]\n\tldmxcsr %[saved]"
                 : [a] "+x"(a), [saved] "+m"(saved) : [b] "x"(b), [standard] "m"(TowardZeroMxcsr));
    return a;
}

float DivideFloatTowardZero(float a, float b) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\tdivss %[b], %[a]\n\tldmxcsr %[saved]"
                 : [a] "+x"(a), [saved] "+m"(saved) : [b] "x"(b), [standard] "m"(TowardZeroMxcsr));
    return a;
}

template<typename TFloat>
TFloat FlushSubnormalQuotient(TFloat quotient, TFloat truncated) {
    using Format = FloatFormat<TFloat>;
    using Rep = typename Format::Rep;
    const Rep signBit = static_cast<Rep>(1) << (sizeof(Rep) * 8 - 1);
    const Rep magnitude = std::bit_cast<Rep>(truncated) & ~signBit;
    if (magnitude >= (static_cast<Rep>(1) << Format::SignificandBits)) return quotient;
    return std::bit_cast<TFloat>(static_cast<Rep>(std::bit_cast<Rep>(truncated) & signBit));
}

float TruncateDouble(double value) {
    std::uint32_t saved = 0;
    float result;
    asm volatile("stmxcsr %[saved]\n\tldmxcsr %[standard]\n\tcvtsd2ss %[value], %[result]\n\tldmxcsr %[saved]"
                 : [result] "=x"(result), [saved] "+m"(saved) : [value] "x"(value), [standard] "m"(DefaultMxcsr));
    return result;
}

double ExtendFloat(float value) {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    const std::uint64_t sign = static_cast<std::uint64_t>(bits >> 31) << 63;
    const std::uint32_t magnitude = bits & 0x7fffffffu;
    if (magnitude >= 0x7f800000u) return std::bit_cast<double>(sign | 0x7ff0000000000000ull | (static_cast<std::uint64_t>(magnitude & 0x400000u) << 29) | (magnitude & 0x3fffffu));
    if (magnitude >= 0x00800000u) return std::bit_cast<double>(sign | ((static_cast<std::uint64_t>(magnitude) << 29) + (static_cast<std::uint64_t>(1023 - 127) << 52)));
    if (magnitude == 0) return std::bit_cast<double>(sign);
    const int shift = std::countl_zero(magnitude) - 8;
    const std::uint64_t fraction = static_cast<std::uint64_t>(magnitude << shift) & 0x7fffffu;
    const std::uint64_t biased = static_cast<std::uint64_t>(1023 - 126 - shift);
    return std::bit_cast<double>(sign | (biased << 52) | (fraction << 29));
}

template<typename TFloat>
int LessEqual(TFloat a, TFloat b, int unordered) {
    using Rep = typename FloatFormat<TFloat>::Rep;
    using Signed = std::make_signed_t<Rep>;
    if (IsNan(a) || IsNan(b)) return unordered;
    const Rep aBits = std::bit_cast<Rep>(a), bBits = std::bit_cast<Rep>(b);
    const Rep magnitudeMask = ~static_cast<Rep>(0) >> 1;
    if (((aBits | bBits) & magnitudeMask) == 0) return 0;
    const Signed aInt = static_cast<Signed>(aBits), bInt = static_cast<Signed>(bBits);
    if ((aInt & bInt) >= 0) {
        if (aInt < bInt) return -1;
        return aInt == bInt ? 0 : 1;
    }
    if (aInt > bInt) return -1;
    return aInt == bInt ? 0 : 1;
}

template<typename TFloat>
bool IsInfinite(TFloat value) { return std::isinf(value); }

template<typename TFloat>
TFloat Box(TFloat value) { return std::copysign(std::isinf(value) ? TFloat(1) : TFloat(0), value); }

template<typename TFloat>
TFloat ZeroIfNan(TFloat value) { return std::isnan(value) ? std::copysign(TFloat(0), value) : value; }

template<typename TFloat, bool Fused>
TFloat MultiplyAdd(TFloat a, TFloat b, TFloat product) {
    if constexpr (Fused) return std::fma(a, b, product);
    else return a * b + product;
}

template<typename TFloat, bool Fused>
TFloat MultiplySubtract(TFloat a, TFloat b, TFloat product) {
    return MultiplyAdd<TFloat, Fused>(a, b, std::isnan(product) ? product : -product);
}

template<typename TFloat, bool Fused>
void MultiplyComplex(TFloat a, TFloat b, TFloat c, TFloat d, TFloat& real, TFloat& imaginary) {
    const TFloat ac = a * c, bd = b * d, ad = a * d, bc = b * c;
    real = ac - bd;
    imaginary = bc + ad;
    if (!std::isnan(real) || !std::isnan(imaginary)) return;
    bool recalculate = false;
    if (IsInfinite(a) || IsInfinite(b)) {
        a = Box(a);
        b = Box(b);
        c = ZeroIfNan(c);
        d = ZeroIfNan(d);
        recalculate = true;
    }
    if (IsInfinite(c) || IsInfinite(d)) {
        c = Box(c);
        d = Box(d);
        a = ZeroIfNan(a);
        b = ZeroIfNan(b);
        recalculate = true;
    }
    if (!recalculate && (IsInfinite(ac) || IsInfinite(bd) || IsInfinite(ad) || IsInfinite(bc))) {
        a = ZeroIfNan(a);
        b = ZeroIfNan(b);
        c = ZeroIfNan(c);
        d = ZeroIfNan(d);
        recalculate = true;
    }
    if (recalculate) {
        real = MultiplySubtract<TFloat, Fused>(a, c, b * d) * static_cast<TFloat>(INFINITY);
        imaginary = MultiplyAdd<TFloat, Fused>(a, d, b * c) * static_cast<TFloat>(INFINITY);
    }
}

template<typename TFloat>
TFloat Scale(TFloat value, int exponent) {
    return static_cast<TFloat>(std::scalbn(static_cast<long double>(value), exponent));
}

template<typename TFloat>
TFloat LargerMagnitude(TFloat c, TFloat d) {
    const TFloat magnitudeC = std::fabs(c), magnitudeD = std::fabs(d);
    if (std::isnan(magnitudeC)) return magnitudeD;
    if (std::isnan(magnitudeD)) return magnitudeC;
    return magnitudeC < magnitudeD ? magnitudeD : magnitudeC;
}

template<typename TFloat, bool Fused>
void DivideComplex(TFloat a, TFloat b, TFloat c, TFloat d, TFloat& real, TFloat& imaginary) {
    int scale = 0;
    const TFloat logScale = std::logb(LargerMagnitude(c, d));
    if (std::isfinite(logScale)) {
        scale = static_cast<int>(logScale);
        c = Scale(c, -scale);
        d = Scale(d, -scale);
    }
    const TFloat denominator = MultiplyAdd<TFloat, Fused>(c, c, d * d);
    real = Scale(MultiplyAdd<TFloat, Fused>(c, a, d * b) / denominator, -scale);
    imaginary = Scale(MultiplySubtract<TFloat, Fused>(c, b, d * a) / denominator, -scale);
    if (!std::isnan(real) || !std::isnan(imaginary)) return;
    if (denominator == TFloat(0) && (!std::isnan(a) || !std::isnan(b))) {
        real = std::copysign(static_cast<TFloat>(INFINITY), c) * a;
        imaginary = std::copysign(static_cast<TFloat>(INFINITY), c) * b;
    } else if ((IsInfinite(a) || IsInfinite(b)) && std::isfinite(c) && std::isfinite(d)) {
        a = Box(a);
        b = Box(b);
        real = MultiplyAdd<TFloat, Fused>(a, c, b * d) * static_cast<TFloat>(INFINITY);
        imaginary = MultiplySubtract<TFloat, Fused>(b, c, d * a) * static_cast<TFloat>(INFINITY);
    } else if (IsInfinite(logScale) && logScale > TFloat(0) && std::isfinite(a) && std::isfinite(b)) {
        c = Box(c);
        d = Box(d);
        real = MultiplyAdd<TFloat, Fused>(a, c, d * b) * TFloat(0);
        imaginary = MultiplySubtract<TFloat, Fused>(b, c, d * a) * TFloat(0);
    }
}

using ComplexFloat = __complex__ float;
using ComplexDouble = __complex__ double;
using ComplexExtended = __complex__ long double;

ComplexFloat MakeComplex(float real, float imaginary) {
    ComplexFloat result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

ComplexDouble MakeComplex(double real, double imaginary) {
    ComplexDouble result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

ComplexExtended MakeComplex(long double real, long double imaginary) {
    ComplexExtended result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

}

extern "C" {

double APS5_VABI __adddf3_nid_postfix(double a, double b) { return GeneratedNan(AddDouble(a, b), a, b); }
float APS5_VABI __addsf3_nid_postfix(float a, float b) { return GeneratedNan(AddFloat(a, b), a, b); }

double APS5_VABI __subdf3_nid_postfix(double a, double b) {
    const double negated = Negate(b);
    return GeneratedNan(AddDouble(a, negated), a, negated);
}

float APS5_VABI __subsf3_nid_postfix(float a, float b) {
    const float negated = Negate(b);
    return GeneratedNan(AddFloat(a, negated), a, negated);
}

double APS5_VABI __muldf3_nid_postfix(double a, double b) { return GeneratedNan(MultiplyDouble(a, b), a, b); }
float APS5_VABI __mulsf3_nid_postfix(float a, float b) { return GeneratedNan(MultiplyFloat(a, b), a, b); }
double APS5_VABI __divdf3_nid_postfix(double a, double b) {
    if (IsNan(a) || IsNan(b)) return GeneratedNan(DivideDouble(a, b), a, b);
    return GeneratedNan(FlushSubnormalQuotient(DivideDouble(a, b), DivideDoubleTowardZero(a, b)), a, b);
}
float APS5_VABI __divsf3_nid_postfix(float a, float b) {
    if (IsNan(a) || IsNan(b)) return GeneratedNan(DivideFloat(a, b), a, b);
    return GeneratedNan(FlushSubnormalQuotient(DivideFloat(a, b), DivideFloatTowardZero(a, b)), a, b);
}
double APS5_VABI __negdf2_nid_postfix(double a) { return Negate(a); }
float APS5_VABI __negsf2_nid_postfix(float a) { return Negate(a); }

int APS5_VABI __eqdf2_nid_postfix(double a, double b) { return LessEqual(a, b, 1); }
int APS5_VABI __nedf2_nid_postfix(double a, double b) { return LessEqual(a, b, 1); }
int APS5_VABI __ltdf2_nid_postfix(double a, double b) { return LessEqual(a, b, 1); }
int APS5_VABI __ledf2_nid_postfix(double a, double b) { return LessEqual(a, b, 1); }
int APS5_VABI __gtdf2_nid_postfix(double a, double b) { return LessEqual(a, b, -1); }
int APS5_VABI __gedf2_nid_postfix(double a, double b) { return LessEqual(a, b, -1); }
int APS5_VABI __unorddf2_nid_postfix(double a, double b) { return IsNan(a) || IsNan(b) ? 1 : 0; }
int APS5_VABI __eqsf2_nid_postfix(float a, float b) { return LessEqual(a, b, 1); }
int APS5_VABI __nesf2_nid_postfix(float a, float b) { return LessEqual(a, b, 1); }
int APS5_VABI __ltsf2_nid_postfix(float a, float b) { return LessEqual(a, b, 1); }
int APS5_VABI __lesf2_nid_postfix(float a, float b) { return LessEqual(a, b, 1); }
int APS5_VABI __gtsf2_nid_postfix(float a, float b) { return LessEqual(a, b, -1); }
int APS5_VABI __gesf2_nid_postfix(float a, float b) { return LessEqual(a, b, -1); }
int APS5_VABI __unordsf2_nid_postfix(float a, float b) { return IsNan(a) || IsNan(b) ? 1 : 0; }

double APS5_VABI __extendsfdf2_nid_postfix(float a) { return ExtendFloat(a); }
float APS5_VABI __truncdfsf2_nid_postfix(double a) {
    if (!IsNan(a)) return TruncateDouble(a);
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(a);
    return std::bit_cast<float>(static_cast<std::uint32_t>(((bits >> 32) & 0x80000000u) | 0x7fc00000u | (bits & 0x3fffffu)));
}

std::int32_t APS5_VABI __fixsfsi_nid_postfix(float a) { return FixSigned<std::int32_t, std::uint32_t>(a); }
std::int64_t APS5_VABI __fixsfdi_nid_postfix(float a) { return FixSigned<std::int64_t, std::uint64_t>(a); }
Int128 APS5_VABI __fixsfti_nid_postfix(float a) { return FixSigned<Int128, Uint128>(a); }
std::int32_t APS5_VABI __fixdfsi_nid_postfix(double a) { return FixSigned<std::int32_t, std::uint64_t>(a); }
std::int64_t APS5_VABI __fixdfdi_nid_postfix(double a) { return FixSigned<std::int64_t, std::uint64_t>(a); }
Int128 APS5_VABI __fixdfti_nid_postfix(double a) { return FixSigned<Int128, Uint128>(a); }
std::uint32_t APS5_VABI __fixunssfsi_nid_postfix(float a) { return FixUnsigned<std::uint32_t>(a); }
std::uint64_t APS5_VABI __fixunssfdi_nid_postfix(float a) { return FixUnsigned<std::uint64_t>(a); }
Uint128 APS5_VABI __fixunssfti_nid_postfix(float a) { return FixUnsigned<Uint128>(a); }

std::uint32_t APS5_VABI __fixunsdfsi_nid_postfix(double a) {
    const Decomposed<double> parts = Decompose(a);
    if (parts.negative || parts.exponent < 0) return 0;
    return static_cast<std::uint32_t>(parts.significand >> 21) >> ((31 - parts.exponent) & 31);
}

std::uint64_t APS5_VABI __fixunsdfdi_nid_postfix(double a) { return FixUnsigned<std::uint64_t>(a); }
Uint128 APS5_VABI __fixunsdfti_nid_postfix(double a) { return FixUnsigned<Uint128>(a); }
std::int64_t APS5_VABI __fixxfdi_nid_postfix(long double a) { return FixExtendedSigned<std::int64_t>(a); }
Int128 APS5_VABI __fixxfti_nid_postfix(long double a) { return FixExtendedSigned<Int128>(a); }
std::uint32_t APS5_VABI __fixunsxfsi_nid_postfix(long double a) { return FixExtendedUnsigned<std::uint32_t>(a); }
std::uint64_t APS5_VABI __fixunsxfdi_nid_postfix(long double a) { return FixExtendedUnsigned<std::uint64_t>(a); }
Uint128 APS5_VABI __fixunsxfti_nid_postfix(long double a) { return FixExtendedUnsigned<Uint128>(a); }

double APS5_VABI __floatsidf_nid_postfix(std::int32_t a) { return static_cast<double>(a); }
double APS5_VABI __floatunsidf_nid_postfix(std::uint32_t a) { return static_cast<double>(a); }
float APS5_VABI __floatsisf_nid_postfix(std::int32_t a) { return RoundToFloat<float>(a < 0, Magnitude(a)); }
float APS5_VABI __floatunsisf_nid_postfix(std::uint32_t a) { return RoundToFloat<float>(false, a); }
double APS5_VABI __floatdidf_nid_postfix(std::int64_t a) { return static_cast<double>(a); }
float APS5_VABI __floatdisf_nid_postfix(std::int64_t a) { return static_cast<float>(a); }
long double APS5_VABI __floatdixf_nid_postfix(std::int64_t a) { return static_cast<long double>(a); }
double APS5_VABI __floatundidf_nid_postfix(std::uint64_t a) { return static_cast<double>(a); }
float APS5_VABI __floatundisf_nid_postfix(std::uint64_t a) { return static_cast<float>(a); }
long double APS5_VABI __floatundixf_nid_postfix(std::uint64_t a) { return static_cast<long double>(a); }
double APS5_VABI __floattidf_nid_postfix(Int128 a) { return RoundToFloat<double>(a < 0, Magnitude(a)); }
float APS5_VABI __floattisf_nid_postfix(Int128 a) { return RoundToFloat<float>(a < 0, Magnitude(a)); }
long double APS5_VABI __floattixf_nid_postfix(Int128 a) { return RoundToExtended(a < 0, Magnitude(a)); }
double APS5_VABI __floatuntidf_nid_postfix(Uint128 a) { return RoundToFloat<double>(false, a); }
float APS5_VABI __floatuntisf_nid_postfix(Uint128 a) { return RoundToFloat<float>(false, a); }
long double APS5_VABI __floatuntixf_nid_postfix(Uint128 a) { return RoundToExtended(false, a); }

__complex__ float APS5_VABI __mulsc3_nid_postfix(float a, float b, float c, float d) {
    float real, imaginary;
    MultiplyComplex<float, true>(a, b, c, d, real, imaginary);
    return MakeComplex(real, imaginary);
}

__complex__ double APS5_VABI __muldc3_nid_postfix(double a, double b, double c, double d) {
    double real, imaginary;
    MultiplyComplex<double, true>(a, b, c, d, real, imaginary);
    return MakeComplex(real, imaginary);
}

__complex__ long double APS5_VABI __mulxc3_nid_postfix(long double a, long double b, long double c, long double d) {
    long double real, imaginary;
    MultiplyComplex<long double, false>(a, b, c, d, real, imaginary);
    return MakeComplex(real, imaginary);
}

__complex__ float APS5_VABI __divsc3_nid_postfix(float a, float b, float c, float d) {
    float real, imaginary;
    DivideComplex<float, true>(a, b, c, d, real, imaginary);
    return MakeComplex(real, imaginary);
}

__complex__ double APS5_VABI __divdc3_nid_postfix(double a, double b, double c, double d) {
    double real, imaginary;
    DivideComplex<double, true>(a, b, c, d, real, imaginary);
    return MakeComplex(real, imaginary);
}

__complex__ long double APS5_VABI __divxc3_nid_postfix(long double a, long double b, long double c, long double d) {
    long double real, imaginary;
    DivideComplex<long double, false>(a, b, c, d, real, imaginary);
    return MakeComplex(real, imaginary);
}

long double APS5_VABI __powixf2_nid_postfix(long double base, int exponent) {
    const bool reciprocal = exponent < 0;
    long double result = (exponent & 1) != 0 ? base : 1.0L;
    for (exponent /= 2; exponent != 0; exponent /= 2) {
        base *= base;
        if (exponent & 1) result *= base;
    }
    return reciprocal ? 1 / result : result;
}

}
