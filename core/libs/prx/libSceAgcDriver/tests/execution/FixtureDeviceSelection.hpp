#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_FIXTUREDEVICESELECTION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_FIXTUREDEVICESELECTION_HPP

#include <vulkan/vulkan.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

inline std::optional<std::size_t> SelectFixtureDevice(std::span<const VkPhysicalDeviceProperties> devices, std::string_view request) {
    const auto rankDeviceType = [](VkPhysicalDeviceType type) {
        switch (type) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
            default: return 0;
        }
    };
    std::optional<std::size_t> selected;
    int selectedRank = -1;
    for (std::size_t index = 0; index < devices.size(); ++index) {
        const auto& candidate = devices[index];
        if (candidate.apiVersion < VK_API_VERSION_1_1) continue;
        const std::string_view name(candidate.deviceName);
        if (!request.empty() && std::search(name.begin(), name.end(), request.begin(), request.end(), [](char left, char right) {
            return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
        }) == name.end()) continue;
        const int rank = request.empty() ? rankDeviceType(candidate.deviceType) : 4;
        if (rank <= selectedRank) continue;
        selected = index;
        selectedRank = rank;
    }
    return selected;
}

inline VkPhysicalDevice SelectFixturePhysicalDevice(std::span<const VkPhysicalDevice> devices, PFN_vkGetPhysicalDeviceProperties physicalProperties) {
    std::vector<VkPhysicalDeviceProperties> properties(devices.size());
    for (std::size_t index = 0; index < devices.size(); ++index) physicalProperties(devices[index], &properties[index]);
    const char* requestedName = std::getenv("ANYPS5_GPU");
    const std::string_view request = requestedName != nullptr ? requestedName : "";
    if (const auto selected = SelectFixtureDevice(properties, request)) return devices[*selected];
    if (request.empty()) throw std::runtime_error("no Vulkan 1.1 device");
    std::string names;
    for (const auto& candidate : properties) {
        if (!names.empty()) names += ", ";
        names += candidate.deviceName;
    }
    throw std::runtime_error("no Vulkan 1.1 device whose name contains ANYPS5_GPU=\"" + std::string(request) + "\"; devices: " + names);
}

#endif
