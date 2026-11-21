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
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 16;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Groups = Width * Height / Threads;
constexpr std::uint32_t MaxPitch = 256;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t SwizzleXYZW = 0xfacu;
constexpr std::uint32_t Mark = 0xa0000000u;

alignas(256) std::array<std::uint32_t, Width * Height * 4> Output{};
alignas(256) std::array<std::uint32_t, MaxPitch * Height> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 16> LoadXy{
    0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u, 0x2c3e3c84u, 0x363c3c8fu, 0x7e140280u, 0x7e160280u,
    0x7e180280u, 0x7e1a0280u, 0xf0001f08u, 0x00010a1eu, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t depth) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        SwizzleXYZW | (Type2D << 28u),
        depth,
        0u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t depth) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> output{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu};
    const auto texture = TextureDescriptor(depth);
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code = LoadXy;
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
    device.Dispatch(result, Groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(AgcDriver::VulkanDevice& device, std::uint32_t depth, std::uint32_t pitch) {
    Run(device, depth);
    for (std::uint32_t index = 0; index < Width * Height; ++index) {
        const auto x = index % Width;
        const auto y = index / Width;
        const auto expected = Mark | (y * pitch + x);
        const auto actual = Output[index * 4u];
        Require(actual == expected, "linear 2D texture with DEPTH " + std::to_string(depth) + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + Hex(actual) + ", expected " + Hex(expected) + " (row pitch " + std::to_string(pitch) + ")");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t index = 0; index < Texels.size(); ++index) Texels[index] = Mark | index;
        for (const auto& [depth, pitch] : std::array<std::pair<std::uint32_t, std::uint32_t>, 6>{{{15u, 64u}, {63u, 64u}, {64u, 128u}, {127u, 128u}, {128u, 192u}, {255u, 256u}}}) {
            Check(*device, depth, pitch);
        }
        std::puts("linear texture pitch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
