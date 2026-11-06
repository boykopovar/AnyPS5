#include "prx/libSceAgcDriver/Execution/include/HostReportVulkan.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    ++failures;
}

struct FakeDevice {
    bool bytes = true;
    bool address = true;
    bool int64 = true;
    bool signedZero = true;
    bool stores = true;
    bool anisotropy = true;
    bool bc = true;
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    std::vector<VkPresentModeKHR> modes{VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR};
};

FakeDevice device;

template <typename TStructure>
TStructure* Find(void* chain, VkStructureType type) {
    for (auto* header = static_cast<VkBaseOutStructure*>(chain); header != nullptr; header = header->pNext)
        if (header->sType == type) return reinterpret_cast<TStructure*>(header);
    return nullptr;
}

void VKAPI_CALL Properties2(VkPhysicalDevice, VkPhysicalDeviceProperties2* properties) {
    std::strcpy(properties->properties.deviceName, "Fake GPU 9000");
    properties->properties.deviceType = device.type;
    properties->properties.vendorID = 0x1002;
    properties->properties.deviceID = 0x73bf;
    properties->properties.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 250);
    properties->properties.driverVersion = 0x00abcdef;
    if (auto* driver = Find<VkPhysicalDeviceDriverProperties>(properties->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES)) {
        std::strcpy(driver->driverName, "fake-driver");
        std::strcpy(driver->driverInfo, "Fake 24.1.2");
    }
    if (auto* controls = Find<VkPhysicalDeviceFloatControlsProperties>(properties->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES))
        controls->shaderSignedZeroInfNanPreserveFloat32 = device.signedZero ? VK_TRUE : VK_FALSE;
}

void VKAPI_CALL Features2(VkPhysicalDevice, VkPhysicalDeviceFeatures2* features) {
    features->features.shaderInt64 = device.int64 ? VK_TRUE : VK_FALSE;
    features->features.vertexPipelineStoresAndAtomics = device.stores ? VK_TRUE : VK_FALSE;
    features->features.fragmentStoresAndAtomics = device.stores ? VK_TRUE : VK_FALSE;
    features->features.samplerAnisotropy = device.anisotropy ? VK_TRUE : VK_FALSE;
    features->features.textureCompressionBC = device.bc ? VK_TRUE : VK_FALSE;
    if (auto* bytes = Find<VkPhysicalDevice8BitStorageFeatures>(features->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES))
        bytes->storageBuffer8BitAccess = device.bytes ? VK_TRUE : VK_FALSE;
    if (auto* address = Find<VkPhysicalDeviceBufferDeviceAddressFeatures>(features->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES))
        address->bufferDeviceAddress = device.address ? VK_TRUE : VK_FALSE;
}

