#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Wide = unsigned __int128;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Stride = 4;
constexpr std::uint32_t Batches = 128;
constexpr std::uint32_t DefaultNan = 0xffc00000u;
alignas(256) std::array<std::uint32_t, Threads * Stride> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

struct Vector {
    std::uint32_t a, b, c, vcc;
};

struct Term {
    bool negative = false;
    Wide mantissa = 0;
    int exponent = 0;
};

int BitLength(Wide value) {
    int length = 0;
    while (value != 0u) {
        ++length;
        value >>= 1u;
    }
    return length;
}

Term Decode(std::uint32_t bits) {
    const std::uint32_t field = (bits >> 23u) & 0xffu;
    const std::uint32_t fraction = bits & 0x007fffffu;
    return {(bits >> 31u) != 0u, field == 0u ? fraction : (fraction | 0x00800000u), field == 0u ? -149 : static_cast<int>(field) - 150};
}

std::uint32_t Round(bool negative, Wide mantissa, int exponent) {
    const std::uint32_t sign = negative ? 0x80000000u : 0u;
    const int top = exponent + BitLength(mantissa) - 1;
    int kept = top >= -126 ? top - 23 : -149;
    const int shift = kept - exponent;
    Wide quotient = 0;
    if (shift >= 128) {
        quotient = 0;
    } else if (shift > 0) {
        quotient = mantissa >> shift;
        const Wide remainder = mantissa & ((Wide{1} << shift) - 1u);
        const Wide half = Wide{1} << (shift - 1);
        if (remainder > half || (remainder == half && (quotient & 1u) != 0u)) ++quotient;
    } else {
        quotient = mantissa << -shift;
    }
    if (quotient == (Wide{1} << 24u)) {
        quotient >>= 1u;
        ++kept;
    }
    if (kept == -149 && quotient <= 0x00800000u) return sign | static_cast<std::uint32_t>(quotient);
    const int field = kept + 150;
    if (field >= 255) return sign | 0x7f800000u;
    return sign | (static_cast<std::uint32_t>(field) << 23u) | (static_cast<std::uint32_t>(quotient) & 0x007fffffu);
}

std::uint32_t Exact(Term product, Term addend, int scale) {
    if (product.mantissa == 0u && addend.mantissa == 0u) return product.negative && addend.negative ? 0x80000000u : 0u;
    product.exponent += scale;
    addend.exponent += scale;
    if (product.mantissa == 0u) return Round(addend.negative, addend.mantissa, addend.exponent);
    if (addend.mantissa == 0u) return Round(product.negative, product.mantissa, product.exponent);
    const int productTop = product.exponent + BitLength(product.mantissa);
    const int addendTop = addend.exponent + BitLength(addend.mantissa);
    const int lowest = std::min(product.exponent, addend.exponent);
    if (std::max(productTop, addendTop) - lowest <= 125) {
        const Wide lhs = product.mantissa << (product.exponent - lowest);
        const Wide rhs = addend.mantissa << (addend.exponent - lowest);
        if (product.negative == addend.negative) return Round(product.negative, lhs + rhs, lowest);
        if (lhs == rhs) return 0u;
        return lhs > rhs ? Round(product.negative, lhs - rhs, lowest) : Round(addend.negative, rhs - lhs, lowest);
    }
    const Term& large = productTop > addendTop ? product : addend;
    const Term& small = productTop > addendTop ? addend : product;
    const Wide shifted = large.mantissa << 65u;
    return Round(large.negative, large.negative == small.negative ? shifted + 1u : shifted - 1u, large.exponent - 65);
}

std::uint32_t Reference(const Vector& v) {
    const auto magnitude = [](std::uint32_t bits) { return bits & 0x7fffffffu; };
    const auto infinite = [&](std::uint32_t bits) { return magnitude(bits) == 0x7f800000u; };
    const auto zero = [&](std::uint32_t bits) { return magnitude(bits) == 0u; };
    const bool productNegative = ((v.a ^ v.b) >> 31u) != 0u;
    if ((infinite(v.a) && zero(v.b)) || (zero(v.a) && infinite(v.b))) return DefaultNan;
    const bool productInfinite = infinite(v.a) || infinite(v.b);
    if (productInfinite && infinite(v.c) && productNegative != ((v.c >> 31u) != 0u)) return DefaultNan;
    if (productInfinite) return (productNegative ? 0x80000000u : 0u) | 0x7f800000u;
    if (infinite(v.c)) return v.c;
    const Term a = Decode(v.a);
    const Term b = Decode(v.b);
    const Term product{productNegative, a.mantissa * b.mantissa, a.exponent + b.exponent};
    const int scale = v.vcc == 0u ? 0 : (((v.c >> 23u) & 0xffu) >= 128u ? 64 : -64);
    return Exact(product, Decode(v.c), scale);
}

