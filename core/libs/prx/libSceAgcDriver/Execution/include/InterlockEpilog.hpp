#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_INTERLOCKEPILOG_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_INTERLOCKEPILOG_HPP

#include <cstdint>
#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool OrderedInterlockEpilogs(VkDriverId driver, std::uint32_t driverVersion) {
    return driver == VK_DRIVER_ID_MESA_RADV && driverVersion >= VK_MAKE_API_VERSION(0, 26, 3, 1);
}

}

#endif
