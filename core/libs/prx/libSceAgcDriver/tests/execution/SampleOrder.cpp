#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "SampleOrderTestFillDepth_spv.h"
#include "SampleOrderTestFillStencil_spv.h"
#include "SampleOrderTestFillVertex_spv.h"
#include "SampleOrderTestR16_spv.h"
#include "SampleOrderTestR32_spv.h"
#include "SampleOrderTestR8_spv.h"
#include "SampleOrderTestReadColor_spv.h"
#include "SampleOrderTestReadDepth_spv.h"
#include "SampleOrderTestReadStencil_spv.h"
#include "SampleOrderTestRG32_spv.h"
#include "SampleOrderTestRGBA32_spv.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Check;
using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::KeptSampleOrder;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::SampleOrder;
using AgcDriver::Graphics::SampleOrderMethod;
using AgcDriver::Graphics::SampleOrderPass;
using AgcDriver::Graphics::SampleOrderTarget;
using ShaderRecompiler::PixelInput;
using ShaderRecompiler::PixelInputBit;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t WaveSize = 64;
constexpr VkImageUsageFlags DepthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
constexpr VkImageUsageFlags ColorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

SampleOrder OrderFor(std::uint32_t samples) {
    if (samples == 2u) return {KeptSampleOrder, KeptSampleOrder, 0x76543201u, 0x76543201u};
    if (samples == 4u) return {KeptSampleOrder, 0x76542301u, 0x76540132u, 0x76541023u};
    return {0x01234567u, KeptSampleOrder, 0x10325476u, 0x45670123u};
}

std::uint32_t Pick(std::uint32_t word, std::uint32_t index) {
    return (word >> (index * 4u)) & 7u;
}

std::uint32_t Quad(std::uint32_t x, std::uint32_t y) {
    return (x & 1u) | ((y & 1u) << 1u);
}

std::string At(std::uint32_t x, std::uint32_t y, std::uint32_t sample) {
    return "sample " + std::to_string(sample) + " of pixel (" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

void FullBarrier(const Context& context, VkCommandBuffer commands) {
    AgcDriver::Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
}

void HostBarrier(const Context& context, VkCommandBuffer commands) {
    AgcDriver::Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
}

class Image {
public:
    Image(const Context& context, VkFormat format, std::uint32_t samples, VkExtent2D extent, VkImageUsageFlags usage, VkImageCreateFlags flags) : context(context) {
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.flags = flags;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = {extent.width, extent.height, 1u};
            info.mipLevels = 1u;
            info.arrayLayers = 1u;
            info.samples = static_cast<VkSampleCountFlagBits>(samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage sample order test");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory sample order test");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0u), "vkBindImageMemory sample order test");
        } catch (...) {
            release();
            throw;
        }
    }
    ~Image() { release(); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    VkImage Handle() const { return image; }
    VkImageView View(VkFormat format, VkImageAspectFlags aspect, VkImageUsageFlags usage) {
        VkImageViewUsageCreateInfo viewUsage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
        viewUsage.usage = usage;
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.pNext = &viewUsage;
        info.image = image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = format;
        info.subresourceRange = {aspect, 0u, 1u, 0u, 1u};
        VkImageView view = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &view), "vkCreateImageView sample order test");
        views.push_back(view);
        return view;
    }
    void ToGeneral(VkCommandBuffer commands, VkImageAspectFlags aspect) const {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {aspect, 0u, 1u, 0u, 1u};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);
    }

private:
    void release() noexcept {
        for (const auto view : views) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
    }
    Context context;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::vector<VkImageView> views;
};

struct Binding {
    std::uint32_t binding;
    VkDescriptorType type;
};

class Program {
public:
    Program(const Context& context, std::span<const std::uint32_t> compute, std::span<const Binding> bindings, std::uint32_t pushBytes, VkShaderStageFlags stages = VK_SHADER_STAGE_COMPUTE_BIT) : context(context), stages(stages), pushBytes(pushBytes) {
        try {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            std::vector<VkDescriptorPoolSize> sizes;
            for (const auto& binding : bindings) {
                layoutBindings.push_back({binding.binding, binding.type, 1u, stages, nullptr});
                sizes.push_back({binding.type, 1u});
                types.push_back(binding);
            }
            VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layoutInfo.bindingCount = static_cast<std::uint32_t>(layoutBindings.size());
            layoutInfo.pBindings = layoutBindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &setLayout), "sample order test descriptor layout");
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1u;
            poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
            poolInfo.pPoolSizes = sizes.data();
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "sample order test descriptor pool");
            const VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1u, &setLayout};
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocate, &set), "sample order test descriptor set");
            const VkPushConstantRange range{stages, 0u, pushBytes};
            VkPipelineLayoutCreateInfo layoutCreate{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutCreate.setLayoutCount = 1u;
            layoutCreate.pSetLayouts = &setLayout;
            layoutCreate.pushConstantRangeCount = 1u;
            layoutCreate.pPushConstantRanges = &range;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutCreate, nullptr, &layout), "sample order test pipeline layout");
            if (compute.empty()) return;
            const auto shader = Module(compute);
            VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0u, VK_SHADER_STAGE_COMPUTE_BIT, shader, "main", nullptr};
            info.layout = layout;
            const auto created = context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1u, &info, nullptr, &pipeline);
            context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, shader, nullptr);
            Check(created, "sample order test compute pipeline");
        } catch (...) {
            release();
            throw;
        }
    }
    ~Program() { release(); }
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    VkShaderModule Module(std::span<const std::uint32_t> code) const {
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size_bytes();
        info.pCode = code.data();
        VkShaderModule module = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &info, nullptr, &module), "sample order test shader module");
        return module;
    }
    void Image(std::uint32_t binding, VkImageView view) const {
        const VkDescriptorImageInfo image{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1u;
        write.descriptorType = typeOf(binding);
        write.pImageInfo = &image;
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1u, &write, 0u, nullptr);
    }
    void Storage(std::uint32_t binding, VkBuffer buffer) const {
        const VkDescriptorBufferInfo info{buffer, 0u, VK_WHOLE_SIZE};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1u;
        write.descriptorType = typeOf(binding);
        write.pBufferInfo = &info;
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1u, &write, 0u, nullptr);
    }
    void Bind(VkCommandBuffer commands, VkPipelineBindPoint point, const void* push) const {
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, point, layout, 0u, 1u, &set, 0u, nullptr);
        context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, stages, 0u, pushBytes, push);
    }
    void Dispatch(VkCommandBuffer commands, const void* push, VkExtent2D extent) const {
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, push);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, (extent.width + 7u) / 8u, (extent.height + 7u) / 8u, 1u);
    }
    VkPipelineLayout Layout() const { return layout; }

private:
    VkDescriptorType typeOf(std::uint32_t binding) const {
        for (const auto& known : types) {
            if (known.binding == binding) return known.type;
        }
        throw std::runtime_error("the sample order test program has no binding " + std::to_string(binding));
    }
    void release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }
    Context context;
    VkShaderStageFlags stages;
    std::uint32_t pushBytes;
    std::vector<Binding> types;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

class DepthFill {
public:
    DepthFill(const Context& context, VkFormat format, std::uint32_t samples, VkExtent2D extent, VkImageView attachment, bool stencil, VkBuffer values) : context(context), extent(extent), samples(samples) {
        const std::array<Binding, 1> bindings{{{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}}};
        program = std::make_unique<Program>(context, std::span<const std::uint32_t>{}, bindings, 8u, VK_SHADER_STAGE_FRAGMENT_BIT);
        program->Storage(0u, values);
        try {
            const bool hasStencil = AgcDriver::Graphics::HasStencil(format);
            VkAttachmentDescription description{};
            description.format = format;
            description.samples = static_cast<VkSampleCountFlagBits>(samples);
            description.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.stencilStoreOp = hasStencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
            description.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
            const VkAttachmentReference reference{0u, VK_IMAGE_LAYOUT_GENERAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pDepthStencilAttachment = &reference;
            VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            passInfo.attachmentCount = 1u;
            passInfo.pAttachments = &description;
            passInfo.subpassCount = 1u;
            passInfo.pSubpasses = &subpass;
            Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &pass), "sample order test fill pass");
            VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebufferInfo.renderPass = pass;
            framebufferInfo.attachmentCount = 1u;
            framebufferInfo.pAttachments = &attachment;
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1u;
            Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "sample order test fill framebuffer");
            const auto vertex = program->Module(SAMPLE_ORDER_TEST_FILLVERTEX_SPV);
            const auto fragment = stencil ? program->Module(SAMPLE_ORDER_TEST_FILLSTENCIL_SPV) : program->Module(SAMPLE_ORDER_TEST_FILLDEPTH_SPV);
            const std::array<VkPipelineShaderStageCreateInfo, 2> stages{{
                {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0u, VK_SHADER_STAGE_VERTEX_BIT, vertex, "main", nullptr},
                {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0u, VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr},
            }};
            VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
            const VkRect2D scissor{{0, 0}, extent};
            VkPipelineViewportStateCreateInfo viewports{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewports.viewportCount = 1u;
            viewports.pViewports = &viewport;
            viewports.scissorCount = 1u;
            viewports.pScissors = &scissor;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = static_cast<VkSampleCountFlagBits>(samples);
            multisample.sampleShadingEnable = VK_TRUE;
            multisample.minSampleShading = 1.0f;
            VkStencilOpState replace{};
            replace.failOp = VK_STENCIL_OP_REPLACE;
            replace.passOp = VK_STENCIL_OP_REPLACE;
            replace.depthFailOp = VK_STENCIL_OP_REPLACE;
            replace.compareOp = VK_COMPARE_OP_ALWAYS;
            replace.compareMask = 0xffu;
            replace.writeMask = 0xffu;
            VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            depth.depthTestEnable = VK_TRUE;
            depth.depthWriteEnable = VK_TRUE;
            depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
            depth.stencilTestEnable = stencil ? VK_TRUE : VK_FALSE;
            depth.front = replace;
            depth.back = replace;
            VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            info.stageCount = static_cast<std::uint32_t>(stages.size());
            info.pStages = stages.data();
            info.pVertexInputState = &input;
            info.pInputAssemblyState = &assembly;
            info.pViewportState = &viewports;
            info.pRasterizationState = &raster;
            info.pMultisampleState = &multisample;
            info.pDepthStencilState = &depth;
            info.pColorBlendState = &blend;
            info.layout = program->Layout();
            info.renderPass = pass;
            const auto created = context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, VK_NULL_HANDLE, 1u, &info, nullptr, &pipeline);
            context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, fragment, nullptr);
            context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, vertex, nullptr);
            Check(created, "sample order test fill pipeline");
        } catch (...) {
            release();
            throw;
        }
    }
    ~DepthFill() { release(); }
    DepthFill(const DepthFill&) = delete;
    DepthFill& operator=(const DepthFill&) = delete;
    void Run(VkCommandBuffer commands) const {
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = pass;
        begin.framebuffer = framebuffer;
        begin.renderArea = {{0, 0}, extent};
        context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        const std::array<std::uint32_t, 2> push{extent.width, samples};
        program->Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, push.data());
        context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 3u, 1u, 0u, 0u);
        context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
    }

