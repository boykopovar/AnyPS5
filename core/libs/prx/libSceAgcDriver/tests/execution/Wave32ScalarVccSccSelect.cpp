#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {

constexpr std::uint32_t WaveSize = 32;
constexpr std::uint32_t Threads = 2 * WaveSize;
alignas(256) std::array<std::uint32_t, 4 * Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 23> Code{
    0x3604009fu,
    0x7e0e0281u,
    0x7d840480u,
    0x02100e80u,
    0xbe88036au,
    0xbf078008u,
    0xbe8e03ffu, 0x000000ffu,
    0x850a0ec1u,
    0xbeea030au,
    0x02120e80u,
    0x7e14020au,
    0x7e160208u,
    0x340c0082u,
    0xe0702000u, 0x80000806u,
    0xe0702004u, 0x80000906u,
    0xe0702008u, 0x80000a06u,
    0xe070200cu, 0x80000b06u,
    0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(std::span<std::uint32_t> values) {
    const auto address = reinterpret_cast<std::uintptr_t>(values.data());
    return {static_cast<std::uint32_t>(address),
            static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u),
            static_cast<std::uint32_t>(values.size()), 0x11016facu};
}

bool Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    const auto userData = BufferDescriptor(Output);
    const std::span<const std::uint32_t> code(Code);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{codeAddress, std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, codeAddress, code, 0, {}},
        {WaveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128},
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, codeAddress);
    device.WaitIdle();

    bool passed = true;
    for (std::uint32_t wave = 0; wave < Threads / WaveSize; ++wave) {
        std::uint32_t compare = 0, selected = 0, compareMiss = 0, selectedMiss = 0;
        std::uint32_t ballotOne = 0, ballotZero = 0, wordFull = 0, wordLowByte = 0;
        for (std::uint32_t lane = 0; lane < WaveSize; ++lane) {
            const auto offset = 4 * (wave * WaveSize + lane);
            if (Output[offset] == 1u) compare |= 1u << lane;
            if (Output[offset + 1] == 1u) selected |= 1u << lane;
            if (Output[offset] != (lane == 0 ? 1u : 0u)) ++compareMiss;
            if (Output[offset + 1] != 1u) ++selectedMiss;
            if (Output[offset + 2] == 0xffffffffu) ++wordFull;
            if (Output[offset + 2] == 0xffu) ++wordLowByte;
            if (Output[offset + 3] == 1u) ++ballotOne;
            if (Output[offset + 3] == 0u) ++ballotZero;
        }
        std::printf("wave=%u compare=0x%08x selected=0x%08x compare_miss=%u selected_miss=%u "
                    "ballot_one=%u ballot_zero=%u select_full=%u select_ff=%u\n",
                    wave, compare, selected, compareMiss, selectedMiss, ballotOne, ballotZero, wordFull, wordLowByte);
        if (compareMiss || selectedMiss || wordFull + wordLowByte != WaveSize || ballotOne + ballotZero != WaveSize) passed = false;
    }
    return passed;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        return Run(*device) ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
