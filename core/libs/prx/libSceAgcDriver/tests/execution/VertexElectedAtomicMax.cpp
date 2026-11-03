#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t MaxVertices = 129;
alignas(256) std::array<float, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, 64> Result{};
alignas(256) std::array<std::array<float, 4>, MaxVertices> Positions{};

alignas(256) constexpr std::array<std::uint32_t, 41> VertexCode{
    0xe00c2000, 0x80030005, 0xbf8c3f70, 0x7e0c0d05, 0xd5088006, 0x02020cff, 0x3d000000, 0xbe94047e,
    0xbe9625c1, 0x7e1402ff, 0xff800000, 0xd501000a, 0x00520d0a, 0x201414fa, 0xff01110a, 0x201414fa,
    0xff01120a, 0x201414fa, 0xff01140a, 0x201414fa, 0xff01180a, 0xd7780009, 0x0305830a, 0x2014130a,
    0xbefe0416, 0xd7600018, 0x02013f0a, 0xd7600019, 0x02017f0a, 0xd5100008, 0x02003218, 0x7c240d08,
    0xbf880002, 0xe0e00000, 0x80020800, 0xbefe0414, 0xf80008cf, 0x03020100, 0xf800020f, 0x00000000,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0002f2, 0xf800180f, 0x00000000, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void FillPositions() {
    for (std::uint32_t vertex = 0; vertex < MaxVertices; ++vertex) {
        const auto triangle = vertex / 3u;
        const auto corner = vertex % 3u;
        const auto x = -0.9f + 0.04f * static_cast<float>(triangle % 40u);
        const auto y = -0.9f + 0.4f * static_cast<float>(triangle / 40u);
        Positions[vertex] = {x + (corner == 1u ? 0.03f : 0.0f), y + (corner == 2u ? 0.03f : 0.0f), 0.5f, 1.0f};
    }
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t vertices) {
    Pixels.fill(0.0f);
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(16, 0u);
    const auto result = BufferDescriptor(Result.data(), 4u, static_cast<std::uint32_t>(Result.size()));
    const auto positions = BufferDescriptor(Positions.data(), 16u, vertices);
    std::copy(result.begin(), result.end(), vertexUserData.begin() + 8);
    std::copy(positions.begin(), positions.end(), vertexUserData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.executeOnNoop = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData;
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::vector<AgcDriver::Graphics::CompiledShader> shaders{{ShaderStage::Vertex, &vertexResult, 0}, {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Pixels), 0xe4u, AgcDriver::Graphics::ColorTileMode::Linear, 16u};
    state.colors = {state.color};
    state.hasColorTarget = true;
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
    device.Draw(state, AgcDriver::Pm4::DrawParameters{0, vertices, 0, 1, 0, false}, shaders);
    device.WaitIdle();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().vertexSubgroups) {
            std::puts("skipped, the device runs vertex programs in subgroups, so they keep their cross-lane reductions");
            return VulkanTestSkipped;
        }
        FillPositions();
        for (const auto vertices : {3u, 21u, 33u, 48u, 129u}) {
            Result.fill(0u);
            Result[1] = 0x3e800000u;
            Draw(*device, vertices);
            const auto expected = std::bit_cast<std::uint32_t>(std::min(1.0f, static_cast<float>(vertices - 1u) / 32.0f));
            Require(Result[0] == expected, "elected atomic max over " + std::to_string(vertices) + " vertices: the buffer holds " + std::to_string(std::bit_cast<float>(Result[0])) + ", expected " + std::to_string(std::bit_cast<float>(expected)));
            Require(Result[1] == 0x3e800000u, "elected atomic max over " + std::to_string(vertices) + " vertices: a word past the target changed");
        }
        std::puts("vertex elected atomic max tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