private:
    void release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        if (pass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, pass, nullptr);
    }
    Context context;
    VkExtent2D extent;
    std::uint32_t samples;
    std::unique_ptr<Program> program;
    VkRenderPass pass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

using Samples = std::vector<std::array<std::uint32_t, 4>>;

template<typename Record>
void Submit(const Context& context, Record record) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    AgcDriver::Graphics::CommandBatch batch(context);
    record(batch.Handle());
    batch.SubmitAndWait();
}

Samples ReadValues(AgcDriver::Graphics::Buffer& buffer, std::size_t count) {
    buffer.Invalidate();
    Samples values(count);
    std::memcpy(values.data(), buffer.Bytes().data(), count * sizeof(values[0]));
    return values;
}

void CheckOrdered(const Samples& before, const Samples& after, VkExtent2D extent, VkExtent2D region, std::uint32_t samples, const SampleOrder& order, bool toHost, const std::string& what) {
    std::uint32_t moved = 0;
    std::string first;
    std::array<std::uint32_t, 4> wrong{};
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        for (std::uint32_t x = 0; x < extent.width; ++x) {
            const bool inside = x < region.width && y < region.height;
            const auto word = order[Quad(x, y)];
            const auto map = toHost ? word : AgcDriver::Graphics::InvertSampleOrder(word);
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const auto base = (y * extent.width + x) * samples;
                const auto source = inside ? Pick(map, sample) : sample;
                if (after[base + sample] != before[base + source]) {
                    if (first.empty()) {
                        char held[96];
                        std::snprintf(held, sizeof(held), " holds 0x%08x 0x%08x, expected 0x%08x 0x%08x (sample %u)", after[base + sample][0], after[base + sample][1], before[base + source][0], before[base + source][1], source);
                        first = At(x, y, sample) + held;
                    }
                    ++wrong[Quad(x, y)];
                }
                if (source != sample) ++moved;
            }
        }
    }
    if (!first.empty()) {
        std::string classes;
        for (std::uint32_t quad = 0; quad < wrong.size(); ++quad) {
            const bool kept = order[quad] == 0u || order[quad] == KeptSampleOrder;
            classes += (quad == 0u ? "" : ", ") + std::to_string(wrong[quad]) + " in pixels with x " + ((quad & 1u) != 0u ? "odd" : "even") + " and y " + ((quad & 2u) != 0u ? "odd" : "even") + (kept ? " (kept)" : " (reordered)");
        }
        throw std::runtime_error(what + ": " + first + "; wrong samples: " + classes);
    }
    Require(moved != 0, what + ": the order moved no sample");
}

const std::array<std::pair<VkFormat, std::uint32_t>, 5> ColorFormats{{
    {VK_FORMAT_R8_UNORM, 1u}, {VK_FORMAT_R16_SFLOAT, 2u}, {VK_FORMAT_R8G8B8A8_SRGB, 4u}, {VK_FORMAT_R16G16B16A16_SFLOAT, 8u}, {VK_FORMAT_R32G32B32A32_SFLOAT, 16u}
}};

std::span<const std::uint32_t> TexelCode(VkFormat alias) {
    switch (alias) {
    case VK_FORMAT_R8_UINT: return SAMPLE_ORDER_TEST_R8_SPV;
    case VK_FORMAT_R16_UINT: return SAMPLE_ORDER_TEST_R16_SPV;
    case VK_FORMAT_R32_UINT: return SAMPLE_ORDER_TEST_R32_SPV;
    case VK_FORMAT_R32G32_UINT: return SAMPLE_ORDER_TEST_RG32_SPV;
    case VK_FORMAT_R32G32B32A32_UINT: return SAMPLE_ORDER_TEST_RGBA32_SPV;
    default: throw std::runtime_error("the sample order test has no texel program for this alias");
    }
}

std::uint32_t AliasMask(std::uint32_t elementBytes, std::uint32_t component) {
    if (elementBytes == 1u) return component == 0u ? 0xffu : 0u;
    if (elementBytes == 2u) return component == 0u ? 0xffffu : 0u;
    if (elementBytes == 4u) return component == 0u ? ~0u : 0u;
    if (elementBytes == 8u) return component < 2u ? ~0u : 0u;
    return ~0u;
}

Samples Masked(Samples values, std::uint32_t elementBytes) {
    for (auto& value : values) {
        for (std::uint32_t component = 0; component < 4u; ++component) value[component] &= AliasMask(elementBytes, component);
    }
    return values;
}

constexpr VkExtent2D SwapExtent{20u, 12u};
constexpr VkExtent2D SwapRegion{16u, 10u};

bool ColorSwaps(const Context& context, VkFormat format, std::uint32_t elementBytes, std::uint32_t samples) {
    const auto alias = AgcDriver::Graphics::SampleOrderStorageFormat(context, format, elementBytes, samples);
    const auto name = "the " + std::to_string(samples) + "x color format " + std::to_string(format);
    if (alias == VK_FORMAT_UNDEFINED) {
        std::printf("%s has no %u-sample storage alias here, not swapped\n", name.c_str(), samples);
        return false;
    }
    const auto order = OrderFor(samples);
    Image image(context, format, samples, SwapExtent, ColorUsage, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT);
    const auto view = image.View(alias, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_USAGE_STORAGE_BIT);
    const auto count = static_cast<std::size_t>(SwapExtent.width) * SwapExtent.height * samples;
    AgcDriver::Graphics::Buffer values(context, count * 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Samples before(count);
    for (std::size_t index = 0; index < count; ++index) {
        for (std::uint32_t component = 0; component < 4u; ++component) before[index][component] = static_cast<std::uint32_t>(index * 0x9e3779b1u + component * 0x85ebca6bu + 0x1000u * (index % samples)) & AliasMask(elementBytes, component);
        before[index][0] = (before[index][0] & ~7u & AliasMask(elementBytes, 0u)) | static_cast<std::uint32_t>(index % samples);
    }
    std::memcpy(values.Bytes().data(), before.data(), count * sizeof(before[0]));
    const std::array<Binding, 2> bindings{{{0u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}, {1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}}};
    Program texels(context, TexelCode(alias), bindings, 16u);
    texels.Image(0u, view);
    texels.Storage(1u, values.Handle());
    const std::array<std::uint32_t, 4> store{SwapExtent.width, SwapExtent.height, samples, 1u};
    const std::array<std::uint32_t, 4> load{SwapExtent.width, SwapExtent.height, samples, 0u};
    const SampleOrderTarget target{image.Handle(), view, alias, SwapExtent, samples, SampleOrderMethod::Storage, nullptr};
    const SampleOrderPass pass{order, SwapRegion, {}, false};
    std::vector<std::shared_ptr<void>> kept;
    Submit(context, [&](VkCommandBuffer commands) {
        image.ToGeneral(commands, VK_IMAGE_ASPECT_COLOR_BIT);
        texels.Dispatch(commands, store.data(), SwapExtent);
        FullBarrier(context, commands);
        texels.Dispatch(commands, load.data(), SwapExtent);
        HostBarrier(context, commands);
    });
    Require(Masked(ReadValues(values, count), elementBytes) == before, name + ": the stored samples did not read back");
    Submit(context, [&](VkCommandBuffer commands) {
        AgcDriver::Graphics::RecordSampleOrder(context, commands, pass, std::span(&target, 1u), true, kept);
        FullBarrier(context, commands);
        texels.Dispatch(commands, load.data(), SwapExtent);
        HostBarrier(context, commands);
    });
    CheckOrdered(before, Masked(ReadValues(values, count), elementBytes), SwapExtent, SwapRegion, samples, order, true, name + " put in the host's order");
    Submit(context, [&](VkCommandBuffer commands) {
        AgcDriver::Graphics::RecordSampleOrder(context, commands, pass, std::span(&target, 1u), false, kept);
        FullBarrier(context, commands);
        texels.Dispatch(commands, load.data(), SwapExtent);
        HostBarrier(context, commands);
    });
    Require(Masked(ReadValues(values, count), elementBytes) == before, name + ": putting the samples back in the guest's order did not restore every sample bit for bit");
    std::printf("%s round-trips through its %u-byte storage alias\n", name.c_str(), elementBytes);
    return true;
}

bool DepthSampleable(const Context& context, VkFormat format, std::uint32_t samples) {
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    constexpr VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if ((properties.optimalTilingFeatures & needed) != needed) return false;
    VkImageFormatProperties supported{};
    return context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, DepthUsage, 0u, &supported) == VK_SUCCESS && (supported.sampleCounts & samples) != 0u;
}

