#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Capacity = 1u << 18;
alignas(256) std::array<std::uint32_t, Capacity * 4u> Input{};
alignas(256) std::array<std::uint32_t, Capacity> Output{};

struct Case {
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
    std::uint32_t expected;
};

class Exact {
public:
    void Add(std::uint64_t magnitude, std::uint32_t shift, bool negative) {
        std::array<std::uint32_t, Limbs> term{};
        for (std::uint32_t bit = 0; bit < 64u; ++bit) {
            if (((magnitude >> bit) & 1u) == 0u) continue;
            const std::uint32_t position = bit + shift;
            term[position / 32u] |= 1u << (position % 32u);
        }
        if (negative) {
            for (auto& limb : term) limb = ~limb;
            Accumulate(term, 1u);
        } else {
            Accumulate(term, 0u);
        }
    }

    bool Zero() const { return std::all_of(limbs.begin(), limbs.end(), [](std::uint32_t limb) { return limb == 0u; }); }

    std::optional<std::uint32_t> Rounded() const {
        auto magnitude = limbs;
        const bool negative = (magnitude[Limbs - 1u] >> 31u) != 0u;
        if (negative) {
            for (auto& limb : magnitude) limb = ~limb;
            std::uint64_t carry = 1u;
            for (auto& limb : magnitude) {
                const std::uint64_t sum = std::uint64_t{limb} + carry;
                limb = static_cast<std::uint32_t>(sum);
                carry = sum >> 32u;
            }
        }
        int top = -1;
        for (int bit = static_cast<int>(Limbs * 32u) - 1; bit >= 0; --bit) {
            if (Bit(magnitude, static_cast<std::uint32_t>(bit))) {
                top = bit;
                break;
            }
        }
        if (top < 23) return std::nullopt;
        std::uint64_t mantissa = 0;
        for (int bit = top; bit > top - 24; --bit) mantissa = (mantissa << 1u) | (Bit(magnitude, static_cast<std::uint32_t>(bit)) ? 1u : 0u);
        const int guardBit = top - 24;
        const bool guard = guardBit >= 0 && Bit(magnitude, static_cast<std::uint32_t>(guardBit));
        bool sticky = false;
        for (int bit = guardBit - 1; bit >= 0 && !sticky; --bit) sticky = Bit(magnitude, static_cast<std::uint32_t>(bit));
        int exponent = top - 22;
        if (guard && (sticky || (mantissa & 1u) != 0u)) {
            if (++mantissa == (1u << 24u)) {
                mantissa >>= 1u;
                ++exponent;
            }
        }
        if (exponent < 1 || exponent > 254) return std::nullopt;
        return (negative ? 0x80000000u : 0u) | (static_cast<std::uint32_t>(exponent) << 23u) | static_cast<std::uint32_t>(mantissa & 0x7fffffu);
    }

private:
    static constexpr std::uint32_t Limbs = 10;

    static bool Bit(const std::array<std::uint32_t, Limbs>& value, std::uint32_t bit) { return ((value[bit / 32u] >> (bit % 32u)) & 1u) != 0u; }

    void Accumulate(const std::array<std::uint32_t, Limbs>& term, std::uint64_t carry) {
        for (std::uint32_t index = 0; index < Limbs; ++index) {
            const std::uint64_t sum = std::uint64_t{limbs[index]} + term[index] + carry;
            limbs[index] = static_cast<std::uint32_t>(sum);
            carry = sum >> 32u;
        }
    }

    std::array<std::uint32_t, Limbs> limbs{};
};

struct Half {
    std::uint32_t bits;
    bool nan;
    bool infinite;
    bool zero;
    bool negative;
    std::uint64_t significand;
    std::uint32_t scale;
};

Half Decode(std::uint32_t packed, std::uint32_t shift) {
    const std::uint32_t bits = (packed >> shift) & 0xffffu;
    const std::uint32_t magnitude = bits & 0x7fffu;
    const std::uint32_t exponent = magnitude >> 10u;
    const std::uint32_t fraction = magnitude & 0x3ffu;
    Half half{bits, magnitude > 0x7c00u, magnitude == 0x7c00u, magnitude == 0u, (bits & 0x8000u) != 0u, 0u, 0u};
    half.significand = exponent == 0u ? fraction : (fraction | 0x400u);
    half.scale = exponent == 0u ? 0u : exponent - 1u;
    return half;
}

