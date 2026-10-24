#include "prx/libSceAgcDriver/Execution/include/PresentationUpscaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/FsrEasuFrag_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/FsrRcasFrag_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/PresentationPassVert_spv.h"
#include <array>
#include <cmath>

namespace AgcDriver {
namespace {

struct EasuParameters {
    float sourceWidth;
    float sourceHeight;
    float outputWidth;
    float outputHeight;
};

struct RcasParameters {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
    float sharpness;
};

constexpr std::uint32_t ParameterBytes = sizeof(RcasParameters);
static_assert(sizeof(EasuParameters) <= ParameterBytes);

}

PresentationUpscaler::PresentationUpscaler(const Graphics::Context& context, std::size_t slotCount, float sharpness) : context(context), sharpness(sharpness), slots(slotCount) {
    Graphics::Require(slotCount != 0, "presentation upscaler needs at least one slot");
    Graphics::Require(std::isfinite(sharpness) && sharpness >= 0.0f && sharpness <= 2.0f, "FSR sharpness must be from 0.0 to 2.0 stops");
    try {
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Graphics::Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &samplerInfo, nullptr, &sampler), "vkCreateSampler presentation upscaler");
        const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler};
        VkDescriptorSetLayoutCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptorInfo.bindingCount = 1;
        descriptorInfo.pBindings = &binding;
        Graphics::Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &descriptorInfo, nullptr, &descriptorLayout), "vkCreateDescriptorSetLayout presentation upscaler");
        const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, ParameterBytes};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        Graphics::Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout presentation upscaler");
        const auto sets = static_cast<std::uint32_t>(slotCount * 2);
        const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = sets;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &size;
        Graphics::Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &descriptorPool), "vkCreateDescriptorPool presentation upscaler");
        for (auto& slot : slots) {
            const std::array<VkDescriptorSetLayout, 2> layouts{descriptorLayout, descriptorLayout};
            std::array<VkDescriptorSet, 2> allocated{};
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool = descriptorPool;
            allocation.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
            allocation.pSetLayouts = layouts.data();
            Graphics::Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, allocated.data()), "vkAllocateDescriptorSets presentation upscaler");
            slot.easuSet = allocated[0];
            slot.rcasSet = allocated[1];
        }
        vertex = Graphics::CreateShaderModule(context, PRESENTATION_PASS_VERT_SPV, sizeof(PRESENTATION_PASS_VERT_SPV), "vkCreateShaderModule presentation upscaler vertex");
        easu = Graphics::CreateShaderModule(context, FSR_EASU_FRAG_SPV, sizeof(FSR_EASU_FRAG_SPV), "vkCreateShaderModule FSR EASU");
        rcas = Graphics::CreateShaderModule(context, FSR_RCAS_FRAG_SPV, sizeof(FSR_RCAS_FRAG_SPV), "vkCreateShaderModule FSR RCAS");
    } catch (...) {
        release();
        throw;
    }
}

PresentationUpscaler::~PresentationUpscaler() {
    release();
}

void PresentationUpscaler::release() noexcept {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        Release(i);
        releaseSurface(slots[i].stage);
        releaseSurface(slots[i].upscaled);
    }
    for (const auto& [key, entry] : targets) {
        if (entry.pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, entry.pipeline, nullptr);
        if (entry.renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, entry.renderPass, nullptr);
    }
    targets.clear();
    for (auto* module : {&rcas, &easu, &vertex}) {
        if (*module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, *module, nullptr);
        *module = VK_NULL_HANDLE;
    }
    if (descriptorPool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, descriptorPool, nullptr);
    if (pipelineLayout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
    if (descriptorLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, descriptorLayout, nullptr);
    if (sampler) context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, sampler, nullptr);
    descriptorPool = VK_NULL_HANDLE;
    pipelineLayout = VK_NULL_HANDLE;
    descriptorLayout = VK_NULL_HANDLE;
    sampler = VK_NULL_HANDLE;
}

bool PresentationUpscaler::Upscales(std::uint32_t width, std::uint32_t height, VkExtent2D extent) {
    const auto rect = ComputeContainRect_nid_postfix(width, height, extent.width, extent.height);
    return rect.width > width || rect.height > height;
}

VkExtent2D PresentationUpscaler::StageExtent(std::uint32_t width, std::uint32_t height, VkExtent2D extent) {
    if (Upscales(width, height, extent)) return {width, height};
    const auto rect = ComputeContainRect_nid_postfix(width, height, extent.width, extent.height);
    return {rect.width, rect.height};
}

