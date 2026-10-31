#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/Shutdown.hpp"
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

namespace DriverDetail {

struct DriverTestAccess {
    static void StoreBufferFill(VulkanDevice& device, std::uint64_t base, std::size_t bytes, std::uint32_t pattern) {
        const std::shared_ptr<VulkanDevice> borrowed(&device, [](VulkanDevice*) {});
        Driver::Get().storeBufferFill(0, base, bytes, {pattern, pattern, pattern, pattern}, borrowed, false, false);
        device.WaitIdle();
    }
};

}

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
constexpr std::size_t PlaneTexels = static_cast<std::size_t>(PlaneWidth) * PlaneHeight;
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
        constexpr std::uint32_t One = 0x3f800000u;

        Fill(plane, 1.0f);
        Require(draw(depth, writeHalf) == PlaneTexels, "the draw writing depth 0.5 did not cover the plane");
        Require(AgcDriver::Graphics::DepthPlanesStale(), "the draw writing depth left the plane unregistered");
        Require(AgcDriver::Graphics::DiscardFilledDepthPlanes(base, PlaneBytes - Block, One) == 0, "a fill shorter than the stale plane discarded it");
        Require(AgcDriver::Graphics::DiscardFilledDepthPlanes(base + Block, PlaneBytes, One) == 0, "a fill starting past the stale plane discarded it");
        Require(AgcDriver::Graphics::DiscardFilledDepthPlanes(base, PlaneBytes, 0xbf800000u) == 0, "a fill of a value outside 0 to 1 discarded the stale plane");
        Require(AgcDriver::Graphics::DiscardFilledDepthPlanes(base, PlaneBytes, 0x7fc00000u) == 0, "a fill of a NaN discarded the stale plane");
        Require(AgcDriver::Graphics::DepthPlanesStale(), "fills that do not clear the plane left it unregistered");

        Require(AgcDriver::Graphics::DiscardFilledDepthPlanes(base - Block, PlaneBytes + Block, One) == 1, "a fill of 1.0 over the whole stale plane did not discard it");
        Require(!AgcDriver::Graphics::DepthPlanesStale(), "a discarded plane is still registered as stale");
        auto before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        Require(AgcDriver::Graphics::FlushDepthPlanes(base, PlaneBytes).empty(), "a read of a discarded plane stored it");
        device->WaitIdle();
        Require(Since(before).stores == 0, "a read of a discarded plane counted a store");
        for (std::size_t unit = 0; unit < Units; ++unit) RequireUnit(plane, unit, 1.0f, UnitTexels, "the discarded plane before the fill lands");

        Require(draw(depth, {}) == PlaneTexels, "a draw at depth 0.5 failed LESS against the 1.0 the discarding fill cleared the plane to");
        static_cast<void>(AgcDriver::Graphics::FlushDepthPlanes(base, PlaneBytes));
        device->WaitIdle();
        for (std::size_t unit = 0; unit < Units; ++unit) RequireUnit(plane, unit, 1.0f, UnitTexels, "the plane after the fill and a draw that wrote no depth");

        Require(draw(depth, writeHalf) == PlaneTexels, "the second draw writing depth 0.5 did not cover the plane");
        Require(AgcDriver::Graphics::DiscardFilledDepthPlanes(base, PlaneBytes, One) == 1, "a fill of exactly the stale plane did not discard it");
        Require(AgcDriver::Graphics::DiscardFilledDepthPlanes(base, PlaneBytes, One) == 0, "a second fill discarded a plane that was no longer stale");
        Require(draw(depth, {}) == PlaneTexels, "a draw after the second fill failed LESS against 1.0");

        Require(draw(depth, writeHalf) == PlaneTexels, "the third draw writing depth 0.5 did not cover the plane");
        Require(AgcDriver::Graphics::DiscardFilledDepthPlanes(base, PlaneBytes, 0u) == 1, "a fill of 0.0 over the stale plane did not discard it");
        Require(draw(depth, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.0 the discarding fill cleared the plane to");

        Require(draw(depth, writeHalf) == PlaneTexels, "the draw writing depth 0.5 before the driver's fill did not cover the plane");
        Require(AgcDriver::Graphics::DepthPlanesStale(), "the draw before the driver's fill left the plane unregistered");
        before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        AgcDriver::DriverDetail::DriverTestAccess::StoreBufferFill(*device, base - Block, PlaneBytes + Block, 0x3f400000u);
        Require(Since(before).stores == 0, "the driver's fill of 0.75 over the whole stale plane stored the plane first");
        Require(!AgcDriver::Graphics::DepthPlanesStale(), "the driver's fill over the whole stale plane left it stale");
        for (std::size_t unit = 0; unit < Units; ++unit) RequireUnit(plane, unit, 0.75f, UnitTexels, "the plane after the driver's fill of 0.75");
        Require(draw(depth, {}) == PlaneTexels, "a draw at depth 0.5 failed LESS against the 0.75 the driver's fill wrote");
        before = AgcDriver::Graphics::DepthPlaneStoreCounts();
        Require(AgcDriver::Graphics::FlushDepthPlanes(base, PlaneBytes).empty(), "a read after the driver's fill and a draw that wrote no depth stored the plane");
        device->WaitIdle();
        Require(Since(before).stores == 0, "a read after the driver's fill counted a store");
        LibcRunShutdown_nid_postfix();
        std::puts("depth plane fill discard tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
