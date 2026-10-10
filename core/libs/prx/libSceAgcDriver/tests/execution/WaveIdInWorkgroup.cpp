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

constexpr std::uint32_t MaxThreads = 128;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, MaxThreads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 10> WaveIdCode{
    0x34060283, 0x4a060700, 0x34080486, 0x4a060704, 0xf4a80100, 0x00000000, 0x7e080204, 0xe0702000,
    0x80000403, 0xbf810000,
};

struct Workgroup {
    std::array<std::uint32_t, 3> threads;
    std::uint32_t waveSize;

    [[nodiscard]] std::uint32_t ThreadCount() const {
        return threads[0] * threads[1] * threads[2];
    }
    [[nodiscard]] std::string Name() const {
        return "wave" + std::to_string(waveSize) + " " + std::to_string(threads[0]) + "x" + std::to_string(threads[1]) + "x" + std::to_string(threads[2]);
    }
};

constexpr std::array<Workgroup, 6> Workgroups{{
    {{128, 1, 1}, 32}, {{128, 1, 1}, 64}, {{8, 8, 2}, 32}, {{8, 8, 2}, 64}, {{8, 13, 1}, 32}, {{8, 13, 1}, 64},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x01016facu};
}

void Run(AgcDriver::VulkanDevice& device, const Workgroup& workgroup) {
    Output.fill(Untouched);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    const std::vector<std::uint32_t> userData(output.begin(), output.end());
    const std::span<const std::uint32_t> code(WaveIdCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{workgroup.threads, 0, {false, false, false}, false, 3};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {workgroup.waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const Workgroup& workgroup) {
    for (std::uint32_t tid = 0; tid < MaxThreads; ++tid) {
        const auto expected = tid < workgroup.ThreadCount() ? tid / workgroup.waveSize : Untouched;
        const auto actual = Output[tid];
        Testing::Require(actual == expected, "wave id in workgroup: " + workgroup.Name() + " thread " + std::to_string(tid) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
    }
}

template<typename TAction>
std::string RequireRefusal(TAction action, const std::string& message) {
    try {
        action();
    } catch (const std::exception& error) {
        return error.what();
    }
    Testing::Fail(message);
}

void CheckRejectedOutsideCompute(const AgcDriver::VulkanDevice& device) {
    alignas(256) static constexpr std::array<std::uint32_t, 3> vertexCode{0xf4a80100, 0x00000000, 0xbf810000};
    const std::vector<std::uint32_t> userData(8, 0u);
    const std::span<const std::uint32_t> code(vertexCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto refusal = RequireRefusal([&] { static_cast<void>(ShaderRecompiler::Recompile(request)); }, "wave id in workgroup: a vertex shader was not rejected");
    Testing::Require(refusal.find("s_get_waveid_in_workgroup is supported only in compute shaders") != std::string::npos, "wave id in workgroup: a vertex shader was not rejected, got '" + refusal + "'");
}

const Testing::Case wave32Linear{"WaveIdInWorkgroup_Wave32Linear128_NumbersWavesInOrder", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Workgroups[0]);
    Check(Workgroups[0]);
}};

const Testing::Case wave64Linear{"WaveIdInWorkgroup_Wave64Linear128_NumbersWavesInOrder", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Workgroups[1]);
    Check(Workgroups[1]);
}};

const Testing::Case wave32Grid{"WaveIdInWorkgroup_Wave32Grid8x8x2_NumbersWavesInOrder", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Workgroups[2]);
    Check(Workgroups[2]);
}};

const Testing::Case wave64Grid{"WaveIdInWorkgroup_Wave64Grid8x8x2_NumbersWavesInOrder", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Workgroups[3]);
    Check(Workgroups[3]);
}};

const Testing::Case wave32Partial{"WaveIdInWorkgroup_Wave32PartialWave8x13_NumbersWavesInOrder", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Workgroups[4]);
    Check(Workgroups[4]);
}};

const Testing::Case wave64Partial{"WaveIdInWorkgroup_Wave64PartialWave8x13_NumbersWavesInOrder", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Workgroups[5]);
    Check(Workgroups[5]);
}};

const Testing::Case vertexStage{"WaveIdInWorkgroup_VertexShader_IsRejected", [] {
    CheckRejectedOutsideCompute(SharedVulkanTestDevice());
}};

} // namespace
