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

constexpr std::uint32_t Waves = 8;
constexpr std::uint32_t Rounds = 16;
constexpr std::uint32_t Groups = 16;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, Waves * 64u> Output{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t wave, const ShaderRecompiler::SpirvTarget& target) {
    const std::uint32_t threads = Waves * wave;
    const std::uint32_t live = threads / 2u;
    alignas(256) const std::array<std::uint32_t, 17> code{
        0x34040082u, 0x4a020081u, 0xbf8a0000u, 0x7da800ffu, live, 0xbf88000au, 0xd8340000u, 0x00000102u,
        0xbf8cc07fu, 0xbf8a0000u, 0xd8d80000u, 0x03000002u, 0xbf8cc07fu, 0xbf8a0000u, 0xe0701000u, 0x80000302u,
        0xbf810000u,
    };
    std::vector<std::uint32_t> userData(4, 0u);
    const auto output = BufferDescriptor(Output.data(), threads * 4u);
    std::copy(output.begin(), output.end(), userData.begin());
    const std::span<const std::uint32_t> program(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(program.data()), std::as_bytes(program)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, threads, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(program.data()), program, 0, {}},
        {wave, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    for (std::uint32_t round = 0; round < Rounds; ++round) {
        Output.fill(Untouched);
        device.Dispatch(result, Groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(program.data()));
        device.WaitIdle();
        for (std::uint32_t thread = 0; thread < threads; ++thread) {
            const std::uint32_t expected = thread < live ? thread + 1u : Untouched;
            Require(Output[thread] == expected, "ended wave barrier, wave" + std::to_string(wave) + " round " + std::to_string(round) + ": thread " + std::to_string(thread) + " (wave " + std::to_string(thread / wave) + ") holds " + std::to_string(Output[thread]) + ", expected " + std::to_string(expected));
        }
    }
    std::printf("ended wave barrier, wave%u: %u live waves passed two barriers after %u waves ended\n", wave, Waves / 2u, Waves / 2u);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto wave32 = device->ComputeTarget(32);
        if (wave32.subgroupSize == 32u) Run(*device, 32, wave32);
        else std::printf("wave32 skipped, wave32 programs do not run on 32-lane subgroups (%u lanes)\n", wave32.subgroupSize);
        Run(*device, 64, device->ComputeTarget(64));
        std::puts("ended wave barrier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
