#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
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
std::uint64_t mirrorEpoch = 0;

std::pair<std::uint32_t, std::uint32_t> DepthBlockExtent(std::uint32_t bytesPerTexel) {
    return {bytesPerTexel == 4 ? 128u : 256u, bytesPerTexel == 1 ? 256u : 128u};
}

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

std::atomic<bool> directDepthCopy{std::getenv("APS5_NO_DIRECT_DEPTH_COPY") == nullptr};
std::atomic<std::uint64_t> directDepthCopies{0};

std::uint32_t aspectCopyBytes(VkFormat format, VkImageAspectFlagBits aspect) {
    if (aspect != VK_IMAGE_ASPECT_DEPTH_BIT) return 0;
    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT) return 4;
    if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D16_UNORM_S8_UINT) return 2;
    return 0;
}

bool singleComponent(VkFormat format, std::uint32_t bytes) {
    switch (bytes) {
        case 2: return format == VK_FORMAT_R16_SFLOAT || format == VK_FORMAT_R16_UNORM || format == VK_FORMAT_R16_SNORM || format == VK_FORMAT_R16_UINT || format == VK_FORMAT_R16_SINT;
        case 4: return format == VK_FORMAT_R32_SFLOAT || format == VK_FORMAT_R32_SINT || format == VK_FORMAT_R32_UINT;
        default: return false;
    }
}

struct DepthGuard {
    VkDevice device;
    std::uint64_t address;
    std::size_t bytes;
    std::shared_ptr<std::atomic<bool>> active;
    std::vector<GuestMemory::UnchangedQuery> live;
    std::shared_ptr<std::atomic<bool>> hostWritten;
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

    void Refuse(const char* reason) {
        if (refusal.empty()) refusal = reason;
    }
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
    if (target.htileAddress == 0u || (target.htileAddress & 255u) != 0u || !target.htilePipeAligned || target.htileVrsEncoding != 0u) return refuse("native multisample depth initial import requires pipe-aligned HTILE without VRS");
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

std::atomic<bool> depthPlaneClearEnabled{true};

enum DepthClearDecline : std::size_t { DeclineLayers, DeclineFormat, DeclineValue, DeclineColor, DeclinePending, DeclineCoverage, DeclineSize, DeclineCount };
static_assert(DeclineCount <= 8, "DepthClearStatistics::declined holds eight reasons");

struct DepthClearCounters {
    std::atomic<std::uint64_t> matched{0};
    std::atomic<std::uint64_t> taken{0};
    std::atomic<std::uint64_t> copiesSkipped{0};
    std::atomic<std::uint64_t> clears{0};
    std::atomic<std::uint64_t> bytesSkipped{0};
    std::array<std::atomic<std::uint64_t>, 8> declined{};
};

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
                const auto sampledUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                const auto shadowResult = context.imageFormatProperties(context.physical, hostFormat, info.imageType, info.tiling, sampledUsage, 0, &shadowProperties);
                if (shadowResult != VK_ERROR_FORMAT_NOT_SUPPORTED) Check(shadowResult, "vkGetPhysicalDeviceImageFormatProperties sampled depth shadow");
                const auto directResult = context.imageFormatProperties(context.physical, hostFormat, info.imageType, info.tiling, info.usage | VK_IMAGE_USAGE_SAMPLED_BIT, info.flags, &directProperties);
                if (directResult != VK_ERROR_FORMAT_NOT_SUPPORTED) Check(directResult, "vkGetPhysicalDeviceImageFormatProperties sampled depth attachment");
                const auto supports = [&](VkResult result, const VkImageFormatProperties& supported) {
                    return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 && result == VK_SUCCESS && (supported.sampleCounts & info.samples) != 0 && supported.maxMipLevels >= 1u && supported.maxArrayLayers >= 1u && supported.maxExtent.depth >= 1u && info.extent.width <= supported.maxExtent.width && info.extent.height <= supported.maxExtent.height;
                };
                useSampleShadow = supports(shadowResult, shadowProperties);
                const bool directSampleable = supports(directResult, directProperties);
                sampleable = useSampleShadow || directSampleable;
                if (!useSampleShadow && directSampleable) info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
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
        Require(!clearPending, "native multisample depth clear has not been recorded");
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
        const VkImageSubresourceRange range{aspects, 0, 1, 0, 1};
        constexpr VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, access, VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkClearDepthStencilValue clear{depth, stencil};
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

