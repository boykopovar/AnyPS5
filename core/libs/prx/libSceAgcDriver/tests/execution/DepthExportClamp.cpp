#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <limits>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

alignas(4096) std::array<float, DepthFastClearHarness::Width * DepthFastClearHarness::Height * 2> Depth{};

alignas(256) constexpr std::array<std::uint32_t, 8> ExportOnePointFive{0x7e0002f2, 0xf800100f, 0x00000000, 0x7e0202ff, 0x3fc00000, 0xf8001881, 0x00000001, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 8> ExportMinusHalf{0x7e0002f2, 0xf800100f, 0x00000000, 0x7e0202ff, 0xbf000000, 0xf8001881, 0x00000001, 0xbf810000};

Shaders WithDepthExport(AgcDriver::VulkanDevice& device, const Shaders& plain, std::span<const std::uint32_t> code, float minimum, float maximum) {
    Shaders shaders = plain;
    const auto target = device.Target();
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    pixel.depthExportEnable = true;
    pixel.depthExportMin = minimum;
    pixel.depthExportMax = maximum;
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderRecompiler::ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(plain.vertexPush, target)
    };
    fragment.useCache = false;
    shaders.pixel = ShaderRecompiler::Recompile(fragment);
    return shaders;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto plain = Compile(*device);
        const Surface surface{reinterpret_cast<std::uintptr_t>(Depth.data()), 0, 0, false, VK_FORMAT_D32_SFLOAT};
        constexpr float low = 0.25f;
        constexpr float high = 0.75f;
        const auto high15 = WithDepthExport(*device, plain, ExportOnePointFive, low, high);
        Require(Draw(*device, high15, surface, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the depth-exporting draw did not cover the target");
        Require(Draw(*device, plain, surface, {.depthTest = true, .depthWrite = false, .depthCompare = VK_COMPARE_OP_GREATER, .minDepth = 0.9f, .maxDepth = 0.9f}) == Covered, "an exported depth of 1.5 was not clamped to ZMAX 0.75");
        const auto minus = WithDepthExport(*device, plain, ExportMinusHalf, low, high);
        Require(Draw(*device, minus, surface, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the second depth-exporting draw did not cover the target");
        Require(Draw(*device, plain, surface, {.depthTest = true, .depthWrite = false, .depthCompare = VK_COMPARE_OP_LESS, .minDepth = 0.1f, .maxDepth = 0.1f}) == Covered, "an exported depth of -0.5 was not clamped to ZMIN 0.25");
        const auto open = WithDepthExport(*device, plain, ExportOnePointFive, -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
        Require(Draw(*device, open, surface, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the unclamped depth-exporting draw did not cover the target");
        Require(Draw(*device, plain, surface, {.depthTest = true, .depthWrite = false, .depthCompare = VK_COMPARE_OP_GREATER, .minDepth = 0.9f, .maxDepth = 0.9f}) == 0u, "an unclamped depth export (DISABLE_VIEWPORT_CLAMP) was clamped");
        std::puts("depth export clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
