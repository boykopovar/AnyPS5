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
using DepthFastClearHarness::DrawOptions;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::HtileBytes;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

constexpr std::size_t DepthPlaneBytes = 128u * 128u * 4u;
constexpr std::size_t StencilPlaneBytes = 256u * 256u;

alignas(4096) std::array<std::byte, DepthPlaneBytes> Depth{};
alignas(4096) std::array<std::byte, StencilPlaneBytes> Stencil{};
alignas(4096) std::array<std::byte, DepthPlaneBytes> HtileDepth{};
alignas(4096) std::array<std::byte, DepthPlaneBytes> AliasDepth{};
alignas(4096) std::array<std::byte, StencilPlaneBytes> AliasStencil{};
alignas(256) std::array<std::uint32_t, HtileBytes / 4u> Htile{};

VkStencilOpState StencilState(VkCompareOp compare, VkStencilOp pass, std::uint32_t reference) {
    VkStencilOpState state{};
    state.failOp = VK_STENCIL_OP_KEEP;
    state.passOp = pass;
    state.depthFailOp = VK_STENCIL_OP_KEEP;
    state.compareOp = compare;
    state.compareMask = 0xffu;
    state.writeMask = 0xffu;
    state.reference = reference;
    return state;
}

DrawOptions StencilEquals(std::uint32_t reference) {
    return {.depthTest = false, .stencilTest = true, .stencil = StencilState(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, reference)};
}

DrawOptions StencilWrite(std::uint32_t reference) {
    return {.depthTest = false, .stencilTest = true, .stencil = StencilState(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, reference)};
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, 4u) == DepthPlaneBytes, "the depth plane size of the test surface changed");
        Require(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, 1u) == StencilPlaneBytes, "the stencil plane size of the test surface changed");
        const auto shaders = Compile(*device);
        const Surface surface{reinterpret_cast<std::uintptr_t>(Depth.data()), reinterpret_cast<std::uintptr_t>(Stencil.data()), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT};
        const auto depth = surface.depth;
        const auto stencil = surface.stencil;

        Require(Draw(*device, shaders, surface, StencilWrite(2u)) == Covered, "the stencil mark draw did not cover the target");
        Require(Draw(*device, shaders, surface, StencilEquals(2u)) == Covered, "the stencil mark was not kept");
        AgcDriver::Graphics::NoteDepthMetadataFill(stencil, StencilPlaneBytes - 4u, 0x01010101u);
        Require(Draw(*device, shaders, surface, StencilEquals(2u)) == Covered, "a fill shorter than the stencil plane changed the stencil");
        AgcDriver::Graphics::NoteDepthMetadataFill(stencil, StencilPlaneBytes, 0x01020304u);
        Require(Draw(*device, shaders, surface, StencilEquals(2u)) == Covered, "a fill whose bytes differ changed the stencil");
        AgcDriver::Graphics::NoteDepthMetadataFill(stencil, StencilPlaneBytes, 0x01010101u);
        Require(Draw(*device, shaders, surface, StencilEquals(2u)) == 0u, "a fill of the whole stencil plane with 1 left the stencil mark 2");
        Require(Draw(*device, shaders, surface, StencilEquals(1u)) == Covered, "a fill of the whole stencil plane with 1 did not clear the stencil to 1");
        Require(Draw(*device, shaders, surface, StencilWrite(2u)) == Covered, "the stencil could not be written after the fill");
        AgcDriver::Graphics::NoteDepthMetadataFill(stencil - 4096u, StencilPlaneBytes + 8192u, 0u);
        Require(Draw(*device, shaders, surface, StencilEquals(0u)) == Covered, "a fill around the whole stencil plane with 0 did not clear the stencil to 0");

        Require(Draw(*device, shaders, surface, {.depthWrite = true}) == Covered, "the depth was not written at 0.5");
        Require(Draw(*device, shaders, surface, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
        AgcDriver::Graphics::NoteDepthMetadataFill(depth, DepthPlaneBytes, 0x3f800000u);
        Require(Draw(*device, shaders, surface, {}) == Covered, "a fill of the whole depth plane with 1.0 did not clear the depth to 1.0");
        AgcDriver::Graphics::NoteDepthMetadataFill(depth, DepthPlaneBytes, 0u);
        Require(Draw(*device, shaders, surface, {}) == 0u, "a fill of the whole depth plane with 0.0 did not clear the depth to 0.0");
        AgcDriver::Graphics::NoteDepthMetadataFill(depth, DepthPlaneBytes, 0xff000000u);
        Require(Draw(*device, shaders, surface, {}) == 0u, "a fill of the depth plane with a value outside 0 to 1 changed the depth");
        AgcDriver::Graphics::NoteDepthMetadataFill(depth, DepthPlaneBytes - 4u, 0x3f800000u);
        Require(Draw(*device, shaders, surface, {}) == 0u, "a fill shorter than the depth plane changed the depth");

        Htile.fill(0xffffffffu);
        const Surface htiled{reinterpret_cast<std::uintptr_t>(HtileDepth.data()), 0, reinterpret_cast<std::uintptr_t>(Htile.data()), false, VK_FORMAT_D32_SFLOAT};
        Require(Draw(*device, shaders, htiled, {.depthWrite = true}) == Covered, "the depth of the surface with HTILE was not written at 0.5");
        AgcDriver::Graphics::NoteDepthMetadataFill(htiled.depth, DepthPlaneBytes, 0u);
        AgcDriver::Graphics::NoteDepthMetadataFill(htiled.htile, HtileBytes, 0u);
        Require(Draw(*device, shaders, htiled, {.depthWrite = true}) == Covered, "an HTILE fill after a depth plane fill of 0.0 did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        AgcDriver::Graphics::NoteDepthMetadataFill(htiled.depth, DepthPlaneBytes, 0u);
        AgcDriver::Graphics::NoteHtileDepthClear(htiled.htile, HtileBytes);
        Require(Draw(*device, shaders, htiled, {}) == Covered, "an HTILE clear kernel after a depth plane fill of 0.0 did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        Htile.fill(0xfffff3ffu);
        AgcDriver::Graphics::NoteDepthMetadataFill(stencil, StencilPlaneBytes, 0x07070707u);
        const Surface stencilAlias{reinterpret_cast<std::uintptr_t>(AliasDepth.data()), stencil, htiled.htile, false, VK_FORMAT_D32_SFLOAT_S8_UINT};
        Require(Draw(*device, shaders, stencilAlias, StencilEquals(7u)) == Covered, "a new attachment lost its inherited pending stencil fill");

        AgcDriver::Graphics::NoteDepthMetadataFill(stencilAlias.depth, DepthPlaneBytes, 0u);
        const Surface depthAlias{stencilAlias.depth, reinterpret_cast<std::uintptr_t>(AliasStencil.data()), htiled.htile, false, VK_FORMAT_D32_SFLOAT_S8_UINT};
        Require(Draw(*device, shaders, depthAlias, {}) == 0u, "a new attachment lost its inherited pending depth fill");
        std::puts("depth plane fill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
