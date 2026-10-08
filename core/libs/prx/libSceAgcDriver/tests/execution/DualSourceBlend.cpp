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
constexpr std::uint32_t Float16x4Info = 0x0730u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Float16Export = 4;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

constexpr std::array<Color, 4> Destinations{{
    {0.5f, 0.25f, 0.75f, 0.5f}, {1.0f, 0.0f, 0.125f, 0.25f}, {0.0625f, 0.875f, 0.5f, 1.0f}, {0.25f, 0.5f, 0.375f, 0.75f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 8> Programs{};
std::size_t programCount = 0;

std::uint16_t Half(float value) {
    Require(value == 0.0f || (value >= 0.0625f && value <= 1.0f), "the test's half values are normal and at most 1");
    if (value == 0.0f) return 0;
    const auto bits = std::bit_cast<std::uint32_t>(value);
    Require((bits & 0x1fffu) == 0, "the test's half values are exact");
    return static_cast<std::uint16_t>(((((bits >> 23u) & 0xffu) - 112u) << 10u) | ((bits >> 13u) & 0x3ffu));
}

float FromHalf(std::uint16_t half) {
    const auto exponent = (half >> 10u) & 0x1fu;
    const auto mantissa = half & 0x3ffu;
    Require((half & 0x8000u) == 0 && exponent != 31u, "a blended texel is negative, infinite or NaN");
    return exponent == 0 ? std::ldexp(static_cast<float>(mantissa), -24) : std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f, static_cast<int>(exponent) - 15);
}

std::span<const std::uint32_t> Program(const Color& first, const Color& second, bool compressed) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    const auto move = [&](std::uint32_t vgpr, std::uint32_t value) {
        code[at++] = 0x7e0002ffu | (vgpr << 17u);
        code[at++] = value;
    };
    if (compressed) {
        move(4, Half(first[0]) | (Half(first[1]) << 16u));
        move(5, Half(first[2]) | (Half(first[3]) << 16u));
        move(6, Half(second[0]) | (Half(second[1]) << 16u));
        move(7, Half(second[2]) | (Half(second[3]) << 16u));
        code[at++] = 0xf800040fu;
        code[at++] = 0x00000504u;
        code[at++] = 0xf8001c1fu;
        code[at++] = 0x00000706u;
    } else {
        for (std::uint32_t channel = 0; channel < 4; ++channel) move(4 + channel, std::bit_cast<std::uint32_t>(first[channel]));
        for (std::uint32_t channel = 0; channel < 4; ++channel) move(8 + channel, std::bit_cast<std::uint32_t>(second[channel]));
        code[at++] = 0xf800000fu;
        code[at++] = 0x07060504u;
        code[at++] = 0xf800181fu;
        code[at++] = 0x0b0a0908u;
    }
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
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
        mutation.Add(data, bytes, true, true);
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Float16x4Info;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R16G16B16A16_SFLOAT && color.elementBytes == 8 && color.bytes == block.bytes, "the 16_16_16_16 float target did not decode as a 256x128 RGBA16F surface");
    return color;
}

