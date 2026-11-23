#include "prx/libSceAgcDriver/Execution/include/Float32Exp2Probe.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <vector>

namespace AgcDriver {
namespace {

constexpr std::uint32_t Threads = 256;
constexpr std::uint32_t InputStride = 4;
constexpr std::uint32_t OutputStride = 16;
constexpr std::uint32_t Count = 254;
alignas(256) std::array<std::uint32_t, Threads * InputStride> Input{};
alignas(256) std::array<std::uint32_t, Threads * OutputStride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 9> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x7e144b04u, 0xe0781000u, 0x80010a02u, 0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

}

Float32Exp2ProbeResult ProbeFloat32HostExp2(VulkanDevice& device, const ShaderRecompiler::SpirvTarget& deviceTarget, bool host) {
    Float32Exp2ProbeResult result;
    for (std::uint32_t index = 0; index < Count; ++index) Input[index * InputStride] = std::bit_cast<std::uint32_t>(static_cast<float>(static_cast<int>(index) - 126));
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    auto target = deviceTarget;
    target.float32HostExp2 = host;
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto shader = ShaderRecompiler::Recompile(request);
    device.Dispatch(shader, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    for (std::uint32_t index = 0; index < Count; ++index) {
        ++result.compared;
        if (Output[index * OutputStride] != ((index + 1u) << 23u)) ++result.mismatches;
    }
    return result;
}

}
