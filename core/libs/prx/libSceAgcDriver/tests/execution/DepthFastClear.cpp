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
alignas(256) std::array<std::uint32_t, 1024> Htile{};

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
        std::puts("depth fast clear tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
