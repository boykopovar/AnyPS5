#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBGROUPCLOCK_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBGROUPCLOCK_HPP

#include <vulkan/vulkan.h>
#include <string_view>

namespace AgcDriver {

bool NarrowSubgroupClock(VkDriverId driver, std::string_view deviceName);

}

#endif
