#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/MultisampleUploadFrag_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/MultisampleUpload_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/PresentationPassVert_spv.h"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>

namespace AgcDriver::Graphics {
namespace {

struct ColorUpload {
    explicit ColorUpload(const Context& context) : context(context) {}
    ~ColorUpload() {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (fragment) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, fragment, nullptr);
        if (vertex) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, vertex, nullptr);
        if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        if (renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, renderPass, nullptr);
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }

    Context context;
    VkImageView view = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

constexpr VkAccessFlags colorUploadConsumers = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

void createColorUploadTarget(ColorUpload& upload, VkImage image, VkFormat format, VkExtent2D extent, VkSampleCountFlagBits samples) {
    const auto& context = upload.context;
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usage.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.pNext = &usage;
    view.image = image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &view, nullptr, &upload.view), "vkCreateImageView multisample color upload");

    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = samples;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    const VkSubpassDependency dependency{0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, colorUploadConsumers, 0};
    VkRenderPassCreateInfo renderPass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPass.attachmentCount = 1;
    renderPass.pAttachments = &attachment;
    renderPass.subpassCount = 1;
    renderPass.pSubpasses = &subpass;
    renderPass.dependencyCount = 1;
    renderPass.pDependencies = &dependency;
    Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &renderPass, nullptr, &upload.renderPass), "vkCreateRenderPass multisample color upload");

    VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer.renderPass = upload.renderPass;
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = &upload.view;
    framebuffer.width = extent.width;
    framebuffer.height = extent.height;
    framebuffer.layers = 1;
    Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebuffer, nullptr, &upload.framebuffer), "vkCreateFramebuffer multisample color upload");
}

std::vector<VkDescriptorSet> createColorUploadSets(ColorUpload& upload, const Buffer& source, VkDeviceSize bytes, VkDeviceSize stride) {
    const auto& context = upload.context;
    const auto count = static_cast<std::uint32_t>((bytes + stride - 1) / stride);
    const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo setLayout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayout.bindingCount = 1;
    setLayout.pBindings = &binding;
    Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setLayout, nullptr, &upload.setLayout), "vkCreateDescriptorSetLayout multisample color upload");

    const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, count};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.maxSets = count;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &size;
    Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &pool, nullptr, &upload.pool), "vkCreateDescriptorPool multisample color upload");
    const std::vector<VkDescriptorSetLayout> layouts(count, upload.setLayout);
    std::vector<VkDescriptorSet> sets(count);
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = upload.pool;
    allocate.descriptorSetCount = count;
    allocate.pSetLayouts = layouts.data();
    Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocate, sets.data()), "vkAllocateDescriptorSets multisample color upload");
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto offset = VkDeviceSize{i} * stride;
        const VkDescriptorBufferInfo buffer{source.Handle(), offset, (std::min(stride, bytes - offset) + 3u) & ~VkDeviceSize{3u}};
        const VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, sets[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buffer, nullptr};
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
    }
    return sets;
}

void createColorUploadPipeline(ColorUpload& upload, VkExtent2D extent, VkSampleCountFlagBits samples, std::uint32_t elementBytes) {
    const auto& context = upload.context;
    const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4 * sizeof(std::uint32_t)};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &upload.setLayout;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &push;
    Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layout, nullptr, &upload.layout), "vkCreatePipelineLayout multisample color upload");
    upload.vertex = CreateShaderModule(context, PRESENTATION_PASS_VERT_SPV, sizeof(PRESENTATION_PASS_VERT_SPV), "vkCreateShaderModule multisample color vertex");
    upload.fragment = CreateShaderModule(context, MULTISAMPLE_UPLOAD_FRAG_SPV, sizeof(MULTISAMPLE_UPLOAD_FRAG_SPV), "vkCreateShaderModule multisample color fragment");
    const VkSpecializationMapEntry entry{0u, 0u, sizeof(elementBytes)};
    const VkSpecializationInfo specialization{1u, &entry, sizeof(elementBytes), &elementBytes};
    const std::array<VkPipelineShaderStageCreateInfo, 2> stages{{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, upload.vertex, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, upload.fragment, "main", &specialization},
    }};
    VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const VkViewport rectangle{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.pViewports = &rectangle;
    viewport.scissorCount = 1;
    const VkDynamicState scissor = VK_DYNAMIC_STATE_SCISSOR;
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 1;
    dynamic.pDynamicStates = &scissor;
    VkPipelineRasterizationStateCreateInfo rasterization{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterization.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = samples;
    multisample.sampleShadingEnable = VK_TRUE;
    multisample.minSampleShading = 1.0f;
    VkPipelineColorBlendAttachmentState color{};
    color.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &color;
    VkGraphicsPipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipeline.stageCount = static_cast<std::uint32_t>(stages.size());
    pipeline.pStages = stages.data();
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &rasterization;
    pipeline.pMultisampleState = &multisample;
    pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = upload.layout;
    pipeline.renderPass = upload.renderPass;
    auto cache = context.LockPipelineCacheForUse();
    Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &pipeline, nullptr, &upload.pipeline), "vkCreateGraphicsPipelines multisample color upload");
}

}

