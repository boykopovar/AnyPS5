#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBGROUPBARRIER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBGROUPBARRIER_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool SubgroupControlBarrierUsable(VkDriverId driver) {
    return driver != VK_DRIVER_ID_AMD_PROPRIETARY;
}

inline bool WorkgroupScopeFenceUsable(VkDriverId driver) {
    return driver != VK_DRIVER_ID_AMD_PROPRIETARY;
}

}

#endif
