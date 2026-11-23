#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GRAPHICSPIPELINELIBRARY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GRAPHICSPIPELINELIBRARY_HPP

#include <cstdint>
#include <vulkan/vulkan.h>

namespace AgcDriver {

inline constexpr std::uint32_t IntelUhdA78bDeviceId = 0xa78bu;
inline constexpr std::uint32_t IntelUhdA78bVulkanDriverVersion_101_5081 = 1659865u;

inline bool GraphicsPipelineLibraryUsable(VkDriverId driver, std::uint32_t deviceId, std::uint32_t driverVersion) {
    return driver != VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS || deviceId != IntelUhdA78bDeviceId || driverVersion != IntelUhdA78bVulkanDriverVersion_101_5081;
}

}

#endif
