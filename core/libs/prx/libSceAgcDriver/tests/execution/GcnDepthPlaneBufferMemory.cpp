#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>

namespace {

using AgcDriver::Graphics::TextureTileMode;
using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;

constexpr std::size_t Block = 1u << 20u;
constexpr std::uint32_t Format32Float = 22;
constexpr std::array<TextureTileMode, 2> Modes{TextureTileMode::kGcnDepth2D64, TextureTileMode::kGcnDepth2D256};

alignas(256) std::array<std::byte, 512u * 512u * 4u> Pixels{};

void* AllocateWatched(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    constexpr std::size_t alignment = 65536;
    void* raw = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED, "cannot map the depth plane block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + alignment - 1) & ~static_cast<std::uintptr_t>(alignment - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    return block;
}

struct Layout {
    VkExtent2D extent;
    AgcDriver::Graphics::GcnTileGeometry geometry;
    AgcDriver::Graphics::TileMipLayout mip;
};

Layout Describe(TextureTileMode mode) {
    const auto geometry = AgcDriver::Graphics::GcnTiling(mode, 4u);
    const VkExtent2D extent{geometry.blockWidth * 2u, geometry.blockHeight * 2u};
    const auto mip = AgcDriver::Graphics::ComputeGcnMipLayout(mode, Format32Float, extent.width, extent.height, extent.width, false, 1u).front();
    return {extent, geometry, mip};
}

std::uint32_t Parity(std::uint32_t mask, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint32_t>(std::popcount((x & mask & 0xfffu) ^ ((y << 12u) & mask & 0xfff000u))) & 1u;
}

std::uint64_t TexelOffset(const Layout& layout, std::uint32_t x, std::uint32_t y) {
    std::uint32_t inBlock = 0, split = 0;
    for (std::uint32_t bit = 0; bit < 16u; ++bit) inBlock |= Parity(layout.geometry.equation[bit], x, y) << bit;
    for (std::uint32_t bit = 0; bit < 4u; ++bit) split |= Parity(layout.geometry.splitMasks[bit], x, y) << bit;
    const auto block = static_cast<std::uint64_t>(y / layout.geometry.blockHeight) * layout.mip.blocksPerRow + x / layout.geometry.blockWidth;
    return block * layout.geometry.blockBytes + inBlock + static_cast<std::uint64_t>(split) * layout.mip.gcnSplitStride;
}

VkRect2D Inner(VkExtent2D extent) {
    return {{static_cast<std::int32_t>(extent.width / 2u + 3u), 5}, {6, 7}};
}

float Expected(VkExtent2D extent, std::uint32_t x, std::uint32_t y) {
    const auto inner = Inner(extent);
    if (x >= static_cast<std::uint32_t>(inner.offset.x) && x < inner.offset.x + inner.extent.width && y >= static_cast<std::uint32_t>(inner.offset.y) && y < inner.offset.y + inner.extent.height) return 0.5f;
    return x < extent.width / 2u ? 0.25f : 0.75f;
}

float TexelAt(const std::byte* plane, const Layout& layout, std::uint32_t x, std::uint32_t y) {
    float value = 0.0f;
    std::memcpy(&value, plane + TexelOffset(layout, x, y), sizeof(value));
    return value;
}

void Case(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, TextureTileMode mode) {
    const auto layout = Describe(mode);
    const auto extent = layout.extent;
    const std::string what = "PS4 depth tile mode " + std::to_string(static_cast<int>(mode)) + " (" + std::to_string(extent.width) + "x" + std::to_string(extent.height) + ")";
    Require(layout.mip.tiledSize <= Block && static_cast<std::size_t>(extent.width) * extent.height * 4u <= Pixels.size(), what + ": the surface does not fit the test buffers");
    auto* plane = static_cast<std::byte*>(AllocateWatched(Block));
    const auto address = reinterpret_cast<std::uintptr_t>(plane);
    const auto reader = Compile(device, plane);
    const std::span<std::byte> pixels(Pixels.data(), static_cast<std::size_t>(extent.width) * extent.height * 4u);
    const auto covered = static_cast<std::uint32_t>(extent.width * extent.height);
    Surface depth{address, 0, 0, false, VK_FORMAT_D32_SFLOAT};
    depth.gcnTileMode = mode;

    Require(Draw(device, shaders, depth, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .minDepth = 0.75f, .maxDepth = 0.75f}, extent, pixels) == covered, what + ": the draw writing depth 0.75 did not cover the target");
    Draw(device, shaders, depth, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .minDepth = 0.25f, .maxDepth = 0.25f, .scissor = VkRect2D{{0, 0}, {extent.width / 2u, extent.height}}}, extent, pixels);
    Draw(device, shaders, depth, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .minDepth = 0.5f, .maxDepth = 0.5f, .scissor = Inner(extent)}, extent, pixels);
    Draw(device, reader, depth, {.depthTest = false}, extent, pixels);
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        for (std::uint32_t x = 0; x < extent.width; ++x) {
            const auto got = TexelAt(plane, layout, x, y);
            if (got != Expected(extent, x, y)) Require(false, what + ": after a buffer read of the depth plane, texel " + std::to_string(x) + "," + std::to_string(y) + " holds " + std::to_string(got) + " at its tiled offset, expected " + std::to_string(Expected(extent, x, y)));
        }
    }

    if (!AgcDriver::GuestMemory::WriteWatched() || !AgcDriver::GuestMemory::Watched(address, Block)) return;
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        for (std::uint32_t x = 0; x < extent.width; ++x) {
            const float value = x < extent.width / 2u ? 0.25f : 0.75f;
            std::memcpy(plane + TexelOffset(layout, x, y), &value, sizeof(value));
        }
    }
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Draw(device, shaders, depth, {}, extent, pixels);
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        for (std::uint32_t x = 0; x < extent.width; ++x) {
            const bool passed = std::to_integer<std::uint8_t>(pixels[(static_cast<std::size_t>(y) * extent.width + x) * 4u]) == 255u;
            if (passed != (x >= extent.width / 2u)) Require(false, what + ": after a guest write into the tiled depth plane, a draw at depth 0.5 under LESS " + (passed ? "passed" : "failed") + " at " + std::to_string(x) + "," + std::to_string(y) + " against " + (x < extent.width / 2u ? "0.25" : "0.75"));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = Compile(*device);
        for (const auto mode : Modes) Case(*device, shaders, mode);
        if (!AgcDriver::GuestMemory::WriteWatched()) std::puts("guest memory is not write-watched here: a guest write into a PS4 depth plane is not exercised");
        std::puts("GCN depth plane buffer memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