    std::shared_ptr<Texture> Sampled(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, bool drawn = false, VkImageViewType multisampleViewType = VK_IMAGE_VIEW_TYPE_2D, bool copySource = false) {
        if (!copySource) CheckCurrent();
        CheckResource(resource);
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
        if (multisampled && useSampleShadow) {
            if (sampleShadow == nullptr) createSampleShadow();
            refreshSampleShadow(drawn);
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

    void ForgetClear(VkImageAspectFlags aspects) {
        pendingClear &= ~aspects;
        if ((aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) depthFill.reset();
        if ((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) stencilFill.reset();
    }

    void NoteHtileClear(VkImageAspectFlags aspects) {
        ForgetClear(aspects);
        pendingClear |= aspects;
    }

    void NotePlaneFill(VkImageAspectFlagBits aspect, float depth, std::uint8_t stencil) {
        pendingClear |= aspect;
        if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) depthFill = depth;
        else stencilFill = stencil;
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
        hostWritten->store(true, std::memory_order_release);
    }

    std::uint64_t written = ++depthWrites;
    std::shared_ptr<std::atomic<bool>> hostWritten = std::make_shared<std::atomic<bool>>(false);
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

    bool StoreBackCovering(const StorageTexture& candidate) {
        for (const bool stencil : {false, true}) {
            if (!CoversPlane(candidate, stencil)) continue;
            auto& plane = stencil ? stencilPlane : depthPlane;
            const auto source = std::const_pointer_cast<StorageTexture>(candidate.shared_from_this());
            copyPlane(plane, source, stencil, false);
            written = ++depthWrites;
            NoteHostWrite();
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
    }

    bool AnyStale() const {
        return staleCount != 0;
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
        const auto bytes = static_cast<std::size_t>(DescribeSurface(resource).guestBytes);
        SizeUnits(bytes);
        const auto collected = GuestMemory::CollectWrites(resource.baseAddress, bytes);
        generation = GuestMemory::TrackerGeneration();
        std::vector<bool> changed(unitGeneration.size(), false);
        if (collected == 0u) {
            for (std::size_t unit = 0; unit < unitGeneration.size(); ++unit) {
                const auto offset = unit * unitBytes;
                const auto address = resource.baseAddress + offset;
                const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(unitBytes, bytes - offset));
                if (GuestMemory::CollectWritesUncached(address, length) != 0u) {
                    changed[unit] = unitGeneration[unit] != 0u && GuestMemory::WrittenSince(address, length, unitGeneration[unit]);
                    continue;
                }
                constexpr std::uint64_t pageBytes = 4096u;
                for (std::size_t page = 0; page < length;) {
                    const auto pageAddress = address + page;
                    const auto pageLength = static_cast<std::size_t>(std::min<std::uint64_t>(length - page, pageBytes - pageAddress % pageBytes));
                    if (GuestMemory::CollectWritesUncached(pageAddress, pageLength) != 0u && unitGeneration[unit] != 0u && GuestMemory::WrittenSince(pageAddress, pageLength, unitGeneration[unit])) changed[unit] = true;
                    page += pageLength;
                }
            }
            generation = GuestMemory::TrackerGeneration();
            return changed;
        }
        if (unitGeneration.size() == 1u) {
            changed[0] = GuestMemory::WrittenSince(resource.baseAddress, bytes, unitGeneration[0]);
            return changed;
        }
        const auto oldest = *std::min_element(unitGeneration.begin(), unitGeneration.end());
        if (oldest != 0u && !GuestMemory::WrittenSince(resource.baseAddress, bytes, oldest)) return changed;
        std::vector<std::uint8_t> stamped(unitGeneration.size(), GuestMemory::BlockUnchanged);
        if (!GuestMemory::ChangedBlocks(resource.baseAddress, bytes, unitGeneration, stamped)) return changed;
        for (std::size_t unit = 0; unit < changed.size(); ++unit) changed[unit] = unitGeneration[unit] != 0u && stamped[unit] == GuestMemory::BlockWritten;
        return changed;
    }

    void SyncFromMemory(const GuestTextureResource& resource) {
        std::uint64_t generation = 0;
        const auto changed = WrittenUnits(resource, generation);
        if ((pendingClear & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 || !anyUnit(changed)) {
            unitGeneration.assign(unitGeneration.size(), generation);
            return;
        }
        ClearStale(changed);
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
        const auto bytes = static_cast<std::size_t>(DescribeSurface(resource).guestBytes);
        GuestMemory::CollectWrites(resource.baseAddress, bytes);
        const auto generation = GuestMemory::TrackerGeneration();
        const bool changed = stencilGeneration != 0u && GuestMemory::WrittenSince(resource.baseAddress, bytes, stencilGeneration);
        stencilGeneration = generation;
        if (!changed || (pendingClear & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) return;
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
    }

    bool stencilStale = false;
    std::uint64_t stencilGeneration = 0;

    std::vector<bool> staleUnits;
    std::size_t staleCount = 0;
    std::vector<std::uint64_t> unitGeneration;
    std::uint64_t unitBytes = 0;
    bool syncing = false;
    std::uint64_t scannedEpoch = 0;

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
    std::shared_ptr<OwnedImage> owned;
    std::shared_ptr<OwnedImage> sampleShadow;
    bool useSampleShadow = false;
    std::uint64_t sampleShadowWritten = 0u;

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
        std::map<std::array<std::uint32_t, 13>, std::shared_ptr<Texture>> textures;
        std::uint64_t written = 0;
        bool filled = false;
    };
    std::map<std::array<std::uint32_t, 13>, std::shared_ptr<Texture>> textures;
    PlaneStorage depthPlane;
    PlaneStorage stencilPlane;
    std::uint64_t depthMemory = 0;
    std::uint64_t stencilMemory = 0;
    StencilCopy stencilCopy;

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
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage sampled depth shadow");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(MemoryAccounting::Allocate(context, allocation, "sampled depth shadow", &memory), "vkAllocateMemory sampled depth shadow");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory sampled depth shadow");
            sampleShadow = std::make_shared<OwnedImage>(context, image, memory);
            image = VK_NULL_HANDLE;
            memory = VK_NULL_HANDLE;
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
            barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
            context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            if (batch) batch->SubmitAndWait();
            else Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        } catch (...) {
            if (image != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory != VK_NULL_HANDLE) MemoryAccounting::Free(context, memory);
            throw;
        }
    }

    void refreshSampleShadow(bool drawn) {
        Require(sampleShadow != nullptr, "native multisample depth shadow was not created");
        if (!drawn && sampleShadowWritten == written) return;
        auto* recorder = RecorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        if (recorder != nullptr) recorder->Keep(sampleShadow);
        constexpr VkAccessFlags accesses = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, accesses, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
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
        if (!direct && plane.staging == nullptr) plane.staging = std::make_unique<DeviceBuffer>(context, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, plane.staging->Handle(), destinationImage, VK_IMAGE_LAYOUT_GENERAL, count, toStorage ? colorRegions.data() : planeRegions.data());
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        if (batch) {
            batch->SubmitAndWait();
            return;
        }
        Recorder::CountBarriers(Recorder::CommandClass::Copy, 3);
        recorder->EndGpuTiming(timing, moved);
    }

    void release() noexcept {
        depthPlane = {};
        stencilPlane = {};
        releaseStencilCopy();
        mirrorPlane = {};
        stencilMirror = {};
        textures.clear();
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
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            owned->Allocate(context, allocation, requirements.memoryTypeBits, "vkAllocateMemory depth plane copy");
            memory = owned->memory;
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory depth plane copy");
            imageBytes = requirements.size;
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
            std::snprintf(text, sizeof(text), "AGC graphics: depth plane copy geometry does not match its layers (copy %ux%u, %u layers, %llu bytes a slice; texture %ux%u guest format %u, dimension %d, tile mode %u, %u layers, %llu bytes a slice, %zu mips)", extent.width, extent.height, layers, static_cast<unsigned long long>(sliceBytes()), resource.width, resource.height, resource.format, static_cast<int>(resource.dimension), resource.tileMode, geometry.layers, static_cast<unsigned long long>(geometry.sliceLinearBytes), geometry.mips.size());
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
            texture->MarkRefreshedPerUse();
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
        const auto bytes = DescribeSurface(resource).guestBytes;
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
            if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) surface.depthFill = owner->depthFill;
            else surface.stencilFill = owner->stencilFill;
            if (!created || (aspect == VK_IMAGE_ASPECT_DEPTH_BIT ? surface.depthFill.has_value() : surface.stencilFill.has_value())) surface.pendingClear |= aspect;
        } else {
            surface.CopyAspectFrom(*owner, aspect);
        }
    }
    entry = {&surface, ++planeBinds};
}

void takePlanes(DepthSurface& surface, bool created, VkImageAspectFlags clearAspects) {
    takePlane(surface, depthOwners(), surface.target.address, VK_IMAGE_ASPECT_DEPTH_BIT, created, clearAspects);
    if (surface.target.stencilAddress != 0) takePlane(surface, stencilOwners(), surface.target.stencilAddress, VK_IMAGE_ASPECT_STENCIL_BIT, created, clearAspects);
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

void settlePendingHtileClear(const Context& context, std::uint64_t address) {
    if (Recorder::InCompletion()) return;
    auto* recorder = RecorderFor(context);
    if (recorder == nullptr) return;
    std::pair<std::uint64_t, std::size_t> metadata;
    {
        std::lock_guard lock(surfacesMutex());
        const auto* surface = planeSurface(context.device, address);
        if (surface == nullptr || !surface->samplingActive || surface->clearGeneration == 0u || !surface->clearCompletionQueued || !surface->sampling.refusal.empty()) return;
        const auto& range = surface->sampling.initial[1];
        metadata = {range.address, range.bytes};
    }
    if (recorder->PendingWriteOverlaps(metadata.first, metadata.second)) recorder->SyncThrough(metadata.first, metadata.second);
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

DepthSurface* drawnDepth = nullptr;

std::atomic<std::uint64_t> surfacesSerial{0};
std::atomic<std::uint64_t> surfacesReleased{0};

void releaseRetired(const Context& context) {
    auto* recorder = Recorder::Active();
    if (recorder != nullptr && recorder->Device() != context.device) return;
    auto& list = surfaces();
    for (auto it = list.begin(); it != list.end();) {
        const auto& surface = **it;
        if (!surface.retired || surface.context.device != context.device || surface.target.samples != 1u || surface.SampledElsewhere()) {
            ++it;
            continue;
        }
        if (drawnDepth == &surface) drawnDepth = nullptr;
        std::shared_ptr<DepthSurface> released = std::move(*it);
        it = list.erase(it);
        surfacesReleased.fetch_add(1, std::memory_order_release);
        if (recorder != nullptr) recorder->Keep(std::move(released));
    }
}

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

std::optional<std::uint32_t> htileAfterPendingFills(const Recorder& recorder, std::uint64_t address, std::size_t bytes) {
    auto writes = recorder.PendingWritesOver(address, bytes);
    std::sort(writes.begin(), writes.end(), [](const Recorder::PendingWrite& a, const Recorder::PendingWrite& b) { return a.note > b.note; });
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwritten{{address, address + bytes}};
    std::optional<std::uint32_t> landed;
    for (const auto& write : writes) {
        if (unwritten.empty()) break;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> rest;
        bool covered = false;
        for (const auto& [from, to] : unwritten) {
            if (write.end <= from || to <= write.begin) {
                rest.emplace_back(from, to);
                continue;
            }
            covered = true;
            if (from < write.begin) rest.emplace_back(from, write.begin);
            if (write.end < to) rest.emplace_back(write.end, to);
        }
        unwritten = std::move(rest);
        if (!covered) continue;
        if (write.word < 0 || (landed.has_value() && *landed != static_cast<std::uint32_t>(write.word))) return std::nullopt;
        landed = static_cast<std::uint32_t>(write.word);
    }
    if (!landed.has_value()) return std::nullopt;
    for (const auto& [from, to] : unwritten) {
        if (from % 4u != 0 || to % 4u != 0) return std::nullopt;
        const auto* words = reinterpret_cast<const volatile std::uint32_t*>(from);
        for (std::uint64_t index = 0; index < (to - from) / 4u; ++index) {
            if (words[index] != *landed) return std::nullopt;
        }
    }
    return landed;
}

struct HtileMaskWrite {
    HtileMaskClear clear;
    std::uint64_t note;
    std::uint64_t recorderId;
    VkDevice device;
};

std::mutex& htileMasksMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<HtileMaskWrite>& htileMasks() {
    static auto* masks = new std::vector<HtileMaskWrite>();
    return *masks;
}

std::optional<std::pair<VkImageAspectFlags, std::uint32_t>> htileAfterMaskClear(const Recorder& recorder, const DepthTarget& target, std::size_t bytes) {
    const auto newest = recorder.NewestWriteNote(target.htileAddress, bytes);
    if (newest == 0) return std::nullopt;
    std::optional<HtileMaskClear> clear;
    {
        std::lock_guard lock(htileMasksMutex());
        for (const auto& write : htileMasks()) {
            if (write.recorderId == recorder.Id() && write.note == newest && write.clear.address <= target.htileAddress && write.clear.address + write.clear.bytes >= target.htileAddress + bytes) clear = write.clear;
        }
    }
    if (!clear.has_value() || (clear->mask & 0xfu) != 0 || (clear->value & 0xfu) != 0) return std::nullopt;
    if (target.firstSlice != 0 && clear->mask != 0) return std::nullopt;
    if (!target.htileStencil) return std::pair<VkImageAspectFlags, std::uint32_t>{VK_IMAGE_ASPECT_DEPTH_BIT, 0xfffc000fu};
    if ((clear->mask & 0x300u) != 0) return std::nullopt;
    if (((clear->value >> 8u) & 0x3u) == 0) return std::pair<VkImageAspectFlags, std::uint32_t>{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0xfffff3ffu};
    if ((clear->mask & 0x3f0u) != 0) return std::nullopt;
    return std::pair<VkImageAspectFlags, std::uint32_t>{VK_IMAGE_ASPECT_DEPTH_BIT, (clear->value & 0x3f0u) | 0xfffff00fu};
}

bool storeHtileOnGpu(const Context& context, Recorder& recorder, std::uint64_t address, std::size_t bytes, std::uint32_t word) {
    if (address % 4u != 0 || bytes % 4u != 0) return false;
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr) return false;
    recorder.FlushKeyStoresOverlapping(address, bytes);
    recorder.FlushStoresOverlapping(address, bytes);
    const auto commands = recorder.Commands();
    const auto timing = recorder.BeginGpuTiming(Recorder::CommandClass::Fill);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::Fill);
    context.Resolved(&DeviceFunctions::cmdFillBuffer, "vkCmdFillBuffer")(commands, import->buffer, address - import->base, bytes, word);
    constexpr VkAccessFlags filledAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, filledAccess);
    Recorder::CountBarriers(Recorder::CommandClass::Fill);
    recorder.EndGpuTiming(timing, bytes);
    recorder.MarkCovered(filledAccess);
    recorder.NotePendingWordFill(address, bytes, word);
    GuestMemory::MarkWritten(address, bytes);
    return true;
}

