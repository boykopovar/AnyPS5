#include "prx/libSceAgcDriver/Graphics/include/StagedMerge.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/PackedMerge_spv.h"
#include <array>
#include <limits>
#include <map>
#include <mutex>
#include <vector>

namespace AgcDriver::Graphics {
namespace {

constexpr VkAccessFlags anyWrite = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
constexpr VkAccessFlags anyAccess = anyWrite | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
constexpr std::uint32_t setsPerPool = 64;

struct MergePush {
    std::uint32_t origin[2];
    std::uint32_t end[2];
    std::uint32_t mask;
    std::uint32_t samples;
};

class Temporary final : public StagedTemporary {
public:
    Temporary(const Context& context, VkExtent2D extent, std::uint32_t samples) : context(context), extent(extent), samples(samples) {
        try {
            VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = VK_FORMAT_R32_UINT;
            imageInfo.extent = {extent.width, extent.height, 1};
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = static_cast<VkSampleCountFlagBits>(samples);
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage staged merge");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            memory = AllocateImageMemory(context, allocation, requirements.memoryTypeBits, "vkAllocateMemory staged merge");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory staged merge");
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_R32_UINT;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView staged merge");
        } catch (...) {
            release();
            throw;
        }
    }
    ~Temporary() override { release(); }
    Temporary(const Temporary&) = delete;
    Temporary& operator=(const Temporary&) = delete;

    VkImage Image() const override { return image; }
    VkImageView View() const override { return view; }

    Context context;
    VkExtent2D extent;
    std::uint32_t samples;

private:
    void release() noexcept {
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
    }

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

class StagedMerges {
public:
    explicit StagedMerges(const Context& context) : context(context) {}
    ~StagedMerges() { release(); }
    StagedMerges(const StagedMerges&) = delete;
    StagedMerges& operator=(const StagedMerges&) = delete;

    std::shared_ptr<StagedTemporary> Acquire(VkExtent2D extent, std::uint32_t samples) {
        for (const auto& temporary : temporaries) {
            if (temporary->extent.width == extent.width && temporary->extent.height == extent.height && temporary->samples == samples) return temporary;
        }
        Require(context.storageImageMultisample, "a partial write of a multisampled packed color target needs shaderStorageImageMultisample");
        VkImageFormatProperties supported{};
        const auto result = context.imageFormatProperties(context.physical, VK_FORMAT_R32_UINT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &supported);
        Require(result == VK_SUCCESS && (supported.sampleCounts & samples) != 0, "a partial write of a multisampled packed color target needs a multisampled R32_UINT color and storage image at this sample count");
        temporaries.push_back(std::make_shared<Temporary>(context, extent, samples));
        return temporaries.back();
    }

