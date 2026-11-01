#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Levels = 5;
constexpr std::uint32_t PassLimit = 80;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height * 3> Results{};
alignas(256) std::array<std::uint8_t, 16384> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 48> PixelCode{
    0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, 0x000000c0, 0xbec4047e, 0x7e120506, 0xbefe0a7e,
    0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, 0x000000c0, 0x102800ff, 0x3baaaaab, 0x102a02ff,
    0x3c000000, 0xf0800f08, 0x01060c14, 0xbe8a047e, 0xbe8c0380, 0x7e0e0280, 0xbe940480, 0xbe82140a,
    0xd7600003, 0x00000506, 0xd4c20014, 0x00020c03, 0xd5010010, 0x00510280, 0x4a0e0f10, 0x8a8a140a,
    0x800c810c, 0xbf09ff0c, 0x00000050, 0xbf850002, 0xbf13800a, 0xbf85fff1, 0xbefe0444, 0x7e100209,
    0x7e12020c, 0x7e140307, 0xe07c2000, 0x80010806, 0xbf8c3f70, 0xf800180f, 0x0f0e0d0c, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Texels.data());
    return {static_cast<std::uint32_t>(address >> 8), static_cast<std::uint32_t>((address >> 40) & 0xffu) | (56u << 20) | (3u << 30),
            ((Size - 1u) >> 2) | ((Size - 1u) << 14), 0xfacu | ((Levels - 1u) << 16) | (9u << 28), 0u, (Levels - 1u) << 4, 0u, 0u};
}

void FillTexture() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor()));
    Require(geometry.guestBytes <= Texels.size(), "helper waterfall texture does not fit");
    Texels.fill(0xffu);
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
    Results.fill(Sentinel);

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
    std::vector<std::uint32_t> pixelUserData(36, 0u);
    const auto results = BufferDescriptor(Results.data(), 12u, static_cast<std::uint32_t>(Results.size() / 3u));
    std::copy(results.begin(), results.end(), pixelUserData.begin() + 4);
    const auto texture = TextureDescriptor();
    std::copy(texture.begin(), texture.end(), pixelUserData.begin() + 24);
    const std::array<std::uint32_t, 4> sampler{0u, 0xfffu << 12, 1u << 26, 0u};
    std::copy(sampler.begin(), sampler.end(), pixelUserData.begin() + 32);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
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
    const auto what = "pixel helper waterfall wave" + std::to_string(waveSize);
    std::map<std::uint32_t, std::vector<std::uint32_t>> waves;
    std::uint32_t partialQuads = 0;
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        if (Pixels[pixel * 4u + 3u] == std::byte{0}) {
            Require(Results[pixel * 3u] == Sentinel, what + ": uncovered pixel " + std::to_string(pixel) + " stored a result");
            continue;
        }
        Require(Results[pixel * 3u] != Sentinel, what + ": covered pixel " + std::to_string(pixel) + " stored nothing");
        waves[Results[pixel * 3u]].push_back(pixel);
        const auto quad = (pixel / Width & ~1u) * Width + (pixel % Width & ~1u);
        if (quad == pixel) {
            for (const auto offset : {1u, Width, Width + 1u}) {
                if (Pixels[(quad + offset) * 4u + 3u] == std::byte{0}) {
                    ++partialQuads;
                    break;
                }
            }
        }
    }
    Require(waves.size() > 4u && partialQuads > 16u, what + ": the triangles made only " + std::to_string(waves.size()) + " waves and " + std::to_string(partialQuads) + " partly covered quads");
    for (const auto& [first, live] : waves) {
        std::set<std::uint32_t> quads;
        for (const auto pixel : live) quads.insert((pixel / Width & ~1u) * Width + (pixel % Width & ~1u));
        const auto lanes = static_cast<std::uint32_t>(quads.size() * 4u);
        const auto passes = Results[live.front() * 3u + 1u];
        const auto wave = what + ": the wave whose first lane is pixel " + std::to_string(first);
        Require(passes < PassLimit, wave + " hit the pass limit");
        Require(passes >= live.size() && passes <= lanes, wave + " made " + std::to_string(passes) + " passes over " + std::to_string(live.size()) + " live lanes in " + std::to_string(quads.size()) + " quads");
        for (const auto pixel : live) {
            Require(Results[pixel * 3u + 1u] == passes, wave + ": pixel " + std::to_string(pixel) + " counted " + std::to_string(Results[pixel * 3u + 1u]) + " passes, its first lane " + std::to_string(passes));
            Require(Results[pixel * 3u + 2u] == 1u, wave + ": pixel " + std::to_string(pixel) + " was picked " + std::to_string(Results[pixel * 3u + 2u]) + " times");
        }
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
        FillTexture();
        for (const auto waveSize : {64u, 32u}) {
            for (std::uint32_t pass = 0; pass < 4; ++pass) {
                Draw(*device, waveSize, device->Target());
                Check(waveSize);
            }
        }
        std::puts("pixel helper waterfall tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