std::optional<std::uint32_t> Dot2(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const Half aLow = Decode(a, 0u);
    const Half bLow = Decode(b, 0u);
    const Half aHigh = Decode(a, 16u);
    const Half bHigh = Decode(b, 16u);
    const auto quiet = [](const Half& half) { return (half.negative ? 0x80000000u : 0u) | 0x7fc00000u | ((half.bits & 0x3ffu) << 13u); };
    for (const Half* half : {&aLow, &bLow, &aHigh, &bHigh}) {
        if (half->nan) return quiet(*half);
    }
    const std::uint32_t magnitudeC = c & 0x7fffffffu;
    if (magnitudeC > 0x7f800000u) return c | 0x00400000u;
    const bool negativeC = (c & 0x80000000u) != 0u;
    const bool infiniteC = magnitudeC == 0x7f800000u;
    const bool signLow = aLow.negative != bLow.negative;
    const bool signHigh = aHigh.negative != bHigh.negative;
    const auto invalidProduct = [](const Half& lhs, const Half& rhs) { return (lhs.infinite && rhs.zero) || (rhs.infinite && lhs.zero); };
    const auto infiniteProduct = [](const Half& lhs, const Half& rhs) { return (lhs.infinite && !rhs.zero) || (rhs.infinite && !lhs.zero); };
    const bool infiniteLow = infiniteProduct(aLow, bLow);
    const bool infiniteHigh = infiniteProduct(aHigh, bHigh);
    const bool invalid = invalidProduct(aLow, bLow) || invalidProduct(aHigh, bHigh) || (infiniteLow && infiniteHigh && signLow != signHigh) ||
        (infiniteLow && infiniteC && signLow != negativeC) || (infiniteHigh && infiniteC && signHigh != negativeC);
    if (invalid) return 0xffc00000u;
    if (infiniteLow || infiniteHigh || infiniteC) return ((infiniteLow ? signLow : infiniteHigh ? signHigh : negativeC) ? 0x80000000u : 0u) | 0x7f800000u;
    Exact sum;
    sum.Add(aLow.significand * bLow.significand, aLow.scale + bLow.scale + 101u, signLow);
    sum.Add(aHigh.significand * bHigh.significand, aHigh.scale + bHigh.scale + 101u, signHigh);
    if (magnitudeC >= 0x00800000u) sum.Add((c & 0x7fffffu) | 0x800000u, (magnitudeC >> 23u) - 1u, negativeC);
    if (sum.Zero()) return (signLow && signHigh && negativeC) ? 0x80000000u : 0u;
    const auto rounded = sum.Rounded();
    if (!rounded.has_value() || ((*rounded >> 23u) & 0xffu) < 27u || ((*rounded >> 23u) & 0xffu) >= 254u) return std::nullopt;
    return rounded;
}

std::uint32_t RandomHalf(std::mt19937& random) {
    const std::uint32_t exponent = random() % 31u;
    return ((random() & 1u) << 15u) | (exponent << 10u) | (random() & 0x3ffu);
}

std::uint32_t RandomF32(std::mt19937& random, std::uint32_t lowExponent, std::uint32_t highExponent) {
    const std::uint32_t exponent = lowExponent + random() % (highExponent - lowExponent + 1u);
    return ((random() & 1u) << 31u) | (exponent << 23u) | (random() & 0x7fffffu);
}

std::uint32_t HalfToF32(std::uint32_t half) {
    const Half decoded = Decode(half, 0u);
    if (decoded.zero) return decoded.negative ? 0x80000000u : 0u;
    std::uint64_t significand = decoded.significand;
    std::uint32_t exponent = decoded.scale + 103u;
    while ((significand & 0x400u) == 0u) {
        significand <<= 1u;
        --exponent;
    }
    return (decoded.negative ? 0x80000000u : 0u) | (exponent << 23u) | static_cast<std::uint32_t>((significand & 0x3ffu) << 13u);
}

