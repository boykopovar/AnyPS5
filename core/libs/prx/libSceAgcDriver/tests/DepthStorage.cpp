#include "DepthStorageUnormTyped_spv.h"
#include "DepthStorageUnormFormatless_spv.h"
#include "DepthStorageUnormArray_spv.h"
#include "DepthStorageUnormArrayFormatless_spv.h"
#include "DepthStorageUnormTransition_spv.h"
#include "DepthStorageUnormArrayTransition_spv.h"
#include "DepthStorageTyped_spv.h"
#include "DepthStorageTransition_spv.h"
#include "DepthStorageArrayTransition_spv.h"
#include "DepthStorageFormatless_spv.h"
#include "DepthStorageArray_spv.h"
#include "DepthStorageArrayFormatless_spv.h"
#include "DepthAttachment_vert_spv.h"
#include "DepthAttachment_frag_spv.h"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "RuntimeAbi.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include <SDL_loadso.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using namespace AgcDriver::Graphics;

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
            application.apiVersion = VK_API_VERSION_1_1;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            info.pApplicationInfo = &application;
            Check(function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
            std::uint32_t count = 0;
            const auto enumerate = function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
            Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            Require(count != 0, "no Vulkan device");
            std::vector<VkPhysicalDevice> devices(count);
            Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            const auto rankDeviceType = [](VkPhysicalDeviceType type) {
                switch (type) {
                    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
                    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
                    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
                    default: return 0;
                }
            };
            const auto physicalProperties = function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
            int selectedRank = -1;
            for (const auto physical : devices) {
                VkPhysicalDeviceProperties candidate{};
                physicalProperties(physical, &candidate);
                if (candidate.apiVersion < VK_API_VERSION_1_1) continue;
                const int rank = rankDeviceType(candidate.deviceType);
                if (rank <= selectedRank) continue;
                context.physical = physical;
                selectedRank = rank;
                cpu = candidate.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
            }
            Require(context.physical != VK_NULL_HANDLE, "no Vulkan 1.1 device");
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) != (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) ++family;
            Require(family < count, "no Vulkan compute queue");
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkPhysicalDeviceFeatures enabled{};
            VkPhysicalDeviceFeatures available{};
            function<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(context.physical, &available);
            Require(available.shaderStorageImageExtendedFormats, "R16 storage images unavailable");
            Require(available.shaderStorageImageReadWithoutFormat && available.shaderStorageImageWriteWithoutFormat, "formatless storage image reads/writes unavailable");
            Require(available.fragmentStoresAndAtomics, "fragment buffer atomics unavailable");
            enabled.shaderStorageImageExtendedFormats = VK_TRUE;
            enabled.shaderStorageImageReadWithoutFormat = VK_TRUE;
            enabled.shaderStorageImageWriteWithoutFormat = VK_TRUE;
            enabled.fragmentStoresAndAtomics = VK_TRUE;
            std::uint32_t extensionCount = 0;
            const auto enumerateExtensions = function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            Check(enumerateExtensions(context.physical, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> extensions(extensionCount);
            Check(enumerateExtensions(context.physical, nullptr, &extensionCount, extensions.data()), "vkEnumerateDeviceExtensionProperties");
            Require(std::any_of(extensions.begin(), extensions.end(), [](const auto& extension) { return std::strcmp(extension.extensionName, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME) == 0; }), "typed image heaps require VK_EXT_robustness2");
            VkPhysicalDeviceRobustness2FeaturesEXT robustness{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
            VkPhysicalDeviceFeatures2 queried{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &robustness};
            function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(context.physical, &queried);
            Require(robustness.nullDescriptor == VK_TRUE, "typed image heaps require nullDescriptor");
            robustness = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
            robustness.nullDescriptor = VK_TRUE;
            const char* extension = VK_EXT_ROBUSTNESS_2_EXTENSION_NAME;
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            device.pNext = &robustness;
            device.enabledExtensionCount = 1;
            device.ppEnabledExtensionNames = &extension;
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.nullDescriptors = true;
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.imageFormatProperties = function<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties");
            std::cout << "Vulkan device: " << properties.deviceName << '\n';
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.queueFamilyIndex = family;
            Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.pool), "vkCreateCommandPool");
            detiler = std::make_unique<TextureDetiler>(context);
            context.detiler = detiler.get();
            textureCache = std::make_unique<TextureCache>(context);
            context.textureCache = textureCache.get();
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
        if (context.device) {
            ClearCachedTextures(context.device);
            ClearDepthSurfaces(context.device);
        }
        textureCache.reset();
        detiler.reset();
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
    std::unique_ptr<TextureDetiler> detiler;
    std::unique_ptr<TextureCache> textureCache;
};

