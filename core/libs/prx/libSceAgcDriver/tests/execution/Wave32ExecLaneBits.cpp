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

constexpr std::uint32_t Threads = 32;
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 17> Code{
    0x340c0082u,
    0x7d88008du,
    0x02020080u,
    0xbe8a3c6au,
    0x4a0202c0u,
    0x887e0a7eu,
    0x7d820094u,
    0xbe8b3c6au,
    0x4a0202a0u,
    0xbefe030bu,
    0x7d82008au,
    0x7e0e0281u,
    0x02040e80u,
    0x4a020302u,
    0xe0701000u, 0x80000106u,
    0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::uint32_t Expected(std::uint32_t lane) {
    std::uint32_t value = lane < 13u ? lane + 64u : 0u;
    if (lane > 20u) value += 32u;
    if (lane > 10u) value += 1u;
    return value;
}

std::size_t Ballots(std::span<const std::uint32_t> words) {
    std::size_t count = 0;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpGroupNonUniformBallot) ++count;
    }
    return count;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Output.fill(0xdeadbeefu);
        std::vector<std::uint32_t> userData(4, 0u);
        const auto buffer = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(buffer.begin(), buffer.end(), userData.begin());
        const std::span<const std::uint32_t> code(Code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        const auto ballots = Ballots(result.spirv.Words());
        Require(ballots == 0u, "wave32 compares, saveexec and exec restores emitted " + std::to_string(ballots) + " subgroup ballots, expected none");
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            Require(Output[lane] == Expected(lane), "wave32 lane " + std::to_string(lane) + " is " + std::to_string(Output[lane]) + ", expected " + std::to_string(Expected(lane)));
        }
        std::puts("wave32 exec lane bits tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