void PresentationUpscaler::Release(std::size_t index) noexcept {
    if (index >= slots.size()) return;
    for (const auto& [view, framebuffer] : slots[index].transient) {
        if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    }
    slots[index].transient.clear();
}

void PresentationUpscaler::releaseSurface(Surface& surface) noexcept {
    if (surface.sampled) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, surface.sampled, nullptr);
    if (surface.image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, surface.image, nullptr);
    if (surface.memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, surface.memory, nullptr);
    surface = Surface{};
}

void PresentationUpscaler::ensureSurface(Surface& surface, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage) {
    if (surface.image != VK_NULL_HANDLE && surface.format == format && surface.extent.width == extent.width && surface.extent.height == extent.height) return;
    releaseSurface(surface);
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if ((usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0) needed |= VK_FORMAT_FEATURE_BLIT_DST_BIT;
    Graphics::Require((properties.optimalTilingFeatures & needed) == needed, "presentation upscaler format cannot be rendered, sampled and blitted to");
    try {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {extent.width, extent.height, 1u};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &surface.image), "vkCreateImage presentation upscaler");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, surface.image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &surface.memory), "vkAllocateMemory presentation upscaler");
        Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, surface.image, surface.memory, 0), "vkBindImageMemory presentation upscaler");
        surface.sampled = createView(surface.image, format, VK_IMAGE_USAGE_SAMPLED_BIT);
        surface.format = format;
        surface.extent = extent;
    } catch (...) {
        releaseSurface(surface);
        throw;
    }
}

VkImageView PresentationUpscaler::createView(VkImage image, VkFormat format, VkImageUsageFlags usage) const {
    VkImageViewUsageCreateInfo usageInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usageInfo.usage = usage;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.pNext = &usageInfo;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    Graphics::Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView presentation upscaler");
    return view;
}

VkImage PresentationUpscaler::Stage(std::size_t index, VkFormat format, VkExtent2D extent) {
    Graphics::Require(index < slots.size(), "presentation upscaler slot is out of range");
    Graphics::Require(extent.width != 0 && extent.height != 0, "presentation upscaler stage extent must be non-zero");
    auto& slot = slots[index];
    ensureSurface(slot.stage, format, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    return slot.stage.image;
}

void PresentationUpscaler::RecordStageTransferDestination(VkCommandBuffer commands, std::size_t index) {
    Graphics::Require(index < slots.size() && slots[index].stage.image != VK_NULL_HANDLE, "presentation upscaler slot has no stage");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slots[index].stage.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void PresentationUpscaler::RecordStageWritten(VkCommandBuffer commands, std::size_t index, VkImageLayout layout, VkPipelineStageFlags stage, VkAccessFlags access) {
    Graphics::Require(index < slots.size() && slots[index].stage.image != VK_NULL_HANDLE, "presentation upscaler slot has no stage");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = access;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slots[index].stage.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, stage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

const PresentationUpscaler::Target& PresentationUpscaler::target(VkFormat format, VkImageLayout finalLayout, Filter filter) {
    const auto key = std::tuple{format, finalLayout, filter};
    if (const auto found = targets.find(key); found != targets.end()) return found->second;
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    Graphics::Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0, "presentation upscaler format is not renderable");
    Target created;
    try {
        VkAttachmentDescription attachment{};
        attachment.format = format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = finalLayout;
        const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &reference;
        const VkSubpassDependency dependencies[2] = {
            {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0},
            PresentationPass::ExitDependency(finalLayout),
        };
        VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 2;
        renderPassInfo.pDependencies = dependencies;
        Graphics::Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &renderPassInfo, nullptr, &created.renderPass), "vkCreateRenderPass presentation upscaler");

        VkPipelineShaderStageCreateInfo stages[2]{{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertex;
        stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = filter == Filter::Easu ? easu : rcas;
        stages[1].pName = "main";
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rasterization{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;
        const VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &assembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = created.renderPass;
        const auto lock = context.LockPipelineCacheForUse();
        Graphics::Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &created.pipeline), "vkCreateGraphicsPipelines presentation upscaler");
    } catch (...) {
        if (created.renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, created.renderPass, nullptr);
        throw;
    }
    return targets.emplace(key, created).first->second;
}

VkFramebuffer PresentationUpscaler::transientFramebuffer(Slot& slot, VkRenderPass renderPass, VkImage image, VkFormat format, VkExtent2D extent, VkImageLayout finalLayout) {
    const auto view = createView(image, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (finalLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u));
    VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    info.renderPass = renderPass;
    info.attachmentCount = 1;
    info.pAttachments = &view;
    info.width = extent.width;
    info.height = extent.height;
    info.layers = 1;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    const auto created = context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &info, nullptr, &framebuffer);
    if (created != VK_SUCCESS) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    Graphics::Check(created, "vkCreateFramebuffer presentation upscaler");
    slot.transient.emplace_back(view, framebuffer);
    return framebuffer;
}

