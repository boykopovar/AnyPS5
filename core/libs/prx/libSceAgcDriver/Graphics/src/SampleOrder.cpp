#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderDepth_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderR16_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderR32_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderR8_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderRG32_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderRGBA32_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderStencilBits_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderStencilExport_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleOrderVertex_spv.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdio>
#include <limits>
#include <map>
#include <mutex>
#include <tuple>
#include <utility>

namespace AgcDriver::Graphics {
namespace {

constexpr VkAccessFlags anyWrite = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
constexpr VkAccessFlags anyAccess = anyWrite | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
constexpr VkPipelineStageFlags reorderStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
constexpr std::uint32_t setsPerPool = 64;

struct StoragePush {
    std::array<std::uint32_t, 4> map;
    std::array<std::uint32_t, 2> extent;
    std::uint32_t count;
};

struct DepthPush {
    std::array<std::uint32_t, 4> map;
    std::uint32_t bit;
};

bool exactDepth(VkFormat format) {
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

bool stencilOf(const Context& context, const DepthTarget& depth) {
    return depth.stencilAddress != 0 && HasStencil(HostDepthFormat(context, depth.format));
}

bool depthSampleable(const Context& context, VkFormat format, std::uint32_t samples) {
    if (context.imageFormatProperties == nullptr || context.formatProperties == nullptr) return false;
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0) return false;
    VkImageFormatProperties supported{};
    const VkImageCreateFlags flags = SampleLocationsCompatibleDepth(context, format, samples) ? VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT : 0u;
    constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, flags, &supported) != VK_SUCCESS) return false;
    return (supported.sampleCounts & samples) != 0;
}

std::array<std::uint32_t, 4> mapFor(const SampleOrder& order, bool toHost) {
    std::array<std::uint32_t, 4> map{};
    for (std::size_t pixel = 0; pixel < map.size(); ++pixel) {
        const auto word = order[pixel] == 0u ? KeptSampleOrder : order[pixel];
        map[pixel] = toHost ? word : InvertSampleOrder(word);
    }
    return map;
}

VkExtent2D regionOf(const SampleOrderPass& pass, VkExtent2D extent) {
    return {std::min(pass.extent.width, extent.width), std::min(pass.extent.height, extent.height)};
}

void transitionLocated(const Context& context, VkCommandBuffer commands, VkImage image, VkFormat format, const VkSampleLocationsInfoEXT& locations, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags sourceStages, VkAccessFlags sourceAccess, VkPipelineStageFlags destinationStages, VkAccessFlags destinationAccess) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, &locations};
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | (HasStencil(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u), 0, 1, 0, 1};
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, sourceStages, destinationStages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

class CopyImage {
public:
    CopyImage(const Context& context, VkFormat format, std::uint32_t samples, VkExtent2D extent, bool stencil) : context(context), format(format), samples(samples), extent(extent) {
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = {extent.width, extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = static_cast<VkSampleCountFlagBits>(samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage sample order copy");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory sample order copy");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory sample order copy");
            depthView = createView(VK_IMAGE_ASPECT_DEPTH_BIT);
            if (stencil) stencilView = createView(VK_IMAGE_ASPECT_STENCIL_BIT);
        } catch (...) {
            release();
            throw;
        }
    }
    ~CopyImage() { release(); }
    CopyImage(const CopyImage&) = delete;
    CopyImage& operator=(const CopyImage&) = delete;

    Context context;
    VkFormat format;
    std::uint32_t samples;
    VkExtent2D extent;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkImageView stencilView = VK_NULL_HANDLE;
    bool general = false;

private:
    VkImageView createView(VkImageAspectFlags aspect) {
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = format;
        info.subresourceRange = {aspect, 0, 1, 0, 1};
        VkImageView view = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &view), "vkCreateImageView sample order copy");
        return view;
    }

    void release() noexcept {
        if (stencilView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, stencilView, nullptr);
        if (depthView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, depthView, nullptr);
        if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
    }
};

