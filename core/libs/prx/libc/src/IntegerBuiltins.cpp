#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"

namespace {

constexpr Uint128 Int128SignBit = static_cast<Uint128>(1) << 127;
constexpr Int128 Int128Max = static_cast<Int128>(~Int128SignBit);
constexpr Int128 Int128Min = static_cast<Int128>(Int128SignBit);

std::uint64_t High(Uint128 value) { return static_cast<std::uint64_t>(value >> 64); }
std::uint64_t Low(Uint128 value) { return static_cast<std::uint64_t>(value); }

template<typename TSigned>
struct IntegerTraits;

template<>
struct IntegerTraits<std::int32_t> {
    using Unsigned = std::uint32_t;
    static constexpr std::int32_t Min = INT32_MIN;
    static constexpr std::int32_t Max = INT32_MAX;
};

template<>
struct IntegerTraits<std::int64_t> {
    using Unsigned = std::uint64_t;
    static constexpr std::int64_t Min = INT64_MIN;
    static constexpr std::int64_t Max = INT64_MAX;
};

template<>
struct IntegerTraits<Int128> {
    using Unsigned = Uint128;
    static constexpr Int128 Min = Int128Min;
};

[[noreturn]] void Overflow(const char* function) {
    throw std::runtime_error(std::string(function) + ": integer overflow");
}

[[noreturn]] void DivideByZero(const char* function) {
    throw std::runtime_error(std::string(function) + ": integer divide by zero");
}

template<typename TSigned>
TSigned Divide(TSigned dividend, TSigned divisor, const char* function) {
    if (divisor == 0) DivideByZero(function);
    if (dividend == IntegerTraits<TSigned>::Min && divisor == -1) return dividend;
    return dividend / divisor;
}

template<typename TSigned>
TSigned Modulo(TSigned dividend, TSigned divisor, const char* function) {
    if (divisor == 0) DivideByZero(function);
    if (divisor == -1) return 0;
    return dividend % divisor;
}

template<typename TUnsigned>
TUnsigned UnsignedDivide(TUnsigned dividend, TUnsigned divisor, const char* function) {
    if (divisor == 0) DivideByZero(function);
    return dividend / divisor;
}

template<typename TUnsigned>
TUnsigned UnsignedModulo(TUnsigned dividend, TUnsigned divisor, const char* function) {
    if (divisor == 0) DivideByZero(function);
    return dividend % divisor;
}

template<typename TSigned>
TSigned WrappingAdd(TSigned a, TSigned b) {
    using TUnsigned = typename IntegerTraits<TSigned>::Unsigned;
    return static_cast<TSigned>(static_cast<TUnsigned>(a) + static_cast<TUnsigned>(b));
}

template<typename TSigned>
TSigned WrappingSubtract(TSigned a, TSigned b) {
    using TUnsigned = typename IntegerTraits<TSigned>::Unsigned;
    return static_cast<TSigned>(static_cast<TUnsigned>(a) - static_cast<TUnsigned>(b));
}

std::uint32_t UnsignedDivide32(std::uint32_t dividend, std::uint32_t divisor) {
    return divisor == 0 ? 0 : dividend / divisor;
}

template<typename TSigned>
TSigned CheckedNegate(TSigned value, const char* function) {
    if (value == IntegerTraits<TSigned>::Min) Overflow(function);
    return -value;
}

template<typename TSigned>
TSigned CheckedAbsolute(TSigned value, const char* function) {
    if (value == IntegerTraits<TSigned>::Min) Overflow(function);
    return value < 0 ? -value : value;
}

template<typename TSigned>
TSigned MultiplyWithOverflow(TSigned a, TSigned b, int* overflow) {
    using TUnsigned = typename IntegerTraits<TSigned>::Unsigned;
    const TSigned product = static_cast<TSigned>(static_cast<TUnsigned>(a) * static_cast<TUnsigned>(b));
    constexpr TSigned min = IntegerTraits<TSigned>::Min;
    constexpr TSigned max = IntegerTraits<TSigned>::Max;
    bool overflowed = false;
    if (a == min) {
        overflowed = b != 0 && b != 1;
    } else if (b == min) {
        overflowed = a != 0 && a != 1;
    } else {
        const TSigned absoluteA = a < 0 ? -a : a;
        const TSigned absoluteB = b < 0 ? -b : b;
        if (absoluteA >= 2 && absoluteB >= 2) {
            overflowed = ((a < 0) == (b < 0)) ? absoluteA > max / absoluteB : absoluteA > -(min / absoluteB);
        }
    }
    *overflow = overflowed ? 1 : 0;
    return product;
}

Int128 Multiply128(Int128 a, Int128 b) {
    const Uint128 ua = static_cast<Uint128>(a);
    const Uint128 ub = static_cast<Uint128>(b);
    const std::uint64_t aLow = Low(ua), aHigh = High(ua), bLow = Low(ub), bHigh = High(ub);
    const std::uint64_t aLowLow = aLow & 0xffffffffu, aLowHigh = aLow >> 32;
    const std::uint64_t bLowLow = bLow & 0xffffffffu, bLowHigh = bLow >> 32;
    const std::uint64_t lowLow = aLowLow * bLowLow;
    const std::uint64_t middleFirst = aLowHigh * bLowLow + (lowLow >> 32);
    const std::uint64_t middleSecond = aLowLow * bLowHigh + (middleFirst & 0xffffffffu);
    const std::uint64_t productLow = (middleSecond << 32) | (lowLow & 0xffffffffu);
    const std::uint64_t productHigh = aLowHigh * bLowHigh + (middleFirst >> 32) + (middleSecond >> 32)
        + aHigh * bLow + aLow * bHigh;
    return static_cast<Int128>((static_cast<Uint128>(productHigh) << 64) | productLow);
}

Int128 Multiply128WithOverflow(Int128 a, Int128 b, int* overflow) {
    const Int128 product = Multiply128(a, b);
    bool overflowed = false;
    if (a == Int128Min) {
        overflowed = b != 0 && b != 1;
    } else if (b == Int128Min) {
        overflowed = a != 0 && a != 1;
    } else {
        const Int128 absoluteA = a < 0 ? -a : a;
        const Int128 absoluteB = b < 0 ? -b : b;
        if (absoluteA >= 2 && absoluteB >= 2) {
            const Uint128 limit = ((a < 0) == (b < 0)) ? static_cast<Uint128>(Int128Max) : Int128SignBit;
            overflowed = static_cast<Uint128>(absoluteA) > SonyUdivti3(limit, static_cast<Uint128>(absoluteB));
        }
    }
    *overflow = overflowed ? 1 : 0;
    return product;
}

int CountLeadingZeros128(Uint128 value) {
    const std::uint64_t high = High(value);
    return high != 0 ? std::countl_zero(high) : 64 + std::countl_zero(Low(value));
}

int CountTrailingZeros128(Uint128 value) {
    const std::uint64_t low = Low(value);
    return low != 0 ? std::countr_zero(low) : 64 + std::countr_zero(High(value));
}

template<typename TValue>
int Compare(TValue a, TValue b) {
    if (a < b) return 0;
    return a > b ? 2 : 1;
}

}

