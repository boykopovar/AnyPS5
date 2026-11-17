#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type3D = 10;
constexpr std::array<std::uint8_t, 4> Texel{40u, 120u, 200u, 255u};
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

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t width, std::uint32_t type) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((width - 1u) & 3u) << 30u),
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

void Check(const std::string& name) {
    for (std::uint32_t index = 0; index < Width * Height; ++index) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto value = std::to_integer<std::uint32_t>(Pixels[index * 4u + component]);
            Require(value == Texel[component], "single-texel image, " + name + ": pixel " + std::to_string(index) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(Texel[component]));
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
        struct Case {
            const char* name;
            std::array<std::uint32_t, 4> sampler;
        };
        const std::array<Case, 4> cases{{
            {"a buffer descriptor read as an unnormalized point sampler", {0x938cfd00u, 0x00100010u, 0x0000002du, 0x0004dfacu}},
            {"an image descriptor read as an unnormalized point sampler", {0x10049900u, 0x03800000u, 0x00000000u, 0xa0500f2eu}},
            {"an unnormalized anisotropic wrap sampler", {0x00008600u, 0x00fff000u, 0x09f00000u, 0u}},
            {"a normalized anisotropic sampler with linear mips", {0x00000692u, 0x00fff000u, 0x0af00000u, 0u}},
        }};
        for (const auto type : {Type2D, Type3D}) {
            for (const auto& item : cases) {
                Draw(*device, TextureDescriptor(1u, type), item.sampler);
                Check(std::string(type == Type2D ? "2D, " : "one-slice volume, ") + item.name);
            }
        }
        std::string refusal;
        try {
            Draw(*device, TextureDescriptor(2u, Type2D), cases[2].sampler);
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find("unnormalized coordinates with anisotropic filtering") != std::string::npos, "a two-texel image kept its unnormalized anisotropic sampler refused: " + refusal);
        std::puts("single-texel image with any sampler tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
