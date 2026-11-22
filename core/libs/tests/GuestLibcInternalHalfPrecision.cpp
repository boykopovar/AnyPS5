#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <random>
#include <source_location>
#include <vector>

extern "C" std::uint32_t APS5_VABI TruncateThroughXmm_nid_no_patch(double value);

#if defined(_WIN32) || defined(__APPLE__)
#define APS5_TEST_ASM_CALL(name) "    call " APS5_ASM_SYMBOL(name) "\n"
#else
#define APS5_TEST_ASM_CALL(name) "    call " name "@PLT\n"
#endif

asm(".text\n"
    APS5_ASM_FUNCTION("TruncateThroughXmm_nid_no_patch")
    "    subq $8, %rsp\n"
    APS5_TEST_ASM_CALL("__truncdfhf2_nid_postfix")
    "    addq $8, %rsp\n"
    "    movd %xmm0, %eax\n"
    "    ret\n"
    APS5_ASM_FUNCTION_END("TruncateThroughXmm_nid_no_patch"));

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "half precision check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::uint16_t Infinity = 0x7c00;
constexpr std::uint16_t LargestFinite = 0x7bff;

double HalfMagnitude(unsigned half) {
    const int exponent = static_cast<int>(half >> 10);
    const int fraction = static_cast<int>(half & 0x3ff);
    if (exponent == 0) return std::ldexp(fraction, -24);
    return std::ldexp(1024 + fraction, exponent - 25);
}

double HalfValue(std::uint16_t half) {
    const unsigned magnitude = half & 0x7fff;
    const double value = magnitude == Infinity ? std::numeric_limits<double>::infinity() : HalfMagnitude(magnitude);
    return (half & 0x8000) != 0 ? -value : value;
}

double DoubleFromBits(std::uint64_t bits) {
    return std::bit_cast<double>(bits);
}

double Nan(bool negative, std::uint64_t fraction) {
    return DoubleFromBits((negative ? std::uint64_t{1} << 63 : 0) | (std::uint64_t{0x7ff} << 52) | fraction);
}

class Reference {
public:
    Reference() : magnitudes(LargestFinite + 1) {
        for (unsigned half = 0; half <= LargestFinite; ++half) magnitudes[half] = HalfMagnitude(half);
    }

    std::uint16_t operator()(double value) const {
        const auto bits = std::bit_cast<std::uint64_t>(value);
        const std::uint16_t sign = (bits >> 63) != 0 ? 0x8000 : 0;
        if (std::isnan(value)) return sign | 0x7e00 | static_cast<std::uint16_t>((bits >> 42) & 0x1ff);
        const double magnitude = std::fabs(value);
        if (magnitude >= 65520.0) return sign | Infinity;
        const auto above = std::upper_bound(magnitudes.begin(), magnitudes.end(), magnitude);
        const auto low = static_cast<std::uint16_t>(above - magnitudes.begin() - 1);
        if (magnitudes[low] == magnitude) return sign | low;
        const double high = low == LargestFinite ? 65536.0 : magnitudes[low + 1];
        const double middle = (magnitudes[low] + high) / 2;
        if (magnitude < middle) return sign | low;
        if (magnitude > middle) return sign | static_cast<std::uint16_t>(low + 1);
        return sign | static_cast<std::uint16_t>((low & 1) != 0 ? low + 1 : low);
    }

private:
    std::vector<double> magnitudes;
};

void Check(double value, std::uint16_t expected, std::source_location location = std::source_location::current()) {
    Require(__truncdfhf2_nid_postfix(value) == expected, location);
    Require(TruncateThroughXmm_nid_no_patch(value) == expected, location);
}

void TestEveryHalf(const Reference& reference) {
    for (unsigned code = 0; code <= 0xffff; ++code) {
        const auto half = static_cast<std::uint16_t>(code);
        const bool nan = (half & Infinity) == Infinity && (half & 0x3ff) != 0;
        const double value = nan ? Nan((half & 0x8000) != 0, static_cast<std::uint64_t>(half & 0x3ff) << 42) : HalfValue(half);
        const auto expected = static_cast<std::uint16_t>(nan ? half | 0x200 : half);
        Require(reference(value) == expected);
        Check(value, expected);
    }
}

