#include "prx/libSceAgcDriver/Execution/include/NanSpeculationCounters.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t NanLane = 5;
constexpr std::uint32_t SignalingNan = 0x7f800005u;
constexpr std::uint32_t One = 0x3f800000u;

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x34140084u, 0x34160084u, 0xe03c1000u, 0x8001010au, 0xbf8c3f70u, 0x7e0e0280u, 0x7d8800c0u, 0x10080501u,
    0x7e0a0304u, 0x060c0705u, 0xe0781000u, 0x8000040bu, 0xbf810000u,
};

alignas(256) std::array<std::uint32_t, Threads * 4> Inputs{};
alignas(256) std::array<std::uint32_t, Threads * 4> Outputs{};

void Set(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t nanLane) {
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Inputs[lane * 4u] = lane == nanLane ? SignalingNan : One;
        Inputs[lane * 4u + 1u] = One;
        Inputs[lane * 4u + 2u] = 0x3f000000u;
        Inputs[lane * 4u + 3u] = 0u;
    }
    Outputs.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Outputs.data(), static_cast<std::uint32_t>(Outputs.size() * 4u));
    const auto input = BufferDescriptor(Inputs.data(), static_cast<std::uint32_t>(Inputs.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(input.begin(), input.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    Require(Outputs[0] == One, "nan speculation count: a lane without a NaN produced a wrong product");
}

AgcDriver::NanSpeculationCount Counts() {
    const auto count = AgcDriver::ReadNanSpeculationCount(reinterpret_cast<std::uintptr_t>(Code.data()), 0u);
    Require(count.has_value(), "nan speculation count: the region was not speculated, so it has no counter slot");
    return *count;
}

}

int main() {
    try {
        Set("APS5_NAN_SPECULATION", "1");
        Set("APS5_NAN_SPECULATION_MIN", "compute:1,pixel:4096");
        Set("APS5_NAN_SPECULATION_COUNT", "1");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, NanLane);
        const auto forced = Counts();
        Require(forced.entered >= 1u && forced.exact == 1u, "nan speculation count: one NaN lane must send exactly one wave to the exact copy, got " + std::to_string(forced.exact) + " of " + std::to_string(forced.entered));
        Run(*device, Threads);
        const auto clean = Counts();
        Require(clean.exact == forced.exact, "nan speculation count: a dispatch without a NaN fell back to the exact copy");
        Require(clean.entered == 2u * forced.entered, "nan speculation count: the second dispatch entered " + std::to_string(clean.entered - forced.entered) + " waves, the first " + std::to_string(forced.entered));
        std::puts("nan speculation count tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