VkImageAspectFlags storeBackClearedHtile(const Context& context, const DepthTarget& target) {
    if (target.htileAddress == 0) return 0;
    const auto bytes = static_cast<std::size_t>(HtileMetadataBytes(target.extent));
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(target.htileAddress), bytes, true)) return 0;
    const auto collected = GuestMemory::CollectWrites(target.htileAddress, bytes);
    auto* recorder = RecorderFor(context);
    const bool pending = recorder != nullptr && recorder->PendingWriteOverlaps(target.htileAddress, bytes);
    if (!pending) {
        std::lock_guard lock(htileReadsMutex());
        const auto found = htileReads().find(target.htileAddress);
        if (found != htileReads().end() && found->second != 0 && GuestMemory::UnchangedSince(target.htileAddress, bytes, found->second)) return 0;
    }
    static const bool pendingFills = std::getenv("APS5_NO_PENDING_HTILE_FILLS") == nullptr;
    if (pending && pendingFills) {
        std::lock_guard gpu(GuestMemory::GpuMutex());
        if (const auto landed = htileAfterPendingFills(*recorder, target.htileAddress, bytes)) {
            const VkImageAspectFlags cleared = (VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u)) & HtileFillClears(*landed, target.htileStencil);
            if (cleared == 0) {
                std::lock_guard lock(htileReadsMutex());
                htileReads()[target.htileAddress] = collected;
                return 0;
            }
            if (storeHtileOnGpu(context, *recorder, target.htileAddress, bytes, expandedHtileWord(*landed, target.htileStencil, cleared))) {
                const auto generation = GuestMemory::CollectWrites(target.htileAddress, bytes);
                std::lock_guard lock(htileReadsMutex());
                htileReads()[target.htileAddress] = generation;
                return cleared;
            }
        } else if (const auto known = htileAfterMaskClear(*recorder, target, bytes); known.has_value() && storeHtileOnGpu(context, *recorder, target.htileAddress, bytes, known->second)) {
            const auto generation = GuestMemory::CollectWrites(target.htileAddress, bytes);
            std::lock_guard lock(htileReadsMutex());
            htileReads()[target.htileAddress] = generation;
            return known->first;
        }
    }
    std::vector<std::uint32_t> words(bytes / 4u);
    GuestMemory::Read(target.htileAddress, std::as_writable_bytes(std::span(words)), 4);
    VkImageAspectFlags cleared = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
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

struct StalePlanes {
    std::mutex mutex;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::atomic<std::uint64_t> begin{std::numeric_limits<std::uint64_t>::max()};
    std::atomic<std::uint64_t> end{0};
};

StalePlanes& stalePlanes() {
    static auto* planes = new StalePlanes();
    return *planes;
}

void publishStaleHull(StalePlanes& planes) {
    auto begin = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t end = 0;
    for (const auto& range : planes.ranges) {
        begin = std::min(begin, range.first);
        end = std::max(end, range.second);
    }
    planes.begin.store(begin, std::memory_order_release);
    planes.end.store(end, std::memory_order_release);
}

void noteStalePlane(std::uint64_t begin, std::uint64_t end) {
    auto& planes = stalePlanes();
    std::lock_guard lock(planes.mutex);
    if (std::find(planes.ranges.begin(), planes.ranges.end(), std::pair{begin, end}) != planes.ranges.end()) return;
    planes.ranges.emplace_back(begin, end);
    publishStaleHull(planes);
}

bool overlapsStalePlane(std::uint64_t begin, std::uint64_t end) {
    auto& planes = stalePlanes();
    if (begin >= planes.end.load(std::memory_order_acquire) || planes.begin.load(std::memory_order_acquire) >= end) return false;
    std::lock_guard lock(planes.mutex);
    return std::any_of(planes.ranges.begin(), planes.ranges.end(), [&](const auto& range) { return begin < range.second && range.first < end; });
}

bool mirrorable(const DepthTarget& target) {
    return (target.format == VK_FORMAT_D32_SFLOAT || target.format == VK_FORMAT_D32_SFLOAT_S8_UINT) && target.samples == 1 && target.depthSwizzle == XorSwizzleMode(TextureTileMode::kZ64KBX);
}

GuestTextureResource mirrorResource(const DepthTarget& target) {
    GuestTextureResource resource{};
    resource.baseAddress = target.address;
    resource.width = target.extent.width;
    resource.height = target.extent.height;
    resource.mipCount = 1;
    resource.allocatedMipCount = 1;
    resource.tileMode = TextureTileMode::kZ64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 22;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    return resource;
}

std::pair<std::uint64_t, std::uint64_t> stalePlaneRange(const DepthSurface& surface) {
    return {surface.target.address, surface.target.address + DescribeSurface(mirrorResource(surface.target)).guestBytes};
}

GuestTextureResource stencilResource(const DepthTarget& target) {
    auto resource = mirrorResource(target);
    resource.baseAddress = target.stencilAddress;
    resource.format = 5;
    return resource;
}

std::pair<std::uint64_t, std::uint64_t> staleStencilRange(const DepthSurface& surface) {
    return {surface.target.stencilAddress, surface.target.stencilAddress + DescribeSurface(stencilResource(surface.target)).guestBytes};
}

void forgetStaleStencil(const DepthSurface& surface) {
    const auto plane = staleStencilRange(surface);
    if (std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& other) { return other->stencilStale && staleStencilRange(*other) == plane; })) return;
    auto& planes = stalePlanes();
    std::lock_guard lock(planes.mutex);
    std::erase(planes.ranges, plane);
    publishStaleHull(planes);
}

void retireStencil(DepthSurface& surface) {
    if (!surface.stencilStale) return;
    surface.stencilStale = false;
    forgetStaleStencil(surface);
}

void storeStaleStencil(DepthSurface& surface) {
    if (!surface.stencilStale || surface.syncing || !surface.StencilMirrorable()) return;
    const auto [begin, end] = staleStencilRange(surface);
    const auto bytes = static_cast<std::size_t>(end - begin);
    if (GuestMemory::CommittedRanges(begin, bytes).empty()) return;
    static_cast<void>(surface.StoreStencilPlane(stencilResource(surface.target)));
    forgetStaleStencil(surface);
    if (!StorageTexture::FlushPending(begin, bytes, nullptr, "stencil plane store")) return;
    GuestMemory::CollectWrites(begin, bytes);
    surface.NoteStencilStored(GuestMemory::TrackerGeneration());
}

