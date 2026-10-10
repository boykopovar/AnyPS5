#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/TextureDetile_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/CmaskClear_spv.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
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
    std::uint32_t rangeBegin;
    std::uint32_t rangeEnd;
    std::uint32_t tiledBase;
    std::uint32_t linearBase;
    std::uint32_t columnBegin;
    std::uint32_t rowBegin;
};

std::uint32_t BlockBytesFor(TextureTileMode tileMode) {
    switch (tileMode) {
        case TextureTileMode::kLinear: return 0u;
        case TextureTileMode::kStandard256B:
        case TextureTileMode::kD256B: return 256u;
        case TextureTileMode::kStandard4KB:
        case TextureTileMode::kD4KB:
        case TextureTileMode::kS4KBX:
        case TextureTileMode::kD4KBX: return 4096u;
        case TextureTileMode::kStandard64KB:
        case TextureTileMode::kD64KB:
        case TextureTileMode::kS64KBT:
        case TextureTileMode::kD64KBT:
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
    for (const auto pool : cmaskDescriptorPools) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
    if (cmaskPipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, cmaskPipeline, nullptr);
    if (cmaskPipelineLayout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, cmaskPipelineLayout, nullptr);
    if (cmaskDescriptorLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, cmaskDescriptorLayout, nullptr);
    cmaskErrors.reset();
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
        const auto thickMode = tileMode == TextureTileMode::kStandard4KB ? 0x105u : tileMode == TextureTileMode::kStandard64KB ? 0x109u : tileMode == TextureTileMode::kS64KBX ? 0x119u : tileMode == TextureTileMode::kD64KBX ? 0x11au : 0u;
        Require(thickMode != 0, "3D textures are only detiled thick from SW_4KB_S, SW_64KB_S, SW_64KB_S_X or SW_64KB_D_X");
        const auto* equation = FindTextureSwizzleEquation(thickMode, elementBytes);
        Require(equation != nullptr, "no thick swizzle equation for the element size");
        values[2] = 2u;
        std::copy(equation->bits.begin(), equation->bits.end(), values.begin() + 4);
        const auto extent = ThickBlockExtent(tileMode, elementBytes);
        values[20] = extent[0];
        values[21] = extent[1];
    } else if (const auto mode = EquationSwizzleMode(tileMode); mode != 0) {
        const auto* equation = FindTextureSwizzleEquation(mode, elementBytes);
        if (equation == nullptr) Require(false, "no swizzle equation for tile mode " + std::to_string(mode) + " at " + std::to_string(elementBytes) + " bytes per element");
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

void TextureDetiler::Dispatch(VkCommandBuffer commands, TextureTileMode tileMode, std::uint32_t elementBytes, VkBuffer source, std::uint64_t sourceOffset, VkBuffer destination, std::uint64_t destinationOffset, const TileMipLayout& layout, bool retile, std::uint32_t slice, bool thick, const DetileWindow& window) {
    Require(commands != VK_NULL_HANDLE, "texture detiling requires an active command buffer");
    Require(window.rangeBegin < window.rangeEnd && window.tiledBase <= window.rangeBegin, "texture detiling window is empty");
    Require(source != VK_NULL_HANDLE && destination != VK_NULL_HANDLE, "texture detiling requires source and destination buffers");
    Require(layout.width != 0 && layout.height != 0, "texture detiling requires a non-empty mip layout");
    Require(layout.tiledSize != 0 && layout.linearSize != 0, "texture detiling requires a non-empty mip layout");
    const auto columnEnd = window.columnEnd != 0 ? std::min(window.columnEnd, layout.width) : layout.width;
    const auto rowEnd = window.rowEnd != 0 ? std::min(window.rowEnd, layout.height) : layout.height;
    if (!(window.columnBegin < columnEnd && window.rowBegin < rowEnd)) Require(false, "texture detiling window lies outside the mip: columns " + std::to_string(window.columnBegin) + ".." + std::to_string(columnEnd) + ", rows " + std::to_string(window.rowBegin) + ".." + std::to_string(rowEnd) + " of " + std::to_string(layout.width) + "x" + std::to_string(layout.height));
    const auto target = pipeline(tileMode, elementBytes, retile, thick);
    const auto alignment = std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 4);
    const auto sourceDescriptorOffset = sourceOffset - sourceOffset % alignment;
    const auto destinationDescriptorOffset = destinationOffset - destinationOffset % alignment;
    const auto sourceBase = sourceOffset - sourceDescriptorOffset;
    const auto destinationBase = destinationOffset - destinationDescriptorOffset;
    Require(sourceBase <= UINT32_MAX && destinationBase <= UINT32_MAX, "texture detiling buffer offset exceeds addressable range");
    // A window covers the mip's bytes from its base to the range's end (tiled) or the last row the
    // range can touch (linear).
    const std::uint64_t tiledBytes = window.rangeEnd == 0xffffffffu ? layout.tiledSize : std::min<std::uint64_t>(layout.tiledSize, window.rangeEnd) - window.tiledBase;
    const std::uint64_t linearBytes = window.linearBytes != 0 ? window.linearBytes : layout.linearSize - std::min<std::uint64_t>(window.linearBase, layout.linearSize);
    const auto sourceRange = (sourceBase + (retile ? linearBytes : tiledBytes) + 3) / 4 * 4;
    const auto destinationRange = (destinationBase + (retile ? tiledBytes : linearBytes) + 3) / 4 * 4;
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
    push.rangeBegin = window.rangeBegin;
    push.rangeEnd = window.rangeEnd;
    push.tiledBase = window.tiledBase;
    push.linearBase = window.linearBase;
    push.columnBegin = window.columnBegin;
    push.rowBegin = window.rowBegin;
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);
    const auto groupsX = (columnEnd - window.columnBegin + 7u) / 8u;
    const auto groupsY = (rowEnd - window.rowBegin + 7u) / 8u;
    context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, groupsX, groupsY, 1);
}

