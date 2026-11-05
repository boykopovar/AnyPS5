#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 4;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Unorm8_8_8_8 = 56;
constexpr std::uint32_t Uint8_8_8_8 = 60;
constexpr std::uint32_t SwizzleXYZW = 0xfacu;
constexpr std::uint32_t One = 0x3f800000u;
constexpr std::uint32_t ClampBorder = 0x1b6u;
constexpr std::uint32_t AnisoOverride = (1u << 29u) | (2u << 22u) | (2u << 20u);

alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(256) std::array<std::uint32_t, Width> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 9> SampleOutside{
    0x34060084u, 0x7e2802f4u, 0x7e2a02f4u, 0xf09c0f08u, 0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u,
    0xbf810000u,
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        SwizzleXYZW | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t format, std::uint32_t borderType, std::uint32_t filters) {
    Output.fill(0xdeadbeefu);
    Texels.fill(0u);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> output{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu};
    const auto texture = TextureDescriptor(format);
    const std::array<std::uint32_t, 4> sampler{ClampBorder, 0x00fff000u, filters, borderType << 30u};
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(SampleOutside);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
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

void Check(AgcDriver::VulkanDevice& device, std::uint32_t format, std::uint32_t borderType, const std::array<std::uint32_t, 4>& expected, const std::string& what, std::uint32_t filters = 0u) {
    Run(device, format, borderType, filters);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto actual = Output[thread * 4u + component];
            Require(actual == expected[component], what + ": thread " + std::to_string(thread) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(expected[component]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device, Unorm8_8_8_8, 0u, {0u, 0u, 0u, 0u}, "8_8_8_8_UNORM transparent black border");
        Check(*device, Unorm8_8_8_8, 1u, {0u, 0u, 0u, One}, "8_8_8_8_UNORM opaque black border");
        Check(*device, Unorm8_8_8_8, 2u, {One, One, One, One}, "8_8_8_8_UNORM opaque white border");
        Check(*device, Uint8_8_8_8, 0u, {0u, 0u, 0u, 0u}, "8_8_8_8_UINT transparent black border");
        Check(*device, Uint8_8_8_8, 1u, {0u, 0u, 0u, 1u}, "8_8_8_8_UINT opaque black border");
        Check(*device, Uint8_8_8_8, 2u, {1u, 1u, 1u, 1u}, "8_8_8_8_UINT opaque white border");
        Check(*device, Unorm8_8_8_8, 2u, {One, One, One, One}, "8_8_8_8_UNORM opaque white border under ANISO_OVERRIDE", AnisoOverride);
        Check(*device, Uint8_8_8_8, 2u, {1u, 1u, 1u, 1u}, "8_8_8_8_UINT opaque white border under ANISO_OVERRIDE", AnisoOverride);
        std::puts("image sample border color tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
