#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_VULKANTESTDEVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_VULKANTESTDEVICE_HPP

#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>

constexpr int VulkanTestSkipped = 77;

inline bool TargetHasCapability(const ShaderRecompiler::SpirvTarget& target, spv::Capability capability) {
    const auto& capabilities = target.supportedCapabilities;
    return std::find(capabilities.begin(), capabilities.end(), static_cast<std::uint32_t>(capability)) != capabilities.end();
}

inline std::unique_ptr<AgcDriver::VulkanDevice> OpenVulkanTestDevice() {
    try {
        return std::make_unique<AgcDriver::VulkanDevice>();
    } catch (const std::exception& error) {
        if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
        std::printf("skipped, no usable Vulkan device: %s\n", error.what());
        return nullptr;
    }
}

#endif
