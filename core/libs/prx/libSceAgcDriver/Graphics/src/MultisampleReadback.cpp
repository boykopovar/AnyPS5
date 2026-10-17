#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/MultisampleReadbackUint_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/MultisampleReadbackDepth_spv.h"
#include <algorithm>
#include <array>
#include <limits>

namespace AgcDriver::Graphics {
namespace {

std::uint32_t readbackElementBytes(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_UINT: return 1u;
        case VK_FORMAT_R16_UINT: return 2u;
        case VK_FORMAT_R32_UINT: case VK_FORMAT_D32_SFLOAT: return 4u;
        case VK_FORMAT_R32G32_UINT: return 8u;
        case VK_FORMAT_R32G32B32A32_UINT: return 16u;
        default: Require(false, "multisample readback requires an unsigned element view or D32 depth"); return 0u;
    }
}

}

VkPipeline TextureDetiler::multisampleReadbackPipeline(std::uint32_t elementBytes, bool depth) {
    const auto key = elementBytes | (depth ? 32u : 0u);
    for (const auto& entry : readbackPipelines) if (entry.first == key) return entry.second;
    if (readbackDescriptorLayout == VK_NULL_HANDLE) {
        const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{
            {0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
        }};
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = static_cast<std::uint32_t>(bindings.size());
        info.pBindings = bindings.data();
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &info, nullptr, &readbackDescriptorLayout), "vkCreateDescriptorSetLayout multisample readback");
    }
    if (readbackPipelineLayout == VK_NULL_HANDLE) {
        const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0u, 7u * sizeof(std::uint32_t)};
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        info.setLayoutCount = 1u;
        info.pSetLayouts = &readbackDescriptorLayout;
        info.pushConstantRangeCount = 1u;
        info.pPushConstantRanges = &push;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &info, nullptr, &readbackPipelineLayout), "vkCreatePipelineLayout multisample readback");
    }
    auto& module = readbackModules[depth ? 1u : 0u];
    if (module == VK_NULL_HANDLE) {
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = depth ? sizeof(MULTISAMPLE_READBACK_DEPTH_SPV) : sizeof(MULTISAMPLE_READBACK_UINT_SPV);
        info.pCode = depth ? MULTISAMPLE_READBACK_DEPTH_SPV : MULTISAMPLE_READBACK_UINT_SPV;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &info, nullptr, &module), "vkCreateShaderModule multisample readback");
    }
    const VkSpecializationMapEntry entry{0u, 0u, sizeof(elementBytes)};
    const VkSpecializationInfo specialization{1u, &entry, sizeof(elementBytes), &elementBytes};
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = module;
    info.stage.pName = "main";
    info.stage.pSpecializationInfo = &specialization;
    info.layout = readbackPipelineLayout;
    readbackPipelines.reserve(readbackPipelines.size() + 1u);
    VkPipeline result = VK_NULL_HANDLE;
    {
        auto pipelineCacheLock = context.LockPipelineCacheForUse();
        Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1u, &info, nullptr, &result), "vkCreateComputePipelines multisample readback");
    }
    readbackPipelines.emplace_back(key, result);
    return result;
}

void TextureDetiler::DispatchMultisampleReadback(VkCommandBuffer commands, VkFormat format, VkBuffer destination, std::uint64_t offset, std::uint64_t bytes, VkImageView arrayView, std::uint32_t width, std::uint32_t height, std::uint32_t layers, std::uint32_t fragments, std::uint32_t firstElement, std::uint32_t elements) {
    GuestMemory::AssertGpuLockHeld("TextureDetiler::DispatchMultisampleReadback");
    const auto elementBytes = readbackElementBytes(format);
    Require(commands != VK_NULL_HANDLE && destination != VK_NULL_HANDLE && arrayView != VK_NULL_HANDLE, "multisample readback requires commands, buffer and array view");
    Require(width != 0u && width <= 16384u && height != 0u && height <= 16384u && layers != 0u && layers <= 8192u, "multisample readback extent is unsupported");
    Require(fragments == 2u || fragments == 4u || fragments == 8u, "multisample readback fragment count is unsupported");
    const auto total = static_cast<std::uint64_t>(width) * height * layers * fragments;
    Require(total <= std::numeric_limits<std::uint32_t>::max() && elements != 0u && static_cast<std::uint64_t>(firstElement) + elements <= total, "multisample readback element range is invalid");
    const auto required = (static_cast<std::uint64_t>(elements) * elementBytes + 3u) & ~3ull;
    const auto alignment = std::max<std::uint64_t>(4u, context.limits.minStorageBufferOffsetAlignment);
    Require(offset % alignment == 0u && bytes >= required && bytes <= context.limits.maxStorageBufferRange && bytes % 4u == 0u && offset <= std::numeric_limits<std::uint64_t>::max() - bytes, "multisample readback buffer range is invalid");
    Require(context.limits.maxComputeWorkGroupInvocations >= 64u && context.limits.maxComputeWorkGroupSize[0] >= 64u && context.limits.maxComputeWorkGroupSize[1] >= 1u && context.limits.maxComputeWorkGroupSize[2] >= 1u && context.limits.maxComputeWorkGroupCount[2] >= 1u, "device does not support the multisample readback workgroup");
    const auto packedElements = elementBytes < 4u ? 4u / elementBytes : 1u;
    const auto threads = (static_cast<std::uint64_t>(elements) + packedElements - 1u) / packedElements;
    const auto groups = (threads + 63u) / 64u;
    const auto groupsX = std::min<std::uint64_t>(groups, context.limits.maxComputeWorkGroupCount[0]);
    Require(groupsX != 0u, "device has no multisample readback workgroups");
    const auto groupsY = (groups + groupsX - 1u) / groupsX;
    Require(groupsY <= context.limits.maxComputeWorkGroupCount[1] && groupsX * groupsY * 64u <= std::numeric_limits<std::uint32_t>::max(), "multisample readback dispatch exceeds device limits");
    const auto pipeline = multisampleReadbackPipeline(elementBytes, format == VK_FORMAT_D32_SFLOAT);
    const auto set = allocateFrom(readbackDescriptorSets, readbackDescriptorLayout, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64u}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 64u}}, "vkCreateDescriptorPool multisample readback", "vkAllocateDescriptorSets multisample readback", "vkResetDescriptorPool multisample readback");
    const VkDescriptorBufferInfo buffer{destination, offset, bytes};
    const VkDescriptorImageInfo image{VK_NULL_HANDLE, arrayView, VK_IMAGE_LAYOUT_GENERAL};
    const std::array<VkWriteDescriptorSet, 2> writes{{
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buffer, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &image, nullptr, nullptr}
    }};
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0u, nullptr);
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, readbackPipelineLayout, 0u, 1u, &set, 0u, nullptr);
    const std::array<std::uint32_t, 7> push{width, height, layers, fragments, firstElement, elements, static_cast<std::uint32_t>(groupsX * 64u)};
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, readbackPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(push), push.data());
    context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, static_cast<std::uint32_t>(groupsX), static_cast<std::uint32_t>(groupsY), 1u);
}

}
