#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Color = std::array<float, 4>;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t ScissorWidth = 128;
constexpr std::size_t BlockBytes = 65536;

struct TargetFormat {
    const char* name;
    std::uint32_t info;
    VkFormat format;
    std::uint32_t elementBytes;
    bool normalized;
};

constexpr std::array<TargetFormat, 2> Targets{{
    {"8_8 UNORM", (3u << 2u) | (1u << 11u) | 0x8000u, VK_FORMAT_R8G8_UNORM, 2u, true},
    {"16_16 FLOAT", (5u << 2u) | (7u << 8u) | (1u << 11u) | 0x8000u, VK_FORMAT_R16G16_SFLOAT, 4u, false},
}};
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

constexpr std::array<std::array<std::uint8_t, 2>, 4> Destinations{{
    {64, 128}, {255, 0}, {32, 224}, {128, 64}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 16> Programs{};
std::size_t programCount = 0;

std::span<const std::uint32_t> Program(const Color& exported) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        code[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(exported[channel]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

std::uint16_t HalfBits(float value) {
    if (value == 0.0f) return 0u;
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto sign = (bits >> 16u) & 0x8000u;
    const auto exponent = static_cast<std::int32_t>((bits >> 23u) & 0xffu) - 127 + 15;
    Require(exponent > 0 && exponent < 31, "the test value is outside the normal half range");
    const auto mantissa = bits & 0x7fffffu;
    auto half = sign | (static_cast<std::uint32_t>(exponent) << 10u) | (mantissa >> 13u);
    if ((mantissa & 0x1fffu) > 0x1000u || ((mantissa & 0x1fffu) == 0x1000u && (half & 1u) != 0u)) ++half;
    return static_cast<std::uint16_t>(half);
}

float HalfValue(std::uint16_t half) {
    const auto sign = (half & 0x8000u) != 0u ? -1.0f : 1.0f;
    const auto exponent = static_cast<std::int32_t>((half >> 10u) & 0x1fu);
    const auto mantissa = static_cast<float>(half & 0x3ffu);
    if (exponent == 0) return sign * std::ldexp(mantissa, -24);
    return sign * std::ldexp(1.0f + mantissa / 1024.0f, exponent - 15);
}

float Channel(const TargetFormat& target, const std::uint8_t* texel, std::uint32_t channel) {
    if (target.normalized) return static_cast<float>(texel[channel]) / 255.0f;
    std::uint16_t half = 0;
    std::memcpy(&half, texel + channel * 2u, sizeof(half));
    return HalfValue(half);
}

void Store(const TargetFormat& target, std::uint8_t* texel, std::uint32_t channel, std::uint8_t value) {
    if (target.normalized) {
        texel[channel] = value;
        return;
    }
    const auto half = HalfBits(static_cast<float>(value) / 255.0f);
    std::memcpy(texel + channel * 2u, &half, sizeof(half));
}

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes), watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map the color target");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "cannot allocate the color target");
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "the color target is not write-watched");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        } else {
            std::free(data);
        }
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    bool watched;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, const TargetFormat& target) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = target.info;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == target.format && color.componentMapping == 0xecu && color.elementBytes == target.elementBytes && color.bytes == block.bytes, std::string("the ") + target.name + " SWAP_ALT target did not decode as a 256x128 two-channel surface holding the x and w exports");
    return color;
}

VkPipelineColorBlendAttachmentState Blend(VkBlendFactor source, VkBlendFactor destination, VkBlendOp operation, VkBlendFactor sourceAlpha, VkBlendFactor destinationAlpha, VkBlendOp alphaOperation) {
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = source;
    blend.dstColorBlendFactor = destination;
    blend.colorBlendOp = operation;
    blend.srcAlphaBlendFactor = sourceAlpha;
    blend.dstAlphaBlendFactor = destinationAlpha;
    blend.alphaBlendOp = alphaOperation;
    blend.colorWriteMask = 0xf;
    return blend;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const VkPipelineColorBlendAttachmentState& blend, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
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

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {ScissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state)), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint8_t> Fill(Block& block, const ColorTargetLayout& layout, const TargetFormat& target) {
    std::vector<std::uint8_t> bytes(block.bytes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto& color = Destinations[(x + y) % Destinations.size()];
            const auto index = layout.Offset(x, y);
            Store(target, bytes.data() + index, 0, color[0]);
            Store(target, bytes.data() + index, 1, color[1]);
        }
    }
    std::memcpy(block.data, bytes.data(), block.bytes);
    return bytes;
}

