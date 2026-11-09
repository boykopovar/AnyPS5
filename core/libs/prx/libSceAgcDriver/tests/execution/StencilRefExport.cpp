#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
constexpr std::byte Kept{0x40};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::byte, 128 * 128 * 4> DepthMemory{};
alignas(256) std::array<std::byte, 256 * 256> StencilMemory{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PlainPixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> ColumnPixelCode{
    0x7e040f00, 0x7e0e02f2, 0xf8000082, 0x00000200, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> RowPixelCode{
    0x7e040f01, 0x7e0e02f2, 0xf8000082, 0x00000200, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<float, 4>, 3> Vertices{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

ShaderRecompiler::ShaderPixelStageInfo PixelInfo(std::uint32_t shaderControl) {
    const AgcDriver::Registers context{
        {0x1b3, 0x300u}, {0x1b4, 0x300u}, {0x1b6, 0u}, {0x1c5, 9u}, {0x203, shaderControl},
    };
    return AgcDriver::Graphics::DecodePixelStageInfo(context, {0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u});
}

void Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, std::uint32_t shaderControl, const VkStencilOpState& stencil) {
    Pixels.fill(Kept);
    const auto target = device.Target();

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), 16u, static_cast<std::uint32_t>(Vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, PixelInfo(shaderControl), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(DepthMemory.data()), reinterpret_cast<std::uintptr_t>(StencilMemory.data()), {Width, Height}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0};
    state.stencilTest = true;
    state.stencilFront = stencil;
    state.stencilBack = stencil;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(const std::function<bool(std::uint32_t, std::uint32_t)>& drawn, std::string_view what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = drawn(x, y) ? std::byte{255} : Kept;
            const auto actual = Pixels[(y * Width + x) * 4u];
            Require(actual == expected, std::string(what) + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + std::to_string(static_cast<unsigned>(actual)) + ", expected " + (drawn(x, y) ? "drawn" : "stencil-rejected"));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityStencilExportEXT)) {
            std::puts("skipped, the device lacks VK_EXT_shader_stencil_export");
            return VulkanTestSkipped;
        }
        constexpr std::uint32_t exported = 0x52u;
        constexpr std::uint32_t plain = 0x50u;
        const VkStencilOpState replace{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 0u};
        const auto equal = [](std::uint32_t reference) { return VkStencilOpState{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, reference}; };
        Draw(*device, ColumnPixelCode, exported, replace);
        Check([](std::uint32_t, std::uint32_t) { return true; }, "the draw that replaces the stencil with the exported column");
        for (const auto column : {0u, 5u, 37u, 63u}) {
            Draw(*device, PlainPixelCode, plain, equal(column));
            Check([&](std::uint32_t x, std::uint32_t) { return x == column; }, "stencil equal to " + std::to_string(column) + " after the exported column was stored");
        }
        Draw(*device, ColumnPixelCode, exported, equal(200u));
        Check([](std::uint32_t, std::uint32_t) { return true; }, "the exported column tested against the stored column");
        Draw(*device, RowPixelCode, exported, equal(200u));
        Check([](std::uint32_t x, std::uint32_t y) { return x == y; }, "the exported row tested against the stored column");
        Draw(*device, ColumnPixelCode, plain, equal(9u));
        Check([](std::uint32_t x, std::uint32_t) { return x == 9u; }, "a column export without STENCIL_TEST_VAL_EXPORT_ENABLE");
        std::puts("stencil reference export tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
