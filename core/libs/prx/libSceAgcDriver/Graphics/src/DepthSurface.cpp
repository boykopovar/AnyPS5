#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <limits>
#include <shared_mutex>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace AgcDriver::Graphics {
namespace {

std::uint64_t depthWrites = 0;

Recorder* RecorderFor(const Context& context) {
    auto* recorder = Recorder::Active();
    return recorder != nullptr && recorder->Device() == context.device ? recorder : nullptr;
}

bool storageFits(const DepthTarget& target, const GuestTextureResource& descriptor) {
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto format = ResolveTextureFormat(descriptor.format);
    const bool sized = d16 ? (format == VK_FORMAT_R16_UINT || format == VK_FORMAT_R16_UNORM || format == VK_FORMAT_R16_SINT || format == VK_FORMAT_R16_SNORM || format == VK_FORMAT_R16_SFLOAT) : (format == VK_FORMAT_R32_SFLOAT || format == VK_FORMAT_R32_UINT || format == VK_FORMAT_R32_SINT);
    return sized && descriptor.width == target.extent.width && descriptor.height == target.extent.height && descriptor.mipCount == 1 && (descriptor.dimension == TextureDimension::k2D || descriptor.dimension == TextureDimension::k2DArray);
}

struct DepthGuard {
    VkDevice device;
    std::uint64_t address;
    std::size_t bytes;
    std::shared_ptr<std::atomic<bool>> active;
    std::vector<GuestMemory::UnchangedQuery> live;
};

std::shared_mutex& depthGuardMutex() {
    static std::shared_mutex mutex;
    return mutex;
}

std::vector<DepthGuard>& depthGuards() {
    static std::vector<DepthGuard> ranges;
    return ranges;
}

std::atomic<bool>& anyDepthGuard() {
    static std::atomic<bool> any{false};
    return any;
}

struct DepthSamplingState {
    std::array<GuestMemory::UnchangedQuery, 2> initial{};
    std::string refusal;
};

bool htileProvesClear(std::uint32_t word, bool depthOnly, float clearDepth) {
    if ((word & 15u) == 15u) return false;
    const auto depth = depthOnly ? word : word & 0xfffff00fu;
    if (depth == 0u) return clearDepth == 0.0f;
    if (depth == (depthOnly ? 0xfffffff0u : 0xfffc0000u)) return clearDepth == 1.0f;
    return false;
}

DepthSamplingState prepareDepthSampling(const Context& context, const DepthTarget& target) {
    DepthSamplingState state;
    const auto refuse = [&](const char* reason) {
        state.refusal = reason;
        return state;
    };
    if (target.samples != 2u && target.samples != 4u && target.samples != 8u) return refuse("unsupported native depth sample count");
    if (target.format != VK_FORMAT_D16_UNORM && target.format != VK_FORMAT_D32_SFLOAT) return refuse("native multisample stencil or depth format import is not implemented");
    if (target.depthSwizzle != 24u || target.firstSlice != 0u || target.lastSlice != 0u) return refuse("native multisample depth arrays or non-Z_X layout are not implemented");
    if ((target.pipeBankXor & ~0xcf00u) != 0u) return refuse("unsupported native multisample depth XOR");
    if (target.address == 0u || (target.address & 65535u) != 0u) return refuse("native multisample depth base is not aligned");
    if (target.stencilAddress != 0u || target.htileStencil) return refuse("native multisample stencil import is not implemented");
    if (target.htileAddress == 0u || (target.htileAddress & 255u) != 0u || !target.iterate256 || !target.htilePipeAligned || target.htileVrsEncoding != 0u) return refuse("native multisample depth initial import requires pipe-aligned ITERATE_256 HTILE without VRS");
    if (target.clearDepth != 0.0f && target.clearDepth != 1.0f) return refuse("native multisample depth initial clear value is not implemented");
    state.initial = {{{target.address, static_cast<std::size_t>(MultisampledDepthBytes_nid_no_patch(target.extent, target.format == VK_FORMAT_D16_UNORM ? 2u : 4u, target.samples)), 0u}, {target.htileAddress, static_cast<std::size_t>(HtileMetadataBytes(target.extent)), 0u}}};
    for (const auto& range : state.initial) {
        if (range.address > std::numeric_limits<std::uint64_t>::max() - range.bytes) return refuse("native depth import range overflows");
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) || !GuestMemory::Watched(range.address, range.bytes)) return refuse("native multisample depth import requires watched accessible guest memory");
    }
    const auto& depth = state.initial[0];
    const auto& htile = state.initial[1];
    if (depth.address + depth.bytes > htile.address && htile.address + htile.bytes > depth.address) return refuse("native depth and HTILE initial ranges overlap");
    for (auto& range : state.initial) {
        if (auto* recorder = RecorderFor(context)) recorder->SyncThrough(range.address, range.bytes);
        StorageTexture::FlushPending(range.address, range.bytes);
        GuestMemory::FlushGpuWrites(range.address, range.bytes);
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) || !GuestMemory::Watched(range.address, range.bytes)) return refuse("native multisample depth import requires watched accessible guest memory");
        range.generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
        if (range.generation == 0u) return refuse("native multisample depth import requires tracked generations");
    }
    std::vector<std::uint32_t> metadata(htile.bytes / sizeof(std::uint32_t));
    GuestMemory::Read(target.htileAddress, std::as_writable_bytes(std::span(metadata)), 256u);
    for (const auto word : metadata) {
        if (!htileProvesClear(word, target.htileDepthOnly, target.clearDepth)) return refuse("native multisample depth initial HTILE does not prove its clear value");
    }
    for (const auto& range : state.initial) GuestMemory::CollectWritesUncached(range.address, range.bytes);
    if (!GuestMemory::UnchangedSinceAll(state.initial)) return refuse("guest writes raced native multisample depth import");
    return state;
}

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
                VkImageFormatProperties supported{};
                const auto result = context.imageFormatProperties(context.physical, hostFormat, info.imageType, info.tiling, info.usage | VK_IMAGE_USAGE_SAMPLED_BIT, info.flags, &supported);
                if (result != VK_ERROR_FORMAT_NOT_SUPPORTED) Check(result, "vkGetPhysicalDeviceImageFormatProperties sampled depth");
                sampleable = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 && result == VK_SUCCESS && (supported.sampleCounts & info.samples) != 0 && supported.maxMipLevels >= 1u && supported.maxArrayLayers >= 1u && supported.maxExtent.depth >= 1u && info.extent.width <= supported.maxExtent.width && info.extent.height <= supported.maxExtent.height;
            }
            if (sampleable) info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage depth");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory depth target");
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
            const VkClearDepthStencilValue clear{target.clearDepth, target.clearStencil};
            context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &toGeneral.subresourceRange);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            if (batch) batch->SubmitAndWait();
            else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        } catch (...) {
            release();
            throw;
        }
    }

    ~DepthSurface() { release(); }
    DepthSurface(const DepthSurface&) = delete;
    DepthSurface& operator=(const DepthSurface&) = delete;

    void CheckCurrent() {
        if (target.samples == 1u) return;
        const auto clearLease = clearGeneration == 0u ? GuestAllocations::Lease{} : GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
        Require(active->load(std::memory_order_acquire), "the native multisample depth owner was released");
        Require(sampling.refusal.empty(), sampling.refusal);
        for (const auto& range : sampling.initial) {
            Require(GuestMemory::Accessible(reinterpret_cast<const void*>(range.address), range.bytes) && GuestMemory::Watched(range.address, range.bytes), "native multisample depth requires watched accessible guest memory");
            if (const auto* recorder = RecorderFor(context)) Require(!recorder->PendingWriteOverlaps(range.address, range.bytes), "external GPU writes over native multisample depth are not implemented");
            GuestMemory::CollectWritesUncached(range.address, range.bytes);
        }
        if (clearGeneration != 0u) {
            const auto& depth = sampling.initial[0];
            const auto& metadata = sampling.initial[1];
            Require(GuestMemory::UnchangedSince(depth.address, depth.bytes, depth.generation), "guest writes over native multisample depth are not implemented");
            Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, clearGeneration), "CPU writes raced native multisample depth clear");
            Require(GuestMemory::StoredAllSince_nid_no_patch(metadata.address, metadata.bytes, clearGeneration), "native multisample depth clear has no complete tracked HTILE store");
            Require(clearDepth == 0.0f || clearDepth == 1.0f, "native multisample depth clear value is not implemented");
            auto current = sampling.initial;
            current[1].generation = GuestMemory::CollectWritesUncached(metadata.address, metadata.bytes);
            std::vector<std::uint32_t> words(metadata.bytes / sizeof(std::uint32_t));
            std::memcpy(words.data(), reinterpret_cast<const void*>(metadata.address), metadata.bytes);
            for (const auto word : words) Require(htileProvesClear(word, target.htileDepthOnly, clearDepth), "native multisample depth completed HTILE does not prove its clear value");
            for (const auto& range : current) GuestMemory::CollectWritesUncached(range.address, range.bytes);
            Require(GuestMemory::UnchangedSinceAll(current), "guest writes raced native multisample depth clear");
            Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, clearGeneration), "CPU writes raced native multisample depth clear");
            RecordClear(VK_IMAGE_ASPECT_DEPTH_BIT);
            sampling.initial = current;
            clearGeneration = 0u;
            pendingClear = 0u;
        }
        Require(GuestMemory::UnchangedSinceAll(sampling.initial), "guest writes over native multisample depth are not implemented");
    }

    void CheckResource(const GuestTextureResource& resource) const {
        if (target.samples == 1u) return;
        Require(resource.baseAddress == target.address && resource.width == target.extent.width && resource.height == target.extent.height && resource.samples == target.samples && resource.format == (target.format == VK_FORMAT_D16_UNORM ? 7u : 22u), "native multisample depth sampled identity or format mismatch");
        Require(resource.dimension == TextureDimension::k2DMsaa && resource.tileMode == TextureTileMode::kZ64KBX && resource.depthOrLastArray == 0u && resource.baseArray == 0u && resource.baseLevel == 0u && resource.lastLevel == 0u && resource.mipCount == 1u && resource.allocatedMipCount == 1u && resource.minLod == 0u, "native multisample depth sampled array, mip or layout is not implemented");
        Require(resource.pipeBankXor == target.pipeBankXor && resource.dccAddress == target.htileAddress && resource.iterate256 == target.iterate256 && resource.dccPipeAligned == target.htilePipeAligned && !resource.dccAlphaOnMsb, "native multisample depth sampled metadata or XOR mismatch");
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
        written = ++depthWrites;
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        const VkImageSubresourceRange range{aspects, 0, 1, 0, 1};
        constexpr VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, access, VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkClearDepthStencilValue clear{clearDepth, clearStencil};
        context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, access);
        if (batch) batch->SubmitAndWait();
        else {
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            recorder->Keep(owned);
        }
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

    std::shared_ptr<Texture> Sampled(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, bool drawn = false) {
        CheckCurrent();
        CheckResource(resource);
        std::array<std::uint32_t, 12> key{};
        std::copy_n(words.begin(), std::min<std::size_t>(words.size(), 8), key.begin());
        key[8] = components.r;
        key[9] = components.g;
        key[10] = components.b;
        key[11] = components.a;
        if (const auto found = textures.find(key); found != textures.end()) return found->second;
        const bool multisampled = target.samples != 1;
        Require(!multisampled || sampleable, "the device cannot sample the native depth target's format and sample count");
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const auto format = ResolveTextureFormat(resource.format);
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        if (multisampled) {
            Require(!stencil && !depthBits && format == expected, "native multisampled depth sampling needs a numeric depth-plane format");
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
        const auto viewType = resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        auto texture = std::make_shared<Texture>(context, image, hostFormat, stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, components, viewType, 0, owned);
        if (multisampled) {
            Require(samplingActive, "native multisample depth sampling requires its creation-time owner");
            texture->MarkRefreshedPerUse();
        }
        textures.emplace(key, texture);
        return texture;
    }

    void CopyAspectFrom(const DepthSurface& source, VkImageAspectFlagBits aspect) {
        pendingClear &= ~static_cast<VkImageAspectFlags>(aspect);
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

    std::uint64_t written = ++depthWrites;
    bool retired = false;

    bool Overlaps(std::uint64_t address, std::uint64_t bytes) const {
        const auto end = address + bytes;
        if (address < target.address + depthBytes() && target.address < end) return true;
        return target.stencilAddress != 0 && address < target.stencilAddress + stencilBytes() && target.stencilAddress < end;
    }

    void NoteMemory() {
        depthMemory = GuestMemory::CollectWrites(target.address, static_cast<std::size_t>(depthBytes()));
        stencilMemory = target.stencilAddress != 0 ? GuestMemory::CollectWrites(target.stencilAddress, static_cast<std::size_t>(stencilBytes())) : 0;
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

    bool StorageViewMatches(std::span<const std::uint32_t> words, const GuestTextureResource& resource) const {
        if (resource.width != target.extent.width || resource.height != target.extent.height) return false;
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        return ResolveTextureFormat(resource.format) == expected || depthBits;
    }

    void RequireStorable(const GuestTextureResource& resource, std::uint32_t mip) const {
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        Require(target.samples == 1, "storage image access to a multisampled depth surface is not implemented");
        if (!stencil && hostFormat != target.format) {
            char text[224];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image access to the depth plane of depth surface 0x%llx (vk format %d) stored as vk format %d is not implemented", static_cast<unsigned long long>(target.address), static_cast<int>(target.format), static_cast<int>(hostFormat));
            throw std::runtime_error(text);
        }
        if (mip != 0 || resource.baseLevel != 0) {
            char text[224];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image access to level %u of the %s plane of depth surface 0x%llx is not implemented", mip, stencil ? "stencil" : "depth", static_cast<unsigned long long>(target.address));
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
        RequireStorable(resource, mip);
        ApplyFastClear();
        auto& plane = stencil ? stencilPlane : depthPlane;
        if (plane.storage == nullptr || plane.storage->Descriptor().format != resource.format || plane.storage->Descriptor().tileMode != resource.tileMode || plane.storage->Descriptor().baseAddress != resource.baseAddress || plane.storage->Descriptor().dimension != resource.dimension) {
            std::erase_if(plane.replaced, [](const auto& old) { return old.expired(); });
            if (plane.storage != nullptr) plane.replaced.push_back(plane.storage);
            plane.storage = StorageTexture::Create(context, *context.detiler, resource, 0);
        }
        copyPlane(plane, plane.storage, stencil, true);
        return plane.storage;
    }

    bool StoreBack(const StorageTexture& candidate) {
        for (const bool stencil : {false, true}) {
            auto& plane = stencil ? stencilPlane : depthPlane;
            if (plane.storage.get() == &candidate) {
                copyPlane(plane, plane.storage, stencil, false);
                written = ++depthWrites;
                return true;
            }
            if (std::none_of(plane.replaced.begin(), plane.replaced.end(), [&](const auto& old) { return old.lock().get() == &candidate; })) continue;
            char text[288];
            std::snprintf(text, sizeof(text), "AGC graphics: storage image writes to the %s plane of depth surface 0x%llx through a view that another view of the plane (guest format %u, tile mode %d) replaced are not implemented", stencil ? "stencil" : "depth", static_cast<unsigned long long>(target.address), plane.storage->Descriptor().format, static_cast<int>(plane.storage->Descriptor().tileMode));
            throw std::runtime_error(text);
        }
        return false;
    }

    bool StoreBackCovering(const StorageTexture& candidate) {
        for (const bool stencil : {false, true}) {
            if (!CoversPlane(candidate, stencil)) continue;
            auto& plane = stencil ? stencilPlane : depthPlane;
            const auto source = std::const_pointer_cast<StorageTexture>(candidate.shared_from_this());
            copyPlane(plane, source, stencil, false);
            written = ++depthWrites;
            return true;
        }
        return false;
    }

    void TakeStorage() {
        if (target.samples != 1 || hostFormat != target.format) return;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto bytes = DepthSliceBytes(target.extent, d16 ? 2u : 4u);
        auto storage = StorageTexture::FindPending(target.address, bytes);
        if (storage == nullptr) storage = StorageTexture::FindLive(target.address, bytes);
        if (storage == nullptr || !storageFits(target, storage->Descriptor())) return;
        storage->Refresh();
        if (depthPlane.staging == nullptr) depthPlane.staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(target.extent.width) * target.extent.height * (d16 ? 2u : 4u), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        if (recorder != nullptr) recorder->Keep(storage);
        VkBufferImageCopy depthRegion{0, 0, 0, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        VkBufferImageCopy colorRegion = depthRegion;
        colorRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, storage->Image(), VK_IMAGE_LAYOUT_GENERAL, depthPlane.staging->Handle(), 1, &colorRegion);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, depthPlane.staging->Handle(), image, VK_IMAGE_LAYOUT_GENERAL, 1, &depthRegion);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Copy, 3);
    }

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
    std::uint64_t clearGeneration = 0u;
    std::shared_ptr<OwnedImage> owned;

private:
    struct PlaneStorage {
        std::shared_ptr<StorageTexture> storage;
        std::vector<std::weak_ptr<StorageTexture>> replaced;
        std::unique_ptr<DeviceBuffer> staging;
    };
    struct StencilCopy {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        std::unique_ptr<DeviceBuffer> staging;
        std::map<std::array<std::uint32_t, 12>, std::shared_ptr<Texture>> textures;
        std::uint64_t written = 0;
        bool filled = false;
    };
    std::map<std::array<std::uint32_t, 12>, std::shared_ptr<Texture>> textures;
    PlaneStorage depthPlane;
    PlaneStorage stencilPlane;
    std::uint64_t depthMemory = 0;
    std::uint64_t stencilMemory = 0;
    StencilCopy stencilCopy;

    std::shared_ptr<Texture> unormStencil(const std::array<std::uint32_t, 12>& key, VkComponentMapping components, VkImageViewType viewType, bool drawn) {
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
                Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &stencilCopy.memory), "vkAllocateMemory stencil copy");
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
        if (stencilCopy.memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, stencilCopy.memory, nullptr);
        stencilCopy = {};
    }

    std::uint64_t depthBytes() const {
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return DepthSliceBytes(target.extent, d16 ? 2u : 4u);
    }

    std::uint64_t stencilBytes() const {
        return DepthSliceBytes(target.extent, 1u);
    }

    bool CoversPlane(const StorageTexture& storage, bool stencil) const {
        const auto& resource = storage.Descriptor();
        const auto address = stencil ? target.stencilAddress : target.address;
        if (address == 0 || resource.baseAddress != address || resource.width != target.extent.width || resource.height != target.extent.height || resource.baseLevel != 0 || resource.mipCount != 1) return false;
        if (resource.dimension != TextureDimension::k2D && (resource.dimension != TextureDimension::k2DArray || resource.baseArray != 0 || resource.depthOrLastArray != 0)) return false;
        if (target.samples != 1 || (!stencil && hostFormat != target.format)) return false;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        return ResolveTextureFormat(resource.format) == (stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT);
    }

    void copyPlane(PlaneStorage& plane, const std::shared_ptr<StorageTexture>& storage, bool stencil, bool toStorage) {
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "depth surface storage copies need an active recorder");
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto bytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * (stencil ? 1u : d16 ? 2u : 4u);
        if (plane.staging == nullptr) plane.staging = std::make_unique<DeviceBuffer>(context, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder->Commands();
        recorder->Keep(storage);
        const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
        VkBufferImageCopy planeRegion{0, 0, 0, {stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        VkBufferImageCopy colorRegion = planeRegion;
        colorRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        const auto sourceImage = toStorage ? image : storage->Image();
        const auto destinationImage = toStorage ? storage->Image() : image;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, sourceImage, VK_IMAGE_LAYOUT_GENERAL, plane.staging->Handle(), 1, toStorage ? &planeRegion : &colorRegion);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, plane.staging->Handle(), destinationImage, VK_IMAGE_LAYOUT_GENERAL, 1, toStorage ? &colorRegion : &planeRegion);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::Copy, 3);
        recorder->EndGpuTiming(timing, bytes);
    }

    void release() noexcept {
        depthPlane = {};
        stencilPlane = {};
        releaseStencilCopy();
        textures.clear();
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (owned != nullptr) owned.reset();
        else {
            if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
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

class DepthPlaneCopy {
public:
    DepthPlaneCopy(const Context& context, VkExtent2D extent, VkFormat format, std::uint32_t layers) : context(context), extent(extent), format(format), layers(layers), guestLayers(layers), surfaceLayers(layers) {
        this->context.bufferPool.reset();
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
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory depth plane copy");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory depth plane copy");
            stagingSize = static_cast<std::size_t>(sliceBytes()) * layers;
            staging = std::make_unique<DeviceBuffer>(context, stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        } catch (...) {
            release();
            throw;
        }
    }
    ~DepthPlaneCopy() { release(); }
    DepthPlaneCopy(const DepthPlaneCopy&) = delete;
    DepthPlaneCopy& operator=(const DepthPlaneCopy&) = delete;

    std::shared_ptr<Texture> Refresh(std::span<const DepthPlaneSlice> slices, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType viewType, std::uint32_t baseLayer) {
        Require(slices.size() == layers, "depth plane copy slices do not match its layers");
        const auto geometry = DescribeSurface(resource);
        const bool guestFill = std::any_of(slices.begin(), slices.end(), [](const auto& slice) { return slice.image == VK_NULL_HANDLE; });
        if (guestFill && (geometry.layers != layers || geometry.sliceLinearBytes < sliceBytes() || geometry.mips.empty())) {
            char text[320];
            std::snprintf(text, sizeof(text), "AGC graphics: depth plane copy geometry does not match its layers (copy %ux%u, %u layers, %llu bytes a slice; texture %ux%u guest format %u, dimension %d, tile mode %u, %u layers, %llu bytes a slice, %zu mips)", extent.width, extent.height, layers, static_cast<unsigned long long>(sliceBytes()), resource.width, resource.height, resource.format, static_cast<int>(resource.dimension), resource.tileMode, geometry.layers, static_cast<unsigned long long>(geometry.sliceLinearBytes), geometry.mips.size());
            throw std::runtime_error(text);
        }
        const auto stagingBytes = static_cast<std::size_t>(geometry.sliceLinearBytes) * layers;
        if (stagingBytes > stagingSize) {
            retiredStaging.push_back(std::move(staging));
            staging = std::make_unique<DeviceBuffer>(context, stagingBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            stagingSize = stagingBytes;
        }
        std::vector<std::shared_ptr<Buffer>> uploads;
        std::vector<std::uint32_t> uploadLayers;
        std::vector<std::uint32_t> copiedLayers;
        for (std::uint32_t layer = 0; layer < layers; ++layer) {
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
            const GuestLayer current{resource.baseAddress + geometry.GuestLayerOffset(layer), geometry.layerBytes, resource.tileMode, geometry.thick, GuestMemory::CollectWrites(resource.baseAddress + geometry.GuestLayerOffset(layer), static_cast<std::size_t>(geometry.layerBytes))};
            if (held.generation != 0 && held.address == current.address && held.bytes == current.bytes && held.tileMode == current.tileMode && held.thick == current.thick && !Recorder::SnapshotWriteOverlaps(current.address, static_cast<std::size_t>(current.bytes)) && GuestMemory::UnchangedSince(current.address, static_cast<std::size_t>(current.bytes), held.generation)) {
                held.generation = current.generation;
                continue;
            }
            auto upload = std::make_shared<Buffer>(context, static_cast<std::size_t>(geometry.layerBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            GuestMemory::ReadCommitted(current.address, upload->Bytes().first(static_cast<std::size_t>(geometry.layerBytes)));
            uploads.push_back(std::move(upload));
            uploadLayers.push_back(layer);
            held = current;
        }
        if (!copiedLayers.empty() || !uploads.empty()) record(slices, resource, geometry, copiedLayers, uploads, uploadLayers);
        const std::array<std::uint32_t, 6> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a), static_cast<std::uint32_t>(viewType), baseLayer};
        auto& texture = textures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, image, format, aspect(), components, viewType, baseLayer);
            texture->MarkRefreshedPerUse();
        }
        return texture;
    }

    const Context context;
    const VkExtent2D extent;
    const VkFormat format;
    const std::uint32_t layers;

private:
    struct GuestLayer {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
        TextureTileMode tileMode = TextureTileMode::kLinear;
        bool thick = false;
        std::uint64_t generation = 0;
    };

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::unique_ptr<DeviceBuffer> staging;
    std::size_t stagingSize = 0;
    std::vector<std::unique_ptr<DeviceBuffer>> retiredStaging;
    std::map<std::array<std::uint32_t, 6>, std::shared_ptr<Texture>> textures;
    std::vector<GuestLayer> guestLayers;
    std::vector<DepthPlaneSlice> surfaceLayers;
    bool filled = false;

    void record(std::span<const DepthPlaneSlice> slices, const GuestTextureResource& resource, const SurfaceGeometry& geometry, std::span<const std::uint32_t> copiedLayers, std::span<const std::shared_ptr<Buffer>> uploads, std::span<const std::uint32_t> uploadLayers) {
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
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 3);
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
        if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
};

