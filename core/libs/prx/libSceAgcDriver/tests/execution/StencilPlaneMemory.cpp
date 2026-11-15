#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
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
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::DrawOptions;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;

constexpr std::uint32_t PlaneWidth = 256;
constexpr std::uint32_t PlaneHeight = 256;
constexpr VkExtent2D PlaneExtent{PlaneWidth, PlaneHeight};
constexpr std::size_t Block = 65536;
constexpr std::size_t DepthBytes = 4u * Block;
constexpr std::size_t StencilBytes = Block;
constexpr std::size_t PlaneTexels = static_cast<std::size_t>(PlaneWidth) * PlaneHeight;
alignas(256) std::array<std::byte, PlaneTexels * 4u> Colors{};

void* AllocateBlocks(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
    Require(block != nullptr, "cannot allocate the depth and stencil planes");
#else
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED, "cannot map the depth and stencil planes");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + Block - 1) & ~static_cast<std::uintptr_t>(Block - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    return block;
}

void Fill(std::uint8_t* plane, std::uint8_t value) {
    std::fill(plane, plane + StencilBytes, value);
    AgcDriver::GuestMemory::BumpCollectEpoch();
}

std::size_t Holding(const std::uint8_t* plane, std::uint8_t value) {
    return static_cast<std::size_t>(std::count(plane, plane + StencilBytes, value));
}

VkStencilOpState Stencil(VkCompareOp compare, VkStencilOp pass, std::uint32_t reference) {
    return {VK_STENCIL_OP_KEEP, pass, VK_STENCIL_OP_KEEP, compare, 0xffu, 0xffu, reference};
}

void CheckConsumedStencilClear(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, std::uint32_t swizzle, bool sampled) {
    auto* blocks = static_cast<std::byte*>(AllocateBlocks(DepthBytes + StencilBytes + Block));
    auto* depth = reinterpret_cast<float*>(blocks);
    auto* stencil = reinterpret_cast<std::uint8_t*>(blocks + DepthBytes);
    auto* htile = reinterpret_cast<std::uint32_t*>(blocks + DepthBytes + StencilBytes);
    const auto htileBytes = AgcDriver::Graphics::HtileMetadataBytes(PlaneExtent);
    Require(htileBytes <= Block, "the clear metadata does not fit its allocation");
    std::fill(depth, depth + PlaneTexels, 1.0f);
    Fill(stencil, 0u);
    std::fill(htile, htile + htileBytes / sizeof(*htile), 0xffffffffu);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto stencilBase = reinterpret_cast<std::uintptr_t>(stencil);
    Surface surface{reinterpret_cast<std::uintptr_t>(depth), stencilBase, reinterpret_cast<std::uintptr_t>(htile), true, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, 0, swizzle};
    Require(Draw(device, shaders, surface, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x18u)}, PlaneExtent, Colors) == PlaneTexels, "the clear control draw did not cover the plane");
    Require(!AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "the clear control did not store its initial stencil");
    device.WaitIdle();
    Require(Holding(stencil, 0x18u) == StencilBytes, "the clear control stencil was not stored");
    Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "the clear control stencil is not clean");
    std::fill(htile, htile + htileBytes / sizeof(*htile), 0u);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    AgcDriver::Graphics::NoteDepthMetadataFill(surface.htile, htileBytes, 0u);
    if (sampled) {
        AgcDriver::Graphics::GuestTextureResource resource{};
        resource.baseAddress = stencilBase;
        resource.width = PlaneWidth;
        resource.height = PlaneHeight;
        resource.format = 5u;
        resource.dimension = AgcDriver::Graphics::TextureDimension::k2D;
        resource.tileMode = AgcDriver::Graphics::TextureTileMode::kZ64KBX;
        resource.mipCount = resource.allocatedMipCount = 1u;
        resource.dstSelX = 4u; resource.dstSelY = 5u; resource.dstSelZ = 6u; resource.dstSelW = 7u;
        const std::array<std::uint32_t, 8> words{};
        Require(AgcDriver::Graphics::DepthSurfaceTexture(device.graphicsContext(), words, resource, {}) != nullptr, "the stencil clear could not be sampled");
    } else {
        Require(!AgcDriver::Graphics::FlushDepthPlanes(surface.depth, DepthBytes).empty(), "the depth-only flush did not consume the pending clear");
    }
    device.WaitIdle();
    surface.htile = 0u;
    Require(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = false}, PlaneExtent, Colors) == PlaneTexels, "the read-only rebind after a clear did not cover the plane");
    Require(!AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a consumed stencil clear was lost on a read-only rebind");
    device.WaitIdle();
    Require(Holding(stencil, 0u) == StencilBytes, "the consumed stencil clear did not reach guest memory");
    Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "the consumed stencil clear was stored twice");
}

