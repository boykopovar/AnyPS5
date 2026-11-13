#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessResolvers.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessTable.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureService.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "BindlessImageAbi.hpp"
#include "BindlessLookup_spv.h"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
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
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::BindlessMemory;
using AgcDriver::Graphics::BindlessPrepared;
using AgcDriver::Graphics::BindlessResolution;
using AgcDriver::Graphics::BindlessResolver;
using AgcDriver::Graphics::BindlessTable;
using AgcDriver::Graphics::BindlessTableRegistration;
using AgcDriver::Graphics::BindlessUse;
using AgcDriver::Graphics::Buffer;
using AgcDriver::Graphics::Check;
using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::DescriptorHeap;
using AgcDriver::Graphics::DeviceImageKeys;
using AgcDriver::Graphics::DeviceImageValue;
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::TextureService;
namespace Abi = ShaderRecompiler::BindlessImageAbi;
namespace Table = ShaderRecompiler::DeviceImageTableAbi;

constexpr int Skipped = 77;
constexpr std::uint32_t Side = 64u;
constexpr std::uint32_t DistinctImages = 256u;
constexpr std::uint32_t Remapped = 128u;
constexpr std::uint64_t InterfaceId = 0x0123456789abcdefull;
constexpr std::uint32_t RefusedMode = 0x1ffu;
constexpr std::uint32_t PermanentMode = 0x1feu;
constexpr VkBufferUsageFlags AddressUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

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
            enabled12.storageBuffer8BitAccess = supported12.storageBuffer8BitAccess;
            enabled12.uniformAndStorageBuffer8BitAccess = supported12.uniformAndStorageBuffer8BitAccess;
            enabled12.shaderInt8 = supported12.shaderInt8;
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            enabled.shaderInt16 = supported.features.shaderInt16;
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
            std::cout << "Vulkan device: " << properties.deviceName << '\n';
        } catch (...) {
            release();
            throw;
        }
    }

    ~Device() { release(); }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    const Context& GetContext() const { return context; }
    bool Timeline() const { return timeline; }

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

class HostMemory final : public BindlessMemory {
public:
    bool Read(std::uint64_t address, std::span<std::uint32_t> words) override {
        std::lock_guard lock(mutex);
        reads += words.size();
        if (!mappedLocked(address, words.size() * 4u)) return false;
        std::memcpy(words.data(), reinterpret_cast<const void*>(address), words.size() * 4u);
        return true;
    }
    std::uint64_t Collect(std::uint64_t, std::size_t) override {
        std::lock_guard lock(mutex);
        return generation;
    }
    bool Unchanged(std::uint64_t address, std::size_t bytes, std::uint64_t since) override {
        std::lock_guard lock(mutex);
        if (since == 0) return false;
        for (auto page = address / TextureService::PageBytes; page <= (address + bytes - 1u) / TextureService::PageBytes; ++page) {
            const auto found = stamps.find(page);
            if (found != stamps.end() && found->second > since) return false;
        }
        return true;
    }
    std::uint64_t MappingSerial() override {
        std::lock_guard lock(mutex);
        return mapping;
    }
    bool Mapped(std::uint64_t address, std::size_t bytes) override {
        std::lock_guard lock(mutex);
        return mappedLocked(address, bytes);
    }
    void Write(std::uint64_t address, std::span<const std::uint32_t> words) {
        std::memcpy(reinterpret_cast<void*>(address), words.data(), words.size() * 4u);
        Touch(address, words.size() * 4u);
    }
    void Touch(std::uint64_t address, std::size_t bytes) {
        std::lock_guard lock(mutex);
        ++generation;
        for (auto page = address / TextureService::PageBytes; page <= (address + bytes - 1u) / TextureService::PageBytes; ++page) stamps[page] = generation;
    }
    void Unmap(std::uint64_t address, std::size_t bytes) {
        std::lock_guard lock(mutex);
        holes.emplace_back(address, address + bytes);
        ++mapping;
    }
    void Remap() {
        std::lock_guard lock(mutex);
        holes.clear();
        ++mapping;
    }
    std::uint64_t Reads() {
        std::lock_guard lock(mutex);
        return reads;
    }

private:
    bool mappedLocked(std::uint64_t address, std::size_t bytes) const {
        return std::none_of(holes.begin(), holes.end(), [&](const auto& hole) { return address < hole.second && hole.first < address + bytes; });
    }

    std::mutex mutex;
    std::map<std::uint64_t, std::uint64_t> stamps;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> holes;
    std::uint64_t generation = 1;
    std::uint64_t mapping = 1;
    std::uint64_t reads = 0;
};

class TestResolver final : public BindlessResolver {
public:
    TestResolver(const Images& images, HostMemory& memory, std::uint64_t surfaces) : images(images), memory(memory), surfaces(surfaces) {
        for (std::uint32_t image = 0; image < DistinctImages; ++image) owners.push_back(std::make_shared<std::uint32_t>(image));
    }
    BindlessPrepared Prepare(const DeviceImageKeys::Key& key) override {
        ++prepares;
        const auto image = key[0] & 0xffffu;
        Require(image < DistinctImages, "the test T# names no test image");
        if (key[2] == RefusedMode) throw std::runtime_error("AGC graphics: the test resolver refuses this T#");
        if (key[2] == PermanentMode) throw AgcDriver::Graphics::BindlessKeyRefusal("AGC graphics: the test resolver refuses this T# for any mapping");
        BindlessPrepared prepared;
        prepared.address = Surface(image);
        prepared.bytes = TextureService::PageBytes;
        if (!memory.Mapped(prepared.address, static_cast<std::size_t>(prepared.bytes))) throw std::runtime_error("AGC graphics: the test surface is not mapped");
        prepared.generation = memory.Collect(prepared.address, static_cast<std::size_t>(prepared.bytes));
        prepared.state = std::make_shared<std::uint32_t>(image);
        return prepared;
    }
    BindlessResolution Commit(const BindlessPrepared& prepared) override {
        ++commits;
        auto image = *std::static_pointer_cast<std::uint32_t>(prepared.state);
        if (reused.contains(image)) image = (image + Remapped) % DistinctImages;
        BindlessResolution resolution;
        resolution.value = DeviceImageValue{0u, prepared.key[2] & 0xffu, 0u, 0u};
        resolution.owner = owners[image];
        resolution.view = images.View(image);
        resolution.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        resolution.address = prepared.address;
        resolution.bytes = prepared.bytes;
        resolution.generation = prepared.generation;
        resolution.dcc = dcc.contains(image);
        resolution.perUse = resolution.dcc;
        return resolution;
    }
    bool Current(const BindlessResolution&) override { return true; }
    bool RefreshGeneration(BindlessPrepared& prepared) override {
        ++refreshes;
        if (prepared.settled.has_value()) return true;
        if (prepared.state == nullptr) return false;
        prepared.generation = memory.Collect(prepared.address, static_cast<std::size_t>(prepared.bytes));
        return true;
    }
    std::uint64_t Surface(std::uint32_t image) const { return surfaces + static_cast<std::uint64_t>(image) * TextureService::PageBytes; }

