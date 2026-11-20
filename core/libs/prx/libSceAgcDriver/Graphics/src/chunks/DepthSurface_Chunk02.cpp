#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/PrimedSharedMutex.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/FirstSampleCopy.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuardSpans.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DepthToStorage_spv.h"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <shared_mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
DepthClearCounters& depthClearCounters() {
    static DepthClearCounters counters;
    return counters;
}

struct DepthPlaneStoreCounters {
    std::atomic<std::uint64_t> stores{0};
    std::atomic<std::uint64_t> units{0};
    std::atomic<std::uint64_t> bytes{0};
};

DepthPlaneStoreCounters& depthPlaneStoreCounters() {
    static DepthPlaneStoreCounters counters;
    return counters;
}

bool anyUnit(const std::vector<bool>& units) {
    return std::find(units.begin(), units.end(), true) != units.end();
}

thread_local DepthPlaneOverwrite* currentOverwrite = nullptr;

std::optional<DepthPlaneOverwrite> decodeOverwrite(const StorageImageClear& clear) {
    auto& counters = depthClearCounters();
    ++counters.matched;
    const auto decline = [&](DepthClearDecline why) -> std::optional<DepthPlaneOverwrite> {
        ++counters.declined[why];
        return std::nullopt;
    };
    DepthPlaneOverwrite overwrite;
    overwrite.image = clear.image;
    try {
        overwrite.view = DecodeTextureResource(clear.image);
    } catch (...) {
        return decline(DeclineFormat);
    }
    const auto& view = overwrite.view;
    const bool oneLayer = view.dimension == TextureDimension::k2D || (view.dimension == TextureDimension::k2DArray && view.baseArray == 0 && view.depthOrLastArray == 0);
    if (!oneLayer || view.baseLevel != 0) return decline(DeclineLayers);
    VkFormat format = VK_FORMAT_UNDEFINED;
    try {
        format = ResolveTextureFormat(view.format);
    } catch (...) {
        return decline(DeclineFormat);
    }
    enum class Kind { Unorm16, Uint16, Float32, Uint32 };
    Kind kind;
    switch (format) {
        case VK_FORMAT_R16_UNORM: kind = Kind::Unorm16; break;
        case VK_FORMAT_R16_UINT: kind = Kind::Uint16; break;
        case VK_FORMAT_R32_SFLOAT: kind = Kind::Float32; break;
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT: kind = Kind::Uint32; break;
        default: return decline(DeclineFormat);
    }
    const std::uint64_t base = clear.color[0] | (static_cast<std::uint64_t>(clear.color[1] & 0xffffu) << 32u);
    const std::uint64_t stride = (clear.color[1] >> 16u) & 0x3fffu;
    const std::uint64_t extent = stride == 0 ? clear.color[2] : static_cast<std::uint64_t>(clear.color[2]) * stride;
    if (base == 0 || (base & 3u) != 0u || extent < 16u || !GuestMemory::Accessible(reinterpret_cast<const void*>(base), 16)) return decline(DeclineColor);
    if (const auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(base, 16)) return decline(DeclinePending);
    const std::pair<std::uint64_t, std::uint64_t> colorRange{base, base + 16u};
    if (StorageTexture::AnyPendingOverlaps(std::span(&colorRange, 1))) return decline(DeclinePending);
    std::uint32_t c0 = 0;
    try {
        GuestMemory::Read(base, std::as_writable_bytes(std::span(&c0, 1)), 4);
    } catch (...) {
        return decline(DeclineColor);
    }
    switch (kind) {
        case Kind::Unorm16:
            if (c0 == 0u || c0 == 0x80000000u) overwrite.raw = 0u;
            else if (c0 == 0x3f800000u) overwrite.raw = 0xffffu;
            else return decline(DeclineValue);
            overwrite.bits = 16u;
            overwrite.depth = static_cast<float>(overwrite.raw) / 65535.0f;
            break;
        case Kind::Uint16:
            if (c0 != 0u && c0 != 0xffffu) return decline(DeclineValue);
            overwrite.raw = c0;
            overwrite.bits = 16u;
            overwrite.depth = static_cast<float>(overwrite.raw) / 65535.0f;
            break;
        case Kind::Float32:
        case Kind::Uint32: {
            const auto depth = std::bit_cast<float>(c0);
            if (c0 != 0u && !(std::isnormal(depth) && depth > 0.0f && depth <= 1.0f)) return decline(DeclineValue);
            overwrite.raw = c0;
            overwrite.bits = 32u;
            overwrite.depth = depth;
            break;
        }
    }
    overwrite.colorAddress = base;
    overwrite.colorBits = c0;
    overwrite.width = clear.width;
    overwrite.height = clear.height;
    return overwrite;
}

bool ProbeStencilShadowCopy(const Context& context, VkFormat format, std::uint32_t samples) {
    if (context.detiler == nullptr) return false;
    if (auto* recorder = RecorderFor(context)) recorder->Sync();
    bool works = true;
    VkImageView view = VK_NULL_HANDLE;
    VkImageView sourceView = VK_NULL_HANDLE;
    VkRenderPass patternPass = VK_NULL_HANDLE;
    VkFramebuffer patternFb = VK_NULL_HANDLE;
    std::shared_ptr<OwnedImage> source;
    std::shared_ptr<OwnedImage> shadow;
    try {
        constexpr VkExtent3D extent{129u, 67u, 1u};
        const auto make = [&](VkImageUsageFlags usage, VkImageCreateFlags flags = 0) {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.flags = flags;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = extent;
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = static_cast<VkSampleCountFlagBits>(samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImage image = VK_NULL_HANDLE;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage stencil shadow probe");
            auto owned = std::make_shared<OwnedImage>(context, image, VK_NULL_HANDLE);
            try {
                owned->Allocate(context, info, "vkAllocateMemory stencil shadow probe");
            } catch (...) {
                context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
                throw;
            }
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, owned->allocation.Memory(), owned->allocation.Offset()), "vkBindImageMemory stencil shadow probe");
            return owned;
        };
        source = make(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            SampleLocationsCompatibleDepth(context, format, samples) ? VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT : 0);
        shadow = make(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (HasStencil(format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0u));
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        const VkImageSubresourceRange aspects{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
        const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
        std::array<VkImageMemoryBarrier, 2> toGeneral{};
        for (std::size_t index = 0; index < toGeneral.size(); ++index) {
            auto& entry = toGeneral[index];
            entry.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            entry.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            entry.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            entry.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            entry.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            entry.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            entry.image = index == 0 ? source->image : shadow->image;
            entry.subresourceRange = aspects;
        }
        barrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(toGeneral.size()), toGeneral.data());
        const VkClearDepthStencilValue clear{0.25f, 0xa5u};
        const VkImageSubresourceRange depthOnly{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, source->image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &depthOnly);
        VkImageViewCreateInfo sourceViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        sourceViewInfo.image = source->image;
        sourceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        sourceViewInfo.format = format;
        sourceViewInfo.subresourceRange = aspects;
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &sourceViewInfo, nullptr, &sourceView), "vkCreateImageView stencil shadow probe attachment");
        const VkAttachmentDescription attachment{0u, format, static_cast<VkSampleCountFlagBits>(samples), VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL};
        const VkAttachmentReference reference{0u, VK_IMAGE_LAYOUT_GENERAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &reference;
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = 1u;
        passInfo.pAttachments = &attachment;
        passInfo.subpassCount = 1u;
        passInfo.pSubpasses = &subpass;
        Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &patternPass), "vkCreateRenderPass stencil shadow probe");
        VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = patternPass;
        fbInfo.attachmentCount = 1u;
        fbInfo.pAttachments = &sourceView;
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1u;
        Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &fbInfo, nullptr, &patternFb), "vkCreateFramebuffer stencil shadow probe");
        const auto attachmentAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, attachmentAccess);
        VkClearValue passClear{};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = patternPass;
        begin.framebuffer = patternFb;
        begin.renderArea = {{0, 0}, {extent.width, extent.height}};
        begin.clearValueCount = 1u;
        begin.pClearValues = &passClear;
        context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        constexpr std::uint32_t tile = 16u;
        for (std::uint32_t cy = 0; cy * tile < extent.height; ++cy) {
            for (std::uint32_t cx = 0; cx * tile < extent.width; ++cx) {
                VkClearAttachment pattern{};
                pattern.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
                pattern.clearValue.depthStencil.stencil = (cx * 37u + cy * 53u + 0x5au) & 0xffu;
                const VkClearRect rect{{{static_cast<std::int32_t>(cx * tile), static_cast<std::int32_t>(cy * tile)}, {std::min(tile, extent.width - cx * tile), std::min(tile, extent.height - cy * tile)}}, 0u, 1u};
                context.Function<PFN_vkCmdClearAttachments>("vkCmdClearAttachments")(commands, 1u, &pattern, 1u, &rect);
            }
        }
        context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkImageCopy region{};
        region.srcSubresource = {aspects.aspectMask, 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = extent;
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source->image, VK_IMAGE_LAYOUT_GENERAL, shadow->image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = shadow->image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.format = format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView stencil shadow probe");
        const std::uint32_t elements = extent.width * extent.height * samples;
        const auto bytes = (static_cast<std::size_t>(elements) + 3u) & ~std::size_t{3u};
        Buffer linear(context, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        context.detiler->DispatchMultisampleReadback(commands, VK_FORMAT_R8_UINT, linear.Handle(), 0u, bytes, view, extent.width, extent.height, 1u, samples, 0u, elements);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        batch.SubmitAndWait();
        linear.Invalidate();
        works = true;
        for (std::uint32_t element = 0; element < elements && works; ++element) {
            const auto pixel = element / samples;
            const auto x = pixel % extent.width;
            const auto y = (pixel / extent.width) % extent.height;
            works = linear.Bytes()[element] == static_cast<std::byte>(((x / tile) * 37u + (y / tile) * 53u + 0x5au) & 0xffu);
        }
    } catch (...) {
        works = false;
    }
    if (view != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    if (sourceView != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, sourceView, nullptr);
    if (patternFb != VK_NULL_HANDLE) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, patternFb, nullptr);
    if (patternPass != VK_NULL_HANDLE) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, patternPass, nullptr);
    return works;
}

bool StencilShadowCopyWorks(const Context& context, VkFormat format, std::uint32_t samples) {
    const std::pair key{format, samples};
    const auto cached = std::find_if(context.stencilShadowCopyProbes.begin(), context.stencilShadowCopyProbes.end(), [&](const auto& entry) { return entry.first == key; });
    if (cached != context.stencilShadowCopyProbes.end()) return cached->second;
    const bool works = ProbeStencilShadowCopy(context, format, samples);
    context.stencilShadowCopyProbes.emplace_back(key, works);
    return works;
}

class DepthSurface;
std::pair<std::uint64_t, std::uint64_t> staleStencilRange(const DepthSurface& surface);
void noteStalePlane(std::uint64_t begin, std::uint64_t end);
std::uint64_t nextPlaneSerial();

