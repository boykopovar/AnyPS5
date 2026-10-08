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
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t DistanceVector = 0x00400000u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80000005, 0xe0382010, 0x80000405, 0xbf8c3f70,
    0xf80000cf, 0x03020100, 0xf80008df, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0x7e0002f2, 0x7e0202f2, 0x7e0402f2, 0x7e0602f2, 0xf800080f, 0x03020100, 0xbf810000,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 4> distances;
};

constexpr std::array<float, 3> PositionX{-3.0f, 1.0f, 1.0f};
constexpr std::array<float, 3> PositionY{-1.0f, -1.0f, 3.0f};
constexpr std::array<float, 3> Negative{-1.0f, -1.0f, -1.0f};
constexpr std::array<float, 3> Mixed{-1.0f, 1.0f, -1.0f};

enum class Visible { All, None, X, Y, XAndY };

struct Row {
    const char* name;
    std::uint32_t control;
    Visible visible;
    std::array<float, 3> component0;
    std::array<float, 3> component1;
    std::array<float, 3> component2;
};

const std::array<Row, 7> Rows{{
    {"no enable bits", DistanceVector, Visible::All, Negative, Negative, Negative},
    {"clip plane 0 from component 0", DistanceVector | 0x1u, Visible::X, PositionX, Negative, Negative},
    {"clip plane 1 alone, from component 1", DistanceVector | 0x2u, Visible::Y, Negative, PositionY, Negative},
    {"clip planes 0x5, components 0 and 2", DistanceVector | 0x5u, Visible::XAndY, PositionX, Negative, PositionY},
    {"cull plane 0, every vertex negative", DistanceVector | 0x100u, Visible::None, Negative, Negative, Negative},
    {"cull plane 0, one vertex not negative", DistanceVector | 0x100u, Visible::All, Mixed, Negative, Negative},
    {"cull plane 0 from component 2, sparse 0x4", DistanceVector | 0x400u, Visible::None, Mixed, Negative, Negative},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

bool Expected(Visible visible, std::uint32_t pixel) {
    const double x = -1.0 + 2.0 * ((pixel % Width) + 0.5) / Width;
    const double y = 1.0 - 2.0 * ((pixel / Width) + 0.5) / Height;
    switch (visible) {
        case Visible::All: return true;
        case Visible::None: return false;
        case Visible::X: return x >= 0.0;
        case Visible::Y: return y >= 0.0;
        case Visible::XAndY: return x >= 0.0 && y >= 0.0;
    }
    return false;
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const Row& row) {
    Pixels.fill(std::byte{0});
    std::array<Vertex, 3> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        vertices[i] = {{PositionX[i], PositionY[i], 0.5f, 1.0f}, {row.component0[i], row.component1[i], row.component2[i], 0.0f}};
    }

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = row.control;
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        device.Target(),
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {waveSize, 0, std::vector<std::uint32_t>(4, 0u), std::nullopt, pixel, std::nullopt, pixelMemory},
        device.Target(),
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
    state.paClVsOutCntl = row.control;
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(const Row& row, std::uint32_t waveSize) {
    const auto what = std::string("vertex clip and cull distances, ") + row.name + ", wave" + std::to_string(waveSize);
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        const auto value = std::to_integer<std::uint8_t>(Pixels[pixel * 4u]);
        Require(value == 0u || value == 255u, what + ": pixel " + std::to_string(pixel) + " stored " + std::to_string(value));
        const bool written = value == 255u;
        const bool expected = Expected(row.visible, pixel);
        Require(written == expected, what + ": pixel (" + std::to_string(pixel % Width) + ", " + std::to_string(pixel / Width) + ") is " + (written ? "written" : "clear") + ", expected " + (expected ? "written" : "clear"));
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
            for (const auto& row : Rows) {
                Draw(*device, waveSize, row);
                Check(row, waveSize);
            }
        }
        std::puts("vertex clip and cull distance tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