std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat, std::uint32_t>, std::unique_ptr<DepthPlaneCopy>>& planeCopies() {
    static auto* copies = new std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat, std::uint32_t>, std::unique_ptr<DepthPlaneCopy>>();
    return *copies;
}

struct LayeredSlice {
    DepthSurface* surface;
    std::uint32_t layer;
};

class LayeredDepthStorage {
public:
    LayeredDepthStorage(const Context& context, const GuestTextureResource& resource, const DepthTarget& base, std::uint32_t bytesPerTexel, std::uint32_t firstLayer, std::uint32_t layers) : context(context), base(base), extent(base.extent), bytesPerTexel(bytesPerTexel), firstLayer(firstLayer), layers(layers) {
        const auto bytes = DescribeSurface(resource).guestBytes;
        StorageTexture::FlushPending(resource.baseAddress, static_cast<std::size_t>(bytes), nullptr, "storage image creation", PublishScope::None);
        storage = StorageTexture::Create(context, *context.detiler, resource, 0);
        Require(storage->ImageLayers() == layers, "a layered depth storage view's image does not hold its layers");
        staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(sliceBytes()) * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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

std::mutex& surfacesMutex() {
    static std::mutex mutex;
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

void takePlane(DepthSurface& surface, PlaneOwners& owners, std::uint64_t address, VkImageAspectFlagBits aspect) {
    auto& entry = owners[{surface.context.device, address}];
    auto* owner = entry.surface;
    if (owner != nullptr && owner->active->load(std::memory_order_acquire) && owner != &surface && owner->target.format == surface.target.format && owner->target.samples == surface.target.samples && owner->target.extent.width == surface.target.extent.width && owner->target.extent.height == surface.target.extent.height) {
        if ((owner->pendingClear & aspect) != 0) surface.pendingClear |= aspect;
        else surface.CopyAspectFrom(*owner, aspect);
    }
    entry = {&surface, ++planeBinds};
}

void takePlanes(DepthSurface& surface) {
    takePlane(surface, depthOwners(), surface.target.address, VK_IMAGE_ASPECT_DEPTH_BIT);
    if (surface.target.stencilAddress != 0) takePlane(surface, stencilOwners(), surface.target.stencilAddress, VK_IMAGE_ASPECT_STENCIL_BIT);
}

DepthSurface* depthSlice(VkDevice device, std::uint64_t address, const DepthTarget& base) {
    if (const auto found = depthOwners().find({device, address}); found != depthOwners().end()) {
        const auto& target = found->second.surface->target;
        if (found->second.surface->active->load(std::memory_order_acquire) && target.extent.width == base.extent.width && target.extent.height == base.extent.height && target.format == base.format) return found->second.surface;
    }
    const auto& list = surfaces();
    const auto slice = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && surface->context.device == device && !surface->retired && surface->target.address == address && surface->target.extent.width == base.extent.width && surface->target.extent.height == base.extent.height && surface->target.format == base.format; });
    return slice == list.rend() ? nullptr : slice->get();
}

DepthSurface* planeSurface(VkDevice device, std::uint64_t address) {
    const auto depth = depthOwners().find({device, address});
    const auto stencil = stencilOwners().find({device, address});
    const bool hasDepth = depth != depthOwners().end() && depth->second.surface->active->load(std::memory_order_acquire);
    const bool hasStencil = stencil != stencilOwners().end() && stencil->second.surface->active->load(std::memory_order_acquire);
    if (hasDepth && (!hasStencil || depth->second.serial > stencil->second.serial)) return depth->second.surface;
    if (hasStencil) return stencil->second.surface;
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
        return surface->active->load(std::memory_order_acquire) && surface->context.device == device && !surface->retired && (surface->target.address == address || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == address));
    });
    return found == list.rend() ? nullptr : found->get();
}