class DepthSurface {
public:
    DepthSurface(const Context& context, const DepthTarget& target, DepthSamplingState sampling = {}) : context(context), target(target), hostFormat(HostDepthFormat(context, target.format)), sampling(std::move(sampling)) {
        this->context.bufferPool.reset();
        VkFormatProperties properties{};
        context.formatProperties(context.physical, hostFormat, &properties);
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0, "depth/stencil format " + std::to_string(hostFormat) + " cannot be an attachment on this device");
        Require(target.samples != 1u || (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0, "depth/stencil format " + std::to_string(hostFormat) + " cannot be sampled on this device");
        Require(target.extent.width <= context.limits.maxFramebufferWidth && target.extent.height <= context.limits.maxFramebufferHeight, "depth target exceeds framebuffer limits");
        const VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT | (target.stencilAddress != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            if (SampleLocationsCompatibleDepth(context, hostFormat, target.samples)) info.flags = VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = hostFormat;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = static_cast<VkSampleCountFlagBits>(target.samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            sampleable = target.samples == 1;
            if (target.samples != 1) {
                Require(context.imageFormatProperties != nullptr, "native multisample depth requires image format queries");
                VkImageFormatProperties shadowProperties{};
                VkImageFormatProperties directProperties{};
                const auto sampledUsage = sampleShadowUsage();
                const auto shadowResult = context.imageFormatProperties(context.physical, hostFormat, info.imageType, info.tiling, sampledUsage, 0, &shadowProperties);
                if (shadowResult != VK_ERROR_FORMAT_NOT_SUPPORTED) Check(shadowResult, "vkGetPhysicalDeviceImageFormatProperties sampled depth shadow");
                const auto directResult = context.imageFormatProperties(context.physical, hostFormat, info.imageType, info.tiling, info.usage | VK_IMAGE_USAGE_SAMPLED_BIT, info.flags, &directProperties);
                if (directResult != VK_ERROR_FORMAT_NOT_SUPPORTED) Check(directResult, "vkGetPhysicalDeviceImageFormatProperties sampled depth attachment");
                const auto supports = [&](VkResult result, const VkImageFormatProperties& supported) {
                    return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 && result == VK_SUCCESS && (supported.sampleCounts & info.samples) != 0 && supported.maxMipLevels >= 1u && supported.maxArrayLayers >= 1u && supported.maxExtent.depth >= 1u && info.extent.width <= supported.maxExtent.width && info.extent.height <= supported.maxExtent.height;
                };
                useSampleShadow = supports(shadowResult, shadowProperties);
                const bool directSampleable = supports(directResult, directProperties);
                if (useSampleShadow && directSampleable && HasStencil(hostFormat)) useSampleShadow = StencilShadowCopyWorks(context, hostFormat, info.samples);
                sampleable = useSampleShadow || directSampleable;
                if (!useSampleShadow && directSampleable) {
                    info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
                    directSampled = true;
                }
            }
            if (target.samples == 1 && sampleable) info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage depth");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            memory = AllocateImageMemory(context, allocation, requirements.memoryTypeBits, "vkAllocateMemory depth target");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory depth");
            if (target.samples != 1u) owned = std::make_shared<OwnedImage>(this->context, image, memory);
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = hostFormat;
            viewInfo.subresourceRange = {aspects, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth");
            if (target.samples != 1u && sampleable && this->sampling.refusal.empty()) CheckCurrent();
            auto* recorder = RecorderFor(context);
            if (recorder != nullptr && owned != nullptr) recorder->Keep(owned);
            std::unique_ptr<CommandBatch> batch;
            if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
            const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
            VkImageMemoryBarrier toGeneral{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toGeneral.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.image = image;
            toGeneral.subresourceRange = viewInfo.subresourceRange;
            const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
            barrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
            const VkClearDepthStencilValue clear{this->sampling.initialDepth.value_or(target.clearDepth), this->sampling.initialStencil.value_or(this->sampling.aliasStencil.value_or(target.clearStencil))};
            context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &toGeneral.subresourceRange);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            std::vector<std::shared_ptr<void>> imported;
            if (this->sampling.importedSamples != nullptr) importSamples(commands, recorder, imported);
            if (batch) batch->SubmitAndWait();
            else {
                Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
                for (auto& object : imported) recorder->Keep(std::move(object));
            }
        } catch (...) {
            release();
            throw;
        }
        if (this->sampling.overwriteStencil) pendingOverwriteProofs().fetch_add(1u, std::memory_order_acq_rel);
    }

    ~DepthSurface() {
        if (sampling.overwriteStencil) pendingOverwriteProofs().fetch_sub(1u, std::memory_order_acq_rel);
        release();
    }
    DepthSurface(const DepthSurface&) = delete;
    DepthSurface& operator=(const DepthSurface&) = delete;

    void CheckCurrent() {
        if (target.samples == 1u) return;
        ProveOverwrite();
        const auto clearLease = clearGeneration == 0u && stencilClearGeneration == 0u ? GuestAllocations::Lease{} : GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
        Require(active->load(std::memory_order_acquire), "the native multisample depth owner was released");
        Require(sampling.refusal.empty(), sampling.refusal);
        Require(!clearPending, "native multisample depth clear has not been recorded");
        const auto* recorder = RecorderFor(context);
        if (recorder != nullptr && clearGeneration != 0u && clearPattern.has_value() && clearFillNote == 0u) clearFillNote = fillNoteAfter(*recorder, sampling.initial[1], clearFillFloor, *clearPattern);
        if (recorder != nullptr && stencilClearGeneration != 0u && stencilFillNote == 0u) stencilFillNote = fillNoteAfter(*recorder, sampling.initial[2], stencilFillFloor, *stencilClearValue * 0x01010101u);
        if (!publication.empty()) {
            if (auto* pending = RecorderFor(context)) {
                if (clearFillNote != 0u) pending->SyncThrough(sampling.initial[1].address, sampling.initial[1].bytes);
                if (stencilFillNote != 0u) pending->SyncThrough(sampling.initial[2].address, sampling.initial[2].bytes);
            }
        }
        const auto newestHtileNote = recorder != nullptr && sampling.initial.size() > 1u ? recorder->NewestStoreNote(sampling.initial[1].address, sampling.initial[1].bytes) : 0u;
        const bool queuedFill = clearPattern.has_value() && clearFillNote != 0u && newestHtileNote == clearFillNote;
        const auto newestStencilNote = recorder != nullptr && sampling.initial.size() > 2u ? recorder->NewestStoreNote(sampling.initial[2].address, sampling.initial[2].bytes) : 0u;
        const bool queuedStencilFill = stencilClearValue.has_value() && stencilFillNote != 0u && newestStencilNote == stencilFillNote;
        const bool separatePlane = target.stencilAddress != 0u && !target.htileStencil && target.htileDepthOnly && sampling.initial.size() > 2u;
        bool stencilPlaneAliased = false;
        for (std::size_t index = 0u; index < sampling.initial.size(); ++index) {
            const auto& range = sampling.initial[index];
            Require(GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) && GuestMemory::Watched(range.address, range.bytes), "native multisample depth requires watched accessible guest memory");
            if (recorder != nullptr && !(index == 1u && queuedFill) && !(index == 2u && queuedStencilFill) && recorder->PendingWriteOverlaps(range.address, range.bytes)) {
                Require(!recorder->PendingStoreOverlaps(range.address, range.bytes), "external GPU writes over native multisample depth are not implemented");
                const auto settled = GuestMemory::TrackerGeneration();
                RecorderFor(context)->SyncThrough(range.address, range.bytes);
                Require(!GuestMemory::WrittenSince(range.address, range.bytes, settled), "external GPU writes over native multisample depth are not implemented");
            }
            GuestMemory::CollectWritesUncached(range.address, range.bytes);
            const auto alias = MultisampleColorTargetOver(range.address, range.bytes);
            if (!alias || (index == 2u && sampling.unprovenStencil && stencilClearGeneration == 0u)) continue;
            if (index == 0u && sampling.depthAlias && sameDepthAlias(*sampling.depthAlias, range)) continue;
            const bool uniform = uniformPlaneTarget(target);
            std::optional<float> depthValue;
            std::optional<std::uint8_t> stencilValue;
            if (index == 0u) depthValue = aliasedDepth(*alias, range, target.samples);
            else if (index == 2u && separatePlane) stencilValue = aliasedStencil(*alias, range, target.samples);
            bool kept = false;
            if (depthValue) kept = !sampling.importedPerSample && std::bit_cast<std::uint32_t>(*depthValue) == std::bit_cast<std::uint32_t>(sampling.initialDepth.value_or(target.clearDepth));
            else if (stencilValue) kept = stencilClearGeneration != 0u ? *stencilValue == *stencilClearValue : *stencilValue == sampling.aliasStencil;
            const bool stencilRange = index == 2u || (index == 1u && uniform);
            if (!kept) reportAlias(*alias, range, stencilRange ? "stencil plane" : index == 1u ? "HTILE" : "depth");
            Require(kept, stencilRange ? stencilAliasRefusal : depthAliasRefusal);
            if (index == 2u && stencilClearGeneration != 0u) sampling.stencilAlias = *alias;
            if (stencilValue) stencilPlaneAliased = true;
        }
        if (stencilClearGeneration != 0u) {
            auto& plane = sampling.initial[2];
            const bool gpuFill = stencilFillNote != 0u && (queuedStencilFill || newestStencilNote == 0u);
            Require(GuestMemory::UnchangedSinceCollected(plane.address, plane.bytes, stencilClearGeneration), "CPU writes raced native multisample stencil plane fill");
            if (stencilPlaneAliased) sampling.aliasStencil = *stencilClearValue;
            else Require(gpuFill || stencilStored || GuestMemory::StoredAllSince_nid_no_patch(plane.address, plane.bytes, stencilClearGeneration), "native multisample stencil plane fill has no complete tracked store");
            if (!queuedStencilFill && !stencilPlaneAliased) {
                const auto* bytes = reinterpret_cast<const std::uint8_t*>(plane.address);
                Require(std::all_of(bytes, bytes + plane.bytes, [&](std::uint8_t value) { return value == *stencilClearValue; }), "native multisample stencil plane does not hold its fill value");
            }
            const auto generation = GuestMemory::CollectWritesUncached(plane.address, plane.bytes);
            Require(GuestMemory::UnchangedSinceCollected(plane.address, plane.bytes, stencilClearGeneration), "CPU writes raced native multisample stencil plane fill");
            stencilFill = *stencilClearValue;
            RecordClear(VK_IMAGE_ASPECT_STENCIL_BIT);
            plane.generation = generation;
            RebasePublication(2u);
            stencilClearGeneration = 0u;
            sampling.unprovenStencil.reset();
        }
        if (clearGeneration != 0u) {
            const auto& metadata = sampling.initial[1];
            const bool gpuFill = clearPattern.has_value() && clearFillNote != 0u && (queuedFill || newestHtileNote == 0u);
            for (std::size_t index = 0u; index < sampling.initial.size(); ++index) {
                const auto& plane = sampling.initial[index];
                if (index != 1u) Require(GuestMemory::UnchangedSince(plane.address, plane.bytes, plane.generation), "guest writes over native multisample depth are not implemented");
            }
            Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, clearGeneration), "CPU writes raced native multisample depth clear");
            Require(gpuFill || clearStored || GuestMemory::StoredAllSince_nid_no_patch(metadata.address, metadata.bytes, clearGeneration), "native multisample depth clear has no complete tracked HTILE store");
            Require(clearDepth == 0.0f || clearDepth == 1.0f, "native multisample depth clear value is not implemented");
            auto current = sampling.initial;
            current[1].generation = GuestMemory::CollectWritesUncached(metadata.address, metadata.bytes);
            std::vector<std::uint32_t> words(queuedFill ? 1u : metadata.bytes / sizeof(std::uint32_t), queuedFill ? *clearPattern : 0u);
            if (!queuedFill) std::memcpy(words.data(), reinterpret_cast<const void*>(metadata.address), metadata.bytes);
            for (const auto word : words) {
                Require(htileProvesClear(word, target.htileDepthOnly, clearDepth), "native multisample depth completed HTILE does not prove its clear value");
                Require((clearAspects & VK_IMAGE_ASPECT_STENCIL_BIT) == 0u || htileProvesStencilClear(word), "native multisample stencil completed HTILE does not prove its clear value");
            }
            for (const auto& range : current) GuestMemory::CollectWritesUncached(range.address, range.bytes);
            Require(GuestMemory::UnchangedSinceAll(current), "guest writes raced native multisample depth clear");
            Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, clearGeneration), "CPU writes raced native multisample depth clear");
            RecordClear(clearAspects);
            sampling.initial = current;
            RebasePublication(1u);
            clearGeneration = 0u;
            clearAspects = 0u;
            pendingClear = 0u;
        }
        Require(GuestMemory::UnchangedSinceAll(sampling.initial), "guest writes over native multisample depth are not implemented");
    }

    void CheckResource(const GuestTextureResource& resource) const {
        if (target.samples == 1u) return;
        const bool stencil = target.stencilAddress != 0u && resource.baseAddress == target.stencilAddress;
        const bool identity = stencil ? resource.format == 5u : resource.baseAddress == target.address && resource.format == (target.format == VK_FORMAT_D16_UNORM ? 7u : 22u);
        Require(identity && resource.width == target.extent.width && resource.height == target.extent.height && resource.samples == target.samples, "native multisample depth sampled identity or format mismatch");
        Require(resource.dimension == TextureDimension::k2DMsaa && resource.tileMode == TextureTileMode::kZ64KBX && resource.depthOrLastArray == 0u && resource.baseArray == 0u && resource.baseLevel == 0u && resource.lastLevel == 0u && resource.mipCount == 1u && resource.allocatedMipCount == 1u && resource.minLod == 0u, "native multisample depth sampled array, mip or layout is not implemented");
        if (uniformPlaneTarget(target) || bareDepthTarget(target)) {
            Require(resource.pipeBankXor == target.pipeBankXor && resource.dccAddress == 0u && !resource.dccAlphaOnMsb, "native multisample depth sampled metadata or XOR mismatch");
            return;
        }
        const bool htile = resource.dccAddress == target.htileAddress && resource.dccPipeAligned == target.htilePipeAligned;
        const bool bare = resource.dccAddress == 0u && !resource.dccPipeAligned;
        if (stencil) {
            Require(resource.pipeBankXor == target.pipeBankXor && bare && !resource.dccAlphaOnMsb && !target.htileStencil && target.htileDepthOnly, "native multisample depth sampled metadata or XOR mismatch");
            return;
        }
        const bool layout = resource.pipeBankXor == target.pipeBankXor && resource.iterate256 == target.iterate256 && !resource.dccAlphaOnMsb;
        if (layout && bare && !htile && !expanded) {
            static std::atomic<int> reports{0};
            if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] native multisample depth 0x%llx sampled through a descriptor without metadata before an in-place expand (last DB_RENDER_CONTROL 0x%08x)\n", static_cast<unsigned long long>(target.address), renderControl);
        }
        Require(layout && (htile || (bare && expanded)), "native multisample depth sampled metadata or XOR mismatch");
    }

    void RequireStencilProven() const {
        if (sampling.unprovenStencil) {
            const auto& plane = sampling.initial[2];
            reportAlias(MultisampleColorTargetOver(plane.address, plane.bytes).value_or(*sampling.unprovenStencil), plane, "unproven stencil plane");
        }
        Require(!sampling.unprovenStencil, stencilAliasRefusal);
    }

    void ApplyFastClear() {
        if (samplingActive) {
            CheckCurrent();
            Require(pendingClear == 0u, "native multisample depth fast-clear transition is not implemented");
        }
        const auto aspects = pendingClear & (VK_IMAGE_ASPECT_DEPTH_BIT | (target.stencilAddress != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u));
        pendingClear = 0;
        if (aspects == 0) return;
        RecordClear(aspects);
    }

    void RecordClear(VkImageAspectFlags aspects) {
        const float depth = depthFill.value_or(clearDepth);
        const std::uint8_t stencil = stencilFill.value_or(clearStencil);
        depthFill.reset();
        stencilFill.reset();
        RecordClearValue(aspects, depth, stencil);
    }

    void RecordClearValue(VkImageAspectFlags aspects, float depth, std::uint8_t stencil) {
        written = ++depthWrites;
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        constexpr VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (aspects == VK_IMAGE_ASPECT_STENCIL_BIT && directSampled) RecordStencilClearPass(commands, access, stencil);
        else {
            const VkImageSubresourceRange range{aspects, 0, 1, 0, 1};
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, access, VK_ACCESS_TRANSFER_WRITE_BIT);
            const VkClearDepthStencilValue clear{depth, stencil};
            context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, access);
        }
        if (batch) batch->SubmitAndWait();
        else {
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            recorder->Keep(owned);
        }
    }

    void RecordStencilClearPass(VkCommandBuffer commands, VkAccessFlags access, std::uint8_t stencil) {
        if (stencilClearPass == VK_NULL_HANDLE) {
            const VkAttachmentDescription attachment{0u, hostFormat, static_cast<VkSampleCountFlagBits>(target.samples), VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL};
            const VkAttachmentReference reference{0u, VK_IMAGE_LAYOUT_GENERAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pDepthStencilAttachment = &reference;
            VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            info.attachmentCount = 1u;
            info.pAttachments = &attachment;
            info.subpassCount = 1u;
            info.pSubpasses = &subpass;
            Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &info, nullptr, &stencilClearPass), "vkCreateRenderPass native depth stencil fill");
            VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebuffer.renderPass = stencilClearPass;
            framebuffer.attachmentCount = 1u;
            framebuffer.pAttachments = &view;
            framebuffer.width = target.extent.width;
            framebuffer.height = target.extent.height;
            framebuffer.layers = 1u;
            Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebuffer, nullptr, &stencilClearFb), "vkCreateFramebuffer native depth stencil fill");
        }
        const auto attachment = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, access, attachment);
        VkClearValue value{};
        value.depthStencil.stencil = stencil;
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = stencilClearPass;
        begin.framebuffer = stencilClearFb;
        begin.renderArea = {{0, 0}, target.extent};
        begin.clearValueCount = 1u;
        begin.pClearValues = &value;
        context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, access);
    }

    bool UnormStencilView(const GuestTextureResource& resource) const {
        return target.samples == 1 && target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress && ResolveTextureFormat(resource.format) == VK_FORMAT_R8_UNORM;
    }

    bool SampledAccepts(std::span<const std::uint32_t> words, const GuestTextureResource& resource) const {
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        const bool oneSlice = target.samples != 1 ? resource.dimension == TextureDimension::k2DMsaa : resource.dimension == TextureDimension::k2D || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray == 0);
        return (ResolveTextureFormat(resource.format) == expected || depthBits || UnormStencilView(resource)) && oneSlice && resource.width == target.extent.width && resource.height == target.extent.height && resource.baseLevel == 0 && resource.lastLevel == 0 && resource.baseArray == 0;
    }

    std::shared_ptr<Texture> Sampled(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, bool drawn = false, VkImageViewType multisampleViewType = VK_IMAGE_VIEW_TYPE_2D, bool copySource = false) {
        if (!copySource) CheckCurrent();
        CheckResource(resource);
        const bool compressionEnabled = words.size() > 6u && ((words[6] >> 21u) & 1u) != 0u;
        Require(!bareDepthTarget(target) || !compressionEnabled, "native multisample depth without HTILE sampled through a descriptor that enables HTILE compression is not implemented");
        std::array<std::uint32_t, 13> key{};
        std::copy_n(words.begin(), std::min<std::size_t>(words.size(), 8), key.begin());
        key[8] = components.r;
        key[9] = components.g;
        key[10] = components.b;
        key[11] = components.a;
        key[12] = static_cast<std::uint32_t>(multisampleViewType);
        const bool multisampled = target.samples != 1;
        Require(!multisampled || sampleable, "the device cannot sample the native depth target's format and sample count");
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const auto format = ResolveTextureFormat(resource.format);
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        if (multisampled) {
            Require(!depthBits && format == expected, "native multisampled depth sampling needs a numeric depth-plane format");
            Require(!stencil || (context.limits.sampledImageStencilSampleCounts & target.samples) != 0u, "the device cannot sample the native depth target's stencil at its sample count");
            Require(resource.samples == target.samples, "the sampled depth descriptor's sample count differs from its native target");
            Require(hostFormat == target.format, "sampling a native multisampled depth plane that the device stores in another depth format is not implemented");
        }
        if (stencil && UnormStencilView(resource) && SampledAccepts(words, resource)) {
            return unormStencil(key, components, resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, drawn);
        }
        if (!SampledAccepts(words, resource)) {
            char text[448];
            std::snprintf(text, sizeof(text), "AGC graphics: sampling the %s plane of depth surface 0x%llx (%ux%u, vk format %d) as a %ux%u texture of guest format %u (vk %d), tile mode %u, dimension %d, levels %u-%u, slice %u is not implemented (T# %08x %08x %08x %08x %08x %08x %08x %08x)",
                          stencil ? "stencil" : "depth", static_cast<unsigned long long>(target.address), target.extent.width, target.extent.height, static_cast<int>(target.format), resource.width, resource.height, resource.format, static_cast<int>(format),
                          static_cast<unsigned>(resource.tileMode), static_cast<int>(resource.dimension), resource.baseLevel, resource.lastLevel, resource.baseArray, key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
            throw std::runtime_error(text);
        }
        if (multisampled && useSampleShadow) {
            if (sampleShadow == nullptr) createSampleShadow();
            refreshSampleShadow(drawn, stencil);
        }
        if (const auto found = textures.find(key); found != textures.end()) return found->second;
        const auto viewType = multisampled ? multisampleViewType : resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        const auto sampledImage = useSampleShadow ? sampleShadow : owned;
        auto texture = std::make_shared<Texture>(context, useSampleShadow ? sampleShadow->image : image, hostFormat, stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, components, viewType, 0, sampledImage);
        if (multisampled) {
            Require(copySource || samplingActive, "native multisample depth sampling requires its creation-time owner");
            texture->MarkRefreshedPerUse();
        }
        textures.emplace(key, texture);
        return texture;
    }

    std::shared_ptr<Texture> FirstSample(const Texture& source, VkComponentMapping components) {
        auto* recorder = RecorderFor(context);
        Require(recorder != nullptr, "a native multisampled depth target is sampled only by recorded draws and dispatches");
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto format = d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        if (firstSample == nullptr) firstSample = std::make_shared<FirstSampleTextures>(context, format, target.extent);
        firstSample->copy.Record(*recorder, source.View(), source.Layout());
        const std::array<std::uint32_t, 4> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a)};
        auto& texture = firstSample->textures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, firstSample->copy.Image(), format, VK_IMAGE_ASPECT_COLOR_BIT, components);
            texture->MarkRefreshedPerUse();
        }
        return {firstSample, texture.get()};
    }

    void ForgetClear(VkImageAspectFlags aspects) {
        pendingClear &= ~aspects;
        if ((aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) depthFill.reset();
        if ((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) stencilFill.reset();
    }

    void TakeStencilBaseline() {
        const auto [begin, end] = staleStencilRange(*this);
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (GuestMemory::Watched(begin, bytes) || !GuestMemory::Accessible(reinterpret_cast<const void*>(begin), bytes)) {
            stencilBaseline.clear();
            return;
        }
        stencilBaseline.resize(bytes);
        GuestMemory::Read(begin, stencilBaseline);
    }

    void MarkStencilStale(bool written = false) {
        if (!StencilMirrorable()) return;
        const auto [begin, end] = staleStencilRange(*this);
        if (stencilBaseline.size() != static_cast<std::size_t>(end - begin) || (written && !GuestMemory::EqualsCommittedUnsynced(begin, stencilBaseline))) TakeStencilBaseline();
        stencilStale = true;
        stencilMark = nextPlaneSerial();
        noteStalePlane(begin, end);
    }

    void NoteHtileClear(VkImageAspectFlags aspects) {
        ForgetClear(aspects);
        pendingClear |= aspects;
        if ((aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0u) NoteDepthWork();
        if ((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0u) MarkStencilStale();
    }

    void NotePlaneFill(VkImageAspectFlagBits aspect, float depth, std::uint8_t stencil) {
        pendingClear |= aspect;
        if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
            depthFill = depth;
            NoteDepthWork();
        } else {
            stencilFill = stencil;
            MarkStencilStale(true);
        }
    }

    void CopyAspectFrom(const DepthSurface& source, VkImageAspectFlagBits aspect) {
        ForgetClear(aspect);
        NoteHostWrite();
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        constexpr VkAccessFlags all = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, all, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageCopy region{};
        region.srcSubresource = {static_cast<VkImageAspectFlags>(aspect), 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = {target.extent.width, target.extent.height, 1};
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source.image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, all);
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
    }

    bool Holds(const Texture* texture) const {
        const auto held = [&](const auto& entry) { return entry.second.get() == texture; };
        return std::any_of(textures.begin(), textures.end(), held) || std::any_of(stencilCopy.textures.begin(), stencilCopy.textures.end(), held);
    }

    bool SampledElsewhere() const {
        return std::any_of(textures.begin(), textures.end(), [](const auto& entry) { return entry.second.use_count() > 1; });
    }

    void NoteHostWrite() {
        if (samplingActive && !hostWritten->load(std::memory_order_acquire)) capturePublication();
        hostEverWritten->store(true, std::memory_order_release);
        hostWritten->store(true, std::memory_order_release);
    }

    void ProveOverwrite() {
        if (!sampling.overwriteStencil || !hostEverWritten->load(std::memory_order_acquire) || Recorder::InCompletion()) return;
        const auto value = *sampling.overwriteStencil;
        sampling.overwriteStencil.reset();
        pendingOverwriteProofs().fetch_sub(1u, std::memory_order_acq_rel);
        if (!sampleable || context.detiler == nullptr || !active->load(std::memory_order_acquire)) return;
        if (auto* recorder = RecorderFor(context)) recorder->Sync();
        for (const auto& range : sampling.initial) {
            StorageTexture::FlushPending(range.address, range.bytes);
            GuestMemory::FlushGpuWrites(range.address, range.bytes);
        }
        if (useSampleShadow) {
            if (sampleShadow == nullptr) createSampleShadow();
            refreshSampleShadow(true, true);
            if (auto* recorder = RecorderFor(context)) recorder->Sync();
        }
        const auto samples = static_cast<std::uint64_t>(target.extent.width) * target.extent.height * target.samples;
        const auto stencil = readBackSamples(VK_IMAGE_ASPECT_STENCIL_BIT, samples);
        const auto bytes = stencil->Bytes().first(static_cast<std::size_t>(samples));
        if (std::ranges::any_of(bytes, [&](std::byte stored) { return stored != static_cast<std::byte>(value); })) return;
        for (const auto& range : sampling.initial) GuestMemory::CollectWritesUncached(range.address, range.bytes);
        if (!GuestMemory::UnchangedSinceAll(sampling.initial)) return;
        sampling.refusal.clear();
        {
            std::unique_lock guardLock(depthGuardMutex());
            for (const auto& range : sampling.initial) depthGuards().push_back({context.device, range.address, range.bytes, active, {{range.address, range.bytes, 0u}}, hostWritten, hostEverWritten});
            publishDepthGuards(true);
        }
        InstallResidentImageReadGuard_nid_no_patch();
        samplingActive = true;
        capturePublication();
    }

    bool GuestHoldsSamples() const {
        return target.samples != 1u && uniformPlaneTarget(target) && samplingActive && sampling.refusal.empty() && !hostWritten->load(std::memory_order_acquire);
    }

    bool PublishedUnderStorage() const {
        return GuestHoldsSamples() && std::ranges::any_of(sampling.initial, [](const auto& range) {
            const std::pair<std::uint64_t, std::uint64_t> plane{range.address, range.address + range.bytes};
            return StorageTexture::AnyPendingUnitsOverlap(std::span(&plane, 1u));
        });
    }

    void PublishToGuest();

    std::uint64_t written = ++depthWrites;
    std::shared_ptr<std::atomic<bool>> hostWritten = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> hostEverWritten = std::make_shared<std::atomic<bool>>(false);
    bool retired = false;

    bool Overlaps(std::uint64_t address, std::uint64_t bytes) const {
        const auto end = address + bytes;
        if (address < target.address + depthBytes() && target.address < end) return true;
        return target.stencilAddress != 0 && address < target.stencilAddress + stencilBytes() && target.stencilAddress < end;
    }

    bool PlaneCoveredBy(bool stencilPlane, std::span<const std::pair<std::uint64_t, std::uint64_t>> written, std::span<const WrittenStorageExtent> extents) const {
        const auto reaches = [&](std::uint64_t address, std::uint32_t bytesPerTexel) {
            const auto [blockWidth, blockHeight] = DepthBlockExtent(bytesPerTexel);
            const std::array<std::uint32_t, 3> block{65536u, blockWidth, blockHeight};
            const auto blocksPerRow = (target.extent.width + blockWidth - 1u) / blockWidth;
            return std::any_of(extents.begin(), extents.end(), [&](const auto& extent) {
                return extent.address == address && extent.bytesPerElement == bytesPerTexel && extent.block == block && extent.blocksPerRow == blocksPerRow && extent.width >= target.extent.width && extent.height >= target.extent.height;
            });
        };
        const auto covers = [&](std::uint64_t begin, std::uint64_t end) {
            for (const auto& [writtenBegin, writtenEnd] : written) {
                if (writtenEnd <= begin) continue;
                if (writtenBegin > begin) return false;
                begin = writtenEnd;
                if (begin >= end) return true;
            }
            return false;
        };
        if (stencilPlane) return target.stencilAddress != 0 && ((target.samples == 1u && reaches(target.stencilAddress, 1u)) || covers(target.stencilAddress, target.stencilAddress + stencilBytes()));
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return (target.samples == 1u && reaches(target.address, d16 ? 2u : 4u)) || covers(target.address, target.address + depthBytes());
    }

    bool CoveredBy(std::span<const std::pair<std::uint64_t, std::uint64_t>> written, std::span<const WrittenStorageExtent> extents) const {
        return PlaneCoveredBy(false, written, extents) || PlaneCoveredBy(true, written, extents);
    }

    bool StencilWrittenInMemory() const {
        if (target.stencilAddress == 0) return false;
        GuestMemory::CollectWrites(target.stencilAddress, static_cast<std::size_t>(stencilBytes()));
        return GuestMemory::WrittenSince(target.stencilAddress, static_cast<std::size_t>(stencilBytes()), stencilMemory);
    }

    void NoteMemory() {
        depthMemory = GuestMemory::CollectWrites(target.address, static_cast<std::size_t>(depthBytes()));
        stencilMemory = target.stencilAddress != 0 ? GuestMemory::CollectWrites(target.stencilAddress, static_cast<std::size_t>(stencilBytes())) : 0;
    }

    bool OverwrittenOnGpu() const {
        std::array<std::pair<std::uint64_t, std::uint64_t>, 2> ranges{{{target.address, target.address + depthBytes()}, {target.stencilAddress, target.stencilAddress + stencilBytes()}}};
        return StorageTexture::AnyPendingOverlaps(std::span(ranges).first(target.stencilAddress != 0 ? 2u : 1u));
    }

    bool OverwrittenInMemory() const {
        GuestMemory::CollectWrites(target.address, static_cast<std::size_t>(depthBytes()));
        if (GuestMemory::WrittenSince(target.address, static_cast<std::size_t>(depthBytes()), depthMemory)) return true;
        if (target.stencilAddress == 0) return false;
        GuestMemory::CollectWrites(target.stencilAddress, static_cast<std::size_t>(stencilBytes()));
        return GuestMemory::WrittenSince(target.stencilAddress, static_cast<std::size_t>(stencilBytes()), stencilMemory);
    }

    VkImageAspectFlags pendingClear = 0;
    float clearDepth = 0.0f;
    std::uint8_t clearStencil = 0;
    std::optional<float> depthFill;
    std::optional<std::uint8_t> stencilFill;

    bool StorageViewMatches(std::span<const std::uint32_t> words, const GuestTextureResource& resource) const {
        if (resource.width != target.extent.width || resource.height != target.extent.height) return false;
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        return ResolveTextureFormat(resource.format) == expected || depthBits;
    }

    void RequireStorable(const GuestTextureResource& resource) const {
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        Require(target.samples == 1, "storage image access to a multisampled depth surface is not implemented");
        if (!stencil && hostFormat != target.format) {
            char text[224];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image access to the depth plane of depth surface 0x%llx (vk format %d) stored as vk format %d is not implemented", static_cast<unsigned long long>(target.address), static_cast<int>(target.format), static_cast<int>(hostFormat));
            throw std::runtime_error(text);
        }
    }

    std::shared_ptr<StorageTexture> Storage(std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip) {
        if (!StorageViewMatches(words, resource)) return nullptr;
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool oneLayer = resource.dimension == TextureDimension::k2DArray && resource.baseArray == 0 && resource.depthOrLastArray == 0;
        if (resource.dimension != TextureDimension::k2D && !oneLayer) {
            char text[256];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image access to depth/stencil surface 0x%llx is not implemented (storage=%ux%u guest format=%u mip=%u dimension=%d tile=%d)", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, mip, static_cast<int>(resource.dimension), static_cast<int>(resource.tileMode));
            throw std::runtime_error(text);
        }
        RequireStorable(resource);
        auto* overwrite = stencil ? nullptr : PlaneOverwritten(words);
        ApplyFastClear();
        auto& plane = stencil ? stencilPlane : depthPlane;
        if (plane.storage == nullptr || plane.storage->Descriptor().format != resource.format || plane.storage->Descriptor().tileMode != resource.tileMode || plane.storage->Descriptor().baseAddress != resource.baseAddress || plane.storage->Descriptor().dimension != resource.dimension) {
            std::erase_if(plane.replaced, [](const auto& old) { return old.expired(); });
            if (plane.storage != nullptr) plane.replaced.push_back(plane.storage);
            plane.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
        }
        if (overwrite != nullptr) {
            ++depthClearCounters().copiesSkipped;
        } else {
            copyPlane(plane, plane.storage, stencil, true);
        }
        return plane.storage;
    }

    bool OverwritesWholePlane(const DepthPlaneOverwrite& overwrite) const {
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return overwrite.bits == (d16 ? 16u : 32u) && overwrite.width >= target.extent.width && overwrite.height >= target.extent.height;
    }

    DepthPlaneOverwrite* PlaneOverwritten(std::span<const std::uint32_t> words) const {
        auto* overwrite = currentOverwrite;
        if (overwrite == nullptr || words.size() < overwrite->image.size() || !std::equal(overwrite->image.begin(), overwrite->image.end(), words.begin())) return nullptr;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        auto& declined = depthClearCounters().declined;
        if (overwrite->bits != (d16 ? 16u : 32u)) {
            ++declined[DeclineSize];
            return nullptr;
        }
        if (overwrite->width < target.extent.width || overwrite->height < target.extent.height) {
            ++declined[DeclineCoverage];
            return nullptr;
        }
        return overwrite;
    }

    bool StoreOverwrite(const StorageTexture& mirror) {
        auto* overwrite = currentOverwrite;
        if (overwrite == nullptr || !OverwritesWholePlane(*overwrite)) return false;
        const auto& described = mirror.Descriptor();
        const auto& view = overwrite->view;
        if (described.baseAddress != view.baseAddress || described.format != view.format || described.tileMode != view.tileMode || described.dimension != view.dimension) return false;
        std::uint32_t current = 0;
        try {
            GuestMemory::Read(overwrite->colorAddress, std::as_writable_bytes(std::span(&current, 1)), 4);
        } catch (...) {
            return false;
        }
        if (current != overwrite->colorBits) return false;
        auto* recorder = RecorderFor(context);
        Require(recorder != nullptr, "depth surface storage clears need the recorder of the surface's device");
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto bytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * (d16 ? 2u : 4u);
        recorder->Commands();
        const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
        RecordClearValue(VK_IMAGE_ASPECT_DEPTH_BIT, overwrite->depth, clearStencil);
        recorder->EndGpuTiming(timing, bytes);
        auto& counters = depthClearCounters();
        ++counters.clears;
        counters.bytesSkipped += 2u * bytes;
        return true;
    }

    bool StoreBack(const StorageTexture& candidate) {
        for (const bool stencil : {false, true}) {
            auto& plane = stencil ? stencilPlane : depthPlane;
            if (plane.storage.get() == &candidate) {
                if (stencil || !StoreOverwrite(candidate)) copyPlane(plane, plane.storage, stencil, false);
                written = ++depthWrites;
                NoteHostWrite();
                return true;
            }
            if (std::none_of(plane.replaced.begin(), plane.replaced.end(), [&](const auto& old) { return old.lock().get() == &candidate; })) continue;
            char text[288];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image writes to the %s plane of depth surface 0x%llx through a view that another view of the plane (guest format %u, tile mode %d) replaced are not implemented", stencil ? "stencil" : "depth", static_cast<unsigned long long>(target.address), plane.storage->Descriptor().format, static_cast<int>(plane.storage->Descriptor().tileMode));
            throw std::runtime_error(text);
        }
        return false;
    }

    void CopyDepthTo(const std::shared_ptr<StorageTexture>& storage) {
        ApplyFastClear();
        copyPlane(depthPlane, storage, false, true);
    }

    void CopyStencilTo(const std::shared_ptr<StorageTexture>& storage) {
        ApplyFastClear();
        copyPlane(stencilPlane, storage, true, true);
    }

    bool StoreBackCovering(const StorageTexture& candidate) {
        for (const bool stencil : {false, true}) {
            if (!CoversPlane(candidate, stencil)) continue;
            auto& plane = stencil ? stencilPlane : depthPlane;
            const auto source = std::const_pointer_cast<StorageTexture>(candidate.shared_from_this());
            copyPlane(plane, source, stencil, false);
            ForgetClear(stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT);
            written = ++depthWrites;
            NoteHostWrite();
            return true;
        }
        return false;
    }

    void TakeStencilStorage() {
        if (target.samples != 1 || target.stencilAddress == 0) return;
        const auto writes = StorageTexture::EightBitWrites();
        if (writes == stencilWritesSeen) return;
        stencilWritesSeen = writes;
        const auto bytes = DepthSliceBytes(target.extent, 1u);
        auto storage = StorageTexture::FindPending(target.stencilAddress, bytes);
        if (storage == nullptr || storage == stencilMirror.storage || storage->Device() != context.device) return;
        const auto& described = storage->Descriptor();
        const auto format = ResolveTextureFormat(described.format);
        if (described.baseAddress != target.stencilAddress || described.width != target.extent.width || described.height != target.extent.height || (format != VK_FORMAT_R8_UINT && format != VK_FORMAT_R8_UNORM)) return;
        if (stencilTaken.lock() == storage && stencilTakenVersion == storage->Version()) return;
        storage->Refresh();
        copyPlane(stencilPlane, storage, true, false, Recorder::Active() == nullptr);
        stencilTaken = storage;
        stencilTakenVersion = storage->Version();
        written = ++depthWrites;
        NoteHostWrite();
    }

    void TakeStorage() {
        if (target.samples != 1 || hostFormat != target.format) return;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto bytes = DepthSliceBytes(target.extent, d16 ? 2u : 4u);
        auto storage = StorageTexture::FindPending(target.address, bytes);
        if (storage == nullptr) storage = StorageTexture::FindLive(target.address, bytes);
        if (storage == nullptr || !storageFits(target, storage->Descriptor())) return;
        storage->Refresh();
        const bool direct = DirectDepthCopy(context, hostFormat, VK_IMAGE_ASPECT_DEPTH_BIT, storage->ImageFormat());
        if (!direct && depthPlane.staging == nullptr) depthPlane.staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(target.extent.width) * target.extent.height * (d16 ? 2u : 4u), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        if (recorder != nullptr) recorder->Keep(storage);
        VkBufferImageCopy depthRegion{0, 0, 0, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        VkBufferImageCopy colorRegion = depthRegion;
        colorRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if (direct) {
            const VkImageCopy copy{{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
            const bool discarding = target.stencilAddress == 0;
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            if (discarding) {
                VkImageMemoryBarrier discard{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                discard.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                discard.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                discard.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                discard.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                discard.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                discard.image = image;
                discard.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
                const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
                barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &discard);
                context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
                VkImageMemoryBarrier toGeneral = discard;
                toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                toGeneral.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
            } else {
                context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            }
            directDepthCopies.fetch_add(1, std::memory_order_relaxed);
            if (batch) batch->SubmitAndWait();
            else Recorder::CountBarriers(Recorder::CommandClass::Copy, discarding ? 3 : 2);
            return;
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, depthPlane.staging->Handle(), 1, &colorRegion);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        if (target.stencilAddress == 0) {
            VkImageMemoryBarrier discard{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            discard.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            discard.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            discard.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            discard.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            discard.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            discard.image = image;
            discard.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
            const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
            barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &discard);
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, depthPlane.staging->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &depthRegion);
            VkImageMemoryBarrier toGeneral = discard;
            toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toGeneral.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        } else {
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, depthPlane.staging->Handle(), image, VK_IMAGE_LAYOUT_GENERAL, 1, &depthRegion);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        }
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Copy, target.stencilAddress == 0 ? 4 : 3);
    }

    std::shared_ptr<StorageTexture> Mirror(const GuestTextureResource& resource) {
        if (mirrorPlane.storage == nullptr) {
            mirrorPlane.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
            ++mirrorEpoch;
        }
        return mirrorPlane.storage;
    }

    bool StoresUnits(const StorageTexture& mirror) const {
        return mirror.BlockUnits() && mirror.TrackedUnits() == staleUnits.size();
    }

    std::shared_ptr<StorageTexture> StoreDepthPlane(const GuestTextureResource& resource, std::vector<bool>& units) {
        ApplyFastClear();
        auto mirror = Mirror(resource);
        auto& counters = depthPlaneStoreCounters();
        ++counters.stores;
        const auto planeBytes = mirror->GuestBytes();
        if (!StoresUnits(*mirror)) {
            units.assign(units.size(), true);
            copyPlane(mirrorPlane, mirror, false, true, true);
            mirror->MarkWholeWritten();
            counters.units += units.size();
            counters.bytes += planeBytes;
            return mirror;
        }
        const auto rects = mirror->UnitRects(units);
        copyPlane(mirrorPlane, mirror, false, true, true, &rects);
        mirror->MarkUnitsWritten(units);
        for (std::size_t unit = 0; unit < units.size(); ++unit) {
            if (!units[unit]) continue;
            ++counters.units;
            counters.bytes += std::min<std::uint64_t>(unitBytes, planeBytes - unit * unitBytes);
        }
        return mirror;
    }

    void SizeUnits(std::uint64_t planeBytes) {
        static const bool whole = std::getenv("APS5_DEPTH_PLANE_WHOLE_STORE") != nullptr;
        const std::uint64_t bytes = !whole && target.address % 65536u == 0 ? std::uint64_t{65536} : std::max<std::uint64_t>(planeBytes, 1u);
        const auto count = static_cast<std::size_t>(std::max<std::uint64_t>((planeBytes + bytes - 1u) / bytes, 1u));
        if (unitBytes == bytes && staleUnits.size() == count) return;
        unitBytes = bytes;
        staleUnits.assign(count, false);
        staleCount = 0;
        unitGeneration.assign(count, 0);
        unitBacking.assign(count, 0);
        backingGeneration = 0;
    }

    const StorageTexture* MirrorStorage() const {
        return mirrorPlane.storage.get();
    }

    bool AnyStale() const {
        return staleCount != 0;
    }

    void NoteDepthWork() {
        depthMark = nextPlaneSerial();
    }

    bool AllStale() const {
        return staleCount != 0 && staleCount == staleUnits.size();
    }

    void MarkAllStale(std::uint64_t planeBytes) {
        SizeUnits(planeBytes);
        staleUnits.assign(staleUnits.size(), true);
        staleCount = staleUnits.size();
    }

    void ClearStale() {
        staleUnits.assign(staleUnits.size(), false);
        staleCount = 0;
    }

    void ClearStale(const std::vector<bool>& units) {
        for (std::size_t unit = 0; unit < units.size() && unit < staleUnits.size(); ++unit) {
            if (!units[unit] || !staleUnits[unit]) continue;
            staleUnits[unit] = false;
            --staleCount;
        }
    }

    std::vector<bool> StaleUnitsIn(std::uint64_t planeBytes, std::uint64_t begin, std::uint64_t end) const {
        std::vector<bool> units(staleUnits.size(), false);
        const auto plane = target.address;
        if (units.empty() || unitBytes == 0 || begin >= end || begin >= plane + planeBytes || plane >= end) return units;
        const auto from = std::max(begin, plane) - plane;
        const auto to = std::min(end, plane + planeBytes) - plane;
        const auto last = std::min<std::uint64_t>((to - 1u) / unitBytes, units.size() - 1u);
        for (auto unit = from / unitBytes; unit <= last; ++unit) units[static_cast<std::size_t>(unit)] = staleUnits[static_cast<std::size_t>(unit)];
        return units;
    }

    std::pair<std::uint64_t, std::uint64_t> UnitHull(const std::vector<bool>& units, std::uint64_t planeBytes) const {
        std::uint64_t first = planeBytes;
        std::uint64_t last = 0;
        for (std::size_t unit = 0; unit < units.size(); ++unit) {
            if (!units[unit]) continue;
            first = std::min<std::uint64_t>(first, unit * unitBytes);
            last = std::max<std::uint64_t>(last, std::min<std::uint64_t>(planeBytes, (unit + 1u) * unitBytes));
        }
        return {target.address + first, target.address + std::max(first, last)};
    }

    void NoteStored(const std::vector<bool>& units, std::uint64_t generation) {
        for (std::size_t unit = 0; unit < units.size() && unit < unitGeneration.size(); ++unit) {
            if (units[unit]) unitGeneration[unit] = generation;
        }
    }

    std::vector<bool> WrittenUnits(const GuestTextureResource& resource, std::uint64_t& generation) {
        const auto bytes = static_cast<std::size_t>(MirrorBytes());
        SizeUnits(bytes);
        const auto collected = GuestMemory::CollectWrites(resource.baseAddress, bytes);
        generation = GuestMemory::TrackerGeneration();
        std::vector<bool> changed;
        const auto mark = [&](std::size_t unit) {
            if (changed.empty()) changed.assign(unitGeneration.size(), false);
            changed[unit] = true;
        };
        if (collected == 0u) {
            for (std::size_t unit = 0; unit < unitGeneration.size(); ++unit) {
                const auto offset = unit * unitBytes;
                const auto address = resource.baseAddress + offset;
                const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(unitBytes, bytes - offset));
                if (GuestMemory::CollectWritesUncached(address, length) != 0u) {
                    if (unitGeneration[unit] != 0u && GuestMemory::WrittenSince(address, length, unitGeneration[unit])) mark(unit);
                    continue;
                }
                constexpr std::uint64_t pageBytes = 4096u;
                for (std::size_t page = 0; page < length;) {
                    const auto pageAddress = address + page;
                    const auto pageLength = static_cast<std::size_t>(std::min<std::uint64_t>(length - page, pageBytes - pageAddress % pageBytes));
                    if (GuestMemory::CollectWritesUncached(pageAddress, pageLength) != 0u && unitGeneration[unit] != 0u && GuestMemory::WrittenSince(pageAddress, pageLength, unitGeneration[unit])) mark(unit);
                    page += pageLength;
                }
            }
            generation = GuestMemory::TrackerGeneration();
            return changed;
        }
        if (unitGeneration.size() == 1u) {
            if (GuestMemory::WrittenSince(resource.baseAddress, bytes, unitGeneration[0])) mark(0);
            return changed;
        }
        const auto oldest = *std::min_element(unitGeneration.begin(), unitGeneration.end());
        if (oldest != 0u && !GuestMemory::WrittenSince(resource.baseAddress, bytes, oldest)) return changed;
        std::vector<std::uint8_t> stamped(unitGeneration.size(), GuestMemory::BlockUnchanged);
        if (!GuestMemory::ChangedBlocks(resource.baseAddress, bytes, unitGeneration, stamped)) return changed;
        for (std::size_t unit = 0; unit < stamped.size(); ++unit) {
            if (unitGeneration[unit] != 0u && stamped[unit] == GuestMemory::BlockWritten) mark(unit);
        }
        return changed;
    }

    std::vector<bool> ChangedBackingUnits() const {
        std::vector<bool> changed(unitBacking.size(), false);
        if (backingGeneration == GuestAllocations::GuestAllocationsGeneration_nid_postfix()) return changed;
        for (std::size_t unit = 0; unit < unitBacking.size(); ++unit) {
            changed[unit] = unitBacking[unit] != 0 && RegisteredBacking(target.address + unit * unitBytes) != unitBacking[unit];
        }
        return changed;
    }

    void SyncFromMemory(const GuestTextureResource& resource, bool storing = false) {
        std::uint64_t generation = 0;
        const auto changed = WrittenUnits(resource, generation);
        // The registry generation moves on every mapping mutation, so an unchanged value means every recorded backing is still current. Read it before the lookups: a mutation racing them leaves the stored value old and the next sync refreshes again.
        if (const auto registryGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix(); registryGeneration != backingGeneration) {
            for (std::size_t unit = 0; unit < unitBacking.size(); ++unit) unitBacking[unit] = RegisteredBacking(target.address + unit * unitBytes);
            backingGeneration = registryGeneration;
        }
        const bool clearing = (pendingClear & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
        if ((clearing && !storing) || !anyUnit(changed)) {
            unitGeneration.assign(unitGeneration.size(), generation);
            return;
        }
        StorageTexture::NoteRefreshWork();
        if (clearing) {
            ClearStale();
            ForgetClear(VK_IMAGE_ASPECT_DEPTH_BIT);
        } else {
            ClearStale(changed);
        }
        {
            struct Syncing {
                bool& flag;
                ~Syncing() { flag = false; }
            } guard{syncing};
            syncing = true;
            if (mirrorPlane.storage == nullptr) {
                mirrorPlane.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
                ++mirrorEpoch;
            } else {
                static_cast<void>(mirrorPlane.storage->Refresh());
            }
        }
        unitGeneration.assign(unitGeneration.size(), generation);
        const auto mirror = mirrorPlane.storage;
        if (StoresUnits(*mirror)) {
            const auto rects = mirror->UnitRects(changed);
            copyPlane(mirrorPlane, mirror, false, false, true, &rects);
        } else {
            ClearStale();
            copyPlane(mirrorPlane, mirror, false, false, true);
        }
        written = ++depthWrites;
    }

    void DropMirror() {
        mirrorPlane = {};
    }

    bool StencilMirrorable() const {
        return target.samples == 1u && target.stencilAddress != 0u && target.depthSwizzle == XorSwizzleMode(TextureTileMode::kZ64KBX);
    }

    void SyncStencilFromMemory(const GuestTextureResource& resource) {
        const auto bytes = static_cast<std::size_t>(StencilMirrorBytes());
        GuestMemory::CollectWrites(resource.baseAddress, bytes);
        const auto generation = GuestMemory::TrackerGeneration();
        const bool changed = stencilGeneration != 0u && GuestMemory::WrittenSince(resource.baseAddress, bytes, stencilGeneration);
        stencilGeneration = generation;
        if (!changed || (pendingClear & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) return;
        StorageTexture::NoteRefreshWork();
        stencilStale = false;
        {
            struct Syncing {
                bool& flag;
                ~Syncing() { flag = false; }
            } guard{syncing};
            syncing = true;
            if (stencilMirror.storage == nullptr) stencilMirror.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
            else static_cast<void>(stencilMirror.storage->Refresh());
        }
        copyPlane(stencilMirror, stencilMirror.storage, true, false, true);
        written = ++depthWrites;
    }

    std::shared_ptr<StorageTexture> StoreStencilPlane(const GuestTextureResource& resource) {
        ApplyFastClear();
        if (stencilMirror.storage == nullptr) stencilMirror.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
        copyPlane(stencilMirror, stencilMirror.storage, true, true, true);
        stencilMirror.storage->MarkWholeWritten();
        stencilStale = false;
        return stencilMirror.storage;
    }

    void NoteStencilStored(std::uint64_t generation) {
        stencilGeneration = generation;
        if (StencilMirrorable()) TakeStencilBaseline();
    }

    bool stencilStale = false;
    std::uint64_t stencilGeneration = 0;
    std::vector<std::byte> stencilBaseline;

    std::vector<bool> staleUnits;
    std::size_t staleCount = 0;
    std::uint64_t depthMark = 0;
    std::uint64_t stencilMark = 0;
    std::vector<std::uint64_t> unitBacking;
    std::uint64_t backingGeneration = 0;
    std::vector<std::uint64_t> unitGeneration;
    std::uint64_t unitBytes = 0;
    bool syncing = false;
    std::uint64_t scannedEpoch = 0;
    mutable std::uint64_t mirrorBytes = 0;
    mutable std::uint64_t stencilMirrorBytes = 0;

    std::uint64_t MirrorBytes() const;
    std::uint64_t StencilMirrorBytes() const;

    const Context context;
    const DepthTarget target;
    const VkFormat hostFormat;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::shared_ptr<std::atomic<bool>> active = std::make_shared<std::atomic<bool>>(true);
    bool sampleable = true;
    bool samplingActive = false;
    DepthSamplingState sampling;
    bool clearPending = false;
    std::uint64_t clearGeneration = 0u;
    bool clearCompletionQueued = false;
    std::uint32_t clearDispatchesPending = 0u;
    VkImageAspectFlags clearAspects = 0u;
    std::optional<std::uint32_t> clearPattern;
    std::uint64_t clearFillNote = 0u;
    std::uint64_t clearFillFloor = 0u;
    bool clearStored = false;
    std::uint64_t stencilClearGeneration = 0u;
    std::optional<std::uint8_t> stencilClearValue;
    std::uint64_t stencilFillNote = 0u;
    std::uint64_t stencilFillFloor = 0u;
    bool stencilStored = false;
    bool expanded = false;
    std::uint32_t renderControl = 0u;
    std::shared_ptr<OwnedImage> owned;
    std::shared_ptr<OwnedImage> sampleShadow;
    bool useSampleShadow = false;
    bool directSampled = false;
    VkRenderPass stencilClearPass = VK_NULL_HANDLE;
    VkFramebuffer stencilClearFb = VK_NULL_HANDLE;
    std::uint64_t sampleShadowWritten = 0u;
    bool sampleShadowStencil = false;

private:
    struct PublicationPlane {
        static constexpr std::size_t PageBytes = 4096u;
        static constexpr std::size_t Repeated = std::numeric_limits<std::size_t>::max();
        GuestMemory::UnchangedQuery range;
        std::vector<std::uint32_t> words;
        std::vector<std::size_t> starts;
        std::vector<std::byte> stored;

        std::byte Byte(std::size_t offset) const {
            const auto page = offset / PageBytes;
            if (starts[page] != Repeated) return stored[starts[page] + offset % PageBytes];
            return static_cast<std::byte>(words[page] >> (offset % 4u * 8u));
        }

        std::vector<std::byte> Expand() const {
            return ExpandPagedSnapshot(range.bytes, words, starts, stored, PageBytes, Repeated);
        }
    };
    std::vector<PublicationPlane> publication;
    bool publicationInProgress = false;
    bool publicationFailed = false;
    std::exception_ptr publicationFailure;

    void capturePublication() {
        if ((target.format != VK_FORMAT_D32_SFLOAT_S8_UINT && !bareDepthTarget(target)) || context.detiler == nullptr) return;
        try {
            const auto lease = GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
            CheckCurrent();
            std::vector<PublicationPlane> captured;
            for (const auto& range : sampling.initial) captured.push_back(CapturePublication(range));
            for (const auto& plane : captured) {
                Require(GuestMemory::UnchangedSinceCollected(plane.range.address, plane.range.bytes, plane.range.generation), "guest writes raced native multisample depth publication capture");
                Require(GuestMemory::UnchangedSince(plane.range.address, plane.range.bytes, plane.range.generation), "guest writes raced native multisample depth publication capture");
            }
            for (const auto& range : sampling.initial) Require(GuestMemory::UnchangedSinceCollected(range.address, range.bytes, range.generation), "guest writes raced native multisample depth publication capture");
            Require(GuestMemory::UnchangedSinceAll(sampling.initial), "guest writes raced native multisample depth publication capture");
            publication = std::move(captured);
        } catch (...) {
            publicationFailure = std::current_exception();
            publication.clear();
        }
    }

    std::unique_ptr<Buffer> readBackSamples(VkImageAspectFlagBits aspect, std::uint64_t elements) {
        const bool depth = aspect == VK_IMAGE_ASPECT_DEPTH_BIT;
        const auto linearBytes = (elements * (depth ? 4u : 1u) + 3u) & ~std::uint64_t{3u};
        Require(linearBytes <= context.limits.maxStorageBufferRange, "native multisample depth publication exceeds the device's readback range");
        auto linear = std::make_unique<Buffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        VkImageView arrayView = VK_NULL_HANDLE;
        try {
            VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            info.image = useSampleShadow ? sampleShadow->image : image;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            info.format = hostFormat;
            info.subresourceRange = {static_cast<VkImageAspectFlags>(aspect), 0u, 1u, 0u, 1u};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &arrayView), "vkCreateImageView depth publication");
            CommandBatch batch(context);
            constexpr VkAccessFlags writes = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, writes, VK_ACCESS_SHADER_READ_BIT);
            context.detiler->DispatchMultisampleReadback(batch.Handle(), depth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R8_UINT, linear->Handle(), 0u, linearBytes, arrayView, target.extent.width, target.extent.height, 1u, target.samples, 0u, static_cast<std::uint32_t>(elements));
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            batch.SubmitAndWait();
        } catch (...) {
            if (arrayView != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, arrayView, nullptr);
            throw;
        }
        context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, arrayView, nullptr);
        linear->Invalidate();
        return linear;
    }

    PublicationPlane CapturePublication(const GuestMemory::UnchangedQuery& range) {
        PublicationPlane plane;
        plane.range = range;
        plane.range.generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
        const auto pages = (range.bytes + PublicationPlane::PageBytes - 1u) / PublicationPlane::PageBytes;
        plane.words.assign(pages, 0u);
        plane.starts.assign(pages, PublicationPlane::Repeated);
        std::array<std::byte, PublicationPlane::PageBytes> page;
        for (std::size_t index = 0u; index < pages; ++index) {
            const auto first = index * PublicationPlane::PageBytes;
            const auto length = std::min(PublicationPlane::PageBytes, range.bytes - first);
            GuestMemory::CopyGuestBytes(page.data(), reinterpret_cast<const void*>(range.address + first), length);
            if (length <= 4u || std::equal(page.begin() + 4u, page.begin() + length, page.begin())) {
                std::memcpy(&plane.words[index], page.data(), std::min<std::size_t>(length, 4u));
            } else {
                plane.starts[index] = plane.stored.size();
                plane.stored.insert(plane.stored.end(), page.begin(), page.begin() + length);
            }
        }
        Require(GuestMemory::UnchangedSinceCollected(range.address, range.bytes, plane.range.generation) && GuestMemory::UnchangedSince(range.address, range.bytes, plane.range.generation), "guest writes raced native multisample depth publication capture");
        return plane;
    }

    void RebasePublication(std::size_t index) {
        if (publication.empty()) return;
        try {
            publication[index] = CapturePublication(sampling.initial[index]);
        } catch (...) {
            publicationFailure = std::current_exception();
            publication.clear();
        }
    }

    struct PlaneStorage {
        std::shared_ptr<StorageTexture> storage;
        std::vector<std::weak_ptr<StorageTexture>> replaced;
        std::unique_ptr<DeviceBuffer> staging;
    };
    struct StencilCopy {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        std::unique_ptr<DeviceBuffer> staging;
        std::map<std::array<std::uint32_t, 13>, std::shared_ptr<Texture>> textures;
        std::uint64_t written = 0;
        bool filled = false;
    };
    std::map<std::array<std::uint32_t, 13>, std::shared_ptr<Texture>> textures;
    struct FirstSampleTextures {
        FirstSampleTextures(const Context& context, VkFormat format, VkExtent2D extent) : copy(context, format, extent) {}
        FirstSampleCopy copy;
        std::map<std::array<std::uint32_t, 4>, std::shared_ptr<Texture>> textures;
    };
    std::shared_ptr<FirstSampleTextures> firstSample;
    PlaneStorage depthPlane;
    PlaneStorage stencilPlane;
    std::weak_ptr<StorageTexture> stencilTaken;
    std::uint64_t stencilTakenVersion = 0;
    std::uint64_t stencilWritesSeen = 0;
    std::uint64_t depthMemory = 0;
    std::uint64_t stencilMemory = 0;
    StencilCopy stencilCopy;
    VkImageView transferView = VK_NULL_HANDLE;
    std::shared_ptr<OwnedImage> depthUpload;

    const std::shared_ptr<OwnedImage>& depthUploadImage() {
        if (depthUpload != nullptr) return depthUpload;
        VkImage uploadImage = VK_NULL_HANDLE;
        VkDeviceMemory uploadMemory = VK_NULL_HANDLE;
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = hostFormat;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &uploadImage), "vkCreateImage depth upload");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, uploadImage, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(MemoryAccounting::Allocate(context, allocation, "depth upload", &uploadMemory), "vkAllocateMemory depth upload");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, uploadImage, uploadMemory, 0), "vkBindImageMemory depth upload");
            depthUpload = std::make_shared<OwnedImage>(context, uploadImage, uploadMemory);
        } catch (...) {
            if (uploadImage != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, uploadImage, nullptr);
            if (uploadMemory != VK_NULL_HANDLE) MemoryAccounting::Free(context, uploadMemory);
            throw;
        }
        return depthUpload;
    }

    VkImageUsageFlags sampleShadowUsage() const {
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (HasStencil(hostFormat) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0u);
    }

    void importSamples(VkCommandBuffer commands, Recorder* recorder, std::vector<std::shared_ptr<void>>& kept) {
        constexpr std::uint32_t sampleBytes = sizeof(float);
        const auto staged = std::move(sampling.importedSamples);
        Require(context.detiler != nullptr && hostFormat == VK_FORMAT_D32_SFLOAT, "importing native multisample depth samples requires a texture detiler and a D32 float host image");
        kept.push_back(staged);
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.flags = importedSampleFlags;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R32_SFLOAT;
        info.extent = {target.extent.width, target.extent.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = static_cast<VkSampleCountFlagBits>(target.samples);
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        const bool copied = sampling.importedByCopy;
        info.usage = copied ? copiedSampleUsage : importedSampleUsage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage created = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &created), "vkCreateImage native depth samples");
        const auto source = std::make_shared<OwnedImage>(context, created, VK_NULL_HANDLE);
        kept.push_back(source);
        source->Allocate(context, info, "vkAllocateMemory native depth samples");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, source->image, source->allocation.Memory(), source->allocation.Offset()), "vkBindImageMemory native depth samples");
        const auto storage = std::make_shared<Texture>(context, source->image, TextureDetiler::ImageElementFormat(4u), VK_IMAGE_ASPECT_COLOR_BIT, VkComponentMapping{}, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0u, source, VK_IMAGE_USAGE_STORAGE_BIT);
        kept.push_back(storage);
        VkImageMemoryBarrier toGeneral{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral.image = source->image;
        toGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        const auto alignment = std::max<VkDeviceSize>(4u, context.limits.minStorageBufferOffsetAlignment);
        const auto stride = context.limits.maxStorageBufferRange / alignment * alignment;
        Require(stride != 0u, "importing native multisample depth samples needs a storage range of at least one aligned element");
        const auto bytes = static_cast<std::uint64_t>(target.extent.width) * target.extent.height * target.samples * sampleBytes;
        Require(staged->Bytes().size() >= bytes, "the staged native multisample depth samples do not cover the target");
        for (std::uint64_t offset = 0u; offset < bytes; offset += stride) {
            const auto count = std::min<std::uint64_t>(stride, bytes - offset);
            context.detiler->DispatchMultisampleImage(commands, sampleBytes, staged->Handle(), offset, count, storage->View(), target.extent.width, target.extent.height, 1u, target.samples, static_cast<std::uint32_t>(offset / sampleBytes), static_cast<std::uint32_t>(count / sampleBytes));
        }
        if (copied) copySamples(commands, source->image);
        else drawSamples(commands, recorder, source, kept);
        if (recorder != nullptr) Recorder::CountBarriers(copied ? Recorder::CommandClass::Copy : Recorder::CommandClass::Draw, 4);
    }

    void drawSamples(VkCommandBuffer commands, Recorder* recorder, const std::shared_ptr<OwnedImage>& samples, std::vector<std::shared_ptr<void>>& kept) {
        const auto sampled = std::make_shared<Texture>(context, samples->image, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, VkComponentMapping{}, VK_IMAGE_VIEW_TYPE_2D, 0u, samples, VK_IMAGE_USAGE_SAMPLED_BIT);
        kept.push_back(sampled);
        CommandState batchState;
        if (recorder == nullptr) batchState.Begin(commands);
        auto& recording = recorder != nullptr ? recorder->GraphicsState() : batchState;
        Require(recording.Commands() == commands, "the native multisample depth sample import draws outside the command buffer that imports it");
        RecordDepthSampleImport(context, recording, {image, view, hostFormat, target.extent, target.samples, SampleOrderMethod::Depth, owned}, sampled->View(), kept);
    }

    void copySamples(VkCommandBuffer commands, VkImage samples) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkImageCopy copy{{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, samples, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        directDepthCopies.fetch_add(1, std::memory_order_relaxed);
    }

    void createSampleShadow() {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = hostFormat;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = static_cast<VkSampleCountFlagBits>(target.samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = sampleShadowUsage();
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage sampled depth shadow");
            auto owned = std::make_shared<OwnedImage>(context, image, VK_NULL_HANDLE);
            image = VK_NULL_HANDLE;
            owned->Allocate(context, info, "vkAllocateMemory sampled depth shadow");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, owned->image, owned->allocation.Memory(), owned->allocation.Offset()), "vkBindImageMemory sampled depth shadow");
            sampleShadow = std::move(owned);
            auto* recorder = RecorderFor(context);
            if (recorder != nullptr) recorder->Keep(sampleShadow);
            std::unique_ptr<CommandBatch> batch;
            if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
            const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = sampleShadow->image;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | (HasStencil(hostFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u), 0, 1, 0, 1};
            context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            if (batch) batch->SubmitAndWait();
            else Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        } catch (...) {
            if (image != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory != VK_NULL_HANDLE) MemoryAccounting::Free(context, memory);
            throw;
        }
    }

    void refreshSampleShadow(bool drawn, bool stencilRead = false) {
        Require(sampleShadow != nullptr, "native multisample depth shadow was not created");
        if (stencilRead && !sampleShadowStencil) {
            sampleShadowStencil = true;
            sampleShadowWritten = 0u;
        }
        if (!drawn && sampleShadowWritten == written) return;
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        if (recorder != nullptr) recorder->Keep(sampleShadow);
        constexpr VkAccessFlags accesses = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, accesses, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT | (sampleShadowStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u), 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = {target.extent.width, target.extent.height, 1};
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, sampleShadow->image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, accesses);
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        sampleShadowWritten = written;
    }

    std::shared_ptr<Texture> unormStencil(const std::array<std::uint32_t, 13>& key, VkComponentMapping components, VkImageViewType viewType, bool drawn) {
        if (stencilCopy.image == VK_NULL_HANDLE) {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = VK_FORMAT_R8_UNORM;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            try {
                Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &stencilCopy.image), "vkCreateImage stencil copy");
                VkMemoryRequirements requirements{};
                context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, stencilCopy.image, &requirements);
                VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                allocation.allocationSize = requirements.size;
                allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                stencilCopy.memory = AllocateImageMemory(context, allocation, requirements.memoryTypeBits, "vkAllocateMemory stencil copy");
                Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, stencilCopy.image, stencilCopy.memory, 0), "vkBindImageMemory stencil copy");
                stencilCopy.staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(target.extent.width) * target.extent.height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            } catch (...) {
                releaseStencilCopy();
                throw;
            }
        }
        if (drawn || !stencilCopy.filled || stencilCopy.written != written) {
            copyStencil();
            stencilCopy.written = drawn ? 0 : written;
        }
        auto& texture = stencilCopy.textures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, stencilCopy.image, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, components, viewType);
            texture->MarkRefreshedPerUse();
        }
        return texture;
    }

    void copyStencil() {
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy region{0, 0, 0, {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_GENERAL, stencilCopy.staging->Handle(), 1, &region);
        VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = stencilCopy.filled ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = stencilCopy.image;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const VkBufferMemoryBarrier staged{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, stencilCopy.staging->Handle(), 0, VK_WHOLE_SIZE};
        barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &staged, 1, &toTransfer);
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, stencilCopy.staging->Handle(), stencilCopy.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        VkImageMemoryBarrier toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        stencilCopy.filled = true;
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Copy, 3);
    }

    void releaseStencilCopy() noexcept {
        stencilCopy.textures.clear();
        stencilCopy.staging.reset();
        if (stencilCopy.image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, stencilCopy.image, nullptr);
        if (stencilCopy.memory) MemoryAccounting::Free(context, stencilCopy.memory);
        stencilCopy = {};
    }

    std::uint64_t depthBytes() const {
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return DepthSliceBytes(target.extent, d16 ? 2u : 4u);
    }

    std::uint64_t stencilBytes() const {
        return DepthSliceBytes(target.extent, 1u);
    }
    PlaneStorage mirrorPlane;
    PlaneStorage stencilMirror;

    bool CoversPlane(const StorageTexture& storage, bool stencil) const {
        const auto& resource = storage.Descriptor();
        const auto address = stencil ? target.stencilAddress : target.address;
        if (address == 0 || resource.baseAddress != address || resource.width != target.extent.width || resource.height != target.extent.height || resource.baseLevel != 0 || resource.mipCount != 1) return false;
        if (resource.dimension != TextureDimension::k2D && (resource.dimension != TextureDimension::k2DArray || resource.baseArray != 0 || resource.depthOrLastArray != 0)) return false;
        if (target.samples != 1 || (!stencil && hostFormat != target.format)) return false;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return ResolveTextureFormat(resource.format) == (stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT);
    }

    void copyPlane(PlaneStorage& plane, const std::shared_ptr<StorageTexture>& storage, bool stencil, bool toStorage, bool ownBatch = false, const std::vector<VkRect2D>* rects = nullptr) {
        if (rects != nullptr && rects->empty()) return;
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr || ownBatch, "depth surface storage copies need an active recorder");
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const std::uint32_t texelBytes = stencil ? 1u : d16 ? 2u : 4u;
        const auto bytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * texelBytes;
        const auto aspect = stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
        const bool direct = !stencil && DirectDepthCopy(context, hostFormat, aspect, storage->ImageFormat());
        const bool passes = toStorage && !stencil && target.samples == 1 && depthToStoragePass(context, hostFormat, storage->ImageFormat());
        if (!direct && !passes && plane.staging == nullptr) plane.staging = std::make_unique<DeviceBuffer>(context, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        if (recorder != nullptr) recorder->Keep(storage);
        const std::uint32_t timing = recorder != nullptr ? recorder->BeginGpuTiming(Recorder::CommandClass::Copy) : 0u;
        const VkBufferImageCopy planeRegion{0, 0, 0, {stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        std::vector<VkBufferImageCopy> planeRegions;
        std::size_t moved = bytes;
        if (rects == nullptr) {
            planeRegions.push_back(planeRegion);
        } else {
            moved = 0;
            for (const auto& rect : *rects) {
                auto region = planeRegion;
                region.bufferOffset = (static_cast<VkDeviceSize>(rect.offset.y) * target.extent.width + static_cast<VkDeviceSize>(rect.offset.x)) * texelBytes;
                region.bufferRowLength = target.extent.width;
                region.imageOffset = {rect.offset.x, rect.offset.y, 0};
                region.imageExtent = {rect.extent.width, rect.extent.height, 1};
                planeRegions.push_back(region);
                moved += static_cast<std::size_t>(rect.extent.width) * rect.extent.height * texelBytes;
            }
        }
        if (passes) {
            const std::vector<VkRect2D> whole{{{0, 0}, target.extent}};
            const auto& areas = rects != nullptr ? *rects : whole;
            const auto texels = storage->AtomicView(0, storage->Descriptor().dimension == TextureDimension::k2DArray);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            depthToStoragePassFor(context).Record(commands, depthTransferView(), texels, areas);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            depthTransferPassCount.fetch_add(1, std::memory_order_relaxed);
            if (batch) {
                batch->SubmitAndWait();
                return;
            }
            Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
            recorder->EndGpuTiming(timing, moved);
            return;
        }
        auto colorRegions = planeRegions;
        for (auto& region : colorRegions) region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        const auto count = static_cast<std::uint32_t>(planeRegions.size());
        const auto sourceImage = toStorage ? image : storage->Image();
        const auto destinationImage = toStorage ? storage->Image() : image;
        if (direct) {
            std::vector<VkImageCopy> copies;
            for (const auto& region : planeRegions) {
                const VkImageSubresourceLayers planeLayers{static_cast<VkImageAspectFlags>(aspect), 0, 0, 1};
                const VkImageSubresourceLayers colorLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copies.push_back({toStorage ? planeLayers : colorLayers, region.imageOffset, toStorage ? colorLayers : planeLayers, region.imageOffset, region.imageExtent});
            }
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, sourceImage, VK_IMAGE_LAYOUT_GENERAL, destinationImage, VK_IMAGE_LAYOUT_GENERAL, count, copies.data());
            directDepthCopies.fetch_add(1, std::memory_order_relaxed);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            if (batch) {
                batch->SubmitAndWait();
                return;
            }
            Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
            recorder->EndGpuTiming(timing, moved);
            return;
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, sourceImage, VK_IMAGE_LAYOUT_GENERAL, plane.staging->Handle(), count, toStorage ? planeRegions.data() : colorRegions.data());
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const bool throughUpload = !toStorage && !stencil && DepthUploadImage(context);
        if (throughUpload) {
            const auto& upload = depthUploadImage();
            if (recorder != nullptr) recorder->Keep(upload);
            const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
            VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.image = upload->image;
            toTransfer.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | (HasStencil(hostFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u), 0, 1, 0, 1};
            barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, plane.staging->Handle(), upload->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, count, planeRegions.data());
            VkImageMemoryBarrier toSource = toTransfer;
            toSource.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            toSource.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
            std::vector<VkImageCopy> copies;
            copies.reserve(planeRegions.size());
            for (const auto& region : planeRegions) copies.push_back({region.imageSubresource, region.imageOffset, region.imageSubresource, region.imageOffset, region.imageExtent});
            context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, upload->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destinationImage, VK_IMAGE_LAYOUT_GENERAL, count, copies.data());
            depthUploads.fetch_add(1, std::memory_order_relaxed);
        } else {
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, plane.staging->Handle(), destinationImage, VK_IMAGE_LAYOUT_GENERAL, count, toStorage ? colorRegions.data() : planeRegions.data());
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        if (batch) {
            batch->SubmitAndWait();
            return;
        }
        Recorder::CountBarriers(Recorder::CommandClass::Copy, throughUpload ? 5 : 3);
        recorder->EndGpuTiming(timing, moved);
    }

    VkImageView depthTransferView() {
        if (transferView != VK_NULL_HANDLE) return transferView;
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = hostFormat;
        info.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &transferView), "vkCreateImageView depth to storage");
        return transferView;
    }

    void release() noexcept {
        depthPlane = {};
        stencilPlane = {};
        releaseStencilCopy();
        depthUpload.reset();
        mirrorPlane = {};
        stencilMirror = {};
        textures.clear();
        if (transferView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, transferView, nullptr);
        transferView = VK_NULL_HANDLE;
        if (stencilClearFb) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, stencilClearFb, nullptr);
        if (stencilClearPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, stencilClearPass, nullptr);
        stencilClearFb = VK_NULL_HANDLE;
        stencilClearPass = VK_NULL_HANDLE;
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (owned != nullptr) owned.reset();
        else {
            if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory) MemoryAccounting::Free(context, memory);
        }
        view = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
};

