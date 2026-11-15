#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint64_t NaNBits = 0x7ff8000000000000;
constexpr std::uint64_t InfinityBits = 0x7ff0000000000000;
constexpr std::uint64_t NegativeZeroBits = 0x8000000000000000;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU numeric: %s\n", message);
    std::abort();
}

double FromBits(std::uint64_t bits) {
    return std::bit_cast<double>(bits);
}

std::uint64_t Bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void Classification() {
    const double infinity = std::numeric_limits<double>::infinity();
    for (const std::uint64_t bits : std::array<std::uint64_t, 5>{NaNBits, 0x7ff0000000000001ull, 0x7fffffffffffffffull, 0xfff8000000000000ull, 0xfff0000000000001ull}) {
        Require(uprv_isNaN_nid_postfix(FromBits(bits)) == 1, "NaN patterns are NaN");
        Require(uprv_isInfinite_nid_postfix(FromBits(bits)) == 0, "NaN patterns are not infinite");
        Require(uprv_isPositiveInfinity_nid_postfix(FromBits(bits)) == 0 && uprv_isNegativeInfinity_nid_postfix(FromBits(bits)) == 0, "NaN has no infinite sign");
    }
    for (const double value : {0.0, -0.0, 1.0, -1.0, std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min(), -std::numeric_limits<double>::max()}) {
        Require(uprv_isNaN_nid_postfix(value) == 0, "finite values are not NaN");
        Require(uprv_isInfinite_nid_postfix(value) == 0, "finite values are not infinite");
        Require(uprv_isPositiveInfinity_nid_postfix(value) == 0 && uprv_isNegativeInfinity_nid_postfix(value) == 0, "finite values have no infinite sign");
    }
    Require(uprv_isInfinite_nid_postfix(infinity) == 1 && uprv_isInfinite_nid_postfix(-infinity) == 1, "infinities are infinite");
    Require(uprv_isNaN_nid_postfix(infinity) == 0 && uprv_isNaN_nid_postfix(-infinity) == 0, "infinities are not NaN");
    Require(uprv_isPositiveInfinity_nid_postfix(infinity) == 1 && uprv_isPositiveInfinity_nid_postfix(-infinity) == 0, "positive infinity");
    Require(uprv_isNegativeInfinity_nid_postfix(-infinity) == 1 && uprv_isNegativeInfinity_nid_postfix(infinity) == 0, "negative infinity");
    Require(Bits(uprv_getNaN_nid_postfix()) == NaNBits, "NaN bit pattern");
    Require(Bits(uprv_getInfinity_nid_postfix()) == InfinityBits, "infinity bit pattern");
}

void LibraryMath() {
    Require(uprv_floor_nid_postfix(-2.5) == -3.0 && uprv_floor_nid_postfix(2.5) == 2.0, "floor");
    Require(uprv_ceil_nid_postfix(-2.5) == -2.0 && uprv_ceil_nid_postfix(2.5) == 3.0, "ceil");
    Require(Bits(uprv_fabs_nid_postfix(-0.0)) == 0 && uprv_fabs_nid_postfix(-7.25) == 7.25, "fabs");
    double integral = 0;
    Require(uprv_modf_nid_postfix(-3.25, &integral) == -0.25 && integral == -3.0, "modf");
    Require(Rejects([] { uprv_modf_nid_postfix(1.5, nullptr); }), "modf rejects a null integral pointer");
    Require(uprv_fmod_nid_postfix(7.5, 2.0) == 1.5 && uprv_fmod_nid_postfix(-7.5, 2.0) == -1.5, "fmod");
    Require(uprv_pow_nid_postfix(2.0, 10.0) == 1024.0 && uprv_pow_nid_postfix(-2.0, 3.0) == -8.0, "pow");
    Require(uprv_pow10_nid_postfix(3) == 1000.0 && uprv_pow10_nid_postfix(0) == 1.0, "pow10");
    Require(uprv_pow10_nid_postfix(-2) == std::pow(10.0, -2.0) && uprv_pow10_nid_postfix(400) == std::numeric_limits<double>::infinity(), "pow10 of negative and large exponents");
    Require(uprv_log_nid_postfix(1.0) == 0.0 && uprv_log_nid_postfix(0.0) == -std::numeric_limits<double>::infinity(), "log");
    Require(uprv_isNaN_nid_postfix(uprv_log_nid_postfix(-1.0)) == 1, "log of a negative number");
    Require(uprv_maxMantissa_nid_postfix() == 18014398509481983.0, "maximum mantissa is 2^54 - 1");
}

void Rounding() {
    Require(uprv_round_nid_postfix(2.5) == 3.0 && uprv_round_nid_postfix(-2.5) == -2.0, "round half up");
    Require(uprv_round_nid_postfix(-2.6) == -3.0 && uprv_round_nid_postfix(2.4) == 2.0, "round to nearest");
    Require(uprv_round_nid_postfix(0.49999999999999994) == 1.0, "round adds one half before flooring");
    Require(Bits(uprv_round_nid_postfix(-0.5)) == 0, "round of minus one half is positive zero");
    Require(uprv_trunc_nid_postfix(3.7) == 3.0 && uprv_trunc_nid_postfix(-3.7) == -3.0, "trunc");
    Require(Bits(uprv_trunc_nid_postfix(-0.5)) == NegativeZeroBits && Bits(uprv_trunc_nid_postfix(-0.0)) == NegativeZeroBits, "trunc keeps the sign of zero");
    Require(Bits(uprv_trunc_nid_postfix(0.5)) == 0, "trunc of a positive fraction");
    Require(Bits(uprv_trunc_nid_postfix(std::numeric_limits<double>::infinity())) == InfinityBits, "trunc of positive infinity");
    Require(Bits(uprv_trunc_nid_postfix(-std::numeric_limits<double>::infinity())) == InfinityBits, "trunc of negative infinity returns positive infinity");
    Require(Bits(uprv_trunc_nid_postfix(FromBits(0xfff8000000000001ull))) == NaNBits, "trunc returns the canonical NaN");
}