void retire(DepthSurface& surface) {
    surface.retired = true;
    for (auto* owners : {&depthOwners(), &stencilOwners()}) {
        std::erase_if(*owners, [&](const auto& entry) { return entry.second.surface == &surface; });
    }
}

std::map<std::uint64_t, std::size_t>& clearedHtiles() {
    static auto* cleared = new std::map<std::uint64_t, std::size_t>();
    return *cleared;
}

bool takeClearedHtile(const DepthTarget& target) {
    if (target.htileAddress == 0) return false;
    auto& cleared = clearedHtiles();
    const auto found = std::find_if(cleared.begin(), cleared.end(), [&](const auto& entry) { return HtileFillCovers(target.htileAddress, target.extent, entry.first, entry.second); });
    if (found == cleared.end()) return false;
    cleared.erase(found);
    return true;
}

const DepthSurface* drawnDepth = nullptr;

std::atomic<std::uint64_t> surfacesSerial{0};

std::uint32_t expandedHtileWord(std::uint32_t word, bool stencilInHtile, VkImageAspectFlags cleared) {
    if (!stencilInHtile) return (cleared & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 ? 0xfffc000fu : word;
    if ((cleared & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) word = (word & 0x3f0u) | 0xfffff00fu;
    if ((cleared & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) word |= 0x3f0u;
    return word;
}

std::mutex& htileReadsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<std::uint64_t, std::uint64_t>& htileReads() {
    static auto* reads = new std::map<std::uint64_t, std::uint64_t>();
    return *reads;
}

void forgetHtileReads(std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(htileReadsMutex());
    auto& reads = htileReads();
    reads.erase(reads.lower_bound(address), reads.lower_bound(address + bytes));
}

bool RoutesToNativeDepthOwner(const DepthTarget& target) {
    if (target.samples != 1u) return true;
    if (target.htileAddress == 0 || !anyDepthGuard().load(std::memory_order_acquire)) return false;
    const auto bytes = static_cast<std::uint64_t>(HtileMetadataBytes(target.extent));
    std::shared_lock lock(depthGuardMutex());
    return std::ranges::any_of(depthGuards(), [&](const auto& range) { return range.active->load(std::memory_order_acquire) && target.htileAddress < range.address + range.bytes && range.address < target.htileAddress + bytes; });
}

VkImageAspectFlags storeBackClearedHtile(const Context& context, const DepthTarget& target) {
    if (target.htileAddress == 0) return 0;
    const auto bytes = static_cast<std::size_t>(HtileMetadataBytes(target.extent));
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(target.htileAddress), bytes, true)) return 0;
    const auto collected = GuestMemory::CollectWrites(target.htileAddress, bytes);
    auto* recorder = RecorderFor(context);
    if (recorder == nullptr || !recorder->PendingWriteOverlaps(target.htileAddress, bytes)) {
        std::lock_guard lock(htileReadsMutex());
        const auto found = htileReads().find(target.htileAddress);
        if (found != htileReads().end() && found->second != 0 && GuestMemory::UnchangedSince(target.htileAddress, bytes, found->second)) return 0;
    }
    std::vector<std::uint32_t> words(bytes / 4u);
    GuestMemory::Read(target.htileAddress, std::as_writable_bytes(std::span(words)), 4);
    const bool uniform = std::all_of(words.begin(), words.end(), [&](std::uint32_t word) { return word == words.front(); });
    VkImageAspectFlags cleared = target.firstSlice == 0 || uniform ? VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u) : 0u;
    for (const auto word : words) cleared &= HtileFillClears(word, target.htileStencil);
    auto generation = collected;
    if (cleared != 0) {
        for (auto& word : words) word = expandedHtileWord(word, target.htileStencil, cleared);
        GuestMemory::Write(target.htileAddress, std::as_bytes(std::span(words)), 4);
        generation = GuestMemory::CollectWrites(target.htileAddress, bytes);
    }
    std::lock_guard lock(htileReadsMutex());
    htileReads()[target.htileAddress] = generation;
    return cleared;
}
}