    std::atomic<std::uint64_t> prepares{0};
    std::atomic<std::uint64_t> commits{0};
    std::atomic<std::uint64_t> refreshes{0};
    std::set<std::uint32_t> reused;
    std::set<std::uint32_t> dcc;

private:
    const Images& images;
    HostMemory& memory;
    std::uint64_t surfaces;
    std::vector<std::shared_ptr<std::uint32_t>> owners;
};

std::array<std::uint32_t, Table::DescriptorWords> Descriptor(std::uint32_t image, std::uint32_t mode, std::uint32_t salt) {
    return {image, 0x107u, mode, salt * 0x9e3779b9u + 1u, salt, 0x00012300u | ((salt * 0x2f1u) & Abi::FeedbackMask5), 0x4400u | (salt & 0xffu), 0u};
}

struct Record {
    std::uint32_t image = 0;
    std::uint32_t mode = 0;
    std::uint32_t salt = 0;
    bool present = true;
};

struct Fixture {
    static constexpr std::uint32_t Records = 4096u;
    static constexpr std::uint32_t Groups = Records / 64u;

    Fixture(const Device& device, std::uint32_t buckets = DeviceImageKeys::InitialBuckets, std::uint32_t maxBuckets = DeviceImageKeys::MaxBuckets, std::uint32_t ringCapacity = BindlessTable::DefaultRingCapacity) : context(device.GetContext()) {
        const auto capacities = DescriptorHeap::DeviceCapacities(context);
        table = std::make_unique<BindlessTable>(context, DescriptorHeap::Capacities{std::min(capacities.samplers, 64u), std::min(capacities.sampledImages, 16384u), std::min(capacities.storageImages, 64u)}, buckets, maxBuckets, ringCapacity);
        images = std::make_unique<Images>(context, DistinctImages);
        pipeline = std::make_unique<LookupPipeline>(context, table->Heap(), images->View(0));
        descriptors = std::make_shared<Buffer>(context, static_cast<std::size_t>(Records) * 32u, AddressUsage);
        slotList = std::make_shared<Buffer>(context, static_cast<std::size_t>(Records) * 4u, AddressUsage);
        indices = std::make_shared<Buffer>(context, static_cast<std::size_t>(Records) * 4u, AddressUsage);
        results = std::make_shared<Buffer>(context, static_cast<std::size_t>(Records) * 4u, AddressUsage);
        std::memset(slotList->Bytes().data(), 0, slotList->Bytes().size());
        auto* order = reinterpret_cast<std::uint32_t*>(indices->Bytes().data());
        for (std::uint32_t index = 0; index < Records; ++index) order[index] = index;
        surfaceMemory.assign(static_cast<std::size_t>(DistinctImages + 1u) * TextureService::PageBytes, std::byte{0});
        surfaces = (reinterpret_cast<std::uint64_t>(surfaceMemory.data()) + TextureService::PageBytes - 1u) & ~(TextureService::PageBytes - 1u);
        memory = std::make_shared<HostMemory>();
        resolver = std::make_shared<TestResolver>(*images, *memory, surfaces);
        records.resize(Records);
        for (std::uint32_t record = 0; record < Records; ++record) {
            records[record] = {record % DistinctImages, (record * 7u) & 0xffu, record, true};
            write(record, false);
        }
    }

    std::uint64_t Base() const { return reinterpret_cast<std::uint64_t>(descriptors->Bytes().data()); }
    std::uint64_t Address(std::uint32_t record) const { return Base() + static_cast<std::uint64_t>(record) * 32u; }
    BindlessTableRegistration Registration(std::uint32_t first = 0, std::uint32_t count = Records) const { return {Address(first), count, 32u, {BindlessUse{InterfaceId, 0u, {}}}}; }
    Table::Key Key(std::uint32_t record) const {
        const auto words = Descriptor(records[record].image, records[record].mode, records[record].salt);
        return Abi::MakeKey(words, InterfaceId, 0u);
    }

    void write(std::uint32_t record, bool tracked) {
        const auto words = Descriptor(records[record].image, records[record].mode, records[record].salt);
        if (tracked) memory->Write(Address(record), words);
        else std::memcpy(reinterpret_cast<void*>(Address(record)), words.data(), sizeof(words));
    }

    void Dispatch(Recorder& recorder, std::uint32_t pc) {
        const Push push{table->HeaderAddress(), descriptors->DeviceAddress(), indices->DeviceAddress(), results->DeviceAddress(), slotList->DeviceAddress(), static_cast<std::uint32_t>(InterfaceId), static_cast<std::uint32_t>(InterfaceId >> 32u), 2u, 1u, mask, pc};
        pipeline->Record(recorder.Commands(), table->Heap(), push, Groups);
    }

    std::uint32_t Check(std::string_view what, const std::function<bool(std::uint32_t)>& served) const {
        const auto* words = reinterpret_cast<const std::uint32_t*>(results->Bytes().data());
        std::uint32_t mismatches = 0;
        for (std::uint32_t id = 0; id < Records; ++id) {
            const auto record = (id * 7u) & mask;
            const auto& item = records[record];
            const auto image = resolver->reused.contains(item.image) ? (item.image + Remapped) % DistinctImages : item.image;
            const auto want = !served(record) || !item.present || item.mode == RefusedMode || item.mode == PermanentMode ? 0u : Images::Texel(image, record & 63u, (record >> 6u) & 63u) + (item.mode & 0xffu);
            if (words[id] == want) continue;
            if (mismatches < 4u) std::cerr << what << ": record " << record << " read " << words[id] << ", expected " << want << '\n';
            ++mismatches;
        }
        return mismatches;
    }

    Context context;
    std::unique_ptr<BindlessTable> table;
    std::unique_ptr<Images> images;
    std::unique_ptr<LookupPipeline> pipeline;
    std::shared_ptr<Buffer> descriptors;
    std::shared_ptr<Buffer> slotList;
    std::shared_ptr<Buffer> indices;
    std::shared_ptr<Buffer> results;
    std::vector<std::byte> surfaceMemory;
    std::uint64_t surfaces = 0;
    std::shared_ptr<HostMemory> memory;
    std::shared_ptr<TestResolver> resolver;
    std::vector<Record> records;
    std::uint32_t mask = Records - 1u;
};

