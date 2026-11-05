#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

using DepthFastClearHarness::Require;
constexpr VkExtent2D Extent{256u, 256u};
constexpr std::size_t Block = 65536u;
constexpr std::size_t DepthBytes = 4u * Block;
constexpr std::size_t Texels = 256u * 256u;
alignas(256) std::array<std::byte, Texels * 4u> Colors{};

struct Fixture {
    AgcDriver::VulkanDevice& device;
    DepthFastClearHarness::Shaders shaders;
    std::uintptr_t base;
    VkFormat format;

    Fixture(AgcDriver::VulkanDevice& device, VkFormat format) : device(device), shaders(DepthFastClearHarness::Compile(device)), format(format) {
        constexpr std::size_t bytes = 3u * DepthBytes + 4u * Block;
        auto* memory = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
        Require(memory != nullptr, "cannot allocate stencil ownership fixture");
        GuestArena::GuestArenaCommit_nid_postfix(memory, bytes, 0x04, bytes);
        GuestAllocations::Mutation().Add(memory, bytes, true, true, true);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, bytes);
        base = reinterpret_cast<std::uintptr_t>(memory);
        std::fill_n(static_cast<float*>(memory), 3u * DepthBytes / sizeof(float), 1.0f);
        std::memset(reinterpret_cast<void*>(Stencil()), 0, 4u * Block);
        AgcDriver::GuestMemory::BumpCollectEpoch();
    }

    std::uintptr_t Depth(unsigned index) const { return base + index * DepthBytes; }
    std::uintptr_t Stencil(unsigned index = 0u) const { return base + 3u * DepthBytes + index * Block; }

    DepthFastClearHarness::Surface Surface(unsigned depth, unsigned stencil = 0u) const {
        return {Depth(depth), Stencil(stencil), 0u, false, format, 0u, 0u, AgcDriver::Graphics::XorSwizzleMode(AgcDriver::Graphics::TextureTileMode::kZ64KBX)};
    }

    static DepthFastClearHarness::DrawOptions Options(std::uint8_t value, bool write) {
        DepthFastClearHarness::DrawOptions options;
        options.depthTest = write;
        options.depthWrite = write;
        options.depthCompare = VK_COMPARE_OP_ALWAYS;
        options.minDepth = options.maxDepth = 0.5f;
        options.stencilTest = true;
        options.stencil = {VK_STENCIL_OP_KEEP, write ? VK_STENCIL_OP_REPLACE : VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
            write ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_EQUAL, 255u, write ? 255u : 0u, value};
        return options;
    }

    std::uint32_t Draw(const DepthFastClearHarness::Surface& surface, const DepthFastClearHarness::DrawOptions& options) {
        return DepthFastClearHarness::Draw(device, shaders, surface, options, Extent, Colors);
    }

    void Write(unsigned index, std::uint8_t value, unsigned stencil = 0u) {
        Require(Draw(Surface(index, stencil), Options(value, true)) == Texels, "source stencil draw did not cover the plane");
    }

    void CheckStencil(std::uint8_t value, unsigned stencil = 0u) {
        const auto address = Stencil(stencil);
        const auto ranges = AgcDriver::Graphics::FlushDepthPlanes(address, Block);
        device.WaitIdle();
        std::vector<std::byte> bytes(Block);
        AgcDriver::GuestMemory::ReadCommitted(address, bytes);
        Require(ranges.size() == 1u, "a superseded stencil replica was also published");
        Require(std::ranges::all_of(bytes, [&](std::byte byte) { return byte == static_cast<std::byte>(value); }), "guest stencil bytes do not hold the current owner's value");
        Require(AgcDriver::Graphics::FlushDepthPlanes(address, Block).empty(), "a clean stencil plane was published twice");
    }

    void CheckDepth(unsigned index) {
        static_cast<void>(AgcDriver::Graphics::FlushDepthPlanes(Depth(index), DepthBytes));
        device.WaitIdle();
        std::array<std::uint32_t, Texels> values{};
        AgcDriver::GuestMemory::ReadCommitted(Depth(index), std::as_writable_bytes(std::span(values)));
        Require(std::ranges::all_of(values, [](std::uint32_t value) { return value == std::bit_cast<std::uint32_t>(0.5f); }), "stencil handoff discarded an independently owned depth plane");
    }
};

