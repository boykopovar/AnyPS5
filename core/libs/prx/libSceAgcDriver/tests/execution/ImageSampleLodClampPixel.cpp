#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
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
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Levels = 5;
constexpr std::uint32_t MaxLod = 0xfffu;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint8_t, 16384> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> PixelCode{
    0x7e0a0210, 0x7e0c0211, 0x7e0e0212, 0x7e100213, 0x7e120214, 0x7e140215, 0x7e160216, 0xf08c0f08,
    0x00610c05, 0xbf8c3f70, 0xf800180f, 0x0f0e0d0c, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

struct Case {
    std::uint32_t minLod;
    std::uint32_t baseLevel;
    float gradientLod;
    float clamp;
};

constexpr std::array<Case, 9> Cases{{
    {0x200u, 0u, 2.75f, 1.0f},
    {0x200u, 0u, 3.75f, 0.0f},
    {0x200u, 0u, 0.75f, 0.0f},
    {0x300u, 0u, 1.75f, 2.0f},
    {0x200u, 1u, 2.75f, 0.0f},
    {0x2c0u, 1u, 0.25f, 0.0f},
    {0x100u, 0u, 0.75f, 0.0f},
    {0u, 0u, 0.75f, 2.0f},
    {0u, 0u, 3.75f, 1.0f},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const Case& item) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (item.minLod << 8u) | (Format8888UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (item.baseLevel << 12u) | ((Levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

void FillTexture() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888UNorm, Size, Size, Levels);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "image sample LOD clamp in a pixel shader: the mip chain does not fit the texel storage");
    Texels.fill(0xeeu);
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = mips[level];
        for (std::uint32_t y = 0; y < mip.height; ++y) {
            for (std::uint32_t x = 0; x < mip.width; ++x) {
                auto* texel = &Texels[mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u];
                texel[0] = static_cast<std::uint8_t>(level);
                texel[1] = static_cast<std::uint8_t>(x);
                texel[2] = static_cast<std::uint8_t>(y);
                texel[3] = 255u;
            }
        }
    }
}

std::uint32_t ExpectedLevel(const Case& item) {
    const auto last = Levels - 1u - item.baseLevel;
    const auto view = item.minLod > item.baseLevel * 256u ? static_cast<float>(std::min(item.minLod - item.baseLevel * 256u, last * 256u)) / 256.0f : 0.0f;
    const auto lod = std::min(std::max({item.gradientLod, item.clamp, view, 0.0f}), static_cast<float>(MaxLod) / 256.0f);
    return std::min(static_cast<std::uint32_t>(std::ceil(lod + 0.5f)) - 1u, last) + item.baseLevel;
}

std::uint64_t Draw(AgcDriver::VulkanDevice& device, const Case& item) {
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
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(23, 0u);
    const auto texture = TextureDescriptor(item);
    const std::array<std::uint32_t, 4> sampler{0u, MaxLod << 12u, 1u << 26u, 0u};
    std::copy(texture.begin(), texture.end(), pixelUserData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), pixelUserData.begin() + 12);
    pixelUserData[16] = std::bit_cast<std::uint32_t>(std::exp2(item.gradientLod) / static_cast<float>(Size >> item.baseLevel));
    pixelUserData[20] = std::bit_cast<std::uint32_t>(0.5f / static_cast<float>(Size));
    pixelUserData[21] = std::bit_cast<std::uint32_t>(0.5f / static_cast<float>(Size));
    pixelUserData[22] = std::bit_cast<std::uint32_t>(item.clamp);
    const std::span<const std::uint32_t> pixelCode(PixelCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    Require(!target.registrationModules || pixelResult.registrationModule, "image sample LOD clamp in a pixel shader did not exercise a registration module");
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

std::string Describe(const Case& item) {
    return "image_sample_d_cl at a gradient LOD of " + std::to_string(item.gradientLod) + " and a clamp of " + std::to_string(item.clamp) + " from T# MIN_LOD " + std::to_string(item.minLod) + " over BASE_LEVEL " + std::to_string(item.baseLevel);
}

void Check(const Case& item) {
    const auto expected = ExpectedLevel(item);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto level = std::to_integer<std::uint32_t>(Pixels[(y * Width + x) * 4u]);
            Require(level == expected, Describe(item) + ": pixel " + std::to_string(x) + "," + std::to_string(y) + " samples level " + std::to_string(level) + ", expected " + std::to_string(expected));
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const bool shaderLane = argc > 1 && std::strcmp(argv[1], "--shader-lane") == 0;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityMinLod)) {
            std::puts("skipped, the device has no shaderResourceMinLod");
            return VulkanTestSkipped;
        }
        if (shaderLane && !device->Target().shaderImageMinLod) {
            std::puts("skipped, the device does not apply T# minimum LODs in the shader under APS5_NO_IMAGE_VIEW_MIN_LOD");
            return VulkanTestSkipped;
        }
        FillTexture();
        const auto first = Draw(*device, Cases.front());
        Check(Cases.front());
        for (const auto& item : Cases) {
            const auto artifact = Draw(*device, item);
            Require(!device->Target().registrationModules || artifact == first, Describe(item) + " did not reuse the registered fragment artifact");
            Check(item);
        }
        std::puts(device->Target().shaderImageMinLod ? "image sample LOD clamp pixel tests passed in the shader-clamp lane" : "image sample LOD clamp pixel tests passed in the image-view lane");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