void PresentationUpscaler::bindSource(VkDescriptorSet set, VkImageView view) const {
    const VkDescriptorImageInfo imageInfo{sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
}

void PresentationUpscaler::draw(VkCommandBuffer commands, const Target& entry, VkFramebuffer framebuffer, VkExtent2D extent, VkDescriptorSet set, const void* parameters, std::uint32_t size) const {
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = entry.renderPass;
    begin.framebuffer = framebuffer;
    begin.renderArea = {{0, 0}, extent};
    context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, entry.pipeline);
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, extent};
    context.Function<PFN_vkCmdSetViewport>("vkCmdSetViewport")(commands, 0, 1, &viewport);
    context.Function<PFN_vkCmdSetScissor>("vkCmdSetScissor")(commands, 0, 1, &scissor);
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &set, 0, nullptr);
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, size, parameters);
    context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 3, 1, 0, 0);
    context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
}

void PresentationUpscaler::RecordUpscale(VkCommandBuffer commands, std::size_t index, VkImage destination, VkExtent2D extent, VkImageLayout finalLayout) {
    Graphics::Require(index < slots.size() && slots[index].stage.image != VK_NULL_HANDLE, "presentation upscaler slot has no stage");
    Graphics::Require(destination != VK_NULL_HANDLE && extent.width != 0 && extent.height != 0, "presentation upscaler destination is unavailable");
    auto& slot = slots[index];
    const auto& entry = target(slot.stage.format, finalLayout, Filter::Easu);
    const auto framebuffer = transientFramebuffer(slot, entry.renderPass, destination, slot.stage.format, extent, finalLayout);
    bindSource(slot.easuSet, slot.stage.sampled);
    const EasuParameters parameters{static_cast<float>(slot.stage.extent.width), static_cast<float>(slot.stage.extent.height), static_cast<float>(extent.width), static_cast<float>(extent.height)};
    draw(commands, entry, framebuffer, extent, slot.easuSet, &parameters, sizeof(parameters));
}

void PresentationUpscaler::RecordPresent(VkCommandBuffer commands, std::size_t index, std::uint32_t width, std::uint32_t height, VkImage destination, VkExtent2D extent, VkImageLayout finalLayout) {
    Graphics::Require(index < slots.size() && slots[index].stage.image != VK_NULL_HANDLE, "presentation upscaler slot has no stage");
    Graphics::Require(destination != VK_NULL_HANDLE && extent.width != 0 && extent.height != 0, "presentation upscaler destination is unavailable");
    auto& slot = slots[index];
    const auto staged = StageExtent(width, height, extent);
    Graphics::Require(slot.stage.extent.width == staged.width && slot.stage.extent.height == staged.height, "presentation upscaler stage does not match the source and destination");
    const auto rect = ComputeContainRect_nid_postfix(width, height, extent.width, extent.height);
    VkImageView sharpened = slot.stage.sampled;
    if (Upscales(width, height, extent)) {
        const VkExtent2D upscaled{rect.width, rect.height};
        ensureSurface(slot.upscaled, slot.stage.format, upscaled, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        RecordUpscale(commands, index, slot.upscaled.image, upscaled, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        sharpened = slot.upscaled.sampled;
    }
    const auto& entry = target(slot.stage.format, finalLayout, Filter::Rcas);
    const auto framebuffer = transientFramebuffer(slot, entry.renderPass, destination, slot.stage.format, extent, finalLayout);
    bindSource(slot.rcasSet, sharpened);
    const RcasParameters parameters{rect.x, rect.y, static_cast<std::int32_t>(rect.width), static_cast<std::int32_t>(rect.height), sharpness};
    draw(commands, entry, framebuffer, extent, slot.rcasSet, &parameters, sizeof(parameters));
}

}
