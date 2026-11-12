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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 32;
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(256) constexpr std::array<std::uint32_t, 16> Code{
    0x34020084, 0x34060084, 0xe0381000, 0x80000401, 0xbf8c3f70,
    0xcc0f400a, 0x18020b04,
    0xcc0f480b, 0x18020b04,
    0xcc0f410c, 0x5a020b04,
    0xcc0fc00d, 0x18020b04,
    0xe0781000, 0x80010a03, 0xbf810000,
};

struct Row {
    std::uint32_t lhs;
    std::uint32_t rhs;
    std::array<std::uint32_t, 4> expected;
    std::uint32_t quiet = 0u;
};

constexpr std::array<Row, 15> Rows{{
    {0xbc003c00u, 0x90001000u, {0xbc003c00u, 0xbc003c01u, 0xbc013c00u, 0xbc003c00u}},
    {0xbc003c00u, 0x80010001u, {0xbc003c00u, 0xbc003c01u, 0xbc013c00u, 0xbc003c00u}},
    {0xbc003c00u, 0x3c00bc00u, {0u, 0u, 0x80008000u, 0u}},
    {0xfbff7bffu, 0xfbff7bffu, {0xfc007c00u, 0xfbff7c00u, 0xfc007bffu, 0xfbff7bffu}},
    {0x84000400u, 0x03ff83ffu, {0x80010001u, 0x80010001u, 0x80010001u, 0x80010001u}},
    {0xfc007c00u, 0x3c003c00u, {0xfc007c00u, 0xfc007c00u, 0xfc007c00u, 0xfc007c00u}},
    {0xfe427e01u, 0x3c003c00u, {0xfe427e01u, 0xfe427e01u, 0xfe427e01u, 0xfe427e01u}},
    {0xfc007c00u, 0x7c00fc00u, {0xfe00fe00u, 0xfe00fe00u, 0xfe00fe00u, 0xfe00fe00u}},
    {0x80008000u, 0x80008000u, {0x80008000u, 0x80008000u, 0x80008000u, 0x80008000u}},
    {0x00000000u, 0x80008000u, {0u, 0u, 0x80008000u, 0u}},
    {0xbc003c00u, 0x10009000u, {0xbbff3bffu, 0xbbff3bffu, 0xbbff3bffu, 0xbbff3bffu}},
    {0x00010001u, 0x00010001u, {0x00020002u, 0x00020002u, 0x00020002u, 0x00020002u}},
    {0xfbff7bffu, 0xbc003c00u, {0xfbff7bffu, 0xfbff7c00u, 0xfc007bffu, 0xfbff7bffu}},
    {0xfd237c01u, 0x3c003c00u, {0xfd237c01u, 0xfd237c01u, 0xfd237c01u, 0xfd237c01u}, 0x02000200u},
    {0x3c003c00u, 0x7e01fc05u, {0x7e01fc05u, 0x7e01fc05u, 0x7e01fc05u, 0x7e01fc05u}, 0x00000200u},
}};

std::uint32_t Add(std::uint32_t a, std::uint32_t b, std::uint32_t rounding, std::uint32_t denorm, bool ieee, bool overflow) {
    const auto flush = [](std::uint32_t value) { return (value & 0x7fffu) < 0x400u ? value & 0x8000u : value; };
    if ((denorm & 1u) == 0u) { a = flush(a); b = flush(b); }
    const auto mag = [](std::uint32_t value) { return value & 0x7fffu; };
    if (mag(a) > 0x7c00u) return a | (ieee ? 0x200u : 0u);
    if (mag(b) > 0x7c00u) return b | (ieee ? 0x200u : 0u);
    if (mag(a) == 0x7c00u || mag(b) == 0x7c00u) return mag(a) == 0x7c00u && mag(b) == 0x7c00u && a != b ? 0xfe00u : mag(a) == 0x7c00u ? a : b;
    const auto units = [&](std::uint32_t value) {
        const auto exponent = mag(value) >> 10u;
        const std::int64_t magnitude = exponent == 0u ? mag(value) : static_cast<std::int64_t>(0x400u | (value & 0x3ffu)) << (exponent - 1u);
        return (value & 0x8000u) ? -magnitude : magnitude;
    };
    const auto sum = units(a) + units(b);
    if (sum == 0) return rounding == 2u ? (a | b) & 0x8000u : a & b & 0x8000u;
    const std::uint32_t sign = sum < 0 ? 0x8000u : 0u;
    const auto magnitude = static_cast<std::uint64_t>(sum < 0 ? -sum : sum);
    const auto shift = std::bit_width(magnitude) > 11u ? std::bit_width(magnitude) - 11u : 0u;
    auto significand = magnitude >> shift;
    const auto remainder = magnitude - (significand << shift);
    const auto halfway = shift == 0u ? 0u : std::uint64_t{1} << (shift - 1u);
    const bool increment = rounding == 0u ? remainder > halfway || (shift != 0u && remainder == halfway && (significand & 1u)) : remainder != 0u && ((rounding == 1u && sign == 0u) || (rounding == 2u && sign != 0u));
    significand += increment;
    const auto rounded = significand << shift;
    std::uint32_t result;
    if (rounded >= (std::uint64_t{1} << 40u)) {
        const bool infinity = !overflow && (rounding == 0u || (rounding == 1u && sign == 0u) || (rounding == 2u && sign != 0u));
        result = sign | (infinity ? 0x7c00u : 0x7bffu);
    } else if (rounded < 0x400u) {
        result = sign | static_cast<std::uint32_t>(rounded);
    } else {
        const auto exponent = std::bit_width(rounded) - 11u;
        result = sign | static_cast<std::uint32_t>(((exponent + 1u) << 10u) | ((rounded >> exponent) & 0x3ffu));
    }
    return (denorm & 2u) == 0u ? flush(result) : result;
}

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

}