class SampleOrders {
public:
    explicit SampleOrders(const Context& context) : context(context) {
        this->context.bufferPool.reset();
    }
    ~SampleOrders() { release(); }
    SampleOrders(const SampleOrders&) = delete;
    SampleOrders& operator=(const SampleOrders&) = delete;

    void Storage(VkCommandBuffer commands, const SampleOrderTarget& target, const std::array<std::uint32_t, 4>& map, VkExtent2D region) {
        const auto pipeline = storagePipeline(target.format);
        const auto set = allocate(storageSetLayout, {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, setsPerPool}});
        const VkDescriptorImageInfo imageInfo{VK_NULL_HANDLE, target.view, VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo = &imageInfo;
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
        const StoragePush push{map, {region.width, region.height}, target.samples};
        context.Resolved(&DeviceFunctions::cmdBindPipeline, "vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, storagePipelineLayout, 0, 1, &set, 0, nullptr);
        context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, storagePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
        context.Resolved(&DeviceFunctions::cmdDispatch, "vkCmdDispatch")(commands, ((region.width + 1u) / 2u + 7u) / 8u, ((region.height + 1u) / 2u + 7u) / 8u, 1);
    }

    void Depth(VkCommandBuffer commands, const SampleOrderTarget& target, const std::array<std::uint32_t, 4>& map, VkExtent2D region, const SampleOrderPass& order, std::vector<std::shared_ptr<void>>& kept) {
        const bool stencil = target.method != SampleOrderMethod::Depth;
        auto copy = copyFor(target.format, target.samples, target.extent, stencil);
        kept.push_back(copy);
        const VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT | (stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
        const VkImageAspectFlags layoutAspects = VK_IMAGE_ASPECT_DEPTH_BIT | (HasStencil(target.format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
        if (!copy->general) {
            VkImageMemoryBarrier toGeneral{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toGeneral.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.image = copy->image;
            toGeneral.subresourceRange = {layoutAspects, 0, 1, 0, 1};
            context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
            copy->general = true;
        }
        VkImageCopy region3d{};
        region3d.srcSubresource = {aspects, 0, 0, 1};
        region3d.dstSubresource = {aspects, 0, 0, 1};
        region3d.extent = {region.width, region.height, 1};
        const auto& locations = order.locations;
        const bool located = order.located;
        const VkSampleLocationsInfoEXT info{VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, nullptr, static_cast<VkSampleCountFlagBits>(target.samples), {1, 1}, target.samples, locations.data()};
        constexpr VkPipelineStageFlags fragmentStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        constexpr VkAccessFlags depthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (located) transitionLocated(context, commands, target.image, target.format, info, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, anyWrite, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, target.image, located ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL, copy->image, VK_IMAGE_LAYOUT_GENERAL, 1, &region3d);
        if (located) transitionLocated(context, commands, target.image, target.format, info, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, fragmentStages, depthAccess);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, fragmentStages, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | depthAccess);
        const auto& pass = depthPass(target.format, target.samples, target.method, located, locations);
        const auto set = allocate(sampledSetLayout, {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, setsPerPool * 2u}});
        std::array<VkDescriptorImageInfo, 2> images{{{VK_NULL_HANDLE, copy->depthView, VK_IMAGE_LAYOUT_GENERAL}, {VK_NULL_HANDLE, stencil ? copy->stencilView : copy->depthView, VK_IMAGE_LAYOUT_GENERAL}}};
        std::array<VkWriteDescriptorSet, 2> writes{};
        for (std::uint32_t binding = 0; binding < writes.size(); ++binding) {
            writes[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[binding].dstSet = set;
            writes[binding].dstBinding = binding;
            writes[binding].descriptorCount = 1;
            writes[binding].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[binding].pImageInfo = &images[binding];
        }
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, stencil ? 2u : 1u, writes.data(), 0, nullptr);
        const std::array<VkImageView, 1> views{target.view};
        auto framebuffer = std::make_shared<Framebuffer>(context, pass.renderPass, views, region);
        kept.push_back(framebuffer);
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = pass.renderPass;
        begin.framebuffer = framebuffer->Handle();
        begin.renderArea = {{0, 0}, region};
        const VkAttachmentSampleLocationsEXT initial{0, info};
        const VkSubpassSampleLocationsEXT post{0, info};
        VkRenderPassSampleLocationsBeginInfoEXT sampleLocations{VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT};
        sampleLocations.attachmentInitialSampleLocationsCount = 1;
        sampleLocations.pAttachmentInitialSampleLocations = &initial;
        sampleLocations.postSubpassSampleLocationsCount = 1;
        sampleLocations.pPostSubpassSampleLocations = &post;
        if (located) begin.pNext = &sampleLocations;
        context.Resolved(&DeviceFunctions::cmdBeginRenderPass, "vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport{0.0f, 0.0f, static_cast<float>(region.width), static_cast<float>(region.height), 0.0f, 1.0f};
        const VkRect2D scissor{{0, 0}, region};
        context.Resolved(&DeviceFunctions::cmdSetViewport, "vkCmdSetViewport")(commands, 0, 1, &viewport);
        context.Resolved(&DeviceFunctions::cmdSetScissor, "vkCmdSetScissor")(commands, 0, 1, &scissor);
        context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, sampledPipelineLayout, 0, 1, &set, 0, nullptr);
        const auto draw = [&](VkPipeline pipeline, std::uint32_t bit, std::uint32_t writeMask, std::uint32_t reference) {
            context.Resolved(&DeviceFunctions::cmdBindPipeline, "vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            context.Resolved(&DeviceFunctions::cmdSetStencilWriteMask, "vkCmdSetStencilWriteMask")(commands, VK_STENCIL_FACE_FRONT_AND_BACK, writeMask);
            context.Resolved(&DeviceFunctions::cmdSetStencilReference, "vkCmdSetStencilReference")(commands, VK_STENCIL_FACE_FRONT_AND_BACK, reference);
            const DepthPush push{map, bit};
            context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, sampledPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
            context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(commands, 3, 1, 0, 0);
        };
        draw(pass.depth, 0u, 0xffu, 0u);
        if (target.method == SampleOrderMethod::DepthStencilBits) {
            for (std::uint32_t bit = 0; bit < 8u; ++bit) draw(pass.stencilBits, bit, 1u << bit, 0xffu);
        }
        context.Resolved(&DeviceFunctions::cmdEndRenderPass, "vkCmdEndRenderPass")(commands);
    }

    Context context;

private:
    struct Ring {
        std::vector<VkDescriptorPool> pools;
        std::vector<std::uint64_t> serials;
        std::size_t current = 0;
        std::uint32_t used = 0;
    };

    struct DepthPass {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkPipeline depth = VK_NULL_HANDLE;
        VkPipeline stencilBits = VK_NULL_HANDLE;
    };

    using DepthKey = std::tuple<VkFormat, std::uint32_t, SampleOrderMethod, bool, std::array<std::uint32_t, 16>>;
    using CopyKey = std::tuple<VkFormat, std::uint32_t, std::uint32_t, std::uint32_t, bool>;

    VkShaderModule module(const std::uint32_t* code, std::size_t bytes) {
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = bytes;
        info.pCode = code;
        VkShaderModule created = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &info, nullptr, &created), "vkCreateShaderModule sample order");
        return created;
    }

    void destroyModule(VkShaderModule shader) {
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, shader, nullptr);
    }

    VkDescriptorSetLayout setLayout(VkDescriptorType type, std::uint32_t count, VkShaderStageFlags stage) {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        for (std::uint32_t binding = 0; binding < count; ++binding) bindings[binding] = {binding, type, 1, stage, nullptr};
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = count;
        info.pBindings = bindings.data();
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &info, nullptr, &layout), "vkCreateDescriptorSetLayout sample order");
        return layout;
    }

    VkPipelineLayout pipelineLayout(VkDescriptorSetLayout set, VkShaderStageFlags stage, std::uint32_t pushBytes) {
        const VkPushConstantRange range{stage, 0, pushBytes};
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        info.setLayoutCount = 1;
        info.pSetLayouts = &set;
        info.pushConstantRangeCount = 1;
        info.pPushConstantRanges = &range;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &info, nullptr, &layout), "vkCreatePipelineLayout sample order");
        return layout;
    }

    VkPipeline storagePipeline(VkFormat format) {
        const std::array<VkFormat, 5> formats{VK_FORMAT_R8_UINT, VK_FORMAT_R16_UINT, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32A32_UINT};
        const auto found = std::ranges::find(formats, format);
        Require(found != formats.end(), "reordering the samples of a color target through a storage view of this format is not implemented");
        const auto index = static_cast<std::size_t>(found - formats.begin());
        if (storagePipelines[index] != VK_NULL_HANDLE) return storagePipelines[index];
        if (storageSetLayout == VK_NULL_HANDLE) storageSetLayout = setLayout(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
        if (storagePipelineLayout == VK_NULL_HANDLE) storagePipelineLayout = pipelineLayout(storageSetLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(StoragePush));
        const std::array<std::pair<const std::uint32_t*, std::size_t>, 5> code{{{SAMPLE_ORDER_R8_SPV, sizeof(SAMPLE_ORDER_R8_SPV)}, {SAMPLE_ORDER_R16_SPV, sizeof(SAMPLE_ORDER_R16_SPV)}, {SAMPLE_ORDER_R32_SPV, sizeof(SAMPLE_ORDER_R32_SPV)}, {SAMPLE_ORDER_RG32_SPV, sizeof(SAMPLE_ORDER_RG32_SPV)}, {SAMPLE_ORDER_RGBA32_SPV, sizeof(SAMPLE_ORDER_RGBA32_SPV)}}};
        const auto shader = module(code[index].first, code[index].second);
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        info.stage.module = shader;
        info.stage.pName = "main";
        info.layout = storagePipelineLayout;
        VkResult created;
        {
            auto pipelineCacheLock = context.LockPipelineCacheForUse();
            created = context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &info, nullptr, &storagePipelines[index]);
        }
        destroyModule(shader);
        Check(created, "vkCreateComputePipelines sample order");
        return storagePipelines[index];
    }

    const DepthPass& depthPass(VkFormat format, std::uint32_t samples, SampleOrderMethod method, bool located, const std::array<VkSampleLocationEXT, 32>& locations) {
        std::array<std::uint32_t, 16> standIn{};
        if (located) {
            for (std::uint32_t sample = 0; sample < samples && sample < 8u; ++sample) {
                standIn[sample * 2u] = std::bit_cast<std::uint32_t>(locations[sample].x);
                standIn[sample * 2u + 1u] = std::bit_cast<std::uint32_t>(locations[sample].y);
            }
        }
        const DepthKey key{format, samples, method, located, standIn};
        if (const auto found = depthPasses.find(key); found != depthPasses.end()) return found->second;
        if (sampledSetLayout == VK_NULL_HANDLE) sampledSetLayout = setLayout(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2, VK_SHADER_STAGE_FRAGMENT_BIT);
        if (sampledPipelineLayout == VK_NULL_HANDLE) sampledPipelineLayout = pipelineLayout(sampledSetLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DepthPush));
        const bool stencil = method != SampleOrderMethod::Depth;
        DepthPass pass;
        try {
            VkAttachmentDescription attachment{};
            attachment.format = format;
            attachment.samples = static_cast<VkSampleCountFlagBits>(samples);
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = stencil ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = stencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
            if (HasStencil(format) && !stencil) {
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            }
            const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_GENERAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pDepthStencilAttachment = &reference;
            VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            passInfo.attachmentCount = 1;
            passInfo.pAttachments = &attachment;
            passInfo.subpassCount = 1;
            passInfo.pSubpasses = &subpass;
            Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &pass.renderPass), "vkCreateRenderPass sample order");
            const auto create = [&](const std::uint32_t* code, std::size_t bytes, bool writesDepth) {
                const auto vertex = module(SAMPLE_ORDER_VERTEX_SPV, sizeof(SAMPLE_ORDER_VERTEX_SPV));
                const auto fragment = module(code, bytes);
                std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
                stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertex, "main", nullptr};
                stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr};
                VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
                VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
                assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                VkPipelineViewportStateCreateInfo viewports{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
                viewports.viewportCount = 1;
                viewports.scissorCount = 1;
                VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
                raster.polygonMode = VK_POLYGON_MODE_FILL;
                raster.cullMode = VK_CULL_MODE_NONE;
                raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
                raster.lineWidth = 1.0f;
                VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
                multisample.rasterizationSamples = static_cast<VkSampleCountFlagBits>(samples);
                multisample.sampleShadingEnable = VK_TRUE;
                multisample.minSampleShading = 1.0f;
                VkPipelineSampleLocationsStateCreateInfoEXT sampleLocations{VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT};
                sampleLocations.sampleLocationsEnable = VK_TRUE;
                sampleLocations.sampleLocationsInfo = {VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, nullptr, static_cast<VkSampleCountFlagBits>(samples), {1, 1}, samples, locations.data()};
                if (located) multisample.pNext = &sampleLocations;
                VkStencilOpState replace{};
                replace.failOp = VK_STENCIL_OP_REPLACE;
                replace.passOp = VK_STENCIL_OP_REPLACE;
                replace.depthFailOp = VK_STENCIL_OP_REPLACE;
                replace.compareOp = VK_COMPARE_OP_ALWAYS;
                replace.compareMask = 0xffu;
                replace.writeMask = 0xffu;
                VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
                depthStencil.depthTestEnable = writesDepth ? VK_TRUE : VK_FALSE;
                depthStencil.depthWriteEnable = writesDepth ? VK_TRUE : VK_FALSE;
                depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
                depthStencil.stencilTestEnable = stencil ? VK_TRUE : VK_FALSE;
                depthStencil.front = replace;
                depthStencil.back = replace;
                const std::array<VkDynamicState, 4> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, VK_DYNAMIC_STATE_STENCIL_REFERENCE};
                VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
                dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
                dynamic.pDynamicStates = dynamicStates.data();
                VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
                info.stageCount = static_cast<std::uint32_t>(stages.size());
                info.pStages = stages.data();
                info.pVertexInputState = &input;
                info.pInputAssemblyState = &assembly;
                info.pViewportState = &viewports;
                info.pRasterizationState = &raster;
                info.pMultisampleState = &multisample;
                info.pDepthStencilState = &depthStencil;
                info.pDynamicState = &dynamic;
                info.layout = sampledPipelineLayout;
                info.renderPass = pass.renderPass;
                VkPipeline pipeline = VK_NULL_HANDLE;
                VkResult created;
                {
                    auto pipelineCacheLock = context.LockPipelineCacheForUse();
                    created = context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &info, nullptr, &pipeline);
                }
                destroyModule(fragment);
                destroyModule(vertex);
                Check(created, "vkCreateGraphicsPipelines sample order");
                return pipeline;
            };
            if (method == SampleOrderMethod::DepthStencilExport) pass.depth = create(SAMPLE_ORDER_STENCILEXPORT_SPV, sizeof(SAMPLE_ORDER_STENCILEXPORT_SPV), true);
            else pass.depth = create(SAMPLE_ORDER_DEPTH_SPV, sizeof(SAMPLE_ORDER_DEPTH_SPV), true);
            if (method == SampleOrderMethod::DepthStencilBits) pass.stencilBits = create(SAMPLE_ORDER_STENCILBITS_SPV, sizeof(SAMPLE_ORDER_STENCILBITS_SPV), false);
        } catch (...) {
            destroyPass(pass);
            throw;
        }
        return depthPasses.emplace(key, pass).first->second;
    }

    std::shared_ptr<CopyImage> copyFor(VkFormat format, std::uint32_t samples, VkExtent2D extent, bool stencil) {
        const CopyKey key{format, samples, extent.width, extent.height, stencil};
        if (const auto found = copies.find(key); found != copies.end()) {
            found->second.second = ++uses;
            return found->second.first;
        }
        constexpr std::size_t bound = 4;
        while (copies.size() >= bound) {
            const auto oldest = std::ranges::min_element(copies, [](const auto& a, const auto& b) { return a.second.second < b.second.second; });
            copies.erase(oldest);
        }
        auto copy = std::make_shared<CopyImage>(context, format, samples, extent, stencil);
        copies.emplace(key, std::pair{copy, ++uses});
        return copy;
    }

    VkDescriptorSet allocate(VkDescriptorSetLayout layout, const std::vector<VkDescriptorPoolSize>& sizes) {
        auto& ring = layout == storageSetLayout ? storageRing : sampledRing;
        const auto* recorder = Recorder::Active();
        if (ring.pools.empty() || ring.used == setsPerPool) {
            const auto completed = recorder != nullptr ? recorder->CompletedThrough() : 0;
            auto next = ring.pools.size();
            for (std::size_t step = 1; step <= ring.pools.size(); ++step) {
                const auto index = (ring.current + step) % ring.pools.size();
                if (ring.serials[index] <= completed) {
                    next = index;
                    break;
                }
            }
            if (next < ring.pools.size()) {
                Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, ring.pools[next], 0), "vkResetDescriptorPool sample order");
            } else {
                VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
                info.maxSets = setsPerPool;
                info.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
                info.pPoolSizes = sizes.data();
                VkDescriptorPool pool = VK_NULL_HANDLE;
                Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &info, nullptr, &pool), "vkCreateDescriptorPool sample order");
                ring.pools.push_back(pool);
                ring.serials.push_back(0);
            }
            ring.current = next;
            ring.used = 0;
        }
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = ring.pools[ring.current];
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &layout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets sample order");
        ++ring.used;
        ring.serials[ring.current] = recorder != nullptr && recorder->Device() == context.device ? recorder->OpenSerial() : std::numeric_limits<std::uint64_t>::max();
        return set;
    }

    void destroyPass(const DepthPass& pass) noexcept {
        if (pass.stencilBits) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pass.stencilBits, nullptr);
        if (pass.depth) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pass.depth, nullptr);
        if (pass.renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, pass.renderPass, nullptr);
    }

    void release() noexcept {
        copies.clear();
        for (const auto& [key, pass] : depthPasses) destroyPass(pass);
        depthPasses.clear();
        for (const auto pipeline : storagePipelines) {
            if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        }
        for (auto* ring : {&storageRing, &sampledRing}) {
            for (const auto pool : ring->pools) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
            ring->pools.clear();
        }
        for (const auto layout : {storagePipelineLayout, sampledPipelineLayout}) {
            if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        }
        for (const auto layout : {storageSetLayout, sampledSetLayout}) {
            if (layout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, layout, nullptr);
        }
    }

    VkDescriptorSetLayout storageSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout sampledSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout storagePipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout sampledPipelineLayout = VK_NULL_HANDLE;
    std::array<VkPipeline, 5> storagePipelines{};
    std::map<DepthKey, DepthPass> depthPasses;
    std::map<CopyKey, std::pair<std::shared_ptr<CopyImage>, std::uint64_t>> copies;
    std::uint64_t uses = 0;
    Ring storageRing;
    Ring sampledRing;
};

