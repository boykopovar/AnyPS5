#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessTable.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "BindlessImageAbi.hpp"
#include "BindlessLookup_spv.h"
#include <SDL_loadso.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::BindlessTable;
using AgcDriver::Graphics::Buffer;
using AgcDriver::Graphics::Check;
using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::DescriptorHeap;
using AgcDriver::Graphics::DeviceImageKeys;
using AgcDriver::Graphics::DeviceImageValue;
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;
namespace Abi = ShaderRecompiler::BindlessImageAbi;
namespace Table = ShaderRecompiler::DeviceImageTableAbi;

constexpr int Skipped = 77;
constexpr std::uint32_t Side = 64u;
constexpr std::uint32_t DistinctImages = 256u;
constexpr std::uint64_t InterfaceId = 0x123456789abcdef0ull;
constexpr VkBufferUsageFlags AddressUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

struct Push {
    std::uint64_t header;
    std::uint64_t descriptors;
    std::uint64_t indices;
    std::uint64_t results;
    std::uint64_t slots;
    std::uint32_t interfaceLow;
    std::uint32_t interfaceHigh;
    std::uint32_t mode;
    std::uint32_t iterations;
    std::uint32_t listMask;
    std::uint32_t pc;
};

class Device {
public:
    Device() {
#ifdef _WIN32
        library = SDL_LoadObject("vulkan-1.dll");
#else
        library = SDL_LoadObject("libvulkan.so.1");
#endif
        Require(library != nullptr, "cannot load Vulkan");
        try {
            instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
            Require(instanceProc != nullptr, "missing Vulkan instance resolver");
            VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            application.apiVersion = VK_API_VERSION_1_2;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            info.pApplicationInfo = &application;
            Check(function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
            std::uint32_t count = 0;
            const auto enumerate = function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
            Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            Require(count != 0, "no Vulkan device");
            std::vector<VkPhysicalDevice> devices(count);
            Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            const auto physicalProperties = function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
            int selectedRank = -1;
            for (const auto physical : devices) {
                VkPhysicalDeviceProperties candidate{};
                physicalProperties(physical, &candidate);
                if (candidate.apiVersion < VK_API_VERSION_1_2) continue;
                const int rank = candidate.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : candidate.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : candidate.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? 1 : 0;
                if (rank <= selectedRank) continue;
                context.physical = physical;
                selectedRank = rank;
            }
            Require(context.physical != VK_NULL_HANDLE, "no Vulkan 1.2 device");
            physicalProperties(context.physical, &properties);
            const auto extensions = function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            Check(extensions(context.physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> available(count);
            Check(extensions(context.physical, nullptr, &count, available.data()), "vkEnumerateDeviceExtensionProperties");
            const auto hasExtension = [&](const char* name) { return std::any_of(available.begin(), available.end(), [&](const VkExtensionProperties& item) { return std::strcmp(item.extensionName, name) == 0; }); };
            Require(hasExtension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME) && hasExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME), "VK_EXT_robustness2 or VK_KHR_buffer_device_address is unavailable");
            VkPhysicalDeviceRobustness2FeaturesEXT robustness{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
            VkPhysicalDeviceVulkan12Features supported12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &robustness};
            VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &supported12};
            function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(context.physical, &supported);
            const AgcDriver::Graphics::BindlessFeatureSupport support{supported12.bufferDeviceAddress == VK_TRUE, robustness.nullDescriptor == VK_TRUE, supported12.runtimeDescriptorArray == VK_TRUE, supported12.descriptorBindingPartiallyBound == VK_TRUE, supported12.shaderSampledImageArrayNonUniformIndexing == VK_TRUE, supported12.shaderStorageImageArrayNonUniformIndexing == VK_TRUE, supported12.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE, supported12.descriptorBindingStorageImageUpdateAfterBind == VK_TRUE, supported12.descriptorBindingUpdateUnusedWhilePending == VK_TRUE};
            if (const auto missing = AgcDriver::Graphics::BindlessUnsupported(support)) throw std::runtime_error("the device lacks " + *missing);
            Require(supported.features.shaderInt64 == VK_TRUE, "shaderInt64 is unavailable");
            VkPhysicalDeviceRobustness2FeaturesEXT enabledRobustness{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
            enabledRobustness.nullDescriptor = VK_TRUE;
            VkPhysicalDeviceVulkan12Features enabled12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &enabledRobustness};
            enabled12.bufferDeviceAddress = VK_TRUE;
            enabled12.runtimeDescriptorArray = VK_TRUE;
            enabled12.descriptorBindingPartiallyBound = VK_TRUE;
            enabled12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            enabled12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
            enabled12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
            enabled12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
            enabled12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
            enabled12.timelineSemaphore = supported12.timelineSemaphore;
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) ++family;
            Require(family < count, "no Vulkan compute queue");
            timestampBits = families[family].timestampValidBits;
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            std::vector<const char*> extensionNames{VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME};
            if (enabled12.timelineSemaphore == VK_TRUE && hasExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) extensionNames.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &enabled12};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.enabledExtensionCount = static_cast<std::uint32_t>(extensionNames.size());
            device.ppEnabledExtensionNames = extensionNames.data();
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            context.limits = properties.limits;
            context.bufferDeviceAddress = true;
            context.nullDescriptors = true;
            context.descriptorIndexing = true;
            context.runtimeDescriptorArray = true;
            context.partiallyBound = true;
            context.descriptorIndexingLimits = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES};
            VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
            context.descriptorIndexingLimits.pNext = &subgroup;
            VkPhysicalDeviceProperties2 queried{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &context.descriptorIndexingLimits};
            function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(context.physical, &queried);
            context.descriptorIndexingLimits.pNext = nullptr;
            subgroup.pNext = nullptr;
            context.subgroup = subgroup;
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.imageFormatProperties = function<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties");
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            context.queueFamily = family;
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool.queueFamilyIndex = family;
            Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.pool), "vkCreateCommandPool");
            timeline = enabled12.timelineSemaphore == VK_TRUE;
            std::cout << "Vulkan device: " << properties.deviceName << ", subgroup " << subgroup.subgroupSize << '\n';
        } catch (...) {
            release();
            throw;
        }
    }

    ~Device() { release(); }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    const Context& GetContext() const { return context; }
    const VkPhysicalDeviceProperties& Properties() const { return properties; }
    bool Timeline() const { return timeline; }
    std::uint32_t TimestampBits() const { return timestampBits; }
    void WaitIdle() const { Check(context.Function<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(context.device), "vkDeviceWaitIdle"); }

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
    VkPhysicalDeviceProperties properties{};
    Context context{};
    bool timeline = false;
    std::uint32_t timestampBits = 0;
};

