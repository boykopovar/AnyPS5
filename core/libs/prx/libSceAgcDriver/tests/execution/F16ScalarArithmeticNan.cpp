#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 10;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 16> Code{
    0x34020084u, 0x34040084u, 0xe03c1000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
    0x7e1a0280u, 0x64140b04u, 0x66160b04u, 0x68180b04u, 0x6a1a0b04u, 0xe0781000u, 0x80010a02u, 0xbf810000u,
};

constexpr std::array<std::array<std::uint32_t, 2>, Threads> Operands{{
    {0x0000u, 0x7e00u},
    {0x7e00u, 0x3c00u},
    {0xfe55u, 0x7e00u},
    {0x7d01u, 0x3c00u},
    {0x3c00u, 0x7d01u},
    {0x7c00u, 0xfc00u},
    {0x7c00u, 0x0000u},
    {0x3c00u, 0x4248u},
    {0x7c00u, 0x7c00u},
    {0x7d01u, 0xfe55u},
}};

using Expected = std::array<std::array<std::uint32_t, Results>, Threads>;

constexpr Expected IeeeOff{{
    {0x7e00u, 0xfe00u, 0x7e00u, 0x7e00u},
    {0x7e00u, 0x7e00u, 0xfe00u, 0x7e00u},
    {0xfe55u, 0xfe55u, 0x7e55u, 0xfe55u},
    {0x7d01u, 0x7d01u, 0xfd01u, 0x7d01u},
    {0x7d01u, 0xfd01u, 0x7d01u, 0x7d01u},
    {0xfe00u, 0x7c00u, 0xfc00u, 0xfc00u},
    {0x7c00u, 0x7c00u, 0xfc00u, 0xfe00u},
    {0x4424u, 0xc048u, 0x4048u, 0x4248u},
    {0x7c00u, 0xfe00u, 0xfe00u, 0x7c00u},
    {0x7d01u, 0x7d01u, 0xfd01u, 0x7d01u},
}};

constexpr Expected IeeeOn{{
    {0x7e00u, 0xfe00u, 0x7e00u, 0x7e00u},
    {0x7e00u, 0x7e00u, 0xfe00u, 0x7e00u},
    {0xfe55u, 0xfe55u, 0x7e55u, 0xfe55u},
    {0x7f01u, 0x7f01u, 0xff01u, 0x7f01u},
    {0x7f01u, 0xff01u, 0x7f01u, 0x7f01u},
    {0xfe00u, 0x7c00u, 0xfc00u, 0xfc00u},
    {0x7c00u, 0x7c00u, 0xfc00u, 0xfe00u},
    {0x4424u, 0xc048u, 0x4048u, 0x4248u},
    {0x7c00u, 0xfe00u, 0xfe00u, 0x7c00u},
    {0x7f01u, 0x7f01u, 0xff01u, 0x7f01u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%04x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, bool ieee) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = Operands[tid][0];
        Input[tid * Inputs + 1u] = Operands[tid][1];
        Input[tid * Inputs + 2u] = 0u;
        Input[tid * Inputs + 3u] = 0u;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, ieee, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Check(const Expected& expected, const char* mode) {
    constexpr const char* Names[Results] = {"v_add_f16", "v_sub_f16", "v_subrev_f16", "v_mul_f16"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t op = 0; op < Results; ++op) {
            const std::uint32_t actual = Output[tid * Results + op];
            Require(actual == expected[tid][op], std::string(Names[op]) + " " + mode + " of " + Hex(Operands[tid][0]) + ", " + Hex(Operands[tid][1]) + " is " + Hex(actual) + ", expected " + Hex(expected[tid][op]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!TargetHasCapability(target, spv::CapabilityFloat16) || !TargetHasCapability(target, spv::CapabilityRoundingModeRTE) || !TargetHasCapability(target, spv::CapabilityDenormPreserve)) {
            std::puts("f16 scalar arithmetic NaN tests skipped: native f16 arithmetic is unavailable");
            return VulkanTestSkipped;
        }
        Run(*device, false);
        Check(IeeeOff, "with IEEE mode off");
        Run(*device, true);
        Check(IeeeOn, "with IEEE mode on");
        std::puts("f16 scalar arithmetic NaN tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