void VKAPI_CALL Memory(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* memory) {
    *memory = {};
    memory->memoryHeapCount = 3;
    memory->memoryHeaps[0] = {16ull << 30, 0};
    memory->memoryHeaps[1] = {256ull << 20, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    memory->memoryHeaps[2] = {8ull << 30, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
}

VkResult VKAPI_CALL PresentModes(VkPhysicalDevice, VkSurfaceKHR, std::uint32_t* count, VkPresentModeKHR* modes) {
    if (modes == nullptr) {
        *count = static_cast<std::uint32_t>(device.modes.size());
        return VK_SUCCESS;
    }
    std::copy(device.modes.begin(), device.modes.begin() + *count, modes);
    return VK_SUCCESS;
}

PFN_vkVoidFunction VKAPI_CALL Proc(VkInstance, const char* name) {
    if (std::strcmp(name, "vkGetPhysicalDeviceProperties2") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Properties2);
    if (std::strcmp(name, "vkGetPhysicalDeviceFeatures2") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Features2);
    if (std::strcmp(name, "vkGetPhysicalDeviceMemoryProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Memory);
    if (std::strcmp(name, "vkGetPhysicalDeviceSurfacePresentModesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(PresentModes);
    return nullptr;
}

std::vector<VkExtensionProperties> Extensions(std::initializer_list<const char*> names) {
    std::vector<VkExtensionProperties> result;
    for (const char* name : names) {
        VkExtensionProperties item{};
        std::strcpy(item.extensionName, name);
        result.push_back(item);
    }
    return result;
}

std::vector<VkExtensionProperties> AllRequired() {
    return Extensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME});
}

AgcDriver::HostGpuQuery Query(const std::vector<VkExtensionProperties>& extensions) {
    AgcDriver::HostGpuQuery query;
    query.instanceProc = Proc;
    query.physical = reinterpret_cast<VkPhysicalDevice>(static_cast<std::uintptr_t>(1));
    query.surface = reinterpret_cast<VkSurfaceKHR>(static_cast<std::uintptr_t>(2));
    query.presentation = true;
    query.subgroupSize = 64;
    query.extensions = extensions;
    query.candidates = {"Fake GPU 9000", "Other GPU"};
    return query;
}

const AgcDriver::HostRequirement* Find(const AgcDriver::HostGpuFacts& facts, const std::string& name) {
    for (const auto& item : facts.requirements)
        if (item.name == name) return &item;
    return nullptr;
}

void IdentityIsRead() {
    device = {};
    const auto extensions = AllRequired();
    auto query = Query(extensions);
    query.requestedPresentMode = "mailbox";
    const auto gpu = AgcDriver::CollectHostGpuFacts(query);
    Check(gpu.name == "Fake GPU 9000" && gpu.type == "discrete", "the name or type was not read");
    Check(gpu.vendorId == 0x1002 && gpu.deviceId == 0x73bf && gpu.driverVersion == 0x00abcdef, "the ids or the raw driver version were not read");
    Check(gpu.apiVersion == VK_MAKE_API_VERSION(0, 1, 3, 250), "the Vulkan version was not read");
    Check(gpu.driverName == "fake-driver" && gpu.driverInfo == "Fake 24.1.2", "the driver name and info were not read");
    Check(gpu.deviceLocalMiB == 8192, "the largest device-local heap was not chosen");
    Check(gpu.subgroupSize == 64 && gpu.candidates.size() == 2 && gpu.requestedPresentMode == "mailbox", "the caller's facts were not kept");
    Check(gpu.presentModes == std::vector<std::string>{"fifo", "mailbox"}, "the present modes were not read");
    Check(AgcDriver::DescribeMissingRequirements(gpu).empty(), "a complete device reported a missing item");
    device.type = VK_PHYSICAL_DEVICE_TYPE_CPU;
    Check(AgcDriver::CollectHostGpuFacts(Query(extensions)).type == "cpu", "a software device was not named");
}

void NoDriverPropertiesLeavesDriverEmpty() {
    device = {};
    const auto extensions = Extensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME});
    const auto gpu = AgcDriver::CollectHostGpuFacts(Query(extensions));
    Check(gpu.driverName.empty() && gpu.driverInfo.empty(), "a driver was named without VK_KHR_driver_properties");
    const auto* optional = Find(gpu, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
    Check(optional != nullptr && !optional->required && !optional->present, "the absent driver properties extension is not listed as optional and missing");
    const auto* budget = Find(gpu, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    Check(budget != nullptr && !budget->present, "the absent memory budget extension is not listed as missing");
}

void MissingExtensionsAreFound() {
    device = {};
    const auto extensions = Extensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME});
    const auto gpu = AgcDriver::CollectHostGpuFacts(Query(extensions));
    const auto missing = AgcDriver::MissingRequirements(gpu);
    std::vector<std::string> names;
    for (const auto& item : missing) names.push_back(item.name);
    const std::vector<std::string> expected{VK_KHR_8BIT_STORAGE_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, "bufferDeviceAddress", "storageBuffer8BitAccess"};
    Check(names == expected, "the missing extensions and the features that depend on them were not found");
    const auto message = AgcDriver::DescribeMissingRequirements(gpu);
    Check(message.find("Fake GPU 9000") != std::string::npos && message.find("VK_KHR_8bit_storage (extension)") != std::string::npos, "the message does not name the GPU and the missing extension");
}

void MissingFeaturesAreFound() {
    device = {};
    device.int64 = false;
    device.signedZero = false;
    device.anisotropy = false;
    const auto extensions = AllRequired();
    const auto gpu = AgcDriver::CollectHostGpuFacts(Query(extensions));
    std::vector<std::string> names;
    for (const auto& item : AgcDriver::MissingRequirements(gpu)) names.push_back(item.name);
    const std::vector<std::string> expected{"samplerAnisotropy", "shaderInt64", "shaderSignedZeroInfNanPreserveFloat32"};
    Check(names == expected, "the unsupported features were not the only missing items");
    device = {};
    device.stores = false;
    device.bc = false;
    device.bytes = false;
    device.address = false;
    names.clear();
    for (const auto& item : AgcDriver::MissingRequirements(AgcDriver::CollectHostGpuFacts(Query(extensions)))) names.push_back(item.name);
    const std::vector<std::string> second{"bufferDeviceAddress", "fragmentStoresAndAtomics", "storageBuffer8BitAccess", "textureCompressionBC", "vertexPipelineStoresAndAtomics"};
    Check(names == second, "the second set of unsupported features was not found");
}

void SwapchainIsRequiredOnlyWithAWindow() {
    device = {};
    const auto extensions = Extensions({VK_KHR_8BIT_STORAGE_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME});
    auto query = Query(extensions);
    const auto windowed = AgcDriver::CollectHostGpuFacts(query);
    Check(Find(windowed, VK_KHR_SWAPCHAIN_EXTENSION_NAME) != nullptr && !AgcDriver::MissingRequirements(windowed).empty(), "a missing swapchain extension was not required with a window");
    query.presentation = false;
    query.surface = VK_NULL_HANDLE;
    const auto headless = AgcDriver::CollectHostGpuFacts(query);
    Check(Find(headless, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == nullptr && AgcDriver::MissingRequirements(headless).empty(), "the swapchain extension was required without a window");
    Check(headless.presentModes.empty(), "present modes were read without a surface");
}

void MissingEntryPointIsAnError() {
    device = {};
    const auto extensions = AllRequired();
    auto query = Query(extensions);
    query.instanceProc = [](VkInstance, const char*) -> PFN_vkVoidFunction { return nullptr; };
    bool thrown = false;
    try {
        static_cast<void>(AgcDriver::CollectHostGpuFacts(query));
    } catch (const std::runtime_error& error) {
        thrown = std::string(error.what()).find("vkGetPhysicalDeviceProperties2") != std::string::npos;
    }
    Check(thrown, "a missing entry point was not reported by name");
}

}

int main() {
    IdentityIsRead();
    NoDriverPropertiesLeavesDriverEmpty();
    MissingExtensionsAreFound();
    MissingFeaturesAreFound();
    SwapchainIsRequiredOnlyWithAWindow();
    MissingEntryPointIsAnError();
    return failures == 0 ? 0 : 1;
}