std::shared_ptr<void> RecordMultisampleColorUpload(const Context& context, VkCommandBuffer batchCommands, VkImage image, VkFormat format, VkExtent2D extent, std::uint32_t samples, std::uint32_t elementBytes, const Buffer& source, VkDeviceSize stride, Recorder* recorder) {
    const auto sampleCount = static_cast<VkSampleCountFlagBits>(samples);
    const auto rowElements = std::uint64_t{extent.width} * samples;
    const auto elements = rowElements * extent.height;
    Require(elements <= std::numeric_limits<std::uint32_t>::max(), "multisample color upload exceeds the element limit");
    const auto bytes = elements * elementBytes;
    auto upload = std::make_shared<ColorUpload>(context);
    createColorUploadTarget(*upload, image, format, extent, sampleCount);
    const auto sets = createColorUploadSets(*upload, source, bytes, stride);
    createColorUploadPipeline(*upload, extent, sampleCount, elementBytes);
    const auto record = [&](CommandState& recording) {
        const auto commands = recording.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = upload->renderPass;
        begin.framebuffer = upload->framebuffer;
        begin.renderArea = {{0, 0}, extent};
        context.Resolved(&DeviceFunctions::cmdBeginRenderPass, "vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        recording.BindPipeline(context, upload->pipeline, CommandState::Mask(CommandState::Scissor));
        for (std::size_t i = 0; i < sets.size(); ++i) {
            const auto first = i * stride / elementBytes;
            const auto count = std::min(stride / elementBytes, elements - first);
            const auto row = first / rowElements;
            const auto lastRow = (first + count + rowElements - 1) / rowElements;
            const VkRect2D rect{{0, static_cast<std::int32_t>(row)}, {extent.width, static_cast<std::uint32_t>(lastRow - row)}};
            const std::array<std::uint32_t, 4> push{extent.width, samples, static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(count)};
            recording.SetScissor(context, rect);
            context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, upload->layout, 0, 1, &sets[i], 0, nullptr);
            context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, upload->layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), push.data());
            context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(commands, 3, 1, 0, 0);
        }
        context.Resolved(&DeviceFunctions::cmdEndRenderPass, "vkCmdEndRenderPass")(commands);
    };
    if (recorder != nullptr) {
        recorder->Keep(upload);
        recorder->RecordUncounted(record);
    } else {
        CommandState recording;
        recording.Begin(batchCommands, false);
        record(recording);
    }
    return upload;
}

VkPipeline TextureDetiler::multisampleUploadPipeline(std::uint32_t elementBytes) {
    const auto key = (1u << 19u) | elementBytes;
    for (const auto& entry : pipelines) if (entry.first == key) return entry.second;
    if (multisampleUploadModule == VK_NULL_HANDLE) {
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = sizeof(MULTISAMPLE_UPLOAD_SPV);
        info.pCode = MULTISAMPLE_UPLOAD_SPV;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &info, nullptr, &multisampleUploadModule), "vkCreateShaderModule multisample upload");
    }
    const VkSpecializationMapEntry entry{0u, 0u, sizeof(elementBytes)};
    const VkSpecializationInfo specialization{1u, &entry, sizeof(elementBytes), &elementBytes};
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = multisampleUploadModule;
    info.stage.pName = "main";
    info.stage.pSpecializationInfo = &specialization;
    info.layout = imagePipelineLayout;
    pipelines.reserve(pipelines.size() + 1u);
    VkPipeline result = VK_NULL_HANDLE;
    {
        auto pipelineCacheLock = context.LockPipelineCacheForUse();
        Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1u, &info, nullptr, &result), "vkCreateComputePipelines multisample upload");
    }
    pipelines.emplace_back(key, result);
    return result;
}

