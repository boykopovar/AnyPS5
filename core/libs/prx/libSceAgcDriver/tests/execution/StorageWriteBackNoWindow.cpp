#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::DescribeSurface;
using AgcDriver::Graphics::GuestTextureResource;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using AgcDriver::Graphics::TextureDetiler;
using AgcDriver::Graphics::TextureDimension;
using AgcDriver::Graphics::TextureTileMode;

// A thin 3D surface (SW_64KB_R_X, the branch a volume keeps its depth slices in) whose mip tail
// starts at level 1: with 16 depth slices, level 1's blocks exist for the first 8 slices only
// (HasLayer's taper) while the flat 64 KiB unit grid spans all 16. The first 64 KiB unit of every
// slice past the taper start therefore lies outside every (slice, level) window the write-back
// retiles through, and slices 0..7 have both their units inside windows.
constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t Depth = 16;
constexpr std::uint32_t MipCount = 5;
constexpr std::uint32_t Format32Float = 22;
constexpr std::size_t UnitBytes = 65536;
constexpr std::uint8_t Sentinel = 0xa5;

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

class GuestBlock {
public:
    explicit GuestBlock(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, UnitBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
        void* mapped = mmap(nullptr, bytes + UnitBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + UnitBytes - 1) & ~(static_cast<std::uintptr_t>(UnitBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + UnitBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + UnitBytes - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
        }
#endif
        Require(block != nullptr, "storage write-back no-window: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#else
        munmap(block, bytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
    std::size_t bytes = 0;
};

void Run(AgcDriver::VulkanDevice& device) {
    GuestTextureResource resource{};
    resource.width = Width;
    resource.height = Height;
    resource.depthOrLastArray = Depth - 1u;
    resource.mipCount = MipCount;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k3D;
    resource.format = Format32Float;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    Require(geometry.guestBytes >= Depth * UnitBytes, "storage write-back no-window: the fixture's slices are smaller than a 64 KiB unit");

    GuestBlock block(static_cast<std::size_t>(geometry.guestBytes));
    resource.baseAddress = AddressOf(block.Data());
    Require(resource.baseAddress % UnitBytes == 0, "storage write-back no-window: the guest block is not 64 KiB aligned");
    std::memset(block.Data(), Sentinel, static_cast<std::size_t>(geometry.guestBytes));
    AgcDriver::GuestMemory::CollectWrites(resource.baseAddress, static_cast<std::size_t>(geometry.guestBytes));
    AgcDriver::GuestMemory::BumpCollectEpoch();

    auto context = device.graphicsContext();
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    const auto texture = StorageTexture::Create(context, detiler, resource, 0);
    texture->MarkDirty();
    Require(StorageTexture::FindPending(resource.baseAddress, geometry.guestBytes) != nullptr, "storage write-back no-window: the marked image is not pending");

    // The (slice, level) windows sliceWindows would retile a kept unit through: the mip levels of
    // every depth slice HasLayer keeps, each at its tiled offset in the slice.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> windows;
    for (std::uint32_t layer = 0; layer < geometry.layers; ++layer) {
        for (std::uint32_t level = 0; level < geometry.mips.size(); ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            const auto begin = resource.baseAddress + geometry.GuestLayerOffset(layer) + geometry.mips[level].tiledOffset;
            windows.emplace_back(begin, begin + geometry.mips[level].tiledSize);
        }
    }
    Require(!windows.empty(), "storage write-back no-window: the fixture has no slice windows");

    // Every 64 KiB unit of the flat tracking grid is stored on its own: a unit inside a window
    // stores through the retiler, one outside every window must store nothing and leave the guest
    // bytes alone instead of sizing the retiler's buffers at zero.
    std::size_t units = 0, outside = 0;
    for (auto begin = resource.baseAddress; begin < resource.baseAddress + geometry.guestBytes; begin += UnitBytes) {
        const auto end = std::min(begin + UnitBytes, resource.baseAddress + geometry.guestBytes);
        const auto bytes = static_cast<std::size_t>(end - begin);
        const bool inWindow = std::any_of(windows.begin(), windows.end(), [&](const std::pair<std::uint64_t, std::uint64_t>& window) { return begin < window.second && window.first < end; });
        Require(StorageTexture::FlushPending(begin, bytes, nullptr, "test"), "storage write-back no-window: the unit at +" + Hex(begin - resource.baseAddress) + " was not listed by its flush");
        device.WaitIdle();
        Require(!StorageTexture::FlushPending(begin, bytes, nullptr, "test"), "storage write-back no-window: the unit at +" + Hex(begin - resource.baseAddress) + " stayed pending after its flush");
        ++units;
        if (inWindow) continue;
        ++outside;
        const bool untouched = std::all_of(block.Data() + (begin - resource.baseAddress), block.Data() + (end - resource.baseAddress), [](std::uint8_t byte) { return byte == Sentinel; });
        Require(untouched, "storage write-back no-window: the flush of the unit at +" + Hex(begin - resource.baseAddress) + ", which lies outside every slice window, wrote to its guest bytes");
    }
    Require(outside != 0, "storage write-back no-window: the thin volume fixture has no unit outside every slice window");
    Require(!texture->Dirty(), "storage write-back no-window: flushing every unit left the image pending");
    std::printf("storage write-back no-window tests passed (%zu units, %zu outside every window)\n", units, outside);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
