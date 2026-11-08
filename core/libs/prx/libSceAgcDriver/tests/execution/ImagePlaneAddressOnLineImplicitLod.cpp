#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Mips = 5;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1D = 8;
constexpr float SecondCoordinateScale = 3.0f;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint8_t, 16384> Line{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> PixelCode{
    0xbefe0a7e, 0x10040010, 0x06060300, 0x10060611, 0xf0800f08, 0x00610402, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> BiasPixelCode{
    0x10040010, 0x06060300, 0x10060611, 0xf0940f08, 0x00610401, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 8> LineDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Line.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        (Size - 1u) >> 2u,
        0xfacu | ((Mips - 1u) << 16u) | (Type1D << 28u),
        0u,
        (Mips - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

std::uint8_t LevelMark(std::uint32_t level) {
    return static_cast<std::uint8_t>(10u + 40u * level);
}

void FillTexture() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(LineDescriptor()));
    Require(geometry.guestBytes <= Line.size(), "implicit LOD through a 1D texture: the texture does not fit the texel storage");
    Require(geometry.mips.size() == Mips, "implicit LOD through a 1D texture: the texture has " + std::to_string(geometry.mips.size()) + " mips");
    std::fill(Line.begin(), Line.end(), 0xeeu);
    for (std::uint32_t level = 0; level < Mips; ++level) {
        const auto& mip = geometry.mips[level];
        for (std::uint32_t x = 0; x < (Size >> level); ++x) {
            auto* texel = &Line[geometry.GuestLayerOffset(0) + mip.tiledOffset + x * 4u];
            texel[0] = LevelMark(level);
            texel[1] = static_cast<std::uint8_t>(x * 8u);
            texel[2] = 99u;
            texel[3] = 255u;
        }
    }
}

std::uint64_t Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, float firstCoordinateScale, std::uint32_t swizzle = 0xfacu) {
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
    std::vector<std::uint32_t> pixelUserData(18, 0u);
    auto texture = LineDescriptor();
    texture[3] = (texture[3] & ~0xfffu) | swizzle;
    const auto sampler = SamplerDescriptor();
    std::copy(texture.begin(), texture.end(), pixelUserData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), pixelUserData.begin() + 12);
    pixelUserData[16] = std::bit_cast<std::uint32_t>(firstCoordinateScale);
    pixelUserData[17] = std::bit_cast<std::uint32_t>(SecondCoordinateScale);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    Require(!target.registrationModules || pixelResult.registrationModule, "line swizzle did not exercise its requested registration module");
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
    return pixelResult.variantId;
}

void Check(std::uint32_t level, std::uint32_t swizzle = 0xfacu) {
    const auto what = "image_sample 2d on a 1D texture with the first coordinate advancing " + std::to_string(1u << level) + " texels per pixel";
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const std::array<std::uint32_t, 4> source{LevelMark(level), (x % (Size >> level)) * 8u, 99u, 255u};
            std::array<std::uint32_t, 4> expected{};
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto selector = (swizzle >> (component * 3u)) & 7u;
                expected[component] = selector == 0u ? 0u : selector == 1u ? 255u : source.at(selector - 4u);
            }
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto value = std::to_integer<std::uint32_t>(Pixels[(y * Width + x) * 4u + component]);
                Require(value == expected[component], what + ": pixel " + std::to_string(x) + "," + std::to_string(y) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
            }
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device) {
    std::string refusal;
    try {
        Draw(device, BiasPixelCode, 1.0f / static_cast<float>(Size));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("incompatible with the static runtime image interface") != std::string::npos, "image_sample_b 2d on a 1D texture was not refused: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture();
        for (std::uint32_t level = 0; level < 3u; ++level) {
            Draw(*device, PixelCode, static_cast<float>(1u << level) / static_cast<float>(Size));
            Check(level);
        }
        const float scale = 1.0f / static_cast<float>(Size);
        const auto original = Draw(*device, PixelCode, scale);
        Check(0u);
        constexpr std::array<std::uint32_t, 6> selectors{0u, 1u, 4u, 5u, 6u, 7u};
        for (std::uint32_t rotation = 0; rotation < selectors.size(); ++rotation) {
            std::uint32_t swizzle = 0u;
            for (std::uint32_t component = 0; component < 4u; ++component) swizzle |= selectors[(rotation + component) % selectors.size()] << (component * 3u);
            const auto artifact = Draw(*device, PixelCode, scale, swizzle);
            Require(!device->Target().registrationModules || artifact == original, "line swizzle changes did not reuse their registered fragment artifact");
            Check(0u, swizzle);
        }
        for (std::uint32_t pattern = 0; pattern < 16u; ++pattern) {
            std::uint32_t swizzle = 0u;
            for (std::uint32_t component = 0; component < 4u; ++component) swizzle |= ((pattern >> component) & 1u) << (component * 3u);
            const auto artifact = Draw(*device, PixelCode, scale, swizzle);
            Require(!device->Target().registrationModules || artifact == original, "constant swizzle changes did not reuse their registered fragment artifact");
            Check(0u, swizzle);
        }
        const auto restored = Draw(*device, PixelCode, scale);
        Require(!device->Target().registrationModules || restored == original, "line swizzle did not restore its original registered artifact");
        Check(0u);
        RequireRefused(*device);
        std::puts("image plane address on line implicit LOD tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
