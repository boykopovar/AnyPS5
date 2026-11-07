#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "nextafterl check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

static constexpr std::uint16_t POSITIVE_ONE = 0x3fff;
static constexpr std::uint16_t NEGATIVE_ONE = 0xbfff;
static constexpr std::uint16_t SIGN = 0x8000;
static constexpr std::uint64_t INTEGER_BIT = 0x8000000000000000ull;
static constexpr std::uint64_t BELOW_ONE = 0xffffffffffffffffull;

struct Shape {
    std::uint64_t mantissa;
    std::uint16_t signExponent;
};

static Shape Split(long double value) {
    Shape shape{};
    std::memcpy(&shape.mantissa, &value, sizeof(shape.mantissa));
    std::memcpy(&shape.signExponent, reinterpret_cast<const unsigned char*>(&value) + 8, sizeof(shape.signExponent));
    return shape;
}

static long double Join(Shape shape) {
    long double value = 0.0L;
    std::memcpy(&value, &shape.mantissa, sizeof(shape.mantissa));
    std::memcpy(reinterpret_cast<unsigned char*>(&value) + 8, &shape.signExponent, sizeof(shape.signExponent));
    return value;
}

static void Same(long double actual, Shape expected, int line) {
    const Shape shape = Split(actual);
    if (shape.mantissa == expected.mantissa && shape.signExponent == expected.signExponent) return;
    std::fprintf(stderr, "nextafterl shape mismatch at line %d: got %016llx:%04x, wanted %016llx:%04x\n",
                 line, static_cast<unsigned long long>(shape.mantissa), shape.signExponent,
                 static_cast<unsigned long long>(expected.mantissa), expected.signExponent);
    std::abort();
}
#define RequireShape(actual, mantissa, signExponent) Same((actual), Shape{(mantissa), (signExponent)}, __LINE__)

static void EqualStops() {
    RequireShape(nextafterl_nid_postfix(1.0L, 1.0L), INTEGER_BIT, POSITIVE_ONE);
    RequireShape(nextafterl_nid_postfix(-1.0L, -1.0L), INTEGER_BIT, NEGATIVE_ONE);
    RequireShape(nextafterl_nid_postfix(0.0L, -0.0L), 0, SIGN);
    RequireShape(nextafterl_nid_postfix(-0.0L, 0.0L), 0, 0);
}

static void Steps() {
    RequireShape(nextafterl_nid_postfix(1.0L, 2.0L), INTEGER_BIT + 1u, POSITIVE_ONE);
    RequireShape(nextafterl_nid_postfix(1.0L, 0.5L), BELOW_ONE, POSITIVE_ONE - 1);
    RequireShape(nextafterl_nid_postfix(-1.0L, -2.0L), INTEGER_BIT + 1u, NEGATIVE_ONE);
    RequireShape(nextafterl_nid_postfix(-1.0L, -0.5L), BELOW_ONE, NEGATIVE_ONE - 1);
}

static void ZeroTakesTheDirectionSign() {
    RequireShape(nextafterl_nid_postfix(0.0L, 1.0L), 1u, 0);
    RequireShape(nextafterl_nid_postfix(0.0L, -1.0L), 1u, SIGN);
    RequireShape(nextafterl_nid_postfix(-0.0L, 1.0L), 1u, 0);
    RequireShape(nextafterl_nid_postfix(-0.0L, -1.0L), 1u, SIGN);
}

static void CrossesIntoTheSubnormals() {
    const long double smallestNormal = Join(Shape{INTEGER_BIT, 1});
    RequireShape(nextafterl_nid_postfix(smallestNormal, 0.0L), INTEGER_BIT - 1u, 0);
    RequireShape(nextafterl_nid_postfix(smallestNormal, -1.0L), INTEGER_BIT - 1u, 0);
    const long double largestSubnormal = Join(Shape{INTEGER_BIT - 1u, 0});
    RequireShape(nextafterl_nid_postfix(largestSubnormal, 1.0L), INTEGER_BIT, 1);
    RequireShape(nextafterl_nid_postfix(largestSubnormal, 0.0L), INTEGER_BIT - 2u, 0);
    const long double smallestSubnormal = Join(Shape{1u, 0});
    RequireShape(nextafterl_nid_postfix(smallestSubnormal, 0.0L), 0, 0);
    RequireShape(nextafterl_nid_postfix(-smallestSubnormal, 0.0L), 0, SIGN);
}

static void Saturates() {
    RequireShape(nextafterl_nid_postfix(std::numeric_limits<long double>::max(), 1.0L / 0.0L), INTEGER_BIT, 0x7fff);
    RequireShape(nextafterl_nid_postfix(1.0L, 1.0L / 0.0L), INTEGER_BIT + 1u, POSITIVE_ONE);
    RequireShape(nextafterl_nid_postfix(1.0L, -1.0L / 0.0L), BELOW_ONE, POSITIVE_ONE - 1);
}

static void Quietens() {
    const long double quiet = std::numeric_limits<long double>::quiet_NaN();
    Require(std::isnan(nextafterl_nid_postfix(quiet, 1.0L)));
    Require(std::isnan(nextafterl_nid_postfix(1.0L, quiet)));
    Require(std::isnan(nextafterl_nid_postfix(quiet, quiet)));
}

static void MatchesTheHost() {
    const long double values[] = {1.0L, -1.0L, 0.5L, -0.5L, 3.0L, 1e300L, -1e300L,
                                  std::numeric_limits<long double>::min(),
                                  std::numeric_limits<long double>::denorm_min(),
                                  -std::numeric_limits<long double>::denorm_min(),
                                  std::numeric_limits<long double>::max(),
                                  -std::numeric_limits<long double>::max()};
    const long double targets[] = {0.0L, 1.0L, -1.0L, 2.0L, -2.0L, 1e300L, -1e300L,
                                   1.0L / 0.0L, -1.0L / 0.0L};
    for (const long double x : values) {
        for (const long double y : targets) {
            const Shape mine = Split(nextafterl_nid_postfix(x, y));
            const Shape host = Split(std::nextafter(x, y));
            Require(mine.mantissa == host.mantissa && mine.signExponent == host.signExponent);
        }
    }
}

static void AliasAgrees() {
    const long double targets[] = {0.0L, 2.0L, -2.0L, 1.0L / 0.0L};
    for (const long double y : targets) {
        const Shape stepped = Split(nextafterl_nid_postfix(1.0L, y));
        const Shape aliased = Split(nexttowardl_nid_postfix(1.0L, y));
        Require(stepped.mantissa == aliased.mantissa && stepped.signExponent == aliased.signExponent);
    }
    Require(std::isnan(nexttowardl_nid_postfix(std::numeric_limits<long double>::quiet_NaN(), 1.0L)));
}

int main() {
    Require(sizeof(long double) == 16);
    Require(std::numeric_limits<long double>::digits == 64);
    EqualStops();
    Steps();
    ZeroTakesTheDirectionSign();
    CrossesIntoTheSubnormals();
    Saturates();
    Quietens();
    MatchesTheHost();
    AliasAgrees();
    std::puts("nextafterl and nexttowardl step the 80-bit format");
}
