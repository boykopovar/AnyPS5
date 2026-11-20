#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
// For the declaration of PendingStorageOverlaps, defined below.
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ShaderCacheDirectory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeptUnits.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include <algorithm>
#include <filesystem>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>
#include <limits>
#include <exception>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#if defined(__APPLE__)
#include <unistd.h>
#endif

namespace AgcDriver::Graphics {

OwnedImage::~OwnedImage() {
    if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
    if (allocation) allocation.Reset();
    else ReleaseImageMemory(context, memory, memoryBytes, memoryType);
}

void OwnedImage::Allocate(const Context& device, const VkImageCreateInfo& imageInfo, const char* operation, bool newImage, bool budgeted) {
    Require(memory == VK_NULL_HANDLE && !allocation, "an owned image already has memory");
    allocation = AllocateImageMemory(device, image, imageInfo, operation, newImage, budgeted);
    memoryBytes = allocation.Size();
    memoryType = allocation.MemoryType();
    context.bufferPool = device.bufferPool;
}

namespace {

Recorder* recorderFor(const Context& context) {
    auto* recorder = Recorder::Active();
    return recorder != nullptr && recorder->Device() == context.device ? recorder : nullptr;
}

std::atomic<std::uint64_t>& eightBitWrites() {
    static std::atomic<std::uint64_t> writes{0};
    return writes;
}

// APS5_PROFILE_DRAW: accumulate texture setup phases and report every 200 textures.
struct TextureProfile {
    double allocate = 0, read = 0, gpu = 0, view = 0;
    std::uint64_t count = 0;
    std::uint64_t fromStorage = 0;
    double storageCreate = 0, storageWriteBack = 0, storageAlloc = 0, storageHostCopy = 0, storageGpu = 0, storageStore = 0;
    std::uint64_t storageCount = 0;
    std::uint64_t storageBytes = 0;
    std::uint64_t storageReused = 0;
    std::uint64_t storageDirectUploads = 0;
    std::uint64_t storageDirectWriteBacks = 0;
    // Sampled-texture uploads recorded into the open batch instead of waited for.
    std::uint64_t recordedUploads = 0;
    // GPU clears of fast-cleared storage images recorded into the open batch, and waited for in a
    // batch of their own (APS5_NO_RECORDED_CLEAR=1, or no recorder).
    std::uint64_t storageRecordedClears = 0;
    std::uint64_t storageWaitedClears = 0;
    std::uint64_t layerRows = 0;
    double layerRowsMs = 0;
};

TextureProfile& Profile() {
    static TextureProfile profile;
    return profile;
}

void keepStorageTexture(Recorder& recorder, std::shared_ptr<StorageTexture> texture) {
    if (texture == nullptr) return;
    if (!texture->Cached()) recorder.KeepBytes(texture.get(), static_cast<std::size_t>(texture->BudgetBytes()));
    recorder.Keep(std::move(texture));
}

// APS5_PROFILE_DRAW, every 10 s on the [storage] line: storage image bytes uploaded by path
// (recorded DCC clear, direct detile from the import, CPU) and written back by the reason of the
// FlushPending that forced the write-back (the other callers name themselves: "refresh", "cache
// eviction", "explicit"). `flushReason` is set by the caller around its writeBack calls.
thread_local const char* flushReason = nullptr;
// Likewise what made an upload necessary: "first" (a new image), "keys" (the DCC keys changed),
// "cpu" (a CPU store stamped a changed unit), "flushed" (another image's results were stored over
// the surface by this refresh), "store" (a driver store: a fill, a copy, a label, an earlier
// write-back), "untracked" (no write stamps for the surface: every refresh re-uploads), "alias"
// (units taken from an alias image on the device); set by the constructor and Refresh around
// upload() and borrowUnits().
thread_local const char* uploadReason = "first";
// Block-unit traffic, per 10 s like the rest of the [storage] line: partial uploads and write-backs
// (count and bytes), write-backs that stored nothing (every selected block CPU-written), partial
// selections widened to every pending unit (the hysteresis in writeBack), units dropped as dead
// (DiscardPendingInside) and units an alias's own results superseded (markLayersPending); relaxed,
// reported only.
std::atomic<std::uint64_t> singlePassMoves{0}, slabRetiles{0}, paddingSeedCopies{0}, linearImportRetiles{0};
std::atomic<std::uint64_t> partialUploads{0}, partialUploadBytes{0}, partialWriteBacks{0}, partialWriteBackBytes{0}, emptyWriteBacks{0}, coalescedWriteBacks{0}, unitsDropped{0}, unitsSuperseded{0}, clearHandOvers{0}, creationHandOvers{0}, handedOverSuperseded{0};
// FlushPending listings skipped because no pending unit of the image lay inside the range, images
// whose selected pending units were dropped because their memory is no longer registered (see
// writeBackLayers), and pending units a DCC clear -> uncompressed key flip kept as the texels
// (Refresh).
std::atomic<std::uint64_t> pretestSkipped{0}, unregisteredDropped{0}, keyFlipKept{0};
std::atomic<std::uint64_t> queuedReadHeld{0};
// Images stored by a FlushPending after a hook skip of theirs (AccessKeptByCpu), of which by the
// hook for the read site of the last skip; images evictStale dropped (the [hooksync] and [storage] lines).
std::atomic<std::uint64_t> flushedAfterSkip{0}, flushedAfterSkipSameSite{0}, staleEvicted{0};
std::atomic<std::uint64_t> depthCopyTextures{0}, depthCopyRefreshes{0};
std::atomic<std::uint64_t> reuseLookupsHeld{0}, reuseLookupsChanged{0};
std::atomic<std::uint64_t> refreshesProved{0};
std::atomic<std::uint64_t> refreshesProvedPendingKeys{0};
std::atomic<std::uint64_t> untrackedCompares{0};
std::atomic<std::uint64_t> rewatchedConfirms{0};
std::atomic<std::uint64_t> guestSnapshots{0};

struct StorageTraffic {
    AgcDriver::Mutex mutex;
    std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> writeBacks;
    std::array<std::pair<std::uint64_t, std::uint64_t>, 4> uploads{};
    std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> uploadReasons;
    std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> drawnStores;
    std::map<std::string, std::uint64_t> evictions;
    std::uint64_t directWriteBacks = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

StorageTraffic& Traffic() {
    static StorageTraffic traffic;
    return traffic;
}

struct DrawnPending {
    std::size_t images = 0;
    std::uint64_t bytes = 0;
    double oldestMs = 0;
};

DrawnPending drawnPending();

void reportStorageTraffic(StorageTraffic& traffic) {
    const auto now = std::chrono::steady_clock::now();
    if (now - traffic.lastReport < std::chrono::seconds(10)) return;
    traffic.lastReport = now;
    static constexpr const char* uploadNames[4] = {"clear", "direct", "cpu", "alias"};
    std::string line;
    char text[96];
    for (std::size_t i = 0; i < 4; ++i) {
        std::snprintf(text, sizeof(text), " %s %llu/%.1f", uploadNames[i], static_cast<unsigned long long>(traffic.uploads[i].first), traffic.uploads[i].second / 1048576.0);
        line += text;
        traffic.uploads[i] = {};
    }
    line += "; by reason:";
    for (const auto& [reason, totals] : traffic.uploadReasons) {
        std::snprintf(text, sizeof(text), " %llu/%.1f", static_cast<unsigned long long>(totals.first), totals.second / 1048576.0);
        line += " " + reason + text;
    }
    line += "; write-backs by reason (count/MiB):";
    for (const auto& [reason, totals] : traffic.writeBacks) {
        std::snprintf(text, sizeof(text), " %llu/%.1f", static_cast<unsigned long long>(totals.first), totals.second / 1048576.0);
        line += " " + reason + text;
    }
    line += "; evictions by reason (count/MiB flushed):";
    for (const auto& [reason, count] : traffic.evictions) {
        const auto flushed = traffic.writeBacks.find(reason);
        std::snprintf(text, sizeof(text), " %llu/%.1f", static_cast<unsigned long long>(count), flushed != traffic.writeBacks.end() ? flushed->second.second / 1048576.0 : 0.0);
        line += " " + reason + text;
    }
    std::string drawnLine;
    for (const auto& [reason, totals] : traffic.drawnStores) {
        std::snprintf(text, sizeof(text), " %llu/%.1f", static_cast<unsigned long long>(totals.first), totals.second / 1048576.0);
        drawnLine += " " + reason + text;
    }
    const auto drawn = drawnPending();
    const auto take = [](std::atomic<std::uint64_t>& counter) { return static_cast<unsigned long long>(counter.exchange(0, std::memory_order_relaxed)); };
    const auto partialUploadCount = take(partialUploads), partialWriteBackCount = take(partialWriteBacks);
    const auto uploadMiB = take(partialUploadBytes) / 1048576.0, writeBackMiB = take(partialWriteBackBytes) / 1048576.0;
    AgcDriver::ProfilePrint_nid_no_patch( "[storage] uploads by path (count/MiB, 10 s):%s; %llu write-backs GPU-direct, %llu stored nothing; block units: partial uploads %llu/%.1f, partial write-backs %llu/%.1f, %llu widened to every pending unit, units dropped %llu, superseded %llu, handed over at %llu clear-key refreshes and %llu image creations (%llu units superseded), dropped unregistered %llu, kept over a key flip %llu%s, pretest skipped %llu, held for a queued draw read %llu, evicted stale %llu; refreshes proved %llu, %llu of them over pending driver key stores (cumulative); drawn linear targets: stores by reason (count/MiB):%s, pending %zu images / %.1f MiB, oldest %.1f ms\n", line.c_str(), static_cast<unsigned long long>(traffic.directWriteBacks), take(emptyWriteBacks), partialUploadCount, uploadMiB, partialWriteBackCount, writeBackMiB, take(coalescedWriteBacks), take(unitsDropped), take(unitsSuperseded), take(clearHandOvers), take(creationHandOvers), take(handedOverSuperseded), take(unregisteredDropped), take(keyFlipKept), ShadowReport().c_str(), take(pretestSkipped), take(queuedReadHeld), take(staleEvicted), static_cast<unsigned long long>(refreshesProved.load(std::memory_order_relaxed)), static_cast<unsigned long long>(refreshesProvedPendingKeys.load(std::memory_order_relaxed)), drawnLine.c_str(), drawn.images, drawn.bytes / 1048576.0, drawn.oldestMs);
    traffic.writeBacks.clear();
    traffic.uploadReasons.clear();
    traffic.drawnStores.clear();
    traffic.evictions.clear();
    traffic.directWriteBacks = 0;
}

void countStorageUpload(std::size_t path, std::uint64_t bytes) {
    if (!LookupOutcomes::Profiled()) return;
    auto& traffic = Traffic();
    std::lock_guard lock(traffic.mutex);
    ++traffic.uploads[path].first;
    traffic.uploads[path].second += bytes;
    auto& reason = traffic.uploadReasons[uploadReason != nullptr ? uploadReason : "other"];
    ++reason.first;
    reason.second += bytes;
    reportStorageTraffic(traffic);
}

void countStorageWriteBack(std::uint64_t bytes, bool direct, bool drawn = false) {
    if (!LookupOutcomes::Profiled()) return;
    auto& traffic = Traffic();
    std::lock_guard lock(traffic.mutex);
    auto& totals = traffic.writeBacks[flushReason != nullptr ? flushReason : "other"];
    ++totals.first;
    totals.second += bytes;
    if (direct) ++traffic.directWriteBacks;
    if (drawn) {
        auto& stores = traffic.drawnStores[flushReason != nullptr ? flushReason : "other"];
        ++stores.first;
        stores.second += bytes;
    }
    reportStorageTraffic(traffic);
}

void countStorageEviction(const char* reason) {
    if (!LookupOutcomes::Profiled()) return;
    auto& traffic = Traffic();
    std::lock_guard lock(traffic.mutex);
    ++traffic.evictions[reason];
}

// Every storage image alive, for the fill HLE's cover check (StorageTexture::ClassifyFill): the
// storage cache indexes surfaces by key, not by address range. Its mutex is a leaf.
struct LiveImages {
    MapImportGate::LeafMutex<AgcDriver::Mutex> mutex;
    std::vector<StorageTexture*> textures;
};

std::atomic<void (*)()> publishHookForTest{nullptr};
std::atomic<void (*)(std::uint64_t, std::size_t)> pendingHookForTest{nullptr};

LiveImages& Live() {
    static LiveImages live;
    return live;
}

// Per-layer validity of array surfaces (see StorageTexture::layerBegin): a fill of one layer clears
// it on the image and only that layer's guest bytes go stale, so the other layers' fills, uploads
// and write-backs stay separate. APS5_NO_LAYER_CLEAR=1 tracks every surface as one layer, and a
// one-layer FillClear then demands the rest of the surface unchanged, as before.
bool LayerTrackingEnabled() {
    static const bool disabled = std::getenv("APS5_NO_LAYER_CLEAR") != nullptr;
    return !disabled;
}

// APS5_NO_BLOCK_TRACKING=1 tracks array layers (or the whole surface) instead of 64 KiB blocks.
bool BlockTrackingEnabled() {
    static const bool disabled = std::getenv("APS5_NO_BLOCK_TRACKING") != nullptr;
    return !disabled;
}

bool BlockWriteBackEach() {
    static const bool each = std::getenv("APS5_BLOCK_WRITEBACK_EACH") != nullptr || std::getenv("APS5_BLOCK_WRITEBACK_WIDEN") == nullptr;
    return each;
}

bool StorageBlockUnits(const GuestTextureResource& descriptor, const SurfaceGeometry& geometry) {
    return LayerTrackingEnabled() && BlockTrackingEnabled() && !geometry.thick && descriptor.tileMode != TextureTileMode::kLinear && descriptor.baseAddress % 65536 == 0
        && std::none_of(geometry.mips.begin(), geometry.mips.end(), [](const TileMipLayout& mip) { return mip.gcnSplitStride != 0; });
}

struct PhaseTimer {
    std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    double lap() {
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration<double, std::milli>(now - last).count();
        last = now;
        return ms;
    }
};


VkImageType ImageTypeFor(TextureDimension dimension) {
    return dimension == TextureDimension::k1D || dimension == TextureDimension::k1DArray ? VK_IMAGE_TYPE_1D : dimension == TextureDimension::k3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
}

VkImageViewType ViewTypeFor(TextureDimension dimension, [[maybe_unused]] std::uint32_t viewLayerCount) {
    switch (dimension) {
        case TextureDimension::k1D: return VK_IMAGE_VIEW_TYPE_1D;
        case TextureDimension::k2D: return VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::k2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        // Shaders address cube maps as 2D arrays of faces.
        case TextureDimension::kCube: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureDimension::k3D: return VK_IMAGE_VIEW_TYPE_3D;
        case TextureDimension::k1DArray: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case TextureDimension::k2DMsaa: break;
    }
    throw std::runtime_error("AGC graphics: Texture encountered an unknown guest texture dimension");
}

}

namespace {

void ChainMinLod(const Context& context, const GuestTextureResource& descriptor, VkImageViewCreateInfo& viewInfo, VkImageViewMinLodCreateInfoEXT& minLod) {
    const auto clamp = EffectiveMinLod(descriptor);
    if (clamp == 0.0f) return;
    Require(context.imageViewMinLod || context.shaderImageMinLod, "guest texture descriptor uses a minimum LOD clamp, which needs VK_EXT_image_view_min_lod, or shaderResourceMinLod without graphics pipeline libraries");
    if (!context.imageViewMinLod) return;
    minLod.minLod = clamp;
    minLod.pNext = viewInfo.pNext;
    viewInfo.pNext = &minLod;
}

struct ExpandImage {
    VkImage image = VK_NULL_HANDLE;
    std::shared_ptr<OwnedImage> owned;
    VkDeviceSize bytes = 0;
};

ExpandImage CreateExpandImage(const Context& context, const GuestTextureResource& descriptor, std::uint32_t depth, std::uint32_t layers) {
    ExpandImage result;
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | (descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u);
    imageInfo.imageType = ImageTypeFor(descriptor.dimension);
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {descriptor.width, descriptor.height, depth};
    imageInfo.mipLevels = descriptor.mipCount;
    imageInfo.arrayLayers = layers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    result.owned = std::make_shared<OwnedImage>(context, VK_NULL_HANDLE, VK_NULL_HANDLE);
    Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &result.image), "vkCreateImage sRGB copy");
    result.owned->image = result.image;
    result.owned->Allocate(context, imageInfo, "vkAllocateMemory sRGB copy", true, true);
    Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, result.image, result.owned->allocation.Memory(), result.owned->allocation.Offset()), "vkBindImageMemory sRGB copy");
    result.bytes = result.owned->memoryBytes;
    return result;
}