std::mutex& ordersMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::unique_ptr<SampleOrders>>& orders() {
    static std::vector<std::unique_ptr<SampleOrders>> list;
    return list;
}

SampleOrders& ordersFor(const Context& context) {
    std::lock_guard lock(ordersMutex());
    for (const auto& entry : orders()) {
        if (entry->context.device == context.device) return *entry;
    }
    orders().push_back(std::make_unique<SampleOrders>(context));
    return *orders().back();
}

}

SampleOrderMethod DepthSampleOrderMethod(const Context& context, const DepthTarget& depth) {
    if (!stencilOf(context, depth)) return SampleOrderMethod::Depth;
    return context.shaderStencilExport ? SampleOrderMethod::DepthStencilExport : SampleOrderMethod::DepthStencilBits;
}

bool HostsSampleLocationGrid(VkExtent2D largestGrid) {
    return largestGrid.width != 0u && largestGrid.height != 0u && largestGrid.width % 2u == 0u && largestGrid.height % 2u == 0u;
}

bool MayReorderSamples(const Context& context, std::uint32_t samples) {
    return samples > 1u && (context.sampleLocationSampleCounts & samples) != 0 && (context.sampleLocationGridSampleCounts & samples) == 0;
}

VkFormat SampleOrderStorageFormat(const Context& context, VkFormat format, std::uint32_t elementBytes, std::uint32_t samples) {
    if (!context.storageImageMultisample || context.imageFormatProperties == nullptr || samples <= 1u) return VK_FORMAT_UNDEFINED;
    const auto alias = TextureDetiler::ImageElementFormat(elementBytes);
    if (alias == VK_FORMAT_UNDEFINED) return VK_FORMAT_UNDEFINED;
    VkImageFormatProperties supported{};
    if (context.imageFormatProperties(context.physical, alias, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT, 0, &supported) != VK_SUCCESS || (supported.sampleCounts & samples) == 0) return VK_FORMAT_UNDEFINED;
    constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    constexpr VkImageCreateFlags flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
    if (context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, flags, &supported) != VK_SUCCESS || (supported.sampleCounts & samples) == 0) return VK_FORMAT_UNDEFINED;
    return alias;
}

