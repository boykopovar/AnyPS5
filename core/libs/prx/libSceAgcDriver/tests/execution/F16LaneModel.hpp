#pragma once

#include <algorithm>
#include <cstdint>

namespace F16LaneModel {

constexpr std::uint32_t QuietBit = 0x0200u;
constexpr std::uint32_t DefaultNan = 0xfe00u;

enum class HalfOperation { Add, Mul, Fma };

inline bool Nan(std::uint32_t half) {
    return (half & 0x7fffu) > 0x7c00u;
}

inline bool SignalingNan(std::uint32_t half) {
    return Nan(half) && (half & QuietBit) == 0u;
}

inline __int128 Scaled(std::uint32_t half) {
    const auto exponent = (half >> 10u) & 0x1fu;
    const auto mantissa = half & 0x3ffu;
    const __int128 magnitude = exponent == 0u ? static_cast<__int128>(mantissa) : static_cast<__int128>(mantissa | 0x400u) << (exponent - 1u);
    return (half & 0x8000u) != 0u ? -magnitude : magnitude;
}

inline std::uint32_t RoundedHalf(__int128 value, int scale, bool negativeZero) {
    if (value == 0) return negativeZero ? 0x8000u : 0u;
    const std::uint32_t sign = value < 0 ? 0x8000u : 0u;
    auto magnitude = static_cast<unsigned __int128>(value < 0 ? -value : value);
    int top = 127;
    while (((magnitude >> top) & 1u) == 0u) --top;
    const int exponent = top + scale;
    const int quantum = std::max(exponent, -14) - 10;
    const int shift = quantum - scale;
    unsigned __int128 kept = magnitude;
    int keptQuantum = scale;
    if (shift > 0) {
        kept = magnitude >> shift;
        const auto remainder = magnitude & ((static_cast<unsigned __int128>(1) << shift) - 1u);
        const auto half = static_cast<unsigned __int128>(1) << (shift - 1);
        if (remainder > half || (remainder == half && (kept & 1u) != 0u)) ++kept;
        keptQuantum = quantum;
    } else {
        kept = magnitude << -shift;
        keptQuantum = quantum;
    }
    if (kept == 2048u) {
        kept = 1024u;
        ++keptQuantum;
    }
    if (kept < 1024u) return sign | static_cast<std::uint32_t>(kept);
    const int biased = keptQuantum + 25;
    if (biased >= 31) return sign | 0x7c00u;
    return sign | (static_cast<std::uint32_t>(biased) << 10u) | static_cast<std::uint32_t>(kept - 1024u);
}

inline bool Infinite(std::uint32_t half) {
    return (half & 0x7fffu) == 0x7c00u;
}

inline bool Zero(std::uint32_t half) {
    return (half & 0x7fffu) == 0u;
}

inline std::uint32_t Arithmetic(HalfOperation operation, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool ieee) {
    const auto quiet = [&](std::uint32_t half) { return ieee ? half | QuietBit : half; };
    const bool invalidProduct = (Infinite(a) && Zero(b)) || (Infinite(b) && Zero(a));
    if (Nan(a)) return quiet(a);
    if (Nan(b)) return quiet(b);
    if (operation == HalfOperation::Fma && Nan(c) && !invalidProduct) return quiet(c);
    if (operation == HalfOperation::Fma && Nan(c)) return DefaultNan;
    const bool productNegative = ((a ^ b) & 0x8000u) != 0u;
    if (operation == HalfOperation::Add) {
        if (Infinite(a) && Infinite(b)) return (a == b) ? a : DefaultNan;
        if (Infinite(a)) return a;
        if (Infinite(b)) return b;
        return RoundedHalf(Scaled(a) + Scaled(b), -24, (a & b & 0x8000u) != 0u);
    }
    if (invalidProduct) return DefaultNan;
    if (operation == HalfOperation::Mul) {
        if (Infinite(a) || Infinite(b)) return (productNegative ? 0x8000u : 0u) | 0x7c00u;
        return RoundedHalf(Scaled(a) * Scaled(b), -48, productNegative);
    }
    if (Infinite(a) || Infinite(b)) {
        const std::uint32_t product = (productNegative ? 0x8000u : 0u) | 0x7c00u;
        if (Infinite(c) && c != product) return DefaultNan;
        return product;
    }
    if (Infinite(c)) return c;
    const __int128 sum = Scaled(a) * Scaled(b) + (Scaled(c) << 24);
    return RoundedHalf(sum, -48, productNegative && (c & 0x8000u) != 0u);
}

inline std::int32_t OrderKey(std::uint32_t half) {
    const auto value = static_cast<std::int32_t>(static_cast<std::int16_t>(half));
    return value ^ ((value >> 31) & 0x7fff);
}

inline std::uint32_t MinMax(bool maximum, std::uint32_t a, std::uint32_t b, bool ieee) {
    std::uint32_t lane = maximum ? (OrderKey(a) >= OrderKey(b) ? a : b) : (OrderKey(a) < OrderKey(b) ? a : b);
    if (Nan(a)) lane = b;
    if (Nan(b)) lane = a;
    if (!ieee) return lane;
    if (SignalingNan(b)) lane = b | QuietBit;
    if (SignalingNan(a)) lane = a | QuietBit;
    return lane;
}

}