extern "C" {

std::int64_t APS5_VABI __ashldi3_nid_postfix(std::int64_t value, int shift) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(value) << shift);
}

std::int64_t APS5_VABI __ashrdi3_nid_postfix(std::int64_t value, int shift) { return value >> shift; }

std::int64_t APS5_VABI __lshrdi3_nid_postfix(std::int64_t value, int shift) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(value) >> shift);
}

Int128 APS5_VABI __ashlti3_nid_postfix(Int128 value, int shift) {
    return static_cast<Int128>(static_cast<Uint128>(value) << shift);
}

Int128 APS5_VABI __ashrti3_nid_postfix(Int128 value, int shift) { return value >> shift; }

Int128 APS5_VABI __lshrti3_nid_postfix(Int128 value, int shift) {
    return static_cast<Int128>(static_cast<Uint128>(value) >> shift);
}

int APS5_VABI __clzsi2_nid_postfix(std::uint32_t value) { return std::countl_zero(value); }
int APS5_VABI __clzdi2_nid_postfix(std::uint64_t value) { return value == 0 ? 32 : std::countl_zero(value); }
int APS5_VABI __clzti2_nid_postfix(Uint128 value) { return value == 0 ? 64 : CountLeadingZeros128(value); }
int APS5_VABI __ctzsi2_nid_postfix(std::uint32_t value) { return std::countr_zero(value); }
int APS5_VABI __ctzdi2_nid_postfix(std::uint64_t value) { return value == 0 ? 32 : std::countr_zero(value); }
int APS5_VABI __ctzti2_nid_postfix(Uint128 value) { return value == 0 ? 64 : CountTrailingZeros128(value); }