AgcDriver::QueueState MakeState() {
    AgcDriver::QueueState queue;
    queue.userConfig[0x242] = 4;
    queue.context = {
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0}, {0x8f, 0}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040}
    };
    queue.context[0x10f] = std::bit_cast<std::uint32_t>(32.0f);
    queue.context[0x110] = std::bit_cast<std::uint32_t>(32.0f);
    queue.context[0x111] = std::bit_cast<std::uint32_t>(-2.0f);
    queue.context[0x112] = std::bit_cast<std::uint32_t>(2.0f);
    queue.context[0x113] = std::bit_cast<std::uint32_t>(1.0f);
    queue.context[0x114] = 0;
    queue.context[0xb4] = 0;
    queue.context[0xb5] = std::bit_cast<std::uint32_t>(1.0f);
    return queue;
}

class Compute {
public:
    Compute(const Context& context, VkImageView storage, VkImageView depth, Buffer& results, bool formatless = false, bool array = false, bool unorm = false) : context(context) {
        try {
            const std::array<VkDescriptorSetLayoutBinding, 4> bindings{{
                {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
            }};
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.bindingCount = bindings.size();
            setInfo.pBindings = bindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
            const std::array<VkDescriptorPoolSize, 4> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}}};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = sizes.size();
            poolInfo.pPoolSizes = sizes.data();
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1, &setLayout};
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets");
            VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &samplerInfo, nullptr, &sampler), "vkCreateSampler");
            const std::array<VkDescriptorImageInfo, 3> images{{{VK_NULL_HANDLE, storage, VK_IMAGE_LAYOUT_GENERAL}, {VK_NULL_HANDLE, depth, VK_IMAGE_LAYOUT_GENERAL}, {sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED}}};
            const VkDescriptorBufferInfo buffer{results.Handle(), 0, results.Bytes().size()};
            std::array<VkWriteDescriptorSet, 4> writes{};
            for (std::uint32_t i = 0; i < writes.size(); ++i) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = set;
                writes[i].dstBinding = i;
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = bindings[i].descriptorType;
                if (i < 3) writes[i].pImageInfo = &images[i];
                else writes[i].pBufferInfo = &buffer;
            }
            context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, writes.size(), writes.data(), 0, nullptr);
            const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 4};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &setLayout;
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &push;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            const std::span<const std::uint32_t> code = unorm
                ? (array
                    ? (formatless ? std::span<const std::uint32_t>(DEPTH_STORAGE_UNORMARRAYFORMATLESS_SPV) : std::span<const std::uint32_t>(DEPTH_STORAGE_UNORMARRAY_SPV))
                    : (formatless ? std::span<const std::uint32_t>(DEPTH_STORAGE_UNORMFORMATLESS_SPV) : std::span<const std::uint32_t>(DEPTH_STORAGE_UNORMTYPED_SPV)))
                : (array
                    ? (formatless ? std::span<const std::uint32_t>(DEPTH_STORAGE_ARRAYFORMATLESS_SPV) : std::span<const std::uint32_t>(DEPTH_STORAGE_ARRAY_SPV))
                    : (formatless ? std::span<const std::uint32_t>(DEPTH_STORAGE_FORMATLESS_SPV) : std::span<const std::uint32_t>(DEPTH_STORAGE_TYPED_SPV)));
            moduleInfo.codeSize = code.size_bytes();
            moduleInfo.pCode = code.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
            pipelineInfo.layout = layout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        } catch (...) { release(); throw; }
    }
    ~Compute() { release(); }
    void Run(std::uint32_t mode) {
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
        context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, 4, 2, 1);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
        if (batch) batch->SubmitAndWait();
    }
