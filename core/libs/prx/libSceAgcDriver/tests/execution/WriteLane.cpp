#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Stride = 4;
alignas(256) std::array<std::uint32_t, Threads * Stride> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 32> WriteLaneCode{
    0x34020082, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0x7e180300, 0xb0151234, 0xbefe0381, 0xd761000c,
    0x00014a15, 0xbefe03c1, 0xbf8c3f70, 0xbe940380, 0xd7600008, 0x00002904, 0xd7600009, 0x00002905, 0x8096149f,
    0xd761000a, 0x00002c08, 0xd761000b, 0x00002809, 0x80148114, 0xbf0aa014, 0xbf85fff4, 0xe0702000,
    0x80010a01, 0xe0702004, 0x80010b01, 0xe0702008, 0x80010c01, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Stride] = 0xa0000000u + tid * 0x01010101u;
        Input[tid * Stride + 1] = 0x50000000u ^ (tid * 0x00102030u);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(WriteLaneCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 3> expected{
            Input[(Threads - 1u - tid) * Stride],
            Input[tid * Stride + 1],
            tid == 5u ? 0x1234u : tid,
        };
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const auto actual = Output[tid * Stride + j];
            Testing::Require(actual == expected[j], "write lane: lane " + std::to_string(tid) + " result " + std::to_string(j) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[j]));
        }
    }
}

const Testing::Case writeLane{"WriteLane_Wave32_WritesSelectedLanesOnly", [] {
    auto& device = SharedVulkanTestDevice();
    if (device.Target().subgroupSize < Threads) {
        Testing::Skip("subgroup size " + std::to_string(device.Target().subgroupSize) + " cannot hold a wave32");
    }
    Run(device);
    Check();
}};

} // namespace
