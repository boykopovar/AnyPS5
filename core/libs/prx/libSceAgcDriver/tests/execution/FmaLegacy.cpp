#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 30> FmaLegacyCode{
    0x34020083, 0x34060083, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0xd540000a, 0x041a0b04, 0xd540000b, 0x241a0b04, 0xd540010c, 0x841a0b04, 0xd540000d,
    0x041a0a80, 0xd540020e, 0x641a0b04, 0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008,
    0x80010c03, 0xe070200c, 0x80010d03, 0xe0702010, 0x80010e03, 0xbf810000,
};

struct Row {
    std::uint32_t a, b, c;
};

constexpr std::array<Row, 20> Rows{{
    {0x00000000u, 0x7f800000u, 0x3f800000u},
    {0x80000000u, 0x7f800000u, 0xc0200000u},
    {0x7f800000u, 0x00000000u, 0x3f400000u},
    {0xff800000u, 0x80000000u, 0x40400000u},
    {0x00000000u, 0x7fc00000u, 0x3fc00000u},
    {0x7fc00000u, 0x80000000u, 0xc0800000u},
    {0x00000000u, 0x40000000u, 0x7f800000u},
    {0x80000000u, 0xc0400000u, 0xff800000u},
    {0x00000000u, 0x3f800000u, 0x7fc00000u},
    {0x3fc00000u, 0x40100000u, 0xbf400000u},
    {0xc0c00000u, 0x3f000000u, 0x41200000u},
    {0x7f800000u, 0x40000000u, 0x3f800000u},
    {0x7f800000u, 0xbf800000u, 0x7f800000u},
    {0x7fc00000u, 0x3f800000u, 0x3f800000u},
    {0x40400000u, 0x40800000u, 0xc1400000u},
    {0x7f7fffffu, 0x40000000u, 0x3f800000u},
    {0x3ec00000u, 0xc1480000u, 0x42c88000u},
    {0xc4800000u, 0xbd800000u, 0xc2800000u},
    {0x40e00000u, 0x41100000u, 0x3f000000u},
    {0x80000000u, 0x80000000u, 0x40a00000u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < Rows.size(); ++tid) {
        const auto& row = Rows[tid];
        const std::array<std::uint32_t, 3> words{row.a, row.b, row.c};
        std::copy(words.begin(), words.end(), Input.begin() + tid * Inputs);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(FmaLegacyCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool IsNan(std::uint32_t bits) {
    return (bits & 0x7fffffffu) > 0x7f800000u;
}

std::uint32_t FmaLegacy(float a, float b, float c) {
    if (a == 0.0f || b == 0.0f) {
        return std::bit_cast<std::uint32_t>(0.0f + c);
    }
    return std::bit_cast<std::uint32_t>(std::fma(a, b, c));
}

void Check() {
    constexpr std::array<const char*, 5> names{
        "v_fma_legacy_f32", "v_fma_legacy_f32 -src0", "v_fma_legacy_f32 |src0| -src2", "v_fma_legacy_f32 src0=0", "v_fma_legacy_f32 -src0 -|src1|",
    };
    for (std::uint32_t tid = 0; tid < Rows.size(); ++tid) {
        const auto& row = Rows[tid];
        const float a = std::bit_cast<float>(row.a);
        const float b = std::bit_cast<float>(row.b);
        const float c = std::bit_cast<float>(row.c);
        const std::array<std::uint32_t, 5> expected{
            FmaLegacy(a, b, c), FmaLegacy(-a, b, c), FmaLegacy(std::fabs(a), b, -c), FmaLegacy(0.0f, b, c), FmaLegacy(-a, -std::fabs(b), c),
        };
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const auto actual = Output[tid * Results + j];
            const bool matches = actual == expected[j] || (IsNan(actual) && IsNan(expected[j]));
            Require(matches, std::string("fma legacy: lane ") + std::to_string(tid) + " (" + Hex(row.a) + ", " + Hex(row.b) + ", " + Hex(row.c) + ") " + names[j] + " is " + Hex(actual) + ", expected " + Hex(expected[j]));
        }
    }
}

const Testing::Case results{"FmaLegacy_ZeroOperandsAndSourceModifiers_MatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
