#include "DepthStencilStorage_spv.h"
#include "DepthStencilStorageFormatless_spv.h"
#include "DepthStencilStorageArray_spv.h"
#include "DepthStencilStorageArrayFormatless_spv.h"
#include "StencilAttachment_vert_spv.h"
#include "StencilAttachment_frag_spv.h"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <SDL_loadso.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

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
            Require(available.shaderStorageImageExtendedFormats, "R8 storage images unavailable");
            Require(available.shaderStorageImageReadWithoutFormat && available.shaderStorageImageWriteWithoutFormat, "formatless storage image reads/writes unavailable");
            Require(available.fragmentStoresAndAtomics, "fragment buffer atomics unavailable");
            enabled.shaderStorageImageExtendedFormats = VK_TRUE;
            enabled.shaderStorageImageReadWithoutFormat = VK_TRUE;
            enabled.shaderStorageImageWriteWithoutFormat = VK_TRUE;
            enabled.fragmentStoresAndAtomics = VK_TRUE;
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
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
    Compute(const Context& context, VkImageView storage, VkImageView stencil, VkImageView depth, Buffer& results, bool formatless = false, bool array = false) : context(context) {
        try {
            const std::array<VkDescriptorSetLayoutBinding, 4> bindings{{
                {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
            }};
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.bindingCount = bindings.size();
            setInfo.pBindings = bindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
            const std::array<VkDescriptorPoolSize, 3> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}}};
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
            const std::array<VkDescriptorImageInfo, 3> images{{{VK_NULL_HANDLE, storage, VK_IMAGE_LAYOUT_GENERAL}, {sampler, stencil, VK_IMAGE_LAYOUT_GENERAL}, {sampler, depth, VK_IMAGE_LAYOUT_GENERAL}}};
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
            const std::span<const std::uint32_t> code = array
                ? (formatless ? std::span<const std::uint32_t>(DEPTH_STENCIL_STORAGE_ARRAY_FORMATLESS_SPV) : std::span<const std::uint32_t>(DEPTH_STENCIL_STORAGE_ARRAY_SPV))
                : (formatless ? std::span<const std::uint32_t>(DEPTH_STENCIL_STORAGE_FORMATLESS_SPV) : std::span<const std::uint32_t>(DEPTH_STENCIL_STORAGE_SPV));
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

DepthTarget Target(bool compressed = false) {
    auto queue = MakeState();
    queue.context[0x000] = 0;
    queue.context[0x002] = 0;
    queue.context[0x007] = (1u << 16u) | 3u;
    queue.context[0x00a] = 0x11;
    queue.context[0x00b] = std::bit_cast<std::uint32_t>(0.375f);
    queue.context[0x010] = 0x22900983;
    queue.context[0x011] = compressed ? 0x181 : 0x20000181;
    for (const auto offset : {0x012u, 0x014u}) queue.context[offset] = 0x100;
    for (const auto offset : {0x013u, 0x015u}) queue.context[offset] = 0x200;
    queue.context[0x10b] = 0x00050050;
    queue.context[0x10c] = 0x01ffff01;
    queue.context[0x10d] = 0x01000001;
    queue.context[0x200] = 0x00700711;
    return *DecodeState(queue).depth;
}

std::array<std::uint32_t, 8> Descriptor(std::uint64_t address, VkFormat format, bool array = false) {
    const auto guest = FindGuestTextureFormat(format, format == VK_FORMAT_R8_UINT ? 1 : 4);
    Require(guest.has_value(), "test format has no guest descriptor");
    return {static_cast<std::uint32_t>(address >> 8), static_cast<std::uint32_t>(address >> 40) | (*guest << 20u) | (3u << 30u), (1u << 14u), ((array ? 13u : 9u) << 28u) | (24u << 20u) | (7u << 9u) | (6u << 6u) | (5u << 3u) | 4u, 0, 0, 0, 0};
}