std::uint32_t DistinctPages(std::uint64_t first, std::uint64_t last) {
    return static_cast<std::uint32_t>(last / TextureService::PageBytes - first / TextureService::PageBytes + 1u);
}

void Advance(Recorder& recorder) {
    static_cast<void>(recorder.Commands());
    recorder.Submit();
}

void Present(TextureService& service, Recorder& recorder) {
    const auto serial = service.PresentSerial(recorder);
    if (serial != 0) recorder.Sync();
    service.AfterPresentWait(recorder, 0u);
}

void serviceTests(const Device& device) {
    Fixture fixture(device);
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    constexpr std::uint32_t Refused = 3000u;
    for (std::uint32_t record = Refused; record < Refused + 8u; ++record) {
        fixture.records[record].mode = record < Refused + 4u ? RefusedMode : PermanentMode;
        fixture.write(record, false);
    }
    fixture.records[17].present = false;
    std::memset(reinterpret_cast<void*>(fixture.Address(17)), 0, 32u);
    TextureService service(*fixture.table, fixture.resolver, fixture.memory, TextureService::Options{});
    const auto id = service.Register(fixture.Registration());
    Require(service.Register(fixture.Registration()) == id, "registering the same table twice made two tables");
    Require(service.WaitPrimed(id, 60000u), "the service worker did not prime the table off the GPU lock");
    const auto primed = service.Totals();
    Require(primed.primed == 1u && primed.workerPrepared == 1u && primed.workerCommitted == Fixture::Records && primed.created == Fixture::Records && primed.refused == 8u && primed.refusals[static_cast<std::size_t>(AgcDriver::Graphics::BindlessRefusalKind::Other)] == 8u, "the worker primer did not create every table entry off the GPU lock: created " + std::to_string(primed.created) + ", worker committed " + std::to_string(primed.workerCommitted) + ", refused " + std::to_string(primed.refused));
    Require(primed.imageSlots == DistinctImages, "the heap slots were not shared per image view: " + std::to_string(primed.imageSlots));
    Require(primed.tables == 1u && primed.tableHostBytes >= Fixture::Records * Table::DescriptorWords * sizeof(std::uint32_t) && primed.entryHostBytes >= primed.entries * sizeof(DeviceImageKeys::Key), "the host bytes of the primed table and its entries are not counted: " + std::to_string(primed.tableHostBytes));
    Require(primed.rejections != 0u && primed.rejectionHostBytes != 0u, "the host bytes of the refusal reasons are not counted");
    const std::array<std::uint32_t, Table::DescriptorWords> nullWords{};
    const auto nullValue = service.Find(Abi::MakeKey(nullWords, InterfaceId, 0u));
    Require(nullValue.has_value() && nullValue->slot == 0u && nullValue->fault == 0u && nullValue->mode == 0u, "a null T# record is not a null-slot entry");
    const auto refusedValue = service.Find(fixture.Key(Refused));
    Require(refusedValue.has_value() && refusedValue->fault != 0u && refusedValue->slot == 0u && service.Rejection(refusedValue->fault).value_or("").find("refuses this T#") != std::string::npos, "a refused T# is not a cached fault entry naming its reason");
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint64_t, 1> served{id};
    const auto all = [](std::uint32_t) { return true; };
    service.Serve(served, recorder);
    fixture.Dispatch(recorder, 0x100u);
    recorder.Submit();
    recorder.Sync();
    Require(fixture.Check("primed table", all) == 0u, "the primed table read wrong texels");
    Present(service, recorder);
    Require(service.Totals().misses == 0u, "a fully primed table missed");
    std::cout << "primer: " << primed.primedRecords << " records, " << primed.created << " entries, " << primed.imageSlots << " image slots, primed by the worker without the GPU lock; dispatch exact, 0 misses\n";

    const auto beforeWrite = service.Totals();
    for (std::uint32_t record = 1000u; record < 1010u; ++record) {
        fixture.records[record] = {200u + (record - 1000u), 0x30u + (record - 1000u), 50000u + record, true};
        fixture.write(record, true);
    }
    service.Serve(served, recorder);
    fixture.Dispatch(recorder, 0x200u);
    recorder.Submit();
    recorder.Sync();
    const auto afterWrite = service.Totals();
    const auto pages = DistinctPages(fixture.Address(1000u), fixture.Address(1009u) + 31u);
    Require(afterWrite.recordsChanged - beforeWrite.recordsChanged == 10u && afterWrite.pagesChanged - beforeWrite.pagesChanged == pages && afterWrite.created - beforeWrite.created == 10u, "a CPU write of 10 records was not re-decoded from its pages alone: " + std::to_string(afterWrite.recordsChanged - beforeWrite.recordsChanged) + " records, " + std::to_string(afterWrite.pagesChanged - beforeWrite.pagesChanged) + " pages (expected " + std::to_string(pages) + "), " + std::to_string(afterWrite.created - beforeWrite.created) + " created");
    Require(afterWrite.recordsRead - beforeWrite.recordsRead <= static_cast<std::uint64_t>(pages) * (TextureService::PageBytes / 32u + 1u), "the page re-decode read records outside the written pages");
    Require(fixture.Check("CPU-written table", all) == 0u, "a CPU write between dispatches was not seen by the next dispatch");
    Present(service, recorder);
    Require(service.Totals().misses == 0u, "a CPU-written table missed");
    std::cout << "CPU write between dispatches: 10 records re-decoded from " << pages << " page(s), next dispatch exact, 0 misses\n";

    constexpr std::uint32_t GpuRecord = 2000u;
    const auto gpuWords = Descriptor(77u, 9u, 70000u);
    service.Serve(served, recorder);
    const auto commands = recorder.Commands();
    AgcDriver::Graphics::RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    fixture.context.Function<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(commands, fixture.descriptors->Handle(), static_cast<VkDeviceSize>(GpuRecord) * 32u, sizeof(gpuWords), gpuWords.data());
    AgcDriver::Graphics::RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    fixture.Dispatch(recorder, 0x300u);
    const auto beforeMiss = service.Totals();
    Present(service, recorder);
    const auto line = service.TakeFrameLine(0u);
    fixture.records[GpuRecord] = {77u, 9u, 70000u, true};
    Require(fixture.Check("GPU-written record before the drain", [&](std::uint32_t record) { return record != GpuRecord; }) == 0u, "the dispatch that wrote its own table read something other than a miss for the new T#");
    const auto afterMiss = service.Totals();
    Require(afterMiss.misses - beforeMiss.misses == 1u && afterMiss.created - beforeMiss.created == 1u && service.Find(Abi::MakeKey(gpuWords, InterfaceId, 0u)).has_value(), "the miss ring did not bring the GPU-written T# into the table at the present");
    Require(line.find("misses 1 ") != std::string::npos, "the frame line does not report the miss: " + line);
    service.Serve(served, recorder);
    fixture.Dispatch(recorder, 0x301u);
    recorder.Submit();
    recorder.Sync();
    Require(fixture.Check("GPU-written record after the drain", all) == 0u, "the GPU-written T# was not served after its miss was drained");
    Present(service, recorder);
    Require(service.Totals().misses == afterMiss.misses, "the drained GPU-written T# missed again");
    std::cout << "GPU write (miss path): 1 miss drained at the present, next dispatch exact\n" << line << '\n';

    const auto beforeReuse = service.Totals();
    fixture.memory->Touch(fixture.resolver->Surface(5u), TextureService::PageBytes);
    fixture.resolver->reused.insert(5u);
    service.Revalidate(recorder);
    const auto afterReuse = service.Totals();
    const auto usingFive = static_cast<std::uint64_t>(std::count_if(fixture.records.begin(), fixture.records.end(), [](const Record& item) { return item.present && item.image == 5u && item.mode != RefusedMode && item.mode != PermanentMode; }));
    Require(afterReuse.stale - beforeReuse.stale == usingFive, "rewriting a surface did not re-resolve exactly its " + std::to_string(usingFive) + " entries: " + std::to_string(afterReuse.stale - beforeReuse.stale));
    service.Serve(served, recorder);
    fixture.Dispatch(recorder, 0x400u);
    recorder.Submit();
    recorder.Sync();
    Require(fixture.Check("reused surface", all) == 0u, "entries over reused surface memory were not re-resolved");
    std::cout << "surface reuse: " << usingFive << " entries re-resolved to the new image\n";

    const auto usingImage = [&](std::uint32_t image) {
        return static_cast<std::uint64_t>(std::count_if(fixture.records.begin(), fixture.records.end(), [&](const Record& item) { return item.present && item.image == image && item.mode != RefusedMode && item.mode != PermanentMode; }));
    };
    {
        const auto before = service.Totals();
        fixture.memory->Touch(fixture.resolver->Surface(7u), TextureService::PageBytes);
        fixture.resolver->reused.insert(7u);
        gpu.unlock();
        const auto preparesBefore = fixture.resolver->prepares.load();
        service.PrepareRevalidation();
        const auto preparedOff = fixture.resolver->prepares.load() - preparesBefore;
        gpu.lock();
        service.Revalidate(recorder);
        const auto after = service.Totals();
        const auto expected = usingImage(7u);
        Require(after.stale - before.stale == expected, "early revalidation changed the stale count: " + std::to_string(after.stale - before.stale) + ", expected " + std::to_string(expected));
        Require(preparedOff == expected && fixture.resolver->prepares.load() - preparesBefore == expected, "the stale entries were not prepared off the GPU lock exactly once: " + std::to_string(preparedOff) + " off, " + std::to_string(fixture.resolver->prepares.load() - preparesBefore) + " in total");
        Require(after.earlyUsed - before.earlyUsed == expected && after.earlyDropped == before.earlyDropped, "the early preparations were not all used");
        service.Serve(served, recorder);
        fixture.Dispatch(recorder, 0x401u);
        recorder.Submit();
        recorder.Sync();
        Require(fixture.Check("early revalidation", all) == 0u, "entries committed from an early preparation read wrong texels");
        std::cout << "early revalidation: " << expected << " stale entries prepared off the GPU lock, committed exactly\n";
    }
    {
        const auto before = service.Totals();
        fixture.memory->Touch(fixture.resolver->Surface(8u), TextureService::PageBytes);
        fixture.resolver->reused.insert(8u);
        gpu.unlock();
        service.PrepareRevalidation();
        gpu.lock();
        fixture.memory->Remap();
        const auto preparesBefore = fixture.resolver->prepares.load();
        service.Revalidate(recorder);
        const auto after = service.Totals();
        const auto expected = usingImage(8u);
        Require(after.stale - before.stale == expected && after.earlyUsed == before.earlyUsed, "early preparations were used across a mapping change");
        Require(fixture.resolver->prepares.load() - preparesBefore == expected && after.earlyDropped - before.earlyDropped == expected, "the stale entries were not prepared again under the lock after a mapping change");
        service.Serve(served, recorder);
        fixture.Dispatch(recorder, 0x402u);
        recorder.Submit();
        recorder.Sync();
        Require(fixture.Check("early revalidation after a mapping change", all) == 0u, "entries prepared again after a mapping change read wrong texels");
        std::cout << "early revalidation: dropped across a mapping change, " << expected << " entries prepared again under the lock\n";
    }

    const auto beforeUnmap = service.Totals();
    fixture.memory->Unmap(fixture.resolver->Surface(6u), TextureService::PageBytes);
    service.Revalidate(recorder);
    const auto afterUnmap = service.Totals();
    const auto usingSix = static_cast<std::uint64_t>(std::count_if(fixture.records.begin(), fixture.records.end(), [](const Record& item) { return item.present && item.image == 6u && item.mode != RefusedMode && item.mode != PermanentMode; }));
    Require(afterUnmap.unmapped - beforeUnmap.unmapped == usingSix && afterUnmap.retired - beforeUnmap.retired >= usingSix && !service.Find(fixture.Key(6u)).has_value(), "unmapping a surface did not retire its " + std::to_string(usingSix) + " entries");
    Require(fixture.table->Heap().PendingReleases() != 0u, "the unmapped entries' slot was not released through the retire serial");
    Require(!service.Find(fixture.Key(Refused)).has_value(), "a refusal that may depend on the mapping was not retried after the mapping changed");
    Require(afterUnmap.keptRefusals - beforeUnmap.keptRefusals == 4u && service.Find(fixture.Key(Refused + 4u)).value_or(DeviceImageValue{}).fault != 0u, "a refusal fixed by its T# was retired by a mapping change: kept " + std::to_string(afterUnmap.keptRefusals - beforeUnmap.keptRefusals));
    service.Serve(served, recorder);
    fixture.Dispatch(recorder, 0x500u);
    recorder.Submit();
    recorder.Sync();
    Require(fixture.Check("unmapped surface", [](std::uint32_t record) { return record % DistinctImages != 6u; }) == 0u, "entries over an unmapped surface still served their old image");
    Present(service, recorder);
    const auto refused = service.Find(fixture.Key(6u));
    Require(refused.has_value() && refused->fault != 0u && service.Totals().misses == afterMiss.misses, "a T# over an unmapped surface did not become a cached refusal");
    fixture.memory->Remap();
    service.Revalidate(recorder);
    service.Serve(served, recorder);
    fixture.Dispatch(recorder, 0x501u);
    recorder.Submit();
    recorder.Sync();
    Require(fixture.Check("remapped surface", all) == 0u, "entries over a remapped surface were not resolved again");
    std::cout << "unmap/remap: " << usingSix << " entries retired on unmap, refused while unmapped, served again after the remap\n";
    recorder.Sync();
    recorder.FlushDeferredReleases();
}

