#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_BDAFEATURES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_BDAFEATURES_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <span>

namespace AgcDriver {

VkPhysicalDeviceBufferDeviceAddressFeatures QueryBdaFeatures(VkPhysicalDevice physical, PFN_vkGetPhysicalDeviceFeatures2 query, std::span<const VkExtensionProperties> extensions);
VkPhysicalDevice8BitStorageFeatures QueryBdaByteFeatures(VkPhysicalDevice physical, PFN_vkGetPhysicalDeviceFeatures2 query, std::span<const VkExtensionProperties> extensions);

}

#endif
