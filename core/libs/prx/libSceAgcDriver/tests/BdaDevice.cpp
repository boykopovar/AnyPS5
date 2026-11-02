#include "BdaShader.hpp"
#include "ColorTransferTests.hpp"
#include "HostImportFailure.hpp"
#include <fstream>
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <SDL_loadso.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver::Graphics {
Recorder* Recorder::Active() { return nullptr; }
void Recorder::Submit() { Require(false, "the device tests have no recorder"); }
bool Recorder::Reap() { Require(false, "the device tests have no recorder"); return false; }
}

namespace {

using namespace AgcDriver::Graphics;

std::optional<std::size_t> selectDevice(std::span<const VkPhysicalDeviceProperties> devices, std::string_view request) {
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

bool canSkipVulkanFailure(const char* requestedName, const char* requireVulkan) {
    return (requestedName == nullptr || *requestedName == '\0') && requireVulkan == nullptr;
}

void deviceSelectionTests() {
    const auto properties = [](const char* name, VkPhysicalDeviceType type) {
        VkPhysicalDeviceProperties result{};
        result.apiVersion = VK_API_VERSION_1_1;
        result.deviceType = type;
        std::memcpy(result.deviceName, name, std::strlen(name) + 1u);
        return result;
    };
    std::array devices{
        properties("AMD Radeon(TM) Graphics", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU),
        properties("NVIDIA GeForce RTX 5080", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU),
    };
    Require(!selectDevice({}, {}).has_value(), "an empty Vulkan device list selected a device");
    Require(selectDevice(devices, {}) == 1u, "default selection did not prefer the discrete GPU");
    Require(selectDevice(devices, "AMD") == 0u, "an AMD request lost to an unrelated discrete GPU");
    Require(selectDevice(devices, "aMd rAdEoN") == 0u, "GPU name matching was not case-insensitive");
    Require(selectDevice(devices, "RTX 5080") == 1u, "partial GPU name matching failed");
    Require(!selectDevice(devices, "missing device").has_value(), "an unmatched GPU request selected another device");
    Require(canSkipVulkanFailure(nullptr, nullptr), "an optional Vulkan test without a device request cannot skip");
    Require(canSkipVulkanFailure("", nullptr), "an empty GPU request prevents an optional Vulkan skip");
    Require(!canSkipVulkanFailure("missing device", nullptr), "an unmatched GPU request can be reported as a skip");
    Require(!canSkipVulkanFailure(nullptr, "1"), "a required Vulkan test can be reported as a skip");
    devices[0].apiVersion = VK_API_VERSION_1_0;
    Require(!selectDevice(devices, "AMD").has_value(), "a GPU without Vulkan 1.1 matched the request");
    std::cout << "BDA device selection CPU tests passed\n";
}

class Device {
public:
    explicit Device(bool importHostMemory = false) {
#ifdef _WIN32
        library = SDL_LoadObject("vulkan-1.dll");
#elif defined(__APPLE__)
        library = SDL_LoadObject("libvulkan.1.dylib");
#else
        library = SDL_LoadObject("libvulkan.so.1");
#endif
        Require(library != nullptr, "cannot load Vulkan");
        try {
            instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
            Require(instanceProc != nullptr, "missing Vulkan instance resolver");
            VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            application.apiVersion = VK_API_VERSION_1_1;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            info.pApplicationInfo = &application;
#ifdef __APPLE__
            const char* portability = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            info.enabledExtensionCount = 1;
            info.ppEnabledExtensionNames = &portability;
#endif
            Check(function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
            std::uint32_t count = 0;
            const auto enumerate = function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
            Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            Require(count != 0, "no Vulkan device");
            std::vector<VkPhysicalDevice> devices(count);
            Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            devices.resize(count);
            const auto physicalProperties = function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
            std::vector<VkPhysicalDeviceProperties> deviceProperties(count);
            for (std::size_t index = 0; index < devices.size(); ++index) {
                physicalProperties(devices[index], &deviceProperties[index]);
            }
            const char* requestedName = std::getenv("ANYPS5_GPU");
            const std::string_view request = requestedName != nullptr ? requestedName : "";
            const auto selected = selectDevice(deviceProperties, request);
            if (!selected.has_value()) {
                if (!request.empty()) {
                    std::string candidateNames;
                    for (const auto& candidate : deviceProperties) {
                        if (!candidateNames.empty()) candidateNames += ", ";
                        candidateNames += candidate.deviceName;
                    }
                    throw std::runtime_error(std::string("no Vulkan 1.1 device whose name contains ANYPS5_GPU=\"") + std::string(request) + "\"; devices: " + candidateNames);
                }
                throw std::runtime_error("no Vulkan 1.1 device");
            }
            context.physical = devices[*selected];
            cpu = deviceProperties[*selected].deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
            std::cout << "Using \"" << deviceProperties[*selected].deviceName << "\"\n";
            const auto extensions = function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            Check(extensions(context.physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> available(count);
            Check(extensions(context.physical, nullptr, &count, available.data()), "vkEnumerateDeviceExtensionProperties");
            auto bytes = AgcDriver::QueryBdaByteFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            auto address = AgcDriver::QueryBdaFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) ++family;
            Require(family < count, "no Vulkan compute queue");
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            address.pNext = &bytes;
            std::vector<const char*> extensionsEnabled{VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME};
            const bool externalHost = importHostMemory && std::any_of(available.begin(), available.end(), [](const auto& item) { return std::strcmp(item.extensionName, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) == 0; });
            if (externalHost) extensionsEnabled.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &address};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.enabledExtensionCount = static_cast<std::uint32_t>(extensionsEnabled.size());
            device.ppEnabledExtensionNames = extensionsEnabled.data();
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
            if (externalHost) {
                VkPhysicalDeviceExternalMemoryHostPropertiesEXT host{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT};
                VkPhysicalDeviceProperties2 extended{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &host};
                function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(context.physical, &extended);
                context.hostImportAlignment = host.minImportedHostPointerAlignment;
            }
            if (importHostMemory) std::printf("host import device %s vendor=0x%04x device=0x%04x driver=0x%08x alignment=%llu\n", properties.deviceName, properties.vendorID, properties.deviceID, properties.driverVersion, static_cast<unsigned long long>(context.hostImportAlignment));
            context.bufferDeviceAddress = true;
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool.queueFamilyIndex = family;
            Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.pool), "vkCreateCommandPool");
        } catch (...) {
            release();
            throw;
        }
    }

    ~Device() { release(); }
    const Context& GetContext() const { return context; }
    bool RunsOnCpu() const { return cpu; }

private:
    template<typename TFunction>
    TFunction function(const char* name) const {
        const auto result = reinterpret_cast<TFunction>(instanceProc(instance, name));
        Require(result != nullptr, name);
        return result;
    }

    void release() noexcept {
        if (context.pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(context.device, context.pool, nullptr);
        context.bufferPool.reset();
        if (context.device != VK_NULL_HANDLE) function<PFN_vkDestroyDevice>("vkDestroyDevice")(context.device, nullptr);
        if (instance != VK_NULL_HANDLE) function<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
        if (library != nullptr) SDL_UnloadObject(library);
    }

    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    Context context{};
    bool cpu = false;
};

}

int main(int argc, char** argv) {
    try {
        deviceSelectionTests();
        if (argc == 2 && std::string_view(argv[1]) == "--device-selection-cpu") return 0;
        RunBdaContractTests();
        const bool importFailures = argc == 2 && std::strcmp(argv[1], "--host-import-failures") == 0;
        if (argc == 2 && !importFailures) {
            const auto shader = MakeBdaTestShader(0x7fff12340000ULL, 32);
            std::ofstream file(argv[1], std::ios::binary);
            file.write(reinterpret_cast<const char*>(shader.data()), static_cast<std::streamsize>(shader.size() * sizeof(std::uint32_t)));
            Require(static_cast<bool>(file), "cannot save BDA test SPIR-V");
            return 0;
        }
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>(importFailures);
        } catch (const std::exception& error) {
            if (!canSkipVulkanFailure(std::getenv("ANYPS5_GPU"), std::getenv("ANYPS5_REQUIRE_VULKAN"))) throw;
            std::cout << "skipped, no usable Vulkan device: " << error.what() << '\n';
            return 77;
        }
        if (importFailures) {
            if (device->GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, no external host memory import support\n";
                return 77;
            }
            AgcDriver::Tests::RunHostImportFailureTests(device->GetContext(), true);
            return 0;
        }
        Buffer buffer(device->GetContext(), 256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        Require(buffer.DeviceAddress() != 0 && buffer.Bytes().size() == 256, "invalid real BDA buffer");
        buffer.Bytes()[255] = std::byte{0x5a};
        Require(buffer.Bytes()[255] == std::byte{0x5a}, "real BDA buffer mapping failed");
        if (device->RunsOnCpu()) {
            std::cout << "CPU Vulkan device: BDA read and multi-dword write execution not tested\n";
        } else {
            RunBdaExecutionTests(device->GetContext());
        }
        RunBdaStoreExecutionTests(device->GetContext());
        std::cout << "BDA store boundary tests passed\n";
        RunColorTransferTests(device->GetContext());
        std::cout << "Vulkan BDA allocation and execution tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