class Images {
public:
    Images(const Context& context, std::uint32_t count) : context(context) {
        try {
            for (std::uint32_t index = 0; index < count; ++index) create();
            upload();
        } catch (...) {
            release();
            throw;
        }
    }
    ~Images() { release(); }
    Images(const Images&) = delete;
    Images& operator=(const Images&) = delete;
    VkImageView View(std::uint32_t index) const { return views.at(index); }
    std::uint32_t Count() const { return static_cast<std::uint32_t>(images.size()); }
    static std::uint32_t Texel(std::uint32_t image, std::uint32_t x, std::uint32_t y) { return image * 0x10000u + y * Side + x + 1u; }

private:
    void create() {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R32_UINT;
        info.extent = {Side, Side, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage image = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage");
        images.push_back(image);
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory memory = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory");
        memories.push_back(memory);
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory");
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R32_UINT;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView view = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView");
        views.push_back(view);
    }

    void upload() {
        const std::size_t imageBytes = static_cast<std::size_t>(Side) * Side * sizeof(std::uint32_t);
        Buffer staging(context, imageBytes * images.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        auto* texels = reinterpret_cast<std::uint32_t*>(staging.Bytes().data());
        for (std::uint32_t image = 0; image < images.size(); ++image) {
            for (std::uint32_t y = 0; y < Side; ++y) {
                for (std::uint32_t x = 0; x < Side; ++x) texels[(static_cast<std::size_t>(image) * Side + y) * Side + x] = Texel(image, x, y);
            }
        }
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = context.pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocation, &commands), "vkAllocateCommandBuffers");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin), "vkBeginCommandBuffer");
        const auto barrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        for (std::uint32_t image = 0; image < images.size(); ++image) {
            VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.srcQueueFamilyIndex = toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.image = images[image];
            toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
            VkBufferImageCopy region{};
            region.bufferOffset = imageBytes * image;
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {Side, Side, 1};
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, staging.Handle(), images[image], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            auto toRead = toTransfer;
            toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toRead);
        }
        Check(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commands;
        Check(context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit images");
        Check(context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue), "vkQueueWaitIdle images");
        context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 1, &commands);
    }

    void release() noexcept {
        for (const auto view : views) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        for (const auto image : images) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        for (const auto memory : memories) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
        views.clear();
        images.clear();
        memories.clear();
    }

    Context context;
    std::vector<VkImage> images;
    std::vector<VkDeviceMemory> memories;
    std::vector<VkImageView> views;
};

class LookupPipeline {
public:
    LookupPipeline(const Context& context, DescriptorHeap& heap, VkImageView direct) : context(context) {
        try {
            const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
            VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &binding;
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &directLayout), "vkCreateDescriptorSetLayout direct");
            const VkDescriptorSetLayoutCreateInfo emptyInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &emptyInfo, nullptr, &emptyLayout), "vkCreateDescriptorSetLayout empty");
            const std::array<VkDescriptorSetLayout, 4> layouts{directLayout, emptyLayout, emptyLayout, heap.Layout(VK_SHADER_STAGE_COMPUTE_BIT)};
            const VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push)};
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            pipelineLayoutInfo.setLayoutCount = static_cast<std::uint32_t>(layouts.size());
            pipelineLayoutInfo.pSetLayouts = layouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &range;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = sizeof(BINDLESS_LOOKUP_SPV);
            moduleInfo.pCode = BINDLESS_LOOKUP_SPV;
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
            pipelineInfo.layout = pipelineLayout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
            const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &size;
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool direct");
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool = pool;
            allocation.descriptorSetCount = 1;
            allocation.pSetLayouts = &directLayout;
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &directSet), "vkAllocateDescriptorSets direct");
            const VkDescriptorImageInfo image{VK_NULL_HANDLE, direct, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = directSet;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            write.pImageInfo = &image;
            context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
        } catch (...) {
            release();
            throw;
        }
    }
    ~LookupPipeline() { release(); }
    LookupPipeline(const LookupPipeline&) = delete;
    LookupPipeline& operator=(const LookupPipeline&) = delete;

    void Record(VkCommandBuffer commands, DescriptorHeap& heap, const Push& push, std::uint32_t groups) const {
        context.BindComputePipeline(commands, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &directSet, 0, nullptr);
        heap.Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT);
        context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, groups, 1, 1);
    }

