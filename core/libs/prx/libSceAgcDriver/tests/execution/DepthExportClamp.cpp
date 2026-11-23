#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <span>
#include <vector>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::DrawOptions;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

alignas(4096) std::array<float, DepthFastClearHarness::Width * DepthFastClearHarness::Height * 2> Depth{};

constexpr std::uint32_t ViewportExport = (1u << 19u) | (1u << 21u);
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254u;
alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{0xe0382000, 0x80000005, 0xe030201c, 0x80000405, 0xbf8c3f70, 0xf80000cf, 0x03020100, 0xf80008d4, 0x00040000, 0xbf810000};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> color;
    std::uint32_t misc;
};
static_assert(sizeof(Vertex) == 32u);
alignas(4096) std::array<Vertex, 6> Vertices{};

void BuildQuad(std::uint32_t viewport) {
    constexpr std::array<std::array<float, 4>, 6> positions{{
        {-1, -1, 0.5f, 1}, {1, -1, 0.5f, 1}, {-1, 1, 0.5f, 1},
        {1, -1, 0.5f, 1}, {1, 1, 0.5f, 1}, {-1, 1, 0.5f, 1}
    }};
    for (std::size_t i = 0; i < Vertices.size(); ++i) Vertices[i] = {positions[i], {1, 1, 1}, viewport << 16u};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* address, std::uint32_t stride, std::uint32_t count) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    return {static_cast<std::uint32_t>(value), static_cast<std::uint32_t>((value >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

alignas(256) constexpr std::array<std::uint32_t, 8> ExportOnePointFive{0x7e0002f2, 0xf800100f, 0x00000000, 0x7e0202ff, 0x3fc00000, 0xf8001881, 0x00000001, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 8> ExportMinusHalf{0x7e0002f2, 0xf800100f, 0x00000000, 0x7e0202ff, 0xbf000000, 0xf8001881, 0x00000001, 0xbf810000};

ShaderRecompiler::ShaderPixelStageInfo ViewportDepthExportInfo() {
    AgcDriver::Registers registers;
    registers[0x1b6u] = 0u;
    registers[0x1b3u] = 0u;
    registers[0x1b4u] = 0u;
    registers[0x203u] = 1u;
    registers[0x003u] = 0u;
    registers[0x1c5u] = 9u;
    registers[0x0b4u] = std::bit_cast<std::uint32_t>(0.0f);
    registers[0x0b5u] = std::bit_cast<std::uint32_t>(1.0f);
    registers[0x0b6u] = std::bit_cast<std::uint32_t>(0.25f);
    registers[0x0b7u] = std::bit_cast<std::uint32_t>(0.75f);
    std::array<std::uint8_t, 8> exportMappings{};
    exportMappings.fill(0xe4u);
    std::vector<AgcDriver::Graphics::RegisterRead> reads;
    auto pixel = [&] {
        const AgcDriver::Graphics::RegisterReadLogScope logScope(&reads);
        return AgcDriver::Graphics::DecodePixelStageInfo(registers, exportMappings, false, {}, true);
    }();
    for (const auto read : reads) Require(AgcDriver::Graphics::DrawKeyCovers(read), "the depth-export decoder read a register outside DrawKeyRegisters: " + std::to_string(read.offset));
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping.fill(0xe4u);
    Require(pixel.depthExportViewportRanges && pixel.depthExportMinByViewport[0] == 0.0f && pixel.depthExportMaxByViewport[0] == 1.0f &&
                pixel.depthExportMinByViewport[1] == 0.25f && pixel.depthExportMaxByViewport[1] == 0.75f,
            "the pixel-state decoder did not retain viewport 1's depth-export clamp bounds");
    return pixel;
}

Shaders WithViewportDepthExport(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, ShaderRecompiler::ShaderPixelStageInfo pixel) {
    Shaders shaders{};
    const auto target = device.Target();
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(Vertices.size()));
    std::vector<std::uint32_t> vertexUserData(4u, 0u);
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = ViewportExport;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderRecompiler::ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    shaders.vertex = ShaderRecompiler::Recompile(vertex);
    shaders.vertexPush = static_cast<std::uint32_t>(shaders.vertex.pushConstants.size());

    std::vector<std::uint32_t> pixelUserData(4u, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderRecompiler::ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(shaders.vertexPush, target)
    };
    fragment.useCache = false;
    shaders.pixel = ShaderRecompiler::Recompile(fragment);
    return shaders;
}

std::uint32_t DrawViewportOne(AgcDriver::VulkanDevice& device, const Shaders& compiled, const Surface& surface, const DrawOptions& options) {
    BuildQuad(1u);
    auto pixels = std::span<std::byte>(DepthFastClearHarness::Pixels);
    std::fill(pixels.begin(), pixels.end(), std::byte{0});
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderRecompiler::ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderRecompiler::ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{surface.depth, surface.stencil, {Width, Height}, surface.format, 1.0f, surface.clearStencil, surface.htile, surface.htileStencil};
    state.depth->firstSlice = surface.slice;
    state.depth->depthSwizzle = surface.swizzle;
    state.depthTest = options.depthTest;
    state.depthWrite = options.depthWrite;
    state.depthCompare = options.depthCompare;
    state.stencilTest = options.stencilTest;
    state.stencilFront = options.stencil;
    state.stencilBack = options.stencil;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), options.minDepth, options.maxDepth};
    state.viewportExports = 1u << 19u;
    state.extraViewports.assign(AgcDriver::Graphics::MaxViewports - 1u, state.viewport);
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.viewportScissors.assign(AgcDriver::Graphics::MaxViewports, state.scissor);
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15u;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushAllPending("viewport depth-export clamp test");
    device.WaitIdle();
    std::uint32_t covered = 0;
    for (std::size_t pixel = 0; pixel < pixels.size() / 4u; ++pixel) {
        if (std::to_integer<std::uint8_t>(pixels[pixel * 4u]) == 255u) ++covered;
    }
    return covered;
}

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

        const auto target = device->Target();
        const auto supported = [&](std::uint32_t capability) { return std::ranges::find(target.supportedCapabilities, capability) != target.supportedCapabilities.end(); };
        constexpr auto multiViewportCapability = static_cast<std::uint32_t>(spv::CapabilityMultiViewport);
        if (supported(multiViewportCapability) && supported(ShaderViewportIndexLayerCapability)) {
            const auto viewportPixel = ViewportDepthExportInfo();
            const auto highViewport = WithViewportDepthExport(*device, ExportOnePointFive, viewportPixel);
            Require(DrawViewportOne(*device, highViewport, surface, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the viewport-indexed high depth-exporting draw did not cover the target");
            Require(Draw(*device, plain, surface, {.depthTest = true, .depthWrite = false, .depthCompare = VK_COMPARE_OP_GREATER, .minDepth = 0.9f, .maxDepth = 0.9f}) == Covered, "viewport 1's exported depth was clamped to viewport 0's ZMAX instead of its own 0.75");
            const auto lowViewport = WithViewportDepthExport(*device, ExportMinusHalf, viewportPixel);
            Require(DrawViewportOne(*device, lowViewport, surface, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the viewport-indexed low depth-exporting draw did not cover the target");
            Require(Draw(*device, plain, surface, {.depthTest = true, .depthWrite = false, .depthCompare = VK_COMPARE_OP_LESS, .minDepth = 0.1f, .maxDepth = 0.1f}) == Covered, "viewport 1's exported depth was clamped to viewport 0's ZMIN instead of its own 0.25");
        } else {
            std::puts("per-viewport depth-export clamp test skipped: the device has no multiViewport or VK_EXT_shader_viewport_index_layer");
        }
        std::puts("depth export clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
