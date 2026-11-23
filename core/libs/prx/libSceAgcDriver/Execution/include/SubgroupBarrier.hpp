#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBGROUPBARRIER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBGROUPBARRIER_HPP

#include <vulkan/vulkan.h>
#include <cstdint>

namespace AgcDriver {

inline bool SubgroupControlBarrierUsable(VkDriverId driver) {
    return driver != VK_DRIVER_ID_AMD_PROPRIETARY && driver != VK_DRIVER_ID_MOLTENVK;
}

inline bool WorkgroupScopeFenceUsable(VkDriverId driver) {
    return driver != VK_DRIVER_ID_AMD_PROPRIETARY;
}

inline bool LdsLockFencesRequired(VkDriverId driver) {
    return driver == VK_DRIVER_ID_MOLTENVK;
}

inline bool DeviceAtomicWorkgroupMemoryBarriersRequired(VkDriverId driver) {
    return driver == VK_DRIVER_ID_AMD_PROPRIETARY;
}

inline bool MeshSoftwareControlBarriersRequired(VkDriverId driver, std::uint32_t driverVersion) {
    return driver == VK_DRIVER_ID_AMD_PROPRIETARY && driverVersion == VK_MAKE_API_VERSION(0, 2, 0, 331);
}

}

#endif