struct DepthPlaneSlice {
    VkImage image = VK_NULL_HANDLE;
    std::uint64_t written = 0;
};

bool DepthPlaneReuse() {
    static const bool enabled = std::getenv("APS5_NO_DEPTH_PLANE_REUSE") == nullptr;
    return enabled;
}

class DepthPlaneCopy {
public:
    DepthPlaneCopy(const Context& context, VkExtent2D extent, VkFormat format, std::uint32_t layers) : context(context), extent(extent), format(format), layers(layers), guestLayers(layers), surfaceLayers(layers) {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.flags = layers % 6u == 0 && extent.width == extent.height ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {extent.width, extent.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = layers;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        try {
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage depth plane copy");
            owned = std::make_shared<OwnedImage>(context, image, VK_NULL_HANDLE);
            owned->Allocate(context, info, "vkAllocateMemory depth plane copy");
            memory = owned->allocation.Memory();
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, owned->allocation.Offset()), "vkBindImageMemory depth plane copy");
            imageBytes = owned->memoryBytes;
        } catch (...) {
            release();
            throw;
        }
    }
    ~DepthPlaneCopy() { release(); }
    DepthPlaneCopy(const DepthPlaneCopy&) = delete;
    DepthPlaneCopy& operator=(const DepthPlaneCopy&) = delete;

    std::shared_ptr<Texture> Refresh(std::span<const DepthPlaneSlice> slices, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType viewType, std::uint32_t baseLayer) {
        const auto requested = static_cast<std::uint32_t>(slices.size());
        Require(requested != 0 && requested <= layers, "depth plane copy slices exceed its layers");
        Require(baseLayer < requested, "a depth plane copy view starts past the slices it was given");
        const auto geometry = DescribeSurface(resource);
        Require(!geometry.levelMajor, "depth planes of a PS4 layered surface with several mip levels (stored level by level) are not implemented");
        const bool guestFill = std::any_of(slices.begin(), slices.end(), [](const auto& slice) { return slice.image == VK_NULL_HANDLE; });
        if (guestFill && (geometry.layers != requested || geometry.sliceLinearBytes < sliceBytes() || geometry.mips.empty())) {
            char text[320];
            std::snprintf(text, sizeof(text), "AGC graphics: depth plane copy geometry does not match its layers (copy %ux%u, %u layers, %llu bytes a slice; texture %ux%u guest format %u, dimension %d, tile mode %d, %u layers, %llu bytes a slice, %zu mips)", extent.width, extent.height, layers, static_cast<unsigned long long>(sliceBytes()), resource.width, resource.height, resource.format, static_cast<int>(resource.dimension), static_cast<int>(resource.tileMode), geometry.layers, static_cast<unsigned long long>(geometry.sliceLinearBytes), geometry.mips.size());
            throw std::runtime_error(text);
        }
        const auto stagingBytes = std::max<std::size_t>(static_cast<std::size_t>(geometry.sliceLinearBytes), static_cast<std::size_t>(sliceBytes())) * requested;
        std::vector<std::shared_ptr<Buffer>> uploads;
        std::vector<std::uint32_t> uploadLayers;
        std::vector<std::uint32_t> copiedLayers;
        for (std::uint32_t layer = 0; layer < requested; ++layer) {
            auto& held = guestLayers[layer];
            auto& surface = surfaceLayers[layer];
            if (slices[layer].image != VK_NULL_HANDLE) {
                held = {};
                if (slices[layer].written != 0 && surface.image == slices[layer].image && surface.written == slices[layer].written) continue;
                surface = slices[layer];
                copiedLayers.push_back(layer);
                continue;
            }
            surface = {};
            GuestLayer current{resource.baseAddress + geometry.GuestLayerOffset(layer), geometry.layerBytes, resource.tileMode, geometry.thick, GuestMemory::CollectWrites(resource.baseAddress + geometry.GuestLayerOffset(layer), static_cast<std::size_t>(geometry.layerBytes))};
            current.uncommitted = current.generation == 0 && GuestMemory::CommittedRanges(current.address, static_cast<std::size_t>(current.bytes)).empty();
            const bool unchanged = current.uncommitted ? held.uncommitted : held.generation != 0 && GuestMemory::UnchangedSince(current.address, static_cast<std::size_t>(current.bytes), held.generation);
            if (held.address == current.address && held.bytes == current.bytes && held.tileMode == current.tileMode && held.thick == current.thick && !Recorder::SnapshotWriteOverlaps(current.address, static_cast<std::size_t>(current.bytes)) && unchanged) {
                held.generation = current.generation;
                continue;
            }
            auto upload = std::make_shared<Buffer>(context, static_cast<std::size_t>(geometry.layerBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            GuestMemory::ReadCommitted(current.address, upload->Bytes().first(static_cast<std::size_t>(geometry.layerBytes)));
            uploads.push_back(std::move(upload));
            uploadLayers.push_back(layer);
            held = current;
        }
        if (!copiedLayers.empty() || !uploads.empty()) {
            if (staging == nullptr || stagingBytes > staging->Size()) staging = std::make_shared<DeviceBuffer>(context, stagingBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            record(slices, resource, geometry, copiedLayers, uploads, uploadLayers, staging);
        }
        const auto viewLayers = viewType == VK_IMAGE_VIEW_TYPE_2D ? 1u : requested - baseLayer;
        const std::array<std::uint32_t, 7> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a), static_cast<std::uint32_t>(viewType), baseLayer, viewLayers};
        auto& texture = textures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, image, format, aspect(), components, viewType, baseLayer, owned, 0, viewLayers);
            if (!DepthPlaneReuse()) texture->MarkRefreshedPerUse();
        }
        return texture;
    }

    const Context context;
    const VkExtent2D extent;
    const VkFormat format;
    const std::uint32_t layers;
    VkDeviceSize imageBytes = 0;
    std::uint64_t lastUse = 0;
    std::uint64_t lastPresent = 0;
    VkDeviceSize HeldBytes() const { return imageBytes + (staging != nullptr ? staging->Size() : 0u); }

