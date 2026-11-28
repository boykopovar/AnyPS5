#include "prx/libSceLibcInternal/include/MathInternals.hpp"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal logpoly: %s\n", message);
    std::abort();
}

struct DoubleCase {
    std::uint64_t input;
    std::uint64_t expected;
};

struct FloatCase {
    std::uint32_t input;
    std::uint32_t expected;
};

struct ExtendedCase {
    std::uint64_t inputMantissa;
    std::uint16_t inputExponent;
    std::uint64_t expectedMantissa;
    std::uint16_t expectedExponent;
};

long double Extended(std::uint64_t mantissa, std::uint16_t exponent) {
    long double value = 0;
    std::memcpy(&value, &mantissa, sizeof(mantissa));
    std::memcpy(reinterpret_cast<unsigned char*>(&value) + sizeof(mantissa), &exponent, sizeof(exponent));
    return value;
}

bool SameExtended(long double value, std::uint64_t mantissa, std::uint16_t exponent) {
    std::uint64_t actualMantissa = 0;
    std::uint16_t actualExponent = 0;
    std::memcpy(&actualMantissa, &value, sizeof(actualMantissa));
    std::memcpy(&actualExponent, reinterpret_cast<const unsigned char*>(&value) + sizeof(actualMantissa), sizeof(actualExponent));
    return actualMantissa == mantissa && actualExponent == exponent;
}

void TestDouble() {
    static constexpr DoubleCase Cases[] = {
        {0x0000000000000000ull, 0x3fe55555555555bcull}, {0x8000000000000000ull, 0x3fe55555555555bcull},
        {0x3f50000000000000ull, 0x3fe558891aee24e4ull}, {0x3f88000000000000ull, 0x3fe57c0ec6bc4c6dull},
        {0x3fa921fb54442d18ull, 0x3fe5fc0c07e51327ull}, {0x3fd5555555555555ull, 0x3feafa4e03049335ull},
        {0x3ff0000000000000ull, 0x4000764f9f13e118ull}, {0xbfe8000000000000ull, 0x3fdecc2bb29c9d9bull},
        {0x3fcfffffe0000000ull, 0x3fe93e9a7e408a2eull}, {0x40223456789abcdeull, 0x40f7446791399e5cull},
        {0xbfb6a09e667f3bcdull, 0x3fe444d0d5e7bf64ull}, {0x3eb0000000000001ull, 0x3fe5555622222badull},
        {0x7ff0000000000000ull, 0x7ff0000000000000ull}, {0xfff0000000000000ull, 0x7ff0000000000000ull},
        {0x7ff8000000000000ull, 0x7ff8000000000000ull}, {0xfff8000000000000ull, 0xfff8000000000000ull},
        {0x7ff8000000001234ull, 0x7ff8000000001234ull}, {0xfff80000deadbeefull, 0xfff80000deadbeefull},
        {0x7ff0000000001234ull, 0x7ff8000000001234ull}, {0xfff4000000000001ull, 0xfffc000000000001ull}};
    for (const auto& item : Cases) {
        Require(std::bit_cast<std::uint64_t>(_Logpoly_nid_postfix(std::bit_cast<double>(item.input))) == item.expected, "_Logpoly result bits");
    }
}

void TestFloat() {
    static constexpr FloatCase Cases[] = {
        {0x00000000u, 0x3f2aaac0u}, {0x80000000u, 0x3f2aaac0u}, {0x3a800000u, 0x3f2ac45bu}, {0x3c400000u, 0x3f2be076u},
        {0x3d490fdbu, 0x3f2fdffcu}, {0x3eaaaaabu, 0x3f5547e1u}, {0x3f800000u, 0x3faec16au}, {0xbf400000u, 0x3f08f210u},
        {0x3e7fffffu, 0x3f490893u}, {0x4111a2b4u, 0x41e884aeu}, {0xbdb504f3u, 0x3f223812u}, {0x35800000u, 0x3f2aaac6u},
        {0x7f800000u, 0x7f800000u}, {0xff800000u, 0x7f800000u}, {0x7fc00000u, 0x7fc00000u}, {0xffc00000u, 0xffc00000u},
        {0x7fc01234u, 0x7fc01234u}, {0xffc0beefu, 0xffc0beefu}, {0x7f801234u, 0x7fc01234u}, {0xffa00001u, 0xffe00001u}};
    for (const auto& item : Cases) {
        Require(std::bit_cast<std::uint32_t>(_FLogpoly_nid_postfix(std::bit_cast<float>(item.input))) == item.expected, "_FLogpoly result bits");
    }
}