void SharedWrites(Fixture& fixture) {
    fixture.Write(0u, 37u);
    fixture.Write(1u, 203u);
    fixture.Write(2u, 91u);
    fixture.CheckStencil(91u);
    if (fixture.format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        for (unsigned index = 0; index != 3u; ++index) fixture.CheckDepth(index);
    }
}

void ReadOnly(Fixture& fixture) {
    fixture.Write(0u, 37u);
    fixture.Write(1u, 203u);
    Require(fixture.Draw(fixture.Surface(0u), Fixture::Options(203u, false)) == Texels, "a read-only rebind lost the newest stencil value");
    auto masked = Fixture::Options(73u, true);
    masked.depthWrite = false;
    masked.stencil.writeMask = 0u;
    Require(fixture.Draw(fixture.Surface(0u), masked) == Texels, "a masked stencil draw failed");
    fixture.CheckStencil(203u);
    fixture.CheckDepth(0u);
    fixture.CheckDepth(1u);
}

void CpuStore(Fixture& fixture, bool fresh) {
    fixture.Write(0u, 37u);
    fixture.Write(1u, 203u);
    std::memset(reinterpret_cast<void*>(fixture.Stencil()), 5, Block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto destination = fixture.Surface(fresh ? 2u : 0u);
    Require(fixture.Draw(destination, Fixture::Options(5u, false)) == Texels, "the new stencil owner did not consume the CPU store");
    Require(fixture.Draw(destination, Fixture::Options(203u, false)) == 0u, "the old GPU stencil value survived the CPU store");
    fixture.CheckStencil(5u);
    fixture.CheckDepth(0u);
    fixture.CheckDepth(1u);
}

void PendingClear(Fixture& fixture) {
    auto surface = fixture.Surface(0u);
    surface.htile = fixture.Stencil(3u);
    surface.htileStencil = true;
    surface.clearStencil = 90u;
    const auto bytes = AgcDriver::Graphics::HtileMetadataBytes(Extent);
    Require(bytes <= Block, "HTILE does not fit its fixture allocation");
    std::memset(reinterpret_cast<void*>(surface.htile), 255, bytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(fixture.Draw(surface, Fixture::Options(37u, true)) == Texels, "stencil clear source draw failed");
    std::memset(reinterpret_cast<void*>(surface.htile), 0, bytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    AgcDriver::Graphics::NoteDepthMetadataFill(surface.htile, bytes, 0u);
    auto target = fixture.Surface(1u);
    target.clearStencil = 165u;
    Require(fixture.Draw(target, Fixture::Options(90u, false)) == Texels, "a new stencil owner replaced the pending clear with its clear-register value");
    fixture.CheckStencil(90u);
}

void Independent(Fixture& fixture) {
    constexpr std::array<std::uint8_t, 3> values{37u, 203u, 91u};
    for (unsigned index = 0; index != values.size(); ++index) fixture.Write(index, values[index], index);
    for (unsigned index = 0; index != values.size(); ++index) {
        fixture.CheckStencil(values[index], index);
        fixture.CheckDepth(index);
    }
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc == 1 ? "write" : argc == 2 ? argv[1] : "invalid";
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Fixture fixture(*device, mode == "d16" ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT);
        if (mode == "write" || mode == "d16") SharedWrites(fixture);
        else if (mode == "readonly") ReadOnly(fixture);
        else if (mode == "cpu" || mode == "cpu_fresh") {
            if (!AgcDriver::GuestMemory::WriteWatched() || !AgcDriver::GuestMemory::Watched(fixture.Stencil(), Block)) {
                std::puts("skipped, guest write watch unavailable");
                return VulkanTestSkipped;
            }
            CpuStore(fixture, mode == "cpu_fresh");
        } else if (mode == "clear") PendingClear(fixture);
        else if (mode == "independent") Independent(fixture);
        else throw std::runtime_error("unknown stencil ownership test arguments");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("stencil plane ownership tests passed");
}