std::uint32_t TextureDetiler::CmaskErrors() {
    if (cmaskErrors == nullptr) return 0;
    std::uint32_t codes = 0;
    std::memcpy(&codes, cmaskErrors->Bytes().data(), sizeof(codes));
    return codes;
}

void TextureDetiler::DispatchCmaskClear(VkCommandBuffer commands, VkBuffer cmask, std::uint64_t cmaskOffset, std::size_t cmaskBytes, VkImageView view, std::uint32_t width, std::uint32_t height, std::uint32_t elementBytes, const std::array<std::uint32_t, 2>& clearWords, bool write) {
    Require(commands != VK_NULL_HANDLE && cmask != VK_NULL_HANDLE && view != VK_NULL_HANDLE, "a CMASK clear needs a command buffer, the CMASK buffer and the target view");
    Require(elementBytes == 1 || elementBytes == 2 || elementBytes == 4 || elementBytes == 8, "a CMASK clear of this element size is not modeled");
    Require(cmaskOffset % 4 == 0 && cmaskBytes % 4 == 0 && cmaskBytes != 0, "a CMASK clear needs a word-aligned CMASK");
    if (cmaskPipeline == VK_NULL_HANDLE) {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        for (std::uint32_t index = 0; index < bindings.size(); ++index) {
            bindings[index].binding = index;
            bindings[index].descriptorType = index == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[index].descriptorCount = 1;
            bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &cmaskDescriptorLayout), "vkCreateDescriptorSetLayout CMASK clear");
        const VkPushConstantRange pushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, 7 * sizeof(std::uint32_t)};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &cmaskDescriptorLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &pipelineLayoutInfo, nullptr, &cmaskPipelineLayout), "vkCreatePipelineLayout CMASK clear");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = sizeof(CMASK_CLEAR_SPV);
        moduleInfo.pCode = CMASK_CLEAR_SPV;
        VkShaderModule cmaskModule = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &cmaskModule), "vkCreateShaderModule CMASK clear");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = cmaskModule;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = cmaskPipelineLayout;
        const auto created = context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &cmaskPipeline);
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, cmaskModule, nullptr);
        Check(created, "vkCreateComputePipelines CMASK clear");
        cmaskErrors = std::make_unique<Buffer>(context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        std::memset(cmaskErrors->Bytes().data(), 0, 4);
    }
    constexpr std::uint32_t setsPerPool = 64;
    const auto poolIndex = allocatedCmaskSets / setsPerPool;
    if (poolIndex == cmaskDescriptorPools.size()) {
        const std::array<VkDescriptorPoolSize, 2> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, setsPerPool * 2}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, setsPerPool}}};
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.maxSets = setsPerPool;
        info.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        info.pPoolSizes = sizes.data();
        cmaskDescriptorPools.reserve(cmaskDescriptorPools.size() + 1);
        VkDescriptorPool pool = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &info, nullptr, &pool), "vkCreateDescriptorPool CMASK clear");
        cmaskDescriptorPools.push_back(pool);
    }
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = cmaskDescriptorPools.at(poolIndex);
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &cmaskDescriptorLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets CMASK clear");
    ++allocatedCmaskSets;
    const auto alignment = std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 4);
    const auto descriptorOffset = cmaskOffset - cmaskOffset % alignment;
    const auto base = cmaskOffset - descriptorOffset;
    const VkDescriptorBufferInfo cmaskInfo{cmask, descriptorOffset, base + cmaskBytes};
    const VkDescriptorBufferInfo errorInfo{cmaskErrors->Handle(), 0, 4};
    const VkDescriptorImageInfo imageInfo{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
    std::array<VkWriteDescriptorSet, 3> writes{};
    for (std::uint32_t index = 0; index < writes.size(); ++index) {
        writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[index].dstSet = set;
        writes[index].dstBinding = index;
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = index == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    writes[0].pBufferInfo = &cmaskInfo;
    writes[1].pBufferInfo = &errorInfo;
    writes[2].pImageInfo = &imageInfo;
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    const auto barrier = [&](VkPipelineStageFlags source, VkPipelineStageFlags destination, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) {
        VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        memory.srcAccessMask = sourceAccess;
        memory.dstAccessMask = destinationAccess;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, source, destination, 0, 1, &memory, 0, nullptr, 0, nullptr);
    };
    constexpr VkAccessFlags anyWrite = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
    constexpr VkAccessFlags anyAccess = anyWrite | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    barrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, anyWrite, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, cmaskPipeline);
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, cmaskPipelineLayout, 0, 1, &set, 0, nullptr);
    const std::uint32_t mask = elementBytes >= 4 ? 0xffffffffu : (1u << (elementBytes * 8u)) - 1u;
    const std::array<std::uint32_t, 7> push{static_cast<std::uint32_t>(base), width, height, (width + 1023u) / 1024u, clearWords[0] & mask, elementBytes == 8 ? clearWords[1] : 0u, write ? 1u : 0u};
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, cmaskPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push.data());
    context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, (width + 7u) / 8u, (height + 7u) / 8u, 1);
    barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, anyAccess);
    context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, cmask, cmaskOffset, cmaskBytes, 0xffffffffu);
    barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, anyAccess);
}

}