void lazyTests(const Device& device) {
    Fixture fixture(device);
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    TextureService::Options options;
    options.lazy = true;
    options.worker = false;
    TextureService service(*fixture.table, fixture.resolver, fixture.memory, options);
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint64_t, 1> served{service.Register(fixture.Registration())};
    service.Serve(served, recorder);
    Require(service.Totals().created == 0u && fixture.memory->Reads() == 0u, "the lazy mode read or primed the table");
    fixture.Dispatch(recorder, 0x600u);
    Present(service, recorder);
    Require(fixture.Check("lazy first touch", [](std::uint32_t) { return false; }) == 0u, "a lazy first touch read anything but misses");
    const auto first = service.Totals();
    Require(first.created == first.misses && first.misses == std::min<std::uint64_t>(Fixture::Records, fixture.table->RingCapacity()), "the lazy mode did not create every distinct key its ring caught: " + std::to_string(first.created) + " created, " + std::to_string(first.misses) + " misses");
    std::uint32_t frames = 1;
    while (service.Totals().created < Fixture::Records && frames < 16u) {
        service.Serve(served, recorder);
        fixture.Dispatch(recorder, 0x600u + frames);
        Present(service, recorder);
        ++frames;
    }
    const auto misses = service.Totals().misses;
    service.Serve(served, recorder);
    fixture.Dispatch(recorder, 0x700u);
    Present(service, recorder);
    Require(service.Totals().created == Fixture::Records && service.Totals().misses == misses && fixture.Check("lazy after convergence", [](std::uint32_t) { return true; }) == 0u, "the lazy mode did not converge to an exact frame");
    std::cout << "lazy first touch: " << misses << " misses over " << frames << " frames (ring capacity " << fixture.table->RingCapacity() << "), then exact with 0 misses\n";
}