private:
    void release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
        if (sampler) context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, sampler, nullptr);
    }
    Context context;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};

DepthTarget Target(std::uint64_t address = 0x10000) {
    auto queue = MakeState();
    queue.context[0x000] = 0;
    queue.context[0x002] = 0;
    queue.context[0x007] = (1u << 16u) | 3u;
    queue.context[0x00a] = 0;
    queue.context[0x00b] = std::bit_cast<std::uint32_t>(0.375f);
    queue.context[0x010] = 1;
    queue.context[0x011] = 0;
    for (const auto offset : {0x012u, 0x014u}) queue.context[offset] = static_cast<std::uint32_t>(address >> 8u);
    for (const auto offset : {0x01au, 0x01cu}) queue.context[offset] = static_cast<std::uint32_t>(address >> 40u);
    queue.context[0x200] = 0x76;
    const auto state = DecodeState(queue);
    Require(state.depth.has_value() && state.depth->format == VK_FORMAT_D16_UNORM && state.depthWrite, "DB register fixture did not decode writable D16");
    return *state.depth;
}

std::array<std::uint32_t, 8> Descriptor(std::uint64_t address, VkFormat format, bool array = false) {
    const auto guest = FindGuestTextureFormat(format, 2);
    Require(guest.has_value(), "test format has no guest descriptor");
    return {static_cast<std::uint32_t>(address >> 8), static_cast<std::uint32_t>(address >> 40) | (*guest << 20u) | (3u << 30u), (1u << 14u), ((array ? 13u : 9u) << 28u) | (24u << 20u) | (7u << 9u) | (6u << 6u) | (5u << 3u) | 4u, 0, 0, 0, 0};
}

using Pixels = std::array<std::uint32_t, 8>;

Pixels CheckPixels(Buffer& results, const Pixels* expected, const char* phase, bool normalizedWrite = false) {
    results.Invalidate();
    std::array<std::uint32_t, 16> words{};
    std::memcpy(words.data(), results.Bytes().data(), sizeof(words));
    Pixels pixels{};
    for (std::size_t i = 0; i < 8; ++i) {
        pixels[i] = words[i * 2];
        Require(pixels[i] <= 65535, std::string(phase) + ": storage exceeded 16 bits");
        if (expected != nullptr) Require(std::abs(static_cast<int>(pixels[i]) - static_cast<int>((*expected)[i])) <= (normalizedWrite ? 1 : 0), std::string(phase) + ": depth value mismatch at " + std::to_string(i) + ": " + std::to_string(pixels[i]) + " != " + std::to_string((*expected)[i]));
        const auto sampled = std::bit_cast<float>(words[i * 2 + 1]);
        Require(std::isfinite(sampled) && std::abs(sampled - static_cast<float>(pixels[i]) / 65535.0f) <= 0.0000002f, std::string(phase) + ": sampled UNORM depth did not match stored bits");
    }
    return pixels;
}

void CheckClear(const Pixels& pixels, float depth) {
    for (const auto pixel : pixels) Require(std::abs(static_cast<float>(pixel) - depth * 65535.0f) <= 1.0f, "attachment clear did not seed the D16 buffer plane");
}

Pixels Modified(Pixels pixels) {
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] ^= 0x2141u + static_cast<std::uint32_t>(i) * 0x111u;
    return pixels;
}

template<typename TAction>
std::string Refused(TAction action, const char* reason) {
    try { action(); }
    catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, std::string("unexpected refusal: ") + error.what());
        return error.what();
    }
    throw std::runtime_error(std::string("missing refusal: ") + reason);
}

