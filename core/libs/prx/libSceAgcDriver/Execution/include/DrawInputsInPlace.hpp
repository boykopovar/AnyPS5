#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRAWINPUTSINPLACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRAWINPUTSINPLACE_HPP

#include <cstddef>
#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool DrawInputsInPlaceUsable(VkDriverId driver, bool windows) {
    return !windows || driver != VK_DRIVER_ID_NVIDIA_PROPRIETARY;
}

inline std::size_t DrawInputsInPlaceBelow(VkDriverId driver, bool windows) {
    return DrawInputsInPlaceUsable(driver, windows) ? 0 : 1024;
}

}

#endif