void retire(DepthSurface& surface) {
    surface.retired = true;
    for (auto* owners : {&depthOwners(), &stencilOwners()}) {
        std::erase_if(*owners, [&](const auto& entry) { return entry.second.surface == &surface; });
    }
    retireStencil(surface);
    if (!surface.AnyStale()) return;
    surface.ClearStale();
    const auto plane = stalePlaneRange(surface);
    if (std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& other) { return other->AnyStale() && stalePlaneRange(*other) == plane; })) return;
    auto& planes = stalePlanes();
    std::lock_guard lock(planes.mutex);
    std::erase(planes.ranges, plane);
    publishStaleHull(planes);
}

void retireStoringStale(DepthSurface& surface) {
    if (!surface.StencilWrittenInMemory()) storeStaleStencil(surface);
    if (!surface.AnyStale() || !mirrorable(surface.target)) {
        retire(surface);
        return;
    }
    const auto resource = mirrorResource(surface.target);
    const auto planeBytes = DescribeSurface(resource).guestBytes;
    std::uint64_t generation = 0;
    const auto written = surface.WrittenUnits(resource, generation);
    auto units = surface.staleUnits;
    for (std::size_t unit = 0; unit < units.size() && unit < written.size(); ++unit) {
        if (written[unit]) units[unit] = false;
    }
    std::shared_ptr<StorageTexture> mirror;
    if (anyUnit(units) && (!anyUnit(written) || surface.StoresUnits(*surface.Mirror(resource)))) mirror = surface.StoreDepthPlane(resource, units);
    retire(surface);
    if (mirror == nullptr) return;
    const auto [begin, end] = surface.UnitHull(units, planeBytes);
    if (StorageTexture::FlushPending(begin, static_cast<std::size_t>(end - begin), nullptr, "depth plane store")) surface.NoteStored(units, GuestMemory::TrackerGeneration());
}
}

std::uint64_t DepthSurfacesSerial() {
    return surfacesSerial.load(std::memory_order_acquire);
}

std::uint64_t DepthSurfacesReleased() {
    return surfacesReleased.load(std::memory_order_acquire);
}

std::uint64_t DepthSliceBytes(VkExtent2D extent, std::uint32_t bytesPerTexel) {
    const auto [blockWidth, blockHeight] = DepthBlockExtent(bytesPerTexel);
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

void RequireNoResidentDepthRead_nid_no_patch(std::uint64_t address, std::size_t bytes, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear) {
    if (bytes == 0u || !anyDepthGuard().load(std::memory_order_acquire)) return;
    Require(address <= std::numeric_limits<std::uint64_t>::max() - bytes, "native depth read range overflows");
    const auto overlaps = [&](const DepthGuard& range) { return address < range.address + range.bytes && range.address < address + bytes; };
    if (htileClear.has_value() && htileClear->second <= std::numeric_limits<std::uint64_t>::max() - htileClear->first && htileClear->first <= address && bytes <= htileClear->second && address - htileClear->first <= htileClear->second - bytes) {
        std::lock_guard lock(surfacesMutex());
        std::shared_lock guardLock(depthGuardMutex());
        const bool accepted = std::ranges::all_of(depthGuards(), [&](const auto& range) {
            if (!overlaps(range)) return true;
            return std::ranges::any_of(surfaces(), [&](const auto& surface) {
                const auto& metadata = surface->sampling.initial[1];
                return surface->active == range.active && surface->active->load(std::memory_order_acquire) && surface->samplingActive && surface->sampling.refusal.empty() && surface->clearPending && metadata.address == range.address && metadata.bytes == range.bytes && metadata.address == htileClear->first && metadata.bytes <= htileClear->second;
            });
        });
        if (accepted) return;
    }
    {
        std::shared_lock lock(depthGuardMutex());
        if (std::ranges::none_of(depthGuards(), overlaps)) return;
    }
    std::unique_lock lock(depthGuardMutex());
    for (const auto& range : depthGuards()) {
        if (overlaps(range)) Require(!range.hostWritten->load(std::memory_order_acquire), "sample-preserving guest publication of native multisample depth is not implemented");
    }
    for (const auto& range : depthGuards()) {
        if (overlaps(range)) range.active->store(false, std::memory_order_release);
    }
    std::erase_if(depthGuards(), [](const auto& range) { return !range.active->load(std::memory_order_acquire); });
    anyDepthGuard().store(!depthGuards().empty(), std::memory_order_release);
}

void AppendHostWrittenDepthRanges(std::uint64_t address, std::size_t bytes, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) {
    if (bytes == 0u || !anyDepthGuard().load(std::memory_order_acquire)) return;
    std::shared_lock lock(depthGuardMutex());
    for (const auto& range : depthGuards()) {
        if (address < range.address + range.bytes && range.address < address + bytes && range.hostWritten->load(std::memory_order_acquire)) ranges.emplace_back(range.address, range.bytes);
    }
}

void NoteDepthSurfaceWrite(const Context& context, const DepthTarget& target) {
    if (target.samples == 1u || !anyDepthGuard().load(std::memory_order_acquire)) return;
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && sameSurface(surface->target, target)) surface->NoteHostWrite();
    }
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
    static AgcDriver::Mutex mutex;
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

VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target, bool depthWritten) {
    const auto planeBytes = [&](std::uint32_t bytesPerTexel) { return target.samples == 1u ? DepthSliceBytes(target.extent, bytesPerTexel) : MultisampledDepthBytes_nid_no_patch(target.extent, bytesPerTexel, target.samples); };
    ClaimMultisampleTargetMemory(target.address, planeBytes(target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT ? 2u : 4u));
    if (target.stencilAddress != 0u) ClaimMultisampleTargetMemory(target.stencilAddress, planeBytes(1u));
    if (target.htileAddress != 0u) ClaimMultisampleTargetMemory(target.htileAddress, HtileMetadataBytes(target.extent));
    const auto htileCleared = RoutesToNativeDepthOwner(target) ? VkImageAspectFlags{0} : storeBackClearedHtile(context, target);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    settlePendingHtileClear(context, target.address);
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
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto depthBytes = target.samples == 1u ? DepthSliceBytes(target.extent, d16 ? 2u : 4u) : MultisampledDepthBytes_nid_no_patch(target.extent, d16 ? 2u : 4u, target.samples);
        const auto stencilBytes = target.stencilAddress == 0u ? 0u : target.samples == 1u ? DepthSliceBytes(target.extent, 1u) : MultisampledDepthBytes_nid_no_patch(target.extent, 1u, target.samples);
        const auto metadataBytes = target.htileAddress == 0u ? 0u : HtileMetadataBytes(target.extent);
        Require(target.address <= std::numeric_limits<std::uint64_t>::max() - depthBytes && target.stencilAddress <= std::numeric_limits<std::uint64_t>::max() - stencilBytes && target.htileAddress <= std::numeric_limits<std::uint64_t>::max() - metadataBytes, "native depth attachment range overflows");
        lock.unlock();
        RequireNoResidentDepthRead_nid_no_patch(target.address, static_cast<std::size_t>(depthBytes));
        if (stencilBytes != 0u) RequireNoResidentDepthRead_nid_no_patch(target.stencilAddress, static_cast<std::size_t>(stencilBytes));
        if (metadataBytes != 0u) RequireNoResidentDepthRead_nid_no_patch(target.htileAddress, static_cast<std::size_t>(metadataBytes));
        auto sampling = target.samples == 1u ? DepthSamplingState{} : prepareDepthSampling(context, target);
        creationLease = target.samples == 1u || !sampling.refusal.empty() ? GuestAllocations::Lease{} : GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
        lock.lock();
        releaseRetired(context);
        surfacesSerial.fetch_add(1, std::memory_order_release);
        surfaces().push_back(std::make_unique<DepthSurface>(context, target, std::move(sampling)));
        ++mirrorEpoch;
        auto& fresh = *surfaces().back();
        if (target.samples != 1u && fresh.sampleable && fresh.sampling.refusal.empty()) {
            std::unique_lock guardLock(depthGuardMutex());
            for (const auto& range : fresh.sampling.initial) depthGuards().push_back({context.device, range.address, range.bytes, fresh.active, {{range.address, range.bytes, 0u}}, fresh.hostWritten});
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
    takePlanes(*surface, created, htileCleared | (cleared ? VK_IMAGE_ASPECT_DEPTH_BIT : 0u));
    if (created && !inherits && target.htileAddress == 0) surface->TakeStorage();
    if (cleared && (!created || inherits)) {
        if (target.samples != 1u) surface->sampling.Refuse("native multisample depth fast-clear transition is not implemented");
        if (!created) surface->NoteHtileClear(VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    if (!created) surface->NoteHtileClear(htileCleared);
    if (mirrorable(target)) surface->SyncFromMemory(mirrorResource(target));
    if (surface->StencilMirrorable()) surface->SyncStencilFromMemory(stencilResource(target));
    surface->ApplyFastClear();
    if (drawnDepth != nullptr && drawnDepth != surface) drawnDepth->written = ++depthWrites;
    surface->written = ++depthWrites;
    surface->NoteMemory();
    if (mirrorable(target) && surface->scannedEpoch != mirrorEpoch) {
        surface->scannedEpoch = mirrorEpoch;
        const auto plane = stalePlaneRange(*surface);
        for (const auto& other : surfaces()) {
            if (other.get() == surface || other->context.device != context.device || other->written >= surface->written || !mirrorable(other->target)) continue;
            const auto otherPlane = stalePlaneRange(*other);
            if (otherPlane == plane || otherPlane.first >= plane.second || plane.first >= otherPlane.second) continue;
            other->ClearStale();
            other->DropMirror();
        }
    }
    if ((depthWritten || cleared) && mirrorable(target)) {
        const auto plane = target.address;
        const auto planeBytes = DescribeSurface(mirrorResource(target)).guestBytes;
        surface->MarkAllStale(planeBytes);
        noteStalePlane(plane, plane + planeBytes);
    }
    if (surface->StencilMirrorable()) {
        surface->stencilStale = true;
        const auto [begin, end] = staleStencilRange(*surface);
        noteStalePlane(begin, end);
    }
    drawnDepth = surface;
    return surface->view;
}

SampleOrderTarget DepthSurfaceOrderTarget(const Context& context, const DepthTarget& target) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::unique_lock lock(surfacesMutex());
    for (const auto& candidate : surfaces()) {
        if (!candidate->active->load(std::memory_order_acquire) || candidate->context.device != context.device || !sameSurface(candidate->target, target)) continue;
        Require(candidate->sampleable, "the multisampled depth surface whose samples are put in the guest's order cannot be sampled");
        return {candidate->image, candidate->view, candidate->hostFormat, target.extent, target.samples, DepthSampleOrderMethod(context, target), candidate->owned};
    }
    throw std::runtime_error("AGC graphics: the multisampled depth surface whose samples are put in the guest's order is not resident");
}

DepthSampleLocations DepthSurfaceSampleLocations(const Context& context, const DepthTarget& target) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::unique_lock lock(surfacesMutex());
    for (const auto& candidate : surfaces()) {
        if (!candidate->active->load(std::memory_order_acquire) || candidate->context.device != context.device || !sameSurface(candidate->target, target)) continue;
        DepthSampleLocations depth;
        depth.image = candidate->image;
        depth.format = candidate->hostFormat;
        depth.samples = target.samples;
        depth.keep = candidate->owned;
        return depth;
    }
    throw std::runtime_error("AGC graphics: the depth surface drawn at custom sample locations is not resident");
}

bool DepthPlanesStale() {
    auto& planes = stalePlanes();
    return planes.begin.load(std::memory_order_acquire) < planes.end.load(std::memory_order_acquire);
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> FlushDepthPlanes(std::uint64_t address, std::size_t bytes, DepthPlaneScope scope) {
    static thread_local bool flushing = false;
    const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
    if (flushing || !overlapsStalePlane(address, end)) return {};
    if (scope == DepthPlaneScope::Overlapped) {
        std::lock_guard lock(surfacesMutex());
        const auto staleInRange = [&](const auto& surface) {
            if (surface->syncing) return false;
            if (surface->stencilStale) {
                const auto [stencilBegin, stencilEnd] = staleStencilRange(*surface);
                if (stencilBegin < end && address < stencilEnd) return true;
            }
            return surface->AnyStale() && ((surface->pendingClear & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 || anyUnit(surface->StaleUnitsIn(DescribeSurface(mirrorResource(surface->target)).guestBytes, address, end)));
        };
        if (std::none_of(surfaces().begin(), surfaces().end(), staleInRange)) return {};
    }
    flushing = true;
    struct Reset {
        ~Reset() { flushing = false; }
    } reset;
    struct Stored {
        DepthSurface* surface;
        std::shared_ptr<StorageTexture> mirror;
        std::vector<bool> units;
        std::uint64_t begin;
        std::uint64_t end;
        bool flushed;
    };
    std::vector<Stored> stored;
    struct StoredStencil {
        DepthSurface* surface;
        std::shared_ptr<StorageTexture> mirror;
        std::uint64_t begin;
        std::uint64_t end;
        bool flushed;
    };
    std::vector<StoredStencil> storedStencils;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    {
        std::lock_guard gpu(GuestMemory::GpuMutex());
        std::lock_guard lock(surfacesMutex());
        std::vector<std::pair<std::uint64_t, std::uint64_t>> remaining;
        for (const auto& surface : surfaces()) {
            if (surface->stencilStale && !surface->syncing) {
                const auto [stencilBegin, stencilEnd] = staleStencilRange(*surface);
                if (stencilBegin < end && address < stencilEnd) {
                    if (GuestMemory::CommittedRanges(stencilBegin, static_cast<std::size_t>(stencilEnd - stencilBegin)).empty()) {
                        surface->stencilStale = false;
                    } else {
                        auto mirror = surface->StoreStencilPlane(stencilResource(surface->target));
                        ranges.emplace_back(stencilBegin, stencilEnd - stencilBegin);
                        storedStencils.push_back({surface.get(), std::move(mirror), stencilBegin, stencilEnd, false});
                    }
                }
            }
            if (surface->stencilStale) remaining.push_back(staleStencilRange(*surface));
            if (!surface->AnyStale()) continue;
            const auto resource = mirrorResource(surface->target);
            const auto plane = surface->target.address;
            const auto planeBytes = DescribeSurface(resource).guestBytes;
            const bool overlapped = !surface->syncing && plane < end && address < plane + planeBytes;
            if (overlapped && (surface->pendingClear & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) surface->MarkAllStale(planeBytes);
            if (overlapped && (scope == DepthPlaneScope::Whole || anyUnit(surface->StaleUnitsIn(planeBytes, address, end)))) {
                if (GuestMemory::CommittedRanges(plane, static_cast<std::size_t>(planeBytes)).empty()) {
                    static std::atomic<bool> reported{false};
                    if (!reported.exchange(true)) std::fprintf(stderr, "[gpu] depth plane 0x%llx+0x%llx was unmapped with GPU results pending; they are dropped\n", static_cast<unsigned long long>(plane), static_cast<unsigned long long>(planeBytes));
                    surface->ClearStale();
                    continue;
                }
                GuestMemory::CollectWritesUncached(plane, static_cast<std::size_t>(planeBytes));
                surface->SyncFromMemory(resource);
                auto units = scope == DepthPlaneScope::Whole ? surface->staleUnits : surface->StaleUnitsIn(planeBytes, address, end);
                if (anyUnit(units)) {
                    auto mirror = surface->StoreDepthPlane(resource, units);
                    surface->ClearStale(units);
                    const auto hull = surface->UnitHull(units, planeBytes);
                    ranges.emplace_back(hull.first, hull.second - hull.first);
                    stored.push_back({surface.get(), std::move(mirror), std::move(units), hull.first, hull.second, false});
                }
            }
            if (surface->AnyStale()) remaining.emplace_back(plane, plane + planeBytes);
        }
        auto& planes = stalePlanes();
        std::lock_guard staleLock(planes.mutex);
        planes.ranges = std::move(remaining);
        publishStaleHull(planes);
    }
    for (auto& entry : stored) entry.flushed = StorageTexture::FlushPending(entry.begin, static_cast<std::size_t>(entry.end - entry.begin), nullptr, "depth plane store");
    for (auto& entry : storedStencils) entry.flushed = StorageTexture::FlushPending(entry.begin, static_cast<std::size_t>(entry.end - entry.begin), nullptr, "stencil plane store");
    const auto generation = GuestMemory::TrackerGeneration();
    std::lock_guard lock(surfacesMutex());
    for (const auto& entry : storedStencils) {
        if (!entry.flushed || std::none_of(surfaces().begin(), surfaces().end(), [&](const auto& candidate) { return candidate.get() == entry.surface; })) continue;
        GuestMemory::CollectWrites(entry.begin, static_cast<std::size_t>(entry.end - entry.begin));
        entry.surface->NoteStencilStored(GuestMemory::TrackerGeneration());
    }
    for (const auto& entry : stored) {
        if (!entry.flushed || std::none_of(surfaces().begin(), surfaces().end(), [&](const auto& candidate) { return candidate.get() == entry.surface; })) continue;
        entry.surface->NoteStored(entry.units, generation);
        entry.surface->NoteMemory();
    }
    return ranges;
}

DepthPlaneStoreStatistics DepthPlaneStoreCounts() {
    const auto& counters = depthPlaneStoreCounters();
    DepthPlaneStoreStatistics statistics;
    statistics.stores = counters.stores.load();
    statistics.units = counters.units.load();
    statistics.bytes = counters.bytes.load();
    return statistics;
}

std::uint64_t HtileDepthClearAddress(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads) {
    static constexpr std::array<std::uint32_t, 17> htileMaskKernel{0xd7460000u, 0x04010c06u, 0x34000084u, 0xdc388000u, 0x04020000u, 0xbf8c3f70u, 0xd7710008u, 0x00120805u, 0xd7710009u, 0x00120a05u, 0xd771000au, 0x00120c05u, 0xd771000bu, 0x00120e05u, 0xdc788000u, 0x00020800u, 0xbf810000u};
    if (userData.size() < 6 || numThreads[0] != 64 || numThreads[1] != 1 || numThreads[2] != 1) return 0;
    if (code.size() < htileMaskKernel.size() || !std::equal(htileMaskKernel.begin(), htileMaskKernel.end(), code.begin())) return 0;
    if ((userData[5] & 0xfu) != 0 || (userData[4] & 0xfu) != 0) return 0;
    return userData[2] | (static_cast<std::uint64_t>(userData[3]) << 32u);
}

std::optional<HtileMaskClear> MatchHtileBufferMaskClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, std::span<const std::uint32_t> packet, std::uint64_t indirectArguments) {
    static constexpr std::array<std::uint32_t, 16> kernel{0xd7460004u, 0x04010c0cu, 0xe00c2000u, 0x80000004u, 0xbf8c3f70u, 0xd7710000u, 0x00120008u, 0xd7710001u, 0x00160209u, 0xd7710002u, 0x001a040au, 0xd7710003u, 0x001e060bu, 0xe01c2000u, 0x80000004u, 0xbf810000u};
    if (userData.size() != 12u || packet.size() < 5u || indirectArguments != 0u) return std::nullopt;
    if (code.size() < kernel.size() || !std::equal(kernel.begin(), kernel.end(), code.begin())) return std::nullopt;
    if (compute.numThreads != std::array<std::uint32_t, 3>{64u, 1u, 1u} || !compute.groupIdEnable[0] || compute.threadIdComponentCount == 0u || compute.threadIdComponentCount > 3u) return std::nullopt;
    if (packet[1] == 0u || packet[2] != 1u || packet[3] != 1u || (packet[4] & 0x3u) != 0x1u) return std::nullopt;
    for (std::size_t word = 1; word < 4; ++word) {
        if (userData[4 + word] != userData[4] || userData[8 + word] != userData[8]) return std::nullopt;
    }
    if ((userData[8] & 0xfu) != 0u || (userData[4] & 0xfu) != 0u) return std::nullopt;
    const auto stride = (userData[1] >> 16u) & 0x3fffu;
    const bool swizzled = ((userData[1] >> 31u) & 1u) != 0u;
    const bool addTid = ((userData[3] >> 23u) & 1u) != 0u;
    const auto type = userData[3] >> 30u;
    const auto format = (userData[3] >> 12u) & 0x7fu;
    const auto selects = userData[3] & 0xfffu;
    if (stride != 16u || swizzled || addTid || type != 0u || format != 75u || selects != 0xfacu) return std::nullopt;
    const auto address = userData[0] | (static_cast<std::uint64_t>(userData[1] & 0xffffu) << 32u);
    const std::uint64_t threads = (packet[4] & 0x20u) != 0u ? packet[1] : static_cast<std::uint64_t>(packet[1]) * 64u;
    const auto records = std::min<std::uint64_t>(threads, userData[2]);
    if (address == 0u || (address & 15u) != 0u || records == 0u || records > 0x400000u) return std::nullopt;
    return HtileMaskClear{address, static_cast<std::size_t>(records * 16u), userData[8], userData[4]};
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

std::optional<StorageImageClear> MatchStorageImageClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, const std::array<std::uint32_t, 3>& groups) {
    static constexpr std::array<std::uint32_t, 15> kernel{0xd7460004u, 0x0401060cu, 0xf4280204u, 0xfa000000u, 0xd7460005u, 0x0405060du, 0x7e0c020eu, 0xbf8cc07fu, 0x7e000208u, 0x7e020209u, 0x7e04020au, 0x7e06020bu, 0xf0200f28u, 0x00000004u, 0xbf810000u};
    if (code.size() < kernel.size() || code[0] != kernel[0] || !std::equal(kernel.begin(), kernel.end(), code.begin())) return std::nullopt;
    if (userData.size() != 12u) return std::nullopt;
    if (compute.numThreads != std::array<std::uint32_t, 3>{8u, 8u, 1u}) return std::nullopt;
    if (!compute.groupIdEnable[0] || !compute.groupIdEnable[1] || !compute.groupIdEnable[2]) return std::nullopt;
    if (compute.threadIdComponentCount < 2u) return std::nullopt;
    std::array<std::uint64_t, 3> covered{};
    for (std::size_t axis = 0; axis < 3u; ++axis) covered[axis] = compute.partialThreads[axis] != 0u ? compute.partialThreads[axis] : static_cast<std::uint64_t>(groups[axis]) * compute.numThreads[axis];
    if (covered[0] == 0u || covered[1] == 0u || covered[2] != 1u) return std::nullopt;
    StorageImageClear clear;
    std::copy_n(userData.begin(), clear.image.size(), clear.image.begin());
    std::copy_n(userData.begin() + static_cast<std::ptrdiff_t>(clear.image.size()), clear.color.size(), clear.color.begin());
    clear.width = static_cast<std::uint32_t>(std::min<std::uint64_t>(covered[0], std::numeric_limits<std::uint32_t>::max()));
    clear.height = static_cast<std::uint32_t>(std::min<std::uint64_t>(covered[1], std::numeric_limits<std::uint32_t>::max()));
    return clear;
}

std::uint64_t StorageImageClearAddress(const StorageImageClear& clear) {
    try {
        return DecodeTextureResource(clear.image).baseAddress;
    } catch (...) {
        return 0u;
    }
}

DepthPlaneClear::DepthPlaneClear(const StorageImageClear& clear) {
    if (!depthPlaneClearEnabled.load(std::memory_order_relaxed)) return;
    const auto decoded = decodeOverwrite(clear);
    if (decoded) {
        value = *decoded;
        previous = currentOverwrite;
        currentOverwrite = &value;
        installed = true;
        ++depthClearCounters().taken;
    }
}

DepthPlaneClear::~DepthPlaneClear() {
    if (installed) currentOverwrite = previous;
}

DepthClearStatistics DepthClearCounts() {
    const auto& counters = depthClearCounters();
    DepthClearStatistics statistics;
    statistics.matched = counters.matched.load();
    statistics.taken = counters.taken.load();
    statistics.copiesSkipped = counters.copiesSkipped.load();
    statistics.clears = counters.clears.load();
    statistics.bytesSkipped = counters.bytesSkipped.load();
    for (std::size_t reason = 0; reason < statistics.declined.size(); ++reason) statistics.declined[reason] = counters.declined[reason].load();
    return statistics;
}

void SetDepthPlaneClearForTest(bool enabled) {
    depthPlaneClearEnabled.store(enabled, std::memory_order_relaxed);
}

std::size_t HtileDepthClearBytes_nid_no_patch(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, std::span<const std::uint32_t> packet, std::uint64_t indirectArguments) {
    if (userData.size() != 6u || packet.size() < 5u || indirectArguments != 0u || compute.PartialGroups() || !compute.groupIdEnable[0] || compute.threadIdComponentCount == 0u || compute.threadIdComponentCount > 3u) return 0u;
    if (packet[1] == 0u || packet[1] > 0x400000u || packet[2] != 1u || packet[3] != 1u || (packet[4] & 0x23u) != 1u) return 0u;
    const auto address = HtileDepthClearAddress(code, userData, compute.numThreads);
    if (address == 0u || (address & 15u) != 0u) return 0u;
    const auto bytes = static_cast<std::uint64_t>(packet[1]) * 1024u;
    return address <= std::numeric_limits<std::uint64_t>::max() - bytes ? static_cast<std::size_t>(bytes) : 0u;
}

void NoteHtileMaskOnGpu(const Context& context, const HtileMaskClear& clear) {
    const auto* recorder = RecorderFor(context);
    if (recorder == nullptr || clear.bytes == 0) return;
    const auto end = clear.address + clear.bytes;
    std::uint64_t note = 0;
    bool covers = false;
    for (const auto& write : recorder->PendingWritesOver(clear.address, clear.bytes)) {
        if (write.note <= note) continue;
        note = write.note;
        covers = write.begin <= clear.address && write.end >= end;
    }
    std::lock_guard lock(htileMasksMutex());
    auto& masks = htileMasks();
    std::erase_if(masks, [&](const HtileMaskWrite& write) { return clear.address < write.clear.address + write.clear.bytes && write.clear.address < end; });
    if (!covers) return;
    constexpr std::size_t KeptMasks = 64;
    if (masks.size() >= KeptMasks) masks.erase(masks.begin());
    masks.push_back({clear, note, recorder->Id(), context.device});
}

void ForgetHtileMasks_nid_no_patch(std::uint64_t recorderId) {
    std::lock_guard lock(htileMasksMutex());
    std::erase_if(htileMasks(), [&](const HtileMaskWrite& write) { return write.recorderId == recorderId; });
}

bool NoteHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes, std::size_t nativeBytes) {
    std::lock_guard lock(surfacesMutex());
    forgetHtileReads(htileAddress, bytes);
    bool existing = false;
    bool completionCandidate = false;
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        if (surface->samplingActive && surface->target.htileAddress == htileAddress) {
            const auto& metadata = surface->sampling.initial[1];
            if (nativeBytes != bytes || nativeBytes < metadata.bytes || !GuestMemory::Accessible(reinterpret_cast<const void*>(metadata.address), metadata.bytes) || !GuestMemory::Watched(metadata.address, metadata.bytes)) surface->sampling.Refuse("native multisample depth incomplete or unproven fast-clear transition is not implemented");
            else if (surface->clearPending || surface->clearGeneration != 0u) surface->sampling.Refuse("overlapping native multisample depth clear transitions are not implemented");
            else {
                surface->clearPending = true;
                surface->clearCompletionQueued = false;
                completionCandidate = true;
            }
            existing = true;
            continue;
        }
        if (!HtileFillCovers(surface->target.htileAddress, surface->target.extent, htileAddress, bytes)) continue;
        if (surface->target.samples != 1u) surface->sampling.Refuse("native multisample depth fast-clear transition is not implemented");
        surface->NoteHtileClear(VK_IMAGE_ASPECT_DEPTH_BIT);
        existing = true;
    }
    if (!existing) clearedHtiles()[htileAddress] = bytes;
    return completionCandidate;
}

void ArmHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->target.htileAddress != htileAddress || !surface->clearPending) continue;
        const auto& metadata = surface->sampling.initial[1];
        if (bytes < metadata.bytes) continue;
        surface->clearGeneration = GuestMemory::CollectWritesUncached(metadata.address, metadata.bytes);
        surface->clearPending = false;
        if (surface->clearGeneration == 0u) surface->sampling.Refuse("native multisample depth clear requires tracked HTILE generations");
    }
}

void QueueHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->target.htileAddress != htileAddress || surface->clearGeneration == 0u || !surface->sampling.refusal.empty()) continue;
        if (bytes >= surface->sampling.initial[1].bytes) surface->clearCompletionQueued = true;
    }
}

void CompleteHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || !surface->samplingActive || surface->target.htileAddress != htileAddress || surface->clearGeneration == 0u) continue;
        const auto& metadata = surface->sampling.initial[1];
        if (bytes < metadata.bytes) continue;
        Require(GuestMemory::UnchangedSinceCollected(metadata.address, metadata.bytes, surface->clearGeneration), "CPU writes raced native multisample depth clear");
        GuestMemory::MarkWritten(metadata.address, metadata.bytes);
        surface->clearCompletionQueued = false;
    }
}

void ClearDepthSurfaces(VkDevice device) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    {
        std::lock_guard lock(htileMasksMutex());
        std::erase_if(htileMasks(), [&](const HtileMaskWrite& write) { return write.device == device; });
    }
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

std::size_t DepthPlaneCopyCount() {
    return planeCopies().size();
}

VkDeviceSize DepthPlaneCopyBytes() {
    VkDeviceSize total = 0;
    for (const auto& entry : planeCopies()) total += entry.second->HeldBytes();
    return total;
}

