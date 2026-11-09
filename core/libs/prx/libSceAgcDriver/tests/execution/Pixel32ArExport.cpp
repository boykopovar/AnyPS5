#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
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
constexpr std::uint32_t ElementBytes = 4;
constexpr std::size_t RowBytes = std::size_t{Width} * ElementBytes;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Float32Info = 0x0710u;
constexpr std::uint32_t Float32AlternateReversedInfo = 0x1f10u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Export32Ar = 3;
constexpr float Destination = 0.75f;

struct Row {
    std::uint32_t enables;
    std::array<float, 4> sources;
};

constexpr std::array<Row, 4> Rows{{
    {0x3u, {0.5f, 0.25f, 2.0f, 0.0625f}},
    {0xbu, {0.5f, 0.25f, 2.0f, 0.0625f}},
    {0xfu, {0.125f, 0.5f, 2.0f, 0.0625f}},
    {0x3u, {0.25f, 0.75f, 2.0f, 0.0625f}},
}};
constexpr std::uint32_t Height = Rows.size() + 1;
constexpr std::size_t TargetBytes = RowBytes * Height;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 12>, Rows.size()> Programs{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::span<const std::uint32_t> Program(std::size_t index) {
    auto& code = Programs[index];
    const auto& row = Rows[index];
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4u; ++channel) {
        code[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(row.sources[channel]);
    }
    code[at++] = 0xf8001800u | row.enables;
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t info) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = info;
    cx[0x31d] = 0;
    cx[0x323] = 0;
    cx[0x324] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    return AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const VkPipelineColorBlendAttachmentState& blend, std::uint32_t waveSize, std::size_t row) {
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangle.data(), 16u, static_cast<std::uint32_t>(Triangle.size()));
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
    state.scissor = {{0, static_cast<std::int32_t>(row)}, {Width, 1}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto pixelCode = Program(row);
    const AgcDriver::Registers context{{0x1b3, 0x300u}, {0x1b4, 0x300u}, {0x1b6, waveSize == 32u ? 0x8000u : 0u}, {0x1c5, Export32Ar}, {0x203, 0u}};
    const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    Require(pixel.targetOutputMode[0] == Export32Ar && pixel.wave32 == (waveSize == 32u), "the pixel registers did not decode as a 32_AR MRT0 export");
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint32_t> ReadBack(AgcDriver::VulkanDevice& device, std::uint64_t address, std::size_t bytes) {
    AgcDriver::Graphics::StorageTexture::FlushPending(address, bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint32_t> stored(bytes / sizeof(std::uint32_t));
    AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::string Describe(const Row& row) {
    char text[96];
    std::snprintf(text, sizeof(text), "EN 0x%x of (%g, %g, %g, %g)", row.enables, row.sources[0], row.sources[1], row.sources[2], row.sources[3]);
    return text;
}

struct TargetCase {
    const char* name;
    std::uint32_t info;
    std::uint8_t mapping;
    bool blended;
};

void ExportTests(AgcDriver::VulkanDevice& device, const TargetCase& test, std::uint32_t waveSize) {
    Block block(BlockBytes);
    std::vector<std::uint32_t> filled(BlockBytes / sizeof(std::uint32_t), std::bit_cast<std::uint32_t>(Destination));
    std::memcpy(block.data, filled.data(), BlockBytes);
    const auto color = DecodeTarget(block, test.info);
    Require(color.format == VK_FORMAT_R32_SFLOAT && color.componentMapping == test.mapping && color.elementBytes == ElementBytes, std::string("the ") + test.name + " target did not decode as R32_SFLOAT with component mapping " + Hex(test.mapping));
    Require(AgcDriver::Graphics::ColorTargetLayout(Width, Height, AgcDriver::Graphics::ColorTileMode::Linear, ElementBytes).Bytes() == TargetBytes, "the linear COLOR_32 target is padded");
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    if (test.blended) {
        blend.blendEnable = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    for (std::size_t row = 0; row < Rows.size(); ++row) Draw(device, color, blend, waveSize, row);
    const auto stored = ReadBack(device, block.Address(), TargetBytes);
    for (std::size_t row = 0; row < Height; ++row) {
        float expected = Destination;
        if (row < Rows.size()) {
            const auto red = Rows[row].sources[0];
            const auto alpha = Rows[row].sources[1];
            expected = test.blended ? red * alpha + Destination * (1.0f - alpha) : alpha;
        }
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto actual = stored[row * Width + x];
            if (actual == std::bit_cast<std::uint32_t>(expected)) continue;
            const auto source = row < Rows.size() ? "the 32_AR export with " + Describe(Rows[row]) : std::string("a row outside the drawn rows");
            throw std::runtime_error("wave" + std::to_string(waveSize) + " " + source + " into the " + test.name + " target stored " + Hex(actual) + " at pixel (" + std::to_string(x) + ", " + std::to_string(row) + "), expected " + Hex(std::bit_cast<std::uint32_t>(expected)));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        constexpr std::array<TargetCase, 2> targets{{
            {"COLOR_32 FLOAT ALT_REV (alpha only)", Float32AlternateReversedInfo, 0xe7u, false},
            {"COLOR_32 FLOAT STD blended with SRC_ALPHA and ONE_MINUS_SRC_ALPHA", Float32Info, 0xe4u, true},
        }};
        for (const std::uint32_t waveSize : {32u, 64u}) {
            for (const auto& test : targets) ExportTests(*device, test, waveSize);
        }
        std::puts("32_AR export tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
