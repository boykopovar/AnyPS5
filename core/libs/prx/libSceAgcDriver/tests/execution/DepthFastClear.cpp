#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <iostream>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::HtileBytes;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

alignas(4096) std::array<float, Width * Height * 2> Depth{};
alignas(256) std::array<std::uint32_t, HtileBytes / 4u> Htile{};
alignas(16) std::array<std::uint32_t, 4> FillValue{};
constexpr std::array<std::uint32_t, 9> MemoryValueFillKernel{0xd7460000u, 0x04010c08u, 0xf4201a82u, 0xfa000000u, 0xbf8cc07fu, 0x7e02026au, 0xe0102000u, 0x80000100u, 0xbf810000u};

void FillFromMemory(std::uint64_t htile, std::uint32_t groups, std::uint32_t value) {
    FillValue[0] = value;
    const auto valueAddress = reinterpret_cast<std::uintptr_t>(FillValue.data());
    const std::array<std::uint32_t, 8> userData{static_cast<std::uint32_t>(htile), static_cast<std::uint32_t>((htile >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(DepthFastClearHarness::HtileBytes / 4u), 0x00014204u, static_cast<std::uint32_t>(valueAddress), static_cast<std::uint32_t>(valueAddress >> 32u) | (16u << 16u), 1u, 0x0004dfacu};
    const auto fill = AgcDriver::Graphics::MatchMemoryValueFill(MemoryValueFillKernel, userData, {64u, 1u, 1u}, {groups, 1u, 1u});
    DepthFastClearHarness::Require(fill.has_value(), "the HTILE fill from a value in memory was not recognized");
    AgcDriver::Graphics::NoteMemoryValueFill(*fill);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Htile.fill(0xffffffffu);
        const auto shaders = Compile(*device);
        const Surface surface{reinterpret_cast<std::uintptr_t>(Depth.data()), 0, reinterpret_cast<std::uintptr_t>(Htile.data()), false, VK_FORMAT_D32_SFLOAT};
        const auto htile = surface.htile;
        Require(Draw(*device, shaders, surface, {.depthWrite = true}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
        Require(Draw(*device, shaders, surface, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
        AgcDriver::Graphics::NoteDepthMetadataFill(htile, HtileBytes - 4u, 0u);
        Require(Draw(*device, shaders, surface, {}) == 0u, "a fill shorter than the HTILE cleared the depth");
        AgcDriver::Graphics::NoteDepthMetadataFill(htile, HtileBytes, 0xffffffffu);
        Require(Draw(*device, shaders, surface, {}) == 0u, "an expanded HTILE fill (ZMask 0xf) cleared the depth");
        AgcDriver::Graphics::NoteDepthMetadataFill(htile, HtileBytes, 0u);
        Require(Draw(*device, shaders, surface, {.depthWrite = true}) == Covered, "a fill of ZMask 0 over the whole HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        Require(Draw(*device, shaders, surface, {}) == 0u, "the depth written after a fast clear was lost");
        AgcDriver::Graphics::NoteDepthMetadataFill(htile, HtileBytes, 0u);
        Require(Draw(*device, shaders, surface, {}) == Covered, "a second whole-HTILE fill of ZMask 0 did not clear the depth again");
        constexpr auto wholeHtileGroups = static_cast<std::uint32_t>(HtileBytes / 4u / 64u);
        Require(Draw(*device, shaders, surface, {.depthWrite = true}) == Covered, "the depth was not written before the memory value fills");
        FillFromMemory(htile, wholeHtileGroups, 0xffffffffu);
        Require(Draw(*device, shaders, surface, {}) == 0u, "a memory value fill of ZMask 0xf cleared the depth");
        FillFromMemory(htile, wholeHtileGroups - 1u, 0u);
        Require(Draw(*device, shaders, surface, {}) == 0u, "a memory value fill shorter than the HTILE cleared the depth");
        FillFromMemory(htile, wholeHtileGroups, 0u);
        Require(Draw(*device, shaders, surface, {}) == Covered, "a memory value fill of ZMask 0 over the whole HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        std::puts("depth fast clear tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
