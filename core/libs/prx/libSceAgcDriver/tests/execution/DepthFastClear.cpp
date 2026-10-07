#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::size_t HtileBytes = ((Width + 7u) / 8u) * ((Height + 7u) / 8u) * 4u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(4096) std::array<float, Width * Height * 2> Depth{};
alignas(256) std::array<std::uint32_t, 1024> Htile{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0002f2, 0xf800180f, 0x00000000, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

const std::vector<std::array<float, 4>>& Triangles() {
    static const std::vector<std::array<float, 4>> vertices{
        {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f},
    };
    return vertices;
}

struct Shaders {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::uint32_t vertexPush = 0;
};

Shaders Compile(AgcDriver::VulkanDevice& device) {
    const auto target = device.Target();
    Shaders shaders;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangles().data(), 16u, static_cast<std::uint32_t>(Triangles().size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {32, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    shaders.vertex = ShaderRecompiler::Recompile(vertex);
    shaders.vertexPush = static_cast<std::uint32_t>(shaders.vertex.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        {0, 0, shaders.vertexPush, 128 - shaders.vertexPush}
    };
    fragment.useCache = false;
    shaders.pixel = ShaderRecompiler::Recompile(fragment);
    return shaders;
}

std::uint32_t Draw(AgcDriver::VulkanDevice& device, const Shaders& compiled, bool depthWrite) {
    Pixels.fill(std::byte{0});
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderStage::Fragment, &compiled.pixel, compiled.vertexPush}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(Depth.data()), 0, {Width, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, reinterpret_cast<std::uintptr_t>(Htile.data()), false};
    state.depthTest = true;
    state.depthWrite = depthWrite;
    state.depthCompare = VK_COMPARE_OP_LESS;
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles().size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    std::uint32_t covered = 0;
    for (std::size_t pixel = 0; pixel < Pixels.size() / 4u; ++pixel) {
        if (std::to_integer<std::uint8_t>(Pixels[pixel * 4u]) == 255u) ++covered;
    }
    return covered;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Htile.fill(0xffffffffu);
        const auto shaders = Compile(*device);
        constexpr std::uint32_t all = Width * Height;
        const auto htile = reinterpret_cast<std::uintptr_t>(Htile.data());
        Require(Draw(*device, shaders, true) == all, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
        Require(Draw(*device, shaders, false) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
        AgcDriver::Graphics::NoteDepthMetadataFill(htile, HtileBytes - 4u, 0u);
        Require(Draw(*device, shaders, false) == 0u, "a fill shorter than the HTILE cleared the depth");
        AgcDriver::Graphics::NoteDepthMetadataFill(htile, HtileBytes, 0xffffffffu);
        Require(Draw(*device, shaders, false) == 0u, "an expanded HTILE fill (ZMask 0xf) cleared the depth");
        AgcDriver::Graphics::NoteDepthMetadataFill(htile, HtileBytes, 0u);
        Require(Draw(*device, shaders, true) == all, "a fill of ZMask 0 over the whole HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        Require(Draw(*device, shaders, false) == 0u, "the depth written after a fast clear was lost");
        AgcDriver::Graphics::NoteDepthMetadataFill(htile, HtileBytes, 0u);
        Require(Draw(*device, shaders, false) == all, "a second whole-HTILE fill of ZMask 0 did not clear the depth again");
        std::puts("depth fast clear tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
