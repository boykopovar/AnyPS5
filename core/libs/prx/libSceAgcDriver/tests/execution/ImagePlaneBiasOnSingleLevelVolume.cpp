#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
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
constexpr std::uint32_t Mips = 1;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type3D = 10;
constexpr float SecondCoordinateScale = 1.0f / static_cast<float>(Size);
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint8_t, 16384> Volume{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> BiasPixelCode{
    0x10040010, 0x06060300, 0x10060611, 0xf0940f08, 0x00610401, 0xbf8c3f70, 0xf800180f, 0x07060504, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 8> VolumeDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Volume.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((Mips - 1u) << 16u) | (Type3D << 28u),
        0u,
        (Mips - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void FillTexture() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(VolumeDescriptor()));
    Require(geometry.guestBytes <= Volume.size(), "single-level volume does not fit the texel storage");
    Require(geometry.mips.size() == 1u && geometry.imageDepth == 1u, "single-level volume geometry differs");
    std::fill(Volume.begin(), Volume.end(), 0xeeu);
    const auto& mip = geometry.mips.front();
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            auto* texel = &Volume[mip.tiledOffset + y * mip.pitchBytes + x * 4u];
            texel[0] = static_cast<std::uint8_t>(x * 8u);
            texel[1] = static_cast<std::uint8_t>(y * 8u);
            texel[2] = 99u;
            texel[3] = 255u;
        }
    }
}

void Draw(AgcDriver::VulkanDevice& device, std::array<std::uint32_t, 8> texture, std::array<std::uint32_t, 4> sampler, float bias, bool mixedSamplers = false) {
    const auto& target = device.Target();
    alignas(256) std::array<std::uint32_t, 13> code{};
    std::copy_n(BiasPixelCode.begin(), 3u, code.begin());
    code[3] = 0x7e020212u;
    std::copy(BiasPixelCode.begin() + 3, BiasPixelCode.end(), code.begin() + 4);
    if (mixedSamplers) {
        code[6] = 0xf0940f08u;
        code[7] = 0x00a10801u;
        code[8] = 0xbf8c3f70u;
        code[9] = 0x06081104u;
        code[10] = 0xf800180fu;
        code[11] = 0x07060504u;
        code[12] = 0xbf810000u;
    }
    const auto pixelCode = std::span(code).first(mixedSamplers ? 13u : 10u);
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
    pixelUserData[16] = std::bit_cast<std::uint32_t>(1.0f / static_cast<float>(Size));
    pixelUserData[17] = std::bit_cast<std::uint32_t>(SecondCoordinateScale);
    pixelUserData[18] = std::bit_cast<std::uint32_t>(bias);
    auto secondSampler = sampler;
    secondSampler[0] &= ~(1u << 20u);
    std::copy(secondSampler.begin(), secondSampler.end(), pixelUserData.begin() + 20);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(std::span(pixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(bool degamma = false) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            std::array<std::uint32_t, 4> expected{(x % Size) * 8u, ((x + y + 1u) % Size) * 8u, 99u, 255u};
            if (degamma) {
                for (std::uint32_t component = 0; component < 3u; ++component) {
                    const float encoded = static_cast<float>(expected[component]) / 255.0f;
                    const float linear = encoded <= 0.04045f ? encoded / 12.92f : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
                    expected[component] = static_cast<std::uint32_t>(std::lround(linear * 255.0f));
                }
            }
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto value = std::to_integer<std::uint32_t>(Pixels[(y * Width + x) * 4u + component]);
                Require(std::abs(static_cast<int>(value) - static_cast<int>(expected[component])) <= (degamma ? 1 : 0), "single-level volume bias: pixel " + std::to_string(x) + "," + std::to_string(y) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[component]));
            }
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::array<std::uint32_t, 8> texture, std::array<std::uint32_t, 4> sampler, const char* expected) {
    std::string refusal;
    try {
        Draw(device, texture, sampler, 0.0f);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "single-level volume refusal differs: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture();
        for (const float bias : {-16.0f, -0.5f, 0.0f, 0.5f, 16.0f}) {
            Draw(*device, VolumeDescriptor(), SamplerDescriptor(), bias);
            Check();
        }
        for (const std::uint32_t reduction : {1u, 2u}) {
            auto sampler = SamplerDescriptor();
            sampler[0] |= reduction << 29u;
            Draw(*device, VolumeDescriptor(), sampler, 8.0f);
            Check();
        }
        auto integerTexture = VolumeDescriptor();
        integerTexture[1] = (integerTexture[1] & ~(0x1ffu << 20u)) | (60u << 20u);
        auto forcedSampler = SamplerDescriptor();
        forcedSampler[0] |= 1u << 20u;
        for (const float bias : {-16.0f, 0.0f, 16.0f}) {
            Draw(*device, integerTexture, forcedSampler, bias);
            Check(true);
        }
        std::string mixedRefusal;
        try {
            Draw(*device, integerTexture, forcedSampler, 0.0f, true);
        } catch (const std::exception& error) {
            mixedRefusal = error.what();
        }
        Require(mixedRefusal.find("mixed sampler states") != std::string::npos, "mixed FORCE_DEGAMMA samplers were not refused: " + mixedRefusal);
        auto texture = VolumeDescriptor();
        texture[4] = 1u;
        RequireRefused(*device, texture, SamplerDescriptor(), "one slice and mip level zero only");
        texture = VolumeDescriptor();
        texture[3] |= 1u << 16u;
        texture[5] |= 1u << 4u;
        RequireRefused(*device, texture, SamplerDescriptor(), "one slice and mip level zero only");
        auto sampler = SamplerDescriptor();
        sampler[2] |= (1u << 20u) | (1u << 22u);
        RequireRefused(*device, VolumeDescriptor(), sampler, "normalized point sampler");
        sampler = SamplerDescriptor();
        sampler[0] |= 1u << 15u;
        RequireRefused(*device, VolumeDescriptor(), sampler, "normalized point sampler");
        std::puts("image plane bias on single-level volume tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