void TestMidpoints() {
    for (unsigned low = 0; low < LargestFinite; ++low) {
        const double middle = (HalfMagnitude(low) + HalfMagnitude(low + 1)) / 2;
        const auto even = static_cast<std::uint16_t>((low & 1) != 0 ? low + 1 : low);
        for (const std::uint16_t sign : {std::uint16_t{0}, std::uint16_t{0x8000}}) {
            const double direction = sign != 0 ? -1.0 : 1.0;
            Check(direction * middle, sign | even);
            Check(direction * std::nextafter(middle, 0.0), sign | static_cast<std::uint16_t>(low));
            Check(direction * std::nextafter(middle, 1.0e9), sign | static_cast<std::uint16_t>(low + 1));
        }
    }
    Check(65504.0, LargestFinite);
    Check(std::nextafter(65520.0, 0.0), LargestFinite);
    Check(65520.0, Infinity);
    Check(-65520.0, 0xfc00);
}

void TestSpecialValues() {
    Check(0.0, 0x0000);
    Check(-0.0, 0x8000);
    Check(std::numeric_limits<double>::denorm_min(), 0x0000);
    Check(-std::numeric_limits<double>::denorm_min(), 0x8000);
    Check(std::numeric_limits<double>::min(), 0x0000);
    Check(std::ldexp(1.0, -24), 0x0001);
    Check(std::ldexp(1.0, -25), 0x0000);
    Check(std::nextafter(std::ldexp(1.0, -25), 1.0), 0x0001);
    Check(std::ldexp(1.0, -14), 0x0400);
    Check(std::nextafter(std::ldexp(1.0, -14), 0.0), 0x0400);
    Check(1.0, 0x3c00);
    Check(-2.0, 0xc000);
    Check(0.1, 0x2e66);
    Check(1.0 / 3.0, 0x3555);
    Check(std::numeric_limits<double>::max(), Infinity);
    Check(-std::numeric_limits<double>::max(), 0xfc00);
    Check(std::numeric_limits<double>::infinity(), Infinity);
    Check(-std::numeric_limits<double>::infinity(), 0xfc00);
    Check(DoubleFromBits(0x7ff8000000000000), 0x7e00);
    Check(DoubleFromBits(0xfff8000000000000), 0xfe00);
    Check(DoubleFromBits(0x7ff0000000000001), 0x7e00);
    Check(DoubleFromBits(0x7ff7fc0000000000), 0x7fff);
    Check(DoubleFromBits(0x7fffffffffffffff), 0x7fff);
    Check(DoubleFromBits(0xfff0040000000000), 0xfe01);
}

void TestRandomValues(const Reference& reference) {
    std::mt19937_64 generator(0x7f16);
    for (int round = 0; round < 1000000; ++round) {
        const std::uint64_t bits = generator();
        const double any = DoubleFromBits(bits);
        Check(any, reference(any));
        const std::uint64_t exponent = 993 + generator() % 48;
        const double near = DoubleFromBits((bits & 0x800fffffffffffff) | (exponent << 52));
        Check(near, reference(near));
    }
}

void TestNoFloatingPointExceptions() {
    const double inputs[] = {
        0.1, 1.0 / 3.0, 65519.0, 65520.0, 1.0e300, -1.0e300, std::ldexp(1.0, -25), std::ldexp(3.0, -26),
        1.0e-300, std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::infinity(),
        DoubleFromBits(0x7ff0000000000001), DoubleFromBits(0x7ff8000000000000),
    };
    std::uint16_t results[std::size(inputs)];
    std::feclearexcept(FE_ALL_EXCEPT);
    for (std::size_t index = 0; index < std::size(inputs); ++index) results[index] = __truncdfhf2_nid_postfix(inputs[index]);
    Require(std::fetestexcept(FE_ALL_EXCEPT) == 0);
    Require(results[0] == 0x2e66 && results[3] == Infinity && results[6] == 0 && results[11] == 0x7e00);
}

}

int main() {
    const Reference reference;
    TestEveryHalf(reference);
    TestMidpoints();
    TestSpecialValues();
    TestRandomValues(reference);
    TestNoFloatingPointExceptions();
    std::puts("libSceLibcInternal half precision tests passed");
}
