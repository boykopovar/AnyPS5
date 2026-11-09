#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t TwoChannelSwizzle = 0x22cu;
constexpr std::size_t BlockBytes = 65536;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> PixelCode{
    0x7e3c0f00, 0x7e3e0f01, 0x7e280300, 0x7e2a0301, 0x7e2c02f2, 0x7e2e02f5, 0xf0201f08, 0x0001141e,
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

struct Format {
    const char* name;
    std::uint32_t value;
    std::uint32_t components;
};

constexpr std::array<Format, 3> Formats{{
    {"16_FLOAT", 13u, 1u},
    {"16_16_FLOAT", 29u, 2u},
    {"16_16_16_16_FLOAT", 71u, 4u},
}};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "pixel float16 store: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::uint16_t HalfTowardZero(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto sign = static_cast<std::uint16_t>((bits >> 16u) & 0x8000u);
    const int unbiased = static_cast<int>((bits >> 23u) & 0xffu) - 127;
    return static_cast<std::uint16_t>(sign | (static_cast<std::uint16_t>(unbiased + 15) << 10u) | ((bits & 0x7fffffu) >> 13u));
}

std::array<std::uint16_t, 4> Expected(std::uint32_t x, std::uint32_t y) {
    return {HalfTowardZero(static_cast<float>(x) + 0.5f), HalfTowardZero(static_cast<float>(y) + 0.5f), 0x3c00u, 0xc000u};
}

AgcDriver::Graphics::TileMipLayout Layout(const Format& format) {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.value, Width, Height, 1u).at(0);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, const Format& format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format.value << 20u) | (((Width - 1u) & 3u) << 30u),
            ((Width - 1u) >> 2u) | ((Height - 1u) << 14u), (format.components == 2u ? TwoChannelSwizzle : IdentitySwizzle) | (Type2D << 28u), 0u, 0u, 0u, 0u};
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint8_t* texels, const Format& format) {
    static const std::vector<std::array<float, 4>> triangles{
        {-1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
        {1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, 1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
    };
    const auto& target = device.Target();
    Pixels.fill(std::byte{0});
    std::fill(texels, texels + BlockBytes, 0x5au);

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
    std::vector<std::uint32_t> pixelUserData(16, 0u);
    const auto texture = TextureDescriptor(texels, format);
    std::copy(texture.begin(), texture.end(), pixelUserData.begin() + 4);
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
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), BlockBytes, nullptr, "test");
    device.WaitIdle();
}

void Check(const std::uint8_t* texels, const Format& format, std::uint32_t waveSize) {
    const auto layout = Layout(format);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = layout.tiledOffset + static_cast<std::uint64_t>(y) * layout.pitchBytes + static_cast<std::uint64_t>(x) * format.components * sizeof(std::uint16_t);
            const auto expected = Expected(x, y);
            for (std::uint32_t component = 0; component < format.components; ++component) {
                std::uint16_t actual = 0u;
                std::memcpy(&actual, texels + offset + component * sizeof(actual), sizeof(actual));
                Require(actual == expected[component], std::string("pixel shader wave") + std::to_string(waveSize) + " image_store of " + format.name + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[component]));
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        std::vector<std::uint32_t> waves{64u};
        if (device->Target().subgroupSize >= 32u) waves.push_back(32u);
        for (const auto waveSize : waves) {
            for (const auto& format : Formats) {
                Require(Layout(format).tiledOffset + static_cast<std::uint64_t>(Height) * Layout(format).pitchBytes <= BlockBytes, "pixel float16 store: the image does not fit the guest block");
                Draw(*device, waveSize, block.Data(), format);
                Check(block.Data(), format, waveSize);
            }
        }
        std::puts("pixel float16 store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
