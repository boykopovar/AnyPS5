#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height * 4> Results{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

std::vector<std::uint32_t> PixelCode() {
    std::vector<std::uint32_t> code{0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, Width};
    const auto vop2 = [&](std::uint32_t opcode, std::uint32_t destination, std::uint32_t source, std::uint32_t vector) {
        code.push_back(opcode | (destination << 17u) | (vector << 9u) | source);
    };
    vop2(0x34000000, 8, 0x82, 6);
    code.push_back(0xbefc0380);
    for (std::uint32_t bank = 0; bank < 3; ++bank) {
        vop2(0x4a000000, 5, 0x80 + bank, 8);
        code.push_back(0xdac00000 | (bank * 256u));
        code.push_back(0x00000500);
    }
    code.push_back(0xbf8cc07f);
    for (std::uint32_t bank = 0; bank < 3; ++bank) {
        code.push_back(0xdac40000 | (((bank + 1u) % 3u) * 256u));
        code.push_back((10u + bank) << 24u);
    }
    code.push_back(0xbf8cc07f);
    for (std::uint32_t bank = 0; bank < 3; ++bank) {
        code.push_back(0xe0702000 | (bank * 4u));
        code.push_back(0x80010006 | ((10u + bank) << 8u));
    }
    code.insert(code.end(), {0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000});
    return code;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

constexpr std::array<std::array<float, 4>, 3> Triangles{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    const auto& triangles = Triangles;
    const auto pixelCode = PixelCode();
    Pixels.fill(std::byte{0});
    Results.fill(Sentinel);
    const auto target = device.Target();

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(triangles.data(), 16u, static_cast<std::uint32_t>(triangles.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const auto results = BufferDescriptor(Results.data(), 16u, Width * Height);
    std::copy(results.begin(), results.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(std::span(pixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        {0, 0, vertexPush, 128 - vertexPush}
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, vertexPush}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
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

void Check(std::uint32_t waveSize) {
    const auto what = "pixel LDS lane slots wave" + std::to_string(waveSize);
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
            Require(Pixels[pixel * 4u + channel] == std::byte{255}, what + ": missing color at pixel " + std::to_string(pixel));
        }
        for (std::uint32_t bank = 0; bank < 3; ++bank) {
            const auto expected = pixel * 4u + (bank + 1u) % 3u;
            const auto actual = Results[pixel * 4u + bank];
            Require(actual == expected, what + ": pixel " + std::to_string(pixel) + " bank " + std::to_string(bank) + " expected " + std::to_string(expected) + " got " + std::to_string(actual));
        }
        Require(Results[pixel * 4u + 3u] == Sentinel, what + ": output stride guard overwritten");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        for (const auto waveSize : {64u, 32u}) {
            std::fprintf(stderr, "pixel LDS lane slots wave%u\n", waveSize);
            for (std::uint32_t pass = 0; pass < 3; ++pass) {
                Draw(*device, waveSize);
                Check(waveSize);
            }
        }
        std::puts("pixel LDS lane slot tests passed: 6 draws, 12288 pixels, 36864 LDS results");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