int APS5_VABI __ffsdi2_nid_postfix(std::uint64_t value) {
    return value == 0 ? 0 : std::countr_zero(value) + 1;
}

int APS5_VABI __ffsti2_nid_postfix(Uint128 value) {
    return value == 0 ? 0 : CountTrailingZeros128(value) + 1;
}

int APS5_VABI __paritysi2_nid_postfix(std::uint32_t value) { return std::popcount(value) & 1; }
int APS5_VABI __paritydi2_nid_postfix(std::uint64_t value) { return std::popcount(value) & 1; }

int APS5_VABI __parityti2_nid_postfix(Uint128 value) {
    return (std::popcount(High(value)) + std::popcount(Low(value))) & 1;
}

int APS5_VABI __popcountsi2_nid_postfix(std::uint32_t value) { return std::popcount(value); }
int APS5_VABI __popcountdi2_nid_postfix(std::uint64_t value) { return std::popcount(value); }

int APS5_VABI __popcountti2_nid_postfix(Uint128 value) {
    return std::popcount(High(value)) + std::popcount(Low(value));
}

int APS5_VABI __cmpdi2_nid_postfix(std::int64_t a, std::int64_t b) { return Compare(a, b); }
int APS5_VABI __cmpti2_nid_postfix(Int128 a, Int128 b) { return Compare(a, b); }
int APS5_VABI __ucmpdi2_nid_postfix(std::uint64_t a, std::uint64_t b) { return Compare(a, b); }
int APS5_VABI __ucmpti2_nid_postfix(Uint128 a, Uint128 b) { return Compare(a, b); }

std::int64_t APS5_VABI __negdi2_nid_postfix(std::int64_t value) {
    return static_cast<std::int64_t>(0 - static_cast<std::uint64_t>(value));
}

Int128 APS5_VABI __negti2_nid_postfix(Int128 value) {
    return static_cast<Int128>(0 - static_cast<Uint128>(value));
}

std::int64_t APS5_VABI __muldi3_nid_postfix(std::int64_t a, std::int64_t b) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b));
}

Int128 APS5_VABI __multi3_nid_postfix(Int128 a, Int128 b) { return Multiply128(a, b); }

std::int32_t APS5_VABI __divsi3_nid_postfix(std::int32_t a, std::int32_t b) { return Divide(a, b, "__divsi3"); }
std::int64_t APS5_VABI __divdi3_nid_postfix(std::int64_t a, std::int64_t b) { return Divide(a, b, "__divdi3"); }
std::int32_t APS5_VABI __modsi3_nid_postfix(std::int32_t a, std::int32_t b) { return Modulo(a, b, "__modsi3"); }
std::int64_t APS5_VABI __moddi3_nid_postfix(std::int64_t a, std::int64_t b) { return Modulo(a, b, "__moddi3"); }

std::uint32_t APS5_VABI __udivsi3_nid_postfix(std::uint32_t a, std::uint32_t b) {
    return UnsignedDivide32(a, b);
}

std::uint64_t APS5_VABI __udivdi3_nid_postfix(std::uint64_t a, std::uint64_t b) {
    return UnsignedDivide(a, b, "__udivdi3");
}

std::uint32_t APS5_VABI __umodsi3_nid_postfix(std::uint32_t a, std::uint32_t b) {
    return a - UnsignedDivide32(a, b) * b;
}

std::uint64_t APS5_VABI __umoddi3_nid_postfix(std::uint64_t a, std::uint64_t b) {
    return UnsignedModulo(a, b, "__umoddi3");
}

std::int32_t APS5_VABI __divmodsi4_nid_postfix(std::int32_t a, std::int32_t b, std::int32_t* remainder) {
    const std::int32_t quotient = Divide(a, b, "__divmodsi4");
    *remainder = static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(quotient) * static_cast<std::uint32_t>(b));
    return quotient;
}

