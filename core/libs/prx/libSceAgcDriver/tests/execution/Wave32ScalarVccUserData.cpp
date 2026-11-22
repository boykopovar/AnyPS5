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

namespace {

constexpr std::uint32_t WaveSize = 32;
constexpr std::uint32_t Threads = 2 * WaveSize;
constexpr std::uint32_t CompareMask = 0x7b36c4a9u;
constexpr std::array<std::array<std::uint32_t, 3>, 2> UserWords{{
    {0xa9673c4bu, 0xc35a6d92u, 0xf7edb6dbu},
    {0xd8415e93u, 0x2e943bc5u, 0xdebf9cd7u},
}};
constexpr std::array<const char*, 3> Paths{"userdata_s4", "userdata_xor_and", "xor_ballot_guard"};
using Buffer = std::array<std::uint32_t, 4 * Threads>;
alignas(256) std::array<Buffer, 2> Outputs{};
alignas(256) constexpr std::array<std::uint32_t, 29> Code{
    0x7e0e0281u,
    0xbeea0304u,
    0x02100e80u,
    0x89080504u,
    0x87080608u,
    0xbeea0308u,
    0x02120e80u,
    0x7e160208u,
    0x3604009fu,
    0x7e0602ffu, 0x7b36c4a9u,
    0x2c080702u,
    0x36080881u,
    0x7d8a0880u,
    0xbe8a036au,
    0x890c800au,
    0xbeea0380u,
    0xbeea030cu,
    0x02140e80u,
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

bool Run(AgcDriver::VulkanDevice& device, std::uint32_t round) {
    auto& outputWords = Outputs[round];
    outputWords.fill(0xdeadbeefu);
    std::array<std::uint32_t, 7> userData{};
    const auto descriptor = BufferDescriptor(outputWords);
    std::copy(descriptor.begin(), descriptor.end(), userData.begin());
    std::copy(UserWords[round].begin(), UserWords[round].end(), userData.begin() + 4);
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
    const auto rawWord = (userData[4] ^ userData[5]) & userData[6];
    const std::array<std::uint32_t, 3> expected{userData[4], rawWord, CompareMask};
    for (std::uint32_t wave = 0; wave < Threads / WaveSize; ++wave) {
        std::array<std::uint32_t, 3> observed{}, mismatches{};
        std::uint32_t scalarMismatches = 0;
        for (std::uint32_t lane = 0; lane < WaveSize; ++lane) {
            const auto offset = 4 * (wave * WaveSize + lane);
            if (outputWords[offset + 3] != rawWord) ++scalarMismatches;
            for (std::size_t path = 0; path < Paths.size(); ++path) {
                const auto actual = outputWords[offset + path];
                if (actual == 1u) observed[path] |= 1u << lane;
                if (actual != ((expected[path] >> lane) & 1u)) ++mismatches[path];
            }
        }
        std::printf("round=%u wave=%u xor_and_word=0x%08x scalar_mismatches=%u\n", round, wave, rawWord, scalarMismatches);
        if (scalarMismatches) passed = false;
        for (std::size_t path = 0; path < Paths.size(); ++path) {
            std::printf("round=%u wave=%u path=%s expected=0x%08x selected=0x%08x mismatches=%u\n",
                        round, wave, Paths[path], expected[path], observed[path], mismatches[path]);
            if (mismatches[path]) passed = false;
        }
    }
    return passed;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool first = Run(*device, 0);
        const bool second = Run(*device, 1);
        return first && second ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
