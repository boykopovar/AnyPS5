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

alignas(4096) std::array<float, Width * Height * 2> Depth{};
alignas(4096) std::array<float, Width * Height> StencilDepth{};
alignas(4096) std::array<std::uint8_t, Width * Height> Stencil{};
alignas(256) std::array<std::uint32_t, 1024> Htile{};

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
        std::puts("depth HTILE write tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
