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

        Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, 0x2au)}) == PlaneTexels, "the draw writing stencil 0x2a did not cover the plane");
        AgcDriver::Graphics::RetireDepthSurfaces(device->graphicsContext().device, reinterpret_cast<std::uintptr_t>(depthPlane), DepthBytes);
        device->WaitIdle();
        Require(Holding(stencilPlane, 0x2au) == StencilBytes, "retiring the surface for a write over its depth plane only dropped the stencil 0x2a a draw wrote");
        Require(AgcDriver::Graphics::FlushDepthPlanes(stencilBase, StencilBytes).empty(), "a read of the stencil plane stored after its surface retired stored it again");

        if (!AgcDriver::GuestMemory::WriteWatched() || !AgcDriver::GuestMemory::Watched(stencilBase, StencilBytes)) {
            std::puts("guest memory is not write-watched here: a guest write into the stencil plane is not exercised");
        } else {
            Fill(stencilPlane, 0x05u);
            Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, 0x05u)}) == PlaneTexels, "a draw testing stencil EQUAL 0x05 after the guest wrote 0x05 into the stencil plane did not pass everywhere");
            Require(draw({.depthTest = false, .stencilTest = true, .stencil = Stencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, 0x18u)}) == 0u, "a draw testing stencil EQUAL 0x18 passed after the guest wrote 0x05 over the stored stencil plane");
        }
        std::puts("stencil plane memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
