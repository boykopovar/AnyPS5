#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRAWINPUTSINPLACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRAWINPUTSINPLACE_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool DrawInputsInPlaceUsable(VkDriverId driver, bool windows) {
    return !windows || driver != VK_DRIVER_ID_NVIDIA_PROPRIETARY;
}

}

#endif