private:
    void release() noexcept {
        if (pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (pipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (module != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
        if (emptyLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, emptyLayout, nullptr);
        if (directLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, directLayout, nullptr);
        pool = VK_NULL_HANDLE;
        pipeline = VK_NULL_HANDLE;
        module = VK_NULL_HANDLE;
        pipelineLayout = VK_NULL_HANDLE;
        emptyLayout = VK_NULL_HANDLE;
        directLayout = VK_NULL_HANDLE;
    }

    Context context;
    VkDescriptorSetLayout directLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet directSet = VK_NULL_HANDLE;
};

std::array<std::uint32_t, Table::DescriptorWords> Descriptor(std::uint32_t record) {
    std::array<std::uint32_t, Table::DescriptorWords> words{};
    for (std::uint32_t word = 0; word < words.size(); ++word) words[word] = record * 0x9e3779b9u + word * 0x85ebca6bu + 0x51ed27u;
    return words;
}

std::array<std::uint32_t, Table::DescriptorWords> WithFeedback(std::array<std::uint32_t, Table::DescriptorWords> words, std::uint32_t record) {
    words[Abi::FeedbackWord5] = (words[Abi::FeedbackWord5] & ~Abi::FeedbackMask5) | ((record * 0x2f1u) & Abi::FeedbackMask5);
    words[Abi::FeedbackWord6] = (words[Abi::FeedbackWord6] & ~Abi::FeedbackMask6) | ((record * 0x3du) & Abi::FeedbackMask6);
    return words;
}

Table::Key RecordKey(std::uint32_t record) {
    const auto words = Descriptor(record);
    return Abi::MakeKey(words, InterfaceId, 0u);
}

std::uint32_t RecordMode(std::uint32_t record) {
    return (record * 7u) & 0xffu;
}

void keyTests() {
    const auto plain = Descriptor(5u);
    const auto noisy = WithFeedback(plain, 77u);
    Require(plain != noisy, "the feedback test descriptor did not change");
    Require(Abi::MakeKey(plain, InterfaceId, 0u) == Abi::MakeKey(noisy, InterfaceId, 0u), "the bindless key keeps T# streaming-feedback bits");
    auto other = plain;
    other[3] ^= 1u << 12u;
    Require(Abi::MakeKey(plain, InterfaceId, 0u) != Abi::MakeKey(other, InterfaceId, 0u), "the bindless key drops a view field");
    Require(Abi::MakeKey(plain, InterfaceId, 0u) != Abi::MakeKey(plain, InterfaceId + 1u, 0u), "the bindless key drops the interface id");
    const std::array<std::uint32_t, Table::SamplerWords> sampler{1u, 2u, 3u, 4u};
    const auto withSampler = Abi::MakeKey(plain, InterfaceId, 9u, sampler);
    Require(withSampler[Table::SamplerClassWord] == 9u && withSampler[Table::SamplerWord + 3u] == 4u, "the bindless key lost its S# words");
    Require(Abi::PackSlot(5u, 3u) == (5u | (3u << Abi::SlotBits)) && Abi::SlotIndex(Abi::PackSlot(0x3ffffu, 0xfffu)) == 0x3ffffu && Abi::SlotGeneration(Abi::PackSlot(1u, 0xfffu)) == 0xfffu, "slot packing changed");
    Require(!AgcDriver::Graphics::BindlessUnsupported({true, true, true, true, true, true, true, true, true}).has_value(), "a complete feature set was refused");
    const auto missing = AgcDriver::Graphics::BindlessUnsupported({true, true, true, true, true, true, true, false, false});
    Require(missing.has_value() && *missing == "descriptorBindingStorageImageUpdateAfterBind, descriptorBindingUpdateUnusedWhilePending", "the missing bindless features were not named");
    std::cout << "bindless key tests passed\n";
}

struct Fixture {
    Fixture(const Device& device, std::uint32_t records, std::uint32_t missing, std::uint32_t list, std::uint32_t workingSet = 0u, std::uint32_t ringCapacity = BindlessTable::DefaultRingCapacity) : context(device.GetContext()) {
        const auto capacities = DescriptorHeap::DeviceCapacities(context);
        Require(capacities.sampledImages > records, "the device's update-after-bind sampled image limit is below the test's records: " + std::to_string(capacities.sampledImages));
        table = std::make_unique<BindlessTable>(context, DescriptorHeap::Capacities{std::min(capacities.samplers, 64u), capacities.sampledImages, std::min(capacities.storageImages, 64u)}, DeviceImageKeys::InitialBuckets, DeviceImageKeys::MaxBuckets, ringCapacity);
        images = std::make_unique<Images>(context, DistinctImages);
        pipeline = std::make_unique<LookupPipeline>(context, table->Heap(), images->View(0));
        total = records + missing;
        descriptors = std::make_shared<Buffer>(context, static_cast<std::size_t>(total) * 32u, AddressUsage);
        slotList = std::make_shared<Buffer>(context, static_cast<std::size_t>(total) * 4u, AddressUsage);
        indices = std::make_shared<Buffer>(context, static_cast<std::size_t>(list) * 4u, AddressUsage);
        auto* words = reinterpret_cast<std::uint32_t*>(descriptors->Bytes().data());
        auto* slotWords = reinterpret_cast<std::uint32_t*>(slotList->Bytes().data());
        slots.assign(total, 0u);
        for (std::uint32_t record = 0; record < total; ++record) {
            const auto descriptor = WithFeedback(Descriptor(record), record);
            std::copy(descriptor.begin(), descriptor.end(), words + static_cast<std::size_t>(record) * 8u);
            if (record < records) {
                slots[record] = table->Heap().AllocateImage(DescriptorHeap::Array::SampledImages, images->View(record % DistinctImages), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                Require(table->Insert(RecordKey(record), DeviceImageValue{slots[record], RecordMode(record), 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a bindless record was not inserted");
            }
            slotWords[record] = Abi::SlotIndex(slots[record]);
        }
        std::mt19937 random(1234u);
        order.resize(list);
        const auto span = workingSet != 0u ? std::min(workingSet, total) : total;
        for (auto& value : order) value = std::uniform_int_distribution<std::uint32_t>(0u, span - 1u)(random) * (total / span);
        if (missing > 1u) order[0] = records + 1u;
        std::memcpy(indices->Bytes().data(), order.data(), order.size() * 4u);
        listMask = list - 1u;
        Require((list & listMask) == 0u, "the index list is not a power of two");
    }

    std::uint32_t Expected(std::uint32_t id, std::uint32_t iterations, bool direct, std::span<const std::uint32_t> imageOf, std::span<const std::uint32_t> modeOf) const {
        std::uint32_t sum = 0;
        for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
            const auto record = order[(id * 7u + iteration * 131u) & listMask];
            const auto x = record & 63u;
            const auto y = (record >> 6u) & 63u;
            if (direct) {
                sum += Images::Texel(0u, x, y);
                continue;
            }
            if (imageOf[record] == ~0u) continue;
            sum += Images::Texel(imageOf[record], x, y) + modeOf[record];
        }
        return sum;
    }

    Push MakePush(std::uint32_t mode, std::uint32_t iterations, const Buffer& results, std::uint32_t pc) const {
        return Push{table->HeaderAddress(), descriptors->DeviceAddress(), indices->DeviceAddress(), results.DeviceAddress(), slotList->DeviceAddress(), static_cast<std::uint32_t>(InterfaceId), static_cast<std::uint32_t>(InterfaceId >> 32u), mode, iterations, listMask, pc};
    }

    Context context;
    std::unique_ptr<BindlessTable> table;
    std::unique_ptr<Images> images;
    std::unique_ptr<LookupPipeline> pipeline;
    std::shared_ptr<Buffer> descriptors;
    std::shared_ptr<Buffer> slotList;
    std::shared_ptr<Buffer> indices;
    std::vector<std::uint32_t> slots;
    std::vector<std::uint32_t> order;
    std::uint32_t total = 0;
    std::uint32_t listMask = 0;
};

std::uint32_t Mismatches(std::span<const std::byte> bytes, std::uint32_t threads, const std::function<std::uint32_t(std::uint32_t)>& expected, std::string_view what) {
    std::uint32_t mismatches = 0;
    const auto* words = reinterpret_cast<const std::uint32_t*>(bytes.data());
    for (std::uint32_t id = 0; id < threads; ++id) {
        const auto want = expected(id);
        if (words[id] == want) continue;
        if (mismatches < 4u) std::cerr << what << ": invocation " << id << " read " << words[id] << ", expected " << want << '\n';
        ++mismatches;
    }
    return mismatches;
}

void gpuTests(const Device& device) {
    constexpr std::uint32_t Records = 33000u;
    constexpr std::uint32_t Missing = 64u;
    constexpr std::uint32_t Groups = 64u;
    constexpr std::uint32_t Threads = Groups * 64u;
    constexpr std::uint32_t Iterations = 4u;
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    Fixture fixture(device, Records, Missing, 1u << 16u);
    const auto& context = fixture.context;
    auto& table = *fixture.table;
    Recorder recorder(context, device.Timeline());
    recorder.Activate();
    std::vector<std::uint32_t> imageOf(fixture.total, ~0u);
    std::vector<std::uint32_t> modeOf(fixture.total, 0u);
    for (std::uint32_t record = 0; record < Records; ++record) {
        imageOf[record] = record % DistinctImages;
        modeOf[record] = RecordMode(record);
    }
    std::uint32_t distinctPerSubgroup = 0;
    const auto subgroup = std::max(1u, context.subgroup.subgroupSize);
    for (std::uint32_t first = 0; first < Threads; first += subgroup) {
        std::set<std::uint32_t> seen;
        for (std::uint32_t lane = 0; lane < subgroup; ++lane) seen.insert(fixture.slots[fixture.order[((first + lane) * 7u) & fixture.listMask]]);
        distinctPerSubgroup += static_cast<std::uint32_t>(seen.size());
    }
    std::cout << "lanes of a subgroup read " << static_cast<double>(distinctPerSubgroup) / static_cast<double>(Threads / subgroup) << " distinct heap slots on average (subgroup " << subgroup << ")\n";

    const auto updated = Records / 2u;
    const auto late = Records + 1u;
    const auto lateSlot = fixture.table->Heap().AllocateImage(DescriptorHeap::Array::SampledImages, fixture.images->View(3u), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    std::array<std::shared_ptr<Buffer>, 4> results;
    for (auto& result : results) result = std::make_shared<Buffer>(context, Threads * 4u, AddressUsage);
    table.Publish(recorder);
    fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *results[0], 0x111u), Groups);
    Require(table.Insert(RecordKey(updated), DeviceImageValue{fixture.slots[updated + 1u], 0x42u, 0u, 0u}) == DeviceImageKeys::Insertion::Updated, "updating a bindless entry inserted it");
    Require(table.Insert(RecordKey(late), DeviceImageValue{lateSlot, 0x5u, 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a late bindless entry was not inserted");
    fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *results[1], 0x222u), Groups);
    table.Publish(recorder);
    fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *results[2], 0x333u), Groups);
    fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(1u, Iterations, *results[3], 0x444u), Groups);
    const auto firstBatch = recorder.OpenSerial();
    recorder.Submit();
    recorder.Sync();
    std::uint32_t failures = 0;
    failures += Mismatches(results[0]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, imageOf, modeOf); }, "lookup before the update");
    failures += Mismatches(results[1]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, imageOf, modeOf); }, "lookup recorded after an unpublished update");
    auto updatedImage = imageOf;
    auto updatedMode = modeOf;
    updatedImage[updated] = (updated + 1u) % DistinctImages;
    updatedMode[updated] = 0x42u;
    updatedImage[late] = 3u;
    updatedMode[late] = 0x5u;
    failures += Mismatches(results[2]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, updatedImage, updatedMode); }, "lookup after the published update");
    std::vector<std::uint32_t> slotModes(fixture.total, 0u);
    failures += Mismatches(results[3]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, imageOf, slotModes); }, "slot-indexed reads");
    Require(failures == 0u, std::to_string(failures) + " invocations read wrong bindless results");
    std::cout << "publication ordering and nonuniform lookup tests passed (batch " << firstBatch << ")\n";

    std::set<std::uint32_t> touched;
    for (std::uint32_t id = 0; id < Threads; ++id) {
        for (std::uint32_t iteration = 0; iteration < Iterations; ++iteration) touched.insert(fixture.order[(id * 7u + iteration * 131u) & fixture.listMask]);
    }
    std::set<Table::Key> wantedMisses;
    for (const auto record : touched) {
        if (record >= Records) wantedMisses.insert(RecordKey(record));
    }
    const auto misses = table.TakeMisses();
    std::set<Table::Key> gotMisses;
    std::set<std::uint32_t> pcs;
    for (const auto& miss : misses) {
        gotMisses.insert(miss.key);
        pcs.insert(miss.pc);
    }
    auto wantedWithLate = wantedMisses;
    wantedWithLate.erase(RecordKey(late));
    Require(touched.contains(late) && wantedMisses.contains(RecordKey(late)), "the test's index list never reads the late record");
    Require(gotMisses == wantedMisses, "the miss ring returned " + std::to_string(gotMisses.size()) + " distinct keys, expected " + std::to_string(wantedMisses.size()));
    Require(!pcs.empty() && std::all_of(pcs.begin(), pcs.end(), [](std::uint32_t pc) { return pc == 0x111u || pc == 0x222u; }), "the miss ring records name other program counters than the two dispatches that missed");
    const auto totals = table.Totals();
    Require(totals.drained == 1u && totals.rings == 1u && totals.dropped == 0u, "the miss ring was not drained once after its batch");
    Require(totals.misses == gotMisses.size(), "the GPU appended " + std::to_string(totals.misses) + " ring records for " + std::to_string(gotMisses.size()) + " distinct missing keys: a key missed by many invocations and two dispatches must be reported once");
    Require(table.TakeMisses().empty(), "taken misses came back");
    std::cout << "miss ring round trip passed: " << totals.misses << " records, " << gotMisses.size() << " distinct keys, " << wantedWithLate.size() << " still missing after the late insert\n";

    const auto retiredSlot = fixture.slots[updated + 1u];
    const auto retiredIndex = Abi::SlotIndex(retiredSlot);
    const auto dropped = table.Retire([&](const Table::Key&, const DeviceImageValue& value) { return value.slot == retiredSlot; });
    Require(dropped == 2u && !table.Find(RecordKey(updated)).has_value() && !table.Find(RecordKey(updated + 1u)).has_value() && table.Find(RecordKey(updated + 2u)).has_value(), "retiring a heap slot did not drop exactly its two entries");
    const auto retireSerial = recorder.OpenSerial();
    table.Heap().Release(DescriptorHeap::Array::SampledImages, retiredSlot, retireSerial);
    Require(!table.Heap().Live(DescriptorHeap::Array::SampledImages, retiredSlot) && table.Heap().PendingReleases() == 1u, "a released slot is still live");
    bool staleRefused = false;
    try {
        table.Heap().Release(DescriptorHeap::Array::SampledImages, retiredSlot, retireSerial);
    } catch (const std::runtime_error&) {
        staleRefused = true;
    }
    Require(staleRefused, "a slot was released twice");
    const auto fresh = table.Heap().AllocateImage(DescriptorHeap::Array::SampledImages, fixture.images->View(9u), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Require(Abi::SlotIndex(fresh) != retiredIndex, "a slot was reused before its retire serial completed");
    table.Publish(recorder);
    Require(table.Heap().PendingReleases() == 1u, "a slot was freed while its batch was open");
    auto* slotWords = reinterpret_cast<std::uint32_t*>(fixture.slotList->Bytes().data());
    fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *results[0], 0x555u), Groups);
    recorder.Submit();
    recorder.Sync();
    auto retiredImage = updatedImage;
    auto retiredMode = updatedMode;
    retiredImage[updated] = ~0u;
    retiredImage[updated + 1u] = ~0u;
    failures = Mismatches(results[0]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, retiredImage, retiredMode); }, "lookup after a retire");
    Require(failures == 0u, std::to_string(failures) + " invocations read retired bindless entries");
    Require(table.Heap().Collect(recorder.CompletedThrough()) == 1u && table.Heap().PendingReleases() == 0u, "the retired slot was not freed after its batch completed");
    slotWords[updated + 1u] = retiredIndex;
    fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(1u, Iterations, *results[1], 0x666u), Groups);
    recorder.Submit();
    recorder.Sync();
    auto nulledSlotImage = imageOf;
    nulledSlotImage[updated + 1u] = ~0u;
    failures = Mismatches(results[1]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, nulledSlotImage, slotModes); }, "slot read after its release");
    Require(failures == 0u, "a freed heap slot still held its old descriptor");
    const auto reused = table.Heap().AllocateImage(DescriptorHeap::Array::SampledImages, fixture.images->View(11u), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Require(Abi::SlotIndex(reused) == retiredIndex && Abi::SlotGeneration(reused) == Abi::SlotGeneration(retiredSlot) + 1u, "the freed slot was not reused with the next generation");
    Require(table.Heap().Live(DescriptorHeap::Array::SampledImages, reused) && !table.Heap().Live(DescriptorHeap::Array::SampledImages, retiredSlot), "slot generations do not tell the reused slot from the retired one");
    std::cout << "retire and generation reuse tests passed\n";
    recorder.Sync();
    recorder.FlushDeferredReleases();
}

