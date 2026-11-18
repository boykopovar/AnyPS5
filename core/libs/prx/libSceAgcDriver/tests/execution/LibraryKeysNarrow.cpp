#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Triangles;
using DepthFastClearHarness::Width;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Samples = 4;

alignas(4096) std::array<float, Width * Height> Depth32{};
alignas(4096) std::array<std::uint16_t, Width * Height> Depth16{};
alignas(4096) std::array<float, Width * Height> Depth32Stencil{};
alignas(4096) std::array<std::uint8_t, Width * Height> Stencil8{};
alignas(4096) std::array<std::byte, 65536> Multisampled{};
alignas(4096) std::array<float, Width * Height> MultisampledDepth{};

struct Target {
    std::string_view name;
    Surface surface;
};

template<typename TPlane>
std::uint64_t AddressOf(const TPlane& plane) {
    return reinterpret_cast<std::uintptr_t>(plane.data());
}

std::array<Target, 3> Targets() {
    return {{
        {"a 32-bit float depth target", {AddressOf(Depth32), 0, 0, false, VK_FORMAT_D32_SFLOAT}},
        {"a 16-bit depth target", {AddressOf(Depth16), 0, 0, false, VK_FORMAT_D16_UNORM}},
        {"a 32-bit float depth target with stencil", {AddressOf(Depth32Stencil), AddressOf(Stencil8), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT}},
    }};
}

std::array<std::uint64_t, 4> Built(VkDevice device) {
    return PipelineLibraryCountersOf(device).built;
}

void DrawMultisampled(AgcDriver::VulkanDevice& device, const Shaders& compiled) {
    const std::array<CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    MultisampledDepth.fill(1.0f);
    State state{};
    state.stages = {ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Multisampled.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Multisampled.size(), 0xe4u};
    state.color.tileMode = ColorTileMode::RenderTarget;
    state.color.samples = Samples;
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = DepthTarget{reinterpret_cast<std::uintptr_t>(MultisampledDepth.data()), 0, {Width, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0, false, Samples};
    state.depthTest = true;
    state.depthCompare = VK_COMPARE_OP_LESS;
    state.samples = Samples;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles().size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Arm(AgcDriver::VulkanDevice& device, const Shaders& compiled, std::span<const Target> targets, bool multisampled, bool wide, bool libraries) {
    RuntimeSwitches::Assign(RuntimeSwitch::WideLibraryKeys, wide);
    const auto vkDevice = AgcDriver::VulkanDeviceTestAccess::ContextOf(device).device;
    const auto before = Built(vkDevice);
    const std::string arm = wide ? "wide library keys" : "narrow library keys";
    for (const auto& target : targets) {
        const auto written = DepthFastClearHarness::Draw(device, compiled, target.surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .maxDepth = 0.5f});
        Require(written == Covered, arm + ", " + std::string(target.name) + ": a draw that writes depth 0.25 covered " + std::to_string(written) + " of " + std::to_string(Covered) + " pixels");
        const auto behind = DepthFastClearHarness::Draw(device, compiled, target.surface, {.depthCompare = VK_COMPARE_OP_LESS});
        Require(behind == 0, arm + ", " + std::string(target.name) + ": " + std::to_string(behind) + " pixels at depth 0.5 passed a less-than test against depth 0.25");
        const auto front = DepthFastClearHarness::Draw(device, compiled, target.surface, {.depthCompare = VK_COMPARE_OP_GREATER});
        Require(front == Covered, arm + ", " + std::string(target.name) + ": " + std::to_string(front) + " of " + std::to_string(Covered) + " pixels at depth 0.5 passed a greater-than test against depth 0.25");
    }
    if (multisampled) DrawMultisampled(device, compiled);
    if (!libraries) return;
    const auto after = Built(vkDevice);
    const auto preRasterization = after[1] - before[1];
    const auto fragment = after[2] - before[2];
    const auto formats = static_cast<std::uint64_t>(targets.size());
    const auto expectedFragment = wide ? formats + (multisampled ? 1u : 0u) : 1u;
    const auto expectedPreRasterization = wide ? formats : 1u;
    Require(preRasterization == expectedPreRasterization && fragment == expectedFragment, arm + ": built " + std::to_string(preRasterization) + " pre-rasterization and " + std::to_string(fragment) + " fragment shader libraries for " + std::to_string(formats) + " depth formats" + (multisampled ? " and a 4-sample draw" : "") + ", expected " + std::to_string(expectedPreRasterization) + " and " + std::to_string(expectedFragment));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const bool libraries = context.graphicsPipelineLibrary;
        const bool multisampled = (context.limits.framebufferColorSampleCounts & Samples) != 0 && (context.limits.framebufferDepthSampleCounts & Samples) != 0;
        const auto compiled = DepthFastClearHarness::Compile(*device);
        const auto targets = Targets();
        Arm(*device, compiled, targets, multisampled, false, libraries);
        Arm(*device, compiled, targets, multisampled, true, libraries);
        RuntimeSwitches::Assign(RuntimeSwitch::WideLibraryKeys, false);
        std::printf("library key tests passed (%s%s)\n", libraries ? "pipeline libraries" : "whole pipelines", multisampled ? ", with a 4-sample draw" : "");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
