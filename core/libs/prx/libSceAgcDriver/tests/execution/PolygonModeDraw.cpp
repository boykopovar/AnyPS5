#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
constexpr std::byte Kept{0x40};
constexpr std::byte Written{0xff};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

using Pixel = std::pair<std::uint32_t, std::uint32_t>;

constexpr std::array<std::array<float, 2>, 3> Corners{{{4.5f, 2.5f}, {40.5f, 2.5f}, {4.5f, 12.5f}}};

const std::set<Pixel> CornerPixels{{4, 2}, {40, 2}, {4, 12}};
const std::set<Pixel> Interior{{10, 4}, {20, 4}, {15, 5}, {8, 8}};
const std::set<Pixel> Outside{{50, 10}, {60, 2}, {30, 12}, {2, 2}, {4, 14}};

alignas(256) std::array<std::array<float, 4>, Corners.size()> Vertices{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void Draw(AgcDriver::VulkanDevice& device, VkPolygonMode mode) {
    Pixels.fill(Kept);
    for (std::size_t index = 0; index < Corners.size(); ++index) {
        Vertices[index] = {Corners[index][0] * 2.0f / Width - 1.0f, 1.0f - Corners[index][1] * 2.0f / Height, 0.5f, 1.0f};
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
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.polygonMode = mode;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

bool IsWritten(std::uint32_t x, std::uint32_t y) {
    bool written = true;
    bool kept = true;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        const auto value = Pixels[(y * Width + x) * 4u + channel];
        written = written && value == Written;
        kept = kept && value == Kept;
    }
    Require(written || kept, "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is neither written nor kept");
    return written;
}

void Expect(const std::set<Pixel>& pixels, bool written, std::string_view what) {
    for (const auto& [x, y] : pixels) {
        Require(IsWritten(x, y) == written, std::string(what) + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + (written ? "kept" : "written") + ", expected " + (written ? "written" : "kept"));
    }
}

void ExpectExactly(const std::set<Pixel>& pixels, std::string_view what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const bool expected = pixels.contains({x, y});
            Require(IsWritten(x, y) == expected, std::string(what) + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + (expected ? "kept" : "written") + ", expected " + (expected ? "written" : "kept"));
        }
    }
}

std::set<Pixel> StraightEdges() {
    std::set<Pixel> pixels;
    for (std::uint32_t x = 5; x <= 39; ++x) pixels.insert({x, 2});
    for (std::uint32_t y = 3; y <= 11; ++y) pixels.insert({4, y});
    return pixels;
}

bool Unsupported(AgcDriver::VulkanDevice& device, VkPolygonMode mode) {
    try {
        Draw(device, mode);
        return false;
    } catch (const std::exception& error) {
        const std::string_view message = error.what();
        if (message.find("needs the fillModeNonSolid feature") != std::string_view::npos || message.find("needs VK_KHR_maintenance5") != std::string_view::npos) {
            std::printf("skipped, %s\n", error.what());
            return true;
        }
        throw;
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Draw(*device, VK_POLYGON_MODE_FILL);
        Expect(Interior, true, "filled triangle");
        Expect(Outside, false, "filled triangle");
        if (Unsupported(*device, VK_POLYGON_MODE_POINT)) return VulkanTestSkipped;
        ExpectExactly(CornerPixels, "point polygon mode");
        if (Unsupported(*device, VK_POLYGON_MODE_LINE)) return VulkanTestSkipped;
        Expect(StraightEdges(), true, "line polygon mode");
        Expect(Interior, false, "line polygon mode");
        Expect(Outside, false, "line polygon mode");
        Draw(*device, VK_POLYGON_MODE_FILL);
        Expect(Interior, true, "filled triangle after the point and line modes");
        std::puts("point and line polygon modes drew the triangle's corners and edges");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
