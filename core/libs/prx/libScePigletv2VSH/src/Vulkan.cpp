#include "prx/libScePigletv2VSH/include/Vulkan.hpp"

#include <SDL.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

extern "C" void AgcDriverLockVulkanLoader_nid_postfix();
extern "C" void AgcDriverUnlockVulkanLoader_nid_postfix();

namespace Piglet {

namespace {

bool Matches(std::string_view name, std::string_view request) {
    for (std::size_t start = 0; start + request.size() <= name.size(); ++start) {
        std::size_t index = 0;
        while (index < request.size() && std::tolower(static_cast<unsigned char>(name[start + index])) == std::tolower(static_cast<unsigned char>(request[index]))) ++index;
        if (index == request.size()) return true;
    }
    return false;
}

int Rank(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
        default: return 0;
    }
}

}

void Check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string("libScePigletv2VSH: ") + what + " failed with VkResult " + std::to_string(static_cast<int>(result)));
}

Device& Device::Get() {
    static Device device;
    return device;
}

Device::Device() {
#ifdef _WIN32
    void* library = SDL_LoadObject("vulkan-1.dll");
#else
    void* library = SDL_LoadObject("libvulkan.so.1");
#endif
    if (!library) throw std::runtime_error(std::string("libScePigletv2VSH: Vulkan loader: ") + SDL_GetError());
    const auto instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
    if (!instanceProc) throw std::runtime_error("libScePigletv2VSH: Vulkan loader: vkGetInstanceProcAddr missing");
    AgcDriverLockVulkanLoader_nid_postfix();
    struct Unlock {
        ~Unlock() { AgcDriverUnlockVulkanLoader_nid_postfix(); }
    } unlock;
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "AnyPS5 libScePigletv2VSH";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &application;
    VkInstance instance = VK_NULL_HANDLE;
    const auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(instanceProc(nullptr, "vkCreateInstance"));
    Check(createInstance(&instanceInfo, nullptr, &instance), "vkCreateInstance");
    const auto instanceFunction = [&](const char* name) {
        const auto function = instanceProc(instance, name);
        if (!function) throw std::runtime_error(std::string("libScePigletv2VSH: missing Vulkan function ") + name);
        return function;
    };
    const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(instanceFunction("vkEnumeratePhysicalDevices"));
    const auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(instanceFunction("vkGetPhysicalDeviceProperties"));
    const auto queueFamilies = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(instanceFunction("vkGetPhysicalDeviceQueueFamilyProperties"));
    const auto memory = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(instanceFunction("vkGetPhysicalDeviceMemoryProperties"));
    const auto extensions = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(instanceFunction("vkEnumerateDeviceExtensionProperties"));
    const auto createDevice = reinterpret_cast<PFN_vkCreateDevice>(instanceFunction("vkCreateDevice"));
    const auto deviceProc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(instanceFunction("vkGetDeviceProcAddr"));

    std::uint32_t count = 0;
    Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> devices(count);
    Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
    const char* requested = std::getenv("ANYPS5_GPU");
    if (requested && !*requested) requested = nullptr;
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    std::uint32_t family = 0;
    int selectedRank = -1;
    VkDeviceSize alignment = 256;
    for (const auto physical : devices) {
        VkPhysicalDeviceProperties info{};
        properties(physical, &info);
        if (info.apiVersion < VK_API_VERSION_1_1) continue;
        if (requested && !Matches(info.deviceName, requested)) continue;
        const int rank = requested ? 4 : Rank(info.deviceType);
        if (rank <= selectedRank) continue;
        std::uint32_t familyCount = 0;
        queueFamilies(physical, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        queueFamilies(physical, &familyCount, families.data());
        for (std::uint32_t index = 0; index < familyCount; ++index) {
            if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue;
            selected = physical;
            family = index;
            selectedRank = rank;
            alignment = info.limits.minUniformBufferOffsetAlignment;
            break;
        }
    }
    if (!selected) throw std::runtime_error(requested ? std::string("libScePigletv2VSH: no Vulkan 1.1 graphics device matches ANYPS5_GPU=") + requested : "libScePigletv2VSH: no Vulkan 1.1 graphics device");
    uniformAlignment = alignment;
    memory(selected, &memoryProperties);

    std::uint32_t extensionCount = 0;
    Check(extensions(selected, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
    std::vector<VkExtensionProperties> available(extensionCount);
    Check(extensions(selected, nullptr, &extensionCount, available.data()), "vkEnumerateDeviceExtensionProperties");
    for (const auto& extension : available)
        if (std::strcmp(extension.extensionName, VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME) == 0) depthClipControl = true;

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    VkPhysicalDeviceDepthClipControlFeaturesEXT clipControl{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT};
    clipControl.depthClipControl = VK_TRUE;
    const char* clipExtension = VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME;
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    if (depthClipControl) {
        deviceInfo.pNext = &clipControl;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = &clipExtension;
    }
    Check(createDevice(selected, &deviceInfo, nullptr, &device), "vkCreateDevice");
#define PIGLET_LOAD(name)                                                                                        \
    name = reinterpret_cast<PFN_##name>(deviceProc(device, #name));                                              \
    if (!name) throw std::runtime_error("libScePigletv2VSH: missing Vulkan function " #name);
    PIGLET_DEVICE_FUNCTIONS(PIGLET_LOAD)
#undef PIGLET_LOAD
    vkGetDeviceQueue(device, family, 0, &queue);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = family;
    Check(vkCreateCommandPool(device, &poolInfo, nullptr, &pool), "vkCreateCommandPool");
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    Check(vkCreateFence(device, &fenceInfo, nullptr, &fence), "vkCreateFence");
}

bool Device::HasMemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags) const {
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
        if ((bits & (1u << index)) && (memoryProperties.memoryTypes[index].propertyFlags & flags) == flags) return true;
    return false;
}

std::uint32_t Device::MemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags) const {
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
        if ((bits & (1u << index)) && (memoryProperties.memoryTypes[index].propertyFlags & flags) == flags) return index;
    throw std::runtime_error("libScePigletv2VSH: no Vulkan memory type with the required properties");
}

Buffer Device::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool readback) {
    Buffer result;
    result.size = size;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    Check(vkCreateBuffer(device, &info, nullptr, &result.buffer), "vkCreateBuffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, result.buffer, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    constexpr VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const bool cached = readback && HasMemoryType(requirements.memoryTypeBits, visible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    allocation.memoryTypeIndex = MemoryType(requirements.memoryTypeBits, cached ? visible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT : visible);
    Check(vkAllocateMemory(device, &allocation, nullptr, &result.memory), "vkAllocateMemory");
    Check(vkBindBufferMemory(device, result.buffer, result.memory, 0), "vkBindBufferMemory");
    Check(vkMapMemory(device, result.memory, 0, VK_WHOLE_SIZE, 0, &result.mapped), "vkMapMemory");
    return result;
}

void Device::DestroyBuffer(Buffer& buffer) {
    if (buffer.buffer) vkDestroyBuffer(device, buffer.buffer, nullptr);
    if (buffer.memory) vkFreeMemory(device, buffer.memory, nullptr);
    buffer = {};
}

Image Device::CreateImage(std::uint32_t width, std::uint32_t height, VkImageUsageFlags usage) {
    Image result;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {width, height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Check(vkCreateImage(device, &info, nullptr, &result.image), "vkCreateImage");
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, result.image, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Check(vkAllocateMemory(device, &allocation, nullptr, &result.memory), "vkAllocateMemory");
    Check(vkBindImageMemory(device, result.image, result.memory, 0), "vkBindImageMemory");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = result.image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R8G8B8A8_UNORM;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    Check(vkCreateImageView(device, &view, nullptr, &result.view), "vkCreateImageView");
    return result;
}

void Device::DestroyImage(Image& image) {
    if (image.view) vkDestroyImageView(device, image.view, nullptr);
    if (image.image) vkDestroyImage(device, image.image, nullptr);
    if (image.memory) vkFreeMemory(device, image.memory, nullptr);
    image = {};
}

VkCommandBuffer Device::AllocateCommandBuffer() {
    VkCommandBufferAllocateInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    info.commandPool = pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    Check(vkAllocateCommandBuffers(device, &info, &commands), "vkAllocateCommandBuffers");
    return commands;
}

void Device::Submit(VkCommandBuffer commands) {
    std::lock_guard lock(submitMutex);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    Check(vkResetFences(device, 1, &fence), "vkResetFences");
    Check(vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit");
    Check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
}

void Device::RunOnce(const std::function<void(VkCommandBuffer)>& record) {
    static std::mutex onceMutex;
    std::lock_guard lock(onceMutex);
    static VkCommandBuffer commands = AllocateCommandBuffer();
    Check(vkResetCommandBuffer(commands, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(vkBeginCommandBuffer(commands, &begin), "vkBeginCommandBuffer");
    record(commands);
    Check(vkEndCommandBuffer(commands), "vkEndCommandBuffer");
    Submit(commands);
}

}