void dedupTests(const Device& device) {
    constexpr std::uint32_t Records = 256u;
    constexpr std::uint32_t Missing = 64u;
    constexpr std::uint32_t Ring = 16u;
    constexpr std::uint32_t Groups = 16u;
    constexpr std::uint32_t Threads = Groups * 64u;
    constexpr std::uint32_t Iterations = 4u;
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    Fixture fixture(device, Records, Missing, 1u << 10u, 0u, Ring);
    auto& table = *fixture.table;
    Require(table.RingCapacity() == Ring && table.SeenSlots() >= 64u, "the miss ring capacity was not taken from the constructor");
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    std::vector<std::uint32_t> imageOf(fixture.total, ~0u);
    std::vector<std::uint32_t> modeOf(fixture.total, 0u);
    for (std::uint32_t record = 0; record < Records; ++record) {
        imageOf[record] = record % DistinctImages;
        modeOf[record] = RecordMode(record);
    }
    std::set<std::uint32_t> missing;
    for (std::uint32_t id = 0; id < Threads; ++id) {
        for (std::uint32_t iteration = 0; iteration < Iterations; ++iteration) {
            const auto record = fixture.order[(id * 7u + iteration * 131u) & fixture.listMask];
            if (record >= Records) missing.insert(record);
        }
    }
    Require(missing.size() > 2u * Ring, "the test's index list touches too few missing records: " + std::to_string(missing.size()));
    auto result = std::make_shared<Buffer>(fixture.context, Threads * 4u, AddressUsage);
    const auto batch = [&](std::uint32_t pc) {
        const auto before = table.Totals();
        table.Publish(recorder);
        fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *result, pc), Groups);
        fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *result, pc + 1u), Groups);
        recorder.Submit();
        recorder.Sync();
        const auto after = table.Totals();
        return std::pair{after.misses - before.misses, after.dropped - before.dropped};
    };
    std::map<Table::Key, std::uint32_t> recordOf;
    for (const auto record : missing) recordOf.emplace(RecordKey(record), record);
    std::uint32_t batches = 0;
    for (auto remaining = missing.size(); remaining != 0u; ++batches) {
        Require(batches < missing.size(), "the miss storm did not converge");
        const auto [written, dropped] = batch(0x900u + batches * 2u);
        Require(written == remaining, "batch " + std::to_string(batches) + ": the GPU appended " + std::to_string(written) + " ring records for " + std::to_string(remaining) + " distinct missing keys (2 dispatches, about " + std::to_string(Threads * Iterations * 2u / fixture.total) + " reads per key)");
        Require(dropped == remaining - std::min<std::uint64_t>(remaining, Ring), "batch " + std::to_string(batches) + ": " + std::to_string(dropped) + " ring records dropped, expected " + std::to_string(remaining - std::min<std::uint64_t>(remaining, Ring)));
        const auto taken = table.TakeMisses();
        Require(taken.size() == std::min<std::uint64_t>(remaining, Ring), "batch " + std::to_string(batches) + ": took " + std::to_string(taken.size()) + " distinct keys from a ring of " + std::to_string(Ring));
        for (const auto& miss : taken) {
            const auto found = recordOf.find(miss.key);
            Require(found != recordOf.end(), "the miss ring returned a key no invocation missed");
            const auto record = found->second;
            recordOf.erase(found);
            const auto slot = table.Heap().AllocateImage(DescriptorHeap::Array::SampledImages, fixture.images->View(record % DistinctImages), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            Require(table.Insert(RecordKey(record), DeviceImageValue{slot, RecordMode(record), 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a reported key was not inserted");
            imageOf[record] = record % DistinctImages;
            modeOf[record] = RecordMode(record);
        }
        remaining -= taken.size();
    }
    Require(recordOf.empty() && batches == (missing.size() + Ring - 1u) / Ring, "every missing key was reported, but over " + std::to_string(batches) + " batches");
    const auto [quiet, quietDropped] = batch(0xa00u);
    Require(quiet == 0u && quietDropped == 0u, "a batch after the fill still missed");
    auto failures = Mismatches(result->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, imageOf, modeOf); }, "lookup after the miss storm was filled");
    Require(failures == 0u, std::to_string(failures) + " invocations read wrong results after the miss storm was filled");
    const auto victim = *missing.begin();
    Require(table.Retire([&](const Table::Key& key, const DeviceImageValue&) { return key == RecordKey(victim); }) == 1u, "the victim entry was not retired");
    auto [again, againDropped] = batch(0xb00u);
    Require(again == 1u && againDropped == 0u && table.TakeMisses().size() == 1u, "a key missed after its entry was retired was not reported again (" + std::to_string(again) + " records)");
    std::tie(again, againDropped) = batch(0xc00u);
    Require(again == 0u, "a key reported since the last clear of the reported-miss set was appended again");
    table.ForgetReportedMisses();
    std::tie(again, againDropped) = batch(0xd00u);
    Require(again == 1u && table.TakeMisses().size() == 1u, "a key still missing after ForgetReportedMisses was not reported again");
    imageOf[victim] = ~0u;
    failures = Mismatches(result->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, imageOf, modeOf); }, "lookup with the retired entry");
    Require(failures == 0u, std::to_string(failures) + " invocations read wrong results around the retired entry");
    std::cout << "miss dedup passed: " << missing.size() << " missing keys read about " << Threads * Iterations * 2u / fixture.total << " times per batch, each reported once per batch through a " << Ring << "-record ring over " << batches << " batches; retire and clear re-report; " << table.Totals().seenClears << " set clears\n";
    recorder.Sync();
    recorder.FlushDeferredReleases();
}