std::uint64_t DepthSurfacesSerial() {
    return surfacesSerial.load(std::memory_order_acquire);
}

std::uint64_t DepthSliceBytes(VkExtent2D extent, std::uint32_t bytesPerTexel) {
    const std::uint32_t blockWidth = bytesPerTexel == 4 ? 128u : 256u;
    const std::uint32_t blockHeight = bytesPerTexel == 1 ? 256u : 128u;
    const auto width = static_cast<std::uint64_t>((extent.width + blockWidth - 1) / blockWidth * blockWidth);
    const auto height = static_cast<std::uint64_t>((extent.height + blockHeight - 1) / blockHeight * blockHeight);
    return width * height * bytesPerTexel;
}

std::uint64_t MultisampledDepthBytes_nid_no_patch(VkExtent2D extent, std::uint32_t bytesPerTexel, std::uint32_t samples) {
    Require(extent.width != 0u && extent.width <= 16384u && extent.height != 0u && extent.height <= 16384u, "invalid multisample depth extent");
    Require((bytesPerTexel == 1u || bytesPerTexel == 2u || bytesPerTexel == 4u) && (samples == 2u || samples == 4u || samples == 8u), "unsupported multisample depth footprint");
    const auto sampleBits = std::countr_zero(samples);
    const auto pixelBits = 16u - std::countr_zero(bytesPerTexel) - sampleBits;
    const auto widthBits = (pixelBits + ((sampleBits & 1u) == 0u ? 1u : 0u)) / 2u;
    const auto blockWidth = 1u << widthBits;
    const auto blockHeight = 1u << (pixelBits - widthBits);
    const auto width = static_cast<std::uint64_t>((extent.width + blockWidth - 1u) / blockWidth * blockWidth);
    const auto height = static_cast<std::uint64_t>((extent.height + blockHeight - 1u) / blockHeight * blockHeight);
    return width * height * bytesPerTexel * samples;
}

