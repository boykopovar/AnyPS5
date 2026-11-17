#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Type2D = 9;
constexpr std::array<std::uint8_t, 4> Texel{40u, 120u, 200u, 255u};
constexpr std::uint32_t DisableDegamma = 1u << 31u;
constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0x05500000u, 0u};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint8_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> BiasPixelCode{
    0x10040010, 0x06060300, 0x10060611, 0x7e020212, 0xf0940f08, 0x00610401, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format, std::uint32_t type) {
    constexpr std::uint32_t width = 1u;
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        (width - 1u) >> 2u,
        0xfacu | (type << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Draw(AgcDriver::VulkanDevice& device, std::array<std::uint32_t, 8> texture, std::array<std::uint32_t, 4> sampler) {
    const auto& target = device.Target();
    Pixels.fill(std::byte{0});
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangle.data(), 16u, static_cast<std::uint32_t>(Triangle.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
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
    std::vector<std::uint32_t> pixelUserData(24, 0u);
    std::copy(texture.begin(), texture.end(), pixelUserData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), pixelUserData.begin() + 12);
    pixelUserData[16] = std::bit_cast<std::uint32_t>(0.37f);
    pixelUserData[17] = std::bit_cast<std::uint32_t>(0.21f);
    pixelUserData[18] = std::bit_cast<std::uint32_t>(0.0f);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(BiasPixelCode.data()), std::as_bytes(std::span(BiasPixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(BiasPixelCode.data()), BiasPixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

float SrgbToLinear(std::uint8_t value) {
    const float encoded = static_cast<float>(value) / 255.0f;
    return encoded <= 0.04045f ? encoded / 12.92f : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}

void Check(const std::string& name, const std::array<std::uint32_t, 4>& expected) {
    for (std::uint32_t index = 0; index < Width * Height; ++index) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto value = std::to_integer<std::uint32_t>(Pixels[index * 4u + component]);
            Require(std::abs(static_cast<int>(value) - static_cast<int>(expected[component])) <= 1, name + ": pixel " + std::to_string(index) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::fill(Texels.begin(), Texels.end(), 0xeeu);
        std::copy(Texel.begin(), Texel.end(), Texels.begin());
        auto disabled = PointSampler;
        disabled[0] |= DisableDegamma;
        const auto decoded = [](std::uint8_t value) { return static_cast<std::uint32_t>(std::lround(SrgbToLinear(value) * 255.0f)); };
        struct Case {
            const char* name;
            std::uint32_t srgb;
            std::uint32_t unorm;
            std::uint32_t channels;
        };
        const std::array<Case, 3> cases{{
            {"8_8_8_8", 130u, 56u, 3u},
            {"8", 128u, 1u, 1u},
            {"8_8", 129u, 14u, 2u},
        }};
        const auto capture = [] {
            std::array<std::uint32_t, 4> pixel{};
            for (std::uint32_t component = 0; component < 4u; ++component) pixel[component] = std::to_integer<std::uint32_t>(Pixels[component]);
            return pixel;
        };
        for (const auto& item : cases) {
            Draw(*device, TextureDescriptor(item.unorm, Type2D), PointSampler);
            const auto raw = capture();
            for (std::uint32_t component = 0; component < item.channels; ++component) Require(raw[component] == Texel[component], std::string(item.name) + " UNORM did not read texel component " + std::to_string(component));
            Check(std::string(item.name) + " UNORM", raw);
            Draw(*device, TextureDescriptor(item.unorm, Type2D), disabled);
            Check(std::string(item.name) + " UNORM with DISABLE_DEGAMMA", raw);
            Draw(*device, TextureDescriptor(item.srgb, Type2D), disabled);
            Check(std::string(item.name) + " sRGB with DISABLE_DEGAMMA", raw);
            Draw(*device, TextureDescriptor(item.srgb, Type2D), PointSampler);
            auto expected = raw;
            for (std::uint32_t component = 0; component < item.channels; ++component) expected[component] = decoded(static_cast<std::uint8_t>(raw[component]));
            Check(std::string(item.name) + " sRGB", expected);
        }
        std::puts("disabled degamma tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
