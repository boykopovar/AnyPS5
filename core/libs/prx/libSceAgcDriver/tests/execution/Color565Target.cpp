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
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t ScissorWidth = 128;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Color565StandardInfo = 0x8040u;
constexpr std::uint32_t Color565ReversedInfo = 0x9040u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32Export = 9;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 16> Programs{};
std::size_t programCount = 0;

using Color = std::array<float, 4>;

std::uint16_t Texel(const Color& color, bool reversed) {
    const auto unorm = [](float value, std::uint32_t maximum) { return static_cast<std::uint32_t>(std::lround(std::fmin(std::fmax(static_cast<double>(value), 0.0), 1.0) * maximum)); };
    const auto low = unorm(color[reversed ? 2 : 0], 31);
    const auto high = unorm(color[reversed ? 0 : 2], 31);
    return static_cast<std::uint16_t>(low | (unorm(color[1], 63) << 5u) | (high << 11u));
}

std::span<const std::uint32_t> Program(const Color& color) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        code[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(color[channel]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t attrib3, std::uint32_t info) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = info;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = attrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R5G6B5_UNORM_PACK16 && color.elementBytes == 2 && color.bytes == block.bytes, "the 5_6_5 unorm target did not decode as an R5G6B5 surface");
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t exportFormat, std::span<const std::uint32_t> pixelCode) {
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
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, exportFormat}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, {}), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto handle = ShaderRecompiler::PrepareShader(fragment);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{codeAddress, 0, 1, {pixelCode.begin(), pixelCode.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    fragment.shader.code = snapshot.code;
    fragment.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto materialize = [&](const ShaderRecompiler::RecompileRequest& request) {
        const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
        return invocation.Materialize(*invocation.Capture(runtime));
    };
    const auto pixelResult = materialize(fragment);
    Require(pixelResult->variantId == ShaderRecompiler::GetPreparedArtifact(*handle).variantId, "the draw did not use the registered pixel shader");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(vertexPush, target)}
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

std::vector<std::uint16_t> Fill(Block& block) {
    std::vector<std::uint16_t> halves(block.bytes / 2u);
    for (std::size_t index = 0; index < halves.size(); ++index) halves[index] = static_cast<std::uint16_t>((index * 0x9e3779b9u) >> 16u);
    std::memcpy(block.data, halves.data(), block.bytes);
    return halves;
}

void ExpectSurface(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const std::vector<std::uint16_t>& before, std::uint16_t texel, const std::string& what) {
    const auto stored = ReadBack(device, block);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y) / 2u;
            const bool inside = x < ScissorWidth;
            const auto expected = inside ? texel : before[index];
            if (stored[index] == expected) continue;
            char detail[160];
            std::snprintf(detail, sizeof(detail), ": pixel (%u, %u) %s the scissor holds 0x%04x, expected 0x%04x", x, y, inside ? "inside" : "outside", stored[index], expected);
            throw std::runtime_error(what + detail);
        }
    }
}

void TargetTests(AgcDriver::VulkanDevice& device, Block& block, std::uint32_t attrib3, ColorTileMode mode, bool reversed, const char* kind) {
    const ColorTargetLayout layout(Width, Height, mode, 2);
    Require(layout.Bytes() == block.bytes, "the test surface size disagrees with its layout");
    const auto color = DecodeTarget(block, attrib3, reversed ? Color565ReversedInfo : Color565StandardInfo);
    auto before = Fill(block);
    struct Case {
        Color color;
        std::uint16_t reversedTexel;
        std::uint16_t standardTexel;
    };
    const std::array<Case, 11> cases{{
        {{1.0f, 0.0f, 0.0f, 1.0f}, 0xf800, 0x001f},
        {{0.0f, 1.0f, 0.0f, 0.0f}, 0x07e0, 0x07e0},
        {{0.0f, 0.0f, 1.0f, 0.0f}, 0x001f, 0xf800},
        {{1.0f, 0.0f, 1.0f, 0.5f}, 0xf81f, 0xf81f},
        {{1.0f, 1.0f, 1.0f, 1.0f}, 0xffff, 0xffff},
        {{0.0f, 0.0f, 0.0f, 1.0f}, 0x0000, 0x0000},
        {{3.0f / 31.0f, 40.0f / 63.0f, 21.0f / 31.0f, 1.0f}, (3u << 11u) | (40u << 5u) | 21u, 3u | (40u << 5u) | (21u << 11u)},
        {{21.0f / 31.0f, 3.0f / 63.0f, 10.0f / 31.0f, 0.0f}, (21u << 11u) | (3u << 5u) | 10u, 21u | (3u << 5u) | (10u << 11u)},
        {{1.0f / 31.0f, 62.0f / 63.0f, 30.0f / 31.0f, 0.75f}, (1u << 11u) | (62u << 5u) | 30u, 1u | (62u << 5u) | (30u << 11u)},
        {{30.0f / 31.0f, 1.0f / 63.0f, 1.0f / 31.0f, 0.25f}, (30u << 11u) | (1u << 5u) | 1u, 30u | (1u << 5u) | (1u << 11u)},
        {{2.0f, -1.0f, 10.0f / 31.0f, 0.25f}, (31u << 11u) | 10u, 31u | (10u << 11u)},
    }};
    for (const auto& test : cases) {
        const auto& value = test.color;
        const auto texel = reversed ? test.reversedTexel : test.standardTexel;
        Require(Texel(value, reversed) == texel, "the test's expected texel disagrees with the 5_6_5 field layout");
        Draw(device, color, Float32Export, Program(value));
        char what[192];
        std::snprintf(what, sizeof(what), "a 32_ABGR export of (%.9g, %.9g, %.9g, %.9g) into a %s 5_6_5 unorm target with the %s component order", value[0], value[1], value[2], value[3], kind, reversed ? "reversed" : "standard");
        ExpectSurface(device, block, layout, before, texel, what);
        before = ReadBack(device, block);
        programCount = 0;
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Block tiled(ColorTargetLayout(Width, Height, ColorTileMode::RenderTarget, 2).Bytes());
        Block linear(ColorTargetLayout(Width, Height, ColorTileMode::Linear, 2).Bytes());
        for (const bool reversed : {true, false}) {
            TargetTests(*device, tiled, TiledAttrib3, ColorTileMode::RenderTarget, reversed, "tiled");
            TargetTests(*device, linear, LinearAttrib3, ColorTileMode::Linear, reversed, "linear");
        }
        std::puts("5_6_5 unorm color target tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
