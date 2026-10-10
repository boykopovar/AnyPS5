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

constexpr std::uint32_t Threads = 64;
alignas(256) std::array<std::uint32_t, 4> Input{3u, 5u, 0u, 0u};
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 22> WaveCode{
    0xf4240200, 0xfa000000, 0xbf8cc07f, 0xbe8a0380, 0xbe8b0380, 0x800a080a, 0x930c0b0a, 0x800a0c0a,
    0x800b810b, 0xbf0a090b, 0xbf85fffa, 0x7d880094, 0xbe8d106a, 0x4a060087, 0x7e1c0503, 0xd5430001,
    0x00281b00, 0x4a02020e, 0x34040082, 0xe0701000, 0x80010102, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), count * 4u, 0x31016facu};
}

std::uint32_t Accumulated() {
    std::uint32_t sum = 0;
    for (std::uint32_t count = 0; count < Input[1]; ++count) {
        sum += Input[0];
        sum += sum * count;
    }
    return sum;
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(WaveCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    const auto uniform = Accumulated() + 7u;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = tid * 20u + uniform;
        Testing::Require(Output[tid] == expected, "wave uniform: thread " + std::to_string(tid) + " stored " + std::to_string(Output[tid]) + ", expected " + std::to_string(expected));
    }
}

const Testing::Case uniformLoop{"WaveUniform_Wave64ScalarLoop_StoresUniformValuePlusLaneOffset", [] {
    auto& device = SharedVulkanTestDevice();
    if (device.Target().subgroupSize < 32u) {
        Testing::Skip("subgroup size " + std::to_string(device.Target().subgroupSize) + " cannot hold a wave64 in two lanes");
    }
    Run(device);
    Check();
}};

} // namespace
