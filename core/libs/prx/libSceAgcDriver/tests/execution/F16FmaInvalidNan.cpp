#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 10;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 13> FmaCode{
    0x34020084u, 0x34040083u, 0xe03c1000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd74b000au, 0x041a0b04u,
    0x7e160306u, 0x6c160b04u, 0xe0741000u, 0x80010a02u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 12> MixCode{
    0x34020084u, 0x34040083u, 0xe03c1000u, 0x80000401u, 0xbf8c3f70u, 0xcc20600au, 0x1c120b04u, 0xcc20600bu,
    0x0c120d04u, 0xe0741000u, 0x80010a02u, 0xbf810000u,
};

using Rows = std::array<std::array<std::uint32_t, 3>, Threads>;

constexpr Rows FmaRows{{
    {0x7c00u, 0x0000u, 0x0000u},
    {0x0000u, 0xfc00u, 0x3c00u},
    {0x8000u, 0x7c00u, 0xfc00u},
    {0x8000u, 0xfc00u, 0x3c00u},
    {0x7c00u, 0xbc00u, 0x7c00u},
    {0x7c00u, 0x8000u, 0xfc00u},
    {0x7c00u, 0x3c00u, 0xfc00u},
    {0xfc00u, 0x3c00u, 0x7c00u},
    {0x7c00u, 0x0000u, 0x7e00u},
    {0x8000u, 0xfc00u, 0x7d00u},
}};
constexpr Rows MixRows{{
    {0x00007c00u, 0x00000000u, 0x00000000u},
    {0x3c00fc00u, 0x00000000u, 0x80000000u},
    {0x7e007c00u, 0x00008000u, 0x80000000u},
    {0x7d00fc00u, 0x00000000u, 0x00000000u},
    {0x7c007c00u, 0x0000bc00u, 0xbf800000u},
    {0xfc007c00u, 0x00003c00u, 0x3f800000u},
    {0x7c00fc00u, 0x00004000u, 0x40000000u},
    {0xfc00fc00u, 0x0000b800u, 0xbf000000u},
    {0x7e00fc00u, 0x00008000u, 0x00000000u},
    {0x7fff7c00u, 0x00000000u, 0x80000000u},
}};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> value;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const Rows& rows, const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::copy(rows[tid].begin(), rows[tid].end(), &Input[tid * Inputs]);
        Input[tid * Inputs + 3u] = 0u;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = mode.value;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const Mode& mode, const Rows& rows, const char* const (&names)[Results], std::uint32_t expected) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t form = 0; form < Results; ++form) {
            const std::uint32_t actual = Output[tid * Results + form];
            Require(actual == expected, std::string(names[form]) + " " + mode.name + " of " + Hex(rows[tid][0]) + ", " + Hex(rows[tid][1]) + ", " + Hex(rows[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const Mode modes[] = {
            {"without a known float mode", std::nullopt},
            {"with denormals flushed, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x00u, true, false, false}},
            {"with denormals flushed, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x00u, true, true, false}},
            {"with denormals kept, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}},
            {"with denormals kept, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}},
        };
        constexpr const char* FmaNames[Results] = {"v_fma_f16", "v_fmac_f16"};
        constexpr const char* MixNames[Results] = {"v_fma_mix_f32 of three f16 sources", "v_fma_mix_f32 of an f16, an f32 and an f16 source"};
        for (const Mode& mode : modes) {
            Run(*device, FmaCode, FmaRows, mode);
            Check(mode, FmaRows, FmaNames, 0xfe00u);
            if (!mode.value) continue;
            Run(*device, MixCode, MixRows, mode);
            Check(mode, MixRows, MixNames, 0xffc00000u);
        }
        std::puts("f16 fma invalid operation NaN tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
