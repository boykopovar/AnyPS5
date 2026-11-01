#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

namespace Harness = DepthFastClearHarness;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

struct Bounds {
    float min;
    float max;
};

struct Case {
    float stored;
    Bounds bounds;
    bool covered;
    const char* what;
};

alignas(4096) std::array<std::array<float, Harness::Width * Harness::Height>, 16> Depths{};
alignas(256) std::array<std::array<std::array<float, 4>, 3>, 64> VertexSets{};
std::size_t vertexSets = 0;

std::uint32_t Draw(AgcDriver::VulkanDevice& device, std::uint64_t depth, float z, std::optional<Bounds> bounds, bool emulate) {
    Require(vertexSets < VertexSets.size(), "the test ran out of vertex sets");
    auto& vertices = VertexSets[vertexSets++];
    vertices = {{{-1.0f, -1.0f, z, 1.0f}, {3.0f, -1.0f, z, 1.0f}, {-1.0f, 3.0f, z, 1.0f}}};
    Harness::Pixels.fill(std::byte{0});

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Harness::Pixels.data()), {Harness::Width, Harness::Height}, VK_FORMAT_R8G8B8A8_UNORM, Harness::Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{depth, 0, {Harness::Width, Harness::Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    state.depthTest = !bounds.has_value();
    state.depthWrite = !bounds.has_value();
    state.depthCompare = VK_COMPARE_OP_ALWAYS;
    if (bounds) {
        state.depthBoundsTest = true;
        state.minDepthBounds = bounds->min;
        state.maxDepthBounds = bounds->max;
    }
    state.renderExtent = {Harness::Width, Harness::Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Harness::Height), static_cast<float>(Harness::Width), -static_cast<float>(Harness::Height), 0.0f, 1.0f};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Harness::Width, Harness::Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    if (bounds && emulate) {
        auto context = device.graphicsContext();
        context.depthBounds = false;
        context.depthRangeUnrestricted = false;
        AgcDriver::Graphics::ApplyDepthBoundsEmulation(context, state, pixel);
        Require(!state.depthBoundsTest && state.depthTest && !state.depthWrite, "the depth bounds test was not emulated");
    }

    auto target = device.Target();
    target.depthRangeRemap = false;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = Harness::BufferDescriptor(vertices.data(), 16u, static_cast<std::uint32_t>(vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(Harness::VertexCode.data()), std::as_bytes(std::span(Harness::VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(Harness::VertexCode.data()), Harness::VertexCode, 0, {}},
        {32, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(Harness::PixelCode.data()), std::as_bytes(std::span(Harness::PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(Harness::PixelCode.data()), Harness::PixelCode, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    std::uint32_t covered = 0;
    for (std::size_t index = 0; index < Harness::Pixels.size() / 4u; ++index) {
        if (std::to_integer<std::uint8_t>(Harness::Pixels[index * 4u]) == 255u) ++covered;
    }
    return covered;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = device->graphicsContext();
        const float below = std::nextafter(0.25f, 0.0f);
        const float above = std::nextafter(0.25f, 1.0f);
        const std::array<Case, 12> cases{{
            {0.25f, {0.0f, 0.25f}, true, "stored 0.25, upper bound equal to it"},
            {0.25f, {0.0f, below}, false, "stored 0.25, upper bound one step below it"},
            {0.25f, {0.25f, 1.0f}, true, "stored 0.25, lower bound equal to it"},
            {0.25f, {above, 1.0f}, false, "stored 0.25, lower bound one step above it"},
            {0.000125f, {0.0f, 0.000125f}, true, "stored 0.000125 inside [0, 0.000125]"},
            {0.5f, {0.0f, 0.000125f}, false, "stored 0.5 outside [0, 0.000125]"},
            {0.5f, {0.000125f, 1.0f}, true, "stored 0.5 inside [0.000125, 1]"},
            {0.0f, {0.000125f, 1.0f}, false, "stored 0 outside [0.000125, 1]"},
            {0.0f, {0.0f, 1.0f}, true, "stored 0 inside [0, 1]"},
            {1.0f, {0.0f, 1.0f}, true, "stored 1 inside [0, 1]"},
            {1.0f, {0.0f, 0.5f}, false, "stored 1 outside [0, 0.5]"},
            {0.75f, {0.5f, 1.0f}, true, "stored 0.75 inside [0.5, 1]"},
        }};
        std::size_t compared = 0;
        for (std::size_t index = 0; index < cases.size(); ++index) {
            const auto& test = cases[index];
            const auto depth = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Depths[index].data()));
            Require(Draw(*device, depth, test.stored, std::nullopt, false) == Harness::Covered, "the draw that stores the depth did not cover the target");
            const auto expected = test.covered ? Harness::Covered : 0u;
            for (const bool emulate : {true, false}) {
                if (!emulate && !context.depthBounds) continue;
                const auto passed = Draw(*device, depth, 0.5f, test.bounds, emulate);
                if (passed != expected) {
                    char detail[256];
                    std::snprintf(detail, sizeof(detail), "%s (%s): %u of %u pixels passed, expected %s", test.what, emulate ? "emulated" : "native", passed, Harness::Covered, test.covered ? "all" : "none");
                    throw std::runtime_error(detail);
                }
                if (!emulate) ++compared;
            }
        }
        std::printf("depth bounds emulation draw tests passed (%zu cases, %zu compared with the native test)\n", cases.size(), compared);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
