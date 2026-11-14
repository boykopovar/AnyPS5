#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_INPUTTWINS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_INPUTTWINS_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool InputTwinsUsable(VkDriverId driver, bool windows) {
    return windows && driver == VK_DRIVER_ID_NVIDIA_PROPRIETARY;
}

}

#endif
