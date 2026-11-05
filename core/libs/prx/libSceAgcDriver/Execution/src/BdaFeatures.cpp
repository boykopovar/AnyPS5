#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include <algorithm>
#include <cstring>

namespace AgcDriver {

VkPhysicalDevice8BitStorageFeatures QueryBdaByteFeatures(VkPhysicalDevice physical, PFN_vkGetPhysicalDeviceFeatures2 query, std::span<const VkExtensionProperties> extensions) {
    Graphics::Require(query != nullptr, "missing Vulkan feature query");
    Graphics::Require(std::any_of(extensions.begin(), extensions.end(), [](const auto& extension) { return std::strcmp(extension.extensionName, VK_KHR_8BIT_STORAGE_EXTENSION_NAME) == 0; }), "VK_KHR_8bit_storage is unavailable");
    VkPhysicalDevice8BitStorageFeatures bytes{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &bytes};
    query(physical, &features);
    Graphics::Require(bytes.storageBuffer8BitAccess == VK_TRUE, "BDA requires exact byte access through storageBuffer8BitAccess");
    return {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES, nullptr, VK_TRUE, VK_FALSE, VK_FALSE};
}

VkPhysicalDeviceBufferDeviceAddressFeatures QueryBdaFeatures(VkPhysicalDevice physical, PFN_vkGetPhysicalDeviceFeatures2 query, std::span<const VkExtensionProperties> extensions) {
    Graphics::Require(query != nullptr, "missing Vulkan feature query");
    Graphics::Require(std::any_of(extensions.begin(), extensions.end(), [](const auto& extension) { return std::strcmp(extension.extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0; }), "VK_KHR_buffer_device_address is unavailable");
    VkPhysicalDeviceBufferDeviceAddressFeatures address{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &address};
    query(physical, &features);
    Graphics::Require(address.bufferDeviceAddress == VK_TRUE && features.features.shaderInt64 == VK_TRUE, "buffer device address and shaderInt64 are required");
    return {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, nullptr, VK_TRUE, VK_FALSE, VK_FALSE};
}

}
