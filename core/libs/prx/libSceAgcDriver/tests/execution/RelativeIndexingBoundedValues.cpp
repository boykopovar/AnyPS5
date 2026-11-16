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
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Outputs = 16;
constexpr std::uint32_t Results = 3;

alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Outputs> Output{};

alignas(256) constexpr std::array<std::uint32_t, 38> Code{
    0x34020084, 0x34060086, 0x34040088, 0xe0381000, 0x80000401, 0xf4280200, 0xfa000000, 0x4a1404ff,
    0x00001000, 0x4a1604ff, 0x00001001, 0x4a1804ff, 0x00001002, 0x4a1a04ff, 0x00001003, 0x4a1c04ff,
    0x00001004, 0x4a1e04ff, 0x00001005, 0x4a2004ff, 0x00001006, 0x4a2204ff, 0x00001007, 0xbf8c0070,
    0x877c8308, 0x7e3c870a, 0x7e3e870c, 0x870c8108, 0x8f0c820c, 0x887c810c, 0x7e40870a, 0xe0701000,
    0x80011e03, 0xe0701004, 0x80011f03, 0xe0701008, 0x80012003, 0xbf810000,
};

constexpr const char* Names[Results] = {"movrels_masked", "movrels_masked_offset", "movrels_masked_shifted_or"};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads, std::uint32_t selector) {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < threads; ++tid) Input[tid * Inputs] = selector;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
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

void Check(std::uint32_t waveSize, std::uint32_t threads, std::uint32_t selector) {
    const std::array<std::uint32_t, Results> slots{selector & 3u, (selector & 3u) + 2u, ((selector & 1u) << 2u) | 1u};
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        for (std::uint32_t index = 0; index < Results; ++index) {
            const auto actual = Output[tid * Outputs + index];
            const auto expected = 0x1000u + slots[index] + (tid << 8u);
            Require(actual == expected, std::string("relative indexing with bounded M0 values: wave") + std::to_string(waveSize) + " selector " + std::to_string(selector) + " lane " + std::to_string(tid) + " " + Names[index] + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("relative indexing with bounded M0 values skipped, subgroup size %u holds less than a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        for (const std::uint32_t selector : {0u, 1u, 2u, 3u, 6u, 0xfffffffdu}) {
            Run(*device, 32, 32, selector);
            Check(32, 32, selector);
            Run(*device, 64, 64, selector);
            Check(64, 64, selector);
        }
        std::puts("relative indexing with bounded M0 values tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