std::string MethodName(SampleOrderMethod method) {
    switch (method) {
    case SampleOrderMethod::Depth: return "a depth pass";
    case SampleOrderMethod::DepthStencilExport: return "a depth pass exporting stencil";
    case SampleOrderMethod::DepthStencilBits: return "a depth pass and eight stencil bit passes";
    default: return "storage";
    }
}

bool DepthSwaps(const Context& context, VkFormat format, std::uint32_t samples, SampleOrderMethod method) {
    const bool stencil = method != SampleOrderMethod::Depth;
    const auto name = "the " + std::to_string(samples) + "x depth format " + std::to_string(format) + " through " + MethodName(method);
    if (!DepthSampleable(context, format, samples)) {
        std::printf("%s: the format cannot be sampled at this sample count here, not swapped\n", name.c_str());
        return false;
    }
    if (stencil && !context.shaderStencilExport) {
        std::printf("%s: the device has no stencil export to fill the test's stencil, not swapped\n", name.c_str());
        return false;
    }
    const auto order = OrderFor(samples);
    const bool hasStencil = AgcDriver::Graphics::HasStencil(format);
    const VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
    Image image(context, format, samples, SwapExtent, DepthUsage, 0u);
    const auto attachment = image.View(format, aspects, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    const auto depthView = image.View(format, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto stencilView = stencil ? image.View(format, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_USAGE_SAMPLED_BIT) : VK_NULL_HANDLE;
    const auto count = static_cast<std::size_t>(SwapExtent.width) * SwapExtent.height * samples;
    AgcDriver::Graphics::Buffer fill(context, count * 8u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    AgcDriver::Graphics::Buffer values(context, count * 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::vector<std::array<std::uint32_t, 2>> filled(count);
    const bool unorm16 = format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D16_UNORM_S8_UINT;
    for (std::size_t index = 0; index < count; ++index) {
        const auto key = static_cast<std::uint32_t>(index + 1u);
        filled[index] = {std::bit_cast<std::uint32_t>(unorm16 ? static_cast<float>(key) / 65535.0f : static_cast<float>(key) / 4096.0f), (key * 37u) & 0xffu};
    }
    std::memcpy(fill.Bytes().data(), filled.data(), count * sizeof(filled[0]));
    DepthFill raster(context, format, samples, SwapExtent, attachment, stencil, fill.Handle());
    const std::array<Binding, 3> bindings{{{0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE}, {1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE}, {2u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}}};
    Program read(context, stencil ? std::span<const std::uint32_t>(SAMPLE_ORDER_TEST_READSTENCIL_SPV) : std::span<const std::uint32_t>(SAMPLE_ORDER_TEST_READDEPTH_SPV), bindings, 12u);
    read.Image(0u, depthView);
    if (stencil) read.Image(1u, stencilView);
    read.Storage(2u, values.Handle());
    const std::array<std::uint32_t, 3> push{SwapExtent.width, SwapExtent.height, samples};
    const SampleOrderTarget target{image.Handle(), attachment, format, SwapExtent, samples, method, nullptr};
    const SampleOrderPass pass{order, SwapRegion, {}, false};
    std::vector<std::shared_ptr<void>> kept;
    Submit(context, [&](VkCommandBuffer commands) {
        image.ToGeneral(commands, aspects);
        raster.Run(commands);
        FullBarrier(context, commands);
        read.Dispatch(commands, push.data(), SwapExtent);
        HostBarrier(context, commands);
    });
    const auto before = ReadValues(values, count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto key = static_cast<std::uint32_t>(index + 1u);
        const auto depth = std::bit_cast<float>(before[index][0]);
        const bool depthHeld = unorm16 ? static_cast<std::uint32_t>(depth * 65535.0f + 0.5f) == key : before[index][0] == filled[index][0];
        Require(depthHeld && (!stencil || before[index][1] == filled[index][1]), name + ": the filled sample " + std::to_string(index) + " did not read back");
    }
    Submit(context, [&](VkCommandBuffer commands) {
        AgcDriver::Graphics::RecordSampleOrder(context, commands, pass, std::span(&target, 1u), true, kept);
        FullBarrier(context, commands);
        read.Dispatch(commands, push.data(), SwapExtent);
        HostBarrier(context, commands);
    });
    CheckOrdered(before, ReadValues(values, count), SwapExtent, SwapRegion, samples, order, true, name + " put in the host's order");
    Submit(context, [&](VkCommandBuffer commands) {
        AgcDriver::Graphics::RecordSampleOrder(context, commands, pass, std::span(&target, 1u), false, kept);
        FullBarrier(context, commands);
        read.Dispatch(commands, push.data(), SwapExtent);
        HostBarrier(context, commands);
    });
    Require(ReadValues(values, count) == before, name + ": putting the samples back in the guest's order did not restore every sample bit for bit");
    std::printf("%s round-trips\n", name.c_str());
    return true;
}

std::vector<std::uint32_t> SwapSampleCounts(const Context& context) {
    std::vector<std::uint32_t> counts;
    for (const std::uint32_t samples : {2u, 4u, 8u}) {
        if ((context.limits.framebufferColorSampleCounts & samples) != 0u && (context.limits.framebufferDepthSampleCounts & samples) != 0u) counts.push_back(samples);
    }
    return counts;
}

std::uint32_t SwapPasses(const Context& context) {
    std::uint32_t swapped = 0;
    for (const auto samples : SwapSampleCounts(context)) {
        for (const auto& [format, bytes] : ColorFormats) swapped += ColorSwaps(context, format, bytes, samples) ? 1u : 0u;
        swapped += DepthSwaps(context, VK_FORMAT_D32_SFLOAT, samples, SampleOrderMethod::Depth) ? 1u : 0u;
        swapped += DepthSwaps(context, VK_FORMAT_D16_UNORM, samples, SampleOrderMethod::Depth) ? 1u : 0u;
        for (const auto format : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT}) {
            swapped += DepthSwaps(context, format, samples, SampleOrderMethod::Depth) ? 1u : 0u;
            swapped += DepthSwaps(context, format, samples, SampleOrderMethod::DepthStencilExport) ? 1u : 0u;
            swapped += DepthSwaps(context, format, samples, SampleOrderMethod::DepthStencilBits) ? 1u : 0u;
        }
    }
    return swapped;
}

using Vertices = std::vector<std::array<float, 4>>;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> NullExportCode{0xf8001890, 0x00000000, 0xbf810000};

constexpr std::uint32_t RemapWidth = 64;
constexpr std::uint32_t RemapHeight = 8;
alignas(256) std::array<std::uint32_t, RemapWidth * RemapHeight * 8> Invocations{};
alignas(256) std::array<std::array<std::uint32_t, 17>, 4> StoreCodes{};
alignas(256) std::array<std::array<std::uint32_t, 5>, 8> MaskCodes{};
alignas(256) constexpr std::array<std::uint32_t, 7> SampleColorCode{0xd5480007, 0x02111106, 0x7e0e0d07, 0xf800180f, 0x07070100, 0xbf810000, 0xbf810000};
alignas(256) std::array<std::byte, 10 * 256 * 1024> DepthMemory{};

std::span<const std::uint32_t> StoreCode(std::uint32_t samples) {
    const auto shift = static_cast<std::uint32_t>(std::countr_zero(samples));
    auto& code = StoreCodes[shift];
    code = {0x7e100f04, 0x7e120f05, 0x34121286, 0x4a101308, 0x34101080u | shift, 0xd548000a, 0x02111106, 0x4a101508, 0x34101082, 0x7e160281, 0xe0705000, 0x80000b08, 0xbf8c3f70, 0xf8001890, 0x00000000, 0xbf810000, 0xbf810000};
    return code;
}

std::span<const std::uint32_t> MaskCode(std::size_t slot, std::uint32_t mask) {
    auto& code = MaskCodes[slot];
    code = {0x7e1002ffu, mask, 0xf8001884u, 0x00080000u, 0xbf810000u};
    return code;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, stride == 0u ? 0x31016facu : 0x01016facu};
}

struct Pass {
    const Vertices* vertices = nullptr;
    const void* vertexMemory = nullptr;
    std::span<const std::uint32_t> code;
    bool perSample = false;
    std::uint32_t shaderControl = 0u;
    std::uint32_t colorFormat = 0u;
    std::uint32_t samples = 4u;
    VkExtent2D extent{RemapWidth, RemapHeight};
    std::optional<AgcDriver::Graphics::DepthTarget> depth;
    bool depthWrite = false;
    VkCompareOp depthCompare = VK_COMPARE_OP_ALWAYS;
    std::optional<AgcDriver::Graphics::ColorTarget> color;
    std::optional<std::array<std::uint8_t, 32>> locations;
    std::optional<SampleOrder> order;
    std::array<std::uint16_t, 4> quadSampleMask{0xffffu, 0xffffu, 0xffffu, 0xffffu};
    bool storesInvocations = false;
    std::function<void()> beforeReplay;
    bool buildsNoRecipe = false;
};

void Draw(AgcDriver::VulkanDevice& device, const Context& context, const Pass& pass) {
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(pass.vertexMemory != nullptr ? pass.vertexMemory : pass.vertices->data(), 16u, static_cast<std::uint32_t>(pass.vertices->size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.hasColorTarget = pass.color.has_value();
    if (pass.color) {
        VkPipelineColorBlendAttachmentState blend{};
        blend.colorWriteMask = 0xf;
        state.colors = {*pass.color};
        state.color = *pass.color;
        state.blends = {blend};
        state.blend = blend;
    }
    state.renderExtent = pass.extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(pass.extent.height), static_cast<float>(pass.extent.width), -static_cast<float>(pass.extent.height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, pass.extent};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blendConstants = {};
    state.samples = pass.samples;
    state.perSampleShading = pass.perSample;
    state.quadSampleMask = pass.quadSampleMask;
    state.pixelEarlyZ = AgcDriver::Graphics::PixelEarlyZ(pass.shaderControl);
    if (pass.locations) {
        state.customSampleLocations = true;
        state.sampleLocations = *pass.locations;
        for (std::uint32_t pixel = 1; pixel < 4u; ++pixel) {
            for (std::uint32_t sample = 0; sample < pass.samples; ++sample) state.sampleLocationGrid |= state.sampleLocations[pixel * 8u + sample] != state.sampleLocations[sample];
        }
    }
    if (pass.depth) {
        state.depth = pass.depth;
        state.depthTest = true;
        state.depthWrite = pass.depthWrite;
        state.depthCompare = pass.depthCompare;
    }

    const std::uint32_t inputs = pass.perSample ? PixelInputBit(PixelInput::PerspectiveSample) | PixelInputBit(PixelInput::LinearSample) | PixelInputBit(PixelInput::PositionX) | PixelInputBit(PixelInput::PositionY) | PixelInputBit(PixelInput::Ancillary) : 0u;
    const AgcDriver::Registers registers{{0x1b3, inputs}, {0x1b4, inputs}, {0x1b6, 0u}, {0x1c5, pass.colorFormat}, {0x203, pass.shaderControl}};
    auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(registers, AgcDriver::Graphics::ExportMappings(state));
    Require(pixel.sampleShading == pass.perSample, "the pixel inputs did not select the requested shading rate");
    pixel.iteratesSamples = state.perSampleShading;
    pixel.sampleLocations = AgcDriver::Graphics::PixelSampleLocations(state, pixel);
    const auto order = pass.order ? *pass.order : state.sampleLocationGrid ? AgcDriver::Graphics::HostSampleLocationsFor(context, state).order : SampleOrder{};
    pixel.sampleOrder = order;
    pixel.quadPixelMask = AgcDriver::Graphics::QuadPixelMask(state);
    pixel.quadSampleMask = AgcDriver::Graphics::QuadSampleMasks(state, order);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    if (pass.storesInvocations) {
        const auto invocations = BufferDescriptor(Invocations.data(), 0u, static_cast<std::uint32_t>(sizeof(Invocations)));
        std::copy(invocations.begin(), invocations.end(), pixelUserData.begin());
    }
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pass.code.data()), std::as_bytes(pass.code)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pass.code.data()), pass.code, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    std::vector<AgcDriver::Graphics::CompiledShader> shaders{{ShaderStage::Vertex, &vertexResult, 0}};
    ShaderRecompiler::RecompileResult geometry;
    if (pixelResult.barycentricEmulation.active) {
        geometry = ShaderRecompiler::BuildBarycentricGeometryShader(vertexResult, pixelResult, target, device.GeometryLimits());
        shaders.push_back({ShaderStage::Geometry, &geometry, 0});
    }
    shaders.push_back({ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)});
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(pass.vertices->size()), 0, 1, 0, false};
    if (!pass.beforeReplay && !pass.buildsNoRecipe) {
        AgcDriver::Graphics::Draw(context, state, draw, shaders);
        return;
    }
    std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
    AgcDriver::Graphics::Draw(context, state, draw, shaders, {}, &recipe);
    if (pass.buildsNoRecipe) {
        Require(recipe == nullptr, "a draw that reorders its samples built a recipe");
        return;
    }
    Require(recipe != nullptr, "the recorded draw built no recipe");
    pass.beforeReplay();
    const auto replayed = AgcDriver::Graphics::DrawWithRecipe(context, state, draw, shaders, {}, *recipe);
    Require(replayed.recorded, std::string("the draw did not replay its recipe: ") + AgcDriver::Graphics::DrawRecipeMissName(replayed.miss));
}