class DepthDraw {
public:
    DepthDraw(const Context& context, VkRenderPass pass, Buffer& results, float reference) : context(context) {
        try {
            const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.bindingCount = 1;
            setInfo.pBindings = &binding;
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout depth draw");
            const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &size;
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool depth draw");
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1, &setLayout};
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets depth draw");
            const VkDescriptorBufferInfo buffer{results.Handle(), 0, results.Bytes().size()};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = set;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo = &buffer;
            context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &setLayout;
            const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, 4};
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &push;
            this->reference = reference;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout depth draw");
            const std::array<std::span<const std::uint32_t>, 2> codes{DEPTH_ATTACHMENT_vert_SPV, DEPTH_ATTACHMENT_frag_SPV};
            std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
            for (std::size_t i = 0; i < codes.size(); ++i) {
                VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                moduleInfo.codeSize = codes[i].size_bytes();
                moduleInfo.pCode = codes[i].data();
                Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &modules[i]), "vkCreateShaderModule depth draw");
                stages[i] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, i == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT, modules[i], "main", nullptr};
            }
            VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            const VkViewport viewport{0, 0, 4, 2, 0, 1};
            const VkRect2D scissor{{0, 0}, {4, 2}};
            VkPipelineViewportStateCreateInfo viewportInfo{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewportInfo.viewportCount = 1;
            viewportInfo.pViewports = &viewport;
            viewportInfo.scissorCount = 1;
            viewportInfo.pScissors = &scissor;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.lineWidth = 1;
            VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            depth.depthTestEnable = VK_TRUE;
            depth.depthCompareOp = VK_COMPARE_OP_LESS;
            VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipelineInfo.stageCount = stages.size();
            pipelineInfo.pStages = stages.data();
            pipelineInfo.pVertexInputState = &vertex;
            pipelineInfo.pInputAssemblyState = &assembly;
            pipelineInfo.pViewportState = &viewportInfo;
            pipelineInfo.pRasterizationState = &raster;
            pipelineInfo.pMultisampleState = &multisample;
            pipelineInfo.pDepthStencilState = &depth;
            pipelineInfo.pColorBlendState = &blend;
            pipelineInfo.layout = layout;
            pipelineInfo.renderPass = pass;
            Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateGraphicsPipelines depth draw");
        } catch (...) { release(); throw; }
    }
    ~DepthDraw() { release(); }
    void Record(VkCommandBuffer commands) {
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
        context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4, &reference);
        context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 3, 1, 0, 0);
    }
private:
    void release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        for (const auto module : modules) if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }
    Context context;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    float reference = 0;
    std::array<VkShaderModule, 2> modules{};
    VkPipeline pipeline = VK_NULL_HANDLE;
};

void AttachmentOperation(const Context& context, VkImageView view, float depth, Buffer* results = nullptr) {
    VkAttachmentDescription attachment{};
    attachment.format = VK_FORMAT_D16_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = results == nullptr ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_GENERAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &reference;
    VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    passInfo.attachmentCount = 1;
    passInfo.pAttachments = &attachment;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    VkRenderPass pass = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &pass), "vkCreateRenderPass");
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    try {
        VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = pass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &view;
        framebufferInfo.width = 4;
        framebufferInfo.height = 2;
        framebufferInfo.layers = 1;
        Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
        std::unique_ptr<DepthDraw> draw;
        if (results != nullptr) {
            std::fill(results->Bytes().begin(), results->Bytes().end(), std::byte{});
            draw = std::make_unique<DepthDraw>(context, pass, *results, depth);
        }
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        const VkClearValue clear{.depthStencil = {depth, 0}};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = pass;
        begin.framebuffer = framebuffer;
        begin.renderArea = {{0, 0}, {4, 2}};
        begin.clearValueCount = 1;
        begin.pClearValues = &clear;
        context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        if (draw) draw->Record(commands);
        context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
        batch.SubmitAndWait();
    } catch (...) {
        if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, pass, nullptr);
        throw;
    }
    context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
    context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, pass, nullptr);
}

void CheckAttachment(const Context& context, const DepthTarget& target, float reference, std::uint32_t expected, Buffer& results) {
    AttachmentOperation(context, DepthSurfaceView(context, target), reference, &results);
    results.Invalidate();
    std::uint32_t passed = 0;
    std::memcpy(&passed, results.Bytes().data(), 4);
    Require(passed == expected, "depth attachment comparison passed " + std::to_string(passed) + " fragments, expected " + std::to_string(expected));
}