void RecordExpandBlit(const Context& context, VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage target, const GuestTextureResource& descriptor, std::uint32_t depth, std::uint32_t layers) {
    std::vector<VkImageBlit> regions;
    for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
        const VkOffset3D end{static_cast<std::int32_t>(std::max(descriptor.width >> level, 1u)), static_cast<std::int32_t>(std::max(descriptor.height >> level, 1u)), static_cast<std::int32_t>(descriptor.dimension == TextureDimension::k3D ? std::max(depth >> level, 1u) : 1u)};
        VkImageBlit region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, layers};
        region.srcOffsets[1] = end;
        region.dstSubresource = region.srcSubresource;
        region.dstOffsets[1] = end;
        regions.push_back(region);
    }
    context.Function<PFN_vkCmdBlitImage>("vkCmdBlitImage")(commands, source, sourceLayout, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data(), VK_FILTER_NEAREST);
}

std::atomic<std::uint64_t>& SampledMemoryCounter() {
    static std::atomic<std::uint64_t> bytes{0};
    return bytes;
}

}

std::uint64_t SampledTextureMemory() {
    return SampledMemoryCounter().load(std::memory_order_relaxed);
}

static std::uint32_t UnmappedLevelsBelowMinLod(const GuestTextureResource& descriptor, const SurfaceGeometry& geometry) {
    if (descriptor.baseLevel != 0 || (descriptor.dimension != TextureDimension::k2D && descriptor.dimension != TextureDimension::k2DArray) || geometry.thick) return 0;
    const auto lastViewLevel = std::min(descriptor.lastLevel, descriptor.mipCount - 1u);
    const auto clamped = std::min(static_cast<std::uint32_t>(EffectiveMinLod(descriptor)), lastViewLevel);
    std::uint32_t levels = 0;
    for (; levels < clamped && levels < geometry.mips.size() && !geometry.mips[levels].tail; ++levels) {
        const auto& mip = geometry.mips[levels];
        for (std::uint32_t layer = 0; layer < geometry.layers; ++layer) {
            if (!geometry.HasLayer(levels, layer)) continue;
            if (!GuestMemory::CommittedRanges(descriptor.baseAddress + geometry.GuestOffset(layer, mip), static_cast<std::size_t>(mip.tiledSize), false).empty()) return levels;
        }
    }
    return levels;
}