void RequireNoResidentDepthRead_nid_no_patch(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0u || !anyDepthGuard().load(std::memory_order_acquire)) return;
    Require(address <= std::numeric_limits<std::uint64_t>::max() - bytes, "native depth read range overflows");
    std::shared_lock lock(depthGuardMutex());
    for (const auto& range : depthGuards()) Require(address >= range.address + range.bytes || range.address >= address + bytes, "sample-preserving guest publication of native multisample depth is not implemented");
}

bool HasResidentDepthGuards_nid_no_patch() {
    return anyDepthGuard().load(std::memory_order_acquire);
}

void RetireUnmappedDepth_nid_no_patch(std::uintptr_t address, std::size_t bytes) {
    if (bytes == 0u || !HasResidentDepthGuards_nid_no_patch()) return;
    Require(address <= std::numeric_limits<std::uint64_t>::max() - bytes, "native depth release range overflows");
    const auto end = address + bytes;
    std::unique_lock lock(depthGuardMutex());
    for (auto& range : depthGuards()) {
        std::vector<GuestMemory::UnchangedQuery> remaining;
        remaining.reserve(range.live.size() + 1u);
        for (const auto& part : range.live) {
            const auto finish = part.address + part.bytes;
            const auto first = std::max<std::uint64_t>(address, part.address);
            const auto last = std::min<std::uint64_t>(end, finish);
            if (first >= last) remaining.push_back(part);
            else {
                if (part.address < first) remaining.push_back({part.address, static_cast<std::size_t>(first - part.address), 0u});
                if (last < finish) remaining.push_back({last, static_cast<std::size_t>(finish - last), 0u});
            }
        }
        range.live = std::move(remaining);
    }
    for (const auto& range : depthGuards()) {
        if (std::ranges::all_of(depthGuards(), [&](const auto& part) { return part.active != range.active || part.live.empty(); })) range.active->store(false, std::memory_order_release);
    }
    std::erase_if(depthGuards(), [](const auto& range) { return !range.active->load(std::memory_order_acquire); });
    anyDepthGuard().store(!depthGuards().empty(), std::memory_order_release);
}

bool SampleLocationsCompatibleDepth(const Context& context, VkFormat format, std::uint32_t samples) {
    if (samples == 0 || (context.sampleLocationSampleCounts & samples) == 0) return false;
    VkImageFormatProperties properties{};
    const auto result = context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT, &properties);
    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) return false;
    Check(result, "vkGetPhysicalDeviceImageFormatProperties sample locations depth");
    return (properties.sampleCounts & samples) != 0;
}