void Run(const Context& context, bool recorded, bool formatless, bool array, bool unorm) {
    std::unique_ptr<Recorder> recorder;
    if (recorded) {
        recorder = std::make_unique<Recorder>(context);
        recorder->Activate();
    }
    const auto target = Target();
    DepthSurfaceView(context, target);
    const auto depthWords = Descriptor(target.address, VK_FORMAT_R16_UNORM);
    const auto resource = DecodeTextureResource(Descriptor(target.address, unorm ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16_UINT, array));
    const VkComponentMapping components{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    auto depth = DepthSurfaceTexture(context, depthWords, DecodeTextureResource(depthWords), components);
    Buffer results(context, 64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (recorder) recorder->Sync();
    CheckAttachment(context, target, 0.25f, 8, results);
    CheckAttachment(context, target, 0.5f, 0, results);
    std::cout << (recorded ? "recorded " : "immediate ") << (formatless ? "formatless " : "typed ") << (array ? "array" : "2D") << (unorm ? " UNORM" : " UINT") << ": depth attachment positive and negative controls passed\n";
    Require(resource.format == (unorm ? 7u : 11u), "storage fixture did not decode the declared UNORM/UINT guest format");
    auto storage = CachedStorageSurface(context, resource);
    const auto initialVersion = storage->Version();
    Require(CachedStorageSurface(context, resource) == storage, "depth storage lookup did not reuse the image");
    Require(storage->Version() == initialVersion, "repeated lookup fabricated a storage content change");
    Compute compute(context, storage->View(), depth->View(), results, formatless, array, unorm);
    compute.Run(2);
    if (recorder) recorder->Sync();
    auto pixels = CheckPixels(results, nullptr, "resident seed and repeated lookup");
    CheckClear(pixels, 0.375f);
    compute.Run(0);
    storage->MarkDirty();
    compute.Run(1);
    if (recorder) recorder->Sync();
    pixels = Modified(pixels);
    pixels = CheckPixels(results, &pixels, "storage to sampled depth", unorm);
    const auto minimum = *std::min_element(pixels.begin(), pixels.end());
    const auto maximum = *std::max_element(pixels.begin(), pixels.end());
    CheckAttachment(context, target, static_cast<float>(minimum) / 65535.0f - 0.02f, 8, results);
    CheckAttachment(context, target, static_cast<float>(maximum) / 65535.0f + 0.02f, 0, results);
    AttachmentOperation(context, DepthSurfaceView(context, target), 0.625f);
    Require(CachedStorageSurface(context, resource) == storage, "attachment refresh replaced storage");
    compute.Run(2);
    if (recorder) recorder->Sync();
    pixels = CheckPixels(results, nullptr, "attachment to storage refresh");
    CheckClear(pixels, 0.625f);
    compute.Run(0);
    storage->MarkDirty();
    compute.Run(1);
    if (recorder) recorder->Sync();
    pixels = Modified(pixels);
    pixels = CheckPixels(results, &pixels, "second depth round trip", unorm);
    const auto otherResource = DecodeTextureResource(Descriptor(target.address, unorm ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16_UINT, !array));
    auto other = CachedStorageSurface(context, otherResource);
    Require(other != storage, "2D and array descriptors reused a storage view of the wrong type");
    Compute otherCompute(context, other->View(), depth->View(), results, formatless, !array, unorm);
    otherCompute.Run(2);
    if (recorder) recorder->Sync();
    CheckPixels(results, &pixels, "other storage view seeded from resident depth");
    otherCompute.Run(0);
    other->MarkDirty();
    Require(CachedStorageSurface(context, resource) == storage, "original storage view was replaced after alias write");
    compute.Run(2);
    if (recorder) recorder->Sync();
    pixels = Modified(pixels);
    pixels = CheckPixels(results, &pixels, "storage view types share resident revisions", unorm);
    const auto aliasResource = DecodeTextureResource(Descriptor(target.address, unorm ? VK_FORMAT_R16_UINT : VK_FORMAT_R16_UNORM, array));
    auto alias = CachedStorageSurface(context, aliasResource);
    Require(alias != storage && alias->View() != storage->View(), "UNORM and UINT reused the same storage object/view");
    Require(CachedStorageSurface(context, aliasResource) == alias, "cross-format lookup replaced its own storage object");
    Compute aliasCompute(context, alias->View(), depth->View(), results, formatless, array, !unorm);
    aliasCompute.Run(2);
    if (recorder) recorder->Sync();
    CheckPixels(results, &pixels, "cross-format alias seeded from resident depth");
    aliasCompute.Run(0);
    alias->MarkDirty();
    Require(CachedStorageSurface(context, resource) == storage, "cross-format write replaced the original storage object");
    compute.Run(2);
    if (recorder) recorder->Sync();
    pixels = Modified(pixels);
    pixels = CheckPixels(results, &pixels, "cross-format alias write refreshed original storage and sampled depth", !unorm);
    CheckAttachment(context, target, static_cast<float>(*std::min_element(pixels.begin(), pixels.end())) / 65535.0f - 0.02f, 8, results);
    CheckAttachment(context, target, static_cast<float>(*std::max_element(pixels.begin(), pixels.end())) / 65535.0f + 0.02f, 0, results);
    for (const auto invalid : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}) {
        auto mismatch = resource;
        if (invalid == 0) mismatch.width = 8;
        if (invalid == 1) mismatch.tileMode = TextureTileMode::kLinear;
        if (invalid == 2) mismatch.mipCount = 2;
        if (invalid == 3) { mismatch.dimension = TextureDimension::k2DArray; mismatch.depthOrLastArray = 1; }
        if (invalid == 4) mismatch.dccAddress = 0x40000;
        if (invalid == 5) mismatch.format = DecodeTextureResource(Descriptor(target.address, VK_FORMAT_R16_SFLOAT)).format;
        if (invalid == 6) mismatch.baseArray = 1;
        if (invalid == 7) { mismatch.baseLevel = 1; mismatch.lastLevel = 1; }
        if (invalid == 8) mismatch.height = 3;
        if (invalid == 9) mismatch.dimension = TextureDimension::k3D;
        Refused([&] { CachedStorageSurface(context, mismatch); }, "depth storage requires");
    }
    auto unsupported = target;
    unsupported.address = 0x60000;
    unsupported.format = VK_FORMAT_D32_SFLOAT;
    DepthSurfaceView(context, unsupported);
    auto unsupportedResource = resource;
    unsupportedResource.baseAddress = unsupported.address;
    Refused([&] { CachedStorageSurface(context, unsupportedResource); }, "depth plane");
    if (recorder) {
        compute.Run(0);
        storage->MarkDirty();
        compute.Run(1);
        ClearDepthSurfaces(context.device);
        ClearCachedTextures(context.device);
        storage.reset();
        depth.reset();
        other.reset();
        alias.reset();
        recorder->Sync();
        pixels = Modified(pixels);
        pixels = CheckPixels(results, &pixels, "cleared registries during queued work", unorm);
    }
    ClearDepthSurfaces(context.device);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        data = GuestArena::GuestArenaAllocate_nid_postfix(65536, 65536);
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, 65536, PAGE_READWRITE, 65536);
#else
        void* mapped = mmap(nullptr, 131072, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + 65535) & ~std::uintptr_t{65535};
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + 65536 != begin + 131072) munmap(reinterpret_cast<void*>(aligned + 65536), begin + 131072 - aligned - 65536);
            data = reinterpret_cast<void*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, 65536);
        }
