#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t High = 0x11110000u;
constexpr std::uint32_t Last = 0x22220000u;
constexpr std::uint32_t Past = 0x33330000u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height * 4> Results{};
constexpr std::uint32_t Overflow = 256u;
alignas(256) std::array<std::byte, Overflow * Overflow * 4> OverflowPixels{};
alignas(256) std::array<std::uint32_t, Overflow * Overflow * 4> OverflowResults{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

std::vector<std::uint32_t> PixelCode(std::uint32_t width) {
    std::vector<std::uint32_t> code{0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, width};
    const auto mov = [&](std::uint32_t reg, std::uint32_t literal) {
        code.push_back(0x7e0002ffu | (reg << 17u));
        code.push_back(literal);
    };
    const auto shl = [&](std::uint32_t dest, std::uint32_t source) {
        code.push_back(0x34000000u | (dest << 17u) | (source << 9u) | 0x82u);
    };
    const auto write = [&](std::uint32_t index, std::uint32_t value) {
        mov(16u, index);
        shl(17u, 16u);
        mov(18u, value);
        code.push_back(0xd8340000u);
        code.push_back((18u << 8u) | 17u);
    };
    write(8000u, High);
    write(8191u, Last);
    write(8192u, Past);
    code.push_back(0xbf8cc07fu);
    const auto read = [&](std::uint32_t index, std::uint32_t dest) {
        mov(16u, index);
        shl(17u, 16u);
        code.push_back(0xd8d80000u);
        code.push_back((dest << 24u) | 17u);
    };
    read(8000u, 10u);
    read(8191u, 11u);
    read(8192u, 12u);
    shl(15u, 3u);
    code.push_back(0xd8d80000u);
    code.push_back((13u << 24u) | 15u);
    code.push_back(0xbf8cc07fu);
    for (std::uint32_t slot = 0; slot < 4u; ++slot) {
        code.push_back(0xe0702000u | (slot * 4u));
        code.push_back(0x80010006u | ((10u + slot) << 8u));
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

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t width, std::uint32_t height, std::span<std::byte> pixels, std::span<std::uint32_t> results, std::uint32_t draws = 1u) {
    const auto pixelCode = PixelCode(width);
    std::fill(pixels.begin(), pixels.end(), std::byte{0});
    std::fill(results.begin(), results.end(), 0u);
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangles.data(), 16u, static_cast<std::uint32_t>(Triangles.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
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
    pixel.wave32 = false;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const auto stored = BufferDescriptor(results.data(), 16u, width * height);
    std::copy(stored.begin(), stored.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(std::span(pixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    Require(pixelResult.functionLdsBuffer == target.largePixelFunctionLdsBuffer, target.largePixelFunctionLdsBuffer ? "pixel LDS limit: the full array was not backed by a buffer" : "pixel LDS limit: a driver that keeps the function array used the buffer");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {width, height}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {width, height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(height), static_cast<float>(width), -static_cast<float>(height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {width, height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles.size()), 0, 1, 0, false};
    for (std::uint32_t n = 0; n < draws; ++n) device.Draw(state, draw, shaders);
    device.WaitIdle();
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
        Draw(*device, Width, Height, Pixels, Results, 2u);
        for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
            for (std::uint32_t channel = 0; channel < 4u; ++channel) Require(Pixels[pixel * 4u + channel] == std::byte{255}, "pixel LDS limit: missing color");
            const auto base = pixel * 4u;
            Require(Results[base] == High, "pixel LDS limit: dword 8000 expected " + std::to_string(High) + " got " + std::to_string(Results[base]));
            Require(Results[base + 1u] == Last, "pixel LDS limit: dword 8191 expected " + std::to_string(Last) + " got " + std::to_string(Results[base + 1u]));
            Require(Results[base + 2u] == 0u, "pixel LDS limit: dword 8192 wrote " + std::to_string(Results[base + 2u]));
        }
        std::puts("pixel LDS limit passed: dword 8000 and 8191, 2048 pixels, two draws");
        if (device->Target().largePixelFunctionLdsBuffer) {
            bool threw = false;
            try {
                Draw(*device, Overflow, Overflow, OverflowPixels, OverflowResults);
            } catch (const std::exception& error) {
                threw = std::string_view(error.what()) == "AGC graphics: pixel function LDS subgroup slots exhausted";
                if (!threw) throw;
            }
            Require(threw, "pixel LDS limit: a draw past the subgroup slots did not throw");
            std::puts("pixel LDS limit overflow passed");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