void TestExtended() {
    static constexpr ExtendedCase Cases[] = {
        {0x0000000000000000ull, 0x0000, 0xaaaaaaaaaaaaa334ull, 0x3ffe}, {0x0000000000000000ull, 0x8000, 0xaaaaaaaaaaaaa334ull, 0x3ffe},
        {0x8000000000000000ull, 0x3ff5, 0xaac448d77125a22full, 0x3ffe}, {0xc000000000000000ull, 0x3ff8, 0xabe07635e2636014ull, 0x3ffe},
        {0xc90fdaa22168c000ull, 0x3ffa, 0xafe0603f2c7adecaull, 0x3ffe}, {0xaaaaaaaaaaaaa800ull, 0x3ffd, 0xd7d530ead57df60full, 0x3ffe},
        {0x8000000000000000ull, 0x3fff, 0x8b3bde9abd7a37d0ull, 0x4000}, {0xc000000000000000ull, 0xbffe, 0xebc673f15ad0653bull, 0x3ffd},
        {0xffffff0000000000ull, 0x3ffc, 0xc9f526ae1e7b9a34ull, 0x3ffe}, {0x91a2b3c4d5e6f000ull, 0x4002, 0xbce1b52d1f226150ull, 0x4012},
        {0xb504f333f9de6800ull, 0xbffb, 0xa226865f4f72dd64ull, 0x3ffe}, {0x8000000000000800ull, 0x3feb, 0xaaaab111115a2e32ull, 0x3ffe},
        {0x8000000000000000ull, 0x7fff, 0x8000000000000000ull, 0x7fff}, {0x8000000000000000ull, 0xffff, 0x8000000000000000ull, 0xffff},
        {0xc000000000000000ull, 0x7fff, 0xc000000000000000ull, 0x7fff}, {0xc000000000000000ull, 0xffff, 0xc000000000000000ull, 0xffff},
        {0xc000000000001234ull, 0x7fff, 0xc000000000001234ull, 0x7fff}, {0xc0000000deadbeefull, 0xffff, 0xc0000000deadbeefull, 0xffff},
        {0x8000000000001234ull, 0x7fff, 0xc000000000001234ull, 0x7fff}, {0xa000000000000001ull, 0xffff, 0xe000000000000001ull, 0xffff}};
    for (const auto& item : Cases) {
        Require(SameExtended(_LLogpoly_nid_postfix(Extended(item.inputMantissa, item.inputExponent)), item.expectedMantissa, item.expectedExponent),
                "_LLogpoly result bits");
    }
}

template <typename Call>
auto AtMxcsrRounding(std::uint32_t rounding, Call call) {
    std::uint32_t saved = 0;
    asm volatile("stmxcsr %0" : "=m"(saved));
    const std::uint32_t control = (saved & ~0x6000u) | (rounding << 13);
    asm volatile("ldmxcsr %0" : : "m"(control));
    const auto result = call();
    asm volatile("ldmxcsr %0" : : "m"(saved));
    return result;
}

template <typename Call>
long double AtX87Control(std::uint16_t precision, std::uint16_t rounding, Call call) {
    std::uint16_t saved = 0;
    asm volatile("fnstcw %0" : "=m"(saved));
    const std::uint16_t control = static_cast<std::uint16_t>((saved & ~0x0f00) | precision | (rounding << 10));
    asm volatile("fldcw %0" : : "m"(control));
    const long double result = call();
    asm volatile("fldcw %0" : : "m"(saved));
    return result;
}