static bool SparseLevelsSupported(const Context& context, const VkImageCreateInfo& info) {
    if (!context.sparseResidencyImage2D || !context.imageViewMinLod || context.sparseImageFormatProperties == nullptr || info.imageType != VK_IMAGE_TYPE_2D) return false;
    static const bool disabled = std::getenv("APS5_NO_SPARSE_TEXTURE_LEVELS") != nullptr;
    if (disabled) return false;
    if ((context.sparseQueue == VK_NULL_HANDLE || context.sparseQueueMutex == nullptr) && !GuestMemory::GpuMutex().HeldByThisThread()) return false;
    VkImageFormatProperties limits{};
    if (context.imageFormatProperties(context.physical, info.format, info.imageType, info.tiling, info.usage, info.flags | VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT, &limits) != VK_SUCCESS) return false;
    if (limits.maxMipLevels < info.mipLevels || limits.maxArrayLayers < info.arrayLayers || limits.maxExtent.width < info.extent.width || limits.maxExtent.height < info.extent.height) return false;
    std::uint32_t count = 0;
    context.sparseImageFormatProperties(context.physical, info.format, info.imageType, VK_SAMPLE_COUNT_1_BIT, info.usage, info.tiling, &count, nullptr);
    std::vector<VkSparseImageFormatProperties> properties(count);
    context.sparseImageFormatProperties(context.physical, info.format, info.imageType, VK_SAMPLE_COUNT_1_BIT, info.usage, info.tiling, &count, properties.data());
    return std::any_of(properties.begin(), properties.begin() + count, [](const VkSparseImageFormatProperties& format) { return (format.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0; });
}

std::optional<SparseLevelBinds> PlanSparseLevels(VkExtent3D extent, std::uint32_t mipLevels, std::uint32_t arrayLayers, VkDeviceSize alignment, std::span<const VkSparseImageMemoryRequirements> requirements, std::uint32_t firstLevel) {
    const VkSparseImageMemoryRequirements* color = nullptr;
    for (const auto& entry : requirements) {
        const auto aspects = entry.formatProperties.aspectMask;
        if (aspects == VK_IMAGE_ASPECT_COLOR_BIT && color == nullptr) color = &entry;
        else if (aspects != VK_IMAGE_ASPECT_METADATA_BIT) return std::nullopt;
    }
    if (color == nullptr || alignment == 0 || color->formatProperties.imageGranularity.width == 0 || color->formatProperties.imageGranularity.height == 0) return std::nullopt;
    const auto aligned = [&](VkDeviceSize size) { return (size + alignment - 1u) / alignment * alignment; };
    const auto granularity = color->formatProperties.imageGranularity;
    const auto tailFirst = std::min(color->imageMipTailFirstLod, mipLevels);
    const bool singleTail = (color->formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) != 0;
    SparseLevelBinds plan;
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        for (auto level = firstLevel; level < tailFirst; ++level) {
            const auto width = std::max(extent.width >> level, 1u);
            const auto height = std::max(extent.height >> level, 1u);
            const VkDeviceSize blocks = static_cast<VkDeviceSize>((width + granularity.width - 1u) / granularity.width) * ((height + granularity.height - 1u) / granularity.height);
            plan.levels.push_back({{VK_IMAGE_ASPECT_COLOR_BIT, level, layer}, {0, 0, 0}, {width, height, 1u}, VK_NULL_HANDLE, plan.bytes, 0});
            plan.bytes += blocks * alignment;
        }
        if (tailFirst < mipLevels && (!singleTail || layer == 0)) {
            plan.tails.push_back({color->imageMipTailOffset + (singleTail ? 0u : static_cast<VkDeviceSize>(layer) * color->imageMipTailStride), color->imageMipTailSize, VK_NULL_HANDLE, plan.bytes, 0});
            plan.bytes += aligned(color->imageMipTailSize);
        }
    }
    for (const auto& entry : requirements) {
        if (entry.formatProperties.aspectMask != VK_IMAGE_ASPECT_METADATA_BIT) continue;
        const bool single = (entry.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) != 0;
        for (std::uint32_t layer = 0; layer < (single ? 1u : arrayLayers); ++layer) {
            plan.tails.push_back({entry.imageMipTailOffset + static_cast<VkDeviceSize>(layer) * entry.imageMipTailStride, entry.imageMipTailSize, VK_NULL_HANDLE, plan.bytes, VK_SPARSE_MEMORY_BIND_METADATA_BIT});
            plan.bytes += aligned(entry.imageMipTailSize);
        }
    }
    if (plan.bytes == 0) return std::nullopt;
    return plan;
}

