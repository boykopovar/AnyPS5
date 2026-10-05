#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height> Lanes{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 21> PixelCode{
    0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, 0x000000c0, 0xd7650004, 0x0001007e, 0xd7660004,
    0x0002087f, 0xbe88107e, 0x7e120506, 0x8f089708, 0x88080908, 0x34080890, 0x380a0808, 0xe0702000,
    0x80010506, 0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::vector<std::array<float, 4>> Triangles() {
    std::vector<std::array<float, 4>> vertices;
    constexpr std::uint32_t columns = 6;
    constexpr std::uint32_t rows = 4;
    for (std::uint32_t cell = 0; cell < columns * rows; ++cell) {
        const float width = 2.0f / columns;
        const float height = 2.0f / rows;
        const float x = -1.0f + width * static_cast<float>(cell % columns);
        const float y = -1.0f + height * static_cast<float>(cell / columns);
        const float skew = 0.07f * static_cast<float>(cell % 5u);
        vertices.push_back({x + (0.05f + skew) * width, y + 0.05f * height, 0.5f, 1.0f});
        vertices.push_back({x + 0.93f * width, y + (0.11f + skew) * height, 0.5f, 1.0f});
        vertices.push_back({x + (0.37f - skew * 0.5f) * width, y + 0.91f * height, 0.5f, 1.0f});
    }
    return vertices;
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    static const auto triangles = Triangles();
    Pixels.fill(std::byte{0});
    Lanes.fill(Sentinel);

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
    const auto lanes = BufferDescriptor(Lanes.data(), 4u, static_cast<std::uint32_t>(Lanes.size()));
    std::copy(lanes.begin(), lanes.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
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
    const auto what = "pixel helper lanes wave" + std::to_string(waveSize);
    std::map<std::uint32_t, std::vector<std::pair<std::uint32_t, std::uint32_t>>> waves;
    std::uint32_t covered = 0;
    for (std::uint32_t pixel = 0; pixel < Lanes.size(); ++pixel) {
        if (std::to_integer<std::uint8_t>(Pixels[pixel * 4u]) != 255u) {
            Require(Lanes[pixel] == Sentinel, what + ": uncovered pixel " + std::to_string(pixel) + " stored " + std::to_string(Lanes[pixel]));
            continue;
        }
        ++covered;
        const auto word = Lanes[pixel];
        Require(word != Sentinel, what + ": covered pixel " + std::to_string(pixel) + " stored nothing");
        waves[word & 0xffffu].emplace_back((word >> 16u) & 0x7fu, pixel);
        Require((word >> 23u) != 0u, what + ": pixel " + std::to_string(pixel) + " counted no lanes in EXEC");
    }
    Require(covered > 1000, what + ": the triangles covered only " + std::to_string(covered) + " pixels");
    for (auto& [first, lanes] : waves) {
        std::sort(lanes.begin(), lanes.end());
        for (std::uint32_t lane = 0; lane < lanes.size(); ++lane) {
            const auto pixel = lanes[lane].second;
            Require(lanes[lane].first == lane, what + ": the wave whose first lane is pixel " + std::to_string(first) + " has " + std::to_string(lanes.size()) + " covered pixels, but pixel " + std::to_string(pixel) + " is lane " + std::to_string(lanes[lane].first) + " of EXEC");
            Require((Lanes[pixel] >> 23u) == lanes.size(), what + ": the wave whose first lane is pixel " + std::to_string(first) + " has " + std::to_string(lanes.size()) + " covered pixels, but EXEC holds " + std::to_string(Lanes[pixel] >> 23u) + " lanes");
        }
        Require(lanes.front().second == first, what + ": the first lane of EXEC, pixel " + std::to_string(first) + ", is not a covered pixel of its wave");
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
            for (std::uint32_t pass = 0; pass < 4; ++pass) {
                Draw(*device, waveSize, device->Target());
                Check(waveSize);
            }
        }
        if (device->Target().subgroupSize == 32u) {
            auto wide = device->Target();
            wide.subgroupSize = 64u;
            for (std::uint32_t pass = 0; pass < 4; ++pass) {
                Draw(*device, 32u, wide);
                Check(32u);
            }
        }
        std::puts("pixel helper lane tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
