#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t RegionBytes = 8 * BlockBytes;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;
constexpr std::uint8_t Initial = 0x3f;
constexpr std::uint8_t Sentinel = 0x37;
constexpr std::uint8_t Gap = 0x29;
constexpr std::size_t GapBytes = 0x100;

struct Surface {
    std::size_t offset;
    std::uint32_t width;
    std::uint32_t height;
    std::size_t RowBytes() const { return std::size_t{width} * 16u; }
    std::size_t Bytes() const { return RowBytes() * height; }
};

constexpr Surface Tall{0, 64, 256};
constexpr Surface Shifted{6 * BlockBytes + GapBytes, 64, 16};
constexpr VkRect2D Seed{{63, 255}, {1, 1}};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 16>, 2> Programs{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::span<const std::uint32_t> Program(std::size_t index, bool blue) {
    auto& code = Programs.at(index);
    std::size_t at = 0;
    code[at++] = 0x36000000u | (20u << 17u) | 0xffu;
    code[at++] = 0x0000ffffu;
    code[at++] = 0x2c000000u | (21u << 17u) | 0x90u;
    code[at++] = 0x7e000c00u | (20u << 17u) | (256u + 20u);
    code[at++] = 0x7e000c00u | (21u << 17u) | (256u + 21u);
    code[at++] = (blue ? 0x7e0002f2u : 0x7e000280u) | (22u << 17u);
    code[at++] = 0x7e0002f2u | (23u << 17u);
    code[at++] = 0xf800180fu;
    code[at++] = 0x17161514u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes), arena(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (arena) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (arena) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map the color targets");
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
        Require(data != nullptr, "cannot allocate the color targets");
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
        if (arena) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (arena) {
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
    bool arena;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, const Surface& surface) {
    const auto address = block.Address() + surface.offset;
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Rgba32FloatInfo;
    cx[0x31d] = 0;
    cx[0x323] = 0;
    cx[0x324] = 0;
    cx[0x3b0] = ((surface.width - 1u) << 14u) | (surface.height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    color.exportIndex = 0;
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const Surface& surface, std::span<const std::uint32_t> pixelCode, VkRect2D scissor) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
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

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionFixedPoint);
    pixel.targetOutputMode.at(0) = Float32AbgrExport;
    pixel.targetExportMapping.fill(0xe4u);
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

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_FALSE;
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {surface.width, surface.height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(surface.height), static_cast<float>(surface.width), -static_cast<float>(surface.height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = scissor;
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

bool Inside(const VkRect2D& rect, std::uint32_t x, std::uint32_t y) {
    return x >= static_cast<std::uint32_t>(rect.offset.x) && x < static_cast<std::uint32_t>(rect.offset.x) + rect.extent.width && y >= static_cast<std::uint32_t>(rect.offset.y) && y < static_cast<std::uint32_t>(rect.offset.y) + rect.extent.height;
}

std::uint8_t* Pixel(const Block& block, const Surface& surface, std::uint32_t x, std::uint32_t y) {
    return block.data + surface.offset + static_cast<std::size_t>(y) * surface.RowBytes() + static_cast<std::size_t>(x) * 16u;
}

bool Holds(const std::uint8_t* pixel, std::uint8_t value) {
    return std::all_of(pixel, pixel + 16, [&](std::uint8_t byte) { return byte == value; });
}

bool HoldsOutput(const std::uint8_t* pixel, std::uint32_t x, std::uint32_t y, float z) {
    std::array<float, 4> texel{};
    std::memcpy(texel.data(), pixel, sizeof(texel));
    return texel == std::array<float, 4>{static_cast<float>(x), static_cast<float>(y), z, 1.0f};
}

bool Expect(AgcDriver::VulkanDevice& device) {
    Block block(RegionBytes);
    std::fill(block.data, block.data + block.bytes, Initial);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), block.bytes) == nullptr) return false;
    const auto tall = DecodeTarget(block, Tall);
    const auto shifted = DecodeTarget(block, Shifted);
    Require(tall.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && tall.bytes == Tall.Bytes() && shifted.bytes == Shifted.Bytes(), "the targets are not linear R32G32B32A32 targets");
    constexpr VkRect2D first{{8, 4}, {16, 16}};
    constexpr VkRect2D second{{16, 12}, {24, 16}};
    constexpr VkRect2D whole{{0, 0}, {Shifted.width, Shifted.height}};
    Draw(device, tall, Tall, Program(0, false), Seed);
    device.WaitIdle();
    Draw(device, tall, Tall, Program(0, false), first);
    Draw(device, tall, Tall, Program(1, true), second);
    Draw(device, shifted, Shifted, Program(0, false), whole);
    const std::array<std::array<std::uint32_t, 2>, 3> sentinels{{{50, 10}, {20, 40}, {30, 200}}};
    const std::array<std::array<std::uint32_t, 2>, 2> between{{{30, 6}, {10, 25}}};
    for (const auto& [x, y] : sentinels) std::memset(Pixel(block, Tall, x, y), Sentinel, 16);
    for (const auto& [x, y] : between) std::memset(Pixel(block, Tall, x, y), Sentinel, 16);
    Require((block.Address() + Shifted.offset) / BlockBytes * BlockBytes == block.Address() + Shifted.offset - GapBytes, "the shifted target does not start past a 64 KiB boundary");
    std::memset(block.data + Shifted.offset - GapBytes, Gap, GapBytes);
    device.WaitIdle();
    std::size_t wrongDrawn = 0, wrongSentinels = 0, wrongBetween = 0, wrongUntouched = 0, wrongShifted = 0;
    for (std::uint32_t y = 0; y < Tall.height; ++y) {
        for (std::uint32_t x = 0; x < Tall.width; ++x) {
            const auto* pixel = Pixel(block, Tall, x, y);
            const auto at = [&](const auto& point) { return point[0] == x && point[1] == y; };
            if (std::any_of(sentinels.begin(), sentinels.end(), at)) wrongSentinels += Holds(pixel, Sentinel) ? 0u : 1u;
            else if (std::any_of(between.begin(), between.end(), at)) wrongBetween += Holds(pixel, Sentinel) ? 0u : 1u;
            else if (Inside(second, x, y)) wrongDrawn += HoldsOutput(pixel, x, y, 1.0f) ? 0u : 1u;
            else if (Inside(first, x, y) || Inside(Seed, x, y)) wrongDrawn += HoldsOutput(pixel, x, y, 0.0f) ? 0u : 1u;
            else wrongUntouched += Holds(pixel, Initial) ? 0u : 1u;
        }
    }
    for (std::uint32_t y = 0; y < Shifted.height; ++y) {
        for (std::uint32_t x = 0; x < Shifted.width; ++x) wrongShifted += HoldsOutput(Pixel(block, Shifted, x, y), x, y, 0.0f) ? 0u : 1u;
    }
    const auto gapLost = static_cast<std::size_t>(std::count_if(block.data + Shifted.offset - GapBytes, block.data + Shifted.offset, [](std::uint8_t byte) { return byte != Gap; }));
    std::printf("linear target rectangle bound: wrong drawn pixels %zu, lost sentinels %zu/%zu, lost sentinels between the rectangles %zu/%zu, changed untouched pixels %zu, wrong shifted-target pixels %zu, lost gap bytes %zu/%zu\n", wrongDrawn, wrongSentinels, sentinels.size(), wrongBetween, between.size(), wrongUntouched, wrongShifted, gapLost, GapBytes);
    Require(wrongDrawn == 0 && wrongShifted == 0, "a drawn linear target's store lost pixels its draws wrote");
    Require(wrongSentinels == 0 && gapLost == 0, "a drawn linear target's store overwrote CPU stores outside the drawn rectangles");
    Require(wrongBetween == 0, "a drawn linear target's store overwrote CPU stores between two draws' scissor rectangles");
    Require(wrongUntouched == 0, "a drawn linear target's store changed pixels no draw and no CPU store wrote");
    return true;
}

bool ExpectManyRows(AgcDriver::VulkanDevice& device) {
    Block block(RegionBytes);
    std::fill(block.data, block.data + block.bytes, Initial);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), block.bytes) == nullptr) return false;
    const auto tall = DecodeTarget(block, Tall);
    constexpr std::uint32_t Rows = 17;
    constexpr std::uint32_t Spacing = 15;
    Draw(device, tall, Tall, Program(0, false), Seed);
    device.WaitIdle();
    for (std::uint32_t row = 0; row < Rows; ++row) Draw(device, tall, Tall, Program(0, false), VkRect2D{{8, static_cast<std::int32_t>(row * Spacing)}, {16, 1}});
    constexpr std::array<std::uint32_t, 2> between{10, 70};
    std::memset(Pixel(block, Tall, between[0], between[1]), Sentinel, 16);
    device.WaitIdle();
    std::size_t wrongDrawn = 0, wrongUntouched = 0;
    for (std::uint32_t y = 0; y < Tall.height; ++y) {
        const bool drawnRow = y % Spacing == 0 && y / Spacing < Rows;
        for (std::uint32_t x = 0; x < Tall.width; ++x) {
            if (x == between[0] && y == between[1]) continue;
            const auto* pixel = Pixel(block, Tall, x, y);
            if ((drawnRow && x >= 8 && x < 24) || Inside(Seed, x, y)) wrongDrawn += HoldsOutput(pixel, x, y, 0.0f) ? 0u : 1u;
            else wrongUntouched += Holds(pixel, Initial) ? 0u : 1u;
        }
    }
    const bool kept = Holds(Pixel(block, Tall, between[0], between[1]), Sentinel);
    std::printf("linear target rectangle bound, %u one-row scissors: wrong drawn pixels %zu, sentinel between the rows %s, changed untouched pixels %zu\n", Rows, wrongDrawn, kept ? "kept" : "lost", wrongUntouched);
    Require(wrongDrawn == 0, "a drawn linear target's store lost pixels its one-row draws wrote");
    Require(kept, "a drawn linear target's store overwrote a CPU store between two one-row scissors");
    Require(wrongUntouched == 0, "a drawn linear target's store changed pixels no one-row draw and no CPU store wrote");
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!Expect(*device) || !ExpectManyRows(*device)) {
            std::puts("linear target rectangle bound test skipped: no host import on this device");
            return VulkanTestSkipped;
        }
        std::puts("linear target rectangle bound tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
