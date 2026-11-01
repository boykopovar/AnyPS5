#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Columns = 7;
constexpr std::uint32_t Rows = 5;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 44> VertexCode{
    0xd7650015, 0x000100c1, 0xd7660015, 0x00022ac1, 0x7e0e0280, 0x7da206f9, 0x81060015, 0x360e0a81,
    0xbefe04c1, 0xd4c50002, 0x00020e80, 0xd763000f, 0x00022a81, 0xd5010000, 0x000a1e80, 0x380000fa,
    0xff091100, 0x380000fa, 0xff091200, 0x380000fa, 0xff091400, 0x380000fa, 0xff091800, 0xd778000f,
    0x03058300, 0x38001f00, 0x88040302, 0xd7600002, 0x00013f00, 0xd760006b, 0x00017f00, 0x88026b02,
    0xbf060402, 0x850580f2, 0xe0382000, 0x80020005, 0xbf8c3f70, 0x10000005, 0x10020205, 0x10040405,
    0x10060605, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::vector<std::array<float, 4>> Triangles() {
    std::vector<std::array<float, 4>> vertices;
    for (std::uint32_t cell = 0; cell < Columns * Rows; ++cell) {
        const float left = -1.0f + 2.0f * static_cast<float>(cell % Columns) / Columns;
        const float right = -1.0f + 2.0f * static_cast<float>(cell % Columns + 1u) / Columns;
        const float bottom = -1.0f + 2.0f * static_cast<float>(cell / Columns) / Rows;
        const float top = -1.0f + 2.0f * static_cast<float>(cell / Columns + 1u) / Rows;
        for (const auto& corner : {std::array{left, bottom}, std::array{right, bottom}, std::array{right, top}, std::array{left, bottom}, std::array{right, top}, std::array{left, top}}) {
            vertices.push_back({corner[0], corner[1], 0.5f, 1.0f});
        }
    }
    return vertices;
}

void Draw(AgcDriver::VulkanDevice& device) {
    static const auto triangles = Triangles();
    Pixels.fill(std::byte{0});
    const auto target = device.Target();

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(triangles.data(), 16u, static_cast<std::uint32_t>(triangles.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 8, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
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
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
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
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangles.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        const auto actual = std::to_integer<unsigned>(Pixels[pixel * 4u]);
        Require(actual == 255u, "NGG vertex wave reduction: pixel " + std::to_string(pixel) + " is " + std::to_string(actual) + ", expected 255 (a vertex whose wave OR of its lanes' bits, read from lanes 31 and 63, differs from its wave's ballot collapses its triangle)");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto subgroupSize = device->Target().subgroupSize;
        if (subgroupSize != 32u) {
            std::printf("skipped, s3 describes a 32-lane wave only on 32-lane subgroups, and the device has %u lanes\n", subgroupSize);
            return VulkanTestSkipped;
        }
        if ((device->SubgroupStages() & VK_SHADER_STAGE_VERTEX_BIT) == 0u || !TargetHasCapability(device->Target(), spv::CapabilityGroupNonUniformArithmetic)) {
            std::puts("skipped, the device has no subgroup arithmetic in vertex shaders");
            return VulkanTestSkipped;
        }
        for (std::uint32_t pass = 0; pass < 3; ++pass) {
            Draw(*device);
            Check();
        }
        std::puts("NGG vertex wave reduction tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
