#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <optional>
#include <span>
#include <utility>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context Context(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
namespace Memory = AgcDriver::GuestMemory;

constexpr std::size_t BlockBytes = 1048576;

class GuestBlock {
public:
    GuestBlock() {
        Require(Memory::WriteWatched(), "native depth BDA clear requires guest write watching");
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes));
        Require(block != nullptr, "cannot allocate the native depth BDA clear block");
        section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, BlockBytes, nullptr);
        Require(section != nullptr, "cannot create the native depth BDA clear backing");
        GuestArena::GuestArenaMapDirect_nid_postfix(block, BlockBytes, section, 0, PAGE_READWRITE);
#else
        void* raw = mmap(nullptr, 2 * BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "cannot map the native depth BDA clear block");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
        if (aligned != begin) munmap(raw, aligned - begin);
        if (aligned != begin + BlockBytes) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + BlockBytes - aligned);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
#endif
        Require(Memory::Watched(Address(), BlockBytes), "the native depth BDA clear block is not watched");
        std::memset(block, 0xcd, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaForgetSection_nid_postfix(section);
        CloseHandle(section);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }
    const std::uint8_t* Data() const { return block; }
    void FillWith(std::uint8_t value) const { std::memset(block, value, BlockBytes); }

private:
    std::uint8_t* block = nullptr;
#ifdef _WIN32
    HANDLE section = nullptr;
#endif
};

enum class ClearConsumer { Owner, Texture, Attachment };