private:
    struct GuestLayer {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
        TextureTileMode tileMode = TextureTileMode::kLinear;
        bool thick = false;
        std::uint64_t generation = 0;
        bool uncommitted = false;
    };

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::shared_ptr<OwnedImage> owned;
    std::shared_ptr<DeviceBuffer> staging;
    std::map<std::array<std::uint32_t, 7>, std::shared_ptr<Texture>> textures;
    std::vector<GuestLayer> guestLayers;
    std::vector<DepthPlaneSlice> surfaceLayers;
    bool filled = false;

    void record(std::span<const DepthPlaneSlice> slices, const GuestTextureResource& resource, const SurfaceGeometry& geometry, std::span<const std::uint32_t> copiedLayers, std::span<const std::shared_ptr<Buffer>> uploads, std::span<const std::uint32_t> uploadLayers, const std::shared_ptr<DeviceBuffer>& staging) {
        if (!uploads.empty()) {
            Require(context.detiler != nullptr, "depth plane copy requires a texture detiler");
            context.detiler->BeginBatch();
        }
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        std::vector<VkBufferImageCopy> regions;
        const auto& mip = geometry.mips.front();
        const auto region = [&](std::uint32_t layer, VkImageAspectFlags aspectMask, std::uint32_t imageLayer) {
            VkBufferImageCopy copy{};
            copy.bufferOffset = geometry.LinearLayerOffset(layer) + mip.linearOffset;
            copy.bufferRowLength = mip.pitchBytes / (format == VK_FORMAT_D32_SFLOAT ? 4u : 2u);
            copy.imageSubresource = {aspectMask, 0, imageLayer, 1};
            copy.imageExtent = {extent.width, extent.height, 1};
            return copy;
        };
        for (const auto layer : copiedLayers) {
            const auto copy = region(layer, VK_IMAGE_ASPECT_DEPTH_BIT, 0);
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, slices[layer].image, VK_IMAGE_LAYOUT_GENERAL, staging->Handle(), 1, &copy);
            regions.push_back(region(layer, aspect(), layer));
        }
        if (!uploads.empty()) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            for (std::size_t upload = 0; upload < uploads.size(); ++upload) {
                const auto layer = uploadLayers[upload];
                context.detiler->Dispatch(commands, resource.tileMode, format == VK_FORMAT_D32_SFLOAT ? 4u : 2u, uploads[upload]->Handle(), 0, staging->Handle(), geometry.LinearLayerOffset(layer), geometry.mips.front(), false, layer, geometry.thick);
                regions.push_back(region(layer, aspect(), layer));
            }
            if (recorder != nullptr) {
                for (const auto& upload : uploads) recorder->Keep(upload, upload->Bytes().size());
            }
        }
        VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = filled ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange = {aspect(), 0, 1, 0, layers};
        const VkBufferMemoryBarrier staged{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, staging->Handle(), 0, VK_WHOLE_SIZE};
        barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &staged, 1, &toTransfer);
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, staging->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
        VkImageMemoryBarrier toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        filled = true;
        if (batch) batch->SubmitAndWait();
        else {
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 3);
            recorder->Keep(staging, staging->Size());
            if (owned != nullptr) recorder->Keep(owned);
        }
    }

    VkDeviceSize sliceBytes() const {
        return static_cast<VkDeviceSize>(extent.width) * extent.height * (format == VK_FORMAT_D32_SFLOAT ? 4u : 2u);
    }

    VkImageAspectFlags aspect() const {
        return format == VK_FORMAT_R16_UINT ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    void release() noexcept {
        textures.clear();
        staging.reset();
        if (owned != nullptr) owned.reset();
        else {
            if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory) MemoryAccounting::Free(context, memory);
        }
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
};