void growthTests(const Device& device) {
    constexpr std::uint32_t Present = 1000u;
    constexpr std::uint32_t Grown = 33000u;
    constexpr std::uint32_t Middle = 17000u;
    constexpr std::uint32_t Groups = 64u;
    constexpr std::uint32_t Threads = Groups * 64u;
    constexpr std::uint32_t Iterations = 4u;
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    Fixture fixture(device, Present, Grown - Present, 1u << 16u);
    auto& table = *fixture.table;
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    std::vector<std::uint32_t> imageOf(fixture.total, ~0u);
    std::vector<std::uint32_t> modeOf(fixture.total, 0u);
    for (std::uint32_t record = 0; record < Present; ++record) {
        imageOf[record] = record % DistinctImages;
        modeOf[record] = RecordMode(record);
    }
    std::array<std::shared_ptr<Buffer>, 3> results;
    for (auto& result : results) result = std::make_shared<Buffer>(fixture.context, Threads * 4u, AddressUsage);
    table.Publish(recorder);
    const auto bucketsAtFirst = table.Totals().buckets;
    fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *results[0], 0x711u), Groups);
    recorder.Submit();
    auto grownImage = imageOf;
    auto grownMode = modeOf;
    std::vector<std::uint32_t> middleImage;
    std::vector<std::uint32_t> middleMode;
    std::uint32_t bucketsAtMiddle = 0;
    for (std::uint32_t record = Present; record < Grown; ++record) {
        const auto slot = table.Heap().AllocateImage(DescriptorHeap::Array::SampledImages, fixture.images->View(record % DistinctImages), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        Require(table.Insert(RecordKey(record), DeviceImageValue{slot, RecordMode(record), 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a bindless record inserted while earlier batches were in flight was not inserted");
        grownImage[record] = record % DistinctImages;
        grownMode[record] = RecordMode(record);
        if (record + 1u != Middle) continue;
        table.Publish(recorder);
        bucketsAtMiddle = table.Totals().buckets;
        fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *results[1], 0x722u), Groups);
        recorder.Submit();
        middleImage = grownImage;
        middleMode = grownMode;
    }
    table.Publish(recorder);
    const auto bucketsAtEnd = table.Totals().buckets;
    fixture.pipeline->Record(recorder.Commands(), table.Heap(), fixture.MakePush(2u, Iterations, *results[2], 0x733u), Groups);
    recorder.Submit();
    recorder.Sync();
    Require(bucketsAtFirst < bucketsAtMiddle && bucketsAtMiddle < bucketsAtEnd, "the key table did not grow between the publications: " + std::to_string(bucketsAtFirst) + ", " + std::to_string(bucketsAtMiddle) + ", " + std::to_string(bucketsAtEnd) + " buckets");
    std::uint32_t failures = 0;
    failures += Mismatches(results[0]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, imageOf, modeOf); }, "lookup before the growth");
    failures += Mismatches(results[1]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, middleImage, middleMode); }, "lookup after a growth published while the first batch was in flight");
    failures += Mismatches(results[2]->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, false, grownImage, grownMode); }, "lookup after a second growth");
    Require(failures == 0u, std::to_string(failures) + " invocations read wrong bindless results across key table growth");
    std::cout << "key table growth across batches in flight passed: " << bucketsAtFirst << " -> " << bucketsAtMiddle << " -> " << bucketsAtEnd << " buckets\n";
    recorder.Sync();
    recorder.FlushDeferredReleases();
}

