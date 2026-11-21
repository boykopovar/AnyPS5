#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBGROUPBARRIER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBGROUPBARRIER_HPP

#include <vulkan/vulkan.h>
#include <cstdint>

namespace AgcDriver {

inline constexpr std::uint32_t NvidiaSubgroupControlBarrierDriverVersion = 596u << 22u;

inline bool SubgroupControlBarrierUsable(VkDriverId driver, std::uint32_t driverVersion) {
    if (driver == VK_DRIVER_ID_NVIDIA_PROPRIETARY) return driverVersion >= NvidiaSubgroupControlBarrierDriverVersion;
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

}

#endif