std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat>, std::unique_ptr<DepthPlaneCopy>>& planeCopies() {
    static auto* copies = new std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat>, std::unique_ptr<DepthPlaneCopy>>();
    return *copies;
}

std::uint64_t& planeCopyClock() {
    static std::uint64_t clock = 0;
    return clock;
}

void evictPlaneCopies(const std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat>& keep) {
    static const VkDeviceSize budget = [] {
        const char* text = std::getenv("APS5_DEPTH_PLANE_COPY_MIB");
        return static_cast<VkDeviceSize>(text != nullptr ? std::strtoull(text, nullptr, 10) : 256ull) << 20u;
    }();
    if (budget == 0) return;
    auto& copies = planeCopies();
    VkDeviceSize total = 0;
    for (const auto& entry : copies) total += entry.second->HeldBytes();
    const auto present = Recorder::Presents();
    const auto ceiling = budget + std::min(budget, std::numeric_limits<VkDeviceSize>::max() - budget);
    while (total > budget) {
        const bool over = total > ceiling;
        auto oldest = copies.end();
        for (auto it = copies.begin(); it != copies.end(); ++it) {
            if (it->first == keep || (!over && it->second->lastPresent + 1u >= present)) continue;
            if (oldest == copies.end() || it->second->lastUse < oldest->second->lastUse) oldest = it;
        }
        if (oldest == copies.end()) break;
        total -= oldest->second->HeldBytes();
        copies.erase(oldest);
    }
}

