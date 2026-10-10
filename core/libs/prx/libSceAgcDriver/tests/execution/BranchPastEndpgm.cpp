#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Stride = 4;
constexpr std::uint32_t SelectorRegister = 8;
alignas(256) std::array<std::uint32_t, Threads * Stride> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 20> TailBlockCode{
    0x34020082, 0xe0302000, 0x80000401, 0xbf8c3f70, 0xbf068008, 0xbf840008, 0x4a080881, 0xe0702000, 0x80010401, 0xbf810000,
    0x4a080882, 0xe0702000, 0x80010401, 0xbf810000, 0xbf068108, 0xbf85fffa, 0x4a080883, 0xe0702000, 0x80010401, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> LoopBodyCode{
    0x34020082, 0xe0302000, 0x80000401, 0xbf8c3f70, 0xbe890380, 0xbf068008, 0xbf840006, 0x4a0808c0, 0xe0702000,
    0x80010401, 0xbf810000, 0x4a080881, 0x80098109, 0xbf0a0809, 0xbf85fffc, 0xe0702000, 0x80010401, 0xbf810000,
};

struct Case {
    const char* name;
    std::span<const std::uint32_t> code;
    std::uint32_t selector;
    std::uint32_t added;
};

const std::array<Case, 6> Cases{{
    {"return before the tail blocks", TailBlockCode, 0u, 1u},
    {"tail block entered from the tail block after it", TailBlockCode, 1u, 2u},
    {"last tail block", TailBlockCode, 2u, 3u},
    {"return before the loop", LoopBodyCode, 0u, 64u},
    {"loop body entered once from the test after it", LoopBodyCode, 1u, 1u},
    {"loop body entered three times from the test after it", LoopBodyCode, 3u, 3u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x01016facu};
}

void Run(AgcDriver::VulkanDevice& device, const Case& test) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid * Stride] = tid * 0x01010101u + 7u;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(SelectorRegister + 1u, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[SelectorRegister] = test.selector;
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(test.code.data()), std::as_bytes(test.code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(test.code.data()), test.code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(test.code.data()));
    device.WaitIdle();
}

void Check(const Case& test) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Input[tid * Stride] + test.added;
        const auto actual = Output[tid * Stride];
        Require(actual == expected, std::string("branch past endpgm: ") + test.name + ": thread " + std::to_string(tid) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
    }
}

void RunCases(std::span<const std::uint32_t> code) {
    auto& device = SharedVulkanTestDevice();
    for (const auto& test : Cases) {
        if (test.code.data() != code.data()) continue;
        Run(device, test);
        Check(test);
    }
}

const Testing::Case tailBlocks{"BranchPastEndpgm_TailBlocks_AddSelectedIncrement", [] {
    RunCases(TailBlockCode);
}};

const Testing::Case loopBody{"BranchPastEndpgm_LoopBody_AddsOncePerIteration", [] {
    RunCases(LoopBodyCode);
}};

} // namespace
