#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace Harness = DepthFastClearHarness;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

struct Transform {
    bool negativeOneToOne;
    float scale;
    float offset;
    float clampMin;
    float clampMax;
};

struct Case {
    Transform transform;
    float z;
    bool drawn;
    float probe;
    bool covered;
    bool remapOnly;
    const char* what;
};

constexpr Transform Plain{false, 1.0f, 0.0f, 0.0f, 1.0f};

alignas(4096) std::array<std::array<float, Harness::Width * Harness::Height>, 32> Depths{};
alignas(256) std::array<std::array<std::array<float, 4>, 3>, 128> VertexSets{};
std::size_t vertexSets = 0;

// A vertex shader that reads no user data: the fullscreen triangle comes from the vertex index (v5), and the
// clip z is the literal at index 9. Its program carries no push data of its own.
alignas(256) std::array<std::uint32_t, 14> FetchlessCode{
    0x36020a81u, 0x34020282u, 0x4a0202c1u, 0x7e020b01u, 0x2c040a81u, 0x34040482u, 0x4a0404c1u, 0x7e040b02u,
    0x7e0602ffu, 0u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};

std::uint32_t Draw(AgcDriver::VulkanDevice& device, std::uint64_t depth, const Transform& transform, bool remap, float z, bool write, bool fetchless = false, bool targetRemap = false, bool prepared = false) {
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
    state.depthTest = true;
    state.depthWrite = write;
    state.depthCompare = write ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_LESS;
    state.renderExtent = {Harness::Width, Harness::Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const float minDepth = transform.negativeOneToOne ? transform.offset - transform.scale : transform.offset;
    state.viewport = {0, static_cast<float>(Harness::Height), static_cast<float>(Harness::Width), -static_cast<float>(Harness::Height), minDepth, transform.offset + transform.scale};
    state.negativeOneToOne = transform.negativeOneToOne;
    state.depthScale = transform.scale;
    state.depthOffset = transform.offset;
    state.depthClampMin = transform.clampMin;
    state.depthClampMax = transform.clampMax;
    state.scissor = {{0, 0}, {Harness::Width, Harness::Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    auto context = device.graphicsContext();
    context.depthRangeUnrestricted = !remap;
    AgcDriver::Graphics::ApplyDepthRemap(context, state);
    const bool inRange = minDepth >= 0.0f && transform.offset + transform.scale <= 1.0f;
    Require((state.depthRemap.mode != 0u) == (remap && !inRange), "the draw did not take the depth path the test asked for");

    auto target = device.Target();
    target.depthRangeRemap = state.depthRemap.mode != 0u || targetRemap;
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.depthRemapMode = state.depthRemap.mode;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    if (fetchless) {
        FetchlessCode[9] = std::bit_cast<std::uint32_t>(z);
    } else {
        const auto vertexBuffer = Harness::BufferDescriptor(vertices.data(), 16u, static_cast<std::uint32_t>(vertices.size()));
        std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    }
    const std::span<const std::uint32_t> vertexCode = fetchless ? std::span<const std::uint32_t>(FetchlessCode) : std::span<const std::uint32_t>(Harness::VertexCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(vertexCode.data()), std::as_bytes(vertexCode)}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}},
        {32, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = prepared;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> vertexHolder;
    if (prepared) {
        const auto handle = ShaderRecompiler::PrepareShader(vertex);
        const auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(vertex, handle);
        Require(invocation.has_value(), "the prepared vertex shader did not match its own request");
        vertexHolder = invocation->Materialize(*invocation->Capture(ShaderRecompiler::SrtRuntime{std::span<const std::uint32_t>(vertexUserData)}));
    } else {
        vertexHolder = std::make_shared<const ShaderRecompiler::RecompileResult>(ShaderRecompiler::Recompile(vertex));
    }
    const auto& vertexResult = *vertexHolder;
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    if (fetchless) Require(vertexPush == 0u, "the fetchless vertex shader carries push data of its own");

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
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
        if (!context.depthClamp) {
            std::puts("skipped, the device has no depth clamping");
            return VulkanTestSkipped;
        }
        constexpr float Step = 1.0f / 64.0f;
        constexpr Transform minecraft{true, 1.0f, 0.0f, -1.0f, 1.0f};
        constexpr Transform widened{false, 2.0f, -0.5f, 0.0f, 1.0f};
        constexpr Transform bounded{false, 2.0f, -0.5f, 0.25f, 0.75f};
        constexpr std::array<Case, 13> cases{{
            {minecraft, 0.5f - Step, true, 0.5f, false, false, "negative-one-to-one depth just below 0.5"},
            {minecraft, 0.5f + Step, true, 0.5f, true, false, "negative-one-to-one depth just above 0.5"},
            {minecraft, -1.5f, false, 0.5f, true, false, "negative-one-to-one clip z in front of the near plane"},
            {minecraft, 1.5f, false, 0.5f, true, false, "negative-one-to-one clip z behind the far plane"},
            {widened, 0.5f - Step / 2.0f, true, 0.5f, false, false, "scaled zero-to-one depth just below 0.5"},
            {widened, 0.5f + Step / 2.0f, true, 0.5f, true, false, "scaled zero-to-one depth just above 0.5"},
            {widened, -0.125f, false, 0.5f, true, false, "zero-to-one clip z in front of the near plane"},
            {widened, 1.125f, false, 0.5f, true, false, "zero-to-one clip z behind the far plane"},
            {bounded, 0.4f, true, 0.35f, false, false, "depth 0.3 inside clamp bounds [0.25, 0.75], probed at 0.35"},
            {bounded, 0.4f, true, 0.25f, true, false, "depth 0.3 inside clamp bounds [0.25, 0.75], probed at 0.25"},
            {bounded, 0.6f, true, 0.65f, true, false, "depth 0.7 inside clamp bounds [0.25, 0.75], probed at 0.65"},
            {bounded, 0.2f, true, 0.2f, true, true, "depth -0.1 clamped to 0.25, probed at 0.2"},
            {bounded, 0.8f, true, 0.8f, false, true, "depth 1.1 clamped to 0.75, probed at 0.8"},
        }};
        std::size_t surfaces = 0;
        std::size_t ran = 0;
        for (const bool remap : {true, false}) {
            if (!remap && !context.depthRangeUnrestricted) continue;
            for (const auto& test : cases) {
                if (!remap && (test.remapOnly || (test.transform.negativeOneToOne && !context.depthClipControl))) continue;
                Require(surfaces < Depths.size(), "the test ran out of depth surfaces");
                const auto depth = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Depths[surfaces++].data()));
                const auto drawn = Draw(*device, depth, test.transform, remap, test.z, true);
                const auto passed = Draw(*device, depth, Plain, false, test.probe, false);
                const auto path = remap ? "remapped" : "unrestricted";
                if (drawn != (test.drawn ? Harness::Covered : 0u) || passed != (test.covered ? Harness::Covered : 0u)) {
                    char detail[256];
                    std::snprintf(detail, sizeof(detail), "%s (%s path): the draw covered %u of %u pixels (expected %s) and the probe passed %u (expected %s)", test.what, path, drawn, Harness::Covered, test.drawn ? "all" : "none", passed, test.covered ? "all" : "none");
                    throw std::runtime_error(detail);
                }
                ++ran;
            }
        }
        // Scale and offset reach the vertex shader through the draw's push block, so two draws whose depth remap
        // differs only in them build one pipeline between them: the second finds it in the store, and both
        // store the depth they should.
        {
            constexpr Transform widerScale{false, 4.0f, -1.5f, 0.0f, 1.0f};
            Require(surfaces + 2u <= Depths.size(), "the test ran out of depth surfaces");
            const auto depthA = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Depths[surfaces++].data()));
            const auto depthB = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Depths[surfaces++].data()));
            const auto first = Draw(*device, depthA, widened, true, 0.5f + Step / 2.0f, true);
            const auto before = AgcDriver::Graphics::PipelineStoreTotals();
            const auto second = Draw(*device, depthB, widerScale, true, 0.5f + Step / 4.0f, true);
            const auto after = AgcDriver::Graphics::PipelineStoreTotals();
            Require(first == Harness::Covered && second == Harness::Covered, "a draw with a remapped depth did not cover the target");
            Require(after.misses == before.misses && after.hits > before.hits, "a draw that differs only in the depth scale and offset built another pipeline");
            for (const auto depth : {depthA, depthB}) {
                Require(Draw(*device, depth, Plain, false, 0.5f, false) == Harness::Covered, "a depth remapped by a shared pipeline was stored below 0.5 + 1/64");
                Require(Draw(*device, depth, Plain, false, 0.5f + 2.0f * Step, false) == 0u, "a depth remapped by a shared pipeline was stored above 0.5 + 1/64");
            }
            ++ran;
        }
        // A remapped vertex shader with no push data of its own, and a fragment shader with none: the pipeline's
        // push stages must still include the vertex stage (a non-GPL layout names them), or the scale and offset
        // reach no one.
        {
            Require(surfaces < Depths.size(), "the test ran out of depth surfaces");
            const auto depth = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Depths[surfaces++].data()));
            const auto drawn = Draw(*device, depth, widened, true, 0.5f + Step / 2.0f, true, true);
            Require(drawn == Harness::Covered, "a fetchless remapped vertex shader did not cover the target");
            Require(Draw(*device, depth, Plain, false, 0.5f, false) == Harness::Covered, "a fetchless remapped vertex shader stored a depth below 0.5 + 1/64, so its scale and offset did not arrive");
            Require(Draw(*device, depth, Plain, false, 0.5f + 2.0f * Step, false) == 0u, "a fetchless remapped vertex shader stored a depth above 0.5 + 1/64");
            ++ran;
        }
        // On a device without VK_EXT_depth_range_unrestricted the target remaps every vertex shader, so an ordinary
        // in-range draw (mode 0) with no push data of its own still carries the remap's push block: it must validate
        // and draw, from a direct build and from a prepared one.
        for (const bool prepared : {false, true}) {
            Require(surfaces < Depths.size(), "the test ran out of depth surfaces");
            const auto depth = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Depths[surfaces++].data()));
            const auto drawn = Draw(*device, depth, Plain, false, 0.5f + Step / 2.0f, true, true, true, prepared);
            Require(drawn == Harness::Covered, prepared ? "a fetchless mode 0 vertex shader on a remapping target, prepared, did not cover the target" : "a fetchless mode 0 vertex shader on a remapping target did not cover the target");
            Require(Draw(*device, depth, Plain, false, 0.5f, false) == Harness::Covered && Draw(*device, depth, Plain, false, 0.5f + 2.0f * Step, false) == 0u, "a fetchless mode 0 vertex shader on a remapping target stored the wrong depth");
            ++ran;
        }
        std::printf("depth range remap draw tests passed (%zu cases, unrestricted path %s)\n", ran, context.depthRangeUnrestricted ? "compared" : "absent");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