void TextureDetiler::DispatchMultisampleImage(VkCommandBuffer commands, std::uint32_t elementBytes, VkBuffer source, std::uint64_t offset, std::uint64_t bytes, VkImageView arrayView, std::uint32_t width, std::uint32_t height, std::uint32_t layers, std::uint32_t fragments, std::uint32_t firstElement, std::uint32_t elements) {
    Require(context.storageImageMultisample && context.storageImageWriteWithoutFormat, "device does not enable multisampled storage image writes without format");
    Require(commands != VK_NULL_HANDLE && source != VK_NULL_HANDLE && arrayView != VK_NULL_HANDLE, "multisample upload requires commands, buffer and array view");
    Require(std::has_single_bit(elementBytes) && elementBytes <= 16u, "multisample upload element size is unsupported");
    Require(width != 0u && width <= 16384u && height != 0u && height <= 16384u && layers != 0u && layers <= 8192u, "multisample upload extent is unsupported");
    Require(fragments == 2u || fragments == 4u || fragments == 8u, "multisample upload fragment count is unsupported");
    const auto total = static_cast<std::uint64_t>(width) * height * layers * fragments;
    Require(total <= std::numeric_limits<std::uint32_t>::max() && elements != 0u && static_cast<std::uint64_t>(firstElement) + elements <= total, "multisample upload element range is invalid");
    const auto required = (static_cast<std::uint64_t>(elements) * elementBytes + 3u) & ~3ull;
    const auto alignment = std::max<std::uint64_t>(4u, context.limits.minStorageBufferOffsetAlignment);
    Require(offset % alignment == 0u && bytes >= required && bytes <= context.limits.maxStorageBufferRange && bytes % 4u == 0u && offset <= std::numeric_limits<std::uint64_t>::max() - bytes, "multisample upload buffer range is invalid");
    Require(context.limits.maxComputeWorkGroupInvocations >= 64u && context.limits.maxComputeWorkGroupSize[0] >= 64u && context.limits.maxComputeWorkGroupSize[1] >= 1u && context.limits.maxComputeWorkGroupSize[2] >= 1u && context.limits.maxComputeWorkGroupCount[2] >= 1u, "device does not support the multisample upload workgroup");
    const auto groups = (static_cast<std::uint64_t>(elements) + 63u) / 64u;
    const auto groupsX = std::min<std::uint64_t>(groups, context.limits.maxComputeWorkGroupCount[0]);
    Require(groupsX != 0u, "device has no multisample upload workgroups");
    const auto groupsY = (groups + groupsX - 1u) / groupsX;
    Require(groupsY <= context.limits.maxComputeWorkGroupCount[1] && groupsX * groupsY * 64u <= std::numeric_limits<std::uint32_t>::max(), "multisample upload dispatch exceeds the device limits");
    const auto target = multisampleUploadPipeline(elementBytes);
    const auto set = allocateSet(true);
    const VkDescriptorBufferInfo buffer{source, offset, bytes};
    const VkDescriptorImageInfo image{VK_NULL_HANDLE, arrayView, VK_IMAGE_LAYOUT_GENERAL};
    const std::array<VkWriteDescriptorSet, 2> writes{{{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buffer, nullptr}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &image, nullptr, nullptr}}};
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 2u, writes.data(), 0u, nullptr);
    context.BindComputePipeline(commands, target);
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, imagePipelineLayout, 0u, 1u, &set, 0u, nullptr);
    const std::array<std::uint32_t, 7> push{width, height, layers, fragments, firstElement, elements, static_cast<std::uint32_t>(groupsX * 64u)};
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, imagePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(push), push.data());
    context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, static_cast<std::uint32_t>(groupsX), static_cast<std::uint32_t>(groupsY), 1u);
}

}