void Check(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const TargetFormat& target, const std::vector<std::uint8_t>& before, const std::function<std::array<float, 2>(float first, float second)>& blended, const std::string& what) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(block.bytes);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y);
            const auto expected = blended(Channel(target, before.data() + index, 0), Channel(target, before.data() + index, 1));
            for (std::uint32_t channel = 0; channel < 2; ++channel) {
                const bool inside = x < ScissorWidth;
                const auto want = target.normalized ? std::clamp(expected[channel], 0.0f, 1.0f) : expected[channel];
                const auto held = Channel(target, stored.data() + index, channel);
                const auto channelBytes = target.elementBytes / 2u;
                const bool match = inside ? std::fabs(held - want) <= (target.normalized ? 1.0f / 255.0f : 1.0f / 512.0f) : std::memcmp(stored.data() + index + channel * channelBytes, before.data() + index + channel * channelBytes, channelBytes) == 0;
                if (match) continue;
                char detail[192];
                std::snprintf(detail, sizeof(detail), ": channel %u of pixel (%u, %u) %s the scissor holds %.4f, expected %.4f", channel, x, y, inside ? "inside" : "outside", held, inside ? want : Channel(target, before.data() + index, channel));
                throw std::runtime_error(std::string(target.name) + ", " + what + detail);
            }
        }
    }
}

struct BlendCase {
    const char* name;
    VkPipelineColorBlendAttachmentState blend;
    bool split;
    std::function<std::array<float, 2>(float first, float second)> blended;
};

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const Color exported{0.25f, 0.75f, 0.125f, 0.5f};
        const float x = exported[0];
        const float w = exported[3];
        const std::array<BlendCase, 4> cases{{
            {"SRC_ALPHA + ONE_MINUS_SRC_ALPHA, alpha ONE + ZERO", Blend(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD), true,
                [&](float r, float) { return std::array<float, 2>{x * w + r * (1.0f - w), w}; }},
            {"ONE + ONE, alpha ZERO + ONE_MINUS_SRC_ALPHA", Blend(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD), true,
                [&](float r, float g) { return std::array<float, 2>{x + r, g * (1.0f - w)}; }},
            {"ONE + ZERO, alpha ONE reverse-subtract ONE", Blend(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_REVERSE_SUBTRACT), true,
                [&](float, float g) { return std::array<float, 2>{x, g - w}; }},
            {"ONE + ONE_MINUS_SRC_ALPHA for color and alpha", Blend(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD), false,
                [&](float r, float g) { return std::array<float, 2>{x + r * (1.0f - w), w + g * (1.0f - w)}; }},
        }};
        for (const auto& target : Targets) {
            const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, target.elementBytes);
            Block block(layout.Bytes());
            for (const auto& test : cases) {
                const auto before = Fill(block, layout, target);
                const auto blockTarget = DecodeTarget(block, target);
                AgcDriver::Graphics::State probe{};
                probe.colors = {blockTarget};
                probe.blends = {test.blend};
                Require(AgcDriver::Graphics::SplitsAlternateAlpha(probe) == test.split, std::string(target.name) + ", " + test.name + ": the split decision is wrong");
                Draw(*device, blockTarget, test.blend, Program(exported));
                Check(*device, block, layout, target, before, test.blended, test.name);
            }
        }
        std::puts("8_8 and 16_16 SWAP_ALT blend tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
