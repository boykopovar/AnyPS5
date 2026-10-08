#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t TextureWidth = Width * 4u;
constexpr std::uint32_t TextureHeight = Height * 4u;
constexpr std::uint32_t TextureLevels = 9;
constexpr std::uint32_t SampledLevel = 2;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t DemoteToHelperInvocation = 5379;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(4096) std::array<std::byte, 256u * 1024u> Texels{};
alignas(256) std::array<std::array<float, 4>, 3> Triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> ConstantPixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> SampledPixelCode{
    0xbe8c047e, 0xbefe0a7e, 0x100000ff, 0x3c800000, 0x100202ff, 0x3d000000, 0xf0800f08, 0x00400200,
    0xbf8c3f70, 0x87fe0c7e, 0xf800180f, 0x05040302, 0xbf810000,
};

std::array<std::uint8_t, 4> LevelColor(std::uint32_t level) {
    return {static_cast<std::uint8_t>(32u * (level + 1u)), static_cast<std::uint8_t>(255u - 16u * level), static_cast<std::uint8_t>(0x40u + level), 255u};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888Unorm << 20u) | (((TextureWidth - 1u) & 3u) << 30u),
        ((TextureWidth - 1u) >> 2u) | ((TextureHeight - 1u) << 14u),
        0xfacu | ((TextureLevels - 1u) << 16u) | (Type2D << 28u),
        0u, (TextureLevels - 1u) << 4u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void FillTexture() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888Unorm, TextureWidth, TextureHeight, TextureLevels);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "the mipmapped test texture does not fit its storage");
    for (std::uint32_t level = 0; level < TextureLevels; ++level) {
        const auto color = LevelColor(level);
        for (std::uint64_t offset = mips[level].tiledOffset; offset < mips[level].tiledOffset + mips[level].tiledSize; offset += 4u) {
            std::memcpy(Texels.data() + offset, color.data(), color.size());
        }
    }
}

void Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, std::uint8_t quadPixelMask) {
    Pixels.fill(std::byte{0});
    const auto target = device.Target();
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
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    pixel.quadPixelMask = quadPixelMask;
    std::vector<std::uint32_t> pixelUserData(12, 0u);
    const auto texture = TextureDescriptor();
    const auto sampler = SamplerDescriptor();
    std::copy(texture.begin(), texture.end(), pixelUserData.begin());
    std::copy(sampler.begin(), sampler.end(), pixelUserData.begin() + 8);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
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

void Check(const std::string& what, std::uint8_t quadPixelMask, std::array<std::uint8_t, 4> color) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const bool covered = ((quadPixelMask >> ((x & 1u) | ((y & 1u) << 1u))) & 1u) != 0;
            const auto* written = &Pixels[(y * Width + x) * 4u];
            for (std::uint32_t channel = 0; channel < 4u; ++channel) {
                const auto expected = covered ? color[channel] : 0u;
                const auto value = std::to_integer<std::uint32_t>(written[channel]);
                Require(value == expected, what + " with quad pixel mask " + std::to_string(quadPixelMask) + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto capabilities = device->Target().supportedCapabilities;
        const bool demote = std::find(capabilities.begin(), capabilities.end(), DemoteToHelperInvocation) != capabilities.end();
        FillTexture();
        for (const std::uint8_t mask : {std::uint8_t{0xf}, std::uint8_t{0x2}, std::uint8_t{0x9}}) {
            Draw(*device, ConstantPixelCode, mask);
            Check("constant color", mask, {255u, 255u, 255u, 255u});
            if (mask == 0xf || demote) {
                Draw(*device, SampledPixelCode, mask);
                Check("implicit-LOD sample", mask, LevelColor(SampledLevel));
                continue;
            }
            try {
                Draw(*device, SampledPixelCode, mask);
            } catch (const std::exception& error) {
                Require(std::string(error.what()).find("shaderDemoteToHelperInvocation") != std::string::npos, std::string("an implicit-LOD sample under a quad pixel mask failed for another reason: ") + error.what());
                continue;
            }
            Require(false, "an implicit-LOD sample under quad pixel mask " + std::to_string(mask) + " was drawn without demote to helper invocation");
        }
        std::printf("quad pixel mask tests passed (%s)\n", demote ? "demote to helper invocation" : "kill, derivatives refused");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