void CheckPixels(Buffer& results, std::uint32_t stencil, const char* phase) {
    results.Invalidate();
    std::array<std::uint32_t, 16> words{};
    std::memcpy(words.data(), results.Bytes().data(), sizeof(words));
    for (std::size_t i = 0; i < 8; ++i) {
        Require(words[i * 2] == stencil, std::string(phase) + ": stencil " + std::to_string(words[i * 2]) + " != " + std::to_string(stencil));
        Require(words[i * 2 + 1] == std::bit_cast<std::uint32_t>(0.375f), std::string(phase) + ": depth was changed by stencil work");
    }
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

class StencilDraw {
public:
    StencilDraw(const Context& context, VkRenderPass pass, Buffer& results, std::uint32_t reference) : context(context) {
        try {
            const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.bindingCount = 1;
            setInfo.pBindings = &binding;
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout stencil draw");
            const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &size;
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool stencil draw");
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1, &setLayout};
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets stencil draw");
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
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout stencil draw");
            const std::array<std::span<const std::uint32_t>, 2> codes{STENCIL_ATTACHMENT_vert_SPV, STENCIL_ATTACHMENT_frag_SPV};
            std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
            for (std::size_t i = 0; i < codes.size(); ++i) {
                VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                moduleInfo.codeSize = codes[i].size_bytes();
                moduleInfo.pCode = codes[i].data();
                Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &modules[i]), "vkCreateShaderModule stencil draw");
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
            depth.stencilTestEnable = VK_TRUE;
            depth.front = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xff, 0, reference};
            depth.back = depth.front;
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
            Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateGraphicsPipelines stencil draw");
        } catch (...) { release(); throw; }
    }
    ~StencilDraw() { release(); }
    void Record(VkCommandBuffer commands) {
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
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
    std::array<VkShaderModule, 2> modules{};
    VkPipeline pipeline = VK_NULL_HANDLE;
};