static std::vector<VkSparseImageMemoryRequirements> SparseRequirements(const Context& context, VkImage image) {
    std::uint32_t count = 0;
    context.Function<PFN_vkGetImageSparseMemoryRequirements>("vkGetImageSparseMemoryRequirements")(context.device, image, &count, nullptr);
    std::vector<VkSparseImageMemoryRequirements> sparse(count);
    context.Function<PFN_vkGetImageSparseMemoryRequirements>("vkGetImageSparseMemoryRequirements")(context.device, image, &count, sparse.data());
    sparse.resize(count);
    return sparse;
}

static VkDeviceSize BindSparseLevels(const Context& context, OwnedImage& owned, SparseLevelBinds& plan, const VkMemoryRequirements& requirements) {
    auto& levels = plan.levels;
    auto& tails = plan.tails;
    const auto bytes = plan.bytes;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = bytes;
    allocation.memoryTypeIndex = context.DeviceMemoryType(requirements.memoryTypeBits, bytes, "texture");
    owned.memory = AllocateImageMemory(context, allocation, requirements.memoryTypeBits, "vkAllocateMemory sparse texture levels", &owned.memoryType, true);
    owned.memoryBytes = bytes;
    owned.context.bufferPool = context.bufferPool;
    for (auto& bind : levels) bind.memory = owned.memory;
    for (auto& bind : tails) bind.memory = owned.memory;
    const VkSparseImageMemoryBindInfo levelBinds{owned.image, static_cast<std::uint32_t>(levels.size()), levels.data()};
    const VkSparseImageOpaqueMemoryBindInfo tailBinds{owned.image, static_cast<std::uint32_t>(tails.size()), tails.data()};
    VkBindSparseInfo bind{VK_STRUCTURE_TYPE_BIND_SPARSE_INFO};
    bind.imageBindCount = levels.empty() ? 0u : 1u;
    bind.pImageBinds = &levelBinds;
    bind.imageOpaqueBindCount = tails.empty() ? 0u : 1u;
    bind.pImageOpaqueBinds = &tailBinds;
    const bool ownQueue = context.sparseQueue != VK_NULL_HANDLE && context.sparseQueueMutex != nullptr;
    const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence), "vkCreateFence sparse texture levels");
    VkResult submitted = VK_SUCCESS;
    VkResult waited = VK_SUCCESS;
    {
        std::unique_lock<std::mutex> queueLock;
        std::unique_lock<GuestMemory::GpuMutexType> gpuLock;
        if (ownQueue) queueLock = std::unique_lock(*context.sparseQueueMutex);
        else gpuLock = std::unique_lock(GuestMemory::GpuMutex());
        submitted = context.Function<PFN_vkQueueBindSparse>("vkQueueBindSparse")(ownQueue ? context.sparseQueue : context.queue, 1, &bind, fence);
        if (submitted == VK_SUCCESS) waited = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    Check(submitted, "vkQueueBindSparse sparse texture levels");
    Check(waited, "vkWaitForFences sparse texture levels");
    return bytes;
}