void Settle(AgcDriver::VulkanDevice& device) {
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.SubmitRecorded(true);
    }
    device.WaitIdle();
}

Vertices Rectangle(float left, float right, float z) {
    return {{left, -1.0f, z, 1.0f}, {right, -1.0f, z, 1.0f}, {right, 1.0f, z, 1.0f}, {left, -1.0f, z, 1.0f}, {right, 1.0f, z, 1.0f}, {left, 1.0f, z, 1.0f}};
}

Vertices Screen(float z) {
    return {{-1.0f, -1.0f, z, 1.0f}, {3.0f, -1.0f, z, 1.0f}, {-1.0f, 3.0f, z, 1.0f}};
}

float StandardX(std::uint32_t samples, std::uint32_t sample) {
    static constexpr std::array<float, 2> two{0.75f, 0.25f};
    static constexpr std::array<float, 4> four{0.375f, 0.875f, 0.125f, 0.625f};
    static constexpr std::array<float, 8> eight{0.5625f, 0.4375f, 0.8125f, 0.3125f, 0.1875f, 0.0625f, 0.6875f, 0.9375f};
    return samples == 2u ? two[sample] : samples == 4u ? four[sample] : eight[sample];
}

void CheckStored(std::uint32_t samples, const std::string& what, const std::function<std::uint32_t(std::uint32_t, std::uint32_t)>& expected) {
    for (std::uint32_t y = 0; y < RemapHeight; ++y) {
        for (std::uint32_t x = 0; x < RemapWidth; ++x) {
            const auto mask = expected(x, y);
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const auto stored = Invocations[(y * RemapWidth + x) * samples + sample];
                Require(stored == ((mask >> sample) & 1u), what + ": guest " + At(x, y, sample) + (stored != 0u ? " ran" : " did not run") + ", expected guest mask " + std::to_string(mask));
            }
        }
    }
}

std::uint32_t GuestBits(std::uint32_t hostBits, std::uint32_t word, std::uint32_t samples) {
    std::uint32_t guest = 0;
    for (std::uint32_t host = 0; host < samples; ++host) {
        if (((hostBits >> host) & 1u) != 0u) guest |= 1u << Pick(word, host);
    }
    return guest;
}

void SampleIndexRemap(AgcDriver::VulkanDevice& device, const Context& context, std::uint32_t samples) {
    const auto order = OrderFor(samples);
    const auto toNdc = [](float pixel) { return pixel * 2.0f / static_cast<float>(RemapWidth) - 1.0f; };
    const auto rectangle = Rectangle(toNdc(2.5f), toNdc(5.5f), 0.5f);
    for (const bool remapped : {false, true}) {
        Invocations.fill(0u);
        Pass pass;
        pass.vertices = &rectangle;
        pass.code = StoreCode(samples);
        pass.perSample = true;
        pass.samples = samples;
        pass.order = remapped ? order : SampleOrder{};
        pass.storesInvocations = true;
        Draw(device, context, pass);
        Settle(device);
        CheckStored(samples, std::to_string(samples) + "x SampleID " + (remapped ? "under the test's sample order" : "in the host's order"), [&](std::uint32_t x, std::uint32_t y) {
            std::uint32_t host = 0;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const auto at = static_cast<float>(x) + StandardX(samples, sample);
                if (at > 2.5f && at < 5.5f) host |= 1u << sample;
            }
            return remapped ? GuestBits(host, order[Quad(x, y)], samples) : host;
        });
    }
    std::printf("%ux SampleID reads the guest's sample index in all four pixels of the 2x2 grid\n", samples);
}

