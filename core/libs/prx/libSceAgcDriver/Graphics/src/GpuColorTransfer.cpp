#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/ColorTransfer_spv.h"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <array>

namespace AgcDriver::Graphics {

GpuColorTransfer::GpuColorTransfer(const Context& context) : context(context) {
    VkShaderModule module = VK_NULL_HANDLE;
    try {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        for (std::uint32_t i = 0; i < bindings.size(); ++i) {
            bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        }
        VkDescriptorSetLayoutCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptorInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        descriptorInfo.pBindings = bindings.data();
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &descriptorInfo, nullptr, &descriptorLayout), "vkCreateDescriptorSetLayout color transfer");
        const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 16};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout color transfer");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = sizeof(COLOR_TRANSFER_SPV);
        moduleInfo.pCode = COLOR_TRANSFER_SPV;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule color transfer");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = module;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = pipelineLayout;
        Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines color transfer");
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        module = VK_NULL_HANDLE;
        const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &size;
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &descriptorPool), "vkCreateDescriptorPool color transfer");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = descriptorPool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &descriptorLayout;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &descriptorSet), "vkAllocateDescriptorSets color transfer");
    } catch (...) {
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        release();
        throw;
    }
}

GpuColorTransfer::~GpuColorTransfer() {
    release();
}

void GpuColorTransfer::release() noexcept {
    if (descriptorPool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, descriptorPool, nullptr);
    if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
    if (pipelineLayout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
    if (descriptorLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, descriptorLayout, nullptr);
}

void GpuColorTransfer::prepare(std::uint32_t newWidth, std::uint32_t newHeight, ColorTileMode newMode) {
    const ColorTargetLayout layout(newWidth, newHeight, newMode);
    Require(layout.Bytes() <= context.limits.maxStorageBufferRange && layout.LinearBytes() <= context.limits.maxStorageBufferRange, "color transfer exceeds storage buffer limits");
    Require((newWidth + 7u) / 8u <= context.limits.maxComputeWorkGroupCount[0] && (newHeight + 7u) / 8u <= context.limits.maxComputeWorkGroupCount[1], "color transfer exceeds workgroup limits");
    if (tiled && width == newWidth && height == newHeight && mode == newMode) return;
    constexpr VkBufferUsageFlags copies = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto newTiled = std::make_unique<Buffer>(context, layout.Bytes(), copies);
    auto newTiledDevice = std::make_unique<DeviceBuffer>(context, layout.Bytes(), copies | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto newLinear = std::make_unique<DeviceBuffer>(context, layout.LinearBytes(), copies | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const std::array<VkDescriptorBufferInfo, 2> buffers{{{newTiledDevice->Handle(), 0, layout.Bytes()}, {newLinear->Handle(), 0, layout.LinearBytes()}}};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::uint32_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &buffers[i];
    }
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    tiled = std::move(newTiled);
    tiledDevice = std::move(newTiledDevice);
    linear = std::move(newLinear);
    width = newWidth;
    height = newHeight;
    mode = newMode;
}

void GpuColorTransfer::Upload(std::uint64_t address, std::uint32_t newWidth, std::uint32_t newHeight, ColorTileMode newMode) {
    prepare(newWidth, newHeight, newMode);
    const ColorTargetLayout layout(width, height, mode);
    GuestMemory::Read(address, tiled->Bytes(), layout.Alignment());
}

void GpuColorTransfer::convert(VkCommandBuffer commands, bool toTiled, bool swapRedBlue, bool tenBit) {
    Require(commands != VK_NULL_HANDLE && tiled && tiledDevice && linear, "color transfer is not prepared");
    // The guest bytes move to video memory with one DMA copy; the shader's scattered accesses stay local.
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    CopyBuffer(context, commands, tiled->Handle(), 0, tiledDevice->Handle(), 0, tiledDevice->Size());
    VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    before.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    before.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    const auto barrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &before, 0, nullptr, 0, nullptr);
    const std::array<std::uint32_t, 4> push{width, height, (width + 127u) / 128u, (toTiled ? 1u : 0u) | (swapRedBlue ? 2u : 0u) | (mode == ColorTileMode::RenderTarget ? 4u : 0u) | (tenBit ? 8u : 0u)};
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push.data());
    context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, (width + 7u) / 8u, (height + 7u) / 8u, 1);
    VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    after.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    after.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &after, 0, nullptr, 0, nullptr);
    if (toTiled) {
        CopyBuffer(context, commands, tiledDevice->Handle(), 0, tiled->Handle(), 0, tiledDevice->Size());
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    }
}

void GpuColorTransfer::Detile(VkCommandBuffer commands, bool swapRedBlue, bool tenBit) {
    convert(commands, false, swapRedBlue, tenBit);
}

void GpuColorTransfer::Tile(VkCommandBuffer commands) {
    convert(commands, true, false);
}

void GpuColorTransfer::WriteBack(std::uint64_t address) {
    Require(tiled != nullptr, "color transfer is not prepared for writeback");
    const ColorTargetLayout layout(width, height, mode);
    GuestMemory::Write(address, tiled->Bytes(), layout.Alignment());
}

VkBuffer GpuColorTransfer::LinearBuffer() const {
    Require(linear != nullptr, "color transfer is not prepared");
    return linear->Handle();
}

RenderTarget& GpuColorTransfer::Target(const ColorTarget& color, bool blending) {
    if (!target || targetExtent.width != color.extent.width || targetExtent.height != color.extent.height || targetFormat != color.format || targetBlending != blending) {
        auto replacement = std::make_unique<RenderTarget>(context, color, blending);
        target = std::move(replacement);
        targetExtent = color.extent;
        targetFormat = color.format;
        targetBlending = blending;
    }
    return *target;
}

}