VkFormat HostDepthFormat(const Context& context, VkFormat format) {
    if (format != VK_FORMAT_D16_UNORM_S8_UINT) return format;
    static std::mutex mutex;
    static std::map<VkPhysicalDevice, VkFormat> chosen;
    std::lock_guard lock(mutex);
    if (const auto found = chosen.find(context.physical); found != chosen.end()) return found->second;
    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    auto host = format;
    for (const auto candidate : {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
        VkFormatProperties properties{};
        context.formatProperties(context.physical, candidate, &properties);
        if ((properties.optimalTilingFeatures & required) != required) continue;
        host = candidate;
        break;
    }
    chosen.emplace(context.physical, host);
    return host;
}

float HostDepthBiasConstant(const Context& context, VkFormat format, float constant) {
    const auto host = HostDepthFormat(context, format);
    if (constant == 0.0f || host == format) return constant;
    Require(host == VK_FORMAT_D24_UNORM_S8_UINT, "a constant depth bias on a 16-bit depth target with stencil that the device stores as 32-bit float depth is not implemented");
    return constant * 256.0f;
}

VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target) {
    const auto htileCleared = RoutesToNativeDepthOwner(target) ? VkImageAspectFlags{0} : storeBackClearedHtile(context, target);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::unique_lock lock(surfacesMutex());
    const bool cleared = takeClearedHtile(target);
    DepthSurface* surface = nullptr;
    for (const auto& candidate : surfaces()) {
        if (candidate->active->load(std::memory_order_acquire) && candidate->context.device == context.device && sameSurface(candidate->target, target)) {
            surface = candidate.get();
            break;
        }
    }
    const bool created = surface == nullptr;
    GuestAllocations::Lease creationLease;
    if (created) {
        for (const auto& surface : surfaces()) {
            if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->context.device != context.device) continue;
            const auto bytes = target.samples == 1u ? DepthSliceBytes(target.extent, target.format == VK_FORMAT_D16_UNORM ? 2u : 4u) : MultisampledDepthBytes_nid_no_patch(target.extent, target.format == VK_FORMAT_D16_UNORM ? 2u : 4u, target.samples);
            Require(target.address <= std::numeric_limits<std::uint64_t>::max() - bytes && target.htileAddress <= std::numeric_limits<std::uint64_t>::max() - HtileMetadataBytes(target.extent), "native depth attachment range overflows");
            for (const auto& range : surface->sampling.initial) {
                Require(target.address >= range.address + range.bytes || range.address >= target.address + bytes, "native multisample depth attachment layout or metadata changed");
                if (target.stencilAddress != 0u) {
                    const auto stencilBytes = target.samples == 1u ? DepthSliceBytes(target.extent, 1u) : MultisampledDepthBytes_nid_no_patch(target.extent, 1u, target.samples);
                    Require(target.stencilAddress <= std::numeric_limits<std::uint64_t>::max() - stencilBytes && (target.stencilAddress >= range.address + range.bytes || range.address >= target.stencilAddress + stencilBytes), "native multisample stencil attachment aliases a resident depth surface");
                }
                if (target.htileAddress != 0u) Require(target.htileAddress >= range.address + range.bytes || range.address >= target.htileAddress + HtileMetadataBytes(target.extent), "native multisample depth attachment layout or metadata changed");
            }
        }
        lock.unlock();
        auto sampling = target.samples == 1u ? DepthSamplingState{} : prepareDepthSampling(context, target);
        creationLease = target.samples == 1u || !sampling.refusal.empty() ? GuestAllocations::Lease{} : GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
        lock.lock();
        surfacesSerial.fetch_add(1, std::memory_order_release);
        surfaces().push_back(std::make_unique<DepthSurface>(context, target, std::move(sampling)));
        auto& fresh = *surfaces().back();
        if (target.samples != 1u && fresh.sampleable && fresh.sampling.refusal.empty()) {
            std::unique_lock guardLock(depthGuardMutex());
            for (const auto& range : fresh.sampling.initial) depthGuards().push_back({context.device, range.address, range.bytes, fresh.active, {{range.address, range.bytes, 0u}}});
            anyDepthGuard().store(true, std::memory_order_release);
            InstallResidentImageReadGuard_nid_no_patch();
            fresh.samplingActive = true;
        }
        surface = surfaces().back().get();
    }
    surface->retired = false;
    const auto owner = depthOwners().find({context.device, target.address});
    const bool inherits = owner != depthOwners().end() && owner->second.surface->active->load(std::memory_order_acquire) && !owner->second.surface->retired;
    surface->clearDepth = target.clearDepth;
    surface->clearStencil = target.clearStencil;
    takePlanes(*surface);
    if (created && !inherits && target.htileAddress == 0) surface->TakeStorage();
    if (cleared && (!created || inherits)) {
        if (target.samples != 1u) surface->sampling.refusal = "native multisample depth fast-clear transition is not implemented";
        surface->pendingClear |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    surface->pendingClear |= htileCleared;
    surface->ApplyFastClear();
    if (!created || inherits) surface->written = ++depthWrites;
    surface->NoteMemory();
    drawnDepth = surface;
    return surface->view;
}

std::uint64_t HtileDepthClearAddress(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads) {
    static constexpr std::array<std::uint32_t, 17> htileMaskKernel{0xd7460000u, 0x04010c06u, 0x34000084u, 0xdc388000u, 0x04020000u, 0xbf8c3f70u, 0xd7710008u, 0x00120805u, 0xd7710009u, 0x00120a05u, 0xd771000au, 0x00120c05u, 0xd771000bu, 0x00120e05u, 0xdc788000u, 0x00020800u, 0xbf810000u};
    if (userData.size() < 6 || numThreads[0] != 64 || numThreads[1] != 1 || numThreads[2] != 1) return 0;
    if (code.size() < htileMaskKernel.size() || !std::equal(htileMaskKernel.begin(), htileMaskKernel.end(), code.begin())) return 0;
    if ((userData[5] & 0xfu) != 0 || (userData[4] & 0xfu) != 0) return 0;
    return userData[2] | (static_cast<std::uint64_t>(userData[3]) << 32u);
}

std::optional<MemoryValueFill> MatchMemoryValueFill(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads, const std::array<std::uint32_t, 3>& groups) {
    static constexpr std::array<std::uint32_t, 9> kernel{0xd7460000u, 0x04010c08u, 0xf4201a82u, 0xfa000000u, 0xbf8cc07fu, 0x7e02026au, 0xe0102000u, 0x80000100u, 0xbf810000u};
    if (userData.size() < 8 || numThreads != std::array<std::uint32_t, 3>{64u, 1u, 1u} || groups[1] != 1u || groups[2] != 1u) return std::nullopt;
    if (code.size() < kernel.size() || !std::equal(kernel.begin(), kernel.end(), code.begin())) return std::nullopt;
    const auto stride = (userData[1] >> 16u) & 0x3fffu;
    const bool swizzled = ((userData[1] >> 31u) & 1u) != 0;
    const bool addTid = ((userData[3] >> 23u) & 1u) != 0;
    const auto type = userData[3] >> 30u;
    const auto format = (userData[3] >> 12u) & 0x7fu;
    const auto valueStride = (userData[5] >> 16u) & 0x3fffu;
    const auto valueBytes = valueStride == 0 ? static_cast<std::uint64_t>(userData[6]) : static_cast<std::uint64_t>(userData[6]) * valueStride;
    if (type != 0 || stride != 4 || swizzled || addTid || format < 20u || format > 22u || valueBytes < 4u) return std::nullopt;
    const auto dwords = std::min<std::uint64_t>(static_cast<std::uint64_t>(groups[0]) * 64u, userData[2]);
    const auto address = userData[0] | (static_cast<std::uint64_t>(userData[1] & 0xffffu) << 32u);
    const auto valueAddress = userData[4] | (static_cast<std::uint64_t>(userData[5] & 0xffffu) << 32u);
    if (dwords == 0 || valueAddress == 0 || (valueAddress & 3u) != 0) return std::nullopt;
    return MemoryValueFill{address, static_cast<std::size_t>(dwords * 4u), valueAddress};
}

