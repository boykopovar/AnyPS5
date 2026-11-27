#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::DepthBiasUnits;
using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Surface;

alignas(4096) std::array<float, DepthFastClearHarness::Width * DepthFastClearHarness::Height * 2> Depth{};

constexpr float Units = 64.0f;
constexpr float Offset = 1.0f / 1024.0f;

bool Stored(AgcDriver::VulkanDevice& device, const Shaders& shaders, const Surface& surface, float depth) {
    return Draw(device, shaders, surface, {.depthTest = true, .depthWrite = false, .depthCompare = VK_COMPARE_OP_EQUAL, .minDepth = depth, .maxDepth = depth}) == Covered;
}

void Check(AgcDriver::VulkanDevice& device, const Shaders& shaders, const Surface& surface, float depth) {
    const auto what = "64 units of 2^-16 on a D32 target at depth " + std::to_string(depth);
    Require(Draw(device, shaders, surface, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .minDepth = depth, .maxDepth = depth, .depthBias = true, .depthBiasConstant = Units, .depthBiasUnits = DepthBiasUnits::Unorm16}) == Covered, what + ": the biased draw did not cover the target");
    if (Stored(device, shaders, surface, depth + Offset)) return;
    for (int exponent = -24; exponent <= -4; ++exponent) {
        const float found = depth + 64.0f * std::ldexp(1.0f, exponent);
        if (Stored(device, shaders, surface, found)) throw std::runtime_error(what + " stored depth + 64 * 2^" + std::to_string(exponent) + ", expected depth + 64 * 2^-16");
    }
    throw std::runtime_error(what + " did not store depth + 64 * 2^-16 or any power-of-two unit near it");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const bool withoutControl = std::getenv("APS5_NO_DEPTH_BIAS_CONTROL") != nullptr;
        Require(!withoutControl || !context.depthBiasControl, "APS5_NO_DEPTH_BIAS_CONTROL left VK_EXT_depth_bias_control enabled");
        Require(std::getenv("APS5_NO_DEPTH_BIAS_FORCE_UNORM") == nullptr || !context.depthBiasForceUnorm, "APS5_NO_DEPTH_BIAS_FORCE_UNORM left the force-unorm representation in use");
        const auto shaders = Compile(*device);
        const Surface surface{reinterpret_cast<std::uintptr_t>(Depth.data()), 0, 0, false, VK_FORMAT_D32_SFLOAT};
        Check(*device, shaders, surface, 0.75f);
        if (withoutControl) {
            std::puts("depth bias units tests passed without VK_EXT_depth_bias_control, for a largest depth in [0.5, 1) only");
            return 0;
        }
        if (!context.depthBiasForceUnorm && !context.depthBiasFloat) {
            std::puts("skipped, the device has no exact depth bias representation for 16-bit units on a float depth target (VK_EXT_depth_bias_control force-unorm with depthBiasExact, or float)");
            return VulkanTestSkipped;
        }
        Check(*device, shaders, surface, 0.125f);
        Check(*device, shaders, surface, 0.0078125f);
        std::printf("depth bias units tests passed through the %s representation\n", context.depthBiasForceUnorm ? "force-unorm" : "float");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
