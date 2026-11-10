#include "PipelineKeyAbi.hpp"
#include <SDL_loadso.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void Check(VkResult result, const char* message) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(message) + ": " + std::to_string(result));
}

struct Unavailable : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Device {
    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};

    template<typename TFunction>
    TFunction Function(const char* name) const {
        const auto result = reinterpret_cast<TFunction>(instanceProc(instance, name));
        Require(result != nullptr, name);
        return result;
    }

    ~Device() {
        if (device != VK_NULL_HANDLE) {
            if (module != VK_NULL_HANDLE) Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(device, module, nullptr);
            if (layout != VK_NULL_HANDLE) Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(device, layout, nullptr);
            Function<PFN_vkDestroyDevice>("vkDestroyDevice")(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) Function<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
        if (library != nullptr) SDL_UnloadObject(library);
    }

    bool Open() {
#ifdef _WIN32
        library = SDL_LoadObject("vulkan-1.dll");
#else
        library = SDL_LoadObject("libvulkan.so.1");
#endif
        if (library == nullptr) throw Unavailable("cannot load Vulkan");
        instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
        Require(instanceProc != nullptr, "missing Vulkan instance resolver");
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo = &application;
        const auto created = Function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance);
        if (created == VK_ERROR_INCOMPATIBLE_DRIVER) throw Unavailable("no compatible Vulkan driver");
        Check(created, "vkCreateInstance");
        std::uint32_t count = 0;
        const auto enumerate = Function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
        Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
        if (count == 0) throw Unavailable("no Vulkan device");
        std::vector<VkPhysicalDevice> devices(count);
        Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
        const auto lower = [](std::string text) {
            std::ranges::transform(text, text.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            return text;
        };
        const auto* requested = std::getenv("ANYPS5_GPU");
        const auto requestedName = lower(requested != nullptr ? requested : "");
        int best = -1;
        for (const auto candidate : devices) {
            VkPhysicalDeviceProperties queried{};
            Function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(candidate, &queried);
            if (queried.apiVersion < VK_API_VERSION_1_1 || lower(queried.deviceName).find(requestedName) == std::string::npos) continue;
            const int rank = queried.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : queried.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : queried.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? 1 : 0;
            if (rank <= best) continue;
            best = rank;
            physical = candidate;
            properties = queried;
        }
        if (physical == VK_NULL_HANDLE) {
            Require(requestedName.empty(), "no Vulkan device matches ANYPS5_GPU");
            throw Unavailable("no Vulkan 1.1 device");
        }
        const auto extensions = Function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
        Check(extensions(physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
        std::vector<VkExtensionProperties> available(count);
        Check(extensions(physical, nullptr, &count, available.data()), "vkEnumerateDeviceExtensionProperties");
        const auto has = [&](const char* name) { return std::ranges::any_of(available, [&](const auto& extension) { return std::strcmp(extension.extensionName, name) == 0; }); };
        if (properties.apiVersion >= VK_API_VERSION_1_2 || has(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
            VkPhysicalDeviceProperties2 queried{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driver};
            Function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(physical, &queried);
        }
        std::printf("device=%s; driver=%s; driver_info=%s; driver_id=%d; driver_version=%u; platform/ABI=%s; compiler=%s\n", properties.deviceName, driver.driverName, driver.driverInfo, static_cast<int>(driver.driverID), properties.driverVersion, PipelineKeyAbiPlatform(), __VERSION__);
        if (!has(VK_KHR_PIPELINE_BINARY_EXTENSION_NAME)) return false;
        constexpr std::array enabled{VK_KHR_PIPELINE_BINARY_EXTENSION_NAME, VK_KHR_MAINTENANCE_5_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};
        for (const auto* name : enabled) Require(has(name), name);
        VkPhysicalDevicePipelineBinaryFeaturesKHR binary{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_BINARY_FEATURES_KHR};
        VkPhysicalDeviceMaintenance5FeaturesKHR maintenance{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR, &binary};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &maintenance};
        Function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(physical, &features);
        Require(binary.pipelineBinaries == VK_TRUE && maintenance.maintenance5 == VK_TRUE, "pipeline binary or maintenance5 feature is unavailable");
        std::uint32_t familyCount = 0;
        const auto queues = Function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        queues(physical, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        queues(physical, &familyCount, families.data());
        std::uint32_t family = 0;
        while (family < families.size() && (families[family].queueCount == 0 || (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) ++family;
        Require(family < families.size(), "no compute queue");
        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        VkDeviceCreateInfo create{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &maintenance};
        create.queueCreateInfoCount = 1;
        create.pQueueCreateInfos = &queue;
        create.enabledExtensionCount = static_cast<std::uint32_t>(enabled.size());
        create.ppEnabledExtensionNames = enabled.data();
        Check(Function<PFN_vkCreateDevice>("vkCreateDevice")(physical, &create, nullptr, &device), "vkCreateDevice");
        return true;
    }

    VkComputePipelineCreateInfo Pipeline() {
        constexpr std::array spirv{0x07230203u, 0x00010000u, 0u, 6u, 0u,
            0x00020011u, 1u, 0x0003000eu, 0u, 1u,
            0x0005000fu, 5u, 4u, 0x6e69616du, 0u,
            0x00060010u, 4u, 17u, 1u, 1u, 1u,
            0x00020013u, 1u, 0x00030021u, 2u, 1u,
            0x00050036u, 1u, 4u, 0u, 2u, 0x000200f8u, 5u, 0x000100fdu, 0x00010038u};
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = sizeof(spirv);
        moduleInfo.pCode = spirv.data();
        Check(Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        Check(Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
        VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline.layout = layout;
        pipeline.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
        return pipeline;
    }
};

}

int main(int argc, char** argv) try {
    Require(argc == 2 && (std::string_view(argv[1]) == "global" || std::string_view(argv[1]) == "pipeline"), "expected global or pipeline");
    Device device;
    if (!device.Open()) {
        std::puts("skipped, the device lacks VK_KHR_pipeline_binary");
        return 77;
    }
    const bool perPipeline = std::string_view(argv[1]) == "pipeline";
    auto pipeline = perPipeline ? device.Pipeline() : VkComputePipelineCreateInfo{};
    VkPipelineCreateInfoKHR info{VK_STRUCTURE_TYPE_PIPELINE_CREATE_INFO_KHR, &pipeline};
    struct Output {
        std::array<std::uint8_t, 64> prefix;
        VkPipelineBinaryKeyKHR key{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
        std::array<std::uint8_t, 64> suffix;
    } output;
    output.prefix.fill(0xa5);
    output.suffix.fill(0xa5);
    PipelineKeyAbiObservation observation;
    const auto resolver = device.Function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
    const auto getKey = reinterpret_cast<PFN_vkGetPipelineKeyKHR>(resolver(device.device, "vkGetPipelineKeyKHR"));
    Require(getKey != nullptr, "missing vkGetPipelineKeyKHR");
    PipelineKeyAbiProbe(getKey, device.device, perPipeline ? &info : nullptr, &output.key, &observation);
    const auto intact = [](const auto& guard) { return std::ranges::all_of(guard, [](auto value) { return value == 0xa5; }); };
    const bool guards = intact(output.prefix) && intact(output.suffix);
    const bool header = output.key.sType == VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR && output.key.pNext == nullptr;
    std::printf("case=%s; VkResult=%d; key_size=%u; guard=%s; header=%s\n", argv[1], static_cast<int>(observation.result), output.key.keySize, guards ? "intact" : "changed", header ? "intact" : "changed");
    const auto changed = PipelineKeyAbiDifferences(observation);
    std::printf("changed_registers=%u\n", changed);
    return changed == 0 && guards && header ? 0 : 1;
} catch (const Unavailable& error) {
    if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::printf("skipped, no usable Vulkan device: %s\n", error.what());
    return 77;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