Texture::Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot, bool depthCompare, bool layerRows, const VkClearColorValue* clear, std::optional<std::span<const std::uint32_t>> packedSlabs, const HostImport* source) : Texture(context, detiler, descriptor, components, snapshot, nullptr, depthCompare, layerRows, clear, packedSlabs, source) {}

Texture::Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::shared_ptr<Buffer> staging, bool depthCompare, bool layerRows, std::optional<std::span<const std::uint32_t>> packedSlabs) : Texture(context, detiler, descriptor, components, {}, std::move(staging), depthCompare, layerRows, nullptr, packedSlabs, nullptr) {}

Texture::Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot, std::shared_ptr<Buffer> staging, bool depthCompare, bool layerRows, const VkClearColorValue* clear, std::optional<std::span<const std::uint32_t>> packedSlabs, const HostImport* source) : context(context) {

    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    const auto started = profile && layerRows ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    try {
        const auto colorFormat = SampledTextureFormat(context, descriptor.format);
        const bool srgbExpand = !depthCompare && SrgbExpandsToRgba8(context, descriptor.format);
        const bool nativeDepth = colorFormat == VK_FORMAT_D24_UNORM_S8_UINT || colorFormat == VK_FORMAT_D32_SFLOAT_S8_UINT;
        Require(!depthCompare || nativeDepth || colorFormat == VK_FORMAT_R32_SFLOAT || colorFormat == VK_FORMAT_R16_UNORM, "comparison sampling requires an R32 float or R16 unorm depth texture");
        Require(!depthCompare || descriptor.dimension != TextureDimension::k3D, "comparison sampling does not support 3D depth textures");
        const auto vkFormat = depthCompare && !nativeDepth ? (colorFormat == VK_FORMAT_R32_SFLOAT ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_D16_UNORM) : colorFormat;
        const VkImageAspectFlags aspect = depthCompare || nativeDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        const VkImageAspectFlags barrierAspect = nativeDepth ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : aspect;
        const bool depthToFloat = descriptor.format == 141u && colorFormat == VK_FORMAT_R32_SFLOAT;
        if (IsBlockCompressed(descriptor.format)) {
            Require(context.textureCompressionBC, "device does not support BC compressed textures");
        }
        if (layerRows) {
            Require(!depthCompare && descriptor.dimension == TextureDimension::k1DArray, "only a sampled 1D array is laid out as rows of a 2D image");
            Require(!srgbExpand, "a 1D array laid out as rows of a 2D image in a format expanded to RGBA8 is not implemented");
            Require(!IsBlockCompressed(descriptor.format), "a block-compressed 1D array laid out as rows of a 2D image is not implemented");
            Require(descriptor.baseLevel == 0u && std::min(descriptor.lastLevel, descriptor.mipCount - 1u) == 0u, "a 1D array past maxImageArrayLayers viewed at mip levels other than level 0 alone is not implemented");
        }

        const auto geometry = DescribeSurface(descriptor);
        const auto& mips = geometry.mips;
        const auto arrayLayers = geometry.layers;
        const auto elementBytes = BytesPerElement(descriptor.format);
#ifdef APS5_DEBUG_LOG
        AGC_LOG_DEFERRED(stdout, "Texture address=0x%llx %ux%u mips=%u layers=%u dim=%d tile=%d format=%u vk=%d element=%u", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, arrayLayers,
                     static_cast<int>(descriptor.dimension), static_cast<int>(descriptor.tileMode), descriptor.format, static_cast<int>(vkFormat), elementBytes);
        for (const auto& mip : mips) AGC_LOG_DEFERRED(stdout, "  mip %ux%u tiled=0x%llx+0x%llx linear=0x%llx+0x%llx blocksPerRow=%u pitch=%u tail=%d", mip.width, mip.height, static_cast<unsigned long long>(mip.tiledOffset), static_cast<unsigned long long>(mip.tiledSize), static_cast<unsigned long long>(mip.linearOffset), static_cast<unsigned long long>(mip.linearSize), mip.blocksPerRow, mip.pitchBytes, mip.tail ? 1 : 0);
#endif

        const auto guestBytes = geometry.guestBytes;
        AgcDriver::LiveResourceRing::Instance().Note(descriptor.baseAddress, guestBytes, "texture");
        const auto slabBytes = geometry.layerBytes;
        const auto snapshotSize = source != nullptr ? guestBytes : staging != nullptr ? staging->Bytes().size() : snapshot.size();
        Require(clear == nullptr || !packedSlabs.has_value(), "a cleared texture takes no snapshot, so it has no committed slabs");
        Require(clear != nullptr || source != nullptr ? snapshot.empty() && staging == nullptr : packedSlabs.has_value() ? slabBytes != 0 && snapshotSize == packedSlabs->size() * slabBytes : snapshotSize == guestBytes, "texture snapshot size mismatch");
        Require(source == nullptr || !packedSlabs.has_value(), "a texture uploaded from a host import covers the whole surface, so it has no committed slabs");
        Require(source == nullptr || (descriptor.baseAddress >= source->base && descriptor.baseAddress - source->base + guestBytes <= source->bytes), "texture import source does not cover the surface");
        Require(clear == nullptr || (!depthCompare && !srgbExpand && !IsRepackedTextureFormat(descriptor.format) && !IsBlockCompressed(descriptor.format)), "a cleared texture needs a plain color format");
        Require(!packedSlabs.has_value() || (std::ranges::adjacent_find(*packedSlabs, std::greater_equal<>{}) == packedSlabs->end() && (packedSlabs->empty() || packedSlabs->back() < guestBytes / slabBytes)), "texture snapshot slabs are not ascending slabs of the surface");
        Require(!packedSlabs.has_value() || !geometry.levelMajor, "a partially committed snapshot of a PS4 layered surface with several mip levels (stored level by level) is not implemented");

        const auto sliceLinearBytes = geometry.sliceLinearBytes;
        Require(arrayLayers == 0 || sliceLinearBytes <= UINT64_MAX / arrayLayers, "detiled texture buffer size overflows");
        struct UploadedLayer {
            std::uint32_t layer;
            std::uint64_t tiledOffset;
            std::uint64_t linearOffset;
        };
        std::vector<UploadedLayer> uploaded;
        std::vector<std::uint32_t> zeroed;
        uploaded.reserve(arrayLayers);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            if (!packedSlabs.has_value()) {
                uploaded.push_back({layer, geometry.GuestLayerOffset(layer), geometry.LinearLayerOffset(layer)});
                continue;
            }
            const auto slab = geometry.GuestLayerOffset(layer) / slabBytes;
            const auto packed = std::ranges::lower_bound(*packedSlabs, slab);
            if (packed == packedSlabs->end() || *packed != slab) zeroed.push_back(layer);
            else uploaded.push_back({layer, static_cast<std::uint64_t>(packed - packedSlabs->begin()) * slabBytes, uploaded.size() * sliceLinearBytes});
        }
        const auto linearBytes = sliceLinearBytes * uploaded.size();

        const bool depth1D = depthCompare && (descriptor.dimension == TextureDimension::k1D || descriptor.dimension == TextureDimension::k1DArray);
        const auto rowLayers = layerRows ? geometry.imageLayers - descriptor.baseArray : 0u;
        Require(!layerRows || (descriptor.baseArray < geometry.imageLayers && rowLayers <= context.limits.maxImageDimension2D / 2u), "a 1D array past maxImageArrayLayers whose viewed slices need more than maxImageDimension2D rows is not implemented");
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.flags = (descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u) | (SrgbTwinFormat(context, vkFormat) != VK_FORMAT_UNDEFINED ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0u);
        imageInfo.imageType = depth1D || layerRows ? VK_IMAGE_TYPE_2D : ImageTypeFor(descriptor.dimension);
        imageInfo.format = vkFormat;
        imageInfo.extent = {descriptor.width, layerRows ? rowLayers * 2u : depth1D ? 1u : descriptor.height, geometry.imageDepth};
        imageInfo.mipLevels = layerRows ? 1u : descriptor.mipCount;
        Require(layerRows || geometry.imageLayers <= context.limits.maxImageArrayLayers, "a texture array with more layers than maxImageArrayLayers is not implemented");
        imageInfo.arrayLayers = layerRows ? 1u : geometry.imageLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (srgbExpand ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u);
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        const auto unmapped = depthCompare || srgbExpand || clear != nullptr ? 0u : UnmappedLevelsBelowMinLod(descriptor, geometry);
        auto skip = unmapped != 0 && SparseLevelsSupported(context, imageInfo) ? unmapped : 0u;
        const auto denseInfo = imageInfo;
        const std::array<std::uint32_t, 2> families{context.queueFamily, context.sparseFamily};
        if (skip != 0) {
            imageInfo.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
            if (context.sparseQueue != VK_NULL_HANDLE && context.sparseFamily != context.queueFamily) {
                imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
                imageInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(families.size());
                imageInfo.pQueueFamilyIndices = families.data();
            }
        }
        owned = std::make_shared<OwnedImage>(context, VK_NULL_HANDLE, VK_NULL_HANDLE);
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage");
        owned->image = image;
        std::optional<SparseLevelBinds> sparseBinds;
        if (skip != 0) {
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            sparseBinds = PlanSparseLevels(imageInfo.extent, imageInfo.mipLevels, imageInfo.arrayLayers, requirements.alignment, SparseRequirements(context, image), skip);
            if (sparseBinds) {
                allocationBytes = BindSparseLevels(context, *owned, *sparseBinds, requirements);
            } else {
                owned.reset();
                image = VK_NULL_HANDLE;
                skip = 0;
                imageInfo = denseInfo;
                owned = std::make_shared<OwnedImage>(context, VK_NULL_HANDLE, VK_NULL_HANDLE);
                Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage");
                owned->image = image;
            }
        }
        if (!sparseBinds) {
            owned->Allocate(context, imageInfo, "vkAllocateMemory texture", true, true);
            allocationBytes = owned->memoryBytes;
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, owned->allocation.Memory(), owned->allocation.Offset()), "vkBindImageMemory");
        }

        const VkImage uploadImage = image;
        std::shared_ptr<OwnedImage> uploadOwned;
        if (srgbExpand) {
            auto expanded = CreateExpandImage(context, descriptor, geometry.imageDepth, geometry.imageLayers);
            uploadOwned = std::move(owned);
            owned = std::move(expanded.owned);
            image = expanded.image;
            allocationBytes = expanded.bytes;
        }

        if (clear != nullptr) {
            static const bool syncUploads = std::getenv("APS5_SYNC_TEXTURE_UPLOAD") != nullptr;
            auto* recorder = syncUploads ? nullptr : Recorder::Active();
            std::unique_ptr<CommandBatch> batch;
            VkCommandBuffer commands = VK_NULL_HANDLE;
            if (recorder != nullptr) {
                commands = recorder->Commands();
                recorder->Keep(owned);
            } else {
                batch = std::make_unique<CommandBatch>(context);
                commands = batch->Handle();
            }
            const VkImageSubresourceRange range{aspect, 0, imageInfo.mipLevels, 0, imageInfo.arrayLayers};
            VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = image;
            toTransferDst.subresourceRange = range;
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransferDst);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clear, 1, &range);
            VkImageMemoryBarrier toShaderRead = toTransferDst;
            toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShaderRead);
            if (batch) batch->SubmitAndWait();
            else ++Profile().recordedUploads;
            if (profile) Profile().gpu += timer.lap();
        } else {
            // Debug aid: APS5_DUMP_TEXTURE=<hex addresses, comma separated> saves the detiled first mip
            // of those sampled textures on their first 8 uploads as texture_<address>_<n>.raw (u32
            // width, height, VkFormat, then tightly packed rows).
            static const std::string dumpList = [] { const char* text = std::getenv("APS5_DUMP_TEXTURE"); return text ? std::string(text) : std::string(); }();
            bool dumpWanted = false;
            if (!dumpList.empty()) {
                char address[32];
                std::snprintf(address, sizeof(address), "%llx", static_cast<unsigned long long>(descriptor.baseAddress));
                char extent[32];
                std::snprintf(extent, sizeof(extent), "%ux%u", descriptor.width, descriptor.height);
                // Entries may also be extents ("3840x2160"), since heap addresses change between runs.
                dumpWanted = dumpList.find(address) != std::string::npos || dumpList.find(extent) != std::string::npos;
            }
            static const bool syncUploads = std::getenv("APS5_SYNC_TEXTURE_UPLOAD") != nullptr;
            auto* recorder = dumpWanted || syncUploads ? nullptr : Recorder::Active();
            if (staging == nullptr && source == nullptr && !uploaded.empty()) {
                staging = std::make_shared<Buffer>(context, snapshot.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                std::memcpy(staging->Bytes().data(), snapshot.data(), snapshot.size());
            }
            const auto zeros = zeroed.empty() ? nullptr : std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(sliceLinearBytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            if (profile) Profile().allocate += timer.lap();
            if (profile) Profile().read += timer.lap();

            detiler.BeginBatch();
            // The upload is recorded into the open batch, like a storage image's: the work that samples
            // the texture is recorded after it, and a batch of its own (SubmitAndWait) was a full GPU
            // drain under the device lock for every new texture. The buffers and the image live with
            // the batch (the cache may drop the texture before it completes). The dump aid needs the
            // linear bytes on the CPU, so it keeps the waiting batch, as does a build without a
            // recorder (tests). APS5_SYNC_TEXTURE_UPLOAD=1 restores the waiting batch for every upload.
            std::unique_ptr<CommandBatch> batch;
            VkCommandBuffer commands = VK_NULL_HANDLE;
            std::shared_ptr<DeviceBuffer> tiled;
            std::shared_ptr<DeviceBuffer> linear;
            if (recorder != nullptr) {
                commands = recorder->Commands();
                if (staging != nullptr || source != nullptr) {
                    tiled = recorder->UploadScratchBuffer(Recorder::UploadScratch::Tiled, snapshotSize);
                    linear = recorder->UploadScratchBuffer(Recorder::UploadScratch::Linear, static_cast<std::size_t>(linearBytes));
                    if (staging != nullptr) recorder->Keep(staging, snapshotSize);
                }
                if (zeros != nullptr) recorder->Keep(zeros, static_cast<std::size_t>(sliceLinearBytes));
                recorder->Keep(owned);
                if (uploadOwned != nullptr) recorder->Keep(uploadOwned);
            } else {
                batch = std::make_unique<CommandBatch>(context);
                commands = batch->Handle();
                if (staging != nullptr || source != nullptr) {
                    tiled = std::make_shared<DeviceBuffer>(context, snapshotSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                    linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                }
            }

            if (source != nullptr) {
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                CopyBuffer(context, commands, source->buffer, HostImportOffset(*source, descriptor.baseAddress), tiled->Handle(), 0, guestBytes);
            } else {
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT | Recorder::UploadScratchReuseStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
                if (staging != nullptr) CopyBuffer(context, commands, staging->Handle(), 0, tiled->Handle(), 0, snapshotSize);
            }
            if (zeros != nullptr) context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, zeros->Handle(), 0, VK_WHOLE_SIZE, 0u);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            for (const auto& slot : uploaded) {
                for (std::uint32_t level = skip; level < mips.size(); ++level) {
                    if (!geometry.HasLayer(level, slot.layer)) continue;
                    const auto& mip = mips[level];
                    detiler.Dispatch(commands, descriptor.tileMode, elementBytes, tiled->Handle(), packedSlabs.has_value() ? slot.tiledOffset + mip.tiledOffset : geometry.GuestOffset(slot.layer, mip), linear->Handle(), slot.linearOffset + mip.linearOffset, mip, false, slot.layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
                }
            }
            if (IsRepackedTextureFormat(descriptor.format) || depthToFloat) {
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                for (const auto& slot : uploaded) {
                    for (std::uint32_t level = skip; level < mips.size(); ++level) {
                        if (!geometry.HasLayer(level, slot.layer)) continue;
                        const auto offset = slot.linearOffset + mips[level].linearOffset;
                        detiler.DispatchRepack(commands, linear->Handle(), offset, linear->Handle(), offset, mips[level].linearSize, depthToFloat);
                    }
                }
            }

            VkBufferMemoryBarrier linearReadBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            linearReadBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            linearReadBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            linearReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearReadBarrier.buffer = linear != nullptr ? linear->Handle() : VK_NULL_HANDLE;
            linearReadBarrier.offset = 0;
            linearReadBarrier.size = VK_WHOLE_SIZE;
            std::vector<VkBufferMemoryBarrier> readBarriers;
            if (linear != nullptr) readBarriers.push_back(linearReadBarrier);
            if (zeros != nullptr) {
                auto zerosReadBarrier = linearReadBarrier;
                zerosReadBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                zerosReadBarrier.buffer = zeros->Handle();
                readBarriers.push_back(zerosReadBarrier);
            }

            VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransferDst.srcAccessMask = 0;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = uploadImage;
            toTransferDst.subresourceRange = layerRows ? VkImageSubresourceRange{barrierAspect, 0, 1, 0, 1} : VkImageSubresourceRange{barrierAspect, 0, descriptor.mipCount, 0, geometry.imageLayers};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | (zeros != nullptr ? VK_PIPELINE_STAGE_TRANSFER_BIT : 0u), VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, static_cast<std::uint32_t>(readBarriers.size()), readBarriers.data(), 1, &toTransferDst);

            const auto layerRegions = [&](std::uint32_t layer, std::uint64_t linearLayerOffset, std::vector<VkBufferImageCopy>& regions) {
                for (std::uint32_t level = skip; level < descriptor.mipCount; ++level) {
                    if (!geometry.HasLayer(level, layer)) continue;
                    const auto& mip = mips[level];
                    VkBufferImageCopy region{};
                    region.bufferOffset = linearLayerOffset + mip.linearOffset;
                    region.bufferRowLength = mip.pitchBytes / BytesPerElement(descriptor.format) * BlockWidth(descriptor.format);
                    region.bufferImageHeight = 0;
                    region.imageSubresource = {aspect, level, geometry.CopyLayer(layer), 1};
                    region.imageOffset = {0, 0, geometry.CopyDepth(layer)};
                    region.imageExtent = {std::max(descriptor.width >> level, 1u), std::max(descriptor.height >> level, 1u), 1u};
                    regions.push_back(region);
                }
            };
            std::vector<VkBufferImageCopy> regions;
            regions.reserve(uploaded.size() * mips.size() + rowLayers * 2u);
            for (std::uint32_t row = 0; row < rowLayers * 2u; ++row) {
                const auto layer = descriptor.baseArray + row / 2u;
                VkBufferImageCopy region{};
                region.bufferOffset = static_cast<std::uint64_t>(layer) * sliceLinearBytes + mips[0].linearOffset;
                region.bufferRowLength = mips[0].pitchBytes / BytesPerElement(descriptor.format) * BlockWidth(descriptor.format);
                region.imageSubresource = {aspect, 0, 0, 1};
                region.imageOffset = {0, static_cast<std::int32_t>(row), 0};
                region.imageExtent = {descriptor.width, 1u, 1u};
                regions.push_back(region);
            }
            if (!layerRows) {
                for (const auto& slot : uploaded) layerRegions(slot.layer, slot.linearOffset, regions);
            }
            if (linear != nullptr) context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), uploadImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
            std::vector<VkBufferImageCopy> zeroRegions;
            zeroRegions.reserve(zeroed.size() * mips.size());
            for (const auto layer : zeroed) layerRegions(layer, 0, zeroRegions);
            if (zeros != nullptr) context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, zeros->Handle(), uploadImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(zeroRegions.size()), zeroRegions.data());
            if (srgbExpand) {
                std::array<VkImageMemoryBarrier, 2> toBlit{};
                toBlit[0] = toTransferDst;
                toBlit[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                toBlit[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                toBlit[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toBlit[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                toBlit[1] = toTransferDst;
                toBlit[1].image = image;
                context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, toBlit.data());
                RecordExpandBlit(context, commands, uploadImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, descriptor, geometry.imageDepth, geometry.imageLayers);
            }
            std::unique_ptr<Buffer> dump;
            if (dumpWanted && linear != nullptr) {
                dump = std::make_unique<Buffer>(context, static_cast<std::size_t>(mips[0].linearSize), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                CopyBuffer(context, commands, linear->Handle(), mips[0].linearOffset, dump->Handle(), 0, mips[0].linearSize);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            }

            VkImageMemoryBarrier toShaderRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShaderRead.image = image;
            toShaderRead.subresourceRange = toTransferDst.subresourceRange;
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShaderRead);

            if (batch) batch->SubmitAndWait();
            else ++Profile().recordedUploads;
            if (profile) Profile().gpu += timer.lap();
            if (dump) {
                static AgcDriver::Mutex dumpMutex;
                static std::map<std::uint64_t, int> dumped;
                std::lock_guard lock(dumpMutex);
                auto& count = dumped[descriptor.baseAddress];
                if (count < 8) {
                    char name[64];
                    std::snprintf(name, sizeof(name), "texture_%llx_%d.raw", static_cast<unsigned long long>(descriptor.baseAddress), count++);
                    if (std::FILE* file = std::fopen(name, "wb")) {
                        const std::uint32_t header[3] = {mips[0].pitchBytes / static_cast<std::uint32_t>(elementBytes), mips[0].height, static_cast<std::uint32_t>(vkFormat)};
                        std::fwrite(header, sizeof(header), 1, file);
                        std::fwrite(dump->Bytes().data(), 1, dump->Bytes().size(), file);
                        std::fclose(file);
                        std::fprintf(stderr, "[texture] dumped %s (%ux%u VkFormat %d, tile %d)\n", name, header[0], header[1], static_cast<int>(vkFormat), static_cast<int>(descriptor.tileMode));
                    }
                }
            }
        }

        const auto viewLevelCount = std::min(descriptor.lastLevel, descriptor.mipCount - 1u) - descriptor.baseLevel + 1u;
        const auto viewLayerCount = geometry.imageLayers - descriptor.baseArray;
        if (descriptor.dimension == TextureDimension::kCube) {
            Require(viewLayerCount >= 6u, "guest cube texture view holds fewer than 6 array slices");
        }

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = layerRows ? VK_IMAGE_VIEW_TYPE_2D : depth1D ? (descriptor.dimension == TextureDimension::k1DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D) : ViewTypeFor(descriptor.dimension, viewLayerCount);
        viewInfo.format = srgbExpand ? VK_FORMAT_R8G8B8A8_SRGB : vkFormat;
        viewFormat = viewInfo.format;
        viewInfo.components = depthCompare ? VkComponentMapping{} : components;
        viewInfo.subresourceRange = layerRows ? VkImageSubresourceRange{aspect, 0, 1, 0, 1} : VkImageSubresourceRange{aspect, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        ChainMinLod(context, descriptor, viewInfo, minLod);

        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView");
        viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
        createFirstLayerView(descriptor, viewInfo);
        prepareBaseLevelView(descriptor, viewInfo);
        prepareSrgbView(descriptor, viewInfo);
        if (profile) {
            auto& totals = Profile();
            totals.view += timer.lap();
            if (++totals.count % 200 == 0) std::fprintf(stderr, "[texture] %llu textures (%llu copied from storage images, %llu uploads recorded): allocate+image %.0f ms, guest read %.0f ms, detile+copy %.0f ms, view+buffer release %.0f ms\n", static_cast<unsigned long long>(totals.count), static_cast<unsigned long long>(totals.fromStorage), static_cast<unsigned long long>(totals.recordedUploads), totals.allocate, totals.read, totals.gpu, totals.view);
            if (layerRows) {
                const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
                totals.layerRowsMs += ms;
                std::fprintf(stderr, "[texture] layer rows 0x%llx: %u slices of %u texels as a %ux%u image in %.2f ms (%llu layer-row textures, %.0f ms in all)\n", static_cast<unsigned long long>(descriptor.baseAddress), rowLayers, descriptor.width, descriptor.width, rowLayers * 2u, ms, static_cast<unsigned long long>(++totals.layerRows), totals.layerRowsMs);
            }
        }
        countedBytes = allocationBytes;
        SampledMemoryCounter().fetch_add(countedBytes, std::memory_order_relaxed);
    } catch (...) {
        release();
        throw;
    }
}

}