struct LayeredSlice {
    DepthSurface* surface;
    std::uint32_t layer;
};

class LayeredDepthStorage {
public:
    LayeredDepthStorage(const Context& context, const GuestTextureResource& resource, const DepthTarget& base, std::uint32_t bytesPerTexel, std::uint32_t firstLayer, std::uint32_t layers) : context(context), base(base), extent(base.extent), bytesPerTexel(bytesPerTexel), firstLayer(firstLayer), layers(layers) {
        const auto bytes = SurfaceGuestBytes(resource);
        layerPitchBytes = DescribeSurface(resource).GuestLayerOffset(1);
        StorageTexture::FlushPending(resource.baseAddress, static_cast<std::size_t>(bytes), nullptr, "storage image creation", PublishScope::None);
        storage = StorageTexture::Create(context, *context.detiler, resource, 0);
        Require(storage->ImageLayers() == layers, "a layered depth storage view's image does not hold its layers");
        if (!DirectDepthCopy(context, base.format, VK_IMAGE_ASPECT_DEPTH_BIT, storage->ImageFormat())) staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(sliceBytes()) * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }

    void Copy(std::span<const LayeredSlice> slices, bool toStorage) {
        if (slices.empty()) return;
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "depth surface storage copies need an active recorder");
        const auto commands = recorder->Commands();
        recorder->Keep(storage);
        const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
        std::vector<VkBufferImageCopy> storageRegions;
        const auto region = [&](std::uint32_t layer, VkImageAspectFlags aspect, std::uint32_t imageLayer) {
            VkBufferImageCopy copy{};
            copy.bufferOffset = sliceBytes() * layer;
            copy.imageSubresource = {aspect, 0, imageLayer, 1};
            copy.imageExtent = {extent.width, extent.height, 1};
            return copy;
        };
        for (const auto& slice : slices) storageRegions.push_back(region(slice.layer, VK_IMAGE_ASPECT_COLOR_BIT, slice.layer));
        if (DirectDepthCopy(context, base.format, VK_IMAGE_ASPECT_DEPTH_BIT, storage->ImageFormat())) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            for (const auto& slice : slices) {
                const VkImageSubresourceLayers depthLayers{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
                const VkImageSubresourceLayers colorLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, slice.layer, 1};
                const VkImageCopy copy{toStorage ? depthLayers : colorLayers, {0, 0, 0}, toStorage ? colorLayers : depthLayers, {0, 0, 0}, {extent.width, extent.height, 1}};
                context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, toStorage ? slice.surface->image : storage->Image(), VK_IMAGE_LAYOUT_GENERAL, toStorage ? storage->Image() : slice.surface->image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
                directDepthCopies.fetch_add(1, std::memory_order_relaxed);
            }
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
            recorder->EndGpuTiming(timing, sliceBytes() * slices.size());
            return;
        }
        if (staging == nullptr) staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(sliceBytes()) * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto copyToBuffer = context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer");
        const auto copyToImage = context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage");
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        if (toStorage) {
            for (const auto& slice : slices) {
                const auto copy = region(slice.layer, VK_IMAGE_ASPECT_DEPTH_BIT, 0);
                copyToBuffer(commands, slice.surface->image, VK_IMAGE_LAYOUT_GENERAL, staging->Handle(), 1, &copy);
            }
        } else {
            copyToBuffer(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, staging->Handle(), static_cast<std::uint32_t>(storageRegions.size()), storageRegions.data());
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        if (toStorage) {
            copyToImage(commands, staging->Handle(), storage->Image(), VK_IMAGE_LAYOUT_GENERAL, static_cast<std::uint32_t>(storageRegions.size()), storageRegions.data());
        } else {
            for (const auto& slice : slices) {
                const auto copy = region(slice.layer, VK_IMAGE_ASPECT_DEPTH_BIT, 0);
                copyToImage(commands, staging->Handle(), slice.surface->image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
            }
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::Copy, 3);
        recorder->EndGpuTiming(timing, sliceBytes() * slices.size());
    }

    const Context context;
    const DepthTarget base;
    const VkExtent2D extent;
    const std::uint32_t bytesPerTexel;
    const std::uint32_t firstLayer;
    const std::uint32_t layers;
    std::uint64_t layerPitchBytes = 0;
    std::shared_ptr<StorageTexture> storage;

private:
    std::unique_ptr<DeviceBuffer> staging;

    VkDeviceSize sliceBytes() const {
        return static_cast<VkDeviceSize>(extent.width) * extent.height * bytesPerTexel;
    }
};

