#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
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

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Stride = 4;
alignas(256) std::array<std::uint32_t, MaxThreads * Stride> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Stride> Output{};

std::array<std::uint32_t, 17> Code(std::uint32_t selectors) {
    const auto dpp8 = [&](std::uint32_t source) { return (selectors << 8u) | source; };
    return {
        0x34080084u,
        0xe03c1000u, 0x80000104u,
        0xbf8c3f70u,
        0x7e140301u,
        0x781406e9u, dpp8(2u),
        0x7e1a02e9u, dpp8(2u),
        0x7e160301u,
        0x7816070du,
        0x7e180301u,
        0x78180702u,
        0xe07c1000u, 0x80010a04u,
        0xbf810000u,
    };
}

std::uint32_t Selectors(const std::array<std::uint32_t, 8>& lanes) {
    std::uint32_t value = 0;
    for (std::uint32_t lane = 0; lane < 8u; ++lane) value |= lanes[lane] << (lane * 3u);
    return value;
}

std::uint16_t Half(std::uint32_t seed) {
    const auto sign = (seed >> 15u) & 1u;
    const auto exponent = 10u + seed % 10u;
    const auto mantissa = (seed >> 4u) & 0x3ffu;
    return static_cast<std::uint16_t>((sign << 15u) | (exponent << 10u) | mantissa);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(bytes), 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t wave, const std::array<std::uint32_t, 8>& lanes, const std::string& what) {
    for (std::uint32_t lane = 0; lane < MaxThreads; ++lane) {
        for (std::uint32_t word = 0; word < 3u; ++word) {
            const auto seed = lane * 2654435761u + word * 40503u + 12345u;
            Input[lane * Stride + word] = static_cast<std::uint32_t>(Half(seed)) | (static_cast<std::uint32_t>(Half(seed * 7u + 3u)) << 16u);
        }
    }
    Output.fill(0xdeadbeefu);
    const auto code = Code(Selectors(lanes));
    std::vector<std::uint32_t> userData;
    for (const auto& descriptor : {BufferDescriptor(Input.data(), sizeof(Input)), BufferDescriptor(Output.data(), sizeof(Output))}) {
        userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    }
    const std::span<const std::uint32_t> program(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(program)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{wave, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), program, 0, {}},
        {wave, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(wave),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    bool permuted = false;
    for (std::uint32_t lane = 0; lane < wave; ++lane) {
        const auto fused = Output[lane * Stride];
        const auto separate = Output[lane * Stride + 1u];
        const auto plain = Output[lane * Stride + 2u];
        Require(fused == separate, what + " (wave" + std::to_string(wave) + "): lane " + std::to_string(lane) + " v_pk_fmac_f16 dpp8 gave " + std::to_string(fused) +
            ", v_mov_b32 dpp8 then v_pk_fmac_f16 gave " + std::to_string(separate));
        permuted = permuted || fused != plain;
    }
    Require(permuted, what + " (wave" + std::to_string(wave) + "): the dpp8 result equals the unpermuted one in every lane");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const std::uint32_t wave : {32u, 64u}) {
            Run(*device, wave, {7, 6, 5, 4, 3, 2, 1, 0}, "reversed groups of eight");
            Run(*device, wave, {1, 0, 3, 2, 5, 4, 7, 6}, "swapped neighbours");
        }
        std::puts("dpp8 v_pk_fmac_f16 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
