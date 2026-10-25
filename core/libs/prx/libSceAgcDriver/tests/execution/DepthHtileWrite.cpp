#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::DrawOptions;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::HtileBytes;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

constexpr std::size_t Tiles = HtileBytes / 4u;
constexpr VkExtent2D InitialExtent{1024, 1024};
constexpr std::size_t InitialPixels = static_cast<std::size_t>(InitialExtent.width) * InitialExtent.height;
constexpr std::size_t InitialHtileBytes = 2u * 32768u;
constexpr std::size_t PlaneBytes = 64u * 1024u;

alignas(4096) std::array<float, InitialPixels> InitialDepth{};
alignas(4096) std::array<std::uint8_t, InitialPixels> InitialStencil{};
alignas(4096) std::array<std::byte, InitialPixels * 4u> InitialColor{};
alignas(256) std::array<std::uint32_t, InitialHtileBytes / 4u> InitialHtile{};


alignas(4096) std::array<float, PlaneBytes / sizeof(float)> Depth{};
alignas(4096) std::array<float, PlaneBytes / sizeof(float)> StencilDepth{};
alignas(4096) std::array<std::uint8_t, PlaneBytes> Stencil{};
alignas(256) std::array<std::uint32_t, HtileBytes / 4u> Htile{};

void Fill(std::uint32_t word, std::size_t count = Tiles) {
    std::fill_n(Htile.begin(), count, word);
}

bool Holds(std::uint32_t word) {
    return std::all_of(Htile.begin(), Htile.begin() + Tiles, [&](std::uint32_t value) { return value == word; });
}

DrawOptions StencilDraw(VkCompareOp compare, std::uint32_t reference, VkStencilOp op) {
    DrawOptions options;
    options.depthTest = false;
    options.stencilTest = true;
    options.stencil = {op, op, op, compare, 0xffu, op == VK_STENCIL_OP_KEEP ? 0u : 0xffu, reference};
    return options;
}

}

