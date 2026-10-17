#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/PresentationPassFrag_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/PresentationPassVert_spv.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace AgcDriver {
namespace {

constexpr std::uint32_t ModeTenBit = 1;
constexpr std::uint32_t ModeRedLow = 2;
constexpr std::uint32_t ModeKeepTenBit = 4;
constexpr std::uint32_t ModeSubsample = 8;
constexpr std::uint32_t ModePq = 16;

}

PresentationPass::PresentationPass(const Graphics::Context& context, std::size_t slotCount) : context(context), slots(slotCount) {
    Graphics::Require(slotCount != 0, "presentation pass needs at least one slot");
    try {
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Graphics::Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &samplerInfo, nullptr, &sampler), "vkCreateSampler presentation pass");
        const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
        VkDescriptorSetLayoutCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptorInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        descriptorInfo.pBindings = bindings.data();
        Graphics::Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &descriptorInfo, nullptr, &descriptorLayout), "vkCreateDescriptorSetLayout presentation pass");
        const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 36};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        Graphics::Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout presentation pass");
        const std::array<VkDescriptorPoolSize, 2> sizes{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<std::uint32_t>(slotCount)}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<std::uint32_t>(slotCount)}}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = static_cast<std::uint32_t>(slotCount);
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();
        Graphics::Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &descriptorPool), "vkCreateDescriptorPool presentation pass");
        for (auto& slot : slots) {
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool = descriptorPool;
            allocation.descriptorSetCount = 1;
            allocation.pSetLayouts = &descriptorLayout;
            Graphics::Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &slot.set), "vkAllocateDescriptorSets presentation pass");
        }
        const auto& luminance = PqEotfNits();
        pqNits = std::make_unique<Graphics::Buffer>(context, sizeof(luminance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        std::memcpy(pqNits->Bytes().data(), luminance.data(), sizeof(luminance));
        const VkDescriptorBufferInfo tableInfo{pqNits->Handle(), 0, sizeof(luminance)};
        std::vector<VkWriteDescriptorSet> tableWrites(slots.size(), VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET});
        for (std::size_t i = 0; i < slots.size(); ++i) {
            tableWrites[i].dstSet = slots[i].set;
            tableWrites[i].dstBinding = 1;
            tableWrites[i].descriptorCount = 1;
            tableWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            tableWrites[i].pBufferInfo = &tableInfo;
        }
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(tableWrites.size()), tableWrites.data(), 0, nullptr);
        const auto createModule = context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        const auto presentationPassVertCode = Graphics::CacheKeyedSpirv(context.graphicsPipelineLibrary, PRESENTATION_PASS_VERT_SPV, sizeof(PRESENTATION_PASS_VERT_SPV));
        moduleInfo.codeSize = presentationPassVertCode.size() * sizeof(std::uint32_t);
        moduleInfo.pCode = presentationPassVertCode.data();
        Graphics::Check(createModule(context.device, &moduleInfo, nullptr, &vertex), "vkCreateShaderModule presentation vertex");
        const auto presentationPassFragCode = Graphics::CacheKeyedSpirv(context.graphicsPipelineLibrary, PRESENTATION_PASS_FRAG_SPV, sizeof(PRESENTATION_PASS_FRAG_SPV));
        moduleInfo.codeSize = presentationPassFragCode.size() * sizeof(std::uint32_t);
        moduleInfo.pCode = presentationPassFragCode.data();
        Graphics::Check(createModule(context.device, &moduleInfo, nullptr, &fragment), "vkCreateShaderModule presentation fragment");
    } catch (...) {
        release();
        throw;
    }
}

PresentationPass::~PresentationPass() {
    release();
}

void PresentationPass::release() noexcept {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        Release(i);
        releaseDumpTarget(slots[i]);
    }
    for (const auto& [key, entry] : targets) {
        if (entry.pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, entry.pipeline, nullptr);
        if (entry.renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, entry.renderPass, nullptr);
    }
    targets.clear();
    if (fragment) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, fragment, nullptr);
    if (vertex) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, vertex, nullptr);
    if (descriptorPool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, descriptorPool, nullptr);
    if (pipelineLayout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
    if (descriptorLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, descriptorLayout, nullptr);
    if (sampler) context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, sampler, nullptr);
    pqNits.reset();
    fragment = vertex = VK_NULL_HANDLE;
    descriptorPool = VK_NULL_HANDLE;
    pipelineLayout = VK_NULL_HANDLE;
    descriptorLayout = VK_NULL_HANDLE;
    sampler = VK_NULL_HANDLE;
}