#endif
        Require(data != nullptr, "cannot allocate synthetic guest depth bytes");
        Require(AgcDriver::GuestMemory::Watched(Address(), 65536), "generic resource positive control requires write-watched memory");
        std::memset(data, 0, 65536);
        GuestAllocations::Mutation().Add(data, 65536, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(data, 65536);
        GuestArena::GuestArenaRelease_nid_postfix(data, 65536);
#else
        munmap(data, 65536);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, 65536);
#endif
    }
    std::uint64_t Address() const { return reinterpret_cast<std::uint64_t>(data); }
private:
    void* data = nullptr;
};

class ResourceDispatch {
public:
    ResourceDispatch(const Context& context, const ShaderResources& resources, bool array, bool unorm) : context(context) {
        try {
            const auto setLayout = resources.Layout();
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &setLayout;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout resource dispatch");
            const std::span<const std::uint32_t> code = unorm
                ? (array ? std::span<const std::uint32_t>(DEPTH_STORAGE_UNORMARRAYTRANSITION_SPV) : std::span<const std::uint32_t>(DEPTH_STORAGE_UNORMTRANSITION_SPV))
                : (array ? std::span<const std::uint32_t>(DEPTH_STORAGE_ARRAYTRANSITION_SPV) : std::span<const std::uint32_t>(DEPTH_STORAGE_TRANSITION_SPV));
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = code.size_bytes();
            moduleInfo.pCode = code.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule resource dispatch");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
            pipelineInfo.layout = layout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines resource dispatch");
        } catch (...) { release(); throw; }
    }
    ~ResourceDispatch() { release(); }
    void Run(ShaderResources& resources) {
        Recorder recorder(context);
        recorder.Activate();
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        resources.Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, 4, 2, 1);
        resources.MarkGpuWrites(recorder);
        recorder.Sync();
    }