VkPipelineColorBlendAttachmentState Blend(VkBlendFactor source, VkBlendFactor destination, VkBlendFactor sourceAlpha, VkBlendFactor destinationAlpha) {
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = source;
    blend.dstColorBlendFactor = destination;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = sourceAlpha;
    blend.dstAlphaBlendFactor = destinationAlpha;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = 0xf;
    return blend;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const VkPipelineColorBlendAttachmentState& blend, std::uint32_t exportFormat, std::span<const std::uint32_t> pixelCode) {
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
    state.dualSourceBlend = true;

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, exportFormat | (exportFormat << 4u)}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, {}), std::nullopt, pixelMemory},
        target,
        {0, 0, vertexPush, 128 - vertexPush}
    };
    const auto handle = ShaderRecompiler::PrepareShader(fragment);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{codeAddress, 0, 1, {pixelCode.begin(), pixelCode.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    fragment.shader.code = snapshot.code;
    fragment.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    fragment.context.pixel->dualSourceBlend = state.dualSourceBlend;
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto materialize = [&](const ShaderRecompiler::RecompileRequest& request) {
        const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
        return invocation.Materialize(*invocation.Capture(runtime));
    };
    const auto pixelResult = materialize(fragment);
    auto single = fragment;
    single.context.pixel->dualSourceBlend = false;
    const auto plain = materialize(single);
    Require(pixelResult->variantId == ShaderRecompiler::GetPreparedArtifact(*handle).variantId && plain->variantId == pixelResult->variantId, "the draw did not use the registered pixel shader");
    Require(plain->PipelineVariantId() != pixelResult->PipelineVariantId() && materialize(fragment)->PipelineVariantId() == pixelResult->PipelineVariantId(), "dual-source blending did not key its own specialization");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), vertexPush}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint16_t> ReadBack(AgcDriver::VulkanDevice& device, const Block& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint16_t> stored(block.bytes / 2u);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::vector<std::uint16_t> Fill(Block& block, const ColorTargetLayout& layout) {
    std::vector<std::uint16_t> halves(block.bytes / 2u);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto& color = Destinations[(x + y) % Destinations.size()];
            const auto index = layout.Offset(x, y) / 2u;
            for (std::uint32_t channel = 0; channel < 4; ++channel) halves[index + channel] = Half(color[channel]);
        }
    }
    std::memcpy(block.data, halves.data(), block.bytes);
    return halves;
}

void Check(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const std::vector<std::uint16_t>& before, const std::function<Color(const Color&)>& blended, const std::string& what) {
    const auto stored = ReadBack(device, block);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y) / 2u;
            const auto expected = blended(Destinations[(x + y) % Destinations.size()]);
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                const bool inside = x < ScissorWidth;
                const bool match = inside ? std::fabs(FromHalf(stored[index + channel]) - expected[channel]) <= 1.0f / 2048.0f : stored[index + channel] == before[index + channel];
                if (match) continue;
                char detail[160];
                std::snprintf(detail, sizeof(detail), ": channel %u of pixel (%u, %u) %s the scissor holds half 0x%04x, expected %.6g", channel, x, y, inside ? "inside" : "outside", stored[index + channel], inside ? expected[channel] : FromHalf(before[index + channel]));
                throw std::runtime_error(what + detail);
            }
        }
    }
}

struct BlendCase {
    const char* name;
    VkPipelineColorBlendAttachmentState blend;
    std::function<float(float source, float second, float destination, float secondAlpha)> color;
    std::function<float(float source, float second, float destination)> alpha;
};

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->DualSrcBlend()) {
            std::puts("skipped, the device has no dual-source blending");
            return VulkanTestSkipped;
        }
        const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 8);
        Block block(layout.Bytes());
        const Color first{0.25f, 0.5f, 0.125f, 0.5f};
        const Color second{0.5f, 0.25f, 1.0f, 0.75f};
        const std::array<BlendCase, 3> cases{{
            {"ONE + SRC1_COLOR", Blend(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC1_COLOR, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC1_COLOR),
                [](float s, float t, float d, float) { return s + t * d; }, [](float s, float t, float d) { return s + t * d; }},
            {"SRC1_ALPHA + ONE_MINUS_SRC1_ALPHA", Blend(VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA),
                [](float s, float, float d, float a) { return s * a + d * (1.0f - a); }, [](float, float t, float d) { return d * (1.0f - t); }},
            {"ONE_MINUS_SRC1_COLOR + ZERO", Blend(VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ZERO),
                [](float s, float t, float, float) { return s * (1.0f - t); }, [](float s, float t, float) { return s * t; }},
        }};
        for (const bool compressed : {false, true}) {
            for (const auto& test : cases) {
                const auto before = Fill(block, layout);
                const auto color = DecodeTarget(block);
                Draw(*device, color, test.blend, compressed ? Float16Export : Float32Export, Program(first, second, compressed));
                Check(*device, block, layout, before, [&](const Color& destination) {
                    Color result{};
                    for (std::uint32_t channel = 0; channel < 3; ++channel) result[channel] = test.color(first[channel], second[channel], destination[channel], second[3]);
                    result[3] = test.alpha(first[3], second[3], destination[3]);
                    return result;
                }, std::string(test.name) + (compressed ? " with FP16_ABGR exports" : " with 32_ABGR exports"));
            }
        }
        std::puts("dual-source blend tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