    void Merge(VkCommandBuffer commands, StagedTemporary& temporary, VkImageView target, std::uint32_t mask, std::uint32_t samples, VkRect2D area) {
        const auto pipeline = mergePipeline();
        const auto set = allocate();
        const std::array<VkDescriptorImageInfo, 2> images{{{VK_NULL_HANDLE, temporary.View(), VK_IMAGE_LAYOUT_GENERAL}, {VK_NULL_HANDLE, target, VK_IMAGE_LAYOUT_GENERAL}}};
        std::array<VkWriteDescriptorSet, 2> writes{};
        for (std::uint32_t binding = 0; binding < 2; ++binding) {
            writes[binding] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[binding].dstSet = set;
            writes[binding].dstBinding = binding;
            writes[binding].descriptorCount = 1;
            writes[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[binding].pImageInfo = &images[binding];
        }
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 2, writes.data(), 0, nullptr);
        const auto x = static_cast<std::uint32_t>(area.offset.x);
        const auto y = static_cast<std::uint32_t>(area.offset.y);
        const MergePush push{{x, y}, {x + area.extent.width, y + area.extent.height}, mask, samples};
        context.Resolved(&DeviceFunctions::cmdBindPipeline, "vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
        context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
        context.Resolved(&DeviceFunctions::cmdDispatch, "vkCmdDispatch")(commands, (area.extent.width + 7u) / 8u, (area.extent.height + 7u) / 8u, 1);
    }

    Context context;

private:
    VkPipeline mergePipeline() {
        if (pipeline != VK_NULL_HANDLE) return pipeline;
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}}};
        VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setInfo.bindingCount = 2;
        setInfo.pBindings = bindings.data();
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout staged merge");
        const VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MergePush)};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &range;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout staged merge");
        const auto shader = CreateShaderModule(context, PACKED_MERGE_SPV, sizeof(PACKED_MERGE_SPV), "vkCreateShaderModule staged merge");
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        info.stage.module = shader;
        info.stage.pName = "main";
        info.layout = pipelineLayout;
        VkResult created;
        {
            auto pipelineCacheLock = context.LockPipelineCacheForUse();
            created = context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &info, nullptr, &pipeline);
        }
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, shader, nullptr);
        Check(created, "vkCreateComputePipelines staged merge");
        return pipeline;
    }

    VkDescriptorSet allocate() {
        const auto* recorder = Recorder::Active();
        if (pools.empty() || used == setsPerPool) {
            const auto completed = recorder != nullptr ? recorder->CompletedThrough() : 0;
            auto next = pools.size();
            for (std::size_t step = 1; step <= pools.size(); ++step) {
                const auto index = (current + step) % pools.size();
                if (serials[index] <= completed) {
                    next = index;
                    break;
                }
            }
            if (next < pools.size()) {
                Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, pools[next], 0), "vkResetDescriptorPool staged merge");
            } else {
                const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, setsPerPool * 2u};
                VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
                info.maxSets = setsPerPool;
                info.poolSizeCount = 1;
                info.pPoolSizes = &size;
                VkDescriptorPool pool = VK_NULL_HANDLE;
                Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &info, nullptr, &pool), "vkCreateDescriptorPool staged merge");
                pools.push_back(pool);
                serials.push_back(0);
            }
            current = next;
            used = 0;
        }
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pools[current];
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &setLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets staged merge");
        ++used;
        serials[current] = recorder != nullptr && recorder->Device() == context.device ? recorder->OpenSerial() : std::numeric_limits<std::uint64_t>::max();
        return set;
    }

    void release() noexcept {
        temporaries.clear();
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        for (const auto pool : pools) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (pipelineLayout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }

    std::vector<std::shared_ptr<Temporary>> temporaries;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> pools;
    std::vector<std::uint64_t> serials;
    std::size_t current = 0;
    std::uint32_t used = 0;
};

std::mutex& mergesMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::unique_ptr<StagedMerges>>& merges() {
    static std::vector<std::unique_ptr<StagedMerges>> list;
    return list;
}

StagedMerges& mergesFor(const Context& context) {
    for (const auto& entry : merges()) {
        if (entry->context.device == context.device) return *entry;
    }
    merges().push_back(std::make_unique<StagedMerges>(context));
    return *merges().back();
}

}

std::shared_ptr<StagedTemporary> AcquireStagedTemporary(const Context& context, VkExtent2D extent, std::uint32_t samples) {
    std::lock_guard lock(mergesMutex());
    return mergesFor(context).Acquire(extent, samples);
}

void RecordStagedClear(const Context& context, VkCommandBuffer commands, StagedTemporary& temporary) {
    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.srcAccessMask = anyAccess;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = temporary.Image();
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
    VkClearColorValue unwritten{};
    unwritten.uint32[0] = 0xffffffffu;
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Resolved(&DeviceFunctions::cmdClearColorImage, "vkCmdClearColorImage")(commands, temporary.Image(), VK_IMAGE_LAYOUT_GENERAL, &unwritten, 1, &range);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    if (Recorder::Active() != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
}

void RecordStagedMerge(const Context& context, VkCommandBuffer commands, StagedTemporary& temporary, VkImageView target, std::uint32_t mask, std::uint32_t samples, VkRect2D area) {
    if (area.extent.width == 0 || area.extent.height == 0) return;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, anyWrite, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    {
        std::lock_guard lock(mergesMutex());
        mergesFor(context).Merge(commands, temporary, target, mask, samples, area);
    }
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, anyAccess);
    if (Recorder::Active() != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
}

void ClearStagedMerges(VkDevice device) {
    std::lock_guard lock(mergesMutex());
    std::erase_if(merges(), [&](const auto& entry) { return entry->context.device == device; });
}

}