std::int64_t APS5_VABI __divmoddi4_nid_postfix(std::int64_t a, std::int64_t b, std::int64_t* remainder) {
    const std::int64_t quotient = Divide(a, b, "__divmoddi4");
    *remainder = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(quotient) * static_cast<std::uint64_t>(b));
    return quotient;
}

std::uint32_t APS5_VABI __udivmodsi4_nid_postfix(std::uint32_t a, std::uint32_t b, std::uint32_t* remainder) {
    const std::uint32_t quotient = UnsignedDivide32(a, b);
    *remainder = a - quotient * b;
    return quotient;
}

std::uint64_t APS5_VABI __udivmoddi4_nid_postfix(std::uint64_t a, std::uint64_t b, std::uint64_t* remainder) {
    const std::uint64_t quotient = UnsignedDivide(a, b, "__udivmoddi4");
    if (remainder != nullptr) *remainder = a - quotient * b;
    return quotient;
}

Uint128 APS5_VABI __udivmodti4_nid_postfix(Uint128 a, Uint128 b, Uint128* remainder) {
    if (b == 0) DivideByZero("__udivmodti4");
    Uint128 rest = 0;
    const Uint128 quotient = SonyUdivmod128(a, b, &rest);
    if (remainder != nullptr) *remainder = rest;
    return quotient;
}

std::int32_t APS5_VABI __mulosi4_nid_postfix(std::int32_t a, std::int32_t b, int* overflow) {
    return MultiplyWithOverflow(a, b, overflow);
}

std::int64_t APS5_VABI __mulodi4_nid_postfix(std::int64_t a, std::int64_t b, int* overflow) {
    return MultiplyWithOverflow(a, b, overflow);
}

Int128 APS5_VABI __muloti4_nid_postfix(Int128 a, Int128 b, int* overflow) {
    return Multiply128WithOverflow(a, b, overflow);
}

std::int32_t APS5_VABI __mulvsi3_nid_postfix(std::int32_t a, std::int32_t b) {
    int overflow = 0;
    const std::int32_t product = MultiplyWithOverflow(a, b, &overflow);
    if (overflow) Overflow("__mulvsi3");
    return product;
}

std::int64_t APS5_VABI __mulvdi3_nid_postfix(std::int64_t a, std::int64_t b) {
    int overflow = 0;
    const std::int64_t product = MultiplyWithOverflow(a, b, &overflow);
    if (overflow) Overflow("__mulvdi3");
    return product;
}

Int128 APS5_VABI __mulvti3_nid_postfix(Int128 a, Int128 b) {
    int overflow = 0;
    const Int128 product = Multiply128WithOverflow(a, b, &overflow);
    if (overflow) Overflow("__mulvti3");
    return product;
}

std::int32_t APS5_VABI __addvsi3_nid_postfix(std::int32_t a, std::int32_t b) { return WrappingAdd(a, b); }
std::int64_t APS5_VABI __addvdi3_nid_postfix(std::int64_t a, std::int64_t b) { return WrappingAdd(a, b); }
Int128 APS5_VABI __addvti3_nid_postfix(Int128 a, Int128 b) { return WrappingAdd(a, b); }
std::int32_t APS5_VABI __subvsi3_nid_postfix(std::int32_t a, std::int32_t b) { return WrappingSubtract(a, b); }
std::int64_t APS5_VABI __subvdi3_nid_postfix(std::int64_t a, std::int64_t b) { return WrappingSubtract(a, b); }
Int128 APS5_VABI __subvti3_nid_postfix(Int128 a, Int128 b) { return WrappingSubtract(a, b); }
std::int32_t APS5_VABI __negvsi2_nid_postfix(std::int32_t value) { return CheckedNegate(value, "__negvsi2"); }
std::int64_t APS5_VABI __negvdi2_nid_postfix(std::int64_t value) { return CheckedNegate(value, "__negvdi2"); }
Int128 APS5_VABI __negvti2_nid_postfix(Int128 value) { return CheckedNegate(value, "__negvti2"); }
std::int32_t APS5_VABI __absvsi2_nid_postfix(std::int32_t value) { return CheckedAbsolute(value, "__absvsi2"); }
std::int64_t APS5_VABI __absvdi2_nid_postfix(std::int64_t value) { return CheckedAbsolute(value, "__absvdi2"); }
Int128 APS5_VABI __absvti2_nid_postfix(Int128 value) { return CheckedAbsolute(value, "__absvti2"); }

}