std::vector<Case> Cases() {
    std::vector<Case> cases;
    const auto add = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        const auto expected = Dot2(a, b, c);
        if (expected.has_value()) cases.push_back({a, b, c, *expected});
    };
    std::mt19937 random(2026u);
    for (std::uint32_t index = 0; index < 60000u; ++index) add(RandomHalf(random) | (RandomHalf(random) << 16u), RandomHalf(random) | (RandomHalf(random) << 16u), RandomF32(random, 60u, 170u));
    for (std::uint32_t index = 0; index < 30000u; ++index) add(RandomHalf(random) | (RandomHalf(random) << 16u), RandomHalf(random) | (RandomHalf(random) << 16u), 0u);
    for (std::uint32_t index = 0; index < 60000u; ++index) {
        const std::uint32_t a = RandomHalf(random) | (RandomHalf(random) << 16u);
        const std::uint32_t b = RandomHalf(random) | (RandomHalf(random) << 16u);
        const float low = std::bit_cast<float>(HalfToF32(a & 0xffffu)) * std::bit_cast<float>(HalfToF32(b & 0xffffu));
        const float high = std::bit_cast<float>(HalfToF32(a >> 16u)) * std::bit_cast<float>(HalfToF32(b >> 16u));
        const std::uint32_t pick = random() % 4u;
        const float base = pick == 0u ? -low : pick == 1u ? -high : -(low + high);
        const std::uint32_t nudge = static_cast<std::uint32_t>(static_cast<int>(random() % 9u) - 4);
        add(a, b, std::bit_cast<std::uint32_t>(base) + nudge);
    }
    const std::array<std::uint32_t, 8> powers{0x3c00u, 0x4000u, 0x3800u, 0x0400u, 0x0001u, 0x7bffu, 0x3c01u, 0x3bffu};
    for (const std::uint32_t lowA : powers) for (const std::uint32_t highA : powers) for (const std::uint32_t lowB : powers) for (const std::uint32_t highB : powers) {
        for (const std::uint32_t c : {0x00000000u, 0x3f800000u, 0xbf800000u, 0x33800000u, 0x4b800000u, 0x3f800001u, 0xcb7fffffu, 0x0d800000u, 0x00800000u, 0x8c7fffffu}) {
            add(lowA | (highA << 16u), lowB | (highB << 16u), c);
            add(lowA | (highA << 16u), (lowB ^ 0x8000u) | (highB << 16u), c);
        }
    }
    const std::array<std::uint32_t, 10> specials{0x7c00u, 0xfc00u, 0x7e00u, 0x7c01u, 0xfe55u, 0x0000u, 0x8000u, 0x3c00u, 0xbc00u, 0x0001u};
    for (const std::uint32_t lowA : specials) for (const std::uint32_t highA : specials) for (const std::uint32_t lowB : specials) {
        for (const std::uint32_t c : {0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f800123u, 0x3f800000u, 0x00000001u}) add(lowA | (highA << 16u), lowB | (0x3c00u << 16u), c);
    }
    while (cases.size() % Threads != 0u) cases.push_back({0u, 0u, 0x3f800000u, 0x3f800000u});
    if (cases.size() > Capacity) throw std::runtime_error("f32 dot2 rounding: too many cases");
    return cases;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto cases = Cases();
        for (std::size_t index = 0; index < cases.size(); ++index) {
            Input[index * 4u] = cases[index].a;
            Input[index * 4u + 1u] = cases[index].b;
            Input[index * 4u + 2u] = cases[index].c;
        }
        Output.fill(0xdeadbeefu);
        const std::vector<std::uint32_t> code{
            0x34020084u, 0x8f098908u, 0x4a020209u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
            0xcc134008u, 0x1c1a0b04u, 0x2c060282u, 0xe0701000u, 0x80010803u, 0xbf810000u,
        };
        std::vector<std::uint32_t> userData(8, 0u);
        const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::span<const std::uint32_t> words(code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false};
        const auto result = ShaderRecompiler::Recompile(request);
        device->Dispatch(result, static_cast<std::uint32_t>(cases.size() / Threads), 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        for (std::size_t index = 0; index < cases.size(); ++index) {
            const auto& entry = cases[index];
            Require(Output[index] == entry.expected, "f32 dot2 rounding: v_dot2_f32_f16 " + Hex(entry.a) + ", " + Hex(entry.b) + ", " + Hex(entry.c) + " is " + Hex(Output[index]) + ", expected " + Hex(entry.expected));
        }
        std::printf("f32 dot2 rounding tests passed (%zu cases)\n", cases.size());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