void stormTests(const Device& device) {
    {
        Fixture fixture(device, DeviceImageKeys::InitialBuckets, DeviceImageKeys::MaxBuckets, 64u);
        fixture.mask = 15u;
        Recorder recorder(fixture.context, device.Timeline());
        recorder.Activate();
        TextureService::Options options;
        options.lazy = true;
        options.worker = false;
        TextureService service(*fixture.table, fixture.resolver, fixture.memory, options);
        std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
        const std::array<std::uint64_t, 1> served{service.Register(fixture.Registration())};
        const auto before = fixture.table->Totals();
        service.Serve(served, recorder);
        fixture.Dispatch(recorder, 0x800u);
        fixture.Dispatch(recorder, 0x801u);
        Present(service, recorder);
        const auto ring = fixture.table->Totals();
        Require(ring.misses - before.misses == 16u && ring.dropped == before.dropped, "few keys: " + std::to_string(Fixture::Records * 2u) + " invocations missing 16 keys appended " + std::to_string(ring.misses - before.misses) + " ring records (" + std::to_string(ring.dropped - before.dropped) + " dropped), expected each key once");
        Require(service.Totals().created == 16u && service.Totals().misses == 16u, "few keys: the present did not create the 16 missed keys");
        Require(fixture.Check("few keys first touch", [](std::uint32_t) { return false; }) == 0u, "few keys: a first touch read anything but misses");
        service.Serve(served, recorder);
        fixture.Dispatch(recorder, 0x802u);
        Present(service, recorder);
        Require(fixture.Check("few keys after the fill", [](std::uint32_t) { return true; }) == 0u && fixture.table->Totals().misses == ring.misses, "few keys: the frame after the fill was not exact");
        std::cout << "lazy few keys: " << Fixture::Records * 2u << " invocations missing 16 keys -> 16 ring records, exact on the next frame\n";
    }
    constexpr std::uint32_t Ring = 256u;
    Fixture fixture(device, DeviceImageKeys::InitialBuckets, DeviceImageKeys::MaxBuckets, Ring);
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    TextureService::Options options;
    options.lazy = true;
    options.worker = false;
    TextureService service(*fixture.table, fixture.resolver, fixture.memory, options);
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint64_t, 1> served{service.Register(fixture.Registration())};
    std::uint64_t remaining = Fixture::Records;
    std::uint32_t frames = 0;
    while (remaining != 0u) {
        Require(frames < 2u * Fixture::Records / Ring, "the miss storm did not converge: " + std::to_string(remaining) + " keys left after " + std::to_string(frames) + " frames");
        const auto before = fixture.table->Totals();
        const auto createdBefore = service.Totals().created;
        service.Serve(served, recorder);
        fixture.Dispatch(recorder, 0x900u + frames * 2u);
        fixture.Dispatch(recorder, 0x901u + frames * 2u);
        Present(service, recorder);
        const auto after = fixture.table->Totals();
        const auto created = service.Totals().created - createdBefore;
        const auto caught = std::min<std::uint64_t>(remaining, Ring);
        Require(after.misses - before.misses == remaining, "storm frame " + std::to_string(frames) + ": " + std::to_string(after.misses - before.misses) + " ring records for " + std::to_string(remaining) + " missing keys read by two dispatches, expected each key once");
        Require(after.dropped - before.dropped == remaining - caught && created == caught, "storm frame " + std::to_string(frames) + ": " + std::to_string(created) + " created and " + std::to_string(after.dropped - before.dropped) + " dropped, expected " + std::to_string(caught) + " and " + std::to_string(remaining - caught));
        remaining -= created;
        ++frames;
    }
    Require(frames == Fixture::Records / Ring, "the storm took " + std::to_string(frames) + " frames, expected " + std::to_string(Fixture::Records / Ring));
    const auto misses = fixture.table->Totals().misses;
    service.Serve(served, recorder);
    fixture.Dispatch(recorder, 0xa00u);
    Present(service, recorder);
    Require(service.Totals().created == Fixture::Records && fixture.table->Totals().misses == misses && fixture.Check("storm after the fill", [](std::uint32_t) { return true; }) == 0u, "the frame after the miss storm was filled was not exact");
    std::cout << "lazy miss storm: " << Fixture::Records << " keys through a " << Ring << "-record ring, each reported once per frame, created over " << frames << " frames, then exact\n";
}

