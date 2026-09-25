#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/TextureDetile_spv.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

namespace {

struct Push {
    std::uint32_t srcBase;
    std::uint32_t dstBase;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitchBytes;
    std::uint32_t blocksPerRow;
    std::uint32_t tail;
    std::uint32_t tailX;
    std::uint32_t tailY;
    std::uint32_t elementBytes;
    std::uint32_t slice;
};

std::uint32_t BlockBytesFor(TextureTileMode tileMode) {
    switch (tileMode) {
        case TextureTileMode::kLinear: return 0u;
        case TextureTileMode::kStandard256B: return 256u;
        case TextureTileMode::kStandard4KB: return 4096u;
        case TextureTileMode::kStandard64KB:
        case TextureTileMode::kZ64KBX:
        case TextureTileMode::kS64KBX:
        case TextureTileMode::kD64KBX:
        case TextureTileMode::kR64KBX: return 65536u;
    }
    throw std::runtime_error("AGC graphics: TextureDetiler encountered an unknown tile mode");
}

std::uint32_t PipelineKey(TextureTileMode tileMode, std::uint32_t elementBytes, bool retile, bool thick) {
    return (thick ? 1u << 17 : 0u) | (retile ? 1u << 16 : 0u) | (static_cast<std::uint32_t>(tileMode) << 8) | elementBytes;
}

}

TextureDetiler::TextureDetiler(const Context& context) : context(context) {
    try {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        for (std::uint32_t index = 0; index < bindings.size(); ++index) {
            bindings[index].binding = index;
            bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[index].descriptorCount = 1;
            bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &descriptorLayout), "vkCreateDescriptorSetLayout");
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(Push);
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = sizeof(TEXTURE_DETILE_SPV);
        moduleInfo.pCode = TEXTURE_DETILE_SPV;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
    } catch (...) {
        release();
        throw;
    }
}

TextureDetiler::~TextureDetiler() {
    release();
}

void TextureDetiler::release() noexcept {
    for (const auto pool : descriptorPools) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
    for (const auto& entry : pipelines) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, entry.second, nullptr);
    if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    if (pipelineLayout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
    if (descriptorLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, descriptorLayout, nullptr);
}

VkPipeline TextureDetiler::pipeline(TextureTileMode tileMode, std::uint32_t elementBytes, bool retile, bool thick) {
    Require(std::has_single_bit(elementBytes) && elementBytes <= 16u, "unsupported element size for texture detiling");
    const auto key = PipelineKey(tileMode, elementBytes, retile, thick);
    for (const auto& entry : pipelines) {
        if (entry.first == key) return entry.second;
    }
    // Constants 0-3 select element size, block size, addressing family and direction; 4-19 carry the
    // per-bit XOR equation for the equation family (2); 20-21 override the block extent in elements.
    std::array<std::uint32_t, 22> values{elementBytes, BlockBytesFor(tileMode), tileMode == TextureTileMode::kLinear ? 0u : 1u, retile ? 1u : 0u};
    if (thick) {
        const auto thickMode = tileMode == TextureTileMode::kStandard4KB ? 0x105u : tileMode == TextureTileMode::kStandard64KB ? 0x109u : 0u;
        Require(thickMode != 0, "3D textures are only detiled from SW_4KB_S or SW_64KB_S");
        const auto* equation = FindTextureSwizzleEquation(thickMode, elementBytes);
        Require(equation != nullptr, "no thick swizzle equation for the element size");
        values[2] = 2u;
        std::copy(equation->bits.begin(), equation->bits.end(), values.begin() + 4);
        const auto extent = ThickBlockExtent(tileMode, elementBytes);
        values[20] = extent[0];
        values[21] = extent[1];
    } else if (const auto mode = XorSwizzleMode(tileMode); mode != 0) {
        const auto* equation = FindTextureSwizzleEquation(mode, elementBytes);
        Require(equation != nullptr, "no swizzle equation for tile mode " + std::to_string(mode) + " at " + std::to_string(elementBytes) + " bytes per element");
        values[2] = 2u;
        std::copy(equation->bits.begin(), equation->bits.end(), values.begin() + 4);
    }
    std::array<VkSpecializationMapEntry, 22> entries{};
    for (std::uint32_t index = 0; index < entries.size(); ++index) entries[index] = {index, index * 4u, 4};
    VkSpecializationInfo specialization{};
    specialization.mapEntryCount = static_cast<std::uint32_t>(entries.size());
    specialization.pMapEntries = entries.data();
    specialization.dataSize = sizeof(values);
    specialization.pData = values.data();
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = module;
    stage.pName = "main";
    stage.pSpecializationInfo = &specialization;
    VkComputePipelineCreateInfo createInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    createInfo.stage = stage;
    createInfo.layout = pipelineLayout;
    VkPipeline result = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &createInfo, nullptr, &result), "vkCreateComputePipelines");
    pipelines.emplace_back(key, result);
    return result;
}

