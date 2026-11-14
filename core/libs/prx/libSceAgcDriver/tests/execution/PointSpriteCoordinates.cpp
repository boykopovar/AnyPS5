#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Extent = 64u;
constexpr float PointSide = 16.0f;
constexpr std::uint32_t SpriteTex = 1u << 17u;
alignas(256) std::array<float, Extent * Extent * 4u> Pixels{};
alignas(256) constexpr std::array<float, 4> Position{0.0f, 0.0f, 0.25f, 1.0f};

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> PixelCode{
    0xbefe0a7e, 0xc8080000, 0xc8090001, 0xc80c0100, 0xc80d0101, 0xc8100200, 0xc8110201, 0xc8140300, 0xc8150301,
    0xf800180f, 0x05040302, 0xbf810000,
};

enum Select : std::uint32_t { Zero = 0u, One = 1u, S = 2u, T = 3u, None = 4u };

constexpr std::uint32_t Control(Select x, Select y, Select z, Select w, bool topOne) {
    return 0x2u | (x << 2u) | (y << 5u) | (z << 8u) | (w << 11u) | (topOne ? 0x4000u : 0u);
}

struct Case {
    const char* name;
    std::uint32_t control;
    std::array<Select, 4> selects;
    bool topOne;
};

float Expected(const Case& sprite, std::uint32_t component, std::uint32_t x, std::uint32_t y) {
    const float left = Extent / 2.0f - PointSide / 2.0f;
    const float s = (static_cast<float>(x) + 0.5f - left) / PointSide;
    const float t = (static_cast<float>(y) + 0.5f - left) / PointSide;
    if (sprite.control == 0u) return Position[component];
    switch (sprite.selects[component]) {
    case Zero: return 0.0f;
    case One: return 1.0f;
    case S: return s;
    case T: return sprite.topOne ? 1.0f - t : t;
    default: return Position[component];
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, bool barycentric, const Case& sprite) {
    auto target = device.Target();
    target.fragmentShaderBarycentricEnabled = target.fragmentShaderBarycentricEnabled && barycentric;
    const auto address = reinterpret_cast<std::uintptr_t>(Position.data());
    const std::array<std::uint32_t, 4> vertexUserData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), 1u, 0x01016facu};
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.pointList = true;
    vertexInfo.pointSize = PointSide;
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target, {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.interpolatorCount = 1;
    pixel.interpolatorSettings[0] = SpriteTex;
    pixel.pointSpriteControl = sprite.control;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.executeOnNoop = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    pixel.rasterPrimitive = ShaderRecompiler::RasterPrimitive::Point;
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, pixel, std::nullopt, pixelMemory}, target, PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Extent, Extent}, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Pixels), 0xe4u, AgcDriver::Graphics::ColorTileMode::Linear, 16u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Extent, Extent};
    state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    state.vertexPoints = true;
    state.pointSize = PointSide;
    state.pointSpriteControl = sprite.control;
    state.viewport = {0, static_cast<float>(Extent), static_cast<float>(Extent), -static_cast<float>(Extent), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Extent, Extent}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    Pixels.fill(-7.0f);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertexResult, 0}, {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}}};
    device.Draw(state, {0, 1, 0, 1, 0, false}, shaders);
    device.WaitIdle();
    const auto first = static_cast<std::uint32_t>(Extent / 2u - static_cast<std::uint32_t>(PointSide) / 2u);
    const auto last = first + static_cast<std::uint32_t>(PointSide);
    std::uint32_t covered = 0;
    for (std::uint32_t y = 0; y < Extent; ++y) {
        for (std::uint32_t x = 0; x < Extent; ++x) {
            const bool inside = x >= first && x < last && y >= first && y < last;
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto value = Pixels[(y * Extent + x) * 4u + component];
                const auto expected = inside ? Expected(sprite, component, x, y) : -7.0f;
                Require(std::fabs(value - expected) <= 1e-3f, std::string(sprite.name) + " wave" + std::to_string(waveSize) + (barycentric ? "" : " host interpolation") + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
            }
            covered += inside ? 1u : 0u;
        }
    }
    std::printf("%s wave%u%s: %u point pixels match\n", sprite.name, waveSize, barycentric ? "" : " host interpolation", covered);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const std::array<Case, 3> cases{{
            {"S T 0 1", Control(S, T, Zero, One, false), {S, T, Zero, One}, false},
            {"T S attribute 0 with T 1 at the top", Control(T, S, None, Zero, true), {T, S, None, Zero}, true},
            {"point sprites off", 0u, {None, None, None, None}, false},
        }};
        for (const auto wave : {32u, 64u}) {
            for (const bool barycentric : {true, false}) {
                for (const auto& sprite : cases) Run(*device, wave, barycentric, sprite);
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