void CheckGuestWriteBeforeRepeatedClears(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, std::uint32_t swizzle) {
    auto* blocks = static_cast<std::byte*>(AllocateBlocks(DepthBytes + StencilBytes + Block));
    auto* depth = reinterpret_cast<float*>(blocks);
    auto* stencil = reinterpret_cast<std::uint8_t*>(blocks + DepthBytes);
    auto* htile = reinterpret_cast<std::uint32_t*>(blocks + DepthBytes + StencilBytes);
    const auto htileBytes = AgcDriver::Graphics::HtileMetadataBytes(PlaneExtent);
    std::fill(depth, depth + PlaneTexels, 1.0f);
    Fill(stencil, 0u);
    std::fill(htile, htile + htileBytes / sizeof(*htile), 0xffffffffu);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto stencilBase = reinterpret_cast<std::uintptr_t>(stencil);
    const Surface surface{reinterpret_cast<std::uintptr_t>(depth), stencilBase, reinterpret_cast<std::uintptr_t>(htile), true, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, 0, swizzle};
    Require(Draw(device, shaders, surface, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x18u)}, PlaneExtent, Colors) == PlaneTexels, "the draw before the guest write did not cover the plane");
    Require(!AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "the stencil drawn before the guest write was not stored");
    device.WaitIdle();
    Require(Holding(stencil, 0x18u) == StencilBytes, "the stencil drawn before the guest write did not reach guest memory");
    Fill(stencil, 0x77u);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    for (int clear = 0; clear < 2; ++clear) {
        std::fill(htile, htile + htileBytes / sizeof(*htile), 0u);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        AgcDriver::Graphics::NoteDepthMetadataFill(surface.htile, htileBytes, 0u);
    }
    static_cast<void>(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes));
    device.WaitIdle();
    Require(Holding(stencil, 0x77u) == StencilBytes, "stale stencil planes re-marked by HTILE clears were stored over the guest's write after their last store");
    Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a stencil plane the guest wrote after its last store stayed stale");
}

#ifdef __linux__
struct SharedPlanes {
    int file = -1;
    std::uintptr_t begin = 0;
    std::size_t bytes = 0;
    std::uint64_t offset = 0;
};
SharedPlanes sharedPlanes;

bool ResolveSharedPlanes(std::uintptr_t address, std::size_t bytes, int* file, std::uint64_t* offset) {
    if (sharedPlanes.file < 0 || bytes == 0 || address < sharedPlanes.begin || address - sharedPlanes.begin >= sharedPlanes.bytes || bytes > sharedPlanes.bytes - (address - sharedPlanes.begin)) return false;
    *file = fcntl(sharedPlanes.file, F_DUPFD_CLOEXEC, 0);
    *offset = sharedPlanes.offset + (address - sharedPlanes.begin);
    return *file >= 0;
}
#endif