std::string SampleOrderRefusal(const Context& context, const State& state) {
    if (!context.sampleRateShading) return "the device lacks sampleRateShading";
    if (!context.storageImageMultisample) return "the device lacks shaderStorageImageMultisample";
    for (const auto& color : state.colors) {
        if (color.samples > 1u && SampleOrderStorageFormat(context, color.format, color.elementBytes, color.samples) == VK_FORMAT_UNDEFINED) return "color format " + std::to_string(color.format) + " has no storage view at " + std::to_string(color.samples) + " samples";
    }
    if (state.depth && state.depth->samples > 1u) {
        const auto host = HostDepthFormat(context, state.depth->format);
        if (!exactDepth(host)) return "depth format " + std::to_string(host) + " is not written back exactly through FragDepth";
        if (!depthSampleable(context, host, state.depth->samples)) return "depth format " + std::to_string(host) + " cannot be sampled at " + std::to_string(state.depth->samples) + " samples";
    }
    return {};
}

HostSampleLocations HostSampleLocationsFor(const Context& context, const State& state) {
    if (!state.sampleLocationGrid || (context.sampleLocationGridSampleCounts & state.samples) != 0) return {SampleLocationGrid(state), {}};
    const auto order = SameSetSampleOrder(state);
    const auto refusal = order ? SampleOrderRefusal(context, state) : std::string("its pixels hold different locations");
    static std::atomic<std::uint32_t> reported{0};
    const auto bit = refusal.empty() ? state.samples : state.samples << 8u;
    if ((reported.fetch_or(bit, std::memory_order_relaxed) & bit) == 0) {
        if (refusal.empty()) std::fprintf(stderr, "[gpu] no 2x2 sample location grid at sample count %u on this device: pixel (0,0)'s sample locations stand in for every pixel, and each pixel's samples are put in the guest's order at the bounds of its render passes\n", state.samples);
        else std::fprintf(stderr, "[gpu] no 2x2 sample location grid at sample count %u on this device: sample locations varying between pixels take pixel (0,0)'s in every pixel (%s)\n", state.samples, refusal.c_str());
    }
    return {{1, 1}, refusal.empty() ? *order : SampleOrder{}};
}

