#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_HOSTREPORTVULKAN_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_HOSTREPORTVULKAN_HPP

#include "prx/libSceAgcDriver/Execution/include/HostReport.hpp"
#include <vulkan/vulkan.h>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct HostGpuQuery {
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    bool presentation = false;
    std::uint32_t subgroupSize = 0;
    std::span<const VkExtensionProperties> extensions;
    std::vector<std::string> candidates;
    const char* requestedPresentMode = nullptr;
};

HostGpuFacts CollectHostGpuFacts(const HostGpuQuery& query);

}

#endif