void NoteMemoryValueFill(const MemoryValueFill& fill) {
    std::uint32_t value = 0;
    GuestMemory::Read(fill.valueAddress, std::as_writable_bytes(std::span(&value, 1)), 4);
    NoteDepthMetadataFill(fill.address, fill.bytes, value);
}

std::size_t HtileDepthClearBytes_nid_no_patch(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, std::span<const std::uint32_t> packet, std::uint64_t indirectArguments) {
    if (userData.size() != 6u || packet.size() < 5u || indirectArguments != 0u || compute.PartialGroups() || !compute.groupIdEnable[0] || compute.threadIdComponentCount == 0u || compute.threadIdComponentCount > 3u) return 0u;
    if (packet[1] == 0u || packet[1] > 0x400000u || packet[2] != 1u || packet[3] != 1u || (packet[4] & 0x23u) != 1u) return 0u;
    const auto address = HtileDepthClearAddress(code, userData, compute.numThreads);
    if (address == 0u || (address & 15u) != 0u) return 0u;
    const auto bytes = static_cast<std::uint64_t>(packet[1]) * 1024u;
    return address <= std::numeric_limits<std::uint64_t>::max() - bytes ? static_cast<std::size_t>(bytes) : 0u;
}

void NoteHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes, std::size_t nativeBytes) {
    std::lock_guard lock(surfacesMutex());
    forgetHtileReads(htileAddress, bytes);
    bool existing = false;
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        if (surface->samplingActive && surface->target.htileAddress == htileAddress) {
            const auto& metadata = surface->sampling.initial[1];
            if (nativeBytes != bytes || nativeBytes < metadata.bytes || !GuestMemory::Accessible(reinterpret_cast<const void*>(metadata.address), metadata.bytes) || !GuestMemory::Watched(metadata.address, metadata.bytes)) surface->sampling.refusal = "native multisample depth incomplete or unproven fast-clear transition is not implemented";
            else if (surface->clearGeneration != 0u) surface->sampling.refusal = "overlapping native multisample depth clear transitions are not implemented";
            else surface->clearGeneration = GuestMemory::CollectWritesUncached(metadata.address, metadata.bytes);
            existing = true;
            continue;
        }
        if (!HtileFillCovers(surface->target.htileAddress, surface->target.extent, htileAddress, bytes)) continue;
        if (surface->target.samples != 1u) surface->sampling.refusal = "native multisample depth fast-clear transition is not implemented";
        surface->pendingClear |= VK_IMAGE_ASPECT_DEPTH_BIT;
        existing = true;
    }
    if (!existing) clearedHtiles()[htileAddress] = bytes;
}

void ClearDepthSurfaces(VkDevice device) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    drawnDepth = nullptr;
    surfacesSerial.fetch_add(1, std::memory_order_release);
    std::erase_if(depthOwners(), [&](const auto& entry) { return entry.first.first == device; });
    std::erase_if(stencilOwners(), [&](const auto& entry) { return entry.first.first == device; });
    std::erase_if(surfaces(), [&](const auto& surface) { return surface->context.device == device; });
    std::erase_if(planeCopies(), [&](const auto& entry) { return std::get<0>(entry.first) == device; });
    std::erase_if(layeredStorages(), [&](const auto& entry) { return std::get<0>(entry.first) == device; });
    std::unique_lock guardLock(depthGuardMutex());
    std::erase_if(depthGuards(), [&](const auto& range) { return range.device == device; });
    anyDepthGuard().store(!depthGuards().empty(), std::memory_order_release);
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components) {
    const auto held = [&](const auto& surface) {
        return surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && !surface->retired && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    };
    {
        std::lock_guard lock(surfacesMutex());
        if (std::none_of(surfaces().begin(), surfaces().end(), held)) return nullptr;
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    auto* found = planeSurface(context.device, resource.baseAddress);
    if (found == nullptr) return nullptr;
    const auto& base = found->target;
    const bool stencil = base.stencilAddress != 0 && resource.baseAddress == base.stencilAddress;
    if (stencil && BytesPerElement(resource.format) != 1u) return nullptr;
    if (base.samples != 1u || resource.dimension == TextureDimension::k2DMsaa) {
        Require(base.samples > 1u, "multisample reader overlaps a single-sample depth attachment");
        auto* recorder = RecorderFor(context);
        Require(recorder != nullptr, "a native multisampled depth target is sampled only by recorded draws and dispatches");
        found->CheckCurrent();
        found->CheckResource(resource);
        Require(found->pendingClear == 0u, "native multisample depth fast-clear transition is not implemented");
        RecordMemoryBarrier(context, recorder->Commands(), VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        return found->Sampled(words, resource, components);
    }
    if (resource.width != base.extent.width || resource.height != base.extent.height) return nullptr;
    const bool d16 = base.format == VK_FORMAT_D16_UNORM || base.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto format = ResolveTextureFormat(resource.format);
    const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
    const bool integer = d16 && format == VK_FORMAT_R16_UINT && !depthBits;
    if (!stencil && format != (d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT) && !depthBits && !integer) return nullptr;
    const bool cube = resource.dimension == TextureDimension::kCube;
    const bool layered = cube || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray != 0);
    const bool sampled = stencil || (!layered && !integer);
    const auto copyFormat = integer ? VK_FORMAT_R16_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
    const bool planes = (format == copyFormat || depthBits) && resource.width == base.extent.width && resource.height == base.extent.height && resource.baseLevel == 0 && resource.lastLevel == 0 && !(resource.baseArray != 0 && (cube || !layered || resource.baseArray > resource.depthOrLastArray)) && !(cube && resource.depthOrLastArray != 0 && resource.depthOrLastArray != 5) && (resource.dimension == TextureDimension::k2D || resource.dimension == TextureDimension::k2DArray || cube);
    if (!(sampled ? found->SampledAccepts(words, resource) : planes) && found->OverwrittenInMemory()) {
        retire(*found);
        return nullptr;
    }
    found->ApplyFastClear();
    if (sampled) return found->Sampled(words, resource, components, found == drawnDepth);
    Require(found->hostFormat == base.format, "sampling several slices or an integer view of a depth plane that the device stores in another depth format is not implemented");
    const auto layers = cube ? 6u : layered ? resource.depthOrLastArray + 1u : 1u;
    if (!planes) {
        char text[320];
        std::snprintf(text, sizeof(text), "AGC graphics: sampling the depth planes of depth surface 0x%llx (%ux%u, vk format %d) as a %ux%u texture of guest format %u (vk %d), dimension %d, %u slices, levels %u-%u, base slice %u is not implemented", static_cast<unsigned long long>(base.address), base.extent.width, base.extent.height, static_cast<int>(base.format), resource.width, resource.height, resource.format, static_cast<int>(format), static_cast<int>(resource.dimension), layers, resource.baseLevel, resource.lastLevel, resource.baseArray);
        throw std::runtime_error(text);
    }
    std::vector<DepthPlaneSlice> slices;
    const auto geometry = DescribeSurface(resource);
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        const auto address = base.address + geometry.GuestLayerOffset(layer);
        auto* slice = depthSlice(context.device, address, base);
        if (slice != nullptr) slice->ApplyFastClear();
        if (slice == nullptr) slices.push_back({});
        else slices.push_back({slice->image, slice == drawnDepth ? 0u : slice->written});
    }
    const auto imageFormat = integer ? VK_FORMAT_R16_UINT : d16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
    auto& copy = planeCopies()[{context.device, base.address, base.extent.width, base.extent.height, imageFormat, layers}];
    if (copy == nullptr) {
        copy = std::make_unique<DepthPlaneCopy>(context, base.extent, imageFormat, layers);
    }
    return copy->Refresh(slices, resource, components, cube || resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, layered && !cube ? resource.baseArray : 0u);
}