private:
    void release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
    }
    Context context;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

void ResourceTransition(const Context& context, bool array, bool unorm) {
    GuestBlock guest;
    const auto target = Target(guest.Address());
    const auto storageWords = Descriptor(target.address, unorm ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16_UINT, array);
    const auto depthWords = Descriptor(target.address, VK_FORMAT_R16_UNORM);
    const auto resource = DecodeTextureResource(storageWords);
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::ImageResource storageImage;
    storageImage.resourceClass = ShaderRecompiler::ImageResourceClass::Storage;
    storageImage.numericClass = unorm ? ShaderRecompiler::IrTextureNumericClass::Float : ShaderRecompiler::IrTextureNumericClass::Uint;
    storageImage.dimension = array ? ShaderRecompiler::RdnaImageDimension::Dim2DArray : ShaderRecompiler::RdnaImageDimension::Dim2D;
    storageImage.read = true;
    storageImage.written = true;
    auto sampledImage = storageImage;
    sampledImage.resourceClass = ShaderRecompiler::ImageResourceClass::Sampled;
    sampledImage.numericClass = ShaderRecompiler::IrTextureNumericClass::Float;
    sampledImage.dimension = ShaderRecompiler::RdnaImageDimension::Dim2D;
    sampledImage.written = false;
    ShaderRecompiler::DescriptorBinding storageBinding{};
    storageBinding.kind = ShaderRecompiler::DescriptorKind::StorageImage;
    storageBinding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    storageBinding.binding = static_cast<std::uint32_t>(ShaderRecompiler::DescriptorBindingForImage(storageImage));
    storageBinding.count = 1;
    storageBinding.guestDescriptor.assign(storageWords.begin(), storageWords.end());
    storageBinding.imageShape = array ? ShaderRecompiler::DescriptorImageShape::Image2DArray : ShaderRecompiler::DescriptorImageShape::Image2D;
    storageBinding.imageWritten = {true};
    auto sampledBinding = storageBinding;
    sampledBinding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    sampledBinding.binding = static_cast<std::uint32_t>(ShaderRecompiler::DescriptorBindingForImage(sampledImage));
    sampledBinding.guestDescriptor.assign(depthWords.begin(), depthWords.end());
    sampledBinding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    sampledBinding.imageWritten = {false};
    sampledBinding.imageSamplers = {0};
    ShaderRecompiler::DescriptorBinding samplerBinding{};
    samplerBinding.kind = ShaderRecompiler::DescriptorKind::Sampler;
    samplerBinding.role = ShaderRecompiler::DescriptorRole::GuestSamplers;
    samplerBinding.binding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers);
    samplerBinding.count = 1;
    samplerBinding.guestDescriptor = {0, 0, 0, 0};
    samplerBinding.samplerDepthCompare = {false};
    program.bindings = {storageBinding, sampledBinding, samplerBinding};
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    auto generic = std::make_unique<ShaderResources>(context, shader);
    Require(generic->Reusable(), "generic storage/sample set was not reusable");
    ShaderResources::ProofReport proof;
    Require(generic->Revalidate(shader, &proof), "generic set failed its unchanged positive control");
    Require(proof.path == ShaderResources::ProofPath::Fast, "generic set did not exercise the fast guest-memory proof");
    const auto genericImages = generic->StorageImages();
    Require(!genericImages.empty(), "generic storage set has no image");
    ShaderResources deferred(context, shader, {}, true);
    Require(!deferred.Completed(), "deferred generic image capture completed early");
    DepthSurfaceView(context, target);
    Require(!generic->ProveCurrent(shader), "generic storage/sample proof accepted newly resident depth");
    auto rebuilt = std::make_unique<ShaderResources>(context, shader);
    deferred.Complete();
    auto storage = CachedStorageSurface(context, resource);
    Require(rebuilt->StorageImages().front().first == storage->Image() && rebuilt->StorageImages().front().first != genericImages.front().first, "rebuild did not route storage to resident depth");
    Require(deferred.StorageImages().front().first == storage->Image(), "deferred generic capture did not route storage to resident depth");
    const VkComponentMapping components{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    auto depth = DepthSurfaceTexture(context, depthWords, DecodeTextureResource(depthWords), components);
    Buffer results(context, 64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Compute compute(context, storage->View(), depth->View(), results, true, array, unorm);
    compute.Run(2);
    auto pixels = CheckPixels(results, nullptr, "resource transition resident seed");
    CheckClear(pixels, 0.375f);
    ResourceDispatch dispatch(context, *rebuilt, array, unorm);
    dispatch.Run(*rebuilt);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = (pixels[i] * 2u + static_cast<std::uint32_t>(i + 1) * 0x111u) & 0xffffu;
    compute.Run(1);
    pixels = CheckPixels(results, &pixels, "rebuilt resource storage and sample descriptors", unorm);
    dispatch.Run(deferred);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = (pixels[i] * 2u + static_cast<std::uint32_t>(i + 1) * 0x111u) & 0xffffu;
    compute.Run(1);
    pixels = CheckPixels(results, &pixels, "deferred resource storage and sample descriptors", unorm);
    ClearCachedTextures(context.device);
    ClearDepthSurfaces(context.device);
    std::cout << (array ? "array" : "2D") << (unorm ? " UNORM" : " UINT") << ": reusable generic-to-resident storage/sample transition passed\n";
}

void PendingClear(const Context& context, bool array, bool unorm) {
    Recorder recorder(context);
    recorder.Activate();
    const auto target = Target(0x40000);
    DepthSurfaceView(context, target);
    const auto depthWords = Descriptor(target.address, VK_FORMAT_R16_UNORM);
    const auto resource = DecodeTextureResource(Descriptor(target.address, unorm ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16_UINT, array));
    const VkComponentMapping components{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    auto depth = DepthSurfaceTexture(context, depthWords, DecodeTextureResource(depthWords), components);
    auto storage = CachedStorageSurface(context, resource);
    Buffer results(context, 64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Compute compute(context, storage->View(), depth->View(), results, true, array, unorm);
    compute.Run(2);
    std::weak_ptr<StorageTexture> pending = storage;
    ClearDepthSurfaces(context.device);
    ClearCachedTextures(context.device);
    storage.reset();
    depth.reset();
    Require(!pending.expired(), "pending depth transfer lost its image before submission");
    recorder.Sync();
    CheckClear(CheckPixels(results, nullptr, "pending clear, seed and cache removal"), 0.375f);
    Require(pending.expired(), "completed transfer kept the storage image alive");
}

}

int main() {
    try {
        std::unique_ptr<Device> device;
        try { device = std::make_unique<Device>(); }
        catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN")) throw;
            std::cout << "skipped, no usable Vulkan device: " << error.what() << '\n';
            return 77;
        }
        for (const bool unorm : {true, false}) {
            for (const bool array : {true, false}) {
                for (const bool formatless : {false, true}) {
                    Run(device->GetContext(), false, formatless, array, unorm);
                    Run(device->GetContext(), true, formatless, array, unorm);
                }
                PendingClear(device->GetContext(), array, unorm);
                ResourceTransition(device->GetContext(), array, unorm);
            }
        }
        std::cout << "depth storage coherence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