double Benchmark(const Device& device, std::uint32_t records, std::uint32_t workingSet) {
    constexpr std::uint32_t Groups = 16384u;
    constexpr std::uint32_t Threads = Groups * 64u;
    constexpr std::uint32_t Iterations = 16u;
    constexpr std::uint32_t Repeats = 7u;
    Fixture fixture(device, records, 0u, 1u << 20u, workingSet);
    const auto& context = fixture.context;
    Require(device.TimestampBits() != 0u, "the compute queue has no timestamps");
    VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryInfo.queryCount = 2;
    VkQueryPool queries = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(context.device, &queryInfo, nullptr, &queries), "vkCreateQueryPool");
    auto results = std::make_shared<Buffer>(context, Threads * 4u, AddressUsage);
    VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocation.commandPool = context.pool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocation, &commands), "vkAllocateCommandBuffers");
    {
        std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(context, device.Timeline());
        recorder.Activate();
        fixture.table->Publish(recorder);
        recorder.Submit();
        recorder.Sync();
        recorder.FlushDeferredReleases();
    }
    const std::array<const char*, 4> names{"direct binding", "nonuniform heap index (slot from a buffer)", "BDA T# load + hash + probe + nonuniform heap index", "same, key uniform per subgroup"};
    std::array<double, 4> best{};
    const auto period = static_cast<double>(device.Properties().limits.timestampPeriod);
    std::vector<std::uint32_t> imageOf(fixture.total, ~0u);
    std::vector<std::uint32_t> modeOf(fixture.total, 0u);
    std::vector<std::uint32_t> slotModes(fixture.total, 0u);
    for (std::uint32_t record = 0; record < records; ++record) {
        imageOf[record] = record % DistinctImages;
        modeOf[record] = RecordMode(record);
    }
    for (std::uint32_t mode = 0; mode < names.size(); ++mode) {
        best[mode] = 1e30;
        for (std::uint32_t repeat = 0; repeat < Repeats; ++repeat) {
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Check(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin), "vkBeginCommandBuffer");
            context.Function<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(commands, queries, 0, 2);
            context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
            fixture.pipeline->Record(commands, fixture.table->Heap(), fixture.MakePush(mode, Iterations, *results, 0x777u), Groups);
            context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
            AgcDriver::Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            Check(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &commands;
            Check(context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit benchmark");
            Check(context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue), "vkQueueWaitIdle benchmark");
            std::array<std::uint64_t, 2> stamps{};
            Check(context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(context.device, queries, 0, 2, sizeof(stamps), stamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "vkGetQueryPoolResults");
            const auto nanoseconds = static_cast<double>(stamps[1] - stamps[0]) * period;
            best[mode] = std::min(best[mode], nanoseconds);
        }
        const auto samples = static_cast<double>(Threads) * Iterations;
        std::uint32_t mismatches = 0;
        if (mode < 3u) mismatches = Mismatches(results->Bytes(), Threads, [&](std::uint32_t id) { return fixture.Expected(id, Iterations, mode == 0u, imageOf, mode == 1u ? slotModes : modeOf); }, names[mode]);
        std::printf("mode %u %-52s %9.3f ms  %7.4f ns/sample  (%u records, %u read, %s)\n", mode, names[mode], best[mode] / 1e6, best[mode] / samples, records, workingSet != 0u ? std::min(workingSet, records) : records, mode == 3u ? "not checked" : mismatches == 0u ? "results exact" : "RESULTS WRONG");
        Require(mismatches == 0u, std::string(names[mode]) + " read wrong results");
    }
    context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 1, &commands);
    context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, queries, nullptr);
    device.WaitIdle();
    return (best[2] - best[0]) / (static_cast<double>(Threads) * Iterations);
}