bool DepthSurfaceHolds(const Context& context, const GuestTextureResource& resource, const Texture* texture) {
    if ((resource.dimension != TextureDimension::k2D && resource.dimension != TextureDimension::k2DMsaa) || resource.baseArray != 0) return false;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    const auto address = resource.baseAddress;
    std::lock_guard lock(surfacesMutex());
    auto* found = planeSurface(context.device, address);
    if (found == nullptr || !found->Holds(texture)) return false;
    found->CheckCurrent();
    found->CheckResource(resource);
    return true;
}

void RequireNoNativeDepthFeedback_nid_no_patch(const Context& context, const DepthTarget& target, std::span<const std::shared_ptr<Texture>> sampled) {
    if (target.samples == 1u || sampled.empty()) return;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        if (surface->context.device != context.device || !sameSurface(surface->target, target)) continue;
        Require(std::none_of(sampled.begin(), sampled.end(), [&](const auto& texture) { return texture != nullptr && surface->Holds(texture.get()); }), "sampling a native multisampled depth target the draw attaches is not implemented");
    }
}

VkImageAspectFlags HtileFillClears(std::uint32_t pattern, bool stencilInHtile) {
    VkImageAspectFlags cleared = (pattern & 0xfu) == 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0u;
    if (stencilInHtile && ((pattern >> 8u) & 0x3u) == 0) cleared |= VK_IMAGE_ASPECT_STENCIL_BIT;
    return cleared;
}

std::uint64_t HtileMetadataBytes(VkExtent2D extent) {
    const auto blocks = static_cast<std::uint64_t>((extent.width + 1023u) / 1024u) * ((extent.height + 511u) / 512u);
    return blocks * 32768u;
}

bool HtileFillCovers(std::uint64_t htile, VkExtent2D extent, std::uint64_t address, std::size_t bytes) {
    if (htile == 0 || htile < address || htile >= address + bytes) return false;
    return address + bytes - htile >= HtileMetadataBytes(extent);
}

void NoteDepthMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    std::lock_guard lock(surfacesMutex());
    forgetHtileReads(address, bytes);
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        const auto& target = surface->target;
        if (target.samples != 1u && target.htileAddress != 0u && address < target.htileAddress + HtileMetadataBytes(target.extent) && target.htileAddress < address + bytes) surface->sampling.refusal = "native multisample depth metadata fill transition is not implemented";
        if (!HtileFillCovers(target.htileAddress, target.extent, address, bytes)) continue;
        const VkImageAspectFlags written = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
        surface->pendingClear = (surface->pendingClear & ~written) | HtileFillClears(pattern, target.htileStencil);
    }
}

bool DepthSurfaceAt(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource) {
    std::lock_guard lock(surfacesMutex());
    const auto* found = planeSurface(context.device, resource.baseAddress);
    if (found == nullptr) return false;
    const auto& target = found->target;
    if (resource.width != target.extent.width || resource.height != target.extent.height) return false;
    const bool stencil = target.stencilAddress != 0 && target.stencilAddress == resource.baseAddress;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
    const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
    return ResolveTextureFormat(resource.format) == expected || depthBits;
}

namespace {

std::vector<LayeredSlice> layeredSlices(const Context& context, const LayeredDepthStorage& storage) {
    const auto& base = storage.base;
    std::vector<LayeredSlice> slices;
    const auto& list = surfaces();
    for (std::uint32_t layer = storage.firstLayer; layer < storage.layers; ++layer) {
        const auto address = base.address + static_cast<std::uint64_t>(layer) * DepthSliceBytes(base.extent, storage.bytesPerTexel);
        const auto slice = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) { return surface->context.device == context.device && !surface->retired && surface->target.address == address && surface->target.extent.width == base.extent.width && surface->target.extent.height == base.extent.height && surface->target.format == base.format && surface->target.samples == 1; });
        if (slice != list.rend()) slices.push_back({slice->get(), layer});
    }
    return slices;
}

std::shared_ptr<StorageTexture> layeredDepthStorage(const Context& context, DepthSurface& surface, const GuestTextureResource& resource, std::uint32_t mip) {
    const auto& target = surface.target;
    const auto layers = resource.depthOrLastArray + 1u;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto bytesPerTexel = d16 ? 2u : 4u;
    surface.RequireStorable(resource, mip);
    const auto geometry = DescribeSurface(resource);
    if (layers > 1 && geometry.GuestLayerOffset(1) != DepthSliceBytes(target.extent, bytesPerTexel)) {
        char text[256];
        std::snprintf(text, sizeof(text), "AGC graphics: storage image access to the depth planes of depth surface 0x%llx with a layer pitch of 0x%llx bytes, not the depth slice size 0x%llx, is not implemented", static_cast<unsigned long long>(target.address), static_cast<unsigned long long>(geometry.GuestLayerOffset(1)), static_cast<unsigned long long>(DepthSliceBytes(target.extent, bytesPerTexel)));
        throw std::runtime_error(text);
    }
    auto& entry = layeredStorages()[{context.device, resource.baseAddress, resource.width, resource.height, layers, resource.format, static_cast<int>(resource.tileMode), static_cast<int>(resource.dimension), resource.baseArray}];
    if (entry == nullptr) entry = std::make_unique<LayeredDepthStorage>(context, resource, target, bytesPerTexel, resource.baseArray, layers);
    else static_cast<void>(entry->storage->Refresh());
    const auto slices = layeredSlices(context, *entry);
    for (const auto& slice : slices) slice.surface->ApplyFastClear();
    entry->Copy(slices, true);
    return entry->storage;
}

}

std::shared_ptr<StorageTexture> DepthSurfaceStorage(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip) {
    std::lock_guard lock(surfacesMutex());
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
        return surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && !surface->retired && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    });
    if (found == list.rend()) return nullptr;
    auto& surface = **found;
    const bool stencil = surface.target.stencilAddress != 0 && surface.target.stencilAddress == resource.baseAddress;
    if (stencil || resource.dimension != TextureDimension::k2DArray || (resource.baseArray == 0 && resource.depthOrLastArray == 0)) return surface.Storage(words, resource, mip);
    if (!surface.StorageViewMatches(words, resource)) return nullptr;
    return layeredDepthStorage(context, surface, resource, mip);
}

bool DepthSurfaceStoreBack(const Context& context, const StorageTexture& storage) {
    std::lock_guard lock(surfacesMutex());
    for (auto& [key, entry] : layeredStorages()) {
        if (std::get<0>(key) != context.device || entry->storage.get() != &storage) continue;
        const auto slices = layeredSlices(context, *entry);
        entry->Copy(slices, false);
        for (const auto& slice : slices) slice.surface->written = ++depthWrites;
        return true;
    }
    for (const auto& surface : surfaces()) {
        if (surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && surface->StoreBack(storage)) return true;
    }
    for (auto surface = surfaces().rbegin(); surface != surfaces().rend(); ++surface) {
        if ((*surface)->context.device == context.device && (*surface)->StoreBackCovering(storage)) return true;
    }
    return false;
}

bool DepthSurfaceAt(std::uint64_t address) {
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && !surface->retired && (surface->target.address == address || surface->target.stencilAddress == address); });
}

void RetireDepthSurfaces(VkDevice device, std::uint64_t address, std::uint64_t bytes) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device == device && !surface->retired && surface->Overlaps(address, bytes)) retire(*surface);
    }
}

}
