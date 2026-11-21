#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

namespace {

using namespace AgcDriver::Graphics;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

alignas(4096) std::array<float, Width * Height> Depth{};
alignas(4096) std::array<float, Width * Height> DepthWithStencil{};
alignas(4096) std::array<std::uint8_t, Width * Height> Stencil{};

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto compiled = DepthFastClearHarness::Compile(*device);
        const std::array<Surface, 2> surfaces{{
            {reinterpret_cast<std::uintptr_t>(Depth.data()), 0, 0, false, VK_FORMAT_D32_SFLOAT},
            {reinterpret_cast<std::uintptr_t>(DepthWithStencil.data()), reinterpret_cast<std::uintptr_t>(Stencil.data()), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT},
        }};
        for (const auto& surface : surfaces) {
            for (const bool discarded : {true, false, true, false}) {
                const auto covered = DepthFastClearHarness::Draw(*device, compiled, surface, {.depthCompare = VK_COMPARE_OP_ALWAYS, .rasterizerDiscard = discarded});
                Require(covered == (discarded ? 0u : Covered), std::string("AGC graphics: a draw ") + (discarded ? "with" : "without") + " rasterizer discard covered " + std::to_string(covered) + " of " + std::to_string(Covered) + " pixels");
            }
            const auto stenciled = DepthFastClearHarness::Draw(*device, compiled, surface, {.depthCompare = VK_COMPARE_OP_ALWAYS, .stencilTest = true, .stencil = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 1u}, .rasterizerDiscard = true});
            Require(stenciled == 0, "AGC graphics: a discarded draw with a stencil test covered " + std::to_string(stenciled) + " pixels");
        }
        std::puts("rasterizer discard tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
