#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Width = 4;
constexpr std::uint32_t Type2D = 9;
alignas(256) std::array<std::uint32_t, 64 * Results> Output{};
alignas(256) std::array<std::uint32_t, 64> Texels{};
std::vector<std::string> Mismatches;

alignas(256) constexpr std::array<std::uint32_t, 29> Code{
    0x34060086u, 0x363c0083u, 0x7e3e0280u, 0x7e400280u, 0x7e440d1eu, 0x064444f0u, 0x104444ffu, 0x3e800000u,
    0x7e4602f0u,
    0xf0001f08u, 0x00010a1eu,
    0xf09c0f08u, 0x00610e22u,
    0xf0001108u, 0x0001121eu,
    0xf0041f08u, 0x0001131eu,
    0xf0381708u, 0x00011720u,
    0xbf8c3f70u,
    0xe0781000u, 0x80000a03u,
    0xe0781010u, 0x80000e03u,
    0xe0781020u, 0x80001203u,
    0xe0781030u, 0x80001603u,
    0xbf810000u,
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format, std::uint32_t swizzle) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | (1u << 31u),
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, std::uint32_t format, std::uint32_t swizzle, const std::string& run) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> output{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), waveSize * Results * 4u, 0x31016facu};
    const auto texture = TextureDescriptor(format, swizzle);
    const std::array<std::uint32_t, 4> sampler{0u, 0x00fff000u, 0u, 0u};
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span{Code})}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    constexpr const char* names[Results] = {
        "image_load.x", "image_load.y", "image_load.z", "image_load.w",
        "image_sample_lz.x", "image_sample_lz.y", "image_sample_lz.z", "image_sample_lz.w",
        "image_load dmask 1",
        "image_load_mip.x", "image_load_mip.y", "image_load_mip.z", "image_load_mip.w",
        "image_get_resinfo.x", "image_get_resinfo.y", "image_get_resinfo.z",
    };
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        for (std::uint32_t index = 0; index < Results; ++index) {
            const auto actual = Output[lane * Results + index];
            if (actual != 0u) {
                Mismatches.push_back(run + " FORMAT " + Hex(format) + " DST_SEL " + Hex(swizzle) + " lane " + std::to_string(lane) + " " + names[index] + " is " + Hex(actual) + ", expected 0");
            }
        }
    }
}

}

int main() {
    try {
        for (std::uint32_t index = 0; index < Texels.size(); ++index) Texels[index] = 0x41000000u + (index << 16u) + index;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto format : {0u, 0x1ffu}) {
            for (const auto swizzle : {0xfacu, 0u, 0xfffu, 0x924u, 0x6b5u}) {
                Run(*device, 32, device->Target(), format, swizzle, "wave32");
                Run(*device, 64, device->Target(), format, swizzle, "wave64");
                Run(*device, 64, device->ComputeTarget(32), format, swizzle, "wave64 split");
            }
        }
        for (const auto& mismatch : Mismatches) std::cerr << mismatch << '\n';
        Require(Mismatches.empty(), std::to_string(Mismatches.size()) + " unset image format mismatches");
        std::puts("image unset format tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
