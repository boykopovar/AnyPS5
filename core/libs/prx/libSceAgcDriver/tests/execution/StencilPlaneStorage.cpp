#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Texels = Width * Height;
constexpr std::uint32_t Uint8 = 5;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t SwizzleX111 = 0x24cu;
constexpr std::uint8_t Reference = 0x5a;

alignas(4096) std::array<float, Width * Height * 2> Depth{};
alignas(4096) std::array<std::uint8_t, Width * Height * 2> Stencil{};
alignas(256) std::array<std::uint32_t, Texels * 4> Output{};

constexpr std::uint8_t Stored = 42;

alignas(256) constexpr std::array<std::uint32_t, 17> LoadX{
    0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u, 0x2c3e3c86u, 0x363c3cffu, 0x0000003fu, 0x7e140280u,
    0x7e160280u, 0x7e180280u, 0x7e1a0280u, 0xf0001108u, 0x00010a1eu, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 15> StoreX{
    0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x2c3e3c86u, 0x363c3cffu, 0x0000003fu, 0x7e1402aau, 0x7e160280u,
    0x7e180280u, 0x7e1a0280u, 0xf0201108u, 0x00010a1eu, 0xbf8c3f70u, 0xbf800000u, 0xbf810000u,
};

std::array<std::uint32_t, 4> OutputDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu};
}

std::array<std::uint32_t, 8> StencilDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Stencil.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Uint8 << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        SwizzleX111 | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto output = OutputDescriptor();
    const auto texture = StencilDescriptor();
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, Texels / Threads, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = Compile(*device);
        const Surface surface{reinterpret_cast<std::uintptr_t>(Depth.data()), reinterpret_cast<std::uintptr_t>(Stencil.data()), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT};
        const VkStencilOpState replace{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, Reference};
        Require(Draw(*device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = replace}) == Covered, "the stencil draw did not cover the target");
        Run(*device, LoadX);
        for (std::uint32_t texel = 0; texel < Texels; ++texel) {
            const auto actual = Output[texel * 4u];
            Require(actual == Reference && Output[texel * 4u + 1u] == 0u, "image_load of the stencil plane returned " + std::to_string(actual) + " at texel " + std::to_string(texel) + ", expected the stencil the draw wrote (" + std::to_string(Reference) + ")");
        }
        Run(*device, StoreX);
        const auto equal = [](std::uint8_t value) { return VkStencilOpState{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, value}; };
        Require(Draw(*device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = equal(Stored)}) == Covered, "a stencil test after image_store into the stencil plane did not see the stored stencil");
        Require(Draw(*device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = equal(Reference)}) == 0u, "the stencil the draw wrote survived an image_store over the whole plane");
        std::puts("stencil plane storage tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