void Extremes() {
    const double nan = FromBits(0xfff8000000000001ull);
    Require(Bits(uprv_fmax_nid_postfix(nan, 1.0)) == NaNBits && Bits(uprv_fmax_nid_postfix(1.0, nan)) == NaNBits, "fmax with NaN");
    Require(Bits(uprv_fmin_nid_postfix(nan, 1.0)) == NaNBits && Bits(uprv_fmin_nid_postfix(1.0, nan)) == NaNBits, "fmin with NaN");
    Require(uprv_fmax_nid_postfix(1.0, 2.0) == 2.0 && uprv_fmax_nid_postfix(2.0, 1.0) == 2.0, "fmax");
    Require(uprv_fmin_nid_postfix(1.0, 2.0) == 1.0 && uprv_fmin_nid_postfix(2.0, 1.0) == 1.0, "fmin");
    Require(uprv_fmax_nid_postfix(-std::numeric_limits<double>::infinity(), 3.0) == 3.0, "fmax with negative infinity");
    Require(Bits(uprv_fmax_nid_postfix(-0.0, 0.0)) == 0 && Bits(uprv_fmax_nid_postfix(0.0, 0.0)) == 0, "fmax prefers positive zero after negative zero");
    Require(Bits(uprv_fmax_nid_postfix(0.0, -0.0)) == NegativeZeroBits && Bits(uprv_fmax_nid_postfix(-0.0, -0.0)) == NegativeZeroBits, "fmax returns its second zero");
    Require(Bits(uprv_fmin_nid_postfix(0.0, -0.0)) == NegativeZeroBits && Bits(uprv_fmin_nid_postfix(-0.0, 0.0)) == NegativeZeroBits, "fmin prefers negative zero");
    Require(Bits(uprv_fmin_nid_postfix(0.0, 0.0)) == 0, "fmin of positive zeros");
    constexpr std::int32_t low = std::numeric_limits<std::int32_t>::min();
    constexpr std::int32_t high = std::numeric_limits<std::int32_t>::max();
    Require(uprv_max_nid_postfix(low, high) == high && uprv_max_nid_postfix(high, low) == high && uprv_max_nid_postfix(-4, -4) == -4, "max");
    Require(uprv_min_nid_postfix(low, high) == low && uprv_min_nid_postfix(high, low) == low && uprv_min_nid_postfix(7, 7) == 7, "min");
}

void Overflow() {
    constexpr std::int32_t low = std::numeric_limits<std::int32_t>::min();
    constexpr std::int32_t high = std::numeric_limits<std::int32_t>::max();
    std::int32_t result = 0;
    Require(uprv_add32_overflow_nid_postfix(2, 3, &result) == 0 && result == 5, "addition");
    Require(uprv_add32_overflow_nid_postfix(high, 1, &result) == 1 && result == low, "addition overflow wraps");
    Require(uprv_add32_overflow_nid_postfix(low, -1, &result) == 1 && result == high, "addition underflow wraps");
    Require(uprv_add32_overflow_nid_postfix(high, low, &result) == 0 && result == -1, "addition of the extremes");
    Require(uprv_mul32_overflow_nid_postfix(-46340, 46340, &result) == 0 && result == -2147395600, "multiplication");
    Require(uprv_mul32_overflow_nid_postfix(65536, 65536, &result) == 1 && result == 0, "multiplication overflow wraps");
    Require(uprv_mul32_overflow_nid_postfix(-46341, 46341, &result) == 1 && result == 2147479015, "negative multiplication overflow wraps");
    Require(uprv_mul32_overflow_nid_postfix(low, -1, &result) == 1 && result == low, "negating the minimum overflows");
    Require(Rejects([] { uprv_add32_overflow_nid_postfix(1, 2, nullptr); }), "addition rejects a null result");
    Require(Rejects([] { uprv_mul32_overflow_nid_postfix(1, 2, nullptr); }), "multiplication rejects a null result");
}

void MaximumPointer() {
    constexpr std::uintptr_t top = std::numeric_limits<std::uintptr_t>::max();
    const auto limit = [](std::uintptr_t base) { return reinterpret_cast<std::uintptr_t>(uprv_maximumPtr_nid_postfix(reinterpret_cast<void*>(base))); };
    Require(limit(0) == 0x7fffffff && limit(0x1000) == 0x80000fff, "maximum pointer adds 2^31 - 1");
    Require(limit(top - 0x7fffffff) == top, "maximum pointer reaching the top");
    Require(limit(top - 0x7ffffffe) == top && limit(top) == top, "maximum pointer saturates instead of wrapping");
}

}

int main() {
    Classification();
    LibraryMath();
    Rounding();
    Extremes();
    Overflow();
    MaximumPointer();
    return 0;
}
