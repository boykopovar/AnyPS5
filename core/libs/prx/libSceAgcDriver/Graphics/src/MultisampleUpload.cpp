#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/MultisampleUpload_spv.h"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>

namespace AgcDriver::Graphics {

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
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, target);
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, imagePipelineLayout, 0u, 1u, &set, 0u, nullptr);
    const std::array<std::uint32_t, 7> push{width, height, layers, fragments, firstElement, elements, static_cast<std::uint32_t>(groupsX * 64u)};
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, imagePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(push), push.data());
    context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, static_cast<std::uint32_t>(groupsX), static_cast<std::uint32_t>(groupsY), 1u);
}

}
