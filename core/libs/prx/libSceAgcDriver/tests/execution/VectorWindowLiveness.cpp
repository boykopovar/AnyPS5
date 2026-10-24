#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};
alignas(256) std::array<std::uint32_t, 4> Unused{};

alignas(256) constexpr std::array<std::uint32_t, 22> Code{
    0x34060084, 0x4a080081, 0xbf068008, 0xbf850001, 0x4a080890, 0xbefc0380, 0x7e028704, 0xe0701000,
    0x80010103, 0x4a080081, 0xbe890383, 0x7e0a0280, 0xbefc0380, 0x7e0c8704, 0x4a0a0b06, 0x4a080890,
    0x80898109, 0xbf078009, 0xbf85fff9, 0xe0701004, 0x80010503, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads, std::uint32_t overwrite) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(9, 0u);
    const auto input = BufferDescriptor(Unused.data(), static_cast<std::uint32_t>(Unused.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[8] = overwrite;
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
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

void Check(std::uint32_t waveSize, std::uint32_t threads, std::uint32_t overwrite) {
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        const std::uint32_t written = tid + 1u;
        const std::uint32_t branch = overwrite != 0u ? written + 16u : written;
        const std::uint32_t loop = 3u * written + 48u;
        const auto where = std::string("vector window liveness: wave") + std::to_string(waveSize) + " lane " + std::to_string(tid) + (overwrite != 0u ? " (overwritten before the read)" : " (overwrite branched over)");
        Require(out[0] == branch, where + ": the window register written before the branch read " + Hex(out[0]) + ", expected " + Hex(branch));
        Require(out[1] == loop, where + ": the window register carried around the loop read " + Hex(out[1]) + ", expected " + Hex(loop));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const std::uint32_t waveSize : {32u, 64u}) {
            for (const std::uint32_t overwrite : {0u, 1u}) {
                Run(*device, waveSize, waveSize, overwrite);
                Check(waveSize, waveSize, overwrite);
            }
        }
        std::puts("vector window liveness tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