int main(int argc, char** argv) {
    try {
        const auto wave = argc > 1 ? static_cast<std::uint32_t>(std::stoul(argv[1])) : 32u;
        const auto selectedRounding = argc > 2 ? static_cast<std::uint32_t>(std::stoul(argv[2])) : 0u;
        const bool selectedInteger = argc > 3 && std::stoul(argv[3]) != 0u;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint32_t> userData(8);
        const auto input = Descriptor(Input.data(), sizeof(Input));
        const auto output = Descriptor(Output.data(), sizeof(Output));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::span<const std::uint32_t> code(Code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        std::uint64_t compared = 0u;
        for (const bool integer : {false, true}) {
            if (integer != selectedInteger) continue;
            auto target = device->Target();
            std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
            if (integer) std::erase(capabilities, 10u);
            target.supportedCapabilities = capabilities;
            target.featureDigestSealed = false;
            for (std::uint32_t rounding = 0u; rounding < 4u; ++rounding) {
                if (rounding != selectedRounding) continue;
                for (std::uint32_t denorm = 0u; denorm < 4u; ++denorm) {
                    for (const bool ieee : {false, true}) {
                        for (const bool dx10 : {false, true}) {
                            for (const bool overflow : {false, true}) {
                                for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                                    const auto& row = Rows[tid % Rows.size()];
                                    Input[tid * 4u] = row.lhs;
                                    Input[tid * 4u + 1u] = row.rhs;
                                }
                                ShaderRecompiler::RecompileRequest request{
                                    {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
                                    {wave, 0, userData, compute, std::nullopt, std::nullopt, memory}, target, {0, 0, 0, 128}
                                };
                                request.useCache = false;
                                request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0x30u | (rounding << 2u) | (denorm << 6u), dx10, ieee, overflow};
                                const auto result = ShaderRecompiler::Recompile(request);
                                Output.fill(0xdeadbeefu);
                                device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
                                device->WaitIdle();
                                for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                                    for (std::uint32_t op = 0; op < 4u; ++op) {
                                        std::uint32_t expected = 0u;
                                        for (std::uint32_t half = 0; half < 2u; ++half) {
                                            auto a = (Input[tid * 4u] >> (op == 1u && half == 0u ? 16u : half * 16u)) & 0xffffu;
                                            auto b = (Input[tid * 4u + 1u] >> (half * 16u)) & 0xffffu;
                                            if (op == 2u) { if (half == 0u) b ^= 0x8000u; else a ^= 0x8000u; }
                                            auto bits = Add(a, b, rounding, denorm, ieee, overflow);
                                            if (op == 3u) {
                                                if ((bits & 0x7fffu) > 0x7c00u) { if (dx10) bits = 0u; }
                                                else if ((bits & 0x8000u) != 0u) bits = 0u;
                                                else if (bits > 0x3c00u) bits = 0x3c00u;
                                            }
                                            expected |= bits << (half * 16u);
                                        }
                                        if (op == 0u && denorm == 3u && !overflow) Require(expected == (Rows[tid % Rows.size()].expected[rounding] | (ieee ? Rows[tid % Rows.size()].quiet : 0u)), "fixed rounding reference");
                                        if (Output[tid * 4u + op] != expected) {
                                            std::printf("integer=%u round=%u denorm=%u ieee=%u dx10=%u overflow=%u row=%u op=%u actual=%08x expected=%08x\n", integer, rounding, denorm, ieee, dx10, overflow, tid % static_cast<std::uint32_t>(Rows.size()), op, Output[tid * 4u + op], expected);
                                            throw std::runtime_error("packed add f16 rounding mismatch");
                                        }
                                        ++compared;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        std::cout << "packed add f16 rounding: " << compared << " words passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