bool PresentationPass::TenBitFormat(VkFormat format) {
    return format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || format == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
}

VkExtent2D PresentationPass::DumpExtent(std::uint32_t width, std::uint32_t height, std::uint32_t stride) {
    Graphics::Require(stride != 0, "presentation dump stride must be non-zero");
    return {(width + stride - 1) / stride, (height + stride - 1) / stride};
}

void PresentationPass::Release(std::size_t index) noexcept {
    if (index >= slots.size()) return;
    auto& slot = slots[index];
    for (const auto& [view, framebuffer] : slot.transient) {
        if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    }
    slot.transient.clear();
    if (slot.source) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, slot.source, nullptr);
    slot.source = VK_NULL_HANDLE;
}

VkImageView PresentationPass::createView(VkImage image, VkFormat format, VkImageUsageFlags usage, std::uint32_t layer) const {
    VkImageViewUsageCreateInfo usageInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usageInfo.usage = usage;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.pNext = &usageInfo;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, layer, 1};
    VkImageView view = VK_NULL_HANDLE;
    Graphics::Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView presentation pass");
    return view;
}

VkFramebuffer PresentationPass::createFramebuffer(VkRenderPass renderPass, VkImageView view, VkExtent2D extent) const {
    VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    info.renderPass = renderPass;
    info.attachmentCount = 1;
    info.pAttachments = &view;
    info.width = extent.width;
    info.height = extent.height;
    info.layers = 1;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    Graphics::Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &info, nullptr, &framebuffer), "vkCreateFramebuffer presentation pass");
    return framebuffer;
}

const PresentationPass::Target& PresentationPass::target(VkFormat format, VkImageLayout finalLayout) {
    const auto key = std::pair{format, finalLayout};
    if (const auto found = targets.find(key); found != targets.end()) return found->second;
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    Graphics::Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0, "presentation pass format is not renderable");
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
        const bool transfer = finalLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        const VkSubpassDependency dependencies[2] = {
            {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0},
            {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, transfer ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, transfer ? VK_ACCESS_TRANSFER_READ_BIT : VkAccessFlags{0}, 0},
        };
        VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 2;
        renderPassInfo.pDependencies = dependencies;
        Graphics::Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &renderPassInfo, nullptr, &created.renderPass), "vkCreateRenderPass presentation pass");

        VkPipelineShaderStageCreateInfo stages[2]{{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertex;
        stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragment;
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
        Graphics::Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &created.pipeline), "vkCreateGraphicsPipelines presentation pass");
    } catch (...) {
        if (created.renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, created.renderPass, nullptr);
        throw;
    }
    return targets.emplace(key, created).first->second;
}

void PresentationPass::Bind(std::size_t index, VkImage image, std::uint32_t width, std::uint32_t height, std::uint64_t pixelFormat, float whiteNits, std::uint32_t layer) {
    Graphics::Require(index < slots.size(), "presentation pass slot is out of range");
    Graphics::Require(image != VK_NULL_HANDLE && width != 0 && height != 0, "presentation pass source is unavailable");
    Graphics::Require(!DisplayHalfFloat(pixelFormat), "presentation pass does not decode 16-bit float display buffers");
    Graphics::Require(std::isfinite(whiteNits) && whiteNits >= 1.0f, "presentation pass white level must be at least 1 nit");
    Release(index);
    auto& slot = slots[index];
    slot.source = createView(image, VK_FORMAT_R32_UINT, VK_IMAGE_USAGE_SAMPLED_BIT, layer);
    slot.width = width;
    slot.height = height;
    slot.mode = (DisplayTenBit(pixelFormat) ? ModeTenBit : 0u) | (DisplayRedLow(pixelFormat) ? ModeRedLow : 0u) | (DisplayPq(pixelFormat) ? ModePq : 0u);
    slot.whiteNits = whiteNits;
    const VkDescriptorImageInfo imageInfo{sampler, slot.source, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = slot.set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
}

void PresentationPass::draw(VkCommandBuffer commands, const Slot& slot, const Target& entry, VkFramebuffer framebuffer, VkExtent2D extent, const std::int32_t (&parameters)[9]) const {
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
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &slot.set, 0, nullptr);
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(parameters), parameters);
    context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 3, 1, 0, 0);
    context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
}

