#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include "ProvokingVertex_vert_spv.h"
#include "ProvokingVertex_frag_spv.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Panel = 64;
constexpr std::uint32_t Width = Panel * 3;
constexpr std::uint32_t Height = Panel;
constexpr std::byte Sentinel{0x40};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
using Color = std::array<unsigned, 4>;
constexpr Color Red{255, 0, 0, 255};
constexpr Color Green{0, 255, 0, 255};
constexpr Color Blue{0, 0, 255, 255};
constexpr Color Yellow{255, 255, 0, 255};
constexpr Color Magenta{255, 0, 255, 255};

ShaderRecompiler::RecompileResult Shader(std::span<const std::uint32_t> words, std::uint64_t variant) {
    ShaderRecompiler::RecompileResult result{};
    result.spirv = std::vector<std::uint32_t>(words.begin(), words.end());
    result.variantId = variant;
    return result;
}

ShaderRecompiler::RecompileResult GuestPixel(const ShaderRecompiler::SpirvTarget& target) {
    alignas(256) static constexpr std::array<std::uint32_t, 7> code{0xc8120002u, 0xc8160102u, 0xc81a0202u, 0xc81e0302u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = 1;
    pixel.interpolatorSettings[0] = 0x400u;
    pixel.inputAddr = 2;
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target = target;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    auto result = ShaderRecompiler::Recompile(request);
    result.variantId = 3;
    Require(result.fragmentParameters.size() == 1 && result.fragmentParameters[0].flat && !result.fragmentParameters[0].perVertex, "guest P0 shader did not produce a flat input");
    return result;
}

void CheckPixel(std::uint32_t x, std::uint32_t y, const Color& expected, const std::string& name) {
    for (std::uint32_t channel = 0; channel < expected.size(); ++channel) {
        const auto actual = std::to_integer<unsigned>(Pixels[(y * Width + x) * 4 + channel]);
        Require(actual == expected[channel], name + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[channel]));
    }
}

void Run(AgcDriver::VulkanDevice& device, VkPrimitiveTopology topology, std::span<const AgcDriver::Graphics::CompiledShader> shaders, bool useLast = true, std::string_view shaderName = "GLSL") {
    Pixels.fill(Sentinel);
    AgcDriver::Graphics::State state{};
    state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = topology;
    state.scissor = {{0, 0}, {Width, Height}};
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    const bool strip = topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    const AgcDriver::Pm4::DrawParameters draw{0, strip ? 4u : 6u, 0, 1, 0, false};
    for (std::uint32_t panel = 0; panel < 3; ++panel) {
        state.viewport = {static_cast<float>(panel * Panel), static_cast<float>(Height), static_cast<float>(Panel), -static_cast<float>(Height), 0, 1};
        state.provokingVertexMode = panel == 1 && useLast ? VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT : VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
        device.Draw(state, draw, shaders);
    }
    device.WaitIdle();
    const auto name = std::string(shaderName) + (strip ? " triangle strip" : " triangle list");
    for (std::uint32_t panel = 0; panel < 3; ++panel) {
        const bool last = panel == 1 && useLast;
        const auto firstTriangle = last ? Blue : Red;
        const auto secondTriangle = strip ? (last ? Yellow : Green) : (last ? Magenta : Yellow);
        for (std::uint32_t offset = 0; offset < 4; ++offset) {
            CheckPixel(panel * Panel + 16 + offset, 16, firstTriangle, name);
            CheckPixel(panel * Panel + 48 - offset, 48, secondTriangle, name);
        }
        CheckPixel(panel * Panel, 0, {64, 64, 64, 64}, name + " background");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto vertex = Shader(PROVOKING_VERTEX_VERT_SPV, 1);
        const auto fragment = Shader(PROVOKING_VERTEX_FRAG_SPV, 2);
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
            {ShaderRecompiler::ShaderStage::Vertex, &vertex, 0},
            {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}
        }};
        if (!device->ProvokingVertexLast()) {
            for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP}) Run(*device, topology, shaders, false);
            bool rejected = false;
            try {
                Run(*device, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, shaders);
            } catch (const std::exception& error) {
                Require(std::string(error.what()).find("last provoking vertex requires VK_EXT_provoking_vertex with provokingVertexLast enabled") != std::string::npos, std::string("unexpected last-vertex rejection: ") + error.what());
                rejected = true;
            }
            Require(rejected, "last provoking vertex was drawn without device support");
            device->WaitIdle();
            Require(std::getenv("ANYPS5_REQUIRE_VULKAN") == nullptr, "required provoking vertex rendering coverage is unavailable: provokingVertexLast is not supported");
            std::puts("skipped last-vertex rendering, provokingVertexLast is unavailable; first-vertex rendering and feature rejection passed");
            return VulkanTestSkipped;
        }
        for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP}) Run(*device, topology, shaders);
        const auto guestPixel = GuestPixel(device->Target());
        const std::array<AgcDriver::Graphics::CompiledShader, 2> guestShaders{{
            {ShaderRecompiler::ShaderStage::Vertex, &vertex, 0},
            {ShaderRecompiler::ShaderStage::Fragment, &guestPixel, 0}
        }};
        for (const auto topology : {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP}) Run(*device, topology, guestShaders, true, "guest P0");
        std::puts("provoking vertex rendering tests passed: GLSL and guest P0, triangle list and strip, first/last/first");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