void CoverageRemap(AgcDriver::VulkanDevice& device, const Context& context, std::uint32_t samples) {
    const auto order = OrderFor(samples);
    const auto back = Screen(0.25f), middle = Screen(0.5f), probe = Screen(0.375f);
    const AgcDriver::Graphics::DepthTarget depth = [&] {
        AgcDriver::Graphics::DepthTarget target{reinterpret_cast<std::uintptr_t>(DepthMemory.data()) + 256u * 1024u * static_cast<std::uint32_t>(std::countr_zero(samples)), 0u, {RemapWidth, RemapHeight}, VK_FORMAT_D32_SFLOAT, 0.0f, 0};
        target.samples = samples;
        return target;
    }();
    const std::array<std::uint32_t, 2> masks{1u, samples == 2u ? 2u : samples == 4u ? 6u : 0x96u};
    std::size_t slot = 0;
    for (const auto mask : masks) {
        Pass reset;
        reset.vertices = &back;
        reset.code = NullExportCode;
        reset.samples = samples;
        reset.depth = depth;
        reset.depthWrite = true;
        Draw(device, context, reset);
        Pass masked = reset;
        masked.vertices = &middle;
        masked.code = MaskCode(slot++ + (samples == 2u ? 0u : samples == 4u ? 2u : 4u), mask);
        masked.shaderControl = 0x100u;
        masked.order = order;
        Draw(device, context, masked);
        Settle(device);
        Invocations.fill(0u);
        Pass check;
        check.vertices = &probe;
        check.code = StoreCode(samples);
        check.perSample = true;
        check.shaderControl = 0x10u;
        check.samples = samples;
        check.depth = depth;
        check.depthCompare = VK_COMPARE_OP_LESS;
        check.order = order;
        check.storesInvocations = true;
        Draw(device, context, check);
        Settle(device);
        CheckStored(samples, std::to_string(samples) + "x samples a pixel shader's exported guest mask " + std::to_string(mask) + " wrote depth into", [&](std::uint32_t, std::uint32_t) { return mask; });
    }
    std::printf("%ux exported sample masks reach the host samples at the guest samples' locations\n", samples);
}

void CoverageOrder(AgcDriver::VulkanDevice& device, const Context& context, std::uint32_t samples) {
    const auto order = OrderFor(samples);
    const auto toNdc = [](float pixel) { return pixel * 2.0f / static_cast<float>(RemapWidth) - 1.0f; };
    const auto rectangle = Rectangle(toNdc(2.5f), toNdc(5.5f), 0.5f);
    const auto back = Screen(0.25f), probe = Screen(0.375f);
    const AgcDriver::Graphics::DepthTarget depth = [&] {
        AgcDriver::Graphics::DepthTarget target{reinterpret_cast<std::uintptr_t>(DepthMemory.data()) + 256u * 1024u * (static_cast<std::uint32_t>(std::countr_zero(samples)) + 4u), 0u, {RemapWidth, RemapHeight}, VK_FORMAT_D32_SFLOAT, 0.0f, 0};
        target.samples = samples;
        return target;
    }();
    const std::array<std::uint16_t, 4> mask{2u, 2u, 2u, 4u};
    const auto full = (1u << samples) - 1u;
    const auto covered = [&](std::uint32_t x, std::uint32_t y) {
        std::uint32_t host = 0u;
        for (std::uint32_t sample = 0; sample < samples; ++sample) {
            const auto at = static_cast<float>(x) + StandardX(samples, sample);
            if (at > 2.5f && at < 5.5f) host |= 1u << sample;
        }
        return GuestBits(host, order[Quad(x, y)], samples) & (mask[Quad(x, y)] & full);
    };
    Pass reset;
    reset.vertices = &back;
    reset.code = NullExportCode;
    reset.samples = samples;
    reset.depth = depth;
    reset.depthWrite = true;
    Draw(device, context, reset);
    Invocations.fill(0u);
    Pass masked;
    masked.vertices = &rectangle;
    masked.code = StoreCode(samples);
    masked.perSample = true;
    masked.samples = samples;
    masked.depth = depth;
    masked.depthWrite = true;
    masked.order = order;
    masked.quadSampleMask = mask;
    masked.storesInvocations = true;
    Draw(device, context, masked);
    Settle(device);
    CheckStored(samples, std::to_string(samples) + "x a quad sample mask reached the pixel shader in the guest's order", covered);
    Invocations.fill(0u);
    Pass check;
    check.vertices = &probe;
    check.code = StoreCode(samples);
    check.perSample = true;
    check.shaderControl = 0x10u;
    check.samples = samples;
    check.depth = depth;
    check.depthCompare = VK_COMPARE_OP_LESS;
    check.order = order;
    check.storesInvocations = true;
    Draw(device, context, check);
    Settle(device);
    CheckStored(samples, std::to_string(samples) + "x a quad sample mask exported through the sample order landed on the host's samples", covered);
    std::printf("%ux a quad sample mask reaches the pixel shader in the guest's order and exports coverage in the host's\n", samples);
}

std::uint32_t ShaderRemaps(AgcDriver::VulkanDevice& device, const Context& context) {
    std::uint32_t checked = 0;
    for (const std::uint32_t samples : {2u, 4u, 8u}) {
        if ((context.limits.framebufferDepthSampleCounts & samples) == 0u || (context.limits.framebufferNoAttachmentsSampleCounts & samples) == 0u) continue;
        if (context.limits.standardSampleLocations) {
            SampleIndexRemap(device, context, samples);
            ++checked;
        } else {
            std::printf("skipped the %ux SampleID check: the device has no standard sample locations\n", samples);
        }
        CoverageRemap(device, context, samples);
        CoverageOrder(device, context, samples);
        ++checked;
    }
    return checked;
}

constexpr std::size_t BlockBytes = 65536;

