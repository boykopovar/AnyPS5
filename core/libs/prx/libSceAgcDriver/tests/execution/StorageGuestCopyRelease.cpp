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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DescribeSurface;
using AgcDriver::Graphics::GuestTextureResource;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using AgcDriver::Graphics::TextureDetiler;
using AgcDriver::Graphics::TextureDimension;
using AgcDriver::Graphics::TextureTileMode;

constexpr std::uint32_t Width = 512;
constexpr std::uint32_t Height = 512;
constexpr std::uint32_t Format32Float = 22;
constexpr std::size_t UnitBytes = 65536;
constexpr std::uint8_t Sentinel = 0x3c;

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
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
        Require(block != nullptr, "storage guest copy release: cannot allocate the guest block");
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

GuestTextureResource Surface(std::uint64_t address) {
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = Width;
    resource.height = Height;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kStandard64KB;
    resource.dimension = TextureDimension::k2D;
    resource.format = Format32Float;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    return resource;
}

std::size_t SurfaceBytes() {
    return static_cast<std::size_t>(DescribeSurface(Surface(0)).guestBytes);
}

bool Case(AgcDriver::VulkanDevice& device, bool whole) {
    const auto bytes = SurfaceBytes();
    GuestBlock block(bytes);
    auto resource = Surface(AddressOf(block.Data()));
    std::memset(block.Data(), Sentinel, bytes);
    AgcDriver::GuestMemory::CollectWrites(resource.baseAddress, bytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    if (!AgcDriver::GuestMemory::Watched(resource.baseAddress, bytes)) {
        std::printf("storage guest copy release: the guest block is not write-watched on this host; skipped\n");
        return false;
    }

    auto context = device.graphicsContext();
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    const auto texture = StorageTexture::Create(context, detiler, resource, 0);
    Require(texture->TrackedUnits() > 2, "storage guest copy release: the fixture has fewer than three 64 KiB units");
    Require(texture->GuestCopyCapacity() == bytes, "storage guest copy release: the CPU-path upload kept no copy of the guest bytes (" + std::to_string(texture->GuestCopyCapacity()) + " of " + std::to_string(bytes) + " bytes)");

    std::string failure;
    if (whole) {
        texture->MarkWholeWritten();
        if (texture->GuestCopyCapacity() != 0) failure = "the copy of the guest bytes outlived the last unit vouching for it (" + std::to_string(texture->GuestCopyCapacity()) + " bytes held after a whole-surface write)";
    } else {
        std::vector<bool> units(texture->TrackedUnits(), true);
        units.back() = false;
        texture->MarkUnitsWritten(units);
        if (texture->GuestCopyCapacity() != bytes) failure = "the copy of the guest bytes was released while one unit still vouches for it";
        std::vector<bool> last(texture->TrackedUnits(), false);
        last.back() = true;
        texture->MarkUnitsWritten(last);
        if (failure.empty() && texture->GuestCopyCapacity() != 0) failure = "the copy of the guest bytes outlived the last unit vouching for it (" + std::to_string(texture->GuestCopyCapacity()) + " bytes held after every unit was written)";
    }

    Require(StorageTexture::FlushPending(resource.baseAddress, bytes, nullptr, "test"), "storage guest copy release: the written image was not listed by its flush");
    device.WaitIdle();
    Require(!texture->Dirty(), "storage guest copy release: the flush left the image pending");
    Require(failure.empty(), "storage guest copy release: " + std::string(whole ? "whole-surface" : "per-unit") + " case: " + failure);
    std::printf("storage guest copy release: %s case passed (%zu bytes, %u units)\n", whole ? "whole-surface" : "per-unit", bytes, texture->TrackedUnits());
    return true;
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_NO_HOST_IMPORT", "1");
#else
    setenv("APS5_NO_HOST_IMPORT", "1", 1);
#endif
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!Case(*device, true) || !Case(*device, false)) return VulkanTestSkipped;
        std::printf("storage guest copy release tests passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
