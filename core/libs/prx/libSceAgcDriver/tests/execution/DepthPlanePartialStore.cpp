#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::DepthPlaneStoreStatistics;
using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::DrawOptions;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;

constexpr std::uint32_t PlaneWidth = 384;
constexpr std::uint32_t PlaneHeight = 200;
constexpr VkExtent2D PlaneExtent{PlaneWidth, PlaneHeight};
constexpr std::size_t Block = 65536;
constexpr std::size_t Units = 6;
constexpr std::size_t PlaneBytes = Units * Block;
constexpr std::size_t UnitTexels = Block / sizeof(float);
constexpr std::size_t EdgeTexels = 128u * (PlaneHeight - 128u);
constexpr std::size_t PlaneTexels = static_cast<std::size_t>(PlaneWidth) * PlaneHeight;
constexpr VkExtent2D SmallExtent{64, 32};
constexpr std::size_t SmallColorBytes = static_cast<std::size_t>(SmallExtent.width) * SmallExtent.height * 4u;
constexpr std::uint64_t ReadBefore = 4096;
constexpr std::size_t ReadBytes = 7u * 1024u;
alignas(256) std::array<std::byte, PlaneTexels * 4u> Colors{};

void* AllocateBlocks(std::size_t bytes, bool watched) {
#ifdef _WIN32
    void* block = nullptr;
    if (watched) {
        block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
    } else {
        block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    Require(block != nullptr, "cannot allocate the depth plane blocks");
#else
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED, "cannot map the depth plane blocks");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + Block - 1) & ~static_cast<std::uintptr_t>(Block - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    return block;
}

void Fill(float* plane, float value) {
    std::fill(plane, plane + PlaneBytes / sizeof(float), value);
    AgcDriver::GuestMemory::BumpCollectEpoch();
}

void FillUnit(float* plane, std::size_t unit, float value) {
    std::fill(plane + unit * UnitTexels, plane + (unit + 1u) * UnitTexels, value);
    AgcDriver::GuestMemory::BumpCollectEpoch();
}

std::size_t Texels(std::size_t unit) {
    return unit < 3u ? UnitTexels : EdgeTexels;
}

void RequireUnit(const float* plane, std::size_t unit, float depth, std::size_t count, const std::string& what) {
    const auto* first = plane + unit * UnitTexels;
    const auto held = static_cast<std::size_t>(std::count(first, first + UnitTexels, depth));
    Require(held == count, what + ": unit " + std::to_string(unit) + " holds " + std::to_string(held) + " texels of " + std::to_string(depth) + " instead of " + std::to_string(count));
}