namespace {

thread_local bool depthCopySourceActive = false;

}

std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType multisampleViewType) {
    const auto held = [&](const auto& surface) {
        return surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && !surface->retired && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    };
    {
        std::lock_guard lock(surfacesMutex());
        if (std::none_of(surfaces().begin(), surfaces().end(), held)) return nullptr;
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    settlePendingHtileClear(context, resource.baseAddress);
    std::lock_guard lock(surfacesMutex());
    auto* found = planeSurface(context.device, resource.baseAddress);
    if (found == nullptr) return nullptr;
    const auto& base = found->target;
    const bool stencil = base.stencilAddress != 0 && resource.baseAddress == base.stencilAddress;
    if (stencil && BytesPerElement(resource.format) != 1u) return nullptr;
    if (stencil && found->StencilMirrorable()) found->SyncStencilFromMemory(stencilResource(base));
    if (base.samples != 1u || resource.dimension == TextureDimension::k2DMsaa) {
        Require(base.samples > 1u, "multisample reader overlaps a single-sample depth attachment");
        auto* recorder = RecorderFor(context);
        Require(recorder != nullptr, "a native multisampled depth target is sampled only by recorded draws and dispatches");
        if (depthCopySourceActive) {
            found->CheckResource(resource);
            found->ApplyFastClear();
        } else {
            found->CheckCurrent();
            found->CheckResource(resource);
            Require(found->pendingClear == 0u, "native multisample depth fast-clear transition is not implemented");
        }
        RecordMemoryBarrier(context, recorder->Commands(), VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        return found->Sampled(words, resource, components, found == drawnDepth, multisampleViewType, depthCopySourceActive);
    }
    if (resource.width != base.extent.width || resource.height != base.extent.height || resource.baseLevel != 0) return nullptr;
    const bool d16 = base.format == VK_FORMAT_D16_UNORM || base.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto format = ResolveTextureFormat(resource.format);
    const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
    const bool integer = d16 && format == VK_FORMAT_R16_UINT && !depthBits;
    if (!stencil && format != (d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT) && !depthBits && !integer) return nullptr;
    const bool cube = resource.dimension == TextureDimension::kCube;
    const bool layered = cube || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray != 0);
    const bool sampled = stencil || (!layered && !integer);
    const auto copyFormat = integer ? VK_FORMAT_R16_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
    const bool planes = (format == copyFormat || depthBits) && resource.width == base.extent.width && resource.height == base.extent.height && resource.baseLevel == 0 && resource.lastLevel == 0 && !(resource.baseArray != 0 && (cube || !layered || resource.baseArray > resource.depthOrLastArray)) && !(cube && resource.depthOrLastArray != 0 && (resource.depthOrLastArray + 1u) % 6u != 0u) && (resource.dimension == TextureDimension::k2D || resource.dimension == TextureDimension::k2DArray || cube);
    if (!(sampled ? found->SampledAccepts(words, resource) : planes) && (found->OverwrittenInMemory() || found->OverwrittenOnGpu())) {
        retireStoringStale(*found);
        return nullptr;
    }
    if (sampled && !stencil && mirrorable(base)) found->SyncFromMemory(mirrorResource(base));
    found->ApplyFastClear();
    if (sampled) return found->Sampled(words, resource, components, found == drawnDepth);
    Require(found->hostFormat == base.format, "sampling several slices or an integer view of a depth plane that the device stores in another depth format is not implemented");
    const auto layers = cube ? std::max(resource.depthOrLastArray + 1u, 6u) : layered ? resource.depthOrLastArray + 1u : 1u;
    if (!planes) {
        char text[400];
        std::snprintf(text, sizeof(text), "AGC graphics: sampling the depth planes of depth surface 0x%llx (%ux%u, vk format %d) as a %ux%u texture of guest format %u (vk %d), dimension %d, %u slices, levels %u-%u, base slice %u is not implemented (last array %u, depth bits %d, copy format %d)", static_cast<unsigned long long>(base.address), base.extent.width, base.extent.height, static_cast<int>(base.format), resource.width, resource.height, resource.format, static_cast<int>(format), static_cast<int>(resource.dimension), layers, resource.baseLevel, resource.lastLevel, resource.baseArray, resource.depthOrLastArray, depthBits ? 1 : 0, static_cast<int>(copyFormat));
        throw std::runtime_error(text);
    }
    std::vector<DepthPlaneSlice> slices;
    const auto geometry = DescribeSurface(resource);
    Require(!geometry.levelMajor, "depth planes of a PS4 layered surface with several mip levels (stored level by level) are not implemented");
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        const auto address = base.address + geometry.GuestLayerOffset(layer);
        auto* slice = depthSlice(context.device, address, base);
        if (slice != nullptr) slice->ApplyFastClear();
        if (slice == nullptr) slices.push_back({});
        else slices.push_back({slice->image, slice == drawnDepth ? 0u : slice->written});
    }
    const auto imageFormat = integer ? VK_FORMAT_R16_UINT : d16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
    auto& copies = planeCopies();
    const std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, VkFormat> copyKey{context.device, base.address, base.extent.width, base.extent.height, imageFormat};
    auto& copy = copies[copyKey];
    if (copy == nullptr || copy->layers < layers) copy = std::make_unique<DepthPlaneCopy>(context, base.extent, imageFormat, layers);
    copy->lastUse = ++planeCopyClock();
    copy->lastPresent = Recorder::Presents();
    auto texture = copy->Refresh(slices, resource, components, cube || resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, layered && !cube ? resource.baseArray : 0u);
    evictPlaneCopies(copyKey);
    return texture;
}