std::vector<SampleOrderStep> PlanSampleOrder(const Context& context, const State& state, const SampleOrder& order, VkAttachmentLoadOp load, VkAttachmentStoreOp store) {
    std::vector<SampleOrderStep> steps;
    if (!ReordersSamples(order)) return steps;
    const bool toHost = load == VK_ATTACHMENT_LOAD_OP_LOAD;
    const bool toGuest = store == VK_ATTACHMENT_STORE_OP_STORE;
    if (!toHost && !toGuest) return steps;
    for (std::uint32_t index = 0; index < state.colors.size(); ++index) {
        if (state.colors[index].samples > 1u) steps.push_back({index, SampleOrderMethod::Storage, toHost, toGuest});
    }
    if (state.depth && state.depth->samples > 1u) steps.push_back({static_cast<std::uint32_t>(state.colors.size()), DepthSampleOrderMethod(context, *state.depth), toHost, toGuest});
    return steps;
}

SampleOrderPass SampleOrderPassFor(const Context& context, const State& state, const SampleOrder& order) {
    return {order, state.renderExtent, SampleLocations(state), ReordersSamples(order) && ValidateSampleLocations(context, state)};
}

void RecordSampleOrder(const Context& context, VkCommandBuffer commands, const SampleOrderPass& pass, std::span<const SampleOrderTarget> targets, bool toHost, std::vector<std::shared_ptr<void>>& kept) {
    if (targets.empty() || !ReordersSamples(pass.order)) return;
    auto& reorder = ordersFor(context);
    const auto map = mapFor(pass.order, toHost);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, reorderStages, anyWrite, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    for (const auto& target : targets) {
        Require(target.image != VK_NULL_HANDLE && target.view != VK_NULL_HANDLE, "a reordered attachment has no image");
        const auto region = regionOf(pass, target.extent);
        if (region.width == 0 || region.height == 0) continue;
        if (target.method == SampleOrderMethod::Storage) reorder.Storage(commands, target, map, region);
        else reorder.Depth(commands, target, map, region, pass, kept);
        if (target.keep != nullptr) kept.push_back(target.keep);
    }
    RecordMemoryBarrier(context, commands, reorderStages, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, anyAccess);
    if (Recorder::Active() != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
}

void RecordDepthSampleLocations(const Context& context, VkCommandBuffer commands, const DepthSampleLocations& depth) {
    Require(depth.image != VK_NULL_HANDLE && depth.samples * depth.grid.width * depth.grid.height <= depth.locations.size(), "a depth target drawn at custom sample locations has no image or too many locations");
    const VkSampleLocationsInfoEXT info{VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, nullptr, static_cast<VkSampleCountFlagBits>(depth.samples), depth.grid, depth.samples * depth.grid.width * depth.grid.height, depth.locations.data()};
    transitionLocated(context, commands, depth.image, depth.format, info, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, anyWrite, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    transitionLocated(context, commands, depth.image, depth.format, info, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, anyAccess);
}

std::array<std::span<const std::uint32_t>, 9> SampleOrderShaderModules() {
    return {SAMPLE_ORDER_R8_SPV, SAMPLE_ORDER_R16_SPV, SAMPLE_ORDER_R32_SPV, SAMPLE_ORDER_RG32_SPV, SAMPLE_ORDER_RGBA32_SPV, SAMPLE_ORDER_VERTEX_SPV, SAMPLE_ORDER_DEPTH_SPV, SAMPLE_ORDER_STENCILEXPORT_SPV, SAMPLE_ORDER_STENCILBITS_SPV};
}

void ClearSampleOrders(VkDevice device) {
    std::lock_guard lock(ordersMutex());
    std::erase_if(orders(), [&](const auto& entry) { return entry->context.device == device; });
}

}