DepthPlaneStoreStatistics Since(const DepthPlaneStoreStatistics& before) {
    const auto now = AgcDriver::Graphics::DepthPlaneStoreCounts();
    DepthPlaneStoreStatistics difference;
    difference.stores = now.stores - before.stores;
    difference.units = now.units - before.units;
    difference.bytes = now.bytes - before.bytes;
    return difference;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> ReadStart(AgcDriver::VulkanDevice& device, std::uint64_t base) {
    auto stored = AgcDriver::Graphics::FlushDepthPlanes(base - ReadBefore, ReadBytes);
    device.WaitIdle();
    return stored;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(AgcDriver::Graphics::DepthSliceBytes(PlaneExtent, 4u) == PlaneBytes, "the 384x200 depth plane is not six 64 KiB tiles");
        auto* blocks = static_cast<std::byte*>(AllocateBlocks(Block + PlaneBytes, true));
        auto* plane = reinterpret_cast<float*>(blocks + Block);
        const auto base = reinterpret_cast<std::uintptr_t>(plane);
        const auto shaders = Compile(*device);
        const auto swizzle = AgcDriver::Graphics::XorSwizzleMode(AgcDriver::Graphics::TextureTileMode::kZ64KBX);
        const Surface depth{base, 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, swizzle};
        const auto draw = [&](const Surface& surface, const DrawOptions& options) { return Draw(*device, shaders, surface, options, PlaneExtent, Colors); };
        const DrawOptions writeHalf{.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS};

        Fill(plane, 1.0f);
        Require(draw(depth, writeHalf) == PlaneTexels, "the draw writing depth 0.5 did not cover the plane");
        Require(AgcDriver::Graphics::DepthPlanesStale(), "the draw writing depth left the plane unregistered");
        auto before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        const auto first = ReadStart(*device, base);
        auto stored = Since(before);
        Require(first.size() == 1 && first[0].first == base && first[0].second == Block, "a read ending 3 KiB into the stale plane did not store exactly its first 64 KiB unit");
        Require(stored.stores == 1 && stored.units == 1 && stored.bytes == Block, "a read ending 3 KiB into the stale plane did not count one store of one unit");
        RequireUnit(plane, 0, 0.5f, UnitTexels, "after a read of the first unit");
        for (std::size_t unit = 1; unit < Units; ++unit) RequireUnit(plane, unit, 1.0f, UnitTexels, "after a read of the first unit, a unit it does not overlap");
        Require(AgcDriver::Graphics::DepthPlanesStale(), "a store of one unit left no stale plane");

        before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        Require(ReadStart(*device, base).empty(), "the same read again stored the first unit again");
        stored = Since(before);
        Require(stored.stores == 0 && stored.units == 0 && stored.bytes == 0, "the same read again counted a store");

        before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        const auto rest = AgcDriver::Graphics::FlushDepthPlanes(base, PlaneBytes);
        device->WaitIdle();
        stored = Since(before);
        Require(rest.size() == 1 && rest[0].first == base + Block && rest[0].second == PlaneBytes - Block, "a read of the whole plane did not store the five units still stale");
        Require(stored.stores == 1 && stored.units == Units - 1u && stored.bytes == PlaneBytes - Block, "a read of the whole plane did not count one store of five units");
        for (std::size_t unit = 0; unit < Units; ++unit) RequireUnit(plane, unit, 0.5f, Texels(unit), "after a read of the whole plane");
        for (std::size_t unit = 3; unit < Units; ++unit) RequireUnit(plane, unit, 1.0f, UnitTexels - EdgeTexels, "after a read of the whole plane, the padding below the last row");
        Require(!AgcDriver::Graphics::DepthPlanesStale(), "a read of the whole plane left it stale");

        if (!AgcDriver::GuestMemory::WriteWatched() || !AgcDriver::GuestMemory::Watched(base, PlaneBytes)) {
            std::puts("guest memory is not write-watched here: guest writes into a partly stored plane are not exercised");
        } else {
            Fill(plane, 1.0f);
            Require(draw(depth, writeHalf) == PlaneTexels, "the draw writing depth 0.5 over the reloaded plane did not cover it");
            Require(!ReadStart(*device, base).empty(), "a read of the first unit of the drawn plane stored nothing");
            FillUnit(plane, 4, 0.75f);
            draw(depth, {.depthTest = false});
            before = AgcDriver::Graphics::DepthPlaneStoreCounts();
            Require(!AgcDriver::Graphics::FlushDepthPlanes(base, PlaneBytes).empty(), "a read of the whole plane after a guest write into unit 4 stored nothing");
            device->WaitIdle();
            stored = Since(before);
            Require(stored.units == 4 && stored.bytes == 4u * Block, "a read of the whole plane after a guest write into unit 4 did not store the four units still stale");
            for (const std::size_t unit : {0u, 1u, 2u, 3u, 5u}) RequireUnit(plane, unit, 0.5f, Texels(unit), "a guest write into unit 4 after unit 0 was stored, then a read of the whole plane");
            RequireUnit(plane, 4, 0.75f, UnitTexels, "a guest write into unit 4 after unit 0 was stored, then a read of the whole plane, the written unit");
            Require(draw(depth, {.minDepth = 0.6f, .maxDepth = 0.6f}) == EdgeTexels, "a draw at depth 0.6 did not pass LESS exactly over the unit the guest wrote 0.75 into");

            Fill(plane, 1.0f);
            Require(draw(depth, writeHalf) == PlaneTexels, "the draw writing depth 0.5 before a guest write and an unmatched view did not cover the plane");
            Require(!ReadStart(*device, base).empty(), "a read of the first unit before a guest write and an unmatched view stored nothing");
            FillUnit(plane, 2, 0.75f);
            const std::array<std::uint32_t, 8> words{0, 0, 0, 0, PlaneWidth, PlaneHeight, 1, 0};
            AgcDriver::Graphics::GuestTextureResource levels{};
            levels.baseAddress = base;
            levels.width = PlaneWidth;
            levels.height = PlaneHeight;
            levels.mipCount = 2;
            levels.lastLevel = 1;
            levels.dimension = AgcDriver::Graphics::TextureDimension::k2D;
            levels.tileMode = AgcDriver::Graphics::TextureTileMode::kZ64KBX;
            levels.format = 22;
            Require(AgcDriver::Graphics::DepthSurfaceTexture(AgcDriver::VulkanDeviceTestAccess::ContextOf(*device), words, levels, {}) == nullptr, "a view with two levels of a plane the guest wrote was served by the depth surface");
            device->WaitIdle();
            Require(!AgcDriver::Graphics::DepthSurfaceAt(base), "a view with two levels of a plane the guest wrote left the depth surface live");
            for (const std::size_t unit : {0u, 1u, 3u, 4u, 5u}) RequireUnit(plane, unit, 0.5f, Texels(unit), "the retire of a partly stored plane the guest wrote into");
            RequireUnit(plane, 2, 0.75f, UnitTexels, "the retire of a partly stored plane the guest wrote into, the written unit");
        }

        Fill(plane, 1.0f);
        Require(draw(depth, writeHalf) == PlaneTexels, "the draw writing depth 0.5 under a later color target did not cover the plane");
        Require(!ReadStart(*device, base).empty(), "a read of the first unit before a color target stored nothing");
        auto* smallPlane = AllocateBlocks(Block, true);
        const Surface smallDepth{reinterpret_cast<std::uintptr_t>(smallPlane), 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, swizzle};
        const std::span<std::byte> overLastUnit(reinterpret_cast<std::byte*>(plane + 5u * UnitTexels), SmallColorBytes);
        Draw(*device, shaders, smallDepth, {.depthTest = false}, SmallExtent, overLastUnit);
        Require(!AgcDriver::Graphics::DepthSurfaceAt(base), "a color target over the last unit left the depth surface live");
        for (std::size_t unit = 0; unit < 5u; ++unit) RequireUnit(plane, unit, 0.5f, Texels(unit), "a color target over the last unit of a partly stored plane");

        auto* looseBlocks = static_cast<std::byte*>(AllocateBlocks(Block + PlaneBytes, false));
        auto* loose = reinterpret_cast<float*>(looseBlocks + Block);
        const auto looseBase = reinterpret_cast<std::uintptr_t>(loose);
        if (AgcDriver::GuestMemory::Watched(looseBase, PlaneBytes)) {
            std::puts("memory outside the guest arena is write-watched here: a partial store over unwatched memory is not exercised");
        } else {
            const Surface unwatched{looseBase, 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, swizzle};
            Fill(loose, 1.0f);
            Require(draw(unwatched, writeHalf) == PlaneTexels, "the draw writing depth 0.5 into an unwatched plane did not cover it");
            before = AgcDriver::Graphics::DepthPlaneStoreCounts();
            Require(ReadStart(*device, looseBase).size() == 1, "a read of the first unit of an unwatched plane did not store it");
            stored = Since(before);
            Require(stored.units == 1 && stored.bytes == Block, "a read of the first unit of an unwatched plane did not count one unit");
            RequireUnit(loose, 0, 0.5f, UnitTexels, "unwatched plane after a read of the first unit");
            for (std::size_t unit = 1; unit < Units; ++unit) RequireUnit(loose, unit, 1.0f, UnitTexels, "unwatched plane after a read of the first unit, a unit it does not overlap");
            Require(draw(unwatched, {.minDepth = 0.6f, .maxDepth = 0.6f}) == 0u, "a draw at depth 0.6 passed LESS over unwatched units still holding depth 0.5 on the GPU");
        }

        auto* arrayBlocks = static_cast<std::byte*>(AllocateBlocks(3u * PlaneBytes, true));
        auto* layerPlane = reinterpret_cast<float*>(arrayBlocks + PlaneBytes);
        AgcDriver::Graphics::GuestTextureResource array{};
        array.baseAddress = reinterpret_cast<std::uintptr_t>(arrayBlocks);
        array.width = PlaneWidth;
        array.height = PlaneHeight;
        array.depthOrLastArray = 2;
        array.mipCount = 1;
        array.tileMode = AgcDriver::Graphics::TextureTileMode::kR64KBX;
        array.dimension = AgcDriver::Graphics::TextureDimension::k2DArray;
        array.format = 22;
        array.dstSelX = 4;
        array.dstSelY = 5;
        array.dstSelZ = 6;
        array.dstSelW = 7;
        Require(AgcDriver::Graphics::DescribeSurface(array).layerBytes == PlaneBytes, "a layer of the 384x200 R32 array is not six 64 KiB tiles");
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const auto lookup = [&](std::uint32_t first, std::uint32_t count) {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            return AgcDriver::Graphics::CachedStorageLayers(context, array, first, count);
        };
        Fill(layerPlane, 1.0f);
        const auto arrayImage = lookup(0, 1);
        device->WaitIdle();
        const auto provedBefore = AgcDriver::Graphics::StorageTexture::RefreshesProved();
        Require(lookup(0, 1) == arrayImage && lookup(0, 1) == arrayImage, "a lookup of the first layer of an unchanged array replaced its cached image");
        if (!AgcDriver::GuestMemory::WriteWatched() || !AgcDriver::GuestMemory::Watched(array.baseAddress, 3u * PlaneBytes)) {
            std::puts("guest memory is not write-watched here: lookups of array layers through a refresh proof are not exercised");
        } else {
            Require(AgcDriver::Graphics::StorageTexture::RefreshesProved() != provedBefore, "repeated lookups of the first layer of an unchanged array proved no refresh");
        }
        const Surface layerDepth{reinterpret_cast<std::uintptr_t>(layerPlane), 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, swizzle};
        Require(draw(layerDepth, writeHalf) == PlaneTexels, "the draw writing depth 0.5 over the second layer of an array did not cover the plane");
        before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        Require(lookup(0, 1) == arrayImage, "a depth draw over the second layer of an array replaced the array's cached image");
        device->WaitIdle();
        stored = Since(before);
        Require(stored.stores == 0, "a lookup of the first layer of an array stored the depth plane over its second layer");
        for (std::size_t unit = 0; unit < Units; ++unit) RequireUnit(layerPlane, unit, 1.0f, UnitTexels, "a lookup of the first layer of an array, the depth plane over its second layer");
        Require(AgcDriver::Graphics::DepthPlanesStale(), "a lookup of the first layer of an array left no stale plane");

        before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        static_cast<void>(lookup(0, 2));
        device->WaitIdle();
        stored = Since(before);
        Require(stored.stores == 1 && stored.units == Units, "a lookup of the first two layers of a three-layer array did not store the depth plane over its second layer");
        for (std::size_t unit = 0; unit < Units; ++unit) RequireUnit(layerPlane, unit, 0.5f, Texels(unit), "a lookup of the first two layers of a three-layer array, the depth plane over its second layer");

        Require(draw(layerDepth, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .minDepth = 0.25f, .maxDepth = 0.25f}) == PlaneTexels, "the draw writing depth 0.25 over the second layer of an array did not cover the plane");
        before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        static_cast<void>(lookup(0, 3));
        device->WaitIdle();
        stored = Since(before);
        Require(stored.stores == 1 && stored.units == Units, "a lookup of every layer of an array did not store the depth plane over its second layer");
        for (std::size_t unit = 0; unit < Units; ++unit) RequireUnit(layerPlane, unit, 0.25f, Texels(unit), "a lookup of every layer of an array, the depth plane over its second layer");
        std::puts("depth plane partial store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