std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, int, int, std::uint32_t>, std::unique_ptr<LayeredDepthStorage>>& layeredStorages() {
    static auto* storages = new std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, int, int, std::uint32_t>, std::unique_ptr<LayeredDepthStorage>>();
    return *storages;
}

bool sameSurface(const DepthTarget& a, const DepthTarget& b) {
    return a.address == b.address && a.stencilAddress == b.stencilAddress && a.extent.width == b.extent.width && a.extent.height == b.extent.height && a.format == b.format && a.samples == b.samples && (a.samples == 1u || (a.htileAddress == b.htileAddress && a.htileStencil == b.htileStencil && a.depthSwizzle == b.depthSwizzle && a.pipeBankXor == b.pipeBankXor && a.firstSlice == b.firstSlice && a.lastSlice == b.lastSlice && a.iterate256 == b.iterate256 && a.htilePipeAligned == b.htilePipeAligned && a.htileVrsEncoding == b.htileVrsEncoding && a.htileDepthOnly == b.htileDepthOnly));
}

bool sameLayoutWithin(const DepthTarget& owner, const DepthTarget& target) {
    if (owner.samples == 1u || owner.htileAddress != 0u || owner.depthSwizzle != 24u || target.extent.width > owner.extent.width || target.extent.height > owner.extent.height) return false;
    auto resized = target;
    resized.extent = owner.extent;
    if (!sameSurface(owner, resized)) return false;
    const auto depthBytes = owner.format == VK_FORMAT_D16_UNORM || owner.format == VK_FORMAT_D16_UNORM_S8_UINT ? 2u : 4u;
    for (const auto elementBytes : {depthBytes, owner.stencilAddress != 0u ? 1u : depthBytes}) {
        const MultisampleTextureLayout held(TextureTileMode::kZ64KBX, elementBytes, owner.samples, owner.extent.width, owner.extent.height, 1u, owner.pipeBankXor);
        const MultisampleTextureLayout bound(TextureTileMode::kZ64KBX, elementBytes, owner.samples, target.extent.width, target.extent.height, 1u, owner.pipeBankXor);
        if (held.Pitch() != bound.Pitch() || held.PaddedHeight() != bound.PaddedHeight() || held.Bytes() != bound.Bytes()) return false;
    }
    return true;
}

