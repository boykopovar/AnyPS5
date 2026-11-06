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

inline int SetTestEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    return _putenv_s(name, value);
#else
    return setenv(name, value, 1);
#endif
}

inline bool SkipWithoutDescriptorHeap() {
    if (AgcDriver::Graphics::DescriptorHeapEnabled()) return false;
    std::puts("skipped, the device lacks VK_EXT_descriptor_heap, or the maintenance5, slot room and push data the GPU heap needs with it, so the GPU heap has nothing to run through");
    return true;
}

inline std::unique_ptr<AgcDriver::VulkanDevice> OpenVulkanTestDevice() {
    try {
        auto device = std::make_unique<AgcDriver::VulkanDevice>();
        if (std::getenv("APS5_VERIFY_GPU_HEAP") != nullptr && SkipWithoutDescriptorHeap()) return nullptr;
        return device;
    } catch (const AgcDriver::VulkanUnavailable& error) {
        if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
        std::printf("skipped, no usable Vulkan device: %s\n", error.what());
        return nullptr;
    }
}

inline std::uint32_t PixelPushOffset(std::uint32_t vertexPush, const ShaderRecompiler::SpirvTarget& target) {
    return AgcDriver::Graphics::StagePushOffset(vertexPush, ShaderRecompiler::ShaderStage::Fragment, target.fixedPushSlots);
}

inline ShaderRecompiler::BindingLayout PixelPushLayout(std::uint32_t vertexPush, const ShaderRecompiler::SpirvTarget& target) {
    const auto offset = PixelPushOffset(vertexPush, target);
    return {0, 0, offset, AgcDriver::Graphics::PipelinePushSlotBytes - offset % AgcDriver::Graphics::PipelinePushSlotBytes};
}

#endif