void* AllocateSharedBlocks(std::size_t bytes) {
#ifdef __linux__
    const int file = memfd_create("stencil-plane-memory", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    Require(file >= 0, "cannot create the shared depth and stencil planes");
    Require(ftruncate(file, static_cast<off_t>(bytes + Block)) == 0 && fcntl(file, F_ADD_SEALS, F_SEAL_SHRINK) == 0, "cannot size the shared depth and stencil planes");
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    Require(raw != MAP_FAILED, "cannot map the shared depth and stencil planes");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + Block - 1) & ~static_cast<std::uintptr_t>(Block - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
    sharedPlanes = {file, aligned, bytes, aligned - begin};
    GuestArena::GuestArenaSetSharedBacking_nid_postfix(ResolveSharedPlanes);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
    GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    return block;
#else
    return AllocateBlocks(bytes);
#endif
}

void CheckReusedStencilPlane(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, std::uint32_t swizzle) {
    auto* blocks = static_cast<std::byte*>(AllocateSharedBlocks(DepthBytes + StencilBytes));
    auto* depth = reinterpret_cast<float*>(blocks);
    auto* stencil = reinterpret_cast<std::uint8_t*>(blocks + DepthBytes);
    std::fill(depth, depth + DepthBytes / sizeof(float), 1.0f);
    Fill(stencil, 0u);
    const auto stencilBase = reinterpret_cast<std::uintptr_t>(stencil);
    const Surface surface{reinterpret_cast<std::uintptr_t>(depth), stencilBase, 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, 0, swizzle};
    const auto draw = [&](std::uint8_t reference) {
        Require(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, reference)}, PlaneExtent, Colors) == PlaneTexels, "a draw writing stencil over the reused plane did not cover it");
    };
    draw(0x18u);
    Require(!AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the shared stencil plane after a draw stored nothing");
    device.WaitIdle();
    Require(Holding(stencil, 0x18u) == StencilBytes, "the stencil drawn into the shared plane did not reach guest memory");
    if (!AgcDriver::GuestMemory::Watched(stencilBase, StencilBytes)) {
        std::puts("the shared stencil plane is not write-watched here: a guest reusing the plane's memory is not exercised");
        return;
    }
    constexpr std::size_t RecordBytes = 0x30;
    constexpr std::array<std::size_t, 3> RecordOffsets{0x1000, 0x7dd0, StencilBytes - RecordBytes};
    const auto writeRecords = [&](std::uint8_t value) {
        for (const auto offset : RecordOffsets) std::fill(stencil + offset, stencil + offset + RecordBytes, value);
        AgcDriver::GuestMemory::BumpCollectEpoch();
    };
    const auto recordsHold = [&](std::uint8_t value) {
        return std::all_of(RecordOffsets.begin(), RecordOffsets.end(), [&](std::size_t offset) { return std::all_of(stencil + offset, stencil + offset + RecordBytes, [&](std::uint8_t byte) { return byte == value; }); });
    };

    draw(0x66u);
    Fill(stencil, 0x77u);
    const bool storedOverFill = !AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty();
    device.WaitIdle();
    Require(Holding(stencil, 0x77u) == StencilBytes, "a read of a stencil plane the guest wrote over after a draw stored the stencil drawn before the write over the guest's bytes");
    Require(!storedOverFill, "a read of a stencil plane the guest wrote over after a draw stored the stencil drawn before the write");

    draw(0x2bu);
    writeRecords(0x7au);
    AgcDriver::Graphics::RetireDepthSurfaces(device.graphicsContext().device, surface.depth, DepthBytes);
    device.WaitIdle();
    Require(recordsHold(0x7au), "retiring the surface stored the stencil drawn before the guest wrote records into the plane over them");
    Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane whose retired surface's stencil was dropped stored it");

    draw(0x4du);
    writeRecords(0x3cu);
    static_cast<void>(AgcDriver::Graphics::FlushDepthPlanes(surface.depth, DepthBytes + StencilBytes, AgcDriver::Graphics::DepthPlaneScope::Whole));
    device.WaitIdle();
    Require(recordsHold(0x3cu), "a whole read of the planes stored the stencil drawn before the guest wrote records into the plane over them");

    writeRecords(0x5eu);
    draw(0x18u);
    Require(!AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane drawn after the guest's records stored nothing");
    device.WaitIdle();
    Require(Holding(stencil, 0x18u) == StencilBytes, "the stencil drawn after the guest wrote records did not reach guest memory over them");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(AgcDriver::Graphics::DepthSliceBytes(PlaneExtent, 4u) == DepthBytes && AgcDriver::Graphics::DepthSliceBytes(PlaneExtent, 1u) == StencilBytes, "the 256x256 planes are not four and one 64 KiB tiles");
        auto* blocks = static_cast<std::byte*>(AllocateBlocks(DepthBytes + StencilBytes));
        auto* depthPlane = reinterpret_cast<float*>(blocks);
        auto* stencilPlane = reinterpret_cast<std::uint8_t*>(blocks + DepthBytes);
        std::fill(depthPlane, depthPlane + DepthBytes / sizeof(float), 1.0f);
        Fill(stencilPlane, 0u);
        const auto stencilBase = reinterpret_cast<std::uintptr_t>(stencilPlane);
        const auto shaders = Compile(*device);
        const auto swizzle = AgcDriver::Graphics::XorSwizzleMode(AgcDriver::Graphics::TextureTileMode::kZ64KBX);
        const Surface surface{reinterpret_cast<std::uintptr_t>(depthPlane), stencilBase, 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, 0, swizzle};
        const auto draw = [&](const DrawOptions& options) { return Draw(*device, shaders, surface, options, PlaneExtent, Colors); };

        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x18u)}) == PlaneTexels, "the draw writing stencil 0x18 did not cover the plane");
        Require(Holding(stencilPlane, 0u) == StencilBytes, "the draw writing stencil changed the guest stencil plane before anything read it");
        Require(!AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane after a draw wrote stencil stored nothing");
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x18u) == StencilBytes, "a read of the stencil plane after a draw wrote stencil 0x18 does not see it in guest memory");
        Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a second read of the stored stencil plane stored it again");

        CheckConsumedStencilClear(*device, shaders, swizzle, true);
        CheckConsumedStencilClear(*device, shaders, swizzle, false);
        CheckGuestWriteBeforeRepeatedClears(*device, shaders, swizzle);

        std::size_t readOnlyFailures = 0;
        const auto checkReadOnly = [&](const DrawOptions& options, const char* name) {
            Require(draw(options) == PlaneTexels, "a read-only stencil draw did not cover the plane");
            const bool dirty = !AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty();
            device->WaitIdle();
            Require(Holding(stencilPlane, 0x18u) == StencilBytes, "a read-only draw changed the stored stencil bytes");
            std::cout << name << ": " << (dirty ? "dirtied the stored stencil plane" : "kept the stored stencil plane clean") << '\n';
            if (dirty) ++readOnlyFailures;
        };
        checkReadOnly({.depthTest = false, .stencilTest = false}, "stencil-off");
        checkReadOnly({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP, 0x18u)}, "all-KEEP");
        auto maskedStencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x99u);
        maskedStencil.writeMask = 0u;
        checkReadOnly({.depthTest = false, .stencilTest = true, .stencil = maskedStencil}, "zero-mask");

        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x2au)}) == PlaneTexels, "the draw writing stencil 0x2a did not cover the plane");
        AgcDriver::Graphics::RetireDepthSurfaces(device->graphicsContext().device, reinterpret_cast<std::uintptr_t>(depthPlane), DepthBytes);
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x2au) == StencilBytes, "retiring the surface for a write over its depth plane only dropped the stencil 0x2a a draw wrote");
        Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane stored after its surface retired stored it again");

        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x3cu)}) == PlaneTexels, "the draw writing stencil 0x3c did not cover the plane");
        device->WaitIdle();
        Fill(stencilPlane, 0x77u);
        AgcDriver::Graphics::RetireDepthSurfaces(device->graphicsContext().device, reinterpret_cast<std::uintptr_t>(depthPlane), DepthBytes);
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x77u) == StencilBytes, "retiring the surface stored its stale stencil 0x3c over the guest's later write of 0x77");
        Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane the guest overwrote stored the stale stencil");

        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x3cu)}) == PlaneTexels, "the draw writing stencil 0x3c before a guest write and a second draw did not cover the plane");
        device->WaitIdle();
        Fill(stencilPlane, 0x44u);
        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x55u)}) == PlaneTexels, "the draw writing stencil 0x55 after the guest write did not cover the plane");
        device->WaitIdle();
        AgcDriver::Graphics::RetireDepthSurfaces(device->graphicsContext().device, reinterpret_cast<std::uintptr_t>(depthPlane), DepthBytes);
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x55u) == StencilBytes, "retiring the surface kept the guest's 0x44 over the stencil 0x55 a later draw wrote");

        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x3cu)}) == PlaneTexels, "the draw writing stencil 0x3c before a guest write and a read did not cover the plane");
        device->WaitIdle();
        Fill(stencilPlane, 0x77u);
        Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane the guest overwrote after the last draw stored the stale stencil");
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x77u) == StencilBytes, "a read of the stencil plane replaced the guest's write after the last draw");

        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x3cu)}) == PlaneTexels, "the draw writing stencil 0x3c before a guest write, a second draw and a read did not cover the plane");
        device->WaitIdle();
        Fill(stencilPlane, 0x66u);
        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x55u)}) == PlaneTexels, "the draw writing stencil 0x55 after the guest write and before a read did not cover the plane");
        device->WaitIdle();
        Require(!AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane a draw wrote after the guest's write stored nothing");
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x55u) == StencilBytes, "a read of the stencil plane kept the guest's 0x66 over the stencil 0x55 a later draw wrote");
        AgcDriver::Graphics::RetireDepthSurfaces(device->graphicsContext().device, reinterpret_cast<std::uintptr_t>(depthPlane), DepthBytes);
        device->WaitIdle();

        if (!AgcDriver::GuestMemory::WriteWatched() || !AgcDriver::GuestMemory::Watched(stencilBase, StencilBytes)) {
            std::puts("guest memory is not write-watched here: a guest write into the stencil plane is not exercised");
        } else {
            Fill(stencilPlane, 0x05u);
            Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, 0x05u)}) == PlaneTexels, "a draw testing stencil EQUAL 0x05 after the guest wrote 0x05 into the stencil plane did not pass everywhere");
            Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, 0x18u)}) == 0u, "a draw testing stencil EQUAL 0x18 passed after the guest wrote 0x05 over the stored stencil plane");
        }
        Require(readOnlyFailures == 0, "read-only stencil draws dirtied the stored plane");

        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x33u)}) == PlaneTexels, "the draw writing stencil 0x33 before the unmap did not cover the plane");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(blocks);
            mutation.Add(blocks, DepthBytes + StencilBytes, true, true, true);
        }
        Fill(stencilPlane, 0x44u);
        Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of a stencil plane unmapped and mapped again stored the stencil drawn before the unmap");
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x44u) == StencilBytes, "the stencil drawn before the unmap reached the memory mapped again");
        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x18u)}) == PlaneTexels, "the draw writing stencil 0x18 after the unmap did not cover the plane");
        Require(!AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane drawn after the unmap stored nothing");
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x18u) == StencilBytes, "the stencil drawn after the unmap did not reach guest memory");

        CheckReusedStencilPlane(*device, shaders, swizzle);
        std::puts("stencil plane memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
