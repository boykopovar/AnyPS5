#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
constexpr std::byte Kept{0x40};
constexpr float SurfaceDepth = 0.5f;
constexpr float Bias = 1048576.0f;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<float, Width * Height> DepthMemory{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

constexpr std::array<std::array<float, 2>, 6> Corners{{
    {1.0f, 1.0f}, {1.0f, 15.0f}, {15.0f, 1.0f},
    {17.0f, 1.0f}, {31.0f, 1.0f}, {17.0f, 15.0f},
}};

alignas(256) std::array<std::array<float, 4>, Corners.size()> Vertices{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void Draw(AgcDriver::VulkanDevice& device, float frontBias, float backBias) {
    Pixels.fill(Kept);
    for (std::size_t index = 0; index < Corners.size(); ++index) {
        Vertices[index] = {Corners[index][0] * 2.0f / Width - 1.0f, 1.0f - Corners[index][1] * 2.0f / Height, SurfaceDepth, 1.0f};
    }
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

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
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
    state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(DepthMemory.data()), 0, {Width, Height}, VK_FORMAT_D32_SFLOAT, SurfaceDepth, 0};
    state.depthTest = true;
    state.depthWrite = false;
    state.depthCompare = VK_COMPARE_OP_LESS;
    state.depthBias = true;
    state.depthBiasConstant = frontBias;
    state.depthBiasPerFace = frontBias != backBias;
    state.backDepthBiasConstant = backBias;
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

void Check(bool frontDrawn, bool backDrawn, std::string_view what) {
    for (const auto& [x, y, front] : std::array<std::tuple<std::uint32_t, std::uint32_t, bool>, 2>{{{4, 4, true}, {20, 4, false}}}) {
        const bool drawn = front ? frontDrawn : backDrawn;
        const auto expected = drawn ? std::byte{255} : Kept;
        const auto actual = Pixels[(y * Width + x) * 4u];
        Require(actual == expected, std::string(what) + ": the " + (front ? "front" : "back") + "-facing triangle at (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + std::to_string(static_cast<unsigned>(actual)) + ", expected " + (drawn ? "drawn" : "depth-rejected"));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Draw(*device, -Bias, Bias);
        Check(true, false, "front faces pulled closer, back faces pushed away");
        Draw(*device, Bias, -Bias);
        Check(false, true, "front faces pushed away, back faces pulled closer");
        Draw(*device, -Bias, -Bias);
        Check(true, true, "both faces pulled closer");
        Draw(*device, -Bias, 0.0f);
        Check(true, false, "only front faces biased");
        std::puts("per-face depth bias tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
