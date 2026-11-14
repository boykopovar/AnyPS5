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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 30> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd535000au, 0x18020b04u, 0xd532000bu,
    0x18020b04u, 0xd74b000cu, 0x1c1a0b04u, 0xd535000du, 0x08020b04u, 0xd58a000eu, 0x18000104u, 0xd58a000fu,
    0x08000104u, 0xd5df0010u, 0x08000104u, 0xd5390011u, 0x08020b04u, 0xd5d40012u, 0x10000104u, 0xe0781000u,
    0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xbf810000u,
};

constexpr const char* Names[] = {
    "v_mul_f16 omod:div:2", "v_add_f16 omod:div:2", "v_fma_f16 omod:div:2", "v_mul_f16 omod:mul:2", "v_cvt_f16_f32 omod:div:2",
    "v_cvt_f16_f32 omod:mul:2", "v_fract_f16 omod:mul:2", "v_max_f16 omod:mul:2", "v_rcp_f16 omod:mul:4",
};

struct Case {
    std::uint32_t column;
    std::array<std::uint32_t, 3> sources;
    std::uint32_t expected;
};

constexpr std::array<Case, 8> Nearest{{
    {0u, {0x7a00u, 0x3e00u, 0u}, 0x7c00u},
    {1u, {0x7a00u, 0x7a00u, 0u}, 0x7c00u},
    {2u, {0x7bffu, 0x3c00u, 0x4c00u}, 0x7c00u},
    {3u, {0x0401u, 0x3bfeu, 0u}, 0x0800u},
    {4u, {0x47900000u, 0u, 0u}, 0x7c00u},
    {5u, {0x387ff800u, 0u, 0u}, 0x0800u},
    {6u, {0x8c00u, 0u, 0u}, 0x3fffu},
    {6u, {0x3e00u, 0u, 0u}, 0x3c00u},
}};

constexpr std::array<Case, 5> NearestOverflowClamped{{
    {0u, {0x7a00u, 0x3e00u, 0u}, 0x77ffu},
    {1u, {0x7a00u, 0x7a00u, 0u}, 0x77ffu},
    {2u, {0x7bffu, 0x3c00u, 0x4c00u}, 0x77ffu},
    {4u, {0x47900000u, 0u, 0u}, 0x77ffu},
    {7u, {0x7bffu, 0x3c00u, 0u}, 0x7bffu},
}};

constexpr std::array<Case, 3> TowardZero{{
    {4u, {0x47900000u, 0u, 0u}, 0x77ffu},
    {3u, {0x7800u, 0x3c00u, 0u}, 0x7bffu},
    {8u, {0x0400u, 0u, 0u}, 0x7c00u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::ShaderFloatMode& floatMode) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(AgcDriver::VulkanDevice& device, std::span<const Case> cases, std::uint32_t floatMode, bool fp16Overflow) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto& sources = cases[tid % cases.size()].sources;
        std::copy(sources.begin(), sources.end(), Input.begin() + tid * Inputs);
        Input[tid * Inputs + 3u] = 0u;
    }
    Run(device, ShaderRecompiler::ShaderFloatMode{floatMode, true, false, fp16Overflow});
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const Case& item = cases[tid % cases.size()];
        const std::uint32_t actual = Output[tid * Results + item.column] & 0xffffu;
        Require(actual == item.expected, std::string("f16 omod order: lane ") + std::to_string(tid) + " FLOAT_MODE=" + Hex(floatMode) + " FP16_OVFL=" +
            std::to_string(fp16Overflow) + " " + Names[item.column] + " of " + Hex(item.sources[0]) + ", " + Hex(item.sources[1]) + ", " + Hex(item.sources[2]) +
            " is " + Hex(actual) + ", expected " + Hex(item.expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device, Nearest, 0x00u, false);
        Check(*device, NearestOverflowClamped, 0x00u, true);
        Check(*device, TowardZero, 0x0cu, false);
        std::puts("f16 omod order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
