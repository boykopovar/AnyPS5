#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "nexttoward check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

static std::uint64_t Bits(double value) { return std::bit_cast<std::uint64_t>(value); }
static std::uint32_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }

static constexpr std::uint64_t DOUBLE_ONE = 0x3ff0000000000000ull;
static constexpr std::uint64_t DOUBLE_ABOVE_ONE = 0x3ff0000000000001ull;
static constexpr std::uint64_t DOUBLE_BELOW_ONE = 0x3fefffffffffffffull;
static constexpr std::uint64_t DOUBLE_TINY = 0x0000000000000001ull;
static constexpr std::uint64_t DOUBLE_SIGN = 0x8000000000000000ull;
static constexpr std::uint64_t DOUBLE_EXPONENT = 0x7ff0000000000000ull;

static void EqualStops() {
    Require(Bits(nexttoward_nid_postfix(1.0, 1.0L)) == DOUBLE_ONE);
    Require(Bits(nexttoward_nid_postfix(-1.0, -1.0L)) == (DOUBLE_SIGN | DOUBLE_ONE));
    Require(Bits(nexttoward_nid_postfix(0.0, 0.0L)) == 0);
    Require(Bits(nexttoward_nid_postfix(-0.0, 0.0L)) == 0);
    Require(Bits(nexttoward_nid_postfix(0.0, -0.0L)) == DOUBLE_SIGN);
}

static void Steps() {
    Require(Bits(nexttoward_nid_postfix(1.0, 2.0L)) == DOUBLE_ABOVE_ONE);
    Require(Bits(nexttoward_nid_postfix(1.0, 0.5L)) == DOUBLE_BELOW_ONE);
    Require(Bits(nexttoward_nid_postfix(-1.0, -2.0L)) == (DOUBLE_SIGN | DOUBLE_ABOVE_ONE));
    Require(Bits(nexttoward_nid_postfix(-1.0, -0.5L)) == (DOUBLE_SIGN | DOUBLE_BELOW_ONE));
    Require(Bits(nexttoward_nid_postfix(1.0, 2.0L)) == Bits(nextafter_nid_postfix(1.0, 2.0)));
    Require(Bits(nexttoward_nid_postfix(1.0, 0.5L)) == Bits(nextafter_nid_postfix(1.0, 0.5)));
}

static void ZeroTakesTheDirectionSign() {
    Require(Bits(nexttoward_nid_postfix(0.0, 1.0L)) == DOUBLE_TINY);
    Require(Bits(nexttoward_nid_postfix(0.0, -1.0L)) == (DOUBLE_SIGN | DOUBLE_TINY));
    Require(Bits(nexttoward_nid_postfix(-0.0, 1.0L)) == DOUBLE_TINY);
    Require(Bits(nexttoward_nid_postfix(-0.0, -1.0L)) == (DOUBLE_SIGN | DOUBLE_TINY));
}

static void BeyondDoublePrecision() {
    const volatile long double justAbove = 1.0L + 0x1p-63L;
    Require(static_cast<double>(justAbove) == 1.0);
    Require(Bits(nexttoward_nid_postfix(1.0, justAbove)) == DOUBLE_ABOVE_ONE);
    Require(Bits(nextafter_nid_postfix(1.0, static_cast<double>(justAbove))) == DOUBLE_ONE);
    const volatile long double justBelow = 1.0L - 0x1p-64L;
    Require(static_cast<double>(justBelow) == 1.0);
    Require(Bits(nexttoward_nid_postfix(1.0, justBelow)) == DOUBLE_BELOW_ONE);
}

static void SubnormalStartsAtItsHighWord() {
    const double tiny = std::bit_cast<double>(DOUBLE_TINY);
    Require(Bits(nexttoward_nid_postfix(tiny, 1.0L)) == 0);
    Require(Bits(nexttoward_nid_postfix(-tiny, -1.0L)) == (DOUBLE_SIGN | (DOUBLE_TINY + 1u)));
    Require(Bits(nexttowardf_nid_postfix(std::bit_cast<float>(0x00000001u), 1.0L)) == 0x00000002u);
}

static void Saturates() {
    Require(Bits(nexttoward_nid_postfix(std::numeric_limits<double>::max(), 1.0L / 0.0L)) == DOUBLE_EXPONENT);
    Require(Bits(nexttoward_nid_postfix(1.0, 1.0L / 0.0L)) == DOUBLE_ABOVE_ONE);
    Require(Bits(nexttoward_nid_postfix(1.0, -1.0L / 0.0L)) == DOUBLE_BELOW_ONE);
}

static void Quietens() {
    const double quiet = std::numeric_limits<double>::quiet_NaN();
    const long double wideQuiet = std::numeric_limits<long double>::quiet_NaN();
    Require(std::isnan(nexttoward_nid_postfix(quiet, 1.0L)));
    Require(std::isnan(nexttoward_nid_postfix(1.0, wideQuiet)));
    Require(std::isnan(nexttoward_nid_postfix(quiet, wideQuiet)));
    Require(std::isnan(nexttowardf_nid_postfix(std::numeric_limits<float>::quiet_NaN(), 1.0L)));
    Require(std::isnan(nexttowardf_nid_postfix(1.0f, wideQuiet)));
}

static void Narrow() {
    static constexpr std::uint32_t NARROW_ONE = 0x3f800000u;
    Require(Bits(nexttowardf_nid_postfix(1.0f, 2.0L)) == NARROW_ONE + 1);
    Require(Bits(nexttowardf_nid_postfix(1.0f, 0.5L)) == NARROW_ONE - 1);
    Require(Bits(nexttowardf_nid_postfix(-1.0f, -2.0L)) == 0x80000000u + NARROW_ONE + 1);
    Require(Bits(nexttowardf_nid_postfix(-1.0f, -0.5L)) == 0x80000000u + NARROW_ONE - 1);
    Require(Bits(nexttowardf_nid_postfix(0.0f, 1.0L)) == 1u);
    Require(Bits(nexttowardf_nid_postfix(0.0f, -1.0L)) == 0x80000001u);
    Require(Bits(nexttowardf_nid_postfix(1.0f, 1.0f)) == NARROW_ONE);
    const volatile long double justAbove = 1.0L + 0x1p-24L;
    Require(static_cast<float>(justAbove) == 1.0f);
    Require(Bits(nexttowardf_nid_postfix(1.0f, justAbove)) == NARROW_ONE + 1);
    Require(Bits(nexttowardf_nid_postfix(std::numeric_limits<float>::max(), 1.0L / 0.0L)) == 0x7f800000u);
}

int main() {
    EqualStops();
    Steps();
    ZeroTakesTheDirectionSign();
    BeyondDoublePrecision();
    SubnormalStartsAtItsHighWord();
    Saturates();
    Quietens();
    Narrow();
    std::puts("nexttoward and nexttowardf follow their long double direction");
}
