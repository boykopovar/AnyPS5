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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Stride = 4;
constexpr std::uint32_t SelectorRegister = 8;
alignas(256) std::array<std::uint32_t, Threads * Stride> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 21> Code{
    0x34020082, 0xe0302000, 0x80000401, 0xbf8c3f70, 0xbe890380, 0xbf068008, 0xbf85000a, 0x4a080881, 0x80098109, 0xbf060809, 0xbf850006,
    0xbf0a8409, 0xbf85fffa, 0x4a0808c0, 0xe0702000, 0x80010401, 0xbf810000, 0x4a0808a0, 0xe0702000, 0x80010401, 0xbf810000,
};

struct Case {
    const char* name;
    std::uint32_t selector;
    std::uint32_t added;
};

const std::array<Case, 5> Cases{{
    {"early exit before the loop", 0u, 32u},
    {"early exit from the first iteration", 1u, 33u},
    {"early exit from the third iteration", 3u, 35u},
    {"early exit from the last iteration", 4u, 36u},
    {"loop exit to the end of the program", 5u, 68u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x01016facu};
}

void Run(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t waveSize) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid * Stride] = tid * 0x01010101u + 7u;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(SelectorRegister + 1u, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[SelectorRegister] = test.selector;
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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

void Check(const Case& test, std::uint32_t waveSize) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Input[tid * Stride] + test.added;
        const auto actual = Output[tid * Stride];
        Require(actual == expected, "loop ending exits wave" + std::to_string(waveSize) + ": " + test.name + ": thread " + std::to_string(tid) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto waveSize : {32u, 64u}) {
            for (const auto& test : Cases) {
                Run(*device, test, waveSize);
                Check(test, waveSize);
            }
        }
        std::puts("loop ending exits tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
