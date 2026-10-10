#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t WaveSize = 32;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 24> WaveCode{
    0x34020084, 0xd765000a, 0x000100c1, 0x3604009f, 0x7d880488, 0xbe880f6a, 0x7e160208, 0xd7600009,
    0x00010700, 0x7e180209, 0xbe9e037e, 0x7e1a0280, 0x7da80488, 0x7e1a0281, 0xbefe031e, 0xe0701000,
    0x80010a01, 0xe0701004, 0x80010b01, 0xe0701008, 0x80010c01, 0xe070100c, 0x80010d01, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), count * 4u, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(WaveCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {WaveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

ShaderRecompiler::SpirvTarget Wave32Target(AgcDriver::VulkanDevice& device) {
    auto target = device.ComputeTarget(WaveSize);
    if (target.subgroupSize < WaveSize) {
        Testing::Skip("the device runs wave32 programs on " + std::to_string(target.subgroupSize) + "-wide subgroups");
    }
    return target;
}

void Check() {
    constexpr std::array<const char*, Results> names{"v_mbcnt_lo_u32_b32", "s_bcnt1 of a VCC compare", "v_readlane_b32 lane 3", "v_cmpx EXEC"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t lane = tid % WaveSize;
        const std::array<std::uint32_t, Results> expected{lane, 8u, tid - lane + 3u, lane < 8u ? 1u : 0u};
        for (std::uint32_t j = 0; j < Results; ++j) {
            const auto actual = Output[tid * Results + j];
            Testing::Require(actual == expected[j], std::string("wave32 subgroup: thread ") + std::to_string(tid) + " " + names[j] + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[j]));
        }
    }
}

const Testing::Case wave32ComputeTarget{"Wave32Subgroup_Wave32ComputeTarget_MatchesPerWaveReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Wave32Target(device));
    Check();
}};

const Testing::Case defaultTarget{"Wave32Subgroup_DefaultTarget_MatchesPerWaveReference", [] {
    auto& device = SharedVulkanTestDevice();
    const auto target = Wave32Target(device);
    const auto defaultSize = device.Target().subgroupSize;
    if (defaultSize == target.subgroupSize) {
        Testing::Skip("the default target uses the same " + std::to_string(defaultSize) + "-wide subgroups as the wave32 compute target");
    }
    if (defaultSize < WaveSize) {
        Testing::Skip("the default target's " + std::to_string(defaultSize) + "-wide subgroups cannot hold a wave32");
    }
    Run(device, device.Target());
    Check();
}};

const Testing::Case sixtyFourWideSubgroups{"Wave32Subgroup_DefaultTargetWith64WideSubgroups_MatchesPerWaveReference", [] {
    auto& device = SharedVulkanTestDevice();
    static_cast<void>(Wave32Target(device));
    if (device.Target().subgroupSize != WaveSize) {
        Testing::Skip("the default target runs " + std::to_string(device.Target().subgroupSize) + "-wide subgroups, not 32-wide ones");
    }
    auto wide = device.Target();
    wide.subgroupSize = 64;
    Run(device, wide);
    Check();
}};

} // namespace