int flagTests(bool on) {
    const auto device = [] {
        try {
            return std::make_unique<AgcDriver::VulkanDevice>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::cout << "skipped, no usable Vulkan device: " << error.what() << '\n';
            return std::unique_ptr<AgcDriver::VulkanDevice>{};
        }
    }();
    if (device == nullptr) return Skipped;
    Require(AgcDriver::Graphics::BindlessImagesRequested() == on, "APS5_BINDLESS_IMAGES does not match the test mode");
    Require(device->BindlessImagesEnabled() == on, on ? "APS5_BINDLESS_IMAGES=1 did not create the bindless table on a device that supports it" : "the bindless table exists without APS5_BINDLESS_IMAGES=1");
    std::cout << "bindless flag " << (on ? "on" : "off") << ": table " << (device->BindlessImagesEnabled() ? "created" : "absent") << " on " << device->DeviceName() << '\n';
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc >= 2 ? argv[1] : "";
        if (mode == "--flag-off") return flagTests(false);
        if (mode == "--flag-on") return flagTests(true);
        keyTests();
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::cout << "skipped, no usable bindless Vulkan device: " << error.what() << '\n';
            return Skipped;
        }
        if (mode == "--benchmark") {
            const auto records = argc >= 3 ? static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10)) : 33000u;
            const auto workingSet = argc >= 4 ? static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10)) : 0u;
            const auto extra = Benchmark(*device, records, workingSet);
            std::printf("lookup overhead over a direct binding: %.4f ns/sample\n", extra);
            return 0;
        }
        gpuTests(*device);
        dedupTests(*device);
        growthTests(*device);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bindless table test failed: " << error.what() << '\n';
        return 1;
    }
}