void fullTests(const Device& device) {
    Fixture fixture(device, 64u, 128u);
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    TextureService::Options options;
    options.worker = false;
    TextureService service(*fixture.table, fixture.resolver, fixture.memory, options);
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint64_t, 1> first{service.Register(fixture.Registration(0u, 60u))};
    service.Serve(first, recorder);
    Advance(recorder);
    const std::array<std::uint64_t, 1> second{service.Register(fixture.Registration(100u, 30u))};
    service.Serve(second, recorder);
    const auto afterSecond = service.Totals();
    Require(afterSecond.fullEvictions != 0u && afterSecond.evicted != 0u, "a full key table did not evict idle entries");
    for (std::uint32_t record = 100u; record < 130u; ++record) Require(service.Find(fixture.Key(record)).has_value(), "the table being served lost an entry to eviction");
    std::uint32_t evictedFirst = 0;
    for (std::uint32_t record = 0; record < 60u; ++record) evictedFirst += service.Find(fixture.Key(record)).has_value() ? 0u : 1u;
    Require(evictedFirst == afterSecond.evicted, "eviction did not take the idle table's entries");
    Advance(recorder);
    service.Serve(first, recorder);
    for (std::uint32_t record = 0; record < 60u; ++record) Require(service.Find(fixture.Key(record)).has_value(), "an evicted table was not completed again when served");
    Advance(recorder);
    const std::array<std::uint64_t, 1> oversized{service.Register(fixture.Registration(200u, 70u))};
    bool refused = false;
    try {
        service.Serve(oversized, recorder);
    } catch (const std::runtime_error& error) {
        refused = std::string(error.what()).find("bindless key table is full") != std::string::npos;
    }
    Require(refused, "a table larger than the key table was not refused when every entry is in use");
    recorder.Sync();
    std::cout << "full key table: " << afterSecond.evicted << " idle entries evicted for the served table, the evicted table completed again on its next use, an oversized table refused\n";
}

void perUseTests(const Device& device) {
    Fixture fixture(device);
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    TextureService::Options options;
    options.worker = false;
    TextureService service(*fixture.table, fixture.resolver, fixture.memory, options);
    fixture.resolver->dcc = {5u, 70u, 80u};
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint64_t, 1> first{service.Register(fixture.Registration(0u, 64u))};
    const std::array<std::uint64_t, 1> second{service.Register(fixture.Registration(64u, 64u))};
    service.Serve(first, recorder);
    service.Serve(second, recorder);
    const auto served = [&](std::span<const std::uint64_t> tables, bool resolvedOnlyRefreshes = true) {
        const auto before = service.Totals();
        const auto commits = fixture.resolver->commits.load();
        service.Serve(tables, recorder);
        const auto after = service.Totals();
        Require(after.refreshed - before.refreshed == after.dccRefreshed - before.dccRefreshed, "a per-use refresh of a DCC entry was not counted as one");
        Require(!resolvedOnlyRefreshes || fixture.resolver->commits.load() - commits == after.refreshed - before.refreshed, "a per-use refresh did not resolve its entry again");
        return after.dccRefreshed - before.dccRefreshed;
    };
    Require(served(first) == 1u, "serving the first table did not refresh exactly its DCC entry");
    Require(served(second) == 2u, "serving the second table did not refresh exactly its two DCC entries");
    const std::array<std::uint64_t, 2> both{first[0], second[0]};
    Require(served(both) == 3u, "serving both tables did not refresh each DCC entry once");
    fixture.records[5].image = 6u;
    fixture.write(5u, true);
    Require(served(first, false) == 0u, "a rewritten record kept its old DCC entry in the table's refresh list");
    Advance(recorder);
    Present(service, recorder);
    recorder.Sync();
    std::cout << "per-use refresh: DCC entries re-resolved for the tables being served only (1, 2, 3 of 3), none after the record moved off its DCC surface\n";
}

void budgetTests(const Device& device) {
    Fixture fixture(device);
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    TextureService::Options options;
    options.worker = false;
    options.budgetBytes = 16u * TextureService::PageBytes;
    options.idleSubmits = 1u;
    TextureService service(*fixture.table, fixture.resolver, fixture.memory, options);
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint64_t, 1> served{service.Register(fixture.Registration(0u, 40u))};
    service.Serve(served, recorder);
    Require(service.Totals().pinnedBytes == 40u * TextureService::PageBytes, "pinned bytes do not count one surface per image");
    service.Revalidate(recorder);
    Require(service.Totals().evicted == 0u, "entries used by the open batch were evicted");
    Advance(recorder);
    Advance(recorder);
    service.Revalidate(recorder);
    const auto counts = service.Totals();
    Require(counts.evicted == 28u && counts.pinnedBytes == 12u * TextureService::PageBytes, "budget eviction did not drop the oldest idle entries down to three quarters of the budget: " + std::to_string(counts.evicted) + " evicted, " + std::to_string(counts.pinnedBytes) + " bytes pinned");
    const auto pending = fixture.table->Heap().PendingReleases();
    recorder.Sync();
    service.Publish(recorder);
    Require(pending != 0u && fixture.table->Heap().PendingReleases() == 0u, "evicted slots were not freed after their retire serial");
    std::cout << "budget eviction: " << counts.evicted << " idle entries evicted to " << counts.pinnedBytes / TextureService::PageBytes << " pinned surfaces\n";
}

void liveBudgetTests(const Device& device) {
    Fixture fixture(device);
    Recorder recorder(fixture.context, device.Timeline());
    recorder.Activate();
    std::uint64_t live = 64u * TextureService::PageBytes;
    TextureService::Options options;
    options.worker = false;
    options.budgetBytes = 8u * TextureService::PageBytes;
    options.budgetSource = [&live] { return live; };
    options.idleSubmits = 1u;
    TextureService service(*fixture.table, fixture.resolver, fixture.memory, options);
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint64_t, 1> served{service.Register(fixture.Registration(0u, 40u))};
    service.Serve(served, recorder);
    Advance(recorder);
    Advance(recorder);
    service.Revalidate(recorder);
    auto counts = service.Totals();
    Require(counts.evicted == 0u && counts.budgetBytes == 64u * TextureService::PageBytes, "idle entries under the live budget were evicted by the fixed fallback budget: " + std::to_string(counts.evicted) + " evicted, budget " + std::to_string(counts.budgetBytes));
    live = 16u * TextureService::PageBytes;
    service.Revalidate(recorder);
    counts = service.Totals();
    Require(counts.evicted == 28u && counts.pinnedBytes == 12u * TextureService::PageBytes && counts.budgetBytes == live, "a smaller live budget did not evict the oldest idle entries down to three quarters of it: " + std::to_string(counts.evicted) + " evicted, " + std::to_string(counts.pinnedBytes) + " bytes pinned");
    live = 0u;
    service.Revalidate(recorder);
    Require(service.Totals().budgetBytes == 16u * TextureService::PageBytes, "a live budget source with no reading did not keep the last budget");
    recorder.Sync();
    service.Publish(recorder);
    std::cout << "live budget: nothing evicted under 64 surfaces, " << counts.evicted << " idle entries evicted when it fell to 16\n";
}

