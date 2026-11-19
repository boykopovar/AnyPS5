#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/WriteEvidence.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
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
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static EntryOutcome Validate(std::uint64_t address, std::uint32_t expected) {
        DispatchVariant variant;
        variant.compiled = std::make_shared<ShaderRecompiler::RecompileResult>();
        variant.runs.emplace_back(address, address + sizeof(expected));
        variant.words.push_back(expected);
        std::vector<ShaderRecompiler::MemoryRegion> regions;
        Driver::appendEntryRegions(variant, regions);
        std::uint64_t imagesFlushed = 0, runsSynced = 0;
        std::optional<SampledReadScope> sampling;
        return Driver::Get().validateVariant(0, 0, variant, regions, imagesFlushed, runsSynced, sampling);
    }
};

}

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::DriverDetail::DriverTestAccess;
using AgcDriver::DriverDetail::EntryOutcome;
constexpr std::size_t UnitBytes = 65536;
constexpr std::uint32_t Initial = 0x5a5a5a5a;
constexpr std::uint32_t Written = 0x11223344;

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
        Require(block != nullptr, "pending image capture: cannot allocate the guest block");
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

void ClearLayer(const Context& context, Recorder& recorder, const std::shared_ptr<StorageTexture>& texture,
                std::uint32_t layer, std::uint32_t pattern) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto commands = recorder.Commands();
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, layer, 1};
    VkClearColorValue value{};
    value.uint32[0] = pattern;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, texture->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    texture->MarkSliceDirty(layer);
    recorder.Sync();
}

void RequireCurrent(std::uint64_t address, std::uint32_t expected) {
    Require(DriverTestAccess::Validate(address, expected) == EntryOutcome::Equal,
            "pending image capture: published data did not validate as equal");
}

void RequireStale(std::uint64_t address, std::uint32_t expected) {
    const auto result = DriverTestAccess::Validate(address, expected);
    Require(result != EntryOutcome::Equal && result != EntryOutcome::EqualData,
            "pending image capture: an overlapping GPU write reused the stale snapshot");
}

void RequireUntouched(std::uint64_t address, std::uint32_t expected) {
    const auto result = DriverTestAccess::Validate(address, expected);
    Require(result == EntryOutcome::Equal || result == EntryOutcome::FlushingImage,
            "pending image capture: unchanged data was reported different or inaccessible");
}

void Run(AgcDriver::VulkanDevice& device) {
    GuestTextureResource resource{};
    resource.width = resource.height = 128;
    resource.depthOrLastArray = 1;
    resource.mipCount = resource.allocatedMipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2DArray;
    resource.format = 20;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    Require(geometry.layers == 2 && geometry.guestBytes == UnitBytes * 2 && geometry.GuestLayerOffset(1) == UnitBytes,
            "pending image capture: the fixture does not have two separate 64 KiB layers");
    GuestBlock block(geometry.guestBytes);
    std::memset(block.Data(), 0x5a, geometry.guestBytes);
    resource.baseAddress = AddressOf(block.Data());
    AgcDriver::GuestMemory::CollectWrites(resource.baseAddress, geometry.guestBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    auto context = device.graphicsContext();
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    Recorder recorder(context);
    recorder.Activate();
    std::shared_ptr<StorageTexture> texture;
    struct Cleanup {
        Recorder& recorder;
        std::shared_ptr<StorageTexture>& texture;
        ~Cleanup() {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            recorder.Sync();
            if (texture) texture->Flush("pending image capture cleanup");
            recorder.Sync();
            texture.reset();
        }
    } cleanup{recorder, texture};
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        texture = StorageTexture::Create(context, detiler, resource, 0);
        recorder.Sync();
    }
    const auto first = resource.baseAddress;
    const auto second = first + UnitBytes;
    RequireCurrent(first, Initial);
    RequireCurrent(second, Initial);
    const auto publish = [&] {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        texture->Flush("pending image capture publication");
        recorder.Sync();
    };
    ClearLayer(context, recorder, texture, 0, Written);
    RequireUntouched(second, Initial);
    RequireStale(first, Initial);
    publish();
    Require(DriverTestAccess::Validate(first, Initial) == EntryOutcome::Differing,
            "pending image capture: the first published GPU write was not different");
    RequireCurrent(first, Written);
    RequireCurrent(second, Initial);
    constexpr std::uint32_t secondWritten = 0x55667788;
    ClearLayer(context, recorder, texture, 1, secondWritten);
    RequireUntouched(first, Written);
    RequireStale(second, Initial);
    publish();
    Require(DriverTestAccess::Validate(second, Initial) == EntryOutcome::Differing,
            "pending image capture: the second published GPU write was not different");
    RequireCurrent(first, Written);
    RequireCurrent(second, secondWritten);
    std::uint32_t firstStored = 0, secondStored = 0;
    std::memcpy(&firstStored, block.Data(), sizeof(firstStored));
    std::memcpy(&secondStored, block.Data() + UnitBytes, sizeof(secondStored));
    Require(firstStored == Written && secondStored == secondWritten,
            "pending image capture: publication did not preserve both GPU-written layers");
    std::puts("pending image capture validation passed: two GPU writes, stale and current snapshots, preserved layers");
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
