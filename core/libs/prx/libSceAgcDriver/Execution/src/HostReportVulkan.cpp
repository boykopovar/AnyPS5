#include "prx/libSceAgcDriver/Execution/include/HostReportVulkan.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace AgcDriver {

namespace {

template <typename TFunction>
TFunction load(const HostGpuQuery& query, const char* name) {
    const auto function = reinterpret_cast<TFunction>(reinterpret_cast<void*>(query.instanceProc(query.instance, name)));
    if (function == nullptr) throw std::runtime_error(std::string(name) + " is not available");
    return function;
}

const char* deviceType(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: return "other";
    }
}

const char* presentModeName(VkPresentModeKHR mode) {
    switch (mode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "immediate";
        case VK_PRESENT_MODE_MAILBOX_KHR: return "mailbox";
        case VK_PRESENT_MODE_FIFO_KHR: return "fifo";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "fifo_relaxed";
        case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR: return "fifo_latest_ready";
        default: return "other";
    }
}

}

HostGpuFacts CollectHostGpuFacts(const HostGpuQuery& query) {
    const auto has = [&](const char* name) {
        return std::any_of(query.extensions.begin(), query.extensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; });
    };
    HostGpuFacts gpu;
    gpu.candidates = query.candidates;
    gpu.subgroupSize = query.subgroupSize;
    gpu.requestedPresentMode = query.requestedPresentMode != nullptr ? query.requestedPresentMode : "";
    const auto properties2 = load<PFN_vkGetPhysicalDeviceProperties2>(query, "vkGetPhysicalDeviceProperties2");
    VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceFloatControlsProperties floatControls{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    const bool hasDriver = has(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
    const bool hasFloatControls = has(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    void** next = &properties.pNext;
    if (hasDriver) {
        *next = &driver;
        next = &driver.pNext;
    }
    if (hasFloatControls) *next = &floatControls;
    properties2(query.physical, &properties);
    gpu.name = properties.properties.deviceName;
    gpu.type = deviceType(properties.properties.deviceType);
    gpu.vendorId = properties.properties.vendorID;
    gpu.deviceId = properties.properties.deviceID;
    gpu.apiVersion = properties.properties.apiVersion;
    gpu.driverVersion = properties.properties.driverVersion;
    if (hasDriver) {
        gpu.driverName = driver.driverName;
        gpu.driverInfo = driver.driverInfo;
    }
    VkPhysicalDeviceMemoryProperties memory{};
    load<PFN_vkGetPhysicalDeviceMemoryProperties>(query, "vkGetPhysicalDeviceMemoryProperties")(query.physical, &memory);
    std::uint64_t largest = 0;
    for (std::uint32_t index = 0; index < memory.memoryHeapCount && index < VK_MAX_MEMORY_HEAPS; ++index)
        if ((memory.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) largest = std::max<std::uint64_t>(largest, memory.memoryHeaps[index].size);
    gpu.deviceLocalMiB = largest / (1024u * 1024u);

    VkPhysicalDevice8BitStorageFeatures bytes{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES};
    VkPhysicalDeviceBufferDeviceAddressFeatures address{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    const bool hasBytes = has(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
    const bool hasAddress = has(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    void** featureNext = &features.pNext;
    if (hasBytes) {
        *featureNext = &bytes;
        featureNext = &bytes.pNext;
    }
    if (hasAddress) *featureNext = &address;
    load<PFN_vkGetPhysicalDeviceFeatures2>(query, "vkGetPhysicalDeviceFeatures2")(query.physical, &features);
    const auto& core = features.features;
    const auto required = [&](const char* name, const char* kind, bool present) { gpu.requirements.push_back({name, kind, true, present}); };
    const auto optional = [&](const char* name) { gpu.requirements.push_back({name, "extension", false, has(name)}); };
    if (query.presentation) required(VK_KHR_SWAPCHAIN_EXTENSION_NAME, "extension", has(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
    required(VK_KHR_8BIT_STORAGE_EXTENSION_NAME, "extension", hasBytes);
    required(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, "extension", hasAddress);
    required(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, "extension", hasFloatControls);
    required("storageBuffer8BitAccess", "feature", hasBytes && bytes.storageBuffer8BitAccess == VK_TRUE);
    required("bufferDeviceAddress", "feature", hasAddress && address.bufferDeviceAddress == VK_TRUE);
    required("shaderInt64", "feature", core.shaderInt64 == VK_TRUE);
    required("shaderSignedZeroInfNanPreserveFloat32", "feature", hasFloatControls && floatControls.shaderSignedZeroInfNanPreserveFloat32 == VK_TRUE);
    required("vertexPipelineStoresAndAtomics", "feature", core.vertexPipelineStoresAndAtomics == VK_TRUE);
    required("fragmentStoresAndAtomics", "feature", core.fragmentStoresAndAtomics == VK_TRUE);
    required("samplerAnisotropy", "feature", core.samplerAnisotropy == VK_TRUE);
    required("textureCompressionBC", "feature", core.textureCompressionBC == VK_TRUE);
    for (const char* name : {VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME,
             VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, VK_KHR_SHADER_TERMINATE_INVOCATION_EXTENSION_NAME,
             VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME, VK_KHR_SHADER_CLOCK_EXTENSION_NAME, VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
             VK_KHR_MAINTENANCE_5_EXTENSION_NAME, VK_EXT_HDR_METADATA_EXTENSION_NAME, VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
             VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME, VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
             VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME, VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME, VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME,
             VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME, "VK_EXT_present_mode_fifo_latest_ready"})
        optional(name);

    if (query.surface != VK_NULL_HANDLE) {
        const auto modes = load<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(query, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        std::uint32_t count = 0;
        if (modes(query.physical, query.surface, &count, nullptr) == VK_SUCCESS) {
            std::vector<VkPresentModeKHR> available(count);
            if (modes(query.physical, query.surface, &count, available.data()) == VK_SUCCESS)
                for (const auto mode : available) gpu.presentModes.push_back(presentModeName(mode));
        }
    }
    return gpu;
}

}