void PresentationPass::RecordPresent(VkCommandBuffer commands, std::size_t index, VkImage destination, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, VkImageLayout finalLayout) {
    Graphics::Require(index < slots.size() && slots[index].source != VK_NULL_HANDLE, "presentation pass slot has no source");
    Graphics::Require(destination != VK_NULL_HANDLE && extent.width != 0 && extent.height != 0, "presentation pass destination is unavailable");
    auto& slot = slots[index];
    const auto& entry = target(format, finalLayout);
    const auto view = createView(destination, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (finalLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u));
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    try {
        framebuffer = createFramebuffer(entry.renderPass, view, extent);
    } catch (...) {
        context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        throw;
    }
    slot.transient.emplace_back(view, framebuffer);
    const auto rect = ComputeContainRect_nid_postfix(slot.width, slot.height, extent.width, extent.height);
    const bool hdr10 = colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT;
    Graphics::Require(colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR || (hdr10 && (slot.mode & ModePq) != 0 && TenBitFormat(format)), "presentation pass presents only PQ display buffers to a 10-bit HDR10 destination");
    const std::uint32_t mode = (hdr10 ? slot.mode & ~ModePq : slot.mode) | (TenBitFormat(format) ? ModeKeepTenBit : 0u);
    const std::int32_t parameters[9] = {rect.x, rect.y, static_cast<std::int32_t>(rect.width), static_cast<std::int32_t>(rect.height), static_cast<std::int32_t>(slot.width), static_cast<std::int32_t>(slot.height), 1, static_cast<std::int32_t>(mode), std::bit_cast<std::int32_t>(slot.whiteNits)};
    draw(commands, slot, entry, framebuffer, extent, parameters);
}

void PresentationPass::releaseDumpTarget(Slot& slot) noexcept {
    if (slot.dumpFramebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, slot.dumpFramebuffer, nullptr);
    if (slot.dumpView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, slot.dumpView, nullptr);
    if (slot.dumpImage) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, slot.dumpImage, nullptr);
    if (slot.dumpMemory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, slot.dumpMemory, nullptr);
    slot.dumpFramebuffer = VK_NULL_HANDLE;
    slot.dumpView = VK_NULL_HANDLE;
    slot.dumpImage = VK_NULL_HANDLE;
    slot.dumpMemory = VK_NULL_HANDLE;
    slot.dumpExtent = {};
}

void PresentationPass::ensureDumpTarget(Slot& slot, VkExtent2D extent) {
    if (slot.dumpImage != VK_NULL_HANDLE && slot.dumpExtent.width == extent.width && slot.dumpExtent.height == extent.height) return;
    releaseDumpTarget(slot);
    try {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        imageInfo.extent = {extent.width, extent.height, 1u};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &slot.dumpImage), "vkCreateImage presentation dump");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, slot.dumpImage, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &slot.dumpMemory), "vkAllocateMemory presentation dump");
        Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, slot.dumpImage, slot.dumpMemory, 0), "vkBindImageMemory presentation dump");
        slot.dumpView = createView(slot.dumpImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        slot.dumpFramebuffer = createFramebuffer(target(VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL).renderPass, slot.dumpView, extent);
        slot.dumpExtent = extent;
    } catch (...) {
        releaseDumpTarget(slot);
        throw;
    }
}

void PresentationPass::RecordDump(VkCommandBuffer commands, std::size_t index, std::uint32_t stride, VkBuffer destination) {
    Graphics::Require(index < slots.size() && slots[index].source != VK_NULL_HANDLE, "presentation pass slot has no source");
    Graphics::Require(destination != VK_NULL_HANDLE, "presentation dump buffer is unavailable");
    auto& slot = slots[index];
    const auto extent = DumpExtent(slot.width, slot.height, stride);
    ensureDumpTarget(slot, extent);
    const auto& entry = target(VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    const std::int32_t parameters[9] = {0, 0, static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), static_cast<std::int32_t>(slot.width), static_cast<std::int32_t>(slot.height), static_cast<std::int32_t>(stride), static_cast<std::int32_t>(slot.mode | ModeSubsample), std::bit_cast<std::int32_t>(slot.whiteNits)};
    draw(commands, slot, entry, slot.dumpFramebuffer, extent, parameters);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {extent.width, extent.height, 1u};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, slot.dumpImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination, 1, &copy);
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
}

}
