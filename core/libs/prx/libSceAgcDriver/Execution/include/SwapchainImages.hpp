#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SWAPCHAINIMAGES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SWAPCHAINIMAGES_HPP

#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdint>

namespace AgcDriver {

inline std::uint32_t SwapchainImageCount(const VkSurfaceCapabilitiesKHR& surface) {
    const std::uint32_t count = std::max(surface.minImageCount, 3u);
    return surface.maxImageCount != 0 ? std::min(count, surface.maxImageCount) : count;
}

}

#endif
