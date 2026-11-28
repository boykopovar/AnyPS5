#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include <bit>
#include <cstdint>

namespace {

constexpr std::uint64_t DoubleFractionMask = (std::uint64_t{1} << 52) - 1;
constexpr int DoubleInfiniteExponent = 0x7ff;
constexpr int HalfExponentOffset = 1023 - 15;
constexpr int HalfInfiniteExponent = 0x1f;
constexpr int FractionShift = 52 - 10;
constexpr std::uint16_t HalfInfinity = 0x7c00;
constexpr std::uint16_t HalfQuietNan = 0x7e00;
constexpr std::uint64_t HalfNanPayload = 0x1ff;

std::uint16_t RoundToNearestEven(std::uint64_t truncated, std::uint64_t remainder, std::uint64_t halfway) {
    if (remainder > halfway || (remainder == halfway && (truncated & 1) != 0)) ++truncated;
    return static_cast<std::uint16_t>(truncated);
}

}

extern "C" {

std::uint16_t APS5_VABI TruncateDoubleToHalf_nid_no_patch(double value) {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const auto sign = static_cast<std::uint16_t>((bits >> 48) & 0x8000);
    const auto exponent = static_cast<int>((bits >> 52) & DoubleInfiniteExponent);
    const std::uint64_t fraction = bits & DoubleFractionMask;
    if (exponent == DoubleInfiniteExponent) {
        if (fraction == 0) return sign | HalfInfinity;
        return sign | HalfQuietNan | static_cast<std::uint16_t>((fraction >> FractionShift) & HalfNanPayload);
    }
    const int halfExponent = exponent - HalfExponentOffset;
    if (halfExponent >= HalfInfiniteExponent) return sign | HalfInfinity;
    if (halfExponent >= 1) {
        const std::uint64_t truncated = (static_cast<std::uint64_t>(halfExponent) << 10) | (fraction >> FractionShift);
        const std::uint64_t remainder = fraction & ((std::uint64_t{1} << FractionShift) - 1);
        return sign | RoundToNearestEven(truncated, remainder, std::uint64_t{1} << (FractionShift - 1));
    }
    const int shift = FractionShift + 1 - halfExponent;
    if (shift >= 64) return sign;
    const std::uint64_t significand = fraction | (std::uint64_t{1} << 52);
    return sign | RoundToNearestEven(significand >> shift, significand & ((std::uint64_t{1} << shift) - 1), std::uint64_t{1} << (shift - 1));
}

}

#if defined(_WIN32) || defined(__APPLE__)
#define APS5_HALF_ASM_CALL(name) "    call " APS5_ASM_SYMBOL(name) "\n"
#else
#define APS5_HALF_ASM_CALL(name) "    call " name "@PLT\n"
#endif

asm(".text\n"
    APS5_ASM_FUNCTION("__truncdfhf2_nid_postfix")
    "    subq $8, %rsp\n"
    APS5_HALF_ASM_CALL("TruncateDoubleToHalf_nid_no_patch")
    "    addq $8, %rsp\n"
    "    movzwl %ax, %eax\n"
    "    movd %eax, %xmm0\n"
    "    ret\n"
    APS5_ASM_FUNCTION_END("__truncdfhf2_nid_postfix"));
