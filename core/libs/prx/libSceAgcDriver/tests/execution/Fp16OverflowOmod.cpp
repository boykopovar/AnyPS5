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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Columns = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 16> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd535000au, 0x08020b04u, 0xd535000bu,
    0x10020b04u, 0xd535000cu, 0x18020b04u, 0xd535000du, 0x00020b04u, 0xe0781000u, 0x80010a03u, 0xbf810000u,
};

constexpr std::uint32_t Positive = 0x78e2u;
constexpr std::uint32_t One = 0x3c00u;

constexpr std::uint32_t ExpectedOverflow[2][Columns] = {
    {0x00007bffu, 0x00007bffu, 0x000074e2u, 0x000078e2u},
    {0x0000fbffu, 0x0000fbffu, 0x0000f4e2u, 0x0000f8e2u},
};
constexpr std::uint32_t ExpectedInfinity[2][Columns] = {
    {0x00007c00u, 0x00007c00u, 0x000074e2u, 0x000078e2u},
    {0x0000fc00u, 0x0000fc00u, 0x0000f4e2u, 0x0000f8e2u},
};
constexpr const char* Names[Columns] = {"v_mul_f16 omod:mul:2", "v_mul_f16 omod:mul:4", "v_mul_f16 omod:div:2", "v_mul_f16"};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = Positive | ((tid & 1u) != 0u ? 0x8000u : 0u);
        Input[tid * Inputs + 1u] = One;
        Input[tid * Inputs + 2u] = 0u;
        Input[tid * Inputs + 3u] = 0u;
    }
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

void Check(const std::uint32_t (&expected)[2][Columns], const char* mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t column = 0; column < Columns; ++column) {
            const std::uint32_t actual = Output[tid * Results + column] & 0xffffu;
            const std::uint32_t wanted = expected[tid & 1u][column];
            Require(actual == wanted, std::string("fp16 omod overflow: lane ") + std::to_string(tid) + " " + mode + " " + Names[column] + " is " + Hex(actual) + ", expected " + Hex(wanted));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped, the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        Run(*device, ShaderRecompiler::ShaderFloatMode{0x00u, true, false, false});
        Check(ExpectedInfinity, "FP16_OVFL=0");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0x00u, true, false, true});
        Check(ExpectedOverflow, "FP16_OVFL=1");
        std::puts("fp16 omod overflow tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