bool DepthSurfaceHolds(const Context& context, const GuestTextureResource& resource, const Texture* texture) {
    if ((resource.dimension != TextureDimension::k2D && resource.dimension != TextureDimension::k2DMsaa) || resource.baseArray != 0) return false;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    const auto address = resource.baseAddress;
    settlePendingHtileClear(context, address);
    std::lock_guard lock(surfacesMutex());
    auto* found = planeSurface(context.device, address);
    if (found == nullptr || !found->Holds(texture)) return false;
    if (!depthCopySourceActive) found->CheckCurrent();
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

bool PlaneFillCovers(std::uint64_t plane, std::uint64_t planeBytes, std::uint64_t address, std::size_t bytes) {
    return plane != 0 && plane >= address && plane + planeBytes <= address + bytes;
}

std::optional<std::uint8_t> StencilFillValue(std::uint32_t pattern) {
    const auto byte = static_cast<std::uint8_t>(pattern & 0xffu);
    if (pattern != byte * 0x01010101u) return std::nullopt;
    return byte;
}

std::optional<float> DepthFillValue(std::uint32_t pattern, bool d16) {
    if (d16) {
        if ((pattern >> 16u) != (pattern & 0xffffu)) return std::nullopt;
        return static_cast<float>(pattern & 0xffffu) / 65535.0f;
    }
    const auto value = std::bit_cast<float>(pattern);
    if (!(value >= 0.0f && value <= 1.0f)) return std::nullopt;
    return value;
}

void NoteDepthMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    std::lock_guard lock(surfacesMutex());
    forgetHtileReads(address, bytes);
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        const auto& target = surface->target;
        if (target.samples != 1u && target.htileAddress != 0u && address < target.htileAddress + HtileMetadataBytes(target.extent) && target.htileAddress < address + bytes) surface->sampling.Refuse("native multisample depth metadata fill transition is not implemented");
        if (HtileFillCovers(target.htileAddress, target.extent, address, bytes)) {
            const VkImageAspectFlags written = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
            surface->ForgetClear(written);
            surface->NoteHtileClear(HtileFillClears(pattern, target.htileStencil));
            continue;
        }
        if (target.samples != 1) continue;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        if (PlaneFillCovers(target.address, DepthSliceBytes(target.extent, d16 ? 2u : 4u), address, bytes)) {
            if (const auto depth = DepthFillValue(pattern, d16)) surface->NotePlaneFill(VK_IMAGE_ASPECT_DEPTH_BIT, *depth, 0);
        }
        if (PlaneFillCovers(target.stencilAddress, DepthSliceBytes(target.extent, 1u), address, bytes)) {
            if (const auto stencil = StencilFillValue(pattern)) surface->NotePlaneFill(VK_IMAGE_ASPECT_STENCIL_BIT, 0.0f, *stencil);
        }
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
        const auto address = base.address + static_cast<std::uint64_t>(layer) * storage.layerPitchBytes;
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
    surface.RequireStorable(resource);
    Require(!DescribeSurface(resource).levelMajor, "depth planes of a PS4 layered surface with several mip levels (stored level by level) are not implemented");
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
    if (mip != 0 || resource.baseLevel != 0) return nullptr;
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

DepthCopySourceScope::DepthCopySourceScope(bool active) : previous(depthCopySourceActive) {
    depthCopySourceActive = active;
}

DepthCopySourceScope::~DepthCopySourceScope() {
    depthCopySourceActive = previous;
}

std::array<std::uint32_t, 8> DepthCopySourceWords(const DepthTarget& target) {
    const bool multisampled = target.samples != 1u;
    const auto base = (target.address | target.pipeBankXor) >> 8u;
    const auto metadata = multisampled ? target.htileAddress >> 8u : 0u;
    const auto count = static_cast<std::uint32_t>(std::countr_zero(target.samples));
    const auto width = target.extent.width - 1u, height = target.extent.height - 1u;
    const auto type = multisampled ? 14u : 9u;
    const auto word6 = (multisampled && target.iterate256 ? 1u << 10u : 0u) | (multisampled && target.htilePipeAligned ? 1u << 19u : 0u) | (metadata != 0 ? 1u << 21u : 0u) | (static_cast<std::uint32_t>(metadata & 255u) << 24u);
    return {static_cast<std::uint32_t>(base), static_cast<std::uint32_t>((base >> 32u) & 255u) | (22u << 20u) | ((width & 3u) << 30u), (width >> 2u) | (height << 14u), (type << 28u) | (target.depthSwizzle << 20u) | (count << 16u) | 0x204u, 0u, count << 4u, word6, static_cast<std::uint32_t>(metadata >> 8u)};
}

bool DepthSurfaceStoreBack(const Context& context, const StorageTexture& storage) {
    std::lock_guard lock(surfacesMutex());
    for (auto& [key, entry] : layeredStorages()) {
        if (std::get<0>(key) != context.device || entry->storage.get() != &storage) continue;
        const auto slices = layeredSlices(context, *entry);
        entry->Copy(slices, false);
        for (const auto& slice : slices) {
            slice.surface->written = ++depthWrites;
            slice.surface->NoteHostWrite();
        }
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

bool DepthSurfaceOverlaps(VkDevice device, std::uint64_t address, std::uint64_t bytes) {
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && !surface->retired && surface->context.device == device && surface->Overlaps(address, bytes); });
}

void RetireDepthSurfaces(VkDevice device, std::uint64_t address, std::uint64_t bytes) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != device || surface->retired || !surface->Overlaps(address, bytes)) continue;
        const auto [stencilBegin, stencilEnd] = staleStencilRange(*surface);
        if (!(address <= stencilBegin && stencilEnd <= address + bytes)) storeStaleStencil(*surface);
        retire(*surface);
    }
}

void RetireCoveredDepthSurfaces(VkDevice device, std::span<const std::pair<std::uint64_t, std::uint64_t>> written, std::span<const WrittenStorageExtent> extents) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != device || surface->retired || !surface->CoveredBy(written, extents)) continue;
        if (!surface->PlaneCoveredBy(true, written, extents)) storeStaleStencil(*surface);
        retire(*surface);
    }
}

bool DirectDepthCopy(const Context& context, VkFormat depthFormat, VkImageAspectFlagBits aspect, VkFormat colorFormat) {
    if (!context.maintenance8 || !directDepthCopy.load(std::memory_order_relaxed)) return false;
    const auto bytes = aspectCopyBytes(depthFormat, aspect);
    return bytes != 0 && singleComponent(colorFormat, bytes);
}

std::uint64_t DirectDepthCopyCount() {
    return directDepthCopies.load(std::memory_order_relaxed);
}

void SetDirectDepthCopy(bool enabled) {
    directDepthCopy.store(enabled && std::getenv("APS5_NO_DIRECT_DEPTH_COPY") == nullptr, std::memory_order_relaxed);
}

}