int RunNativeClear(AgcDriver::VulkanDevice& device, ClearConsumer consumer) {
    using namespace ShaderRecompiler;
    using namespace AgcDriver::Graphics;
    const std::lock_guard lock(Memory::GpuMutex());
    const GuestBlock depth, metadata;
    metadata.FillWith(0);
    constexpr VkExtent2D extent{5u, 3u};
    const auto metadataBytes = static_cast<std::size_t>(HtileMetadataBytes(extent));
    alignas(256) static constexpr std::array<std::uint32_t, 17> code{
        0xd7460000, 0x04010c06, 0x34000084, 0xdc388000, 0x04020000,
        0xbf8c3f70, 0xd7710008, 0x00120805, 0xd7710009, 0x00120a05,
        0xd771000a, 0x00120c05, 0xd771000b, 0x00120e05,
        0xdc788000, 0x00020800, 0xbf810000,
    };
    std::array<std::uint32_t, 6> userData{0u, 0u, static_cast<std::uint32_t>(metadata.Address()), static_cast<std::uint32_t>(metadata.Address() >> 32u), 0u, 0u};
    const ShaderComputeStageInfo compute{{64u, 1u, 1u}, 0u, {true, false, false}, false, 1u};
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    const auto groups = static_cast<std::uint32_t>(metadataBytes / 1024u);
    const std::array<std::uint32_t, 5> packet{0u, groups, 1u, 1u, 1u};
    Require(HtileDepthClearBytes_nid_no_patch(code, userData, compute, packet, 0u) == metadataBytes, "the native GPU clear kernel is not recognized");
    const auto compile = [&] {
        RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {64u, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device.Target(), {0, 0, 0, 128}, std::nullopt, false,
        };
        auto result = Recompile(request);
        Require(std::ranges::any_of(result.bindings, [](const auto& binding) { return binding.role == DescriptorRole::BdaPagetable; }), "the native clear shader did not use BDA");
        return result;
    };
    device.Dispatch(compile(), groups, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    Require(std::all_of(metadata.Data(), metadata.Data() + BlockBytes, [](auto value) { return value == 0u; }), "the warm-up native clear did not retain zero metadata");
    if (!Memory::Watched(depth.Address(), BlockBytes) || !Memory::Watched(metadata.Address(), BlockBytes)) {
        std::puts("skipped, the device does not retain guest write watching for native clear imports");
        return VulkanTestSkipped;
    }
    const auto context = AgcDriver::VulkanDeviceTestAccess::Context(device);
    Require(context.recorder != nullptr, "the native clear context has no recorder");
    context.recorder->Activate();
    struct Cleanup {
        AgcDriver::VulkanDevice& device;
        ~Cleanup() { device.WaitIdle(); ClearCachedTextures(device.Device()); ClearDepthSurfaces(device.Device()); }
    } cleanup{device};
    DepthTarget target{depth.Address(), 0u, extent, VK_FORMAT_D32_SFLOAT, 0.0f, 0u, metadata.Address(), false, 4u};
    target.depthSwizzle = 24u;
    target.htileDepthOnly = target.iterate256 = target.htilePipeAligned = true;
    const auto view = DepthSurfaceView(context, target);
    const auto base = target.address >> 8u;
    const auto htile = target.htileAddress >> 8u;
    const std::array<std::uint32_t, 8> words{
        static_cast<std::uint32_t>(base), static_cast<std::uint32_t>((base >> 32u) & 255u) | (22u << 20u),
        1u | (2u << 14u), (14u << 28u) | (24u << 20u) | (2u << 16u) | 0x204u, 0u, 2u << 4u,
        (1u << 10u) | (1u << 19u) | (1u << 21u) | (static_cast<std::uint32_t>(htile & 255u) << 24u), static_cast<std::uint32_t>(htile >> 8u),
    };
    const auto sampled = CachedSampledTexture(context, words);
    Require(sampled != nullptr, "the native GPU clear has no initial sampled view");
    device.WaitIdle();
    target.clearDepth = 1.0f;
    Require(DepthSurfaceView(context, target) == view, "changing the native clear value replaced its attachment");
    std::optional<std::pair<std::uint64_t, std::size_t>> htileClear;
    if (NoteHtileDepthClear(target.htileAddress, metadataBytes, metadataBytes)) htileClear.emplace(target.htileAddress, metadataBytes);
    Require(htileClear.has_value(), "the full BDA clear was not registered for completion");
    const auto generation = Memory::TrackerGeneration();
    userData[4] = 0xfffffff0u;
    device.Dispatch(compile(), groups, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()), nullptr, nullptr, htileClear);
    std::puts("Native full BDA clear: checking sampled owner before explicit WaitIdle");
    Require(context.recorder->PendingWriteOverlaps(target.htileAddress, metadataBytes), "the recognized HTILE clear is missing its pending write range");
    switch (consumer) {
        case ClearConsumer::Owner:
            Require(DepthSurfaceHolds(context, DecodeTextureResource(words), sampled.get()), "a pending full BDA clear lost its native sampled owner");
            break;
        case ClearConsumer::Texture:
            Require(CachedSampledTexture(context, words) == sampled, "a pending full BDA clear lost its native sampled view");
            break;
        case ClearConsumer::Attachment:
            Require(DepthSurfaceView(context, target) == view, "a pending full BDA clear lost its native attachment");
            break;
    }
    std::printf("Native full BDA clear: consumer=%u settled the recognized clear before WaitIdle\n", static_cast<unsigned>(consumer));
    Require(!context.recorder->PendingWriteOverlaps(target.htileAddress, metadataBytes), "the native HTILE consumer did not settle its clear's pending write");
    Require(Memory::StoredAllSince_nid_no_patch(target.htileAddress, metadataBytes, generation), "the native HTILE consumer did not complete its recognized clear");
    device.WaitIdle();
    for (std::size_t offset = 0; offset < BlockBytes; offset += 4u) {
        std::uint32_t value = 0;
        std::memcpy(&value, metadata.Data() + offset, 4u);
        Require(value == (offset < metadataBytes ? 0xfffffff0u : 0u), "the native GPU clear did not write its exact metadata range");
    }
    std::printf("Native full BDA clear: exact %zu-byte metadata readback passed; complete=%d\n", metadataBytes, Memory::StoredAllSince_nid_no_patch(target.htileAddress, metadataBytes, generation));
    Require(Memory::StoredAllSince_nid_no_patch(target.htileAddress, metadataBytes, generation), "the completed recognized native clear did not record exact coverage");
    Require(CachedSampledTexture(context, words) == sampled, "a completed full BDA clear lost its native sampled view");
    Require(DepthSurfaceView(context, target) == view, "a completed full BDA clear replaced its native attachment");
    device.WaitIdle();
    Require(std::all_of(depth.Data(), depth.Data() + BlockBytes, [](auto value) { return value == 0xcdu; }), "the native GPU clear changed guest depth bytes");
    std::puts("Native full BDA clear accepted");
    return 0;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto consumer : {ClearConsumer::Owner, ClearConsumer::Texture, ClearConsumer::Attachment}) {
            const auto result = RunNativeClear(*device, consumer);
            if (result != 0) return result;
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