void AttachmentOperation(const Context& context, VkImageView view, std::uint32_t stencil, Buffer* results = nullptr) {
    VkAttachmentDescription attachment{};
    attachment.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = results == nullptr ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
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
        std::unique_ptr<StencilDraw> draw;
        if (results != nullptr) {
            std::fill(results->Bytes().begin(), results->Bytes().end(), std::byte{});
            draw = std::make_unique<StencilDraw>(context, pass, *results, stencil);
        }
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        const VkClearValue clear{.depthStencil = {0.375f, stencil}};
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

void Run(const Context& context, bool recorded, bool formatless, bool array) {
    std::unique_ptr<Recorder> recorder;
    if (recorded) {
        recorder = std::make_unique<Recorder>(context);
        recorder->Activate();
    }
    const auto target = Target();
    const auto view = DepthSurfaceView(context, target);
    const auto stencilWords = Descriptor(target.stencilAddress, VK_FORMAT_R8_UINT);
    const auto depthWords = Descriptor(target.address, VK_FORMAT_R32_SFLOAT);
    const auto resource = DecodeTextureResource(Descriptor(target.stencilAddress, VK_FORMAT_R8_UINT, array));
    const VkComponentMapping components{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    auto stencil = DepthSurfaceTexture(context, stencilWords, DecodeTextureResource(stencilWords), components);
    auto depth = DepthSurfaceTexture(context, depthWords, DecodeTextureResource(depthWords), components);
    Buffer results(context, 64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (recorder) recorder->Sync();
    AttachmentOperation(context, view, 0x11, &results);
    std::uint32_t initialPassed = 0;
    results.Invalidate();
    std::memcpy(&initialPassed, results.Bytes().data(), 4);
    Require(initialPassed == 8, "positive control: resident stencil clear was not visible to attachment tests");
    std::cout << (recorded ? "recorded " : "immediate ") << (formatless ? "formatless " : "typed ") << (array ? "array" : "2D") << ": attachment positive control passed\n";
    auto storage = CachedStorageSurface(context, resource);
    const auto initialVersion = storage->Version();
    Require(CachedStorageSurface(context, resource) == storage, "stencil storage lookup did not reuse the image");
    Require(storage->Version() == initialVersion, "repeated lookup fabricated a storage content change");
    Compute compute(context, storage->View(), stencil->View(), depth->View(), results, formatless, array);
    compute.Run(2);
    if (recorder) recorder->Sync();
    CheckPixels(results, 0x11, "resident seed and repeated lookup");
    compute.Run(0);
    storage->MarkDirty();
    compute.Run(1);
    if (recorder) recorder->Sync();
    CheckPixels(results, 0x15, "storage to sampled stencil");
    for (const auto reference : {0x15u, 0x14u}) {
        AttachmentOperation(context, DepthSurfaceView(context, target), reference, &results);
        std::uint32_t passed = 0;
        results.Invalidate();
        std::memcpy(&passed, results.Bytes().data(), 4);
        Require(passed == (reference == 0x15 ? 8u : 0u), "attachment stencil comparison did not see the storage result");
    }
    AttachmentOperation(context, DepthSurfaceView(context, target), 0x21);
    Require(CachedStorageSurface(context, resource) == storage, "attachment refresh replaced storage");
    compute.Run(2);
    if (recorder) recorder->Sync();
    CheckPixels(results, 0x21, "attachment to storage refresh");
    compute.Run(0);
    storage->MarkDirty();
    compute.Run(1);
    if (recorder) recorder->Sync();
    CheckPixels(results, 0x25, "second stencil round trip");
    const auto otherResource = DecodeTextureResource(Descriptor(target.stencilAddress, VK_FORMAT_R8_UINT, !array));
    auto other = CachedStorageSurface(context, otherResource);
    Require(other != storage, "2D and array descriptors reused a storage view of the wrong type");
    Compute otherCompute(context, other->View(), stencil->View(), depth->View(), results, formatless, !array);
    otherCompute.Run(2);
    if (recorder) recorder->Sync();
    CheckPixels(results, 0x25, "other storage view seeded from resident stencil");
    AttachmentOperation(context, DepthSurfaceView(context, target), 0x41);
    Require(CachedStorageSurface(context, otherResource) == other, "other storage view was replaced on refresh");
    otherCompute.Run(0);
    other->MarkDirty();
    Require(CachedStorageSurface(context, resource) == storage, "original storage view was replaced after alias write");
    compute.Run(2);
    if (recorder) recorder->Sync();
    CheckPixels(results, 0x45, "storage view types share resident revisions");
    compute.Run(1);
    if (recorder) recorder->Sync();
    CheckPixels(results, 0x45, "array alias write to sampled stencil");
    for (const auto invalid : {0, 1, 2, 3, 4, 5, 6, 7}) {
        auto mismatch = resource;
        if (invalid == 0) mismatch.width = 8;
        if (invalid == 1) mismatch.tileMode = TextureTileMode::kLinear;
        if (invalid == 2) mismatch.mipCount = 2;
        if (invalid == 3) { mismatch.dimension = TextureDimension::k2DArray; mismatch.depthOrLastArray = 1; }
        if (invalid == 4) mismatch.dccAddress = 0x40000;
        if (invalid == 5) mismatch.format = DecodeTextureResource(depthWords).format;
        if (invalid == 6) mismatch.baseArray = 1;
        if (invalid == 7) { mismatch.baseLevel = 1; mismatch.lastLevel = 1; }
        Refused([&] { CachedStorageSurface(context, mismatch); }, "stencil storage requires");
    }
    const auto depthRefusal = Refused([&] { CachedStorageSurface(context, DecodeTextureResource(depthWords)); }, "depth plane");
    for (const auto field : {"target format ", "4x2; descriptor format ", "dimension ", "levels 0-0 of 1", "mip 0", "slices 0-0", "tile ", "DCC 0x0"}) {
        Require(depthRefusal.find(field) != std::string::npos, std::string("depth refusal omitted ") + field);
    }
    DepthSurfaceView(context, Target(true));
    Refused([&] { CachedStorageSurface(context, resource); }, "HTILE stencil");
    DepthSurfaceView(context, target);
    auto unsupported = target;
    unsupported.address = 0x60000;
    unsupported.stencilAddress = 0x70000;
    unsupported.format = VK_FORMAT_D24_UNORM_S8_UINT;
    DepthSurfaceView(context, unsupported);
    auto unsupportedResource = resource;
    unsupportedResource.baseAddress = unsupported.stencilAddress;
    Refused([&] { CachedStorageSurface(context, unsupportedResource); }, "stencil storage requires");
    if (recorder) {
        compute.Run(0);
        storage->MarkDirty();
        compute.Run(1);
        ClearDepthSurfaces(context.device);
        storage.reset();
        recorder->Sync();
        CheckPixels(results, 0x45, "cleared cache during queued work");
    }
    ClearDepthSurfaces(context.device);
}

void PendingClear(const Context& context, bool array) {
    Recorder recorder(context);
    recorder.Activate();
    auto target = Target();
    target.address = 0x40000;
    target.stencilAddress = 0x50000;
    DepthSurfaceView(context, target);
    const auto stencilWords = Descriptor(target.stencilAddress, VK_FORMAT_R8_UINT);
    const auto depthWords = Descriptor(target.address, VK_FORMAT_R32_SFLOAT);
    const auto resource = DecodeTextureResource(Descriptor(target.stencilAddress, VK_FORMAT_R8_UINT, array));
    const VkComponentMapping components{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    auto stencil = DepthSurfaceTexture(context, stencilWords, DecodeTextureResource(stencilWords), components);
    auto depth = DepthSurfaceTexture(context, depthWords, DecodeTextureResource(depthWords), components);
    auto storage = CachedStorageSurface(context, resource);
    Buffer results(context, 64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Compute compute(context, storage->View(), stencil->View(), depth->View(), results, true, array);
    compute.Run(2);
    std::weak_ptr<StorageTexture> pending = storage;
    ClearDepthSurfaces(context.device);
    storage.reset();
    Require(!pending.expired(), "pending storage transfer lost its image before submission");
    recorder.Sync();
    CheckPixels(results, 0x11, "pending clear, seed and cache removal");
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
        for (const bool array : {true, false}) {
            for (const bool formatless : {false, true}) {
                Run(device->GetContext(), false, formatless, array);
                Run(device->GetContext(), true, formatless, array);
            }
            PendingClear(device->GetContext(), array);
        }
        std::cout << "stencil storage coherence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
