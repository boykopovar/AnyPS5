#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Width;

alignas(4096) std::array<float, 65536> Depth{};
alignas(256) std::array<std::uint32_t, 64> Output{};

std::uintptr_t DepthAddress() {
    return (reinterpret_cast<std::uintptr_t>(Depth.data()) + 0xffffu) & ~std::uintptr_t{0xffffu};
}
alignas(256) constexpr std::array<std::uint32_t, 13> LoadCode{
    0x7e3c0280u, 0x7e3e0280u, 0x7e140280u, 0xf0001108u, 0x00010a1eu,
    0xbf8c3f70u, 0xf0201108u, 0x00010a1eu, 0x34060082u, 0xe0701000u, 0x80000a03u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 10> SampleCode{
    0x7e3c02ffu, 0x3f000000u, 0x7e3e02ffu, 0x3f000000u, 0xf09c0108u,
    0x00610a1eu, 0x34060082u, 0xe0701000u, 0x80000a03u, 0xbf810000u,
};

void Read(AgcDriver::VulkanDevice& device, std::uint32_t format, std::uint32_t width, bool sampled, float expected, std::uint32_t height = Height, std::uint32_t swizzle = 0u) {
    Output.fill(0xdeadbeefu);
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    const auto depth = DepthAddress();
    const std::array<std::uint32_t, 16> data{
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>(output >> 32u), sizeof(Output), 0x31016facu,
        static_cast<std::uint32_t>(depth >> 8u), static_cast<std::uint32_t>(depth >> 40u) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u), 0x90000facu | (swizzle << 20u), 0u, 0u, 0u, 0u,
        0x92u, 0u, 0u, 0u,
    };
    const auto code = sampled ? std::span<const std::uint32_t>(SampleCode) : std::span<const std::uint32_t>(LoadCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, data, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0, 0, 0, 128},
    };
    request.useCache = false;
    const auto shader = ShaderRecompiler::Recompile(request);
    device.Dispatch(shader, 1, 1, 1);
    device.WaitIdle();
    for (std::size_t lane = 0; lane < 32; ++lane) {
        Require((sampled && format == 7u ? std::abs(std::bit_cast<float>(Output[lane]) - expected) <= 1.0f / 65535.0f : Output[lane] == std::bit_cast<std::uint32_t>(expected)), "depth reuse returned the wrong GPU value at lane " + std::to_string(lane) + ": got " + std::to_string(Output[lane]) + ", expected " + std::to_string(std::bit_cast<std::uint32_t>(expected)));
    }
    Require(std::all_of(Depth.begin(), Depth.end(), [](float value) { return value == 0.25f; }), "depth reuse changed the guest bytes");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = DepthFastClearHarness::Compile(*device);
        const auto address = DepthAddress();
        for (const auto format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}) {
            AgcDriver::Graphics::ClearDepthSurfaces(device->Device());
            Depth.fill(0.25f);
            const DepthFastClearHarness::Surface surface{address, 0, 0, false, format};
            Require(DepthFastClearHarness::Draw(*device, shaders, surface, {.depthWrite = true}) == DepthFastClearHarness::Covered, "depth producer did not cover the surface");
            Read(*device, format == VK_FORMAT_D16_UNORM ? 7u : 22u, Width, true, 0.5f);
            Read(*device, 22u, format == VK_FORMAT_D16_UNORM ? Width : Width / 2u, false, 0.25f);
            if (format != VK_FORMAT_D32_SFLOAT) continue;
            Read(*device, 22u, Width, true, 0.5f);
            Read(*device, 20u, Width, false, 0.25f, Height, 25u);
            Read(*device, 22u, Width, true, 0.25f);
            Require(DepthFastClearHarness::Draw(*device, shaders, surface, {.depthTest = false, .depthWrite = true}) == DepthFastClearHarness::Covered, "depth producer did not cover the surface");
            Read(*device, 22u, Width, true, 0.5f);
            Read(*device, 22u, 128u, false, 0.25f, 128u);
            Read(*device, 22u, Width, true, 0.25f);
        }
        std::puts("depth storage reuse GPU tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