bool holdsTarget(const DepthTarget& owner, const DepthTarget& target) {
    return sameSurface(owner, target) || sameLayoutWithin(owner, target);
}

std::recursive_mutex& surfacesMutex() {
    static std::recursive_mutex mutex;
    return mutex;
}

std::vector<std::unique_ptr<DepthSurface>>& surfaces() {
    static auto* list = new std::vector<std::unique_ptr<DepthSurface>>();
    return *list;
}

struct PlaneOwner {
    DepthSurface* surface = nullptr;
    std::uint64_t serial = 0;
};

using PlaneOwners = std::map<std::pair<VkDevice, std::uint64_t>, PlaneOwner>;

std::uint64_t planeBinds = 0;

PlaneOwners& depthOwners() {
    static auto* owners = new PlaneOwners();
    return *owners;
}

PlaneOwners& stencilOwners() {
    static auto* owners = new PlaneOwners();
    return *owners;
}

void takePlane(DepthSurface& surface, PlaneOwners& owners, std::uint64_t address, VkImageAspectFlagBits aspect, bool created, VkImageAspectFlags clearAspects) {
    auto& entry = owners[{surface.context.device, address}];
    auto* owner = entry.surface;
    if (owner != nullptr && owner->active->load(std::memory_order_acquire) && owner != &surface && owner->target.format == surface.target.format && owner->target.samples == surface.target.samples && owner->target.extent.width == surface.target.extent.width && owner->target.extent.height == surface.target.extent.height && (!created || (clearAspects & aspect) == 0)) {
        if ((owner->pendingClear & aspect) != 0) {
            if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
                surface.depthFill = owner->depthFill;
                surface.depthMark = std::max(surface.depthMark, owner->depthMark);
            } else {
                surface.stencilFill = owner->stencilFill;
                surface.stencilMark = std::max(surface.stencilMark, owner->stencilMark);
            }
            if (!created || (aspect == VK_IMAGE_ASPECT_DEPTH_BIT ? surface.depthFill.has_value() : surface.stencilFill.has_value())) surface.pendingClear |= aspect;
        } else {
            surface.CopyAspectFrom(*owner, aspect);
        }
    }
    entry = {&surface, ++planeBinds};
}

void retireStoringStale(DepthSurface& surface);

bool ownsPlane(const DepthSurface& surface) {
    const auto owns = [&](const PlaneOwners& owners) { return std::any_of(owners.begin(), owners.end(), [&](const auto& entry) { return entry.second.surface == &surface; }); };
    return owns(depthOwners()) || owns(stencilOwners());
}

void takePlanes(DepthSurface& surface, bool created, VkImageAspectFlags clearAspects) {
    std::array<DepthSurface*, 2> displaced{};
    const auto previous = [&](PlaneOwners& owners, std::uint64_t address) {
        const auto found = owners.find({surface.context.device, address});
        return found == owners.end() || found->second.surface == &surface ? nullptr : found->second.surface;
    };
    displaced[0] = previous(depthOwners(), surface.target.address);
    if (surface.target.stencilAddress != 0) displaced[1] = previous(stencilOwners(), surface.target.stencilAddress);
    takePlane(surface, depthOwners(), surface.target.address, VK_IMAGE_ASPECT_DEPTH_BIT, created, clearAspects);
    if (surface.target.stencilAddress != 0) takePlane(surface, stencilOwners(), surface.target.stencilAddress, VK_IMAGE_ASPECT_STENCIL_BIT, created, clearAspects);
    for (std::size_t index = 0; index < displaced.size(); ++index) {
        auto* owner = displaced[index];
        if (owner == nullptr || (index == 1 && owner == displaced[0]) || owner->retired || owner->target.samples != 1u || ownsPlane(*owner)) continue;
        retireStoringStale(*owner);
    }
}

}
}