struct GuestBlock {
    static constexpr std::uint32_t Surfaces = 8u;
    static constexpr std::uint32_t SurfaceSide = 128u;
    static constexpr std::size_t SurfaceBytes = std::size_t{SurfaceSide} * SurfaceSide * 4u;
    static constexpr std::size_t Bytes = SurfaceBytes * Surfaces;

    GuestBlock() {
#ifdef _WIN32
        memory = VirtualAlloc(nullptr, Bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        memory = std::aligned_alloc(65536, Bytes);
#endif
        Require(memory != nullptr, "cannot allocate the guest surfaces");
        auto* texels = static_cast<std::uint32_t*>(memory);
        for (std::size_t texel = 0; texel < Bytes / 4u; ++texel) texels[texel] = static_cast<std::uint32_t>(texel * 2654435761u);
        GuestAllocations::Mutation().Add(memory, Bytes, true, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(memory);
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        std::free(memory);
#endif
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint64_t Surface(std::uint32_t surface) const { return reinterpret_cast<std::uint64_t>(memory) + surface * SurfaceBytes; }

    void* memory = nullptr;
};

std::array<std::uint32_t, 8> GuestDescriptor(std::uint64_t address, std::uint32_t side) {
    constexpr std::uint32_t Format32UInt = 20u;
    constexpr std::uint32_t TileR64KBX = 0x1bu;
    constexpr std::uint32_t Type2D = 9u;
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void driverTests(const Device& device) {
    using ShaderRecompiler::ResourceMaterializer;
    auto context = device.GetContext();
    AgcDriver::Graphics::TextureDetiler detiler(context);
    context.detiler = &detiler;
    GuestBlock block;
    ShaderRecompiler::ImageResource image{};
    image.resourceClass = ShaderRecompiler::ImageResourceClass::Sampled;
    image.dimension = ShaderRecompiler::RdnaImageDimension::Dim2D;
    image.read = true;
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    const std::uint64_t Interface = ResourceMaterializer::RegisterBindlessInterface(image, modes);
    Require(Interface == ResourceMaterializer::BindlessInterfaceId(image, modes) && ResourceMaterializer::FindBindlessInterface(Interface) != nullptr, "the registered interface id differs from the one the emitter embeds");
    constexpr std::uint32_t Records = 20u;
    constexpr std::uint32_t Unmapped = 19u;
    std::vector<std::uint32_t> words(static_cast<std::size_t>(Records) * 8u, 0u);
    for (std::uint32_t record = 0; record < Records; ++record) {
        if (record == 4u || record == 11u) continue;
        const auto address = record == Unmapped ? 0x7ff000000000ull : block.Surface(record % GuestBlock::Surfaces);
        const auto descriptor = GuestDescriptor(address, GuestBlock::SurfaceSide);
        std::copy(descriptor.begin(), descriptor.end(), words.begin() + static_cast<std::ptrdiff_t>(record) * 8);
    }
    struct ClearTextures {
        VkDevice device;
        ~ClearTextures() { AgcDriver::Graphics::ClearCachedTextures(device); }
    } clearTextures{context.device};
    const auto capacities = DescriptorHeap::DeviceCapacities(context);
    BindlessTable table(context, DescriptorHeap::Capacities{std::min(capacities.samplers, 64u), std::min(capacities.sampledImages, 1024u), std::min(capacities.storageImages, 64u)});
    Recorder recorder(context, device.Timeline());
    recorder.Activate();
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        TextureService::Options options;
        options.worker = false;
        TextureService service(table, std::make_shared<AgcDriver::Graphics::DriverBindlessResolver>(context), std::make_shared<AgcDriver::Graphics::GuestBindlessMemory>(), options);
        auto serving = context;
        serving.textureService = &service;
        const std::array<BindlessTableRegistration, 1> tables{BindlessTableRegistration{reinterpret_cast<std::uint64_t>(words.data()), Records, 32u, {BindlessUse{Interface, 0u, {}}}}};
        AgcDriver::Graphics::PrimeBindlessTables(serving, tables);
        AgcDriver::Graphics::ServeBindlessTables(serving, recorder, tables);
        Require(service.Totals().registered == 1u && service.Totals().primed == 1u, "the draw-path helpers did not register and prime the table once");
        std::set<const void*> textures;
        std::uint32_t compared = 0;
        for (std::uint32_t record = 0; record < Records; ++record) {
            const auto descriptor = std::span<const std::uint32_t, 8>(words.data() + static_cast<std::size_t>(record) * 8u, 8u);
            const auto key = Abi::MakeKey(descriptor, Interface, 0u);
            const auto value = service.Find(key);
            if (record == 4u || record == 11u) {
                Require(value.has_value() && value->slot == 0u && value->fault == 0u && service.Owner(key) == nullptr, "a null record is not a null-slot entry");
                continue;
            }
            Require(value.has_value(), "record " + std::to_string(record) + " was not primed");
            if (record == Unmapped) {
                Require(value->fault != 0u && service.Rejection(value->fault).value_or("").find("not mapped") != std::string::npos, "an unmapped surface was not a refusal naming the unmapped surface");
                continue;
            }
            ShaderRecompiler::DescriptorValue guest;
            guest.dwordCount = 8u;
            std::copy(descriptor.begin(), descriptor.end(), guest.dwords.begin());
            const auto oldMode = ResourceMaterializer::RuntimeImageMode(image, guest, modes);
            const auto oldTexture = AgcDriver::Graphics::CachedSampledTexture(context, descriptor);
            Require(value->fault == 0u && value->mode == oldMode, "record " + std::to_string(record) + " got mode " + std::to_string(value->mode) + ", the old resolver's mode is " + std::to_string(oldMode));
            Require(service.Owner(key).get() == oldTexture.get(), "record " + std::to_string(record) + " holds another texture than the old resolver's cached texture");
            Require(table.Heap().Live(DescriptorHeap::Array::SampledImages, value->slot), "record " + std::to_string(record) + " names a dead heap slot");
            textures.insert(oldTexture.get());
            ++compared;
        }
        const auto counts = service.Totals();
        Require(textures.size() == GuestBlock::Surfaces && counts.imageSlots == GuestBlock::Surfaces && counts.created == GuestBlock::Surfaces + 2u && counts.refused == 1u, "the primer did not share one cached texture and one heap slot per surface: " + std::to_string(textures.size()) + " textures, " + std::to_string(counts.imageSlots) + " slots, " + std::to_string(counts.created) + " entries, " + std::to_string(counts.refused) + " refused");
        recorder.Sync();
        std::cout << "driver resolver: " << compared << " primed entries match the old resolver (same cached texture, same runtime mode), " << counts.imageSlots << " heap slots, unmapped T# refused, null records are null-slot entries\n";
    }
    recorder.Sync();
    recorder.FlushDeferredReleases();
}

void r128Tests(const Device& device) {
    using ShaderRecompiler::ResourceMaterializer;
    auto context = device.GetContext();
    AgcDriver::Graphics::TextureDetiler detiler(context);
    context.detiler = &detiler;
    GuestBlock block;
    ShaderRecompiler::ImageResource image{};
    image.resourceClass = ShaderRecompiler::ImageResourceClass::Sampled;
    image.dimension = ShaderRecompiler::RdnaImageDimension::Dim2D;
    image.read = true;
    auto wide = image;
    image.r128 = true;
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    const auto wideModes = ResourceMaterializer::RuntimeImageModes(wide);
    const std::uint64_t Interface = ResourceMaterializer::RegisterBindlessInterface(image, modes);
    const std::uint64_t WideInterface = ResourceMaterializer::RegisterBindlessInterface(wide, wideModes);
    Require(Interface != WideInterface, "a 128-bit image interface shares its id with the 256-bit one");
    constexpr std::uint32_t StreamedSide = 512u;
    constexpr std::uint32_t StreamedBase = 2u;
    constexpr std::uint32_t StreamedLast = 9u;
    auto streamed = GuestDescriptor(block.Surface(0u), StreamedSide);
    streamed[3] |= (StreamedBase << 12u) | (StreamedLast << 16u);
    auto chain = GuestDescriptor(block.Surface(4u), GuestBlock::SurfaceSide);
    chain[3] |= 7u << 16u;
    const auto residentBytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(AgcDriver::Graphics::R128TextureWords(streamed))).guestBytes;
    auto fullWords = streamed;
    fullWords[3] &= ~(0xfu << 12u);
    const auto fullBytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(AgcDriver::Graphics::R128TextureWords(fullWords))).guestBytes;
    Require(residentBytes <= GuestBlock::SurfaceBytes * 4u && fullBytes > GuestBlock::Bytes, "the streamed T#'s resident levels must fit the guest block and its whole chain must not");
    std::vector<std::uint32_t> words(16u, 0u);
    std::copy(streamed.begin(), streamed.end(), words.begin());
    std::copy(chain.begin(), chain.end(), words.begin() + 8);
    struct ClearTextures {
        VkDevice device;
        ~ClearTextures() { AgcDriver::Graphics::ClearCachedTextures(device); }
    } clearTextures{context.device};
    const auto capacities = DescriptorHeap::DeviceCapacities(context);
    BindlessTable table(context, DescriptorHeap::Capacities{std::min(capacities.samplers, 64u), std::min(capacities.sampledImages, 1024u), std::min(capacities.storageImages, 64u)});
    Recorder recorder(context, device.Timeline());
    recorder.Activate();
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        TextureService::Options options;
        options.worker = false;
        TextureService service(table, std::make_shared<AgcDriver::Graphics::DriverBindlessResolver>(context), std::make_shared<AgcDriver::Graphics::GuestBindlessMemory>(), options);
        auto serving = context;
        serving.textureService = &service;
        const std::array<BindlessTableRegistration, 1> tables{BindlessTableRegistration{reinterpret_cast<std::uint64_t>(words.data()), 2u, 32u, {BindlessUse{Interface, 0u, {}}, BindlessUse{WideInterface, 0u, {}}}}};
        AgcDriver::Graphics::PrimeBindlessTables(serving, tables);
        AgcDriver::Graphics::ServeBindlessTables(serving, recorder, tables);
        const auto levels = [&](const std::array<std::uint32_t, 8>& descriptor, std::uint64_t id, const char* name) {
            const auto key = Abi::MakeKey(std::span<const std::uint32_t, 8>(descriptor.data(), 8u), id, 0u);
            const auto value = service.Find(key);
            Require(value.has_value() && value->fault == 0u, std::string(name) + " was refused: " + (value.has_value() ? service.Rejection(value->fault).value_or("") : std::string("no entry")));
            const auto texture = std::static_pointer_cast<AgcDriver::Graphics::Texture>(service.Owner(key));
            Require(texture != nullptr && table.Heap().Live(DescriptorHeap::Array::SampledImages, value->slot), std::string(name) + " has no live texture");
            return std::make_pair(texture, texture->SampledViewRange(false).levels);
        };
        const auto [streamedTexture, streamedLevels] = levels(streamed, Interface, "a 128-bit T# of levels 2..9 of a 512x512 chain whose levels 0 and 1 are not resident");
        Require(streamedLevels == StreamedLast - StreamedBase + 1u && streamedTexture.get() == AgcDriver::Graphics::CachedSampledTexture(context, AgcDriver::Graphics::R128TextureWords(streamed)).get(), "the streamed 128-bit T# did not read as the 128x128 chain of its 8 resident levels: " + std::to_string(streamedLevels) + " levels");
        const auto [chainTexture, chainLevels] = levels(chain, Interface, "a 128-bit T# of levels 0..7");
        const auto [wideTexture, wideLevels] = levels(chain, WideInterface, "a 256-bit T# of levels 0..7 with MAX_MIP 0");
        Require(chainLevels == 8u && wideLevels == 1u && chainTexture != wideTexture, "a 128-bit T# must take its 8 levels from LAST_LEVEL while the same 256-bit T# keeps MAX_MIP's one level: " + std::to_string(chainLevels) + " and " + std::to_string(wideLevels) + " levels");
        recorder.Sync();
        std::cout << "128-bit T#s: a streamed T# past MAX_MIP reads its " << streamedLevels << " resident levels, a full one its " << chainLevels << " levels from LAST_LEVEL\n";
    }
    recorder.Sync();
    recorder.FlushDeferredReleases();
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
    Require(device->TextureServiceRunning() == on && device->BindlessImagesEnabled() == on, on ? "APS5_BINDLESS_IMAGES=1 did not start the texture service" : "the texture service runs without APS5_BINDLESS_IMAGES=1");
    device->BindlessPresentWait();
    std::cout << "bindless flag " << (on ? "on" : "off") << ": texture service " << (on ? "running" : "absent") << '\n';
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc >= 2 ? argv[1] : "";
        if (mode == "--flag-off") return flagTests(false);
        if (mode == "--flag-on") return flagTests(true);
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::cout << "skipped, no usable bindless Vulkan device: " << error.what() << '\n';
            return Skipped;
        }
        if (mode == "--driver") {
            driverTests(*device);
            r128Tests(*device);
            return 0;
        }
        serviceTests(*device);
        lazyTests(*device);
        stormTests(*device);
        fullTests(*device);
        perUseTests(*device);
        budgetTests(*device);
        liveBudgetTests(*device);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "texture service test failed: " << error.what() << '\n';
        return 1;
    }
}