int main() {
    try {
        Require(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, sizeof(float)) <= sizeof(Depth) &&
            AgcDriver::Graphics::DepthSliceBytes({Width, Height}, sizeof(float)) <= sizeof(StencilDepth) &&
            AgcDriver::Graphics::DepthSliceBytes({Width, Height}, sizeof(std::uint8_t)) <= sizeof(Stencil),
            "depth fixtures do not cover their tiled footprints");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = Compile(*device);

        Fill(0xfffc000fu);
        const Surface depth{reinterpret_cast<std::uintptr_t>(Depth.data()), 0, reinterpret_cast<std::uintptr_t>(Htile.data()), false, VK_FORMAT_D32_SFLOAT};
        Require(Draw(*device, shaders, depth, {.depthWrite = true}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
        Require(Draw(*device, shaders, depth, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
        Fill(0u, Tiles - 1);
        Require(Draw(*device, shaders, depth, {}) == 0u, "an HTILE with one tile left expanded cleared the depth");
        Fill(0u);
        Require(Draw(*device, shaders, depth, {.depthWrite = true}) == Covered, "an HTILE written with ZMask 0 on every tile did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        Require(Holds(0xfffc000fu), "the HTILE of the cleared depth was not stored back as expanded");
        Require(Draw(*device, shaders, depth, {}) == 0u, "the depth written after the clear was cleared again");

        Fill(0xfffff3ffu);
        const Surface stencil{reinterpret_cast<std::uintptr_t>(StencilDepth.data()), reinterpret_cast<std::uintptr_t>(Stencil.data()), reinterpret_cast<std::uintptr_t>(Htile.data()), true, VK_FORMAT_D32_SFLOAT_S8_UINT};
        Require(Draw(*device, shaders, stencil, StencilDraw(VK_COMPARE_OP_ALWAYS, 5u, VK_STENCIL_OP_REPLACE)) == Covered, "a stencil write of 5 did not pass ALWAYS");
        Require(Draw(*device, shaders, stencil, StencilDraw(VK_COMPARE_OP_EQUAL, 5u, VK_STENCIL_OP_KEEP)) == Covered, "the stencil plane does not hold the 5 written before");
        Fill(0xfffff00fu);
        Require(Draw(*device, shaders, stencil, StencilDraw(VK_COMPARE_OP_EQUAL, 0u, VK_STENCIL_OP_KEEP)) == Covered, "an HTILE written with SMem 0 on every tile did not clear the stencil to DB_STENCIL_CLEAR (0)");
        Require(Holds(0xfffff3ffu), "the HTILE of the cleared stencil was not stored back as expanded");
        Require(Draw(*device, shaders, stencil, StencilDraw(VK_COMPARE_OP_ALWAYS, 9u, VK_STENCIL_OP_REPLACE)) == Covered, "a stencil write of 9 did not pass ALWAYS");
        Require(Draw(*device, shaders, stencil, StencilDraw(VK_COMPARE_OP_EQUAL, 9u, VK_STENCIL_OP_KEEP)) == Covered, "the stencil written after the clear was cleared again");
        const Surface depthOnlyStencil{stencil.depth, stencil.stencil, stencil.htile, false, stencil.format};
        Fill(0xfffffff0u);
        Require(Draw(*device, shaders, depthOnlyStencil, StencilDraw(VK_COMPARE_OP_EQUAL, 9u, VK_STENCIL_OP_KEEP)) == Covered, "a depth-only HTILE clear changed the stencil plane");

        Fill(0xfffffff0u);
        const Surface inheritedStencil{reinterpret_cast<std::uintptr_t>(InitialDepth.data()), stencil.stencil, stencil.htile, false, stencil.format, 0, 1};
        auto inheritedStencilDraw = StencilDraw(VK_COMPARE_OP_EQUAL, 9u, VK_STENCIL_OP_KEEP);
        inheritedStencilDraw.depthTest = true;
        Require(Draw(*device, shaders, inheritedStencil, inheritedStencilDraw) == Covered, "a fresh depth-only HTILE clear lost its initial depth or inherited stencil data");

        Fill(0xfffff3ffu);
        Require(Draw(*device, shaders, stencil, {.depthWrite = true}) == Covered, "the inherited-depth fixture did not write depth 0.5");
        Fill(0u);
        const Surface inherited{stencil.depth, reinterpret_cast<std::uintptr_t>(InitialStencil.data()), stencil.htile, true, stencil.format};
        Require(Draw(*device, shaders, inherited, {}) == Covered, "a new attachment inheriting depth did not take its HTILE clear");

        Require(AgcDriver::Graphics::HtileMetadataBytes(InitialExtent) == InitialHtileBytes, "the initial-clear fixture has the wrong metadata size");
        std::fill(InitialHtile.begin(), InitialHtile.end(), 0xfffffff0u);
        const Surface initial{reinterpret_cast<std::uintptr_t>(InitialDepth.data()), reinterpret_cast<std::uintptr_t>(InitialStencil.data()), reinterpret_cast<std::uintptr_t>(InitialHtile.data()), false, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, 1};
        Require(Draw(*device, shaders, initial, StencilDraw(VK_COMPARE_OP_EQUAL, 1u, VK_STENCIL_OP_KEEP), InitialExtent, InitialColor) == InitialPixels, "a depth-only HTILE clear discarded the initial stencil clear");
        Require(std::all_of(InitialHtile.begin(), InitialHtile.end(), [](std::uint32_t word) { return word == 0xfffc000fu; }), "the initial HTILE was not stored back as expanded");
        Require(Draw(*device, shaders, initial, {.depthWrite = true}, InitialExtent, InitialColor) == InitialPixels, "the reused initial-clear fixture did not write depth 0.5");
        Require(Draw(*device, shaders, initial, StencilDraw(VK_COMPARE_OP_ALWAYS, 9u, VK_STENCIL_OP_REPLACE), InitialExtent, InitialColor) == InitialPixels, "the reused initial-clear fixture did not write stencil 9");
        std::fill(InitialHtile.begin(), InitialHtile.end(), 0xfffffff0u);
        auto reused = StencilDraw(VK_COMPARE_OP_EQUAL, 9u, VK_STENCIL_OP_KEEP);
        reused.depthTest = true;
        Require(Draw(*device, shaders, initial, reused, InitialExtent, InitialColor) == InitialPixels, "a reused attachment lost its depth-only clear or stencil plane");
        std::puts("depth HTILE write tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