class GuestBlock {
public:
    explicit GuestBlock(std::size_t bytes) : bytes((bytes + BlockBytes - 1u) & ~(BlockBytes - 1u)) {
#ifdef _WIN32
        block = static_cast<std::byte*>(VirtualAlloc(nullptr, this->bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::byte*>(std::aligned_alloc(BlockBytes, this->bytes));
#endif
        Require(block != nullptr, "sample order test: cannot allocate a guest block");
        std::memset(block, 0, this->bytes);
        GuestAllocations::Mutation().Add(block, this->bytes, true, true, true);
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::size_t bytes;
    std::byte* block = nullptr;
};

GuestBlock& NewBlock(std::size_t bytes) {
    static auto* blocks = new std::deque<GuestBlock>();
    return blocks->emplace_back(bytes);
}

constexpr VkExtent2D ReferenceExtent{64u, 32u};
constexpr std::array<std::uint8_t, 32> VillageLocations{0x04, 0x0c, 0, 0, 0, 0, 0, 0, 0x04, 0x0c, 0, 0, 0, 0, 0, 0, 0x0c, 0x04, 0, 0, 0, 0, 0, 0, 0x0c, 0x04, 0, 0, 0, 0, 0, 0};
constexpr float ClearDepth = 0.9375f;
constexpr std::size_t SceneSlots = 6u;
const Vertices Slopes{{-1.0f, -1.0f, 0.125f, 1.0f}, {1.0f, -0.8125f, 0.875f, 1.0f}, {-0.5625f, 1.0f, 0.5f, 1.0f}, {1.0f, -1.0f, 0.75f, 1.0f}, {0.6875f, 1.0f, 0.0625f, 1.0f}, {-1.0f, 0.4375f, 0.3125f, 1.0f}};

struct Rendered {
    Samples color;
    Samples depth;
};

double SlopeDepth(std::size_t triangle, double x, double y) {
    std::array<std::array<double, 3>, 3> corner{};
    for (std::size_t vertex = 0; vertex < corner.size(); ++vertex) {
        const auto& at = Slopes[triangle * 3u + vertex];
        corner[vertex] = {(at[0] + 1.0) * ReferenceExtent.width / 2.0, (1.0 - at[1]) * ReferenceExtent.height / 2.0, at[2]};
    }
    const auto edge = [](const std::array<double, 3>& from, const std::array<double, 3>& to, double px, double py) { return (to[0] - from[0]) * (py - from[1]) - (to[1] - from[1]) * (px - from[0]); };
    const auto area = edge(corner[0], corner[1], corner[2][0], corner[2][1]);
    return (edge(corner[1], corner[2], x, y) * corner[0][2] + edge(corner[2], corner[0], x, y) * corner[1][2] + edge(corner[0], corner[1], x, y) * corner[2][2]) / area;
}

struct DepthLocations {
    std::size_t sloped = 0;
    std::size_t guest = 0;
    std::size_t standard = 0;
    std::array<std::size_t, 2> slopedByRow{};
    std::array<std::size_t, 2> guestByRow{};
};

DepthLocations LocateDepth(const Samples& depth, const std::array<std::uint8_t, 32>& locations = VillageLocations) {
    constexpr std::uint32_t samples = 2u;
    constexpr std::array<std::array<double, 2>, 2> standard{{{0.75, 0.75}, {0.25, 0.25}}};
    DepthLocations located;
    for (std::size_t index = 0; index < depth.size(); ++index) {
        const auto value = static_cast<double>(std::bit_cast<float>(depth[index][0]));
        if (value == ClearDepth) continue;
        ++located.sloped;
        const auto pixel = static_cast<std::uint32_t>(index / samples);
        const auto sample = static_cast<std::uint32_t>(index % samples);
        const auto x = pixel % ReferenceExtent.width;
        const auto y = pixel / ReferenceExtent.width;
        ++located.slopedByRow[y & 1u];
        const auto bits = static_cast<std::uint32_t>(locations[Quad(x, y) * 8u + sample]);
        const std::array<double, 2> guest{0.5 + (static_cast<int>(bits & 7u) - static_cast<int>(bits & 8u)) / 16.0, 0.5 + (static_cast<int>((bits >> 4u) & 7u) - static_cast<int>((bits >> 4u) & 8u)) / 16.0};
        const auto holds = [&](const std::array<double, 2>& at) {
            for (std::size_t triangle = 0; triangle < 2u; ++triangle) {
                if (std::abs(SlopeDepth(triangle, x + at[0], y + at[1]) - value) < 1.0e-5) return true;
            }
            return false;
        };
        if (holds(guest)) {
            ++located.guest;
            ++located.guestByRow[y & 1u];
        } else if (holds(standard[sample])) {
            ++located.standard;
        }
    }
    return located;
}

DepthLocations PrintDepthLocations(const std::string& what, const DepthLocations& located) {
    std::printf("%s: of %zu sloped depth samples, %zu hold the slope's depth at the guest's sample locations (%zu of %zu on even rows, %zu of %zu on odd rows) and %zu at the standard 2x locations\n", what.c_str(), located.sloped, located.guest, located.guestByRow[0], located.slopedByRow[0], located.guestByRow[1], located.slopedByRow[1], located.standard);
    return located;
}

DepthLocations ReportDepthLocations(const std::string& what, const Rendered& rendered, const std::array<std::uint8_t, 32>& locations = VillageLocations) {
    return PrintDepthLocations(what, LocateDepth(rendered.depth, locations));
}

Samples ReadTarget(const Context& context, VkImageView view, bool color, std::uint32_t samples) {
    const auto count = static_cast<std::size_t>(ReferenceExtent.width) * ReferenceExtent.height * samples;
    AgcDriver::Graphics::Buffer values(context, count * 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const std::array<Binding, 2> bindings{{{0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE}, {2u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}}};
    Program read(context, color ? std::span<const std::uint32_t>(SAMPLE_ORDER_TEST_READCOLOR_SPV) : std::span<const std::uint32_t>(SAMPLE_ORDER_TEST_READDEPTH_SPV), bindings, 12u);
    read.Image(0u, view);
    read.Storage(2u, values.Handle());
    const std::array<std::uint32_t, 3> push{ReferenceExtent.width, ReferenceExtent.height, samples};
    Submit(context, [&](VkCommandBuffer commands) {
        FullBarrier(context, commands);
        read.Dispatch(commands, push.data(), ReferenceExtent);
        HostBarrier(context, commands);
    });
    return ReadValues(values, count);
}

Rendered RenderCheckerboard(AgcDriver::VulkanDevice& device, const Context& context, std::uint32_t samples, std::size_t run, const std::optional<std::array<std::uint8_t, 32>>& locations) {
    AgcDriver::Graphics::ColorTarget color{};
    color.extent = ReferenceExtent;
    color.surfaceExtent = ReferenceExtent;
    color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    color.componentMapping = 0xe4u;
    color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
    color.elementBytes = 8u;
    color.samples = samples;
    color.bytes = AgcDriver::Graphics::ColorTargetLayout(ReferenceExtent.width, ReferenceExtent.height, color.tileMode, color.elementBytes).Bytes();
    auto& block = NewBlock(AgcDriver::Graphics::MultisampledColorBytes(color));
    color.address = block.Address();
    color.surfaceAddress = color.address;
    AgcDriver::Graphics::DepthTarget depth{reinterpret_cast<std::uintptr_t>(DepthMemory.data()) + 256u * 1024u * run, 0u, ReferenceExtent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    depth.samples = samples;
    const Vertices clear = Screen(ClearDepth);
    const Vertices shade{{-0.9375f, -0.875f, 0.25f, 1.0f}, {0.9375f, -0.625f, 0.6875f, 1.0f}, {0.0625f, 0.9375f, 0.4375f, 1.0f}};
    Pass depthOnly;
    depthOnly.vertices = &clear;
    depthOnly.code = NullExportCode;
    depthOnly.samples = samples;
    depthOnly.extent = ReferenceExtent;
    depthOnly.depth = depth;
    depthOnly.depthWrite = true;
    depthOnly.locations = locations;
    Draw(device, context, depthOnly);
    depthOnly.vertices = &Slopes;
    depthOnly.depthCompare = VK_COMPARE_OP_LESS;
    Draw(device, context, depthOnly);
    Pass colorPass;
    colorPass.vertices = &clear;
    colorPass.code = SampleColorCode;
    colorPass.perSample = true;
    colorPass.colorFormat = 9u;
    colorPass.samples = samples;
    colorPass.extent = ReferenceExtent;
    colorPass.depth = depth;
    colorPass.depthCompare = VK_COMPARE_OP_ALWAYS;
    colorPass.color = color;
    colorPass.locations = locations;
    Draw(device, context, colorPass);
    colorPass.vertices = &shade;
    colorPass.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
    Draw(device, context, colorPass);
    Settle(device);
    Rendered rendered;
    rendered.color = ReadTarget(context, AgcDriver::Graphics::MultisampleTargetView(context, color), true, samples);
    rendered.depth = ReadTarget(context, AgcDriver::Graphics::DepthSurfaceOrderTarget(context, depth).view, false, samples);
    return rendered;
}

std::size_t Differences(const Samples& reference, const Samples& rendered, std::uint32_t samples, const std::string& what, bool report) {
    std::size_t differences = 0;
    for (std::size_t index = 0; index < reference.size(); ++index) {
        if (reference[index] == rendered[index]) continue;
        if (report && differences < 4u) {
            const auto pixel = static_cast<std::uint32_t>(index / samples);
            std::printf("%s: %s is 0x%08x 0x%08x 0x%08x 0x%08x, the reference's is 0x%08x 0x%08x 0x%08x 0x%08x\n", what.c_str(), At(pixel % ReferenceExtent.width, pixel / ReferenceExtent.width, static_cast<std::uint32_t>(index % samples)).c_str(), rendered[index][0], rendered[index][1], rendered[index][2], rendered[index][3], reference[index][0], reference[index][1], reference[index][2], reference[index][3]);
        }
        ++differences;
    }
    return differences;
}

struct ClearTargets {
    AgcDriver::VulkanDevice& device;
    VkDevice handle;
    ~ClearTargets() {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        device.SubmitRecorded(true);
        device.WaitIdle();
        AgcDriver::Graphics::ClearMultisampleTargets(handle);
        AgcDriver::Graphics::ClearCachedTextures(handle);
    }
};

void CheckScene(const Rendered& rendered, std::uint32_t samples, const std::string& what) {
    std::size_t cleared = 0;
    for (std::size_t index = 0; index < rendered.depth.size(); ++index) {
        const auto sample = static_cast<std::uint32_t>(index % samples);
        const auto pixel = static_cast<std::uint32_t>(index / samples);
        const auto at = At(pixel % ReferenceExtent.width, pixel / ReferenceExtent.width, sample);
        const auto id = static_cast<float>(sample);
        const auto z = std::bit_cast<float>(rendered.color[index][2]);
        const auto w = std::bit_cast<float>(rendered.color[index][3]);
        Require(z == id && w == id, what + ": " + at + " does not hold its own sample index in color (z " + std::to_string(z) + ", w " + std::to_string(w) + ")");
        const auto depth = std::bit_cast<float>(rendered.depth[index][0]);
        Require(depth >= 0.0f && depth <= ClearDepth, what + ": " + at + " holds depth " + std::to_string(depth));
        if (depth == ClearDepth) ++cleared;
    }
    Require(cleared != 0u && cleared != rendered.depth.size(), what + ": the slopes did not leave depth both at and below the clear value");
}

std::uint32_t SceneChecks(AgcDriver::VulkanDevice& device, const Context& context) {
    ClearTargets clearTargets{device, context.device};
    std::uint32_t checked = 0;
    for (const std::uint32_t samples : {2u, 4u, 8u}) {
        if ((context.limits.framebufferColorSampleCounts & samples) == 0u || (context.limits.framebufferDepthSampleCounts & samples) == 0u || !DepthSampleable(context, VK_FORMAT_D32_SFLOAT, samples)) continue;
        CheckScene(RenderCheckerboard(device, context, samples, SceneSlots + static_cast<std::size_t>(std::countr_zero(samples)), std::nullopt), samples, std::to_string(samples) + "x scene with the standard locations");
        ++checked;
    }
    return checked;
}

std::array<std::uint8_t, 32> UniformLocations(const std::array<std::uint8_t, 32>& pattern, std::uint32_t pixel) {
    std::array<std::uint8_t, 32> uniform{};
    for (std::uint32_t target = 0; target < 4u; ++target) std::copy_n(pattern.begin() + 8u * pixel, 8u, uniform.begin() + 8u * target);
    return uniform;
}

Rendered QuadComposite(AgcDriver::VulkanDevice& device, const Context& context, std::uint32_t samples, std::size_t run, const std::array<std::uint8_t, 32>& pattern) {
    std::array<Rendered, 4> parts;
    for (std::uint32_t pixel = 0; pixel < parts.size(); ++pixel) parts[pixel] = RenderCheckerboard(device, context, samples, run + pixel, UniformLocations(pattern, pixel));
    auto composite = parts[0];
    for (std::size_t index = 0; index < composite.color.size(); ++index) {
        const auto pixel = static_cast<std::uint32_t>(index / samples);
        const auto& part = parts[Quad(pixel % ReferenceExtent.width, pixel / ReferenceExtent.width)];
        composite.color[index] = part.color[index];
        composite.depth[index] = part.depth[index];
    }
    return composite;
}

int GridReference(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    ClearTargets clearTargets{device, context.device};
    constexpr std::uint32_t samples = 2u;
    if ((context.limits.framebufferColorSampleCounts & samples) == 0u || (context.limits.framebufferDepthSampleCounts & samples) == 0u) {
        std::printf("skipped, the device has no 2x color and depth targets\n");
        return VulkanTestSkipped;
    }
    if ((context.sampleLocationSampleCounts & samples) == 0u) {
        std::printf("skipped, the device has no custom sample locations at 2 samples\n");
        return VulkanTestSkipped;
    }
    const auto largest = context.sampleLocationGridSizes[1];
    std::printf("the device's largest sample location grid at 2 samples is %ux%u pixels (vkGetPhysicalDeviceMultisamplePropertiesEXT)\n", largest.width, largest.height);
    auto emulated = context;
    emulated.sampleLocationGridSampleCounts = 0u;
    auto fallback = emulated;
    fallback.storageImageMultisample = false;
    AgcDriver::Graphics::State probe{};
    probe.samples = samples;
    probe.customSampleLocations = true;
    probe.sampleLocationGrid = true;
    probe.sampleLocations = VillageLocations;
    AgcDriver::Graphics::ColorTarget color{};
    color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    color.elementBytes = 8u;
    color.samples = samples;
    probe.colors = {color};
    probe.depth = AgcDriver::Graphics::DepthTarget{0x100000u, 0u, ReferenceExtent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    probe.depth->samples = samples;
    const auto refusal = AgcDriver::Graphics::SampleOrderRefusal(emulated, probe);
    if (!refusal.empty()) {
        std::printf("skipped, the device does not put the checkerboard's samples in the guest's order: %s\n", refusal.c_str());
        return VulkanTestSkipped;
    }
    Require(AgcDriver::Graphics::ReordersSamples(AgcDriver::Graphics::HostSampleLocationsFor(emulated, probe).order) && !AgcDriver::Graphics::ReordersSamples(AgcDriver::Graphics::HostSampleLocationsFor(fallback, probe).order), "the emulated and fallback contexts did not take the exact and approximate paths");
    const auto reference = QuadComposite(device, context, samples, 3u, VillageLocations);
    const auto exact = RenderCheckerboard(device, emulated, samples, 1u, VillageLocations);
    const auto approximate = RenderCheckerboard(device, fallback, samples, 2u, VillageLocations);
    CheckScene(reference, samples, "the composite of uniform renders");
    CheckScene(exact, samples, "the exact path");
    CheckScene(approximate, samples, "pixel (0,0)'s locations alone");
    const bool grid = (context.sampleLocationGridSampleCounts & samples) != 0u;
    std::optional<Rendered> native;
    if (grid) {
        native = RenderCheckerboard(device, context, samples, 0u, VillageLocations);
        CheckScene(*native, samples, "the 2x2 grid");
    }
    const auto referenceDepth = ReportDepthLocations("the composite of uniform renders", reference);
    ReportDepthLocations("the exact path", exact);
    ReportDepthLocations("pixel (0,0)'s locations alone", approximate);
    const auto nativeDepth = native ? std::optional<DepthLocations>(ReportDepthLocations("the device's 2x2 grid", *native)) : std::nullopt;
    const bool gridDepthCompared = nativeDepth && nativeDepth->guest == nativeDepth->sloped;
    if (referenceDepth.guest != referenceDepth.sloped) {
        std::printf("skipped, the device does not keep depth drawn at custom sample locations at those locations, even after a transition that names them: the composite of uniform renders holds the guest's depth in %zu of %zu sloped samples, so no path can be compared with it bit for bit\n", referenceDepth.guest, referenceDepth.sloped);
        return VulkanTestSkipped;
    }
    std::size_t gridDifferences = 0;
    if (native) {
        gridDifferences = Differences(reference.color, native->color, samples, "color through the device's 2x2 grid", true) + (gridDepthCompared ? Differences(reference.depth, native->depth, samples, "depth through the device's 2x2 grid", true) : 0u);
        if (!gridDepthCompared) std::printf("the device's 2x2 grid holds the guest's depth in %zu of %zu sloped samples as the device reads it back, so only its color is compared with the composite\n", nativeDepth->guest, nativeDepth->sloped);
        const auto exactGridDifferences = Differences(native->color, exact.color, samples, "color", false) + Differences(native->depth, exact.depth, samples, "depth", false);
        std::printf("2x checkerboard at %ux%u: the device's 2x2 grid differs from the composite of four uniform renders in %zu samples and from the exact path in %zu samples\n", ReferenceExtent.width, ReferenceExtent.height, gridDifferences, exactGridDifferences);
    } else {
        std::printf("the device has no 2x2 sample location grid at 2 samples: the composite of four uniform renders is the reference\n");
    }
    const auto colorDifferences = Differences(reference.color, exact.color, samples, "color with the samples put in the guest's order", true);
    const auto depthDifferences = Differences(reference.depth, exact.depth, samples, "depth with the samples put in the guest's order", true);
    const auto approximateDifferences = Differences(reference.color, approximate.color, samples, "color", false) + Differences(reference.depth, approximate.depth, samples, "depth", false);
    std::printf("2x checkerboard at %ux%u: the exact path differs from the reference in %zu color and %zu depth samples; pixel (0,0)'s locations alone differ in %zu samples\n", ReferenceExtent.width, ReferenceExtent.height, colorDifferences, depthDifferences, approximateDifferences);
    Require(approximateDifferences != 0u, "pixel (0,0)'s locations alone rendered the checkerboard like the reference: the scene does not tell the paths apart");
    Require(gridDifferences == 0u, "the composite of four uniform renders does not render the checkerboard like the device's 2x2 grid");
    Require(colorDifferences == 0u && depthDifferences == 0u, "the exact path did not render the checkerboard bit for bit like the reference");
    std::puts(grid && gridDepthCompared ? "the exact path renders Village's 2x checkerboard like the device's 2x2 sample location grid, bit for bit" : "the exact path renders Village's 2x checkerboard like four uniform renders composed by pixel parity, bit for bit");
    return 0;
}

void CheckRecipeDepth(AgcDriver::VulkanDevice& device, const Context& context, const std::array<std::uint8_t, 32>& locations, std::size_t run, const std::string& what, bool reordered) {
    constexpr std::uint32_t samples = 2u;
    AgcDriver::Graphics::DepthTarget depth{reinterpret_cast<std::uintptr_t>(DepthMemory.data()) + 256u * 1024u * run, 0u, ReferenceExtent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    depth.samples = samples;
    const Vertices clear = Screen(ClearDepth);
    Pass clearPass;
    clearPass.vertices = &clear;
    clearPass.code = NullExportCode;
    clearPass.samples = samples;
    clearPass.extent = ReferenceExtent;
    clearPass.depth = depth;
    clearPass.depthWrite = true;
    clearPass.locations = locations;
    auto slopes = clearPass;
    slopes.vertices = &Slopes;
    auto& vertexBlock = NewBlock(Slopes.size() * sizeof(Slopes[0]));
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(vertexBlock.Address())), Slopes.data(), Slopes.size() * sizeof(Slopes[0]));
    slopes.vertexMemory = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(vertexBlock.Address()));
    if (reordered) {
        slopes.buildsNoRecipe = true;
        Draw(device, context, clearPass);
        Draw(device, context, slopes);
        Settle(device);
        std::printf("%s: builds no recipe, since a draw that reorders its samples is built in full each time, with its sample swaps\n", what.c_str());
        return;
    }
    const auto read = [&] { return ReadTarget(context, AgcDriver::Graphics::DepthSurfaceOrderTarget(context, depth).view, false, samples); };
    Samples recorded;
    slopes.beforeReplay = [&] {
        Settle(device);
        recorded = read();
        Draw(device, context, clearPass);
        Settle(device);
        const auto cleared = read();
        Require(std::all_of(cleared.begin(), cleared.end(), [](const std::array<std::uint32_t, 4>& value) { return std::bit_cast<float>(value[0]) == ClearDepth; }), what + ": the clear between the recorded draw and its replay left depth other than the clear value");
    };
    Draw(device, context, clearPass);
    Draw(device, context, slopes);
    Settle(device);
    const auto replayed = read();
    const auto first = PrintDepthLocations(what + ", the recorded draw", LocateDepth(recorded, locations));
    PrintDepthLocations(what + ", the recipe's replay as the first draw of a pass", LocateDepth(replayed, locations));
    Require(first.sloped != 0u, what + ": the recorded draw left no sloped depth");
    const bool readAtGuestLocations = first.guest == first.sloped;
    if (!readAtGuestLocations) std::printf("%s: the device reads depth drawn at custom sample locations back at other locations, even after a transition that names them: the replay is compared with the recorded draw as the device reads both back\n", what.c_str());
    const auto differences = Differences(recorded, replayed, samples, what + ", depth replayed from the recipe", true);
    std::printf("%s, %ux%u: the recipe's replay differs from the recorded draw in %zu of %zu samples\n", what.c_str(), ReferenceExtent.width, ReferenceExtent.height, differences, recorded.size());
    Require(differences == 0u, what + (readAtGuestLocations ? ": the recipe's replay did not leave the depth the recorded draw left at the guest's sample locations" : ": the recipe's replay did not leave the depth the recorded draw left, as the device reads both back"));
    std::printf("%s: a depth-only 2x draw replayed from its recipe as the first draw of a pass leaves the depth the recorded draw left%s, bit for bit\n", what.c_str(), readAtGuestLocations ? "" : " as the device reads both back");
}

int RecipeDepthLocations(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    ClearTargets clearTargets{device, context.device};
    constexpr std::uint32_t samples = 2u;
    if ((context.limits.framebufferDepthSampleCounts & samples) == 0u || !DepthSampleable(context, VK_FORMAT_D32_SFLOAT, samples)) {
        std::printf("skipped, the device has no sampleable 2x depth targets\n");
        return VulkanTestSkipped;
    }
    if ((context.sampleLocationSampleCounts & samples) == 0u || !AgcDriver::Graphics::SampleLocationsCompatibleDepth(context, VK_FORMAT_D32_SFLOAT, samples)) {
        std::printf("skipped, the device has no custom sample locations at 2 samples\n");
        return VulkanTestSkipped;
    }
    if (!AgcDriver::Graphics::DrawRecipes()) {
        std::printf("skipped, draw recipes are off (APS5_NO_DRAW_RECIPE)\n");
        return VulkanTestSkipped;
    }
    CheckRecipeDepth(device, context, UniformLocations(VillageLocations, 0u), 0u, "pixel (0,0)'s locations in every pixel", false);
    auto emulated = context;
    emulated.sampleLocationGridSampleCounts = 0u;
    AgcDriver::Graphics::State probe{};
    probe.samples = samples;
    probe.customSampleLocations = true;
    probe.sampleLocationGrid = true;
    probe.sampleLocations = VillageLocations;
    probe.depth = AgcDriver::Graphics::DepthTarget{0x100000u, 0u, ReferenceExtent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    probe.depth->samples = samples;
    const auto refusal = AgcDriver::Graphics::SampleOrderRefusal(emulated, probe);
    if (!refusal.empty()) {
        std::printf("the checkerboard through the exact path is not checked: the device does not put its samples in the guest's order: %s\n", refusal.c_str());
        return 0;
    }
    Require(AgcDriver::Graphics::ReordersSamples(AgcDriver::Graphics::HostSampleLocationsFor(emulated, probe).order), "the context without a 2x2 grid did not take the exact path");
    CheckRecipeDepth(device, emulated, UniformLocations(VillageLocations, 0u), 1u, "pixel (0,0)'s locations in every pixel, without a 2x2 grid", false);
    CheckRecipeDepth(device, emulated, VillageLocations, 2u, "the 2x checkerboard through the exact path", true);
    return 0;
}

int Bench(AgcDriver::VulkanDevice& device, VkExtent2D extent, std::uint32_t samples, std::uint32_t frames) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if ((context.limits.framebufferColorSampleCounts & samples) == 0u || (context.limits.framebufferDepthSampleCounts & samples) == 0u) {
        std::printf("skipped, the device has no %ux color and depth targets\n", samples);
        return VulkanTestSkipped;
    }
    const auto alias = AgcDriver::Graphics::SampleOrderStorageFormat(context, VK_FORMAT_R16G16B16A16_SFLOAT, 8u, samples);
    if (alias == VK_FORMAT_UNDEFINED || !DepthSampleable(context, VK_FORMAT_D32_SFLOAT, samples) || !context.limits.timestampComputeAndGraphics) {
        std::printf("skipped, the device has no %ux storage alias for RGBA16F, sampleable D32F or timestamps\n", samples);
        return VulkanTestSkipped;
    }
    const bool located = (context.sampleLocationSampleCounts & samples) != 0u && AgcDriver::Graphics::SampleLocationsCompatibleDepth(context, VK_FORMAT_D32_SFLOAT, samples);
    Image color(context, VK_FORMAT_R16G16B16A16_SFLOAT, samples, extent, ColorUsage, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT);
    Image depth(context, VK_FORMAT_D32_SFLOAT, samples, extent, DepthUsage, located ? VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT : 0u);
    const auto storage = color.View(alias, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_USAGE_STORAGE_BIT);
    const auto attachment = depth.View(VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    const std::array<SampleOrderTarget, 2> targets{{
        {color.Handle(), storage, alias, extent, samples, SampleOrderMethod::Storage, nullptr},
        {depth.Handle(), attachment, VK_FORMAT_D32_SFLOAT, extent, samples, SampleOrderMethod::Depth, nullptr},
    }};
    VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryInfo.queryCount = 5u;
    VkQueryPool queries = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(context.device, &queryInfo, nullptr, &queries), "vkCreateQueryPool sample order bench");
    Submit(context, [&](VkCommandBuffer commands) {
        color.ToGeneral(commands, VK_IMAGE_ASPECT_COLOR_BIT);
        depth.ToGeneral(commands, VK_IMAGE_ASPECT_DEPTH_BIT);
    });
    const auto measure = [&](const SampleOrderPass& pass) {
        std::vector<std::array<double, 4>> phases;
        std::vector<std::shared_ptr<void>> kept;
        for (std::uint32_t frame = 0; frame <= frames; ++frame) {
            kept.clear();
            Submit(context, [&](VkCommandBuffer commands) {
                context.Function<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(commands, queries, 0u, 5u);
                const auto stamp = [&](std::uint32_t query) {
                    context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, query);
                };
                stamp(0u);
                AgcDriver::Graphics::RecordSampleOrder(context, commands, pass, std::span(targets).subspan(1u), true, kept);
                stamp(1u);
                AgcDriver::Graphics::RecordSampleOrder(context, commands, pass, std::span(targets).subspan(1u), false, kept);
                stamp(2u);
                AgcDriver::Graphics::RecordSampleOrder(context, commands, pass, targets, true, kept);
                stamp(3u);
                AgcDriver::Graphics::RecordSampleOrder(context, commands, pass, targets, false, kept);
                stamp(4u);
            });
            std::array<std::uint64_t, 5> ticks{};
            Check(context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(context.device, queries, 0u, 5u, sizeof(ticks), ticks.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "vkGetQueryPoolResults sample order bench");
            if (frame == 0u) continue;
            std::array<double, 4> phase{};
            for (std::size_t index = 0; index < phase.size(); ++index) phase[index] = static_cast<double>(ticks[index + 1u] - ticks[index]) * context.limits.timestampPeriod / 1.0e6;
            phases.push_back(phase);
        }
        std::array<double, 4> median{};
        for (std::size_t index = 0; index < median.size(); ++index) {
            std::vector<double> values;
            for (const auto& phase : phases) values.push_back(phase[index]);
            std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2u), values.end());
            median[index] = values[values.size() / 2u];
        }
        return median;
    };
    const auto report = [&](const std::array<double, 4>& median, const char* how) {
        std::printf("sample order swaps at %ux%u, %ux, %s, median of %u frames (GPU ms): depth-only pass in %.3f, out %.3f; color+depth pass in %.3f, out %.3f; per frame with one pass of each %.3f\n", extent.width, extent.height, samples, how, frames, median[0], median[1], median[2], median[3], median[0] + median[1] + median[2] + median[3]);
    };
    report(measure(SampleOrderPass{OrderFor(samples), extent, {}, false}), "without sample locations");
    if (located) {
        AgcDriver::Graphics::State standard{};
        standard.samples = samples;
        report(measure(SampleOrderPass{OrderFor(samples), extent, AgcDriver::Graphics::SampleLocations(standard), true}), "naming the standard sample locations at the depth copy's transitions");
    } else {
        std::printf("the device draws no %ux depth at custom sample locations: no swap names them\n", samples);
    }
    context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, queries, nullptr);
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? std::string_view(argv[1]) : std::string_view();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (mode == "--grid-reference") return GridReference(*device);
        if (mode == "--recipe-depth-locations") return RecipeDepthLocations(*device);
        if (mode == "--bench") {
            VkExtent2D extent{1920u, 2160u};
            if (argc > 2) Require(std::sscanf(argv[2], "%ux%u", &extent.width, &extent.height) == 2, "--bench takes WIDTHxHEIGHT");
            const auto samples = argc > 3 ? static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10)) : 2u;
            const auto frames = argc > 4 ? static_cast<std::uint32_t>(std::strtoul(argv[4], nullptr, 10)) : 50u;
            return Bench(*device, extent, samples, frames);
        }
        Require(mode.empty(), "unknown mode " + std::string(mode));
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        if (!context.sampleRateShading) {
            std::printf("skipped, the device lacks sampleRateShading\n");
            return VulkanTestSkipped;
        }
        const auto swapped = SwapPasses(context);
        const auto remapped = ShaderRemaps(*device, context);
        const auto scenes = SceneChecks(*device, context);
        if (swapped == 0u && remapped == 0u && scenes == 0u) {
            std::printf("skipped, the device has no multisampled targets the sample order tests can use\n");
            return VulkanTestSkipped;
        }
        std::printf("sample order tests passed: %u swap round trips, %u shader remap checks, %u scene checks\n", swapped, remapped, scenes);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