void TestRoundingAndPrecision() {
    struct DoubleMode {
        std::uint32_t rounding;
        DoubleCase item;
    };
    struct FloatMode {
        std::uint32_t rounding;
        FloatCase item;
    };
    struct ExtendedMode {
        std::uint16_t precision;
        std::uint16_t rounding;
        ExtendedCase item;
    };
    static constexpr DoubleMode DoubleModes[] = {
        {1, {0x3ff0000000000000ull, 0x4000764f9f13e118ull}}, {1, {0xbfb6a09e667f3bcdull, 0x3fe444d0d5e7bf63ull}},
        {2, {0x3ff0000000000000ull, 0x4000764f9f13e119ull}}, {2, {0xbfb6a09e667f3bcdull, 0x3fe444d0d5e7bf64ull}},
        {3, {0x3ff0000000000000ull, 0x4000764f9f13e118ull}}, {3, {0xbfb6a09e667f3bcdull, 0x3fe444d0d5e7bf63ull}}};
    static constexpr FloatMode FloatModes[] = {{1, {0x3f800000u, 0x3faec169u}}, {1, {0xbdb504f3u, 0x3f223811u}}, {2, {0x3f800000u, 0x3faec16au}},
                                               {2, {0xbdb504f3u, 0x3f223812u}}, {3, {0x3f800000u, 0x3faec169u}}, {3, {0xbdb504f3u, 0x3f223811u}}};
    static constexpr ExtendedMode ExtendedModes[] = {
        {0x000, 0, {0x8000000000000000ull, 0x3fff, 0x8b3bdf0000000000ull, 0x4000}}, {0x000, 0, {0xb504f333f9de6800ull, 0xbffb, 0xa226860000000000ull, 0x3ffe}},
        {0x200, 0, {0x8000000000000000ull, 0x3fff, 0x8b3bde9abd7a3800ull, 0x4000}}, {0x200, 0, {0xb504f333f9de6800ull, 0xbffb, 0xa226865f4f72e000ull, 0x3ffe}},
        {0x300, 1, {0x8000000000000000ull, 0x3fff, 0x8b3bde9abd7a37cfull, 0x4000}}, {0x300, 1, {0xb504f333f9de6800ull, 0xbffb, 0xa226865f4f72dd64ull, 0x3ffe}},
        {0x300, 2, {0x8000000000000000ull, 0x3fff, 0x8b3bde9abd7a37d1ull, 0x4000}}, {0x300, 2, {0xb504f333f9de6800ull, 0xbffb, 0xa226865f4f72dd65ull, 0x3ffe}},
        {0x300, 3, {0x8000000000000000ull, 0x3fff, 0x8b3bde9abd7a37cfull, 0x4000}}, {0x300, 3, {0xb504f333f9de6800ull, 0xbffb, 0xa226865f4f72dd64ull, 0x3ffe}}};
    for (const auto& mode : DoubleModes) {
        const double result = AtMxcsrRounding(mode.rounding, [&] { return _Logpoly_nid_postfix(std::bit_cast<double>(mode.item.input)); });
        Require(std::bit_cast<std::uint64_t>(result) == mode.item.expected, "_Logpoly result bits in a directed rounding mode");
    }
    for (const auto& mode : FloatModes) {
        const float result = AtMxcsrRounding(mode.rounding, [&] { return _FLogpoly_nid_postfix(std::bit_cast<float>(mode.item.input)); });
        Require(std::bit_cast<std::uint32_t>(result) == mode.item.expected, "_FLogpoly result bits in a directed rounding mode");
    }
    for (const auto& mode : ExtendedModes) {
        const long double input = Extended(mode.item.inputMantissa, mode.item.inputExponent);
        const long double result = AtX87Control(mode.precision, mode.rounding, [&] { return _LLogpoly_nid_postfix(input); });
        Require(SameExtended(result, mode.item.expectedMantissa, mode.item.expectedExponent), "_LLogpoly result bits under an x87 control word");
    }
}

}

int main() {
    TestDouble();
    TestFloat();
    TestExtended();
    TestRoundingAndPrecision();
    return 0;
}