void TextureDetiler::Dispatch(VkCommandBuffer commands, TextureTileMode tileMode, std::uint32_t elementBytes, VkBuffer source, std::uint64_t sourceOffset, VkBuffer destination, std::uint64_t destinationOffset, const TileMipLayout& layout, bool retile, std::uint32_t slice, bool thick) {
    Require(commands != VK_NULL_HANDLE, "texture detiling requires an active command buffer");
    Require(source != VK_NULL_HANDLE && destination != VK_NULL_HANDLE, "texture detiling requires source and destination buffers");
    Require(layout.width != 0 && layout.height != 0, "texture detiling requires a non-empty mip layout");
    Require(layout.tiledSize != 0 && layout.linearSize != 0, "texture detiling requires a non-empty mip layout");
    const auto target = pipeline(tileMode, elementBytes, retile, thick);
    const auto alignment = std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 4);
    const auto sourceDescriptorOffset = sourceOffset - sourceOffset % alignment;
    const auto destinationDescriptorOffset = destinationOffset - destinationOffset % alignment;
    const auto sourceBase = sourceOffset - sourceDescriptorOffset;
    const auto destinationBase = destinationOffset - destinationDescriptorOffset;
    Require(sourceBase <= UINT32_MAX && destinationBase <= UINT32_MAX, "texture detiling buffer offset exceeds addressable range");
    const auto sourceRange = (sourceBase + (retile ? layout.linearSize : layout.tiledSize) + 3) / 4 * 4;
    const auto destinationRange = (destinationBase + (retile ? layout.tiledSize : layout.linearSize) + 3) / 4 * 4;
    Require(sourceRange <= context.limits.maxStorageBufferRange && destinationRange <= context.limits.maxStorageBufferRange, "texture detiling buffer range exceeds device limits");
    const auto set = allocateSet();
    const VkDescriptorBufferInfo sourceInfo{source, sourceDescriptorOffset, sourceRange};
    const VkDescriptorBufferInfo destinationInfo{destination, destinationDescriptorOffset, destinationRange};
    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &sourceInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &destinationInfo;
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, target);
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
    Push push{};
    push.srcBase = static_cast<std::uint32_t>(sourceBase);
    push.dstBase = static_cast<std::uint32_t>(destinationBase);
    push.width = layout.width;
    push.height = layout.height;
    push.pitchBytes = layout.pitchBytes;
    push.blocksPerRow = layout.blocksPerRow;
    push.tail = layout.tail ? 1u : 0u;
    push.tailX = layout.tailX;
    push.tailY = layout.tailY;
    push.elementBytes = elementBytes;
    push.slice = slice;
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);
    const auto groupsX = (layout.width + 7u) / 8u;
    const auto groupsY = (layout.height + 7u) / 8u;
    context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, groupsX, groupsY, 1);
}

}