std::vector<Vector> Vectors() {
    std::mt19937 random(0xd1f3a5u);
    const auto bits = [&](std::uint32_t lowField, std::uint32_t highField) {
        const std::uint32_t field = lowField + random() % (highField - lowField + 1u);
        return (random() & 0x80000000u) | (field << 23u) | (random() & 0x007fffffu);
    };
    constexpr std::array<std::uint32_t, 12> specials{0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7f7fffffu, 0xff7fffffu, 0x00800000u, 0x80800000u, 0x00000001u, 0x807fffffu, 0x3f800000u, 0xc0000000u};
    std::vector<Vector> vectors;
    for (std::uint32_t index = 0; index < Threads * Batches; ++index) {
        Vector v{0u, 0u, 0u, random() & 1u};
        switch (index % 8u) {
            case 0: v = {bits(0u, 254u), bits(0u, 254u), bits(0u, 254u), v.vcc}; break;
            case 1: v = {bits(100u, 154u), bits(100u, 154u), bits(100u, 154u), v.vcc}; break;
            case 2: {
                v.a = bits(90u, 164u);
                v.b = bits(90u, 164u);
                const float product = std::bit_cast<float>(v.a) * std::bit_cast<float>(v.b);
                const std::uint32_t nearest = std::bit_cast<std::uint32_t>(-product);
                v.c = (nearest & 0x7fffffffu) >= 0x7f800000u ? bits(90u, 164u) : nearest + (random() % 7u) - 3u;
                break;
            }
            case 3: v = {bits(60u, 100u), bits(60u, 100u), bits(0u, 70u), 1u}; break;
            case 4: v = {bits(170u, 254u), bits(170u, 254u), bits(128u, 254u), 1u}; break;
            case 5: v = {bits(0u, 0u), bits(100u, 200u), random() % 2u == 0u ? bits(0u, 0u) : bits(0u, 140u), v.vcc}; if (random() % 2u == 0u) std::swap(v.a, v.b); break;
            case 6: v = {specials[random() % specials.size()], specials[random() % specials.size()], specials[random() % specials.size()], v.vcc}; break;
            default: {
                v.a = bits(110u, 145u) & 0xfffffc00u;
                v.b = bits(110u, 145u) & 0xfffffc00u;
                const int field = static_cast<int>((v.a >> 23u) & 0xffu) + static_cast<int>((v.b >> 23u) & 0xffu) - 153 - static_cast<int>(random() % 40u);
                v.c = random() % 4u == 0u ? 0u : (random() & 0x80000000u) | (static_cast<std::uint32_t>(std::max(field, 1)) << 23u) | (random() & 0x007fffffu);
                break;
            }
        }
        for (std::uint32_t* word : {&v.a, &v.b, &v.c}) {
            if ((*word & 0x7fffffffu) > 0x7f800000u) *word = bits(100u, 154u);
        }
        vectors.push_back(v);
    }
    return vectors;
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

std::vector<std::uint32_t> Program(std::uint32_t count) {
    std::vector<std::uint32_t> code{0x34020082u, 0xe0302000u, 0x80000401u, 0xe0302004u, 0x80000501u, 0xe0302008u, 0x80000601u, 0xe030200cu, 0x80000701u, 0xbf8c3f70u, 0x7d8a0e80u};
    std::uint32_t previous = 4u;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t destination = 10u + index;
        code.push_back(0xd56f0000u | destination);
        code.push_back((262u << 18u) | (261u << 9u) | (256u + previous));
        previous = destination;
    }
    code.insert(code.end(), {0xe0702000u, 0x80010001u | (previous << 8u), 0xbf810000u});
    return code;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false};
    return ShaderRecompiler::Recompile(request);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped, the device has no Float64");
            return VulkanTestSkipped;
        }
        const auto single = Program(1u);
        const auto chained = Program(9u);
        const auto result = Compile(*device, single);
        const auto singleWords = result.spirv.Words().size();
        const auto vectors = Vectors();
        std::uint32_t failures = 0;
        std::string report;
        for (std::uint32_t first = 0; first < vectors.size(); first += Threads) {
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                const auto& v = vectors[first + lane];
                std::copy_n(std::array<std::uint32_t, 4>{v.a, v.b, v.c, v.vcc}.begin(), 4, Input.begin() + lane * Stride);
            }
            Output.fill(0xdeadbeefu);
            device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(single.data()));
            device->WaitIdle();
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                const auto& v = vectors[first + lane];
                const auto expected = Reference(v);
                const auto actual = Output[lane * Stride];
                if (actual != expected && failures++ < 24u) {
                    report += "div fmas exact: v_div_fmas_f32(" + Hex(v.a) + ", " + Hex(v.b) + ", " + Hex(v.c) + ") vcc " + std::to_string(v.vcc) + " is " + Hex(actual) + ", expected " + Hex(expected) + "\n";
                }
            }
        }
        Require(failures == 0u, report + std::to_string(failures) + " of " + std::to_string(vectors.size()) + " results differ");
        const auto chainedWords = Compile(*device, chained).spirv.Words().size();
        Require(chainedWords > singleWords && (chainedWords - singleWords) / 8u < 400u, "div fmas exact: each further v_div_fmas_f32 adds " + std::to_string((chainedWords - singleWords) / 8u) + " SPIR-V words");
        std::printf("div fmas exact tests passed (%zu vectors, %zu SPIR-V words for one v_div_fmas_f32, %zu for nine)\n", vectors.size(), singleWords, chainedWords);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
