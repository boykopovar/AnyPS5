#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_CHANGEDCOPYBACKS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_CHANGEDCOPYBACKS_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool ChangedCopyBacksUsable(VkDriverId driver, bool windows) {
    return !windows || driver != VK_DRIVER_ID_NVIDIA_PROPRIETARY;
}

}

#endif
