#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t WaveSize = 32;
constexpr std::uint32_t Waves = 2;
constexpr std::uint32_t Threads = WaveSize * Waves;
constexpr std::uint32_t Stride = 4;
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 16> ReadFirstLaneWave32Code{
    0x34020082, 0x4a0400ff, 0x00000100, 0xbe94037e, 0xbefe0380, 0x7e100502, 0xbefe03b0, 0x7e120502,
    0xbefe0314, 0x7e060208, 0x7e080209, 0xe0702000, 0x80010301, 0xe0702004, 0x80010401, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(ReadFirstLaneWave32Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {WaveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t subgroupSize) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto wave = tid / WaveSize;
        const auto first = 0x100u + wave * WaveSize;
        const std::array<std::uint32_t, 2> expected{first, first + 4u};
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const auto actual = Output[tid * Stride + j];
            Require(actual == expected[j], "read first lane, " + std::to_string(Waves) + " wave32s on a " + std::to_string(subgroupSize) + "-lane subgroup: wave " + std::to_string(wave) + " lane " + std::to_string(tid % WaveSize) + " result " + std::to_string(j) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[j]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto subgroupSize = device->Target().subgroupSize;
        if (subgroupSize < WaveSize) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", subgroupSize);
            return VulkanTestSkipped;
        }
        Run(*device);
        Check(subgroupSize);
        std::printf("read first lane tests passed: %u wave32s in one workgroup on a %u-lane subgroup\n", Waves, subgroupSize);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
