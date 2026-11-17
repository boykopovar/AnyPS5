#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRAWINPUTSINPLACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRAWINPUTSINPLACE_HPP

#include <cstddef>
#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool DrawInputsInPlaceUsable(VkDriverId driver, bool windows, VkPhysicalDeviceType type) {
    if (!windows) return true;
    if (driver == VK_DRIVER_ID_NVIDIA_PROPRIETARY) return false;
    return driver != VK_DRIVER_ID_AMD_PROPRIETARY || type != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

inline std::size_t DrawInputsInPlaceBelow(VkDriverId driver, bool windows, VkPhysicalDeviceType type) {
    return DrawInputsInPlaceUsable(driver, windows, type) ? 0 : 1024;
}

}

#endif
