#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Stride = 4;
alignas(256) std::array<std::uint32_t, MaxThreads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 16> ReadFirstLaneWave32Code{
    0x34020082, 0x4a0400ff, 0x00000100, 0xbe94037e, 0xbefe0380, 0x7e100502, 0xbefe03b0, 0x7e120502,
    0xbefe0314, 0x7e060208, 0x7e080209, 0xe0702000, 0x80010301, 0xe0702004, 0x80010401, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> ReadFirstLaneWave64Code{
    0x34020082, 0x4a0400ff, 0x00000100, 0xbe94047e, 0xbefe0480, 0x7e100502, 0xbefe0380, 0xbeff03ff,
    0x00000300, 0x7e120502, 0xbefe0414, 0x7e060208, 0x7e080209, 0xe0702000, 0x80010301, 0xe0702004,
    0x80010401, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t waveSize) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t waveSize, std::uint32_t firstActiveLane) {
    const std::array<std::uint32_t, 2> expected{0x100u, 0x100u + firstActiveLane};
    for (std::uint32_t tid = 0; tid < waveSize; ++tid) {
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const auto actual = Output[tid * Stride + j];
            Testing::Require(actual == expected[j], "read first lane wave" + std::to_string(waveSize) + ": lane " + std::to_string(tid) + " result " + std::to_string(j) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[j]));
        }
    }
}

void SkipUnlessWave32(AgcDriver::VulkanDevice& device) {
    if (device.Target().subgroupSize < 32) Testing::Skip("subgroup size " + std::to_string(device.Target().subgroupSize) + " cannot hold a wave32");
}

const Testing::Case readFirstLaneWave32{"ReadFirstLane_Wave32_ReturnsFirstActiveLaneValue", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessWave32(device);
    Run(device, ReadFirstLaneWave32Code, 32);
    Check(32, 4);
}};

const Testing::Case readFirstLaneWave64{"ReadFirstLane_Wave64_ReturnsFirstActiveLaneValue", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessWave32(device);
    Run(device, ReadFirstLaneWave64Code, 64);
    Check(64, 40);
}};

} // namespace
