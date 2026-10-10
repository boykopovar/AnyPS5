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
#include <limits>
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
constexpr std::uint32_t Unorm10_11_11Info = 0x8018u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Float16Export = 4;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 32> Programs{};
std::size_t programCount = 0;

std::uint32_t Unorm(float value, std::uint32_t bits) {
    const auto maximum = (1u << bits) - 1u;
    if (!(value > 0.0f)) return 0;
    if (value >= 1.0f) return maximum;
    return static_cast<std::uint32_t>(std::nearbyint(static_cast<double>(value) * maximum));
}

std::uint32_t Packed(std::array<float, 3> rgb) {
    return Unorm(rgb[0], 11) | (Unorm(rgb[1], 11) << 11u) | (Unorm(rgb[2], 10) << 22u);
}

std::uint32_t Half(float value) {
    Require(value == 0.0f || (value >= 0.0625f && value <= 1.0f), "the test's half values are normal and at most 1");
    if (value == 0.0f) return 0;
    const auto bits = std::bit_cast<std::uint32_t>(value);
    Require((bits & 0x1fffu) == 0, "the test's half values are exact");
    return ((((bits >> 23u) & 0xffu) - 112u) << 10u) | ((bits >> 13u) & 0x3ffu);
}

std::span<const std::uint32_t> Program(const std::array<std::uint32_t, 4>& vgprs, bool compressed) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t vgpr = 0; vgpr < (compressed ? 2u : 4u); ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = vgprs[vgpr];
    }
    code[at++] = compressed ? 0xf8001c0fu : 0xf800180fu;
    code[at++] = compressed ? 0x00000504u : 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

std::span<const std::uint32_t> Float32Program(std::array<float, 3> rgb) {
    return Program({std::bit_cast<std::uint32_t>(rgb[0]), std::bit_cast<std::uint32_t>(rgb[1]), std::bit_cast<std::uint32_t>(rgb[2]), std::bit_cast<std::uint32_t>(1.0f)}, false);
}

std::span<const std::uint32_t> Float16Program(std::array<float, 3> rgb) {
    return Program({Half(rgb[0]) | (Half(rgb[1]) << 16u), Half(rgb[2]) | (Half(1.0f) << 16u), 0, 0}, true);
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t attrib3) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Unorm10_11_11Info;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = attrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R32_UINT && color.elementBytes == 4 && color.packing == ShaderRecompiler::ColorExportPacking::Unorm10_11_11 && color.bytes == block.bytes, "the 10_11_11 unorm target did not decode as a packed R32_UINT surface");
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
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
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
        {0, 0, vertexPush, 128 - vertexPush}
    };
    const auto handle = ShaderRecompiler::PrepareShader(fragment);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{codeAddress, 0, 1, {pixelCode.begin(), pixelCode.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    fragment.shader.code = snapshot.code;
    fragment.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    fragment.context.pixel->targetExportPacking = AgcDriver::Graphics::ExportPackings(state);
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto materialize = [&](const ShaderRecompiler::RecompileRequest& request) {
        const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
        return invocation.Materialize(*invocation.Capture(runtime));
    };
    const auto pixelResult = materialize(fragment);
    auto unpacked = fragment;
    unpacked.context.pixel->targetExportPacking.fill(ShaderRecompiler::ColorExportPacking::None);
    const auto plain = materialize(unpacked);
    Require(pixelResult->variantId == ShaderRecompiler::GetPreparedArtifact(*handle).variantId && plain->variantId == pixelResult->variantId, "the draw did not use the registered pixel shader");
    Require(plain->PipelineVariantId() != pixelResult->PipelineVariantId() && materialize(fragment)->PipelineVariantId() == pixelResult->PipelineVariantId(), "the 10_11_11 unorm packing did not key its own specialization");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), vertexPush}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint32_t> ReadBack(AgcDriver::VulkanDevice& device, const Block& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint32_t> stored(block.bytes / 4u);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

void ExpectSurface(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const std::vector<std::uint32_t>& before, std::uint32_t word, const std::string& what) {
    const auto stored = ReadBack(device, block);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y) / 4u;
            const bool inside = x < ScissorWidth;
            const auto expected = inside ? word : before[index];
            if (stored[index] == expected) continue;
            char detail[160];
            std::snprintf(detail, sizeof(detail), ": pixel (%u, %u) %s the scissor holds 0x%08x, expected 0x%08x", x, y, inside ? "inside" : "outside", stored[index], expected);
            throw std::runtime_error(what + detail);
        }
    }
}

std::vector<std::uint32_t> Fill(Block& block) {
    std::vector<std::uint32_t> words(block.bytes / 4u);
    for (std::size_t index = 0; index < words.size(); ++index) words[index] = static_cast<std::uint32_t>(index * 0x9e3779b9u);
    std::memcpy(block.data, words.data(), block.bytes);
    return words;
}

std::string Describe(std::array<float, 3> rgb) {
    char text[96];
    std::snprintf(text, sizeof(text), "(%.9g, %.9g, %.9g)", rgb[0], rgb[1], rgb[2]);
    return text;
}

void TargetTests(AgcDriver::VulkanDevice& device, Block& block, std::uint32_t attrib3, ColorTileMode mode, const char* kind) {
    const ColorTargetLayout layout(Width, Height, mode, 4);
    Require(layout.Bytes() == block.bytes, "the test surface size disagrees with its layout");
    const auto color = DecodeTarget(block, attrib3);
    auto before = Fill(block);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<std::array<float, 3>, 8> cases{{
        {0.0f, 1.0f, 0.5f},
        {0.5f, 0.25f, 1.0f / 3.0f},
        {nan, -1.0f, 2.0f},
        {infinity, -0.0f, std::numeric_limits<float>::denorm_min()},
        {0.5f / 2047.0f, 1.5f / 2047.0f, 0.5f / 1023.0f},
        {2046.5f / 2047.0f, 1023.5f / 2047.0f, 1022.5f / 1023.0f},
        {-infinity, std::numeric_limits<float>::min(), 0.999999940f},
        {0.1f, 0.7f, 0.9f},
    }};
    for (const auto& rgb : cases) {
        Draw(device, color, Float32Export, Float32Program(rgb));
        ExpectSurface(device, block, layout, before, Packed(rgb), std::string("a 32_ABGR export of ") + Describe(rgb) + " into a " + kind + " 10_11_11 unorm target");
        before = ReadBack(device, block);
    }
    const std::array<float, 3> halves{0.5f, 0.75f, 0.125f};
    Draw(device, color, Float16Export, Float16Program(halves));
    ExpectSurface(device, block, layout, before, Packed(halves), std::string("an FP16_ABGR export of ") + Describe(halves) + " into a " + kind + " 10_11_11 unorm target");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Block tiled(ColorTargetLayout(Width, Height, ColorTileMode::RenderTarget, 4).Bytes());
        Block linear(ColorTargetLayout(Width, Height, ColorTileMode::Linear, 4).Bytes());
        TargetTests(*device, tiled, TiledAttrib3, ColorTileMode::RenderTarget, "tiled");
        TargetTests(*device, linear, LinearAttrib3, ColorTileMode::Linear, "linear");
        std::puts("10_11_11 unorm color target tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
