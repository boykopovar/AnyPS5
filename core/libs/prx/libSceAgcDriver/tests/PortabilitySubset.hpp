#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_PORTABILITYSUBSET_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_PORTABILITYSUBSET_HPP

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>

namespace AgcDriver::Tests {

struct PortabilitySubset {
    VkPhysicalDevicePortabilitySubsetFeaturesKHR features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR};

    void Enable(VkPhysicalDevice physical, PFN_vkEnumerateDeviceExtensionProperties enumerate, PFN_vkGetPhysicalDeviceFeatures2 queryFeatures,
        std::vector<const char*>& extensions, VkDeviceCreateInfo& device) {
        std::uint32_t count = 0;
        if (enumerate(physical, nullptr, &count, nullptr) != VK_SUCCESS) throw std::runtime_error("vkEnumerateDeviceExtensionProperties failed");
        std::vector<VkExtensionProperties> available(count);
        if (enumerate(physical, nullptr, &count, available.data()) != VK_SUCCESS) throw std::runtime_error("vkEnumerateDeviceExtensionProperties failed");
        const bool advertised = std::any_of(available.begin(), available.begin() + count,
            [](const VkExtensionProperties& item) { return std::strcmp(item.extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0; });
        if (advertised) {
            extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
            VkPhysicalDeviceFeatures2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &features};
            queryFeatures(physical, &query);
            features.pNext = const_cast<void*>(device.pNext);
            device.pNext = &features;
        }
        device.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        device.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
    }
};

}

#endif
