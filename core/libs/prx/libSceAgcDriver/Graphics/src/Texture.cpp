#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
// For the declaration of PendingStorageOverlaps, defined below.
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
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
#include <optional>
#include <unordered_map>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

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
};

TextureProfile& Profile() {
    static TextureProfile profile;
    return profile;
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
std::atomic<std::uint64_t> partialUploads{0}, partialUploadBytes{0}, partialWriteBacks{0}, partialWriteBackBytes{0}, emptyWriteBacks{0}, coalescedWriteBacks{0}, unitsDropped{0}, unitsSuperseded{0};
// FlushPending listings skipped because no pending unit of the image lay inside the range, images
// whose selected pending units were dropped because their memory is no longer registered (see
// writeBackLayers), and pending units a DCC clear -> uncompressed key flip kept as the texels
// (Refresh).
std::atomic<std::uint64_t> pretestSkipped{0}, unregisteredDropped{0}, keyFlipKept{0};
// Images stored by a FlushPending after a hook skip of theirs (AccessKeptByCpu), of which by the
// hook for the read site of the last skip; images evictStale dropped (the [hooksync] and [storage] lines).
std::atomic<std::uint64_t> flushedAfterSkip{0}, flushedAfterSkipSameSite{0}, staleEvicted{0};

struct StorageTraffic {
    std::mutex mutex;
    std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> writeBacks;
    std::array<std::pair<std::uint64_t, std::uint64_t>, 4> uploads{};
    std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> uploadReasons;
    std::uint64_t directWriteBacks = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

StorageTraffic& Traffic() {
    static StorageTraffic traffic;
    return traffic;
}

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
    const auto take = [](std::atomic<std::uint64_t>& counter) { return static_cast<unsigned long long>(counter.exchange(0, std::memory_order_relaxed)); };
    const auto partialUploadCount = take(partialUploads), partialWriteBackCount = take(partialWriteBacks);
    const auto uploadMiB = take(partialUploadBytes) / 1048576.0, writeBackMiB = take(partialWriteBackBytes) / 1048576.0;
    AgcDriver::ProfilePrint_nid_no_patch( "[storage] uploads by path (count/MiB, 10 s):%s; %llu write-backs GPU-direct, %llu stored nothing; block units: partial uploads %llu/%.1f, partial write-backs %llu/%.1f, %llu widened to every pending unit, units dropped %llu, superseded %llu, dropped unregistered %llu, kept over a key flip %llu%s, pretest skipped %llu, evicted stale %llu\n", line.c_str(), static_cast<unsigned long long>(traffic.directWriteBacks), take(emptyWriteBacks), partialUploadCount, uploadMiB, partialWriteBackCount, writeBackMiB, take(coalescedWriteBacks), take(unitsDropped), take(unitsSuperseded), take(unregisteredDropped), take(keyFlipKept), ShadowReport().c_str(), take(pretestSkipped), take(staleEvicted));
    traffic.writeBacks.clear();
    traffic.uploadReasons.clear();
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

void countStorageWriteBack(std::uint64_t bytes, bool direct) {
    if (!LookupOutcomes::Profiled()) return;
    auto& traffic = Traffic();
    std::lock_guard lock(traffic.mutex);
    auto& totals = traffic.writeBacks[flushReason != nullptr ? flushReason : "other"];
    ++totals.first;
    totals.second += bytes;
    if (direct) ++traffic.directWriteBacks;
    reportStorageTraffic(traffic);
}

// Every storage image alive, for the fill HLE's cover check (StorageTexture::ClassifyFill): the
// storage cache indexes surfaces by key, not by address range. Its mutex is a leaf.
struct LiveImages {
    std::mutex mutex;
    std::vector<StorageTexture*> textures;
};

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
    }
    throw std::runtime_error("AGC graphics: Texture encountered an unknown guest texture dimension");
}

}

namespace {

void ChainMinLod(const Context& context, const GuestTextureResource& descriptor, VkImageViewCreateInfo& viewInfo, VkImageViewMinLodCreateInfoEXT& minLod) {
    const auto clamp = EffectiveMinLod(descriptor);
    if (clamp == 0.0f) return;
    Require(context.imageViewMinLod, "guest texture descriptor uses a minimum LOD clamp, which needs VK_EXT_image_view_min_lod");
    minLod.minLod = clamp;
    minLod.pNext = viewInfo.pNext;
    viewInfo.pNext = &minLod;
}

std::atomic<std::uint64_t>& SampledMemoryCounter() {
    static std::atomic<std::uint64_t> bytes{0};
    return bytes;
}

}

std::uint64_t SampledTextureMemory() {
    return SampledMemoryCounter().load(std::memory_order_relaxed);
}

Texture::Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot, bool depthCompare) : context(context) {

    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    try {
        const auto colorFormat = SampledTextureFormat(context, descriptor.format);
        Require(!depthCompare || colorFormat == VK_FORMAT_R32_SFLOAT || colorFormat == VK_FORMAT_R16_UNORM, "comparison sampling requires an R32 float or R16 unorm depth texture");
        Require(!depthCompare || descriptor.dimension != TextureDimension::k3D, "comparison sampling does not support 3D depth textures");
        const auto vkFormat = depthCompare ? (colorFormat == VK_FORMAT_R32_SFLOAT ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_D16_UNORM) : colorFormat;
        const VkImageAspectFlags aspect = depthCompare ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        if (IsBlockCompressed(descriptor.format)) {
            Require(context.textureCompressionBC, "device does not support BC compressed textures");
        }

        const auto geometry = DescribeSurface(descriptor);
        const auto& mips = geometry.mips;
        const auto arrayLayers = geometry.layers;
        const auto elementBytes = BytesPerElement(descriptor.format);
        APS5_LOG_OUT("Texture address=0x%llx %ux%u mips=%u layers=%u dim=%d tile=%d format=%u vk=%d element=%u", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, arrayLayers,
                     static_cast<int>(descriptor.dimension), static_cast<int>(descriptor.tileMode), descriptor.format, static_cast<int>(vkFormat), elementBytes);
        for (const auto& mip : mips) APS5_LOG_OUT("  mip %ux%u tiled=0x%llx+0x%llx linear=0x%llx+0x%llx blocksPerRow=%u pitch=%u tail=%d", mip.width, mip.height, static_cast<unsigned long long>(mip.tiledOffset), static_cast<unsigned long long>(mip.tiledSize), static_cast<unsigned long long>(mip.linearOffset), static_cast<unsigned long long>(mip.linearSize), mip.blocksPerRow, mip.pitchBytes, mip.tail ? 1 : 0);

        const auto guestBytes = geometry.guestBytes;
        Require(snapshot.size() == guestBytes, "texture snapshot size mismatch");

        const auto sliceLinearBytes = geometry.sliceLinearBytes;
        Require(arrayLayers == 0 || sliceLinearBytes <= UINT64_MAX / arrayLayers, "detiled texture buffer size overflows");
        const auto linearBytes = sliceLinearBytes * arrayLayers;

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.flags = descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u;
        imageInfo.imageType = ImageTypeFor(descriptor.dimension);
        imageInfo.format = vkFormat;
        imageInfo.extent = {descriptor.width, descriptor.height, geometry.imageDepth};
        imageInfo.mipLevels = descriptor.mipCount;
        imageInfo.arrayLayers = geometry.imageLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage");
        owned = std::make_shared<OwnedImage>(context, image, VK_NULL_HANDLE);

        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocationBytes = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &owned->memory), "vkAllocateMemory texture");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, owned->memory, 0), "vkBindImageMemory");

        {
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
            auto staging = std::make_shared<Buffer>(context, static_cast<std::size_t>(guestBytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            std::memcpy(staging->Bytes().data(), snapshot.data(), snapshot.size());
            auto tiled = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(guestBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            if (profile) Profile().allocate += timer.lap();
            if (profile) Profile().read += timer.lap();

            detiler.BeginBatch();
            // The upload is recorded into the open batch, like a storage image's: the work that samples
            // the texture is recorded after it, and a batch of its own (SubmitAndWait) was a full GPU
            // drain under the device lock for every new texture. The buffers and the image live with
            // the batch (the cache may drop the texture before it completes). The dump aid needs the
            // linear bytes on the CPU, so it keeps the waiting batch, as does a build without a
            // recorder (tests). APS5_SYNC_TEXTURE_UPLOAD=1 restores the waiting batch for every upload.
            static const bool syncUploads = std::getenv("APS5_SYNC_TEXTURE_UPLOAD") != nullptr;
            auto* recorder = dumpWanted || syncUploads ? nullptr : Recorder::Active();
            std::unique_ptr<CommandBatch> batch;
            VkCommandBuffer commands = VK_NULL_HANDLE;
            if (recorder != nullptr) {
                commands = recorder->Commands();
                recorder->Keep(staging, static_cast<std::size_t>(guestBytes));
                recorder->Keep(tiled, static_cast<std::size_t>(guestBytes));
                recorder->Keep(linear, static_cast<std::size_t>(linearBytes));
                recorder->Keep(owned);
            } else {
                batch = std::make_unique<CommandBatch>(context);
                commands = batch->Handle();
            }

            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, staging->Handle(), 0, tiled->Handle(), 0, guestBytes);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                const auto guestLayerOffset = geometry.GuestLayerOffset(layer);
                const auto linearLayerOffset = geometry.LinearLayerOffset(layer);
                for (std::uint32_t level = 0; level < mips.size(); ++level) {
                    if (!geometry.HasLayer(level, layer)) continue;
                    const auto& mip = mips[level];
                    detiler.Dispatch(commands, descriptor.tileMode, elementBytes, tiled->Handle(), guestLayerOffset + mip.tiledOffset, linear->Handle(), linearLayerOffset + mip.linearOffset, mip, false, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
                }
            }

            VkBufferMemoryBarrier linearReadBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            linearReadBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            linearReadBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            linearReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearReadBarrier.buffer = linear->Handle();
            linearReadBarrier.offset = 0;
            linearReadBarrier.size = VK_WHOLE_SIZE;

            VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransferDst.srcAccessMask = 0;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = image;
            toTransferDst.subresourceRange = {aspect, 0, descriptor.mipCount, 0, geometry.imageLayers};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearReadBarrier, 1, &toTransferDst);

            std::vector<VkBufferImageCopy> regions;
            regions.reserve(static_cast<std::size_t>(arrayLayers) * mips.size());
            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                const auto linearLayerOffset = static_cast<std::uint64_t>(layer) * sliceLinearBytes;
                for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
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
            }
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
            std::unique_ptr<Buffer> dump;
            if (dumpWanted) {
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
                static std::mutex dumpMutex;
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
            Require(viewLayerCount % 6u == 0, "guest cube texture view does not contain a multiple of 6 array slices");
        }

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = ViewTypeFor(descriptor.dimension, viewLayerCount);
        viewInfo.format = vkFormat;
        viewFormat = vkFormat;
        viewInfo.components = depthCompare ? VkComponentMapping{} : components;
        viewInfo.subresourceRange = {aspect, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        ChainMinLod(context, descriptor, viewInfo, minLod);

        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView");
        viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
        createFirstLayerView(descriptor, viewInfo);
        if (profile) {
            auto& totals = Profile();
            totals.view += timer.lap();
            if (++totals.count % 200 == 0) std::fprintf(stderr, "[texture] %llu textures (%llu copied from storage images, %llu uploads recorded): allocate+image %.0f ms, guest read %.0f ms, detile+copy %.0f ms, view+buffer release %.0f ms\n", static_cast<unsigned long long>(totals.count), static_cast<unsigned long long>(totals.fromStorage), static_cast<unsigned long long>(totals.recordedUploads), totals.allocate, totals.read, totals.gpu, totals.view);
        }
        countedBytes = allocationBytes;
        SampledMemoryCounter().fetch_add(countedBytes, std::memory_order_relaxed);
    } catch (...) {
        release();
        throw;
    }
}

bool Texture::CanCopyFrom(const StorageTexture& source, const GuestTextureResource& descriptor) {
    const auto& from = source.Descriptor();
    if (IsBlockCompressed(descriptor.format) || IsBlockCompressed(from.format)) return false;
    // Same memory, same layout, same texel size: the GPU copy reinterprets the texels exactly as a
    // guest read through the sampled descriptor would.
    return descriptor.baseAddress == from.baseAddress && descriptor.pipeBankXor == from.pipeBankXor && descriptor.width == from.width && descriptor.height == from.height && descriptor.dimension == from.dimension && descriptor.tileMode == from.tileMode && descriptor.mipCount <= from.mipCount && descriptor.depthOrLastArray == from.depthOrLastArray && BytesPerElement(descriptor.format) == BytesPerElement(from.format) && BlockWidth(descriptor.format) == BlockWidth(from.format);
}

Texture::Texture(const Context& context, const std::shared_ptr<StorageTexture>& source, const GuestTextureResource& descriptor, VkComponentMapping components) : context(context), storageSource(source) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    try {
        Require(source != nullptr && CanCopyFrom(*source, descriptor), "storage image does not match the sampled texture");
        const auto vkFormat = SampledTextureFormat(context, descriptor.format);
        const auto geometry = DescribeSurface(descriptor);
        APS5_LOG_OUT("Texture address=0x%llx %ux%u mips=%u viewed from storage image (vk=%d)", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, static_cast<int>(vkFormat));
        // Storage images stay in the general layout; the view samples them there.
        layout = VK_IMAGE_LAYOUT_GENERAL;
        const auto viewLevelCount = std::min(descriptor.lastLevel, descriptor.mipCount - 1u) - descriptor.baseLevel + 1u;
        const auto viewLayerCount = std::min(geometry.imageLayers, source->ImageLayers()) - descriptor.baseArray;
        if (descriptor.dimension == TextureDimension::kCube) {
            Require(viewLayerCount % 6u == 0, "guest cube texture view does not contain a multiple of 6 array slices");
        }
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = source->Image();
        viewInfo.viewType = ViewTypeFor(descriptor.dimension, viewLayerCount);
        viewInfo.format = vkFormat;
        viewFormat = vkFormat;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        ChainMinLod(context, descriptor, viewInfo, minLod);
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView storage view");
        viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
        createFirstLayerView(descriptor, viewInfo);
        if (profile) {
            auto& totals = Profile();
            totals.view += timer.lap();
            ++totals.fromStorage;
            if (++totals.count % 200 == 0) std::fprintf(stderr, "[texture] %llu textures (%llu viewed from storage images): allocate+image %.0f ms, guest read %.0f ms, detile+copy %.0f ms, view+buffer release %.0f ms\n", static_cast<unsigned long long>(totals.count), static_cast<unsigned long long>(totals.fromStorage), totals.allocate, totals.read, totals.gpu, totals.view);
        }
    } catch (...) {
        release();
        throw;
    }
}

Texture::Texture(const Context& context, VkImage depthImage, VkFormat depthFormat, VkImageAspectFlags aspect, VkComponentMapping components) : context(context) {
    layout = VK_IMAGE_LAYOUT_GENERAL;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewFormat = depthFormat;
    viewInfo.components = components;
    viewInfo.subresourceRange = {aspect, 0, 1, 0, 1};
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth plane");
    viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
}

Texture::~Texture() {
    release();
}

void Texture::createFirstLayerView(const GuestTextureResource& descriptor, VkImageViewCreateInfo viewInfo) {
    if (descriptor.dimension != TextureDimension::k2DArray) return;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.layerCount = 1;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &firstLayerView), "vkCreateImageView first layer");
    firstLayerRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
}

void Texture::release() noexcept {
    upload.reset();
    if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    view = VK_NULL_HANDLE;
    if (firstLayerView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, firstLayerView, nullptr);
    firstLayerView = VK_NULL_HANDLE;
    // The image and its memory go with the last holder: this texture, or the batch still uploading it.
    owned.reset();
    image = VK_NULL_HANDLE;
    SampledMemoryCounter().fetch_sub(std::exchange(countedBytes, 0), std::memory_order_relaxed);
}

VkImageView Texture::View() const {
    return view;
}


namespace {

VkBufferMemoryBarrier WholeBufferBarrier(VkBuffer buffer, VkAccessFlags from, VkAccessFlags to) {
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask = from;
    barrier.dstAccessMask = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    return barrier;
}

// Storage images cannot use sRGB formats; the shader works on the raw encoded values either way.
// The answer is a property of the physical device, so it is computed once per format (every storage
// image lookup asks: a live vkGetPhysicalDeviceFormatProperties call each time was measurable) and
// remembered as VK_FORMAT_UNDEFINED when the format has no storage form.
VkFormat StorageFormatOrUndefined(const Context& context, VkFormat format) {
    struct Table {
        std::mutex mutex;
        std::unordered_map<std::uint64_t, VkFormat> formats;
    };
    static Table table;
    // The key names the physical device: the headless and the windowed device share one GPU, but a
    // second GPU would answer differently.
    const auto key = (static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(context.physical)) << 20u) ^ static_cast<std::uint64_t>(format);
    {
        std::lock_guard lock(table.mutex);
        if (const auto found = table.formats.find(key); found != table.formats.end()) return found->second;
    }
    const auto supports = [&](VkFormat candidate) {
        VkFormatProperties properties{};
        context.formatProperties(context.physical, candidate, &properties);
        return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
    };
    VkFormat storage = VK_FORMAT_UNDEFINED;
    if (supports(format)) {
        storage = format;
    } else {
        VkFormat linear = VK_FORMAT_UNDEFINED;
        switch (format) {
            case VK_FORMAT_R8G8B8A8_SRGB: linear = VK_FORMAT_R8G8B8A8_UNORM; break;
            case VK_FORMAT_B8G8R8A8_SRGB: linear = VK_FORMAT_B8G8R8A8_UNORM; break;
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32: linear = VK_FORMAT_A8B8G8R8_UNORM_PACK32; break;
            case VK_FORMAT_R8_SRGB: linear = VK_FORMAT_R8_UNORM; break;
            case VK_FORMAT_R8G8_SRGB: linear = VK_FORMAT_R8G8_UNORM; break;
            default: break;
        }
        if (linear != VK_FORMAT_UNDEFINED && supports(linear)) storage = linear;
    }
    std::lock_guard lock(table.mutex);
    table.formats.emplace(key, storage);
    return storage;
}

VkFormat UintFormatOfSint(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8G8_SINT: return VK_FORMAT_R8G8_UINT;
        case VK_FORMAT_R8G8B8A8_SINT: return VK_FORMAT_R8G8B8A8_UINT;
        case VK_FORMAT_R16_SINT: return VK_FORMAT_R16_UINT;
        case VK_FORMAT_R16G16_SINT: return VK_FORMAT_R16G16_UINT;
        case VK_FORMAT_R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_UINT;
        case VK_FORMAT_R32_SINT: return VK_FORMAT_R32_UINT;
        case VK_FORMAT_R32G32_SINT: return VK_FORMAT_R32G32_UINT;
        case VK_FORMAT_R32G32B32A32_SINT: return VK_FORMAT_R32G32B32A32_UINT;
        default: return VK_FORMAT_UNDEFINED;
    }
}

VkFormat StorageFormatFor(const Context& context, VkFormat format) {
    const auto storage = StorageFormatOrUndefined(context, format);
    if (storage == VK_FORMAT_UNDEFINED) Require(false, "guest storage texture format " + std::to_string(format) + " cannot be used as a storage image");
    return storage;
}

}

bool StorageFormatAvailable(const Context& context, std::uint32_t guestFormat) {
    try {
        return StorageFormatOrUndefined(context, ResolveTextureFormat(guestFormat)) != VK_FORMAT_UNDEFINED;
    } catch (const std::exception&) {
        // An unknown guest format: the sampled texture path reports it when it gets there.
        return false;
    }
}

// Integer formats take integer clear values; the DCC clear codes are only mapped for the others.
bool IntegerFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_UINT: case VK_FORMAT_R8_SINT: case VK_FORMAT_R8G8_UINT: case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8B8A8_UINT: case VK_FORMAT_R8G8B8A8_SINT: case VK_FORMAT_B8G8R8A8_UINT: case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32: case VK_FORMAT_A8B8G8R8_SINT_PACK32: case VK_FORMAT_A2R10G10B10_UINT_PACK32: case VK_FORMAT_A2B10G10R10_UINT_PACK32:
        case VK_FORMAT_R16_UINT: case VK_FORMAT_R16_SINT: case VK_FORMAT_R16G16_UINT: case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16B16A16_UINT: case VK_FORMAT_R16G16B16A16_SINT: case VK_FORMAT_R32_UINT: case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32G32_UINT: case VK_FORMAT_R32G32_SINT: case VK_FORMAT_R32G32B32_UINT: case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32A32_UINT: case VK_FORMAT_R32G32B32A32_SINT: case VK_FORMAT_R64_UINT: case VK_FORMAT_R64_SINT:
            return true;
        default: return false;
    }
}

// The clear value a DCC clear code stands for, for non-integer formats.
bool ClearColorFor(VkFormat format, DccKeys keys, VkClearColorValue& clear) {
    if (IntegerFormat(format)) return false;
    switch (keys) {
        case DccKeys::Clear0000: clear.float32[0] = clear.float32[1] = clear.float32[2] = clear.float32[3] = 0.0f; return true;
        case DccKeys::Clear0001: clear.float32[0] = clear.float32[1] = clear.float32[2] = 0.0f; clear.float32[3] = 1.0f; return true;
        case DccKeys::Clear1110: clear.float32[0] = clear.float32[1] = clear.float32[2] = 1.0f; clear.float32[3] = 0.0f; return true;
        case DccKeys::Clear1111: clear.float32[0] = clear.float32[1] = clear.float32[2] = clear.float32[3] = 1.0f; return true;
        default: return false;
    }
}

VkFormat StorageFormatForGuest(const Context& context, std::uint32_t guestFormat) {
    return StorageFormatFor(context, ResolveTextureFormat(guestFormat));
}

bool StorageClearAvailable(const Context& context, std::uint32_t guestFormat, DccKeys keys) {
    VkClearColorValue clear{};
    return IsDccClear(keys) && StorageFormatAvailable(context, guestFormat) && ClearColorFor(StorageFormatForGuest(context, guestFormat), keys, clear);
}

VkFormat AttachmentProxyFormat(const Context& context, VkFormat format) {
    if (format != VK_FORMAT_R8_SRGB) return VK_FORMAT_UNDEFINED;
    struct Table {
        std::mutex mutex;
        std::unordered_map<VkPhysicalDevice, VkFormat> formats;
    };
    static Table table;
    {
        std::lock_guard lock(table.mutex);
        if (const auto found = table.formats.find(context.physical); found != table.formats.end()) return found->second;
    }
    const char* forced = std::getenv("APS5_SRGB_ATTACHMENT_PROXY");
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    constexpr VkFormatFeatureFlags attachment = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    const bool proxied = (forced != nullptr && std::strcmp(forced, "1") == 0) || (properties.optimalTilingFeatures & attachment) != attachment;
    const auto proxy = proxied ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_UNDEFINED;
    std::lock_guard lock(table.mutex);
    table.formats.emplace(context.physical, proxy);
    return proxy;
}

StorageTexture::StorageTexture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, std::uint32_t mipLevel) : context(context), detiler(detiler), descriptor(descriptor) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    try {
        Require(!IsBlockCompressed(descriptor.format), "block-compressed textures cannot be storage images");
        Require(mipLevel < descriptor.mipCount, "storage texture mip level is outside the texture");
        const auto vkFormat = StorageFormatFor(context, ResolveTextureFormat(descriptor.format));
        storageFormat = vkFormat;
        APS5_LOG_OUT("StorageTexture address=0x%llx %ux%u mips=%u mip=%u layers=%u base=%u dim=%d tile=%d format=%u vk=%d", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, mipLevel,
                     descriptor.depthOrLastArray, descriptor.baseArray, static_cast<int>(descriptor.dimension), static_cast<int>(descriptor.tileMode), descriptor.format, static_cast<int>(vkFormat));
        geometry = DescribeSurface(descriptor);
        mips = geometry.mips;
        arrayLayers = geometry.layers;
        const auto elementBytes = BytesPerElement(descriptor.format);
        guestBytes = geometry.guestBytes;
        // Storage images in heaps the guest commits on demand only read and store committed pages.
        Require(!GuestMemory::CommittedRanges(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), true).empty(), "storage texture has no committed guest pages");
        // Write stamps are per 64 KiB block, so only 64 KiB-aligned layer slices can be told apart.
        const bool layered = geometry.layers >= 2 && !geometry.thick && geometry.imageDepth == 1 && geometry.layers == geometry.imageLayers && geometry.layerBytes * geometry.layers == guestBytes && geometry.layerBytes % 65536 == 0 && descriptor.baseAddress % 65536 == 0;
        // Tile blocks divide the stamp blocks, so at an aligned base every unit is whole tile blocks
        // of its (layer, mip) slices (sliceWindows).
        blockUnits = LayerTrackingEnabled() && BlockTrackingEnabled() && !geometry.thick && descriptor.tileMode != TextureTileMode::kLinear && descriptor.baseAddress % 65536 == 0;
        trackedLayers = blockUnits ? static_cast<std::uint32_t>((guestBytes + 65535) / 65536) : layered && LayerTrackingEnabled() ? geometry.layers : 1u;
        trackedLayerBytes = blockUnits ? 65536 : guestBytes / trackedLayers;
        layerGeneration.assign(trackedLayers, 0);
        layerPending.assign(trackedLayers, false);
        sliceLinearBytes = geometry.sliceLinearBytes;
        const auto linearBytes = sliceLinearBytes * arrayLayers;

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        // Sampled views of other same-size formats (sRGB, reinterpretations) read the image directly.
        imageInfo.flags = (descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u) | (descriptor.dimension == TextureDimension::k3D ? VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT : 0u) | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
        imageInfo.imageType = ImageTypeFor(descriptor.dimension);
        imageInfo.format = vkFormat;
        imageInfo.extent = {descriptor.width, descriptor.height, geometry.imageDepth};
        imageInfo.mipLevels = descriptor.mipCount;
        imageInfo.arrayLayers = geometry.imageLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        {
            VkFormatProperties properties{};
            context.formatProperties(context.physical, vkFormat, &properties);
            attachable = (descriptor.dimension == TextureDimension::k2D || descriptor.dimension == TextureDimension::k3D) && (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
            if (attachable) imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage storage");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        memoryBytes = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory storage texture");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory storage");
        uploadReason = "first";
        upload();
        defaultMip = mipLevel;
        view = createView(mipLevel, false, storageFormat);
        {
            auto& live = Live();
            std::lock_guard lock(live.mutex);
            live.textures.push_back(this);
        }
        if (profile) {
            auto& totals = Profile();
            totals.storageCreate += timer.lap();
            totals.storageBytes += guestBytes;
            if (++totals.storageCount % 50 == 0) std::fprintf(stderr, "[texture] %llu storage images (%.0f MiB): create %.0f ms, write-back %.0f ms (alloc %.0f, host copy %.0f, gpu %.0f, store %.0f), %llu reused, %llu direct uploads, %llu direct write-backs, clears %llu recorded / %llu waited\n", static_cast<unsigned long long>(totals.storageCount), totals.storageBytes / 1048576.0, totals.storageCreate, totals.storageWriteBack, totals.storageAlloc, totals.storageHostCopy, totals.storageGpu, totals.storageStore, static_cast<unsigned long long>(totals.storageReused), static_cast<unsigned long long>(totals.storageDirectUploads), static_cast<unsigned long long>(totals.storageDirectWriteBacks), static_cast<unsigned long long>(totals.storageRecordedClears), static_cast<unsigned long long>(totals.storageWaitedClears));
        }
    } catch (...) {
        release();
        throw;
    }
}

namespace {

class PendingList {
public:
    using iterator = std::vector<StorageTexture*>::iterator;
    iterator begin() { return textures.begin(); }
    iterator end() { return textures.end(); }
    void push_back(StorageTexture* texture) {
        textures.push_back(texture);
        ++version;
    }
    iterator erase(iterator it) {
        ++version;
        return textures.erase(it);
    }
    void remove(const StorageTexture* texture) {
        if (std::erase(textures, texture) != 0) ++version;
    }
    bool MayOverlap(std::uint64_t address, std::size_t bytes) {
        if (textures.empty() || bytes == 0) return false;
        if (indexed != version) rebuild();
        const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
        const auto it = std::upper_bound(ranges.begin(), ranges.end(), address, [](std::uint64_t value, const std::pair<std::uint64_t, std::uint64_t>& range) { return value < range.second; });
        return it != ranges.end() && it->first < end;
    }

private:
    void rebuild() {
        ranges.clear();
        for (const auto* texture : textures) ranges.emplace_back(texture->Descriptor().baseAddress, texture->Descriptor().baseAddress + texture->GuestBytes());
        std::sort(ranges.begin(), ranges.end());
        std::size_t merged = 0;
        for (const auto& range : ranges) {
            if (merged != 0 && range.first <= ranges[merged - 1].second) ranges[merged - 1].second = std::max(ranges[merged - 1].second, range.second);
            else ranges[merged++] = range;
        }
        ranges.resize(merged);
        indexed = version;
    }
    std::vector<StorageTexture*> textures;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::uint64_t version = 0;
    std::uint64_t indexed = ~std::uint64_t{0};
};

// Storage images whose results have not reached guest memory yet.
struct PendingWrites {
    std::mutex mutex;
    PendingList textures;
    // Images taken out of `textures` by a FlushPending still storing them (see adjacentPendingUnchanged).
    std::vector<StorageTexture*> flushing;
};

PendingWrites& Pending() {
    static PendingWrites pending;
    return pending;
}

// See StorageTexture::PendingSerial: bumped after every mutation of the registry above.
std::atomic<std::uint64_t> pendingSerial{0};

// The texel layout a clear value is built from (FillClearColor): channels in memory order with
// their bit offset and width inside the element and the clear component they feed (R, G, B, A).
enum class ClearKind { Unorm, Snorm, Uint, Sint, Float, UFloat };

struct ClearChannel {
    std::uint32_t offset;
    std::uint32_t bits;
    std::uint32_t component;
};

struct ClearLayout {
    ClearKind kind;
    std::uint32_t channels;
    ClearChannel channel[4];
};

bool ClearLayoutFor(VkFormat format, ClearLayout& layout) {
    const auto uniform = [&](ClearKind kind, std::uint32_t count, std::uint32_t width, bool reversed = false) {
        layout.kind = kind;
        layout.channels = count;
        for (std::uint32_t i = 0; i < count; ++i) layout.channel[i] = {i * width, width, reversed && i < 3 ? 2u - i : i};
    };
    switch (format) {
        case VK_FORMAT_R8_UNORM: uniform(ClearKind::Unorm, 1, 8); return true;
        case VK_FORMAT_R8_SNORM: uniform(ClearKind::Snorm, 1, 8); return true;
        case VK_FORMAT_R8_UINT: uniform(ClearKind::Uint, 1, 8); return true;
        case VK_FORMAT_R8_SINT: uniform(ClearKind::Sint, 1, 8); return true;
        case VK_FORMAT_R8G8_UNORM: uniform(ClearKind::Unorm, 2, 8); return true;
        case VK_FORMAT_R8G8_SNORM: uniform(ClearKind::Snorm, 2, 8); return true;
        case VK_FORMAT_R8G8_UINT: uniform(ClearKind::Uint, 2, 8); return true;
        case VK_FORMAT_R8G8_SINT: uniform(ClearKind::Sint, 2, 8); return true;
        case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_A8B8G8R8_UNORM_PACK32: uniform(ClearKind::Unorm, 4, 8); return true;
        case VK_FORMAT_R8G8B8A8_SNORM: case VK_FORMAT_A8B8G8R8_SNORM_PACK32: uniform(ClearKind::Snorm, 4, 8); return true;
        case VK_FORMAT_R8G8B8A8_UINT: case VK_FORMAT_A8B8G8R8_UINT_PACK32: uniform(ClearKind::Uint, 4, 8); return true;
        case VK_FORMAT_R8G8B8A8_SINT: case VK_FORMAT_A8B8G8R8_SINT_PACK32: uniform(ClearKind::Sint, 4, 8); return true;
        case VK_FORMAT_B8G8R8A8_UNORM: uniform(ClearKind::Unorm, 4, 8, true); return true;
        case VK_FORMAT_B8G8R8A8_SNORM: uniform(ClearKind::Snorm, 4, 8, true); return true;
        case VK_FORMAT_B8G8R8A8_UINT: uniform(ClearKind::Uint, 4, 8, true); return true;
        case VK_FORMAT_B8G8R8A8_SINT: uniform(ClearKind::Sint, 4, 8, true); return true;
        case VK_FORMAT_R16_UNORM: uniform(ClearKind::Unorm, 1, 16); return true;
        case VK_FORMAT_R16_SNORM: uniform(ClearKind::Snorm, 1, 16); return true;
        case VK_FORMAT_R16_UINT: uniform(ClearKind::Uint, 1, 16); return true;
        case VK_FORMAT_R16_SINT: uniform(ClearKind::Sint, 1, 16); return true;
        case VK_FORMAT_R16_SFLOAT: uniform(ClearKind::Float, 1, 16); return true;
        case VK_FORMAT_R16G16_UNORM: uniform(ClearKind::Unorm, 2, 16); return true;
        case VK_FORMAT_R16G16_SNORM: uniform(ClearKind::Snorm, 2, 16); return true;
        case VK_FORMAT_R16G16_UINT: uniform(ClearKind::Uint, 2, 16); return true;
        case VK_FORMAT_R16G16_SINT: uniform(ClearKind::Sint, 2, 16); return true;
        case VK_FORMAT_R16G16_SFLOAT: uniform(ClearKind::Float, 2, 16); return true;
        case VK_FORMAT_R16G16B16A16_UNORM: uniform(ClearKind::Unorm, 4, 16); return true;
        case VK_FORMAT_R16G16B16A16_SNORM: uniform(ClearKind::Snorm, 4, 16); return true;
        case VK_FORMAT_R16G16B16A16_UINT: uniform(ClearKind::Uint, 4, 16); return true;
        case VK_FORMAT_R16G16B16A16_SINT: uniform(ClearKind::Sint, 4, 16); return true;
        case VK_FORMAT_R16G16B16A16_SFLOAT: uniform(ClearKind::Float, 4, 16); return true;
        case VK_FORMAT_R32_UINT: uniform(ClearKind::Uint, 1, 32); return true;
        case VK_FORMAT_R32_SINT: uniform(ClearKind::Sint, 1, 32); return true;
        case VK_FORMAT_R32_SFLOAT: uniform(ClearKind::Float, 1, 32); return true;
        case VK_FORMAT_R32G32_UINT: uniform(ClearKind::Uint, 2, 32); return true;
        case VK_FORMAT_R32G32_SINT: uniform(ClearKind::Sint, 2, 32); return true;
        case VK_FORMAT_R32G32_SFLOAT: uniform(ClearKind::Float, 2, 32); return true;
        case VK_FORMAT_R32G32B32A32_UINT: uniform(ClearKind::Uint, 4, 32); return true;
        case VK_FORMAT_R32G32B32A32_SINT: uniform(ClearKind::Sint, 4, 32); return true;
        case VK_FORMAT_R32G32B32A32_SFLOAT: uniform(ClearKind::Float, 4, 32); return true;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: layout = {ClearKind::Unorm, 4, {{0, 10, 0}, {10, 10, 1}, {20, 10, 2}, {30, 2, 3}}}; return true;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: layout = {ClearKind::Uint, 4, {{0, 10, 0}, {10, 10, 1}, {20, 10, 2}, {30, 2, 3}}}; return true;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: layout = {ClearKind::Unorm, 4, {{0, 10, 2}, {10, 10, 1}, {20, 10, 0}, {30, 2, 3}}}; return true;
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: layout = {ClearKind::Uint, 4, {{0, 10, 2}, {10, 10, 1}, {20, 10, 0}, {30, 2, 3}}}; return true;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: layout = {ClearKind::UFloat, 3, {{0, 11, 0}, {11, 11, 1}, {22, 10, 2}}}; return true;
        default: return false;
    }
}

// A float of `bits` (16: IEEE half; 11 and 10: the unsigned 5-bit-exponent floats of
// B10G11R11) as a float32, or nullopt for a NaN, a denormal or (unsigned) an infinity.
std::optional<float> SmallFloat(std::uint32_t code, std::uint32_t bits, bool denormals = false) {
    const std::uint32_t mantissaBits = bits == 16 ? 10u : bits - 5u;
    const bool negative = bits == 16 && (code >> 15u) != 0;
    const std::uint32_t exponent = (code >> mantissaBits) & 0x1fu;
    const std::uint32_t mantissa = code & ((1u << mantissaBits) - 1u);
    if (exponent == 31u) {
        if (mantissa != 0 || bits != 16) return std::nullopt;
        return negative ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
    }
    if (exponent == 0) {
        if (mantissa != 0 && !denormals) return std::nullopt;
        const float value = std::ldexp(static_cast<float>(mantissa) / static_cast<float>(1u << mantissaBits), -14);
        return negative ? -value : value;
    }
    const float value = std::ldexp(1.0f + static_cast<float>(mantissa) / static_cast<float>(1u << mantissaBits), static_cast<int>(exponent) - 15);
    return negative ? -value : value;
}

// One texel of `format` (`elementBytes` wide) taken from a 16-byte fill pattern as the clear value
// that stores exactly those bits, when the pattern is that texel repeated: integer formats take the
// codes as they are, normalized ones the code over the channel's maximum (the conversion back rounds
// to the same code), float ones the decoded value. Codes a clear might store differently are
// refused: NaNs and denormals (an implementation may canonicalize or flush them) and the most
// negative SNORM code (stored as its neighbour, the same -1.0).
bool FillClearColor(VkFormat format, std::uint32_t elementBytes, std::span<const std::uint32_t, 4> pattern, VkClearColorValue& clear, bool denormals = false) {
    ClearLayout layout{};
    if (elementBytes == 0 || elementBytes > 16 || 16 % elementBytes != 0 || !ClearLayoutFor(format, layout)) return false;
    std::array<std::byte, 16> bytes{};
    std::memcpy(bytes.data(), pattern.data(), bytes.size());
    for (std::size_t at = elementBytes; at < bytes.size(); at += elementBytes) {
        if (std::memcmp(bytes.data(), bytes.data() + at, elementBytes) != 0) return false;
    }
    for (std::uint32_t i = 0; i < layout.channels; ++i) {
        const auto& channel = layout.channel[i];
        const auto word = pattern[channel.offset / 32u];
        const auto code = channel.bits == 32 ? word : (word >> (channel.offset % 32u)) & ((1u << channel.bits) - 1u);
        const auto signedCode = static_cast<std::int32_t>(code << (32u - channel.bits)) >> (32u - channel.bits);
        switch (layout.kind) {
            case ClearKind::Unorm:
                clear.float32[channel.component] = static_cast<float>(code) / static_cast<float>((1u << channel.bits) - 1u);
                break;
            case ClearKind::Snorm: {
                const auto maximum = static_cast<std::int32_t>((1u << (channel.bits - 1u)) - 1u);
                if (signedCode < -maximum) return false;
                clear.float32[channel.component] = static_cast<float>(signedCode) / static_cast<float>(maximum);
                break;
            }
            case ClearKind::Uint:
                clear.uint32[channel.component] = code;
                break;
            case ClearKind::Sint:
                clear.int32[channel.component] = signedCode;
                break;
            case ClearKind::Float:
                if (channel.bits == 32) {
                    const auto exponent = (code >> 23u) & 0xffu;
                    const auto mantissa = code & 0x7fffffu;
                    if ((exponent == 0xffu && mantissa != 0) || (exponent == 0 && mantissa != 0)) return false;
                    clear.float32[channel.component] = std::bit_cast<float>(code);
                } else {
                    const auto value = SmallFloat(code, channel.bits, denormals);
                    if (!value) return false;
                    clear.float32[channel.component] = *value;
                }
                break;
            case ClearKind::UFloat: {
                const auto value = SmallFloat(code, channel.bits, denormals);
                if (!value) return false;
                clear.float32[channel.component] = *value;
                break;
            }
        }
    }
    return true;
}

// The image being validated by Refresh: its own pending results are what the next dispatch wants,
// so the comparison with guest memory must not flush them.
thread_local const StorageTexture* refreshing = nullptr;

void FlushHook(std::uint64_t address, std::size_t bytes) {
    StorageTexture::FlushPending(address, bytes);
}

// Write stamps are per 64 KiB block (GuestMemory::UnchangedSince), so a write-back's own MarkWritten
// stamps the blocks its surface shares with an adjacent surface (video planes packed back to back:
// the luma plane's last block is the chroma plane's first). A pending image of that adjacent surface
// would take the stamp for a CPU write since its generation and, at its own write-back, keep the
// guest bytes of the shared block in place of its results (a stale band at the plane's start, zero
// chroma on a fresh buffer). So a write-back first finds the adjacent pending images unchanged since
// their generation and, once its stamps are made, advances them past the stamps: nothing of theirs
// changed. APS5_NO_ADJACENT_GENERATION=1 leaves them at their generation, as before.
bool AdjacentGenerationEnabled() {
    static const bool disabled = std::getenv("APS5_NO_ADJACENT_GENERATION") != nullptr;
    return !disabled;
}

}

bool ClearKeepsDenormals(const Context& context, VkFormat format) {
    static std::mutex mutex;
    static std::map<std::pair<VkDevice, VkFormat>, bool> known;
    std::lock_guard lock(mutex);
    if (const auto found = known.find({context.device, format}); found != known.end()) return found->second;
    ClearLayout layout{};
    bool smallFloats = ClearLayoutFor(format, layout) && (layout.kind == ClearKind::Float || layout.kind == ClearKind::UFloat);
    std::array<std::uint32_t, 4> pattern{};
    VkClearColorValue clear{};
    std::uint32_t elementBits = 0;
    constexpr std::array<std::uint32_t, 4> halfCodes{0x0001u, 0x83ffu, 0x0200u, 0x8001u};
    for (std::uint32_t i = 0; smallFloats && i < layout.channels; ++i) {
        const auto& channel = layout.channel[i];
        if (channel.bits == 32) {
            smallFloats = false;
            break;
        }
        const auto code = channel.bits == 16 ? halfCodes[i] : 1u;
        pattern[channel.offset / 32u] |= code << (channel.offset % 32u);
        clear.float32[channel.component] = *SmallFloat(code, channel.bits, true);
        elementBits = std::max(elementBits, channel.offset + channel.bits);
    }
    bool keeps = false;
    if (smallFloats) {
        const auto elementBytes = static_cast<std::size_t>((elementBits + 7u) / 8u);
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        struct Release {
            const Context& context;
            VkImage& image;
            VkDeviceMemory& memory;
            ~Release() {
                if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
                if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
            }
        } release{context, image, memory};
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {1, 1, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage denormal clear probe");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory denormal clear probe");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory denormal clear probe");
        Buffer readback(context, elementBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageMemoryBarrier toClear{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toClear.image = image;
        toClear.subresourceRange = range;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toClear);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        VkImageMemoryBarrier toCopy = toClear;
        toCopy.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toCopy.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toCopy);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {1, 1, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.Handle(), 1, &region);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        batch.SubmitAndWait();
        keeps = std::memcmp(readback.Bytes().data(), pattern.data(), elementBytes) == 0;
    }
    known.emplace(std::pair{context.device, format}, keeps);
    return keeps;
}

VkImageView StorageTexture::createView(std::uint32_t mip, bool firstLayer, VkFormat format) const {
    Require(mip < descriptor.mipCount, "storage texture mip level is outside the texture");
    Require(!firstLayer || descriptor.dimension == TextureDimension::k2DArray, "a first-layer storage view needs a 2D array surface");
    const auto viewLayerCount = firstLayer ? 1u : geometry.imageLayers - descriptor.baseArray;
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usage.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.pNext = format == storageFormat ? nullptr : &usage;
    viewInfo.image = image;
    // Storage views address one mip; cube faces are written as array layers.
    viewInfo.viewType = firstLayer ? VK_IMAGE_VIEW_TYPE_2D : descriptor.dimension == TextureDimension::k1D ? VK_IMAGE_VIEW_TYPE_1D : descriptor.dimension == TextureDimension::k2D ? VK_IMAGE_VIEW_TYPE_2D : descriptor.dimension == TextureDimension::k3D ? VK_IMAGE_VIEW_TYPE_3D : descriptor.dimension == TextureDimension::k1DArray ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1u, descriptor.baseArray, viewLayerCount};
    VkImageView created = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created), "vkCreateImageView storage");
    return created;
}

VkImageView StorageTexture::AttachmentView(VkFormat format, std::uint32_t mip, std::uint32_t depthSlice) {
    Require(attachable, "storage image cannot be a color attachment");
    Require(mip < descriptor.mipCount, "attachment mip exceeds the storage image");
    const bool volume = descriptor.dimension == TextureDimension::k3D;
    Require(depthSlice == 0 || (volume && mip == 0 && depthSlice <= descriptor.depthOrLastArray), "attachment slice is outside the storage image");
    const auto found = attachmentViews.find({format, mip, depthSlice});
    if (found != attachmentViews.end()) return found->second;
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usage.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.pNext = &usage;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1u, volume ? depthSlice : descriptor.baseArray, 1u};
    VkImageView created = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created), "vkCreateImageView attachment");
    attachmentViews.emplace(std::tuple{format, mip, depthSlice}, created);
    return created;
}

VkImageView StorageTexture::AttachmentProxyView() {
    if (proxyView != VK_NULL_HANDLE) return proxyView;
    Require(storageFormat == VK_FORMAT_R8_UNORM && descriptor.dimension == TextureDimension::k2D && descriptor.mipCount == 1 && geometry.imageLayers == 1, "an RGBA8_SRGB attachment proxy is only modeled for a single-mip 2D R8 surface");
    try {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {descriptor.width, descriptor.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &proxyImage), "vkCreateImage attachment proxy");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, proxyImage, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &proxyMemory), "vkAllocateMemory attachment proxy");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, proxyImage, proxyMemory, 0), "vkBindImageMemory attachment proxy");
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = proxyImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &proxyView), "vkCreateImageView attachment proxy");
    } catch (...) {
        if (proxyImage) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, proxyImage, nullptr);
        if (proxyMemory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, proxyMemory, nullptr);
        proxyImage = VK_NULL_HANDLE;
        proxyMemory = VK_NULL_HANDLE;
        proxyView = VK_NULL_HANDLE;
        throw;
    }
    return proxyView;
}

namespace {

void blitWhole(const Context& context, VkCommandBuffer commands, VkImage source, VkImage destination, std::uint32_t width, std::uint32_t height) {
    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[1] = {static_cast<std::int32_t>(width), static_cast<std::int32_t>(height), 1};
    region.dstSubresource = region.srcSubresource;
    region.dstOffsets[1] = region.srcOffsets[1];
    context.Function<PFN_vkCmdBlitImage>("vkCmdBlitImage")(commands, source, VK_IMAGE_LAYOUT_GENERAL, destination, VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_NEAREST);
}

VkImageMemoryBarrier proxyBarrier(VkImage image, VkAccessFlags source, VkAccessFlags destination, VkImageLayout from, VkImageLayout to) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = source;
    barrier.dstAccessMask = destination;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    return barrier;
}

}

void StorageTexture::RecordAttachmentProxyLoad(VkCommandBuffer commands, VkImageLayout attachmentLayout) const {
    Require(proxyView != VK_NULL_HANDLE, "the storage image has no attachment proxy");
    const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
    const VkMemoryBarrier written{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT};
    const auto discard = proxyBarrier(proxyImage, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &written, 0, nullptr, 1, &discard);
    blitWhole(context, commands, image, proxyImage, descriptor.width, descriptor.height);
    const auto attach = proxyBarrier(proxyImage, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, attachmentLayout);
    barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &attach);
}

void StorageTexture::RecordAttachmentProxyStore(VkCommandBuffer commands, VkImageLayout attachmentLayout) const {
    Require(proxyView != VK_NULL_HANDLE, "the storage image has no attachment proxy");
    const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
    const auto detach = proxyBarrier(proxyImage, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, attachmentLayout, VK_IMAGE_LAYOUT_GENERAL);
    barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &detach);
    blitWhole(context, commands, proxyImage, image, descriptor.width, descriptor.height);
    const VkMemoryBarrier stored{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
    barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &stored, 0, nullptr, 0, nullptr);
}

VkImageView StorageTexture::View(std::uint32_t mip) {
    if (mip == defaultMip) return view;
    const auto found = extraViews.find(mip);
    if (found != extraViews.end()) return found->second;
    const auto created = createView(mip, false, storageFormat);
    extraViews.emplace(mip, created);
    return created;
}

VkImageView StorageTexture::FirstLayerView(std::uint32_t mip) {
    const auto found = firstLayerViews.find(mip);
    if (found != firstLayerViews.end()) return found->second;
    const auto created = createView(mip, true, storageFormat);
    firstLayerViews.emplace(mip, created);
    return created;
}

VkImageView StorageTexture::StorageView(std::uint32_t mip, bool firstLayer) {
    const auto format = UintFormatOfSint(storageFormat);
    if (format == VK_FORMAT_UNDEFINED) return firstLayer ? FirstLayerView(mip) : View(mip);
    if (format == VK_FORMAT_R32_UINT) return AtomicView(mip, firstLayer);
    const auto found = uintViews.find({mip, firstLayer});
    if (found != uintViews.end()) return found->second;
    Require(StorageFormatOrUndefined(context, format) == format, "storage image of format " + std::to_string(storageFormat) + " has no storage view of its UINT format " + std::to_string(format));
    const auto created = createView(mip, firstLayer, format);
    uintViews.emplace(std::pair{mip, firstLayer}, created);
    return created;
}

VkImageView StorageTexture::ElementView() {
    if (elementView != VK_NULL_HANDLE) return elementView;
    if (descriptor.dimension != TextureDimension::k2D || geometry.imageLayers != 1) return VK_NULL_HANDLE;
    const auto bytes = BytesPerElement(descriptor.format);
    const auto format = bytes == 1 ? VK_FORMAT_R8_UINT : bytes == 2 ? VK_FORMAT_R16_UINT : bytes == 4 ? VK_FORMAT_R32_UINT : bytes == 8 ? VK_FORMAT_R32G32_UINT : VK_FORMAT_UNDEFINED;
    if (format == VK_FORMAT_UNDEFINED || StorageFormatOrUndefined(context, format) != format) return VK_NULL_HANDLE;
    elementView = createView(0, false, format);
    return elementView;
}

VkImageView StorageTexture::AtomicView(std::uint32_t mip, bool firstLayer) {
    if (storageFormat == VK_FORMAT_R32_UINT) return firstLayer ? FirstLayerView(mip) : View(mip);
    Require(storageFormat == VK_FORMAT_R32_SINT || storageFormat == VK_FORMAT_R32_SFLOAT, "storage image atomics need a surface of one 32-bit component");
    const auto found = atomicViews.find({mip, firstLayer});
    if (found != atomicViews.end()) return found->second;
    const auto created = createView(mip, firstLayer, VK_FORMAT_R32_UINT);
    atomicViews.emplace(std::pair{mip, firstLayer}, created);
    return created;
}

VkImageView StorageTexture::Atomic64View(std::uint32_t mip, bool firstLayer) {
    if (storageFormat == VK_FORMAT_R64_UINT) return firstLayer ? FirstLayerView(mip) : View(mip);
    Require(storageFormat == VK_FORMAT_R32G32_UINT || storageFormat == VK_FORMAT_R32G32_SINT || storageFormat == VK_FORMAT_R32G32_SFLOAT, "64-bit storage image atomics need a surface of two 32-bit components");
    const auto found = atomicViews.find({mip, firstLayer});
    if (found != atomicViews.end()) return found->second;
    const auto created = createView(mip, firstLayer, VK_FORMAT_R64_UINT);
    atomicViews.emplace(std::pair{mip, firstLayer}, created);
    return created;
}

const char* LookupOutcomes::Name(Kind kind) {
    static constexpr const char* names[Count] = {"sampled fast hit", "sampled fast miss", "sampled hit view", "sampled hit cleared view", "sampled hit snapshot", "sampled made view", "sampled made snapshot", "storage hit", "storage made", "refresh unchanged", "refresh compared", "upload direct", "upload cpu", "upload clear", "dcc scan", "pending flush"};
    return kind < Count ? names[kind] : "?";
}

bool LookupOutcomes::Profiled() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

std::chrono::steady_clock::time_point LookupOutcomes::Add(Kind kind, std::chrono::steady_clock::time_point start) {
    const auto now = std::chrono::steady_clock::now();
    auto& outcomes = ThreadLookupOutcomes();
    ++outcomes.counts[kind];
    outcomes.ms[kind] += std::chrono::duration<double, std::milli>(now - start).count();
    return now;
}

LookupOutcomes& ThreadLookupOutcomes() {
    thread_local LookupOutcomes outcomes;
    return outcomes;
}

bool StorageTexture::Refresh() {
    const bool profile = LookupOutcomes::Profiled();
    auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    struct Exempt {
        const StorageTexture* previous;
        ~Exempt() { refreshing = previous; }
    } exempt{refreshing};
    // This image's own pending results stay on the GPU, where the next dispatch wants them (the
    // flush below and the compare through the hook skip it).
    refreshing = this;
    NoteProved();
    // Results of other images pending in this memory must reach it first, except an alias's: its
    // units are taken on the device below (borrowUnits), so it stays pending. The keys read for
    // that decision are read again after the flush, which may store keys itself. The exemption
    // holds only when every pending unit of the alias is borrowable (tracked, unstamped since its
    // generation, no results of this image's own there) or dead to its own write-back (tracked and
    // stamped); an untracked unit is stored whole by that write-back, and a unit pending in both
    // images takes the old order (store, then this image re-uploads it).
    auto alias = pendingAlias();
    if (alias != nullptr && !(ProvedClearKeys(descriptor, guestBytes, keyProof) == DccKeys::Uncompressed && HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) != nullptr)) alias = nullptr;
    if (alias != nullptr) {
        std::vector<std::uint8_t> aliasStamped(trackedLayers);
        if (!GuestMemory::ChangedBlocks(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), alias->layerGeneration, aliasStamped)) alias = nullptr;
        for (std::uint32_t unit = 0; alias != nullptr && unit < trackedLayers; ++unit) {
            if (!alias->layerPending[unit]) continue;
            if (alias->layerGeneration[unit] == 0 || aliasStamped[unit] == GuestMemory::BlockMaybeWritten || (aliasStamped[unit] == GuestMemory::BlockUnchanged && layerPending[unit])) {
                alias = nullptr;
                break;
            }
        }
    }
    // No publish: the upload below reads the stored units from the unit shadow itself.
    const bool flushed = FlushPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), alias.get(), "storage refresh", PublishScope::None);
    if (profile && flushed) start = LookupOutcomes::Add(LookupOutcomes::PendingFlush, start);
    // `original` holds the guest bytes the image was last uploaded from or written back as; while the
    // guest memory and the DCC keys still match, the image content is current. Pages nobody wrote
    // since `generation` need no comparison.
    const auto current = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    // Per tracked layer: changed since its generation (a keys change makes every layer stale).
    std::vector<bool> changed(trackedLayers, false);
    // Per 64 KiB block of the surface (blockGenerations): stamped since its layer's generation, and
    // by a CPU store; scanned once when anything changed.
    std::vector<std::uint64_t> generations;
    std::vector<std::uint8_t> stampedBlocks, cpuBlocks;
    // Units taken from the alias's image instead of guest memory.
    std::vector<bool> borrowed;
    bool anyBorrowed = false;
    bool unchanged = true;
    bool stamped = true;
    bool tracked = true;
    bool compared = false;
    bool keysChanged = false;
    bool cpuWrote = false;
    bool direct = false;
    DccKeys keys = uploadedKeys;
    {
        const auto equalsOriginal = [&] {
            // Named for the [hooksync] attribution: the compare goes through the flush hook.
            const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureCompare);
            return GuestMemory::EqualsCommitted(descriptor.baseAddress, original);
        };
        const auto scanBlocks = [&] {
            blockGenerations(generations);
            stampedBlocks.assign(generations.size(), 0);
            cpuBlocks.assign(generations.size(), 0);
            tracked = GuestMemory::ChangedBlocks(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), generations, stampedBlocks, cpuBlocks);
            compared = !tracked && compareUntracked(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), stampedBlocks, true);
            if (compared) cpuBlocks = stampedBlocks;
        };
        const auto keysStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        keys = ProvedKeys();
        if (profile && descriptor.dccAddress != 0) LookupOutcomes::Add(LookupOutcomes::DccScan, keysStart);
        if (keys != uploadedKeys) {
            changed.assign(trackedLayers, true);
            unchanged = false;
            keysChanged = true;
            // Which units the CPU stamped still decides what the drop rule below may drop.
            if (blockUnits) scanBlocks();
        } else if (blockUnits) {
            if (!GuestMemory::UnchangedSince(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), generation)) {
                // One stamp scan for every unit (each is one stamp block); the unchanged units move
                // to the current generation, so the whole-surface check answers next time.
                scanBlocks();
                for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
                    if (stampedBlocks[unit] == 0) {
                        layerGeneration[unit] = current;
                        continue;
                    }
                    changed[unit] = true;
                    unchanged = false;
                }
                refreshGeneration();
            }
        } else {
            for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
                if (GuestMemory::UnchangedSince(layerBegin(layer), static_cast<std::size_t>(layerBytes(layer)), layerGeneration[layer])) continue;
                changed[layer] = true;
                unchanged = false;
            }
            if (!unchanged) scanBlocks();
        }
        // "cpu" names a CPU store in both models; a block without a generation (untracked) says
        // nothing about who wrote it.
        if (compared && !keysChanged) {
            for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
                const auto first = layerBegin(layer) / 65536 - descriptor.baseAddress / 65536;
                const auto last = (layerBegin(layer) + layerBytes(layer) - 1) / 65536 - descriptor.baseAddress / 65536;
                changed[layer] = std::any_of(stampedBlocks.begin() + first, stampedBlocks.begin() + last + 1, [](auto value) { return value != GuestMemory::BlockUnchanged; });
            }
            unchanged = std::none_of(changed.begin(), changed.end(), [](bool value) { return value; });
        }
        for (std::size_t k = 0; k < cpuBlocks.size(); ++k) {
            if (cpuBlocks[k] != 0 && (compared || (tracked && generations[k] != 0))) cpuWrote = true;
        }
        // Unchanged bytes rescue an upload only when nothing else moved: with the keys changed
        // the same bytes read differently, and a changed unit holding pending results must take
        // the drop rule below (the image holds the results, `original` what the CPU wrote back).
        bool pendingChanged = false;
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (changed[layer] && layerPending[layer]) pendingChanged = true;
        }
        if (!unchanged && !keysChanged && !pendingChanged && originalValid && equalsOriginal()) {
            unchanged = true;
            stamped = false;
        }
        // Only the direct path uploads the selected layers alone (see upload); the others replace
        // the whole image, so every pending layer's results are stored first.
        direct = keys == DccKeys::Uncompressed && HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) != nullptr;
        if (alias != nullptr && direct) {
            // The alias's pending units that nothing stamped since its generation (a CPU store or a
            // driver store there wins over its results, as at its own write-back) are taken from
            // its image: every one of them, since its write-back would have stamped them all. Units
            // already holding them at the same alias version stay as they are.
            const bool sameBorrow = borrowedFrom.lock() == alias && borrowedVersion == alias->version && borrowedUnits.size() == trackedLayers;
            std::vector<std::uint8_t> aliasStamped(trackedLayers);
            const bool aliasTracked = GuestMemory::ChangedBlocks(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), alias->layerGeneration, aliasStamped);
            borrowed.assign(trackedLayers, false);
            for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
                if (!aliasTracked || !alias->layerPending[unit] || aliasStamped[unit] != 0 || alias->layerGeneration[unit] == 0 || layerPending[unit]) continue;
                if (sameBorrow && borrowedUnits[unit]) continue;
                borrowed[unit] = true;
                anyBorrowed = true;
                changed[unit] = false;
                unchanged = false;
            }
            if (!sameBorrow) {
                borrowedFrom.reset();
                borrowedUnits.clear();
            }
        }
    }
    if (unchanged) {
        ++Profile().storageReused;
        layerGeneration.assign(trackedLayers, current);
        refreshGeneration();
        if (profile) LookupOutcomes::Add(stamped ? LookupOutcomes::RefreshUnchanged : LookupOutcomes::RefreshCompared, start);
        return true;
    }
    CaptureTrace::Log("refresh image=%llx bytes=%llu generation=%llu dirty=%d keysChanged=%d cpuWrote=%d", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), static_cast<unsigned long long>(generation), dirty, keysChanged, cpuWrote);
    // Debug aid: APS5_TRACE_UPLOAD names why a storage image is uploaded again.
    static const bool traceUpload = std::getenv("APS5_TRACE_UPLOAD") != nullptr;
    if (traceUpload) {
        const auto keys = TextureClearKeys(descriptor, guestBytes);
        std::fprintf(stderr, "[upload] 0x%llx+0x%llx %ux%u mips %u: %s (keys %s -> %s, originalValid %d, dirty %d)\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), descriptor.width, descriptor.height, descriptor.mipCount, keys != uploadedKeys ? "DCC keys changed" : "guest memory changed", DccKeysName(uploadedKeys), DccKeysName(keys), originalValid ? 1 : 0, dirty ? 1 : 0);
    }
    // Changed layers with results pending: the CPU wrote this memory while GPU results were
    // pending, so as with an immediate write-back followed by the CPU write, its blocks win and the
    // results land everywhere else in those layers; the unchanged pending layers keep their
    // results on the GPU when the direct path leaves them alone, and are stored otherwise. The
    // image stays registered throughout (the store clears what it stored).
    std::vector<bool> stored(trackedLayers, false);
    bool pendingResults = false;
    bool anyStored = false;
    bool dropped = false;
    // A unit is one 64 KiB block, which the CPU's bytes win whole once a store stamped it:
    // nothing of its results is stored, and the unit re-uploads from memory below. A keys change
    // alone (the title marking the surface uncompressed behind a pass) leaves unstamped units
    // with their results, which are stored first, as the layer model does; new keys that are a
    // clear code make every result dead on hardware too. Without a generation (no tracking) a
    // unit's stamps say nothing, so it is stored.
    const auto droppable = [&](std::uint32_t unit) {
        if (keysChanged && IsDccClear(keys)) return true;
        if (compared) return stampedBlocks.at(unit) == GuestMemory::BlockWritten;
        if (!tracked || layerGeneration[unit] == 0 || unit >= stampedBlocks.size() || stampedBlocks[unit] != GuestMemory::BlockWritten) return false;
        const auto begin = layerBegin(unit);
        const auto bytes = layerBytes(unit);
        const bool edge = begin % 65536 != 0 || bytes != 65536;
        return !edge || GuestMemory::StoredOver(begin, static_cast<std::size_t>(bytes), layerGeneration[unit]);
    };
    // A clear code -> uncompressed flip on an image with results pending: an unstamped pending
    // unit's results ARE the uncompressed texels (nothing wrote its memory since), so it keeps
    // them instead of being dropped and re-read from stale guest bytes; a stamped one takes the
    // drop rule. Debug aid: APS5_KEYFLIP_DROP=1 drops as before.
    static const bool keyFlipDrop = std::getenv("APS5_KEYFLIP_DROP") != nullptr;
    static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    const bool keyFlip = keysChanged && blockUnits && !keyFlipDrop && IsDccClear(uploadedKeys) && keys == DccKeys::Uncompressed && stampedBlocks.size() >= trackedLayers;
    if (traceKeys && keysChanged) {
        std::size_t pendingUnits = 0, stampedUnits = 0;
        for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
            if (layerPending[unit]) ++pendingUnits;
            if (unit < cpuBlocks.size() && cpuBlocks[unit] != 0 && generations[unit] != 0) ++stampedUnits;
        }
        std::fprintf(stderr, "[dcc-keys] refresh 0x%llx+0x%llx keys %s -> %s (dirty %d, %zu pending units, %zu cpu-stamped units%s)\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), DccKeysName(uploadedKeys), DccKeysName(keys), dirty ? 1 : 0, pendingUnits, stampedUnits, keyFlip ? ", flip keeps unstamped results" : "");
    }
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layerPending[layer]) continue;
        if (keyFlip && layerGeneration[layer] != 0 && stampedBlocks[layer] != GuestMemory::BlockWritten) {
            changed[layer] = false;
            keyFlipKept.fetch_add(1, std::memory_order_relaxed);
            if (direct) continue;
            stored[layer] = true;
            anyStored = true;
            continue;
        }
        if (changed[layer]) {
            pendingResults = true;
            if (blockUnits && droppable(layer)) {
                CaptureTrace::Log("drop-unit image=%llx unit=%u generation=%llu stamped=%d keys=%d", static_cast<unsigned long long>(descriptor.baseAddress), layer, static_cast<unsigned long long>(layerGeneration[layer]), layer < stampedBlocks.size() && stampedBlocks[layer] != 0, static_cast<int>(keys));
                layerPending[layer] = false;
                dropped = true;
                continue;
            }
        } else if (direct) {
            continue;
        }
        stored[layer] = true;
        anyStored = true;
    }
    if (anyStored && !keysChanged && descriptor.dccAddress != 0 && IsDccClear(uploadedKeys)) stored.assign(trackedLayers, true);
    if (pendingResults) {
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] storage image 0x%llx: guest memory changed while GPU results were pending; keeping the CPU's blocks\n", static_cast<unsigned long long>(descriptor.baseAddress));
    }
    if (anyStored) {
        const auto previous = std::exchange(flushReason, "refresh");
        writeBackLayers(stored);
        flushReason = previous;
    } else if (dropped) {
        reconcilePending();
    }
    if (anyBorrowed) {
        uploadReason = "alias";
        borrowUnits(*alias, borrowed);
        for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
            if (borrowed[unit]) layerGeneration[unit] = current;
        }
        refreshGeneration();
    }
    if (std::any_of(changed.begin(), changed.end(), [](bool selected) { return selected; })) {
        uploadReason = keysChanged ? "keys" : cpuWrote ? "cpu" : generation == 0 ? "untracked" : flushed ? "flushed" : "store";
        upload(&changed);
    } else {
        uploadedKeys = keys;
    }
    return false;
}

DccKeys StorageTexture::ProvedKeys() const {
    std::optional<DccKeys> keys;
    if (descriptor.dccAddress != 0 && uploadedKeys == DccKeys::Uncompressed) keys = WaitForKeyWriters(descriptor, guestBytes);
    if (keys.has_value()) keyProof = {};
    else keys = ProvedClearKeys(descriptor, guestBytes, keyProof);
    if (IsDccClear(filledKeys) && *keys != filledKeys && !anyLayerPending()) filledKeys = DccKeys::Uncompressed;
    return *keys;
}

bool StorageTexture::ServesKeysAt(std::uint64_t dccAddress) const {
    const bool locked = GuestMemory::GpuMutex().HeldByThisThread();
    const auto readFollowed = [&] {
        DccKeyProof unlocked;
        return ProvedClearKeys(descriptor, guestBytes, locked ? keyProof : unlocked);
    };
    const auto readNamed = [&] {
        auto named = descriptor;
        named.dccAddress = dccAddress;
        DccKeyProof unlocked;
        if (!locked) return ProvedClearKeys(named, guestBytes, unlocked);
        auto slot = std::find_if(foreignKeyProofs.begin(), foreignKeyProofs.end(), [&](const ForeignKeyProof& entry) { return entry.dccAddress == dccAddress; });
        if (slot == foreignKeyProofs.end()) {
            slot = foreignKeyProofs.begin() + nextForeignKeyProof++ % foreignKeyProofs.size();
            *slot = ForeignKeyProof{dccAddress, {}};
        }
        return ProvedClearKeys(named, guestBytes, slot->proof);
    };
    return KeysServeSurface(descriptor.dccAddress, uploadedKeys, filledKeys, dccAddress, readFollowed, readNamed);
}

namespace {

// Debug aid (APS5_TRACE_DCC_KEYS=1): who stores uncompressed keys over a surface (recommendation
// 2's tracer): the write-back path, the surface and its key range.
void traceKeyStore(const char* path, const GuestTextureResource& descriptor, std::uint64_t guestBytes) {
    static const bool trace = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    if (!trace || descriptor.dccAddress == 0) return;
    const auto packet = GuestMemory::CurrentPacket();
    std::fprintf(stderr, "[dcc-keys] uncompressed store by %s (%s) for surface 0x%llx+0x%llx: keys 0x%llx+0x%llx (packet 0x%x queue 0x%x)\n", path, flushReason != nullptr ? flushReason : "?", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), static_cast<unsigned long long>(descriptor.dccAddress), static_cast<unsigned long long>(DccKeyCount(descriptor, guestBytes)), packet.opcode, packet.queue);
}

}

void StorageTexture::captureGuestBytes(const std::vector<bool>* layers) {
    comparedGuestBytes = {};
    const bool complete = layers == nullptr || std::all_of(layers->begin(), layers->end(), [](bool selected) { return selected; });
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (layers != nullptr && !(*layers)[layer]) continue;
        const auto offset = static_cast<std::size_t>(layerBegin(layer) - descriptor.baseAddress);
        GuestMemory::ReadCommitted(layerBegin(layer), std::span(original).subspan(offset, static_cast<std::size_t>(layerBytes(layer))));
    }
    originalValid = originalValid || complete;
}

bool StorageTexture::compareUntracked(std::uint64_t address, std::size_t bytes, std::span<std::uint8_t> changed, bool memoize) const {
    if (!originalValid) return false;
    struct Exempt {
        const StorageTexture* previous;
        ~Exempt() { refreshing = previous; }
    } exempt{refreshing};
    refreshing = this;
    Require(address >= descriptor.baseAddress && bytes <= guestBytes && address - descriptor.baseAddress <= guestBytes - bytes, "texture comparison exceeds its snapshot");
    Require(original.size() == guestBytes, "texture comparison has no complete snapshot");
    GuestMemory::FlushGpuWrites(address, bytes);
    if (!originalValid) return false;
    constexpr std::uint64_t blockBytes = 65536;
    const auto end = address + bytes;
    const auto first = address / blockBytes;
    Require(changed.size() == (end - 1) / blockBytes - first + 1, "texture comparison block count differs");
    const auto* recorder = Recorder::Active();
    const std::array<std::uint64_t, 4> stamp{GuestMemory::CollectEpoch(), GuestMemory::TrackerGeneration(), GuestMemory::ForgetSerial(), recorder != nullptr ? recorder->NewestWriteNote(address, bytes) : 0};
    const bool cacheable = memoize && stamp[0] != 0 && (stamp[2] & 1u) == 0 && address == descriptor.baseAddress && bytes == guestBytes;
    if (cacheable && comparedGuestBytes == stamp) {
        std::fill(changed.begin(), changed.end(), GuestMemory::BlockUnchanged);
        return true;
    }
    const auto saved = std::span(original).subspan(static_cast<std::size_t>(address - descriptor.baseAddress), bytes);
    if (GuestMemory::EqualsCommittedUnsynced(address, saved)) {
        std::fill(changed.begin(), changed.end(), GuestMemory::BlockUnchanged);
        if (cacheable) comparedGuestBytes = stamp;
        return true;
    }
    comparedGuestBytes = {};
    for (std::size_t index = 0; index < changed.size(); ++index) {
        const auto begin = std::max(address, (first + index) * blockBytes);
        const auto stop = std::min(end, (first + index + 1) * blockBytes);
        const auto saved = std::span(original).subspan(static_cast<std::size_t>(begin - descriptor.baseAddress), static_cast<std::size_t>(stop - begin));
        changed[index] = GuestMemory::EqualsCommittedUnsynced(begin, saved) ? GuestMemory::BlockUnchanged : GuestMemory::BlockWritten;
    }
    return true;
}

constexpr std::uint64_t GenerationBaselineBytes = 1ull << 20;

void StorageTexture::upload(const std::vector<bool>* layers) {
    CaptureTrace::Log("upload image=%llx bytes=%llu generation=%llu reason=%s partial=%d", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), static_cast<unsigned long long>(generation), uploadReason, layers != nullptr);
    const bool profile = LookupOutcomes::Profiled();
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto elementBytes = BytesPerElement(descriptor.format);
    const auto linearBytes = sliceLinearBytes * arrayLayers;
    original.resize(static_cast<std::size_t>(guestBytes));
    // A layer selection applies to the direct path alone (the others upload everything); it names
    // array layers, which the tracked layers are when there are several.
    if (layers != nullptr && ((!blockUnits && trackedLayers != arrayLayers) || std::all_of(layers->begin(), layers->end(), [](bool selected) { return selected; }))) layers = nullptr;
    const auto now = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    const auto stampLayers = [&](bool selectedOnly) {
        const bool whole = !selectedOnly || layers == nullptr;
        if (guestBytes > GenerationBaselineBytes) generationBaseline.clear();
        else if (whole) {
            generationBaseline.resize(static_cast<std::size_t>(guestBytes));
            GuestMemory::ReadCommitted(descriptor.baseAddress, generationBaseline);
        }
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (!whole && !(*layers)[layer]) continue;
            layerGeneration[layer] = now;
            if (whole || generationBaseline.size() != guestBytes) continue;
            const auto offset = static_cast<std::size_t>(layerBegin(layer) - descriptor.baseAddress);
            GuestMemory::ReadCommitted(layerBegin(layer), std::span<std::byte>(generationBaseline).subspan(offset, static_cast<std::size_t>(layerBytes(layer))));
        }
        refreshGeneration();
    };
    uploadedKeys = ProvedClearKeys(descriptor, guestBytes, keyProof);
    filledKeys = DccKeys::Uncompressed;
    VkClearColorValue clearValue{};
    if (IsDccClear(uploadedKeys) && ClearColorFor(storageFormat, uploadedKeys, clearValue)) {
        // A fast-cleared surface is cleared on the GPU; its texel memory is neither read nor filled.
        // The clear is recorded into the open batch like a direct upload (the image kept by it): a
        // batch of its own submitted the recorder's work first and waited for all of it, 25-40 ms
        // under the GPU mutex at the movie stage. APS5_NO_RECORDED_CLEAR=1 waits as before.
        static_cast<void>(HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)));
        if (GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) originalValid = false;
        else captureGuestBytes(nullptr);
        forgetBorrowed(0, trackedLayers);
        stampLayers(false);
        static const bool recordClear = std::getenv("APS5_NO_RECORDED_CLEAR") == nullptr;
        auto* recorder = recordClear ? Recorder::Active() : nullptr;
        std::unique_ptr<CommandBatch> batch;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        auto timing = Recorder::NoTiming;
        if (recorder != nullptr) {
            commands = recorder->Commands();
            timing = recorder->BeginGpuTiming(Recorder::CommandClass::DccClear);
            if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
            ++Profile().storageRecordedClears;
            Recorder::CountBarriers(Recorder::CommandClass::DccClear, 2);
            if (Recorder::BarrierValidate()) {
                const std::pair<VkImage, bool> cleared{image, true};
                recorder->NoteAccess(Recorder::CommandClass::DccClear, Recorder::Access{{}, {}, std::span(&cleared, 1), VK_PIPELINE_STAGE_TRANSFER_BIT});
            }
        } else {
            batch = std::make_unique<CommandBatch>(context);
            commands = batch->Handle();
            ++Profile().storageWaitedClears;
        }
        VkImageMemoryBarrier toClear{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toClear.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toClear.image = image;
        toClear.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toClear);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &toClear.subresourceRange);
        VkImageMemoryBarrier toGeneral = toClear;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        if (batch) batch->SubmitAndWait();
        else recorder->EndGpuTiming(timing, guestBytes);
        countStorageUpload(0, guestBytes);
        ++version;
        if (profile) LookupOutcomes::Add(LookupOutcomes::UploadClear, start);
        return;
    }
    if (const auto* import = uploadedKeys == DccKeys::Uncompressed ? HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) : nullptr) {
        if (GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) originalValid = false;
        else captureGuestBytes(layers);
        stampLayers(true);
        // A whole-surface upload of a block-unit image goes through the windows too when a unit
        // shadow holds part of the surface (the detile then reads the slabs; a new image has no
        // layout yet); a partial one always does.
        if (blockUnits && (layers != nullptr || AnyShadowedOverlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)))) {
            std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
            if (layers != nullptr) {
                for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
                    if ((*layers)[unit]) forgetBorrowed(unit, 1);
                }
                runs = unitRuns(*layers);
            } else {
                forgetBorrowed(0, trackedLayers);
                runs.emplace_back(0, guestBytes);
            }
            const auto uploadedBytes = uploadWindows(*import, runs, layers == nullptr && version == 0);
            countStorageUpload(1, uploadedBytes);
            if (layers != nullptr) {
                partialUploads.fetch_add(1, std::memory_order_relaxed);
                partialUploadBytes.fetch_add(uploadedBytes, std::memory_order_relaxed);
            }
            ++Profile().storageDirectUploads;
            ++version;
            if (profile) LookupOutcomes::Add(LookupOutcomes::UploadDirect, start);
            return;
        }
        // Units another image's results shadow reach the import before the detile reads it in place.
        PublishShadow(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), PublishScope::Whole, PublishReason::Upload);
        forgetBorrowed(0, trackedLayers);
        std::uint64_t uploadedBytes = layers == nullptr ? guestBytes : 0;
        if (layers != nullptr) {
            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                if ((*layers)[layer]) uploadedBytes += trackedLayerBytes;
            }
        }
        auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        detiler.BeginBatch();
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        auto timing = Recorder::NoTiming;
        if (recorder != nullptr) {
            // A queued label store in the surface lands before the detile reads it.
            recorder->FlushStoresOverlapping(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
            commands = recorder->Commands();
            timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageUpload);
            recorder->Keep(linear, linear->Size());
            // The image itself must outlive the recorded copy: the cache may evict it right after.
            if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
            // The detile reads the tiled bytes from the import when the batch runs.
            recorder->NotePendingRead(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), Recorder::ReadKind::StorageUpload);
            Recorder::CountBarriers(Recorder::CommandClass::StorageUpload, 3);
            if (Recorder::BarrierValidate()) {
                const std::pair<std::uint64_t, std::uint64_t> read{descriptor.baseAddress, descriptor.baseAddress + guestBytes};
                const std::pair<VkImage, bool> written{image, true};
                recorder->NoteAccess(Recorder::CommandClass::StorageUpload, Recorder::Access{std::span(&read, 1), {}, std::span(&written, 1), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT});
            }
        } else {
            batch = std::make_unique<CommandBatch>(context);
            commands = batch->Handle();
        }
        const auto importOffset = descriptor.baseAddress - import->base;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            if (layers != nullptr && !(*layers)[layer]) continue;
            for (std::uint32_t level = 0; level < mips.size(); ++level) {
                if (!geometry.HasLayer(level, layer)) continue;
                const auto& mip = mips[level];
                detiler.Dispatch(commands, descriptor.tileMode, elementBytes, import->buffer, importOffset + geometry.GuestLayerOffset(layer) + mip.tiledOffset, linear->Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, mip, false, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
            }
        }
        const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.srcAccessMask = layers != nullptr ? VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0u;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        // Some layers keep their content: the others are replaced, not the whole image.
        toTransfer.oldLayout = layers != nullptr ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearRead, 1, &toTransfer);
        const auto regions = CopyRegions(layers);
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
        VkImageMemoryBarrier toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        if (batch) batch->SubmitAndWait();
        else recorder->EndGpuTiming(timing, uploadedBytes);
        countStorageUpload(1, uploadedBytes);
        ++Profile().storageDirectUploads;
        ++version;
        if (profile) LookupOutcomes::Add(LookupOutcomes::UploadDirect, start);
        return;
    }
    originalValid = true;
    forgetBorrowed(0, trackedLayers);
    stampLayers(false);
    GuestMemory::ReadCommitted(descriptor.baseAddress, original);
    {
            Buffer staging(context, original.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            if (uploadedKeys == DccKeys::Uncompressed) std::memcpy(staging.Bytes().data(), original.data(), original.size());
            else ReadTextureSurface(descriptor, uploadedKeys, staging.Bytes().first(original.size()));
            DeviceBuffer tiled(context, original.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            DeviceBuffer linear(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            detiler.BeginBatch();
            CommandBatch batch(context);
            const auto commands = batch.Handle();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, staging.Handle(), 0, tiled.Handle(), 0, original.size());
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                for (std::uint32_t level = 0; level < mips.size(); ++level) {
                    if (!geometry.HasLayer(level, layer)) continue;
                    const auto& mip = mips[level];
                    detiler.Dispatch(commands, descriptor.tileMode, elementBytes, tiled.Handle(), geometry.GuestLayerOffset(layer) + mip.tiledOffset, linear.Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, mip, false, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
                }
            }
            const auto linearRead = WholeBufferBarrier(linear.Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.image = image;
            toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearRead, 1, &toTransfer);
            const auto regions = CopyRegions();
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear.Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
            VkImageMemoryBarrier toGeneral = toTransfer;
            toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
            APS5_LOG_CHARS_OUT("StorageTexture upload submit");
            batch.SubmitAndWait();
            APS5_LOG_CHARS_OUT("StorageTexture upload done");
    }
    countStorageUpload(2, guestBytes);
    ++version;
    if (profile) LookupOutcomes::Add(LookupOutcomes::UploadCpu, start);
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> StorageTexture::unitRuns(const std::vector<bool>& units) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (!units[unit]) continue;
        const auto begin = static_cast<std::uint64_t>(unit) * trackedLayerBytes;
        const auto end = begin + layerBytes(unit);
        if (!runs.empty() && runs.back().second == begin) runs.back().second = end;
        else runs.emplace_back(begin, end);
    }
    return runs;
}

std::vector<StorageTexture::SliceWindow> StorageTexture::sliceWindows(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs) const {
    std::vector<SliceWindow> windows;
    const auto elementBytes = BytesPerElement(descriptor.format);
    const auto block = ThinBlockLayout(descriptor.tileMode, elementBytes);
    for (const auto& [runBegin, runEnd] : runs) {
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
                if (!geometry.HasLayer(level, layer)) continue;
                const auto& mip = mips[level];
                const auto sliceBegin = geometry.GuestLayerOffset(layer) + mip.tiledOffset;
                const auto sliceEnd = sliceBegin + mip.tiledSize;
                if (runEnd <= sliceBegin || sliceEnd <= runBegin) continue;
                SliceWindow window{layer, level, 0, 0, {}, 0, {}};
                const auto pitch = static_cast<std::uint32_t>(mip.pitchBytes / elementBytes * BlockWidth(descriptor.format));
                if (mip.tail) {
                    // The tail block holds every tail level: each is moved whole.
                    window.tiledBegin = sliceBegin;
                    window.tiledEnd = sliceEnd;
                    window.window = {0, static_cast<std::uint32_t>(mip.tiledSize), 0, 0, mip.linearSize};
                    window.linearBytes = mip.linearSize;
                    VkBufferImageCopy region{};
                    region.bufferRowLength = pitch;
                    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
                    region.imageOffset = {0, 0, geometry.CopyDepth(layer)};
                    region.imageExtent = {mip.width, mip.height, 1u};
                    window.regions.push_back(region);
                    windows.push_back(std::move(window));
                    continue;
                }
                const auto low = std::max(runBegin, sliceBegin) - sliceBegin;
                const auto high = std::min(runEnd, sliceEnd) - sliceBegin;
                Require(low % block[0] == 0 && high % block[0] == 0, "storage image window is not tile-block aligned");
                const auto firstBlock = low / block[0];
                const auto lastBlock = (high - 1) / block[0];
                const auto firstRow = static_cast<std::uint32_t>(firstBlock / mip.blocksPerRow);
                const auto lastRow = static_cast<std::uint32_t>(lastBlock / mip.blocksPerRow);
                const auto rowBegin = firstRow * block[2];
                const auto rowEnd = std::min(mip.height, (lastRow + 1u) * block[2]);
                window.tiledBegin = sliceBegin + low;
                window.tiledEnd = sliceBegin + high;
                window.linearBytes = static_cast<std::uint64_t>(rowEnd - rowBegin) * mip.pitchBytes;
                window.window = {static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(high), static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(rowBegin) * mip.pitchBytes, window.linearBytes};
                // The dispatched rectangle: the block rows, and inside one block row the block span.
                window.window.rowBegin = rowBegin;
                window.window.rowEnd = rowEnd;
                if (firstRow == lastRow) {
                    window.window.columnBegin = static_cast<std::uint32_t>(firstBlock - static_cast<std::uint64_t>(firstRow) * mip.blocksPerRow) * block[1];
                    window.window.columnEnd = std::min(mip.width, static_cast<std::uint32_t>(lastBlock - static_cast<std::uint64_t>(firstRow) * mip.blocksPerRow + 1u) * block[1]);
                }
                // One region per block row; whole rows merge (a run's middle rows are whole).
                for (auto row = firstRow; row <= lastRow; ++row) {
                    const auto rowFirst = std::max<std::uint64_t>(firstBlock, static_cast<std::uint64_t>(row) * mip.blocksPerRow);
                    const auto rowLast = std::min<std::uint64_t>(lastBlock, static_cast<std::uint64_t>(row + 1u) * mip.blocksPerRow - 1u);
                    const auto x0 = static_cast<std::uint32_t>(rowFirst - static_cast<std::uint64_t>(row) * mip.blocksPerRow) * block[1];
                    const auto x1 = std::min(mip.width, static_cast<std::uint32_t>(rowLast - static_cast<std::uint64_t>(row) * mip.blocksPerRow + 1u) * block[1]);
                    const auto y0 = row * block[2];
                    const auto y1 = std::min(mip.height, (row + 1u) * block[2]);
                    if (x0 >= x1 || y0 >= y1) continue;
                    if (!window.regions.empty()) {
                        auto& previous = window.regions.back();
                        const bool wholeRows = previous.imageOffset.x == 0 && previous.imageExtent.width == mip.width && x0 == 0 && x1 == mip.width;
                        if (wholeRows && static_cast<std::uint32_t>(previous.imageOffset.y) + previous.imageExtent.height == y0) {
                            previous.imageExtent.height += y1 - y0;
                            continue;
                        }
                    }
                    VkBufferImageCopy region{};
                    region.bufferOffset = static_cast<std::uint64_t>(y0 - rowBegin) * mip.pitchBytes + static_cast<std::uint64_t>(x0) * elementBytes;
                    region.bufferRowLength = pitch;
                    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
                    region.imageOffset = {static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0), geometry.CopyDepth(layer)};
                    region.imageExtent = {x1 - x0, y1 - y0, 1u};
                    window.regions.push_back(region);
                }
                windows.push_back(std::move(window));
            }
        }
    }
    return windows;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> StorageTexture::uncoveredBytes(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> covered;
    if (!geometry.thick) {
        const auto elementBytes = BytesPerElement(descriptor.format);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
                if (!geometry.HasLayer(level, layer)) continue;
                const auto sliceBegin = geometry.GuestLayerOffset(layer) + mips[level].tiledOffset;
                for (const auto& [begin, end] : CoveredMipBytes(descriptor.tileMode, elementBytes, mips[level])) covered.emplace_back(sliceBegin + begin, sliceBegin + end);
            }
        }
        std::sort(covered.begin(), covered.end());
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> sorted(runs.begin(), runs.end());
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> uncovered;
    const auto add = [&](std::uint64_t begin, std::uint64_t end) {
        if (begin >= end) return;
        if (!uncovered.empty() && uncovered.back().second >= begin) uncovered.back().second = std::max(uncovered.back().second, end);
        else uncovered.emplace_back(begin, end);
    };
    std::size_t next = 0;
    for (const auto& [runBegin, runEnd] : sorted) {
        while (next < covered.size() && covered[next].second <= runBegin) ++next;
        auto at = runBegin;
        for (auto i = next; i < covered.size() && covered[i].first < runEnd; ++i) {
            add(at, std::min(covered[i].first, runEnd));
            at = std::max(at, covered[i].second);
        }
        add(at, runEnd);
    }
    return uncovered;
}

StorageTexture::PaddingSeeds StorageTexture::paddingSeeds(const HostImport& import, std::vector<CopiedBytes> copied) const {
    PaddingSeeds seeds;
    std::sort(copied.begin(), copied.end(), [](const CopiedBytes& a, const CopiedBytes& b) { return a.begin < b.begin; });
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    for (const auto& range : copied) runs.emplace_back(range.begin, range.end);
    const auto uncovered = uncoveredBytes(runs);
    if (uncovered.empty()) return seeds;
    std::vector<CopiedBytes> pieces;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pieceRuns;
    std::size_t next = 0;
    for (const auto& [begin, end] : uncovered) {
        while (next < copied.size() && copied[next].end <= begin) ++next;
        for (auto i = next; i < copied.size() && copied[i].begin < end; ++i) {
            const auto from = std::max(begin, copied[i].begin);
            const auto to = std::min(end, copied[i].end);
            if (from >= to) continue;
            pieces.push_back({from, to, copied[i].scratch + (from - copied[i].begin)});
            pieceRuns.emplace_back(from, to);
        }
    }
    if (pieces.empty()) return seeds;
    const auto sources = ShadowSources(context, import, descriptor.baseAddress, pieceRuns, {}, false);
    std::size_t first = 0;
    for (const auto& source : sources) {
        while (first < pieces.size() && pieces[first].end <= source.begin) ++first;
        for (auto i = first; i < pieces.size() && pieces[i].begin < source.end; ++i) {
            const auto from = std::max(source.begin, pieces[i].begin);
            const auto to = std::min(source.end, pieces[i].end);
            if (from >= to) continue;
            auto target = std::find_if(seeds.copies.begin(), seeds.copies.end(), [&](const auto& entry) { return entry.first == source.buffer; });
            if (target == seeds.copies.end()) target = seeds.copies.insert(seeds.copies.end(), {source.buffer, {}});
            target->second.push_back({source.offset + (from - source.begin), pieces[i].scratch + (from - pieces[i].begin), to - from});
            if (!source.shadow) {
                const auto guestBegin = descriptor.baseAddress + from;
                if (!seeds.importReads.empty() && seeds.importReads.back().second == guestBegin) seeds.importReads.back().second = descriptor.baseAddress + to;
                else seeds.importReads.emplace_back(guestBegin, descriptor.baseAddress + to);
            }
        }
        if (source.shadow && std::find(seeds.slabs.begin(), seeds.slabs.end(), source.slab) == seeds.slabs.end()) seeds.slabs.push_back(source.slab);
    }
    return seeds;
}

std::uint64_t StorageTexture::uploadWindows(const HostImport& import, std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, bool discard) {
    const auto elementBytes = BytesPerElement(descriptor.format);
    // Each run's pieces by source (a unit shadow's slab while fresh, else the import), the tail
    // blocks moved whole by one window each; the windows are built per piece.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> tailBlocks;
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            const auto& mip = mips[level];
            if (!mip.tail) continue;
            const auto sliceBegin = geometry.GuestLayerOffset(layer) + mip.tiledOffset;
            if (tailBlocks.empty() || tailBlocks.back().first != sliceBegin) tailBlocks.emplace_back(sliceBegin, sliceBegin + mip.tiledSize);
        }
    }
    const auto sources = ShadowSources(context, import, descriptor.baseAddress, runs, tailBlocks);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pieces;
    bool allShadow = true;
    for (const auto& source : sources) {
        pieces.emplace_back(source.begin, source.end);
        if (!source.shadow) allShadow = false;
    }
    if (allShadow && !sources.empty() && (std::strcmp(uploadReason, "flushed") == 0 || std::strcmp(uploadReason, "store") == 0)) uploadReason = "shadow";
    auto windows = sliceWindows(pieces);
    Require(!windows.empty(), "storage image upload selects no unit");
    const auto sourceOf = [&](const SliceWindow& window) -> const ShadowRun& {
        for (const auto& source : sources) {
            if (source.begin <= window.tiledBegin && window.tiledBegin < source.end) return source;
        }
        throw std::runtime_error("AGC graphics: storage image window has no source");
    };
    // One linear buffer for every window, each at a 256-byte position (the detiler's descriptor
    // offsets are aligned by the dispatch).
    std::vector<std::uint64_t> positions;
    std::uint64_t linearTotal = 0, uploadedBytes = 0;
    for (const auto& window : windows) {
        positions.push_back(linearTotal);
        linearTotal += (window.linearBytes + 255) & ~std::uint64_t{255};
    }
    for (const auto& [begin, end] : runs) uploadedBytes += end - begin;
    auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearTotal), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    detiler.BeginBatch();
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    auto timing = Recorder::NoTiming;
    if (recorder != nullptr) {
        // The import pieces are read in place when the batch runs; the slab pieces are the
        // device's own.
        std::vector<std::pair<std::uint64_t, std::uint64_t>> reads;
        for (const auto& source : sources) {
            if (!source.shadow) reads.emplace_back(descriptor.baseAddress + source.begin, descriptor.baseAddress + source.end);
        }
        // A queued label store in a run lands before the detile reads it.
        for (const auto& [begin, end] : reads) recorder->FlushStoresOverlapping(begin, static_cast<std::size_t>(end - begin));
        commands = recorder->Commands();
        timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageUpload);
        recorder->Keep(linear, linear->Size());
        for (const auto& source : sources) {
            if (source.slab != nullptr) recorder->Keep(source.slab);
        }
        if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
        if (!reads.empty()) recorder->NotePendingReads(reads, Recorder::ReadKind::StorageUpload);
        Recorder::CountBarriers(Recorder::CommandClass::StorageUpload, 3);
        if (Recorder::BarrierValidate()) {
            const std::pair<VkImage, bool> written{image, true};
            recorder->NoteAccess(Recorder::CommandClass::StorageUpload, Recorder::Access{reads, {}, std::span(&written, 1), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT});
        }
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    const auto importOffset = descriptor.baseAddress - import.base;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    std::vector<VkBufferImageCopy> regions;
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        const auto& mip = mips[window.level];
        const auto& source = sourceOf(window);
        auto detile = window.window;
        detile.pipeBankXor = descriptor.pipeBankXor;
        if (source.shadow) {
            // The slab holds the piece from the window's first tiled byte: the window's tiled base
            // is its range begin (sliceWindows builds it so), read from the piece's slab offset.
            detiler.Dispatch(commands, descriptor.tileMode, elementBytes, source.buffer, source.offset + (window.tiledBegin - source.begin), linear->Handle(), positions[i], mip, false, window.layer, false, detile);
        } else {
            // The import holds the whole mip: the window's tiled base is the mip's.
            detile.tiledBase = 0;
            detiler.Dispatch(commands, descriptor.tileMode, elementBytes, import.buffer, importOffset + geometry.GuestLayerOffset(window.layer) + mip.tiledOffset, linear->Handle(), positions[i], mip, false, window.layer, false, detile);
        }
        for (auto region : window.regions) {
            region.bufferOffset += positions[i];
            regions.push_back(region);
        }
    }
    const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.srcAccessMask = discard ? 0u : VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    // The other blocks keep their content (a new image, every block of which is written, has none).
    toTransfer.oldLayout = discard ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image;
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearRead, 1, &toTransfer);
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
    VkImageMemoryBarrier toGeneral = toTransfer;
    toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    if (batch) batch->SubmitAndWait();
    else recorder->EndGpuTiming(timing, uploadedBytes);
    return uploadedBytes;
}

std::uint64_t StorageTexture::writeBackWindows(const HostImport& import, std::span<const std::pair<std::uint64_t, std::uint64_t>> keep, std::uint64_t firstStored, std::uint64_t lastStored, std::vector<ShadowedRange>& shadowed, std::vector<std::pair<std::uint64_t, std::uint64_t>>& imported) {
    const auto elementBytes = BytesPerElement(descriptor.format);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    std::uint64_t storedBytes = 0;
    for (const auto& [from, to] : keep) {
        runs.emplace_back(from - descriptor.baseAddress, to - descriptor.baseAddress);
        storedBytes += to - from;
    }
    if (runs.empty()) return 0;
    auto windows = sliceWindows(runs);
    // The linear rows and the tiled bytes of every window, each at a 256-byte position; the tail
    // levels of a layer retile into one scratch region (their block is shared).
    std::vector<std::uint64_t> linearPositions, scratchPositions;
    std::uint64_t linearTotal = 0, scratchTotal = 0;
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        linearPositions.push_back(linearTotal);
        linearTotal += (window.linearBytes + 255) & ~std::uint64_t{255};
        std::size_t shared = 0;
        while (shared < i && (windows[shared].tiledBegin != window.tiledBegin || windows[shared].tiledEnd != window.tiledEnd)) ++shared;
        if (shared < i) {
            scratchPositions.push_back(scratchPositions[shared]);
            continue;
        }
        scratchPositions.push_back(scratchTotal);
        scratchTotal += (window.tiledEnd - window.tiledBegin + 255) & ~std::uint64_t{255};
    }
    // The kept ranges inside each window, from the window's scratch (a shared tail region once)
    // to their destination: the import's unit shadow where a slab takes the piece (split at slab
    // boundaries; units a piece covers partly and does not already hold fresh are seeded from the
    // import first), else the import as before. Decided before anything is recorded: making a
    // slab may evict another with a publish of its own (a chosen slab stays pinned by its
    // destination until the pieces are marked, so no later choice evicts it).
    struct SlabPieces {
        std::shared_ptr<ShadowSlab> slab;
        std::shared_ptr<ShadowSlabPin> pin;
        std::vector<VkBufferCopy> copies;
    };
    std::vector<SlabPieces> slabPieces;
    std::vector<VkBufferCopy> importCopies;
    std::vector<ShadowedRange> seeds;
    const auto addPiece = [&](std::uint64_t scratchOffset, std::uint64_t guestBegin, std::uint64_t guestEnd) {
        while (guestBegin < guestEnd) {
            const auto pieceEnd = UnitShadowEnabled() ? std::min(guestEnd, SlabBoundary(import, guestBegin)) : guestEnd;
            const auto destination = ShadowDestinationFor(context, import, guestBegin, pieceEnd);
            if (destination.has_value()) {
                if (slabPieces.empty() || slabPieces.back().slab != destination->slab) slabPieces.push_back({destination->slab, destination->pin, {}});
                slabPieces.back().copies.push_back({scratchOffset, destination->offset, pieceEnd - guestBegin});
                for (const auto& [seedBegin, seedEnd] : destination->seedUnits) {
                    if (std::none_of(seeds.begin(), seeds.end(), [&](const ShadowedRange& seed) { return seed.begin == seedBegin; })) seeds.push_back({seedBegin, seedEnd, destination->slab});
                }
                if (!shadowed.empty() && shadowed.back().end == guestBegin && shadowed.back().slab == destination->slab) shadowed.back().end = pieceEnd;
                else shadowed.push_back({guestBegin, pieceEnd, destination->slab});
            } else {
                importCopies.push_back({scratchOffset, guestBegin - import.base, pieceEnd - guestBegin});
                if (!imported.empty() && imported.back().second == guestBegin) imported.back().second = pieceEnd;
                else imported.emplace_back(guestBegin, pieceEnd);
            }
            scratchOffset += pieceEnd - guestBegin;
            guestBegin = pieceEnd;
        }
    };
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        // The tail levels follow each other, sharing the previous one's region.
        if (i != 0 && scratchPositions[i] == scratchPositions[i - 1]) continue;
        for (const auto& [from, to] : runs) {
            const auto begin = std::max(from, window.tiledBegin);
            const auto end = std::min(to, window.tiledEnd);
            if (begin >= end) continue;
            addPiece(scratchPositions[i] + (begin - window.tiledBegin), descriptor.baseAddress + begin, descriptor.baseAddress + end);
        }
    }
    std::sort(seeds.begin(), seeds.end(), [](const ShadowedRange& a, const ShadowedRange& b) { return a.begin < b.begin; });
    std::vector<CopiedBytes> copied;
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        if (i != 0 && scratchPositions[i] == scratchPositions[i - 1]) continue;
        for (const auto& [from, to] : runs) {
            const auto begin = std::max(from, window.tiledBegin);
            const auto end = std::min(to, window.tiledEnd);
            if (begin < end) copied.push_back({begin, end, scratchPositions[i] + (begin - window.tiledBegin)});
        }
    }
    const auto padding = paddingSeeds(import, std::move(copied));
    // A queued label or key store inside a seeded unit lands before the seed copies the import.
    auto flushBegin = firstStored;
    auto flushEnd = lastStored;
    for (const auto& seed : seeds) {
        flushBegin = std::min(flushBegin, seed.begin);
        flushEnd = std::max(flushEnd, seed.end);
    }
    auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearTotal), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto tiledScratch = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(scratchTotal), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    detiler.BeginBatch();
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkAccessFlags covered = 0;
    auto timing = Recorder::NoTiming;
    if (recorder != nullptr) {
        recorder->FlushKeyStoresOverlapping(flushBegin, static_cast<std::size_t>(flushEnd - flushBegin));
        recorder->FlushStoresOverlapping(flushBegin, static_cast<std::size_t>(flushEnd - flushBegin));
        commands = recorder->Commands(&covered);
        timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageWriteBack);
        recorder->Keep(linear, linear->Size());
        recorder->Keep(tiledScratch, tiledScratch->Size());
        for (const auto& pieces : slabPieces) recorder->Keep(pieces.slab);
        for (const auto& slab : padding.slabs) recorder->Keep(slab);
        if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
        Recorder::CountBarriers(Recorder::CommandClass::StorageWriteBack, 4);
        if (Recorder::BarrierValidate()) {
            const std::pair<VkImage, bool> read{image, false};
            std::vector<std::pair<std::uint64_t, std::uint64_t>> seedReads;
            for (const auto& seed : seeds) seedReads.emplace_back(seed.begin, seed.end);
            seedReads.insert(seedReads.end(), padding.importReads.begin(), padding.importReads.end());
            recorder->NoteAccess(Recorder::CommandClass::StorageWriteBack, Recorder::Access{seedReads, imported, std::span(&read, 1), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT});
        }
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSource.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.image = image;
    toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
    if (!seeds.empty() || !padding.copies.empty()) {
        // The seeded units' import bytes (every earlier writer of them, host stores included)
        // precede the seed copies; the previous command's trailing barrier may have covered that.
        constexpr VkAccessFlags transferAccess = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        if (recorder != nullptr && (covered & transferAccess) == transferAccess && Recorder::MergeBarriers()) {
            Recorder::CountMerged(Recorder::CommandClass::StorageWriteBack);
        } else {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, transferAccess);
            Recorder::CountBarriers(Recorder::CommandClass::StorageWriteBack);
        }
        for (const auto& [begin, end, slab] : seeds) {
            // The piece's own slab (a seeded unit is one the piece covers partly).
            const VkBufferCopy seed{begin - import.base, SlabOffset(import, *slab, begin), end - begin};
            context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, import.buffer, slab->buffer, 1, &seed);
            NoteShadowSeed(begin, end);
        }
        // The padding's current bytes, under the retile that follows (the import-ready barrier
        // orders the scratch writes).
        for (const auto& [source, copies] : padding.copies) context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, source, tiledScratch->Handle(), static_cast<std::uint32_t>(copies.size()), copies.data());
    }
    std::vector<VkBufferImageCopy> regions;
    for (std::size_t i = 0; i < windows.size(); ++i) {
        for (auto region : windows[i].regions) {
            region.bufferOffset += linearPositions[i];
            regions.push_back(region);
        }
    }
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear->Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
    const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    const VkMemoryBarrier importReady{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &importReady, 1, &linearRead, 0, nullptr);
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& window = windows[i];
        auto retile = window.window;
        retile.pipeBankXor = descriptor.pipeBankXor;
        detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear->Handle(), linearPositions[i], tiledScratch->Handle(), scratchPositions[i], mips[window.level], true, window.layer, false, retile);
    }
    {
        // The retiled scratch (and a seed's slab bytes, which the scratch copies overwrite in
        // part) precede the copies into the slabs and the import.
        const VkMemoryBarrier scratchDone{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &scratchDone, 0, nullptr, 0, nullptr);
        const auto copyBuffer = context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
        for (const auto& pieces : slabPieces) copyBuffer(commands, tiledScratch->Handle(), pieces.slab->buffer, static_cast<std::uint32_t>(pieces.copies.size()), pieces.copies.data());
        if (!importCopies.empty()) copyBuffer(commands, tiledScratch->Handle(), import.buffer, static_cast<std::uint32_t>(importCopies.size()), importCopies.data());
    }
    VkImageMemoryBarrier backToGeneral = toSource;
    backToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    backToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    backToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    backToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    // Transfer writes are in the destination mask too: a following publish or detile of the slabs
    // merges its leading barrier.
    constexpr VkAccessFlags storedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
    const VkMemoryBarrier stored{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, storedAccess};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &stored, 0, nullptr, 1, &backToGeneral);
    if (batch) {
        batch->SubmitAndWait();
    } else {
        recorder->EndGpuTiming(timing, storedBytes);
        recorder->MarkCovered(storedAccess);
        // Only the pieces written into the import are stores a CPU reader must wait for; the
        // shadowed ones reach it through a publish, which notes its own.
        if (!imported.empty()) recorder->NotePendingWrites(imported);
    }
    partialWriteBacks.fetch_add(1, std::memory_order_relaxed);
    partialWriteBackBytes.fetch_add(storedBytes, std::memory_order_relaxed);
    return storedBytes;
}

std::vector<VkBufferImageCopy> StorageTexture::CopyRegions(const std::vector<bool>* layers) const {
    std::vector<VkBufferImageCopy> regions;
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        if (layers != nullptr && !(*layers)[layer]) continue;
        for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            const auto& mip = mips[level];
            VkBufferImageCopy region{};
            region.bufferOffset = layer * sliceLinearBytes + mip.linearOffset;
            region.bufferRowLength = mip.pitchBytes / BytesPerElement(descriptor.format) * BlockWidth(descriptor.format);
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
            region.imageOffset = {0, 0, geometry.CopyDepth(layer)};
            region.imageExtent = {std::max(descriptor.width >> level, 1u), std::max(descriptor.height >> level, 1u), 1u};
            regions.push_back(region);
        }
    }
    return regions;
}

bool StorageTexture::overlaps(std::uint64_t address, std::size_t bytes) const {
    return address < descriptor.baseAddress + guestBytes && descriptor.baseAddress < address + bytes;
}

bool StorageTexture::overlapsLive(std::uint64_t address, std::size_t bytes) const {
    return !released && guestBytes != 0 && overlaps(address, bytes);
}

// A fill of this image's DCC keys starts at the dccAddress and is at least one key per 256 guest
// bytes. While any live image overlaps the fill (`overlapped`), it must also stay inside the key extent
// (DccKeyCount, the pipe-aligned count, at least guestBytes / 256): the bytes past the keys may be
// memory the overlapping image owns, so a longer fill is not a key fill. The bound is inferred, not
// measured (docs/dev/TechnicalDebt.md). With nothing overlapping, the length rule alone decides, as
// it did before the bound.
bool StorageTexture::keysFillMatches(std::uint64_t address, std::size_t bytes, bool overlapped) const {
    constexpr std::uint64_t keyBytes = 256;
    if (released || descriptor.dccAddress != address || guestBytes / keyBytes == 0 || bytes < guestBytes / keyBytes) return false;
    return !overlapped || bytes <= DccKeyCount(descriptor, guestBytes);
}

void StorageTexture::MarkDirty() {
    if (descriptor.dccAddress != 0 && IsDccClear(uploadedKeys) && !IsDccClear(filledKeys)) {
        traceKeyStore("first write", descriptor, guestBytes);
        MarkDccUncompressed(context, descriptor.dccAddress, guestBytes, DccKeyCount(descriptor, guestBytes));
        uploadedKeys = DccKeys::Uncompressed;
    }
    markLayersPending(0, trackedLayers);
}

bool StorageTexture::anyLayerPending() const {
    return std::any_of(layerPending.begin(), layerPending.end(), [](bool pending) { return pending; });
}

void StorageTexture::refreshGeneration() {
    generation = *std::min_element(layerGeneration.begin(), layerGeneration.end());
}

void StorageTexture::markLayersPending(std::uint32_t first, std::uint32_t count) {
    CaptureTrace::Log("image-write image=%llx first=%u count=%u generation=%llu", static_cast<unsigned long long>(descriptor.baseAddress), first, count, static_cast<unsigned long long>(generation));
    static const bool eager = std::getenv("APS5_EAGER_WRITEBACK") != nullptr || std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    for (std::uint32_t layer = first; layer < first + count; ++layer) layerPending[layer] = true;
    // Results of this image now cover the alias's results borrowed into these units: this image
    // stores them, the alias no longer has to (its results, unchanged since the borrow, are in
    // this image's content). An alias written since keeps its own newer results pending.
    if (auto source = borrowedFrom.lock(); source != nullptr && borrowedUnits.size() == trackedLayers) {
        bool superseded = false;
        for (std::uint32_t unit = first; unit < first + count; ++unit) {
            if (!borrowedUnits[unit]) continue;
            borrowedUnits[unit] = false;
            if (source->version != borrowedVersion || !source->layerPending[unit]) continue;
            source->layerPending[unit] = false;
            unitsSuperseded.fetch_add(1, std::memory_order_relaxed);
            superseded = true;
        }
        if (superseded) source->reconcilePending();
    }
    if (eager) {
        WriteBack();
        return;
    }
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    // The active recorder installs a hook that also waits for recorded work; without one (tests),
    // pending storage results alone are flushed.
    if (Recorder::Active() == nullptr) {
        static const bool hooked = [] {
            GuestMemory::SetFlushHook(&FlushHook);
            return true;
        }();
        static_cast<void>(hooked);
    }
    ++version;
    const bool wasLent = std::exchange(lent, false);
    if (dirty) {
        if (wasLent) BumpPendingSerial();
        return;
    }
    dirty = true;
    pending.textures.push_back(this);
    BumpPendingSerial();
}

void StorageTexture::reconcilePending() {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    const bool any = anyLayerPending();
    if (any == dirty) return;
    dirty = any;
    if (any) pending.textures.push_back(this);
    else pending.textures.remove(this);
    BumpPendingSerial();
}

void StorageTexture::Flush() {
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        released = true;
    }
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        if (!dirty) return;
        dirty = false;
        pending.textures.remove(this);
        BumpPendingSerial();
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    const auto previous = std::exchange(flushReason, "cache eviction");
    writeBack(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    flushReason = previous;
}

// Whether a pending unit of the image lies inside the range (writeBack's own selection): an image
// listed by FlushPending without one stored nothing and only cost the GPU mutex.
bool StorageTexture::pendingUnitInside(std::uint64_t address, std::size_t bytes) const {
    static const bool pretest = std::getenv("APS5_NO_FLUSH_PRETEST") == nullptr;
    if (!pretest) return true;
    const bool whole = descriptor.dccAddress != 0 && uploadedKeys != DccKeys::Uncompressed;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layerPending[layer]) continue;
        if (whole) return true;
        const auto begin = layerBegin(layer);
        if (address < begin + layerBytes(layer) && begin < address + bytes) return true;
    }
    return false;
}

bool StorageTexture::FlushPending(std::uint64_t address, std::size_t bytes, const StorageTexture* except, const char* reason, PublishScope scope, bool* published) {
    if (published != nullptr) *published = false;
    // The images stay alive across the scan: their last owner may be a batch's kept list, which
    // another thread releases outside the GPU mutex once the batch completed.
    std::vector<std::shared_ptr<StorageTexture>> flush;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        const auto* exempt = refreshing;
        for (auto it = pending.textures.MayOverlap(address, bytes) ? pending.textures.begin() : pending.textures.end(); it != pending.textures.end();) {
            auto* texture = *it;
            if (texture != except && texture != exempt && texture->overlaps(address, bytes)) {
                if (!texture->pendingUnitInside(address, bytes)) {
                    pretestSkipped.fetch_add(1, std::memory_order_relaxed);
                    ++it;
                    continue;
                }
                texture->dirty = false;
                if (const auto skips = texture->hookSkips.exchange(0, std::memory_order_relaxed); skips != 0 && texture->skippedResultsInside(address, bytes)) {
                    flushedAfterSkip.fetch_add(1, std::memory_order_relaxed);
                    if (PublishReasonFor(reason) == PublishReason::Hook && texture->hookSkipSite.load(std::memory_order_relaxed) == static_cast<std::uint8_t>(GuestMemory::CurrentReadSite())) flushedAfterSkipSameSite.fetch_add(1, std::memory_order_relaxed);
                }
                if (auto alive = texture->weak_from_this().lock()) flush.push_back(std::move(alive));
                it = pending.textures.erase(it);
            } else {
                ++it;
            }
        }
        // Still pending for the adjacency rule of writeBack until each one is stored.
        for (const auto& texture : flush) pending.flushing.push_back(texture.get());
        if (!flush.empty()) BumpPendingSerial();
    }
    if (flush.empty()) {
        // Units shadowed over the range (another image's results retiled into the import's unit
        // shadow, or an earlier flush's) still reach the import as the scope asks.
        const bool units = PublishShadowsOnly(address, bytes, scope, reason);
        if (published != nullptr) *published = units;
        return false;
    }
    struct Unregister {
        const std::vector<std::shared_ptr<StorageTexture>>& flush;
        ~Unregister() {
            auto& pending = Pending();
            std::lock_guard lock(pending.mutex);
            for (const auto& texture : flush) std::erase(pending.flushing, texture.get());
            BumpPendingSerial();
        }
    } unregister{flush};
    // Debug aid: APS5_TRACE_FLUSH names what forces pending results to guest memory.
    static const bool trace = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    if (trace) {
        for (const auto& texture : flush) std::fprintf(stderr, "[flush] image 0x%llx+0x%llx for %s 0x%llx+0x%zx\n", static_cast<unsigned long long>(texture->descriptor.baseAddress), static_cast<unsigned long long>(texture->guestBytes), reason, static_cast<unsigned long long>(address), bytes);
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::exception_ptr failure;
    const auto previousReason = std::exchange(flushReason, reason);
    for (const auto& texture : flush) {
        try {
            // Only the pending layers the access overlaps are stored; the others stay pending (the
            // store re-registers the image for them).
            texture->writeBack(address, bytes);
        } catch (...) {
            if (!failure) failure = std::current_exception();
        }
    }
    flushReason = previousReason;
    if (failure) std::rethrow_exception(failure);
    // The stores above went into the unit shadows where slabs took them: the consumer reads the
    // import, so they are published under the same hold, after every store.
    if (scope != PublishScope::None) {
        const auto units = PublishShadow(address, bytes, scope, PublishReasonFor(reason));
        if (published != nullptr) *published = units != 0;
    }
    return true;
}

bool StorageTexture::PublishShadowsOnly(std::uint64_t address, std::size_t bytes, PublishScope scope, const char* reason) {
    if (scope == PublishScope::None || !AnyShadowedOverlaps(address, bytes)) return false;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    return PublishShadow(address, bytes, scope, PublishReasonFor(reason)) != 0;
}

void StorageTexture::FlushAllPending(const char* reason) {
    static_cast<void>(FlushPending(0, std::numeric_limits<std::size_t>::max(), nullptr, reason));
}

bool StorageTexture::StoreAtFlipRequested(const char* value) {
    if (value == nullptr) return false;
    if (std::strcmp(value, "1") == 0) return true;
    throw std::runtime_error(std::string("APS5_STORE_AT_FLIP=") + value + ": expected 1");
}

std::shared_ptr<StorageTexture> StorageTexture::FindPending(std::uint64_t address, std::uint64_t bytes) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    StorageTexture* containing = nullptr;
    for (auto* texture : pending.textures) {
        if (texture->descriptor.baseAddress != address || texture->guestBytes < bytes) continue;
        if (texture->guestBytes == bytes) return texture->weak_from_this().lock();
        // Containment, not equality: a descriptor of a chain's first mips (its own guestBytes are
        // shorter) is served by the chain's image; CanCopyFrom then checks the geometry.
        if (containing == nullptr) containing = texture;
    }
    return containing != nullptr ? containing->weak_from_this().lock() : nullptr;
}

bool PendingStorageOverlaps(std::uint64_t address, std::size_t bytes, const StorageTexture* except) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    if (!pending.textures.MayOverlap(address, bytes)) return false;
    for (const auto* texture : pending.textures) {
        // A free function (declared in ShaderResources.hpp): the overlap is computed from the public
        // surface description rather than the private helper.
        const auto begin = texture->Descriptor().baseAddress;
        if (texture != except && address < begin + texture->GuestBytes() && begin < address + bytes) return true;
    }
    return false;
}

bool StorageTexture::AnyPendingOverlaps(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    if (ranges.empty()) return false;
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    const auto overlapsAny = [&](const StorageTexture* texture) {
        for (const auto& [begin, end] : ranges) {
            if (end > begin && texture->overlaps(begin, static_cast<std::size_t>(end - begin))) return true;
        }
        return false;
    };
    for (const auto* texture : pending.textures) {
        if (overlapsAny(texture)) return true;
    }
    for (const auto* texture : pending.flushing) {
        if (overlapsAny(texture)) return true;
    }
    return false;
}

std::uint64_t StorageTexture::PendingSerial() {
    return pendingSerial.load(std::memory_order_acquire);
}

void StorageTexture::NoteProved() const {
    provedPresent.store(Recorder::Presents(), std::memory_order_relaxed);
}

std::vector<std::shared_ptr<StorageTexture>> StorageTexture::overlappingPending(std::uint64_t address, std::size_t bytes) {
    // FlushPending's listing; the images stay alive across the caller's scan as there.
    std::vector<std::shared_ptr<StorageTexture>> overlapping;
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    if (!pending.textures.MayOverlap(address, bytes)) return overlapping;
    const auto* exempt = refreshing;
    for (auto* texture : pending.textures) {
        if (texture == exempt || !texture->overlaps(address, bytes) || !texture->pendingUnitInside(address, bytes)) continue;
        if (auto alive = texture->weak_from_this().lock()) overlapping.push_back(std::move(alive));
    }
    return overlapping;
}

bool StorageTexture::blocksKept(std::span<const std::shared_ptr<StorageTexture>> images, std::uint64_t address, std::size_t bytes) {
    // The stamps are read outside the registry mutex (the tracker lock is never taken under it);
    // per pending layer, as the keep decision of writeBackLayers reads them.
    constexpr std::uint64_t block = 65536;
    const auto accessEnd = address + bytes;
    for (const auto& texture : images) {
        for (std::uint32_t layer = 0; layer < texture->trackedLayers; ++layer) {
            if (!texture->layerPending[layer]) continue;
            const auto begin = std::max(address, texture->layerBegin(layer));
            const auto end = std::min(accessEnd, texture->layerBegin(layer) + texture->layerBytes(layer));
            if (end <= begin) continue;
            if (texture->layerGeneration[layer] == 0) return false;
            for (auto at = begin & ~(block - 1); at < end; at += block) {
                const auto from = std::max(at, begin);
                const auto to = std::min(at + block, end);
                if (!GuestMemory::WrittenSince(from, static_cast<std::size_t>(to - from), texture->layerGeneration[layer])) return false;
            }
        }
    }
    return true;
}

void StorageTexture::ClassifyAccess(std::uint64_t address, std::size_t bytes, AccessClassification& out) {
    out = {};
    const auto overlapping = overlappingPending(address, bytes);
    if (overlapping.empty()) return;
    out.images = overlapping.size();
    const auto present = Recorder::Presents();
    for (const auto& texture : overlapping) {
        if (texture->provedPresent.load(std::memory_order_relaxed) + DeadImagePresents <= present) ++out.dead;
        else ++out.live;
    }
    if (bytes > ClassifyLimit) return;
    out.checked = true;
    GuestMemory::CollectWritesUncached(address, bytes);
    out.allCpuWritten = blocksKept(overlapping, address, bytes);
}

bool StorageTexture::AccessKeptByCpu(std::uint64_t address, std::size_t bytes, std::size_t* images, std::size_t* evicted) {
    if (evicted != nullptr) *evicted = 0;
    const auto overlapping = overlappingPending(address, bytes);
    if (images != nullptr) *images = overlapping.size();
    if (overlapping.empty() || bytes > ClassifyLimit) return false;
    // The keep decision of a store sees every CPU write to the range up to now (an uncached
    // collect, as writeBackLayers makes it: a memo hit could leave the store's own faults for the
    // next walk); so must this one.
    GuestMemory::CollectWritesUncached(address, bytes);
    // A neighbour's write-back stamps the 64 KiB block its surface shares with one of these images
    // and then advances that image's layer generation past the stamp (advanceAdjacent), both under
    // the GpuMutex; read between the two, the stamp would pass for a CPU write and the access
    // would read the image's stale bytes in the block. Under the mutex the layer state is what a
    // store there would see, and the stamps only grow.
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Hook);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    if (!blocksKept(overlapping, address, bytes)) return false;
    const auto site = static_cast<std::uint8_t>(GuestMemory::CurrentReadSite());
    std::size_t dropped = 0;
    for (const auto& texture : overlapping) {
        if (texture->evictStale(address, bytes, dropped)) continue;
        texture->hookSkips.fetch_add(1, std::memory_order_relaxed);
        texture->hookSkipSite.store(site, std::memory_order_relaxed);
        for (std::uint32_t layer = 0; layer < texture->trackedLayers; ++layer) {
            if (!texture->layerPending[layer] || address >= texture->layerBegin(layer) + texture->layerBytes(layer) || texture->layerBegin(layer) >= address + bytes) continue;
            texture->hookSkipLayer = layer;
            texture->hookSkipVersion = texture->version;
            texture->hookSkipGeneration = texture->layerGeneration[layer];
            break;
        }
    }
    if (evicted != nullptr) *evicted = dropped;
    return true;
}

bool StorageTexture::evictStale(std::uint64_t address, std::size_t bytes, std::size_t& dropped) {
    static const bool staleEvict = std::getenv("APS5_STALE_EVICT") != nullptr;
    constexpr std::uint64_t block = 65536;
    // writeBack's selection for the access: the pending layers it overlaps, every one under a
    // clear code (see there), or every one of a dead image (H3).
    const bool whole = descriptor.dccAddress != 0 && uploadedKeys != DccKeys::Uncompressed;
    const bool all = whole || (staleEvict && provedPresent.load(std::memory_order_relaxed) + DeadImagePresents <= Recorder::Presents());
    const auto accessEnd = address + bytes;
    const auto selected = [&](std::uint32_t layer) {
        return layerPending[layer] && (all || (address < layerBegin(layer) + layerBytes(layer) && layerBegin(layer) < accessEnd));
    };
    std::uint64_t first = descriptor.baseAddress + guestBytes;
    std::uint64_t last = descriptor.baseAddress;
    bool beyond = false;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!selected(layer)) continue;
        if (layerGeneration[layer] == 0) return false;
        const auto begin = layerBegin(layer);
        const auto end = begin + layerBytes(layer);
        first = std::min(first, begin);
        last = std::max(last, end);
        if (begin < (address & ~(block - 1)) || end > ((accessEnd + block - 1) & ~(block - 1))) beyond = true;
    }
    if (last <= first) return false;
    if (beyond) {
        // Blocks outside the access's own were not collected by this call: the walk of the layers'
        // span (a write-back's own) runs once per present per image.
        const auto present = Recorder::Presents();
        if (staleCheckPresent == present) return false;
        staleCheckPresent = present;
        GuestMemory::CollectWritesUncached(first, static_cast<std::size_t>(last - first));
    }
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!selected(layer)) continue;
        const auto begin = layerBegin(layer);
        const auto end = begin + layerBytes(layer);
        for (auto at = begin & ~(block - 1); at < end; at += block) {
            const auto from = std::max(at, begin);
            if (!GuestMemory::WrittenSince(from, static_cast<std::size_t>(std::min(at + block, end) - from), layerGeneration[layer])) return false;
        }
    }
    // As the empty write-back leaves the layers: no results pending, the generation unchanged (the
    // stamps make their next Refresh re-upload), `original` no longer vouching for the bytes.
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (selected(layer)) layerPending[layer] = false;
    }
    originalValid = false;
    if (!anyLayerPending()) {
        ++dropped;
        staleEvicted.fetch_add(1, std::memory_order_relaxed);
    }
    reconcilePending();
    return true;
}

bool StorageTexture::skippedResultsInside(std::uint64_t address, std::size_t bytes) const {
    const auto layer = hookSkipLayer;
    if (layer >= trackedLayers || !layerPending[layer] || version != hookSkipVersion || layerGeneration[layer] != hookSkipGeneration) return false;
    const bool whole = descriptor.dccAddress != 0 && uploadedKeys != DccKeys::Uncompressed;
    return whole || (address < layerBegin(layer) + layerBytes(layer) && layerBegin(layer) < address + bytes);
}

StorageTexture::HookSkipCounts StorageTexture::TakeHookSkipCounts() {
    return {flushedAfterSkip.exchange(0, std::memory_order_relaxed), flushedAfterSkipSameSite.exchange(0, std::memory_order_relaxed)};
}

void StorageTexture::BumpPendingSerial() {
    pendingSerial.fetch_add(1, std::memory_order_release);
}

bool StorageTexture::ScanPending(std::span<PendingQuery> queries) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    bool identities = true;
    for (auto& query : queries) {
        query.overlaps = false;
        query.found = nullptr;
        if (query.end <= query.begin) continue;
        const auto bytes = static_cast<std::size_t>(query.end - query.begin);
        for (const auto* texture : pending.textures) {
            if (query.found == nullptr && texture->descriptor.baseAddress == query.begin && texture->guestBytes >= bytes) query.found = texture;
            if (texture != query.except && texture->overlaps(query.begin, bytes)) query.overlaps = true;
        }
        for (const auto* texture : pending.flushing) {
            if (texture != query.except && texture->overlaps(query.begin, bytes)) query.overlaps = true;
        }
        if (query.pending != nullptr && query.found != query.pending) identities = false;
    }
    return identities;
}

std::size_t StorageTexture::DiscardPendingInside(std::uint64_t address, std::size_t bytes) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    std::size_t discarded = 0;
    const auto end = address + bytes;
    const auto* exempt = refreshing;
    for (auto it = pending.textures.begin(); it != pending.textures.end();) {
        auto* texture = *it;
        const auto begin = texture->descriptor.baseAddress;
        if (texture != exempt && begin >= address && begin + texture->guestBytes <= end) {
            // The content no longer matches guest memory anywhere: the next use re-uploads once the
            // fill stamped the range, never from a matching `original` (the fill may not have
            // touched the bytes a CPU-path upload copied).
            texture->dirty = false;
            texture->layerPending.assign(texture->trackedLayers, false);
            texture->originalValid = false;
            it = pending.textures.erase(it);
            ++discarded;
        } else if (texture != exempt && texture->blockUnits && texture->overlaps(address, bytes)) {
            // Its units wholly inside the range are dead too (re-uploaded from the range's bytes at
            // the next use, never from a matching `original`: the dropped units hold results the
            // bytes never received); the others stay pending.
            bool dropped = false;
            for (std::uint32_t unit = 0; unit < texture->trackedLayers; ++unit) {
                const auto unitBegin = texture->layerBegin(unit);
                if (!texture->layerPending[unit] || unitBegin < address || unitBegin + texture->layerBytes(unit) > end) continue;
                texture->layerPending[unit] = false;
                unitsDropped.fetch_add(1, std::memory_order_relaxed);
                dropped = true;
            }
            if (dropped) {
                texture->originalValid = false;
                ++discarded;
            }
            if (dropped && !texture->anyLayerPending()) {
                texture->dirty = false;
                it = pending.textures.erase(it);
                continue;
            }
            ++it;
        } else {
            ++it;
        }
    }
    if (discarded != 0) BumpPendingSerial();
    return discarded;
}

StorageTexture::FillCoverage StorageTexture::ClassifyFill(std::uint64_t address, std::size_t bytes) {
    FillCoverage coverage;
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    std::vector<StorageTexture*> overlapping;
    for (auto* texture : live.textures) {
        if (texture->overlapsLive(address, bytes)) overlapping.push_back(texture);
    }
    const auto end = address + bytes;
    StorageTexture* covered = nullptr;
    // The newest of several exact matches (the registry is in construction order): the same
    // surface under two storage formats keeps two images, and the older one may be bound by
    // nothing any more, so clearing it would only revive it for a write-back.
    for (auto* texture : overlapping) {
        if (texture->descriptor.baseAddress == address && texture->guestBytes == bytes) {
            covered = texture;
            coverage.cover = FillCover::Exact;
            coverage.layer = WholeImage;
        }
    }
    // APS5_NO_LAYER_REFUSAL_MEMO=1: match a layer of an image whose surface check failed at its
    // current generation anyway (the whole-surface collect that check costs then runs per fill).
    static const bool layerMemo = std::getenv("APS5_NO_LAYER_REFUSAL_MEMO") == nullptr;
    for (auto* texture : overlapping) {
        if (covered != nullptr) break;
        // One array layer of a thin surface: its guest bytes are one contiguous slice.
        const auto& geometry = texture->geometry;
        const auto base = texture->descriptor.baseAddress;
        if (geometry.layers < 2 || geometry.thick || geometry.imageDepth != 1 || geometry.layers != geometry.imageLayers || geometry.layerBytes != bytes || geometry.layerBytes * geometry.layers != texture->guestBytes || address < base) continue;
        if (layerMemo && texture->layerRefusedGeneration == texture->generation) continue;
        const auto offset = address - base;
        if (offset % bytes != 0 || offset / bytes >= geometry.layers) continue;
        covered = texture;
        coverage.cover = FillCover::Layer;
        coverage.layer = static_cast<std::uint32_t>(offset / bytes);
    }
    if (covered != nullptr) {
        coverage.image = covered->weak_from_this().lock();
        if (coverage.image == nullptr) {
            coverage.cover = FillCover::None;
        } else {
            for (const auto* texture : overlapping) {
                if (texture == covered) continue;
                ++coverage.others;
                if (texture->descriptor.baseAddress >= address && texture->descriptor.baseAddress + texture->guestBytes <= end) ++coverage.inside;
            }
        }
    } else if (std::any_of(live.textures.begin(), live.textures.end(), [&](const StorageTexture* texture) { return texture->keysFillMatches(address, bytes, !overlapping.empty()); })) {
        coverage.cover = FillCover::Keys;
    } else if (overlapping.size() > 1) {
        coverage.cover = FillCover::Several;
    } else if (overlapping.size() == 1) {
        auto* single = overlapping.front();
        const auto begin = single->descriptor.baseAddress;
        const auto stop = begin + single->guestBytes;
        if (address >= begin && end <= stop) coverage.cover = FillCover::Inside;
        else if (address <= begin && end >= stop) coverage.cover = FillCover::Around;
        else coverage.cover = FillCover::Straddle;
    }
    // Debug aid: APS5_TRACE_FILL_COVER=1 prints the first fills that meet images without covering
    // one of them (or a layer of one), with the images' surfaces, so a wider conversion can be
    // designed from them.
    static const bool trace = std::getenv("APS5_TRACE_FILL_COVER") != nullptr;
    static int traced = 0;
    const auto cover = coverage.cover;
    if (trace && (cover == FillCover::Inside || cover == FillCover::Around || cover == FillCover::Straddle || cover == FillCover::Several) && traced < 96) {
        ++traced;
        std::fprintf(stderr, "[fill-cover] fill 0x%llx+0x%zx meets %zu image(s):", static_cast<unsigned long long>(address), bytes, overlapping.size());
        for (const auto* texture : overlapping) {
            const auto& d = texture->descriptor;
            std::fprintf(stderr, " [0x%llx+0x%llx %ux%u mips %u layers %u dim %d tile %d format %u dcc 0x%llx%s]", static_cast<unsigned long long>(d.baseAddress), static_cast<unsigned long long>(texture->guestBytes), d.width, d.height, d.mipCount, texture->geometry.imageLayers, static_cast<int>(d.dimension), static_cast<int>(d.tileMode), d.format, static_cast<unsigned long long>(d.dccAddress), texture->dirty ? " dirty" : "");
        }
        std::fprintf(stderr, "\n");
    }
    return coverage;
}

std::size_t StorageTexture::NoteKeysFill(std::uint64_t address, std::size_t bytes, std::uint8_t key) {
    static const bool refillDisabled = std::getenv("APS5_NO_KEYS_REFILL") != nullptr;
    DccKeys keys = DccKeys::Mixed;
    switch (key) {
        case 0x00: keys = DccKeys::Clear0000; break;
        case 0x40: keys = DccKeys::Clear0001; break;
        case 0x80: keys = DccKeys::Clear1110; break;
        case 0xc0: keys = DccKeys::Clear1111; break;
        case 0x20: keys = DccKeys::ClearRegister; break;
        case 0xff: keys = DccKeys::Uncompressed; break;
        default: return 0;
    }
    static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    const bool overlapped = std::any_of(live.textures.begin(), live.textures.end(), [&](const StorageTexture* texture) { return texture->overlapsLive(address, bytes); });
    std::size_t covered = 0;
    for (auto* texture : live.textures) {
        if (!texture->keysFillMatches(address, bytes, overlapped)) continue;
        texture->filledKeys = keys;
        ++covered;
        if (traceKeys && IsDccClear(keys)) std::fprintf(stderr, "[dcc-keys] %s key fill over 0x%llx+0x%llx (uploaded %s, dirty %d)\n", DccKeysName(keys), static_cast<unsigned long long>(texture->descriptor.baseAddress), static_cast<unsigned long long>(texture->guestBytes), DccKeysName(texture->uploadedKeys), texture->dirty ? 1 : 0);
        if (refillDisabled || !IsDccClear(keys) || texture->uploadedKeys != keys) continue;
        texture->uploadedKeys = DccKeys::Uncompressed;
    }
    return covered;
}

std::size_t StorageTexture::ClearByKeysFill(std::uint64_t address, std::size_t bytes, std::uint8_t key) {
    static const bool enabled = [] { const char* text = std::getenv("APS5_KEYS_FILL_CLEAR"); return text != nullptr && std::strcmp(text, "1") == 0; }();
    if (!enabled) return 0;
    DccKeys keys = DccKeys::Mixed;
    switch (key) {
        case 0x00: keys = DccKeys::Clear0000; break;
        case 0x40: keys = DccKeys::Clear0001; break;
        case 0x80: keys = DccKeys::Clear1110; break;
        case 0xc0: keys = DccKeys::Clear1111; break;
        case 0x20: keys = DccKeys::ClearRegister; break;
        default: return 0;
    }
    std::vector<std::shared_ptr<StorageTexture>> covered;
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        const bool overlapped = std::any_of(live.textures.begin(), live.textures.end(), [&](const StorageTexture* texture) { return texture->overlapsLive(address, bytes); });
        for (auto* texture : live.textures) {
            if (!texture->keysFillMatches(address, bytes, overlapped)) continue;
            if (auto shared = texture->weak_from_this().lock()) covered.push_back(std::move(shared));
        }
    }
    std::size_t cleared = 0;
    for (const auto& texture : covered) {
        if (texture->clearByKeysFill(keys, key)) ++cleared;
    }
    return cleared;
}

bool StorageTexture::clearByKeysFill(DccKeys keys, std::uint8_t key) {
    static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    VkClearColorValue clearValue{};
    auto* recorder = Recorder::Active();
    const char* refusal = !ClearColorFor(storageFormat, keys, clearValue) ? (keys == DccKeys::ClearRegister ? "clear-register code" : "no clear value in the storage format") : recorder == nullptr ? "no recorder" : nullptr;
    if (refusal != nullptr) {
        static std::atomic<bool> reported{false};
        if (!reported.exchange(true)) std::fprintf(stderr, "[dcc-keys] key fill 0x%02x (%s) over surface 0x%llx+0x%llx (guest format %u, vk format %d, keys 0x%llx) not applied at once: %s\n", key, DccKeysName(keys), static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), descriptor.format, static_cast<int>(storageFormat), static_cast<unsigned long long>(descriptor.dccAddress), refusal);
        return false;
    }
    std::size_t droppedUnits = 0;
    const bool wasDirty = dirty;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        droppedUnits = static_cast<std::size_t>(std::count(layerPending.begin(), layerPending.end(), true));
        layerPending.assign(trackedLayers, false);
        if (dirty) {
            dirty = false;
            pending.textures.remove(this);
            BumpPendingSerial();
        }
    }
    const auto commands = recorder->Commands();
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::DccClear);
    if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
    Recorder::CountBarriers(Recorder::CommandClass::DccClear, 2);
    if (Recorder::BarrierValidate()) {
        const std::pair<VkImage, bool> clearedImage{image, true};
        recorder->NoteAccess(Recorder::CommandClass::DccClear, Recorder::Access{{}, {}, std::span(&clearedImage, 1), VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    VkImageMemoryBarrier toClear{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toClear.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.image = image;
    toClear.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toClear);
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &toClear.subresourceRange);
    VkImageMemoryBarrier toGeneral = toClear;
    toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    recorder->EndGpuTiming(timing, guestBytes);
    originalValid = false;
    uploadedKeys = keys;
    filledKeys = DccKeys::Uncompressed;
    keyProof = {};
    forgetBorrowed(0, trackedLayers);
    layerGeneration.assign(trackedLayers, GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)));
    refreshGeneration();
    ++version;
    if (traceKeys) std::fprintf(stderr, "[dcc-keys] key fill %s clears 0x%llx+0x%llx at once (keys 0x%llx, %zu pending units dropped, dirty %d)\n", DccKeysName(keys), static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), static_cast<unsigned long long>(descriptor.dccAddress), droppedUnits, wasDirty ? 1 : 0);
    return true;
}

bool StorageTexture::FillClear(std::span<const std::uint32_t, 4> pattern, std::uint32_t layer, const char*& refusal) {
    // Debug aid (APS5_TRACE_FILL_COVER=1): the first layer covers refused, with the surface, the
    // pattern and the generation the refusal was memoized at (ClassifyFill's layerRefusedGeneration).
    static const bool trace = std::getenv("APS5_TRACE_FILL_COVER") != nullptr;
    struct TraceLayerRefusal {
        const StorageTexture& texture;
        std::span<const std::uint32_t, 4> pattern;
        std::uint32_t layer;
        const char*& refusal;
        bool cleared = false;
        ~TraceLayerRefusal() {
            static std::atomic<int> traced{0};
            if (cleared || !trace || layer == WholeImage || refusal == nullptr || traced.fetch_add(1) >= 64) return;
            const auto& d = texture.descriptor;
            std::fprintf(stderr, "[fill-cover] layer %u of 0x%llx+0x%llx (%ux%u mips %u layers %u, %llu bytes per layer, format %u tile %d dcc 0x%llx%s) refused: %s; pattern %08x %08x %08x %08x; generation %llu\n", layer, static_cast<unsigned long long>(d.baseAddress), static_cast<unsigned long long>(texture.guestBytes), d.width, d.height, d.mipCount, texture.geometry.imageLayers, static_cast<unsigned long long>(texture.geometry.layerBytes), d.format, static_cast<int>(d.tileMode), static_cast<unsigned long long>(d.dccAddress), texture.dirty ? ", dirty" : "", refusal, pattern[0], pattern[1], pattern[2], pattern[3], static_cast<unsigned long long>(texture.generation));
        }
    } traceRefusal{*this, pattern, layer, refusal};
    VkClearColorValue clearValue{};
    const auto elementBytes = BytesPerElement(descriptor.format);
    if (!FillClearColor(storageFormat, elementBytes, pattern, clearValue) && !(ClearKeepsDenormals(context, storageFormat) && FillClearColor(storageFormat, elementBytes, pattern, clearValue, true))) {
        refusal = "pattern";
        return false;
    }
    if (layer != WholeImage && (layer >= geometry.imageLayers || geometry.thick || geometry.imageDepth != 1)) {
        refusal = "layer";
        return false;
    }
    if (descriptor.dccAddress != 0) {
        // Under a clear code reads see that value whatever the texels hold, and keys a recorded
        // kernel still writes are not in the bytes yet: only keys that read as uncompressed now let
        // the image stand for the texels.
        if (Recorder::SnapshotWriteOverlaps(descriptor.dccAddress, static_cast<std::size_t>(guestBytes / 256))) {
            refusal = "keys pending";
            return false;
        }
        if (TextureClearKeys(descriptor, guestBytes) != DccKeys::Uncompressed) {
            refusal = "keys";
            return false;
        }
    }
    if (HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) == nullptr) {
        refusal = "not imported";
        return false;
    }
    auto* recorder = Recorder::Active();
    if (recorder == nullptr) {
        refusal = "no recorder";
        return false;
    }
    const auto begin = layer == WholeImage ? descriptor.baseAddress : descriptor.baseAddress + geometry.GuestLayerOffset(layer);
    const auto bytes = static_cast<std::size_t>(layer == WholeImage ? guestBytes : geometry.layerBytes);
    // A tracked layer answers for itself (its own generation and pending flag): the rest of the
    // surface may have changed under the image, which its own layers' next use uploads.
    if (layer != WholeImage && trackedLayers == 1) {
        // The rest of the surface keeps the image's content, which must still be what guest memory
        // holds: the one generation the write-back keeps CPU-written blocks after moves to now. A
        // surface that changed under the image (another image's results stored into its memory, a
        // fill of another layer stored as bytes) is not refreshed here: the images so filled at the
        // movie stage are stale ones of an earlier use of the memory, and reviving one every frame
        // (a 60 MiB write-back and re-upload inside the fill) cost what the conversion gained.
        // Uncached, as writeBack's keep decision walks: a memo hit of this submission's epoch
        // would leave pages the game dirtied since that walk unconsumed, and the check below
        // would pass over them.
        GuestMemory::CollectWritesUncached(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
        const auto before = static_cast<std::size_t>(begin - descriptor.baseAddress);
        const auto after = static_cast<std::size_t>(descriptor.baseAddress + guestBytes - (begin + bytes));
        if ((before != 0 && !GuestMemory::UnchangedSince(descriptor.baseAddress, before, generation)) || (after != 0 && !GuestMemory::UnchangedSince(begin + bytes, after, generation))) {
            refusal = "surface changed";
            layerRefusedGeneration = generation;
            return false;
        }
    }
    if (layer != WholeImage && blockUnits && ((begin - descriptor.baseAddress) % trackedLayerBytes != 0 || bytes % trackedLayerBytes != 0)) {
        refusal = "layer alignment";
        return false;
    }
    // Other images over the range hold content the fill supersedes: stamped below, they re-upload
    // at their next use (behind this image's write-back), and never from a matching `original`,
    // since the fill leaves the bytes a CPU-path upload copied as they are.
    bool aliasedKeys = false;
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        for (auto* texture : live.textures) {
            if (texture == this || texture->released || !texture->overlaps(begin, bytes)) continue;
            texture->originalValid = false;
            if (texture->descriptor.dccAddress != 0) aliasedKeys = true;
        }
    }
    // Debug aid (APS5_TRACE_FILL_COVER=1): the first clears of a surface with DCC keys (its own or
    // another image's over the same memory), with the pattern: a title that fills the keys to a
    // clear code right after makes the clear moot for that image (its next lookup re-uploads
    // under the code) unless the pattern is that code's.
    static std::atomic<int> traced{0};
    if (trace && (descriptor.dccAddress != 0 || aliasedKeys) && traced.fetch_add(1) < 16) std::fprintf(stderr, "[fill-cover] clear of surface 0x%llx+0x%llx (format %u, keys 0x%llx%s): pattern %08x %08x %08x %08x\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), descriptor.format, static_cast<unsigned long long>(descriptor.dccAddress), aliasedKeys ? ", another image over the memory has keys" : "", pattern[0], pattern[1], pattern[2], pattern[3]);
    // A neighbour's results shadowed in the range's edge units reach the import before the stamp
    // below makes those units stale (nothing for a 64 KiB-multiple surface).
    PublishShadow(begin, bytes, PublishScope::PartialUnits, PublishReason::FillClear);
    GuestMemory::MarkWritten(begin, bytes);
    const auto commands = recorder->Commands();
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::FillClear);
    Recorder::CountBarriers(Recorder::CommandClass::FillClear, 2);
    if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
    if (Recorder::BarrierValidate()) {
        const std::pair<VkImage, bool> cleared{image, true};
        recorder->NoteAccess(Recorder::CommandClass::FillClear, Recorder::Access{{}, {}, std::span(&cleared, 1), VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    VkImageMemoryBarrier toClear{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toClear.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    // A whole image is discarded into the clear; one layer keeps the others' content.
    toClear.oldLayout = layer == WholeImage ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.image = image;
    toClear.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, layer == WholeImage ? 0u : layer, layer == WholeImage ? geometry.imageLayers : 1u};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toClear);
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &toClear.subresourceRange);
    VkImageMemoryBarrier toGeneral = toClear;
    toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    recorder->EndGpuTiming(timing, bytes);
    traceRefusal.cleared = true;
    // The image holds the fill and guest memory what it held: the texels reach it at the
    // write-back, which keeps the 64 KiB blocks the CPU writes from now on (a CPU write before the
    // fill is overwritten by it in program order; the stamps above are below this generation), and
    // the keys were read uncompressed. The collect is uncached (see writeBack's keep decision): a
    // memo hit of this submission's epoch would not consume pages the game dirtied since that
    // walk, the write-back's own walk would stamp them above this generation and keep their
    // blocks, and the fill's pattern would be lost in them.
    originalValid = false;
    uploadedKeys = DccKeys::Uncompressed;
    if (layer != WholeImage && trackedLayers > 1) {
        // The cleared layer's units alone: their stamps above lie below this generation, the other
        // units keep theirs (their pages are consumed by their own collects).
        const auto first = blockUnits ? static_cast<std::uint32_t>((begin - descriptor.baseAddress) / trackedLayerBytes) : layer;
        const auto count = blockUnits ? static_cast<std::uint32_t>(bytes / trackedLayerBytes) : 1u;
        const auto now = GuestMemory::CollectWritesUncached(begin, bytes);
        for (auto unit = first; unit < first + count; ++unit) layerGeneration[unit] = now;
        refreshGeneration();
        forgetBorrowed(first, count);
        markLayersPending(first, count);
        return true;
    }
    layerGeneration.assign(trackedLayers, GuestMemory::CollectWritesUncached(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)));
    refreshGeneration();
    forgetBorrowed(0, trackedLayers);
    MarkDirty();
    return true;
}

void StorageTexture::WriteBack() {
    const auto previous = std::exchange(flushReason, "explicit");
    writeBack(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    flushReason = previous;
}

std::shared_ptr<StorageTexture> StorageTexture::FindLive(std::uint64_t address, std::uint64_t bytes) {
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    for (auto it = live.textures.rbegin(); it != live.textures.rend(); ++it) {
        auto* texture = *it;
        if (texture->released || !texture->Cached() || texture->descriptor.baseAddress != address || texture->guestBytes != bytes) continue;
        return texture->weak_from_this().lock();
    }
    return nullptr;
}

bool StorageTexture::SameSurfaceShape(const StorageTexture& other) const {
    const auto& mine = descriptor;
    const auto& theirs = other.descriptor;
    return guestBytes == other.guestBytes && mine.width == theirs.width && mine.height == theirs.height && mine.depthOrLastArray == theirs.depthOrLastArray && mine.baseArray == theirs.baseArray && mine.mipCount == theirs.mipCount && mine.tileMode == theirs.tileMode && mine.dimension == theirs.dimension && mine.format == theirs.format && storageFormat == other.storageFormat && geometry.imageLayers == other.geometry.imageLayers && geometry.imageDepth == other.geometry.imageDepth;
}

bool StorageTexture::CopyFrom(StorageTexture& source, const char*& refusal) {
    if (&source == this || !SameSurfaceShape(source)) {
        refusal = "shape";
        return false;
    }
    if (descriptor.dccAddress != 0 || source.descriptor.dccAddress != 0) {
        refusal = "keys";
        return false;
    }
    if (released || source.released || !Cached() || !source.Cached()) {
        refusal = "released";
        return false;
    }
    if (HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) == nullptr) {
        refusal = "not imported";
        return false;
    }
    auto* recorder = Recorder::Active();
    if (recorder == nullptr) {
        refusal = "no recorder";
        return false;
    }
    // The source image must hold the surface: a stale one uploads first (and other images'
    // results pending over its memory land first, as the transfer's flush would have them).
    source.Refresh();
    // Results pending over the destination range from before the copy are dead inside it and land
    // first around it; other images over the range hold content the copy supersedes (they
    // re-upload at their next use, behind this image's write-back).
    static_cast<void>(DiscardPendingInside(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)));
    FlushPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), this, "buffer copy destination", PublishScope::PartialUnits);
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        for (auto* texture : live.textures) {
            if (texture != this && !texture->released && texture->overlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) texture->originalValid = false;
        }
    }
    GuestMemory::MarkWritten(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    const auto commands = recorder->Commands();
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
    Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
    if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
    if (auto other = source.weak_from_this().lock()) recorder->Keep(std::move(other));
    if (Recorder::BarrierValidate()) {
        const std::array<std::pair<VkImage, bool>, 2> images{{{source.image, false}, {image, true}}};
        recorder->NoteAccess(Recorder::CommandClass::Copy, Recorder::Access{{}, {}, images, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    VkImageMemoryBarrier barriers[2]{};
    for (auto& barrier : barriers) {
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
    }
    barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].image = source.image;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].image = image;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);
    std::vector<VkImageCopy> regions;
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            VkImageCopy region{};
            region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
            region.srcOffset = {0, 0, geometry.CopyDepth(layer)};
            region.dstSubresource = region.srcSubresource;
            region.dstOffset = region.srcOffset;
            region.extent = {std::max(descriptor.width >> level, 1u), std::max(descriptor.height >> level, 1u), 1u};
            regions.push_back(region);
        }
    }
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source.image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, static_cast<std::uint32_t>(regions.size()), regions.data());
    VkImageMemoryBarrier copied = barriers[1];
    copied.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copied.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &copied);
    recorder->EndGpuTiming(timing, guestBytes);
    // As after a clear: the image holds the surface, guest memory follows at the write-back (which
    // keeps the 64 KiB blocks the CPU writes from now on; the collect is uncached for the reason
    // given in FillClear).
    originalValid = false;
    uploadedKeys = DccKeys::Uncompressed;
    layerGeneration.assign(trackedLayers, GuestMemory::CollectWritesUncached(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)));
    refreshGeneration();
    forgetBorrowed(0, trackedLayers);
    MarkDirty();
    return true;
}

// See AdjacentGenerationEnabled above.
std::vector<StorageTexture::Adjacent> StorageTexture::adjacentPendingUnchanged(std::uint64_t firstBlock, std::uint64_t lastBlock) const {
    std::vector<Adjacent> adjacent;
    if (!AdjacentGenerationEnabled() || guestBytes == 0) return adjacent;
    constexpr std::uint64_t block = 65536;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        const auto consider = [&](StorageTexture* texture) {
            if (texture == this || texture->guestBytes == 0 || texture->overlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) return;
            const auto begin = texture->descriptor.baseAddress;
            const auto neighbourFirst = begin / block;
            const auto neighbourLast = (begin + texture->guestBytes - 1) / block;
            if (neighbourFirst > lastBlock || neighbourLast < firstBlock) return;
            // Surfaces that do not overlap share at most one block: the neighbour's tracked layer
            // holding it is what the stamps touch.
            const auto shared = std::max(neighbourFirst, firstBlock);
            const auto layer = static_cast<std::uint32_t>((std::max(shared * block, begin) - begin) / texture->trackedLayerBytes);
            if (auto alive = texture->weak_from_this().lock()) adjacent.push_back({std::move(alive), layer, texture->layerGeneration[layer]});
        };
        for (auto* texture : pending.textures) consider(texture);
        for (auto* texture : pending.flushing) consider(texture);
    }
    // Only a layer whose memory has not changed since its generation can be advanced: the collect
    // stamps the CPU's writes so far, and a later one lands at a newer generation either way.
    std::erase_if(adjacent, [](const Adjacent& entry) {
        const auto& texture = entry.texture;
        const auto begin = texture->layerBegin(entry.layer);
        const auto bytes = static_cast<std::size_t>(texture->layerBytes(entry.layer));
        GuestMemory::CollectWritesUncached(begin, bytes);
        return !GuestMemory::UnchangedSince(begin, bytes, entry.generation);
    });
    return adjacent;
}

void StorageTexture::advanceAdjacent(const std::vector<Adjacent>& adjacent, std::uint64_t now, std::uint64_t firstBlock, std::uint64_t lastBlock) {
    static const bool trace = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    constexpr std::uint64_t block = 65536;
    for (const auto& [texture, layer, seen] : adjacent) {
        // A layer stored or refreshed meanwhile set its own generation (an old one on purpose when
        // it kept blocks for the CPU): only the value seen at the check is advanced.
        if (texture->layerGeneration[layer] != seen || now <= seen) continue;
        // A CPU write to the neighbour landing since the check may have been stamped by another
        // thread's walk of its range at a value in (seen, now]; the advance would hide it. Its blocks
        // outside this write-back's span carry no stamp of ours, so they are re-checked against the
        // seen generation (a stamp scan, no walk). A write inside the shared block itself in that
        // window is indistinguishable from this write-back's stamp, as it is for the block rule.
        const auto begin = texture->layerBegin(layer);
        const auto end = begin + texture->layerBytes(layer);
        const auto beforeEnd = std::min(end, firstBlock * block);
        const auto afterBegin = std::max(begin, (lastBlock + 1) * block);
        if (begin < beforeEnd && !GuestMemory::UnchangedSince(begin, static_cast<std::size_t>(beforeEnd - begin), seen)) continue;
        if (afterBegin < end && !GuestMemory::UnchangedSince(afterBegin, static_cast<std::size_t>(end - afterBegin), seen)) continue;
        if (trace) std::fprintf(stderr, "[flush] adjacent pending image 0x%llx+0x%llx layer %u advanced past a write-back's stamps (generation %llu -> %llu)\n", static_cast<unsigned long long>(texture->descriptor.baseAddress), static_cast<unsigned long long>(texture->guestBytes), layer, static_cast<unsigned long long>(seen), static_cast<unsigned long long>(now));
        texture->layerGeneration[layer] = now;
        texture->refreshGeneration();
    }
}

void StorageTexture::blockGenerations(std::vector<std::uint64_t>& generations) const {
    constexpr std::uint64_t block = 65536;
    const auto spanBegin = descriptor.baseAddress & ~(block - 1);
    generations.assign(static_cast<std::size_t>((descriptor.baseAddress + guestBytes - spanBegin + block - 1) / block), 0);
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        const auto end = layerBegin(layer) + layerBytes(layer);
        for (auto at = layerBegin(layer) & ~(block - 1); at < end; at += block) generations[static_cast<std::size_t>((at - spanBegin) / block)] = layerGeneration[layer];
    }
}

std::shared_ptr<StorageTexture> StorageTexture::pendingAlias() const {
    static const bool disabled = std::getenv("APS5_NO_ALIAS_BORROW") != nullptr;
    if (disabled || !blockUnits) return nullptr;
    std::vector<std::shared_ptr<StorageTexture>> candidates;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        for (auto* texture : pending.textures) {
            if (texture == this || texture->descriptor.baseAddress != descriptor.baseAddress || texture->guestBytes != guestBytes || !texture->blockUnits) continue;
            if (auto alive = texture->weak_from_this().lock()) candidates.push_back(std::move(alive));
        }
    }
    for (const auto& candidate : candidates) {
        const auto& mine = descriptor;
        const auto& theirs = candidate->descriptor;
        // The same texels under another format of the same size: the device copy moves them bit
        // for bit, as the guest bytes would.
        const bool sameShape = mine.width == theirs.width && mine.height == theirs.height && mine.depthOrLastArray == theirs.depthOrLastArray && mine.baseArray == theirs.baseArray && mine.mipCount == theirs.mipCount && mine.tileMode == theirs.tileMode && mine.dimension == theirs.dimension && BytesPerElement(mine.format) == BytesPerElement(theirs.format) && geometry.imageLayers == candidate->geometry.imageLayers && geometry.imageDepth == candidate->geometry.imageDepth && trackedLayers == candidate->trackedLayers;
        if (sameShape && candidate->Cached() && candidate->uploadedKeys == DccKeys::Uncompressed) return candidate;
    }
    return nullptr;
}

void StorageTexture::forgetBorrowed(std::uint32_t first, std::uint32_t count) {
    if (borrowedUnits.empty()) return;
    for (auto unit = first; unit < first + count && unit < borrowedUnits.size(); ++unit) borrowedUnits[unit] = false;
    if (std::none_of(borrowedUnits.begin(), borrowedUnits.end(), [](bool held) { return held; })) {
        borrowedUnits.clear();
        borrowedFrom.reset();
    }
}

std::uint64_t StorageTexture::borrowUnits(StorageTexture& source, const std::vector<bool>& units) {
    const auto runs = unitRuns(units);
    const auto windows = sliceWindows(runs);
    Require(!windows.empty(), "storage image borrows no unit");
    std::vector<VkImageCopy> regions;
    for (const auto& window : windows) {
        for (const auto& region : window.regions) regions.push_back({region.imageSubresource, region.imageOffset, region.imageSubresource, region.imageOffset, region.imageExtent});
    }
    std::uint64_t bytes = 0;
    for (const auto& [begin, end] : runs) bytes += end - begin;
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    auto timing = Recorder::NoTiming;
    if (recorder != nullptr) {
        commands = recorder->Commands();
        timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageUpload);
        if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
        if (auto other = source.weak_from_this().lock()) recorder->Keep(std::move(other));
        Recorder::CountBarriers(Recorder::CommandClass::StorageUpload, 2);
        if (Recorder::BarrierValidate()) {
            const std::array<std::pair<VkImage, bool>, 2> images{{{source.image, false}, {image, true}}};
            recorder->NoteAccess(Recorder::CommandClass::StorageUpload, Recorder::Access{{}, {}, images, VK_PIPELINE_STAGE_TRANSFER_BIT});
        }
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    VkImageMemoryBarrier barriers[2]{};
    for (auto& barrier : barriers) {
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
    }
    barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].image = source.image;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].image = image;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source.image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, static_cast<std::uint32_t>(regions.size()), regions.data());
    VkImageMemoryBarrier copied = barriers[1];
    copied.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copied.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &copied);
    if (batch) batch->SubmitAndWait();
    else recorder->EndGpuTiming(timing, bytes);
    countStorageUpload(3, bytes);
    if (borrowedFrom.lock() != source.weak_from_this().lock() || borrowedUnits.size() != trackedLayers) {
        borrowedUnits.assign(trackedLayers, false);
        borrowedFrom = source.weak_from_this();
    }
    borrowedVersion = source.version;
    source.lent = true;
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (units[unit]) borrowedUnits[unit] = true;
    }
    ++version;
    return bytes;
}

void StorageTexture::writeBack(std::uint64_t address, std::size_t bytes) {
    // Block units are stored per access; an image whose units keep being asked for in pieces (a
    // consumer touching it through many small ranges, each piece re-arming the pending memos of
    // the next dispatch) stores every pending unit once more than `pieces` partial stores fall
    // within `window` presents, and keeps doing so for another window. Off by default: the title's
    // aliased images are stored in pieces by design (every piece is another image's hand-over) and
    // widening them stored whole images again (movie stage: 39 vs 35 ms of GPU per present, 188 vs
    // 214 presents per 10 s); APS5_BLOCK_WRITEBACK_WIDEN=1 enables the widening, and
    // APS5_BLOCK_WRITEBACK_EACH=1 stores the touched units only, every time.
    static const bool each = std::getenv("APS5_BLOCK_WRITEBACK_EACH") != nullptr || std::getenv("APS5_BLOCK_WRITEBACK_WIDEN") == nullptr;
    static const std::uint32_t pieces = [] { const char* text = std::getenv("APS5_BLOCK_WRITEBACK_PIECES"); return text != nullptr ? static_cast<std::uint32_t>(std::strtoul(text, nullptr, 10)) : 4u; }();
    static const std::uint64_t window = [] { const char* text = std::getenv("APS5_BLOCK_WRITEBACK_FRAMES"); return text != nullptr ? std::strtoull(text, nullptr, 10) : 8ull; }();
    std::vector<bool> layers(trackedLayers, false);
    std::size_t selected = 0, pendingCount = 0;
    // Under a clear code the texels outside the stored units would read as the clear while the
    // keys, marked uncompressed for the whole surface, say texels: such an image stores whole.
    const bool whole = descriptor.dccAddress != 0 && uploadedKeys != DccKeys::Uncompressed;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layerPending[layer]) continue;
        ++pendingCount;
        layers[layer] = whole || (address < layerBegin(layer) + layerBytes(layer) && layerBegin(layer) < address + bytes);
        if (layers[layer]) ++selected;
    }
    if (selected == 0) {
        reconcilePending();
        return;
    }
    if (blockUnits && !each && selected < pendingCount) {
        const auto present = Recorder::Presents();
        if (present - partialWindowStart >= window) {
            partialWindowStart = present;
            partialStores = 0;
        }
        if (present < storeWholeUntil || ++partialStores > pieces) {
            storeWholeUntil = present + window;
            layers = layerPending;
            coalescedWriteBacks.fetch_add(1, std::memory_order_relaxed);
        }
    }
    writeBackLayers(layers);
}

bool StorageTexture::unchangedSinceBaseline(std::uint64_t from, std::uint64_t to) const {
    const auto& baseline = originalValid ? original : generationBaseline;
    if (baseline.size() != guestBytes) return false;
    const auto offset = static_cast<std::size_t>(from - descriptor.baseAddress);
    return GuestMemory::EqualsCommittedUnsynced(from, std::span<const std::byte>(baseline).subspan(offset, static_cast<std::size_t>(to - from)));
}

void StorageTexture::writeBackLayers(const std::vector<bool>& layers) {
    CaptureTrace::Log("writeback image=%llx generation=%llu reason=%s units=%zu", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(generation), flushReason, static_cast<std::size_t>(std::count(layers.begin(), layers.end(), true)));
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    struct Account {
        bool enabled;
        PhaseTimer& timer;
        ~Account() { if (enabled) Profile().storageWriteBack += timer.lap(); }
    } account{profile, timer};
    // Whatever the outcome, the registration follows the pending flags (a throw leaves the
    // layers pending).
    struct Reconcile {
        StorageTexture& texture;
        ~Reconcile() { texture.reconcilePending(); }
    } reconcile{*this};
    const auto elementBytes = BytesPerElement(descriptor.format);
    // 64 KiB blocks the CPU wrote since the layer was last in sync keep the CPU's bytes: the game
    // may have reused the memory for something else entirely (see layerGeneration). A generation of
    // zero means no tracking, and the layer is stored whole.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> keep;
    std::vector<bool> skippedLayer(trackedLayers, false);
    bool skippedAny = false;
    std::size_t skipped = 0;
    std::uint64_t firstStored = descriptor.baseAddress + guestBytes;
    std::uint64_t lastStored = descriptor.baseAddress;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layers[layer]) continue;
        firstStored = std::min(firstStored, layerBegin(layer));
        lastStored = std::max(lastStored, layerBegin(layer) + layerBytes(layer));
    }
    if (lastStored <= firstStored) return;
    constexpr std::uint64_t block = 65536;
    const auto spanBegin = firstStored & ~(block - 1);
    const auto spanBlocks = static_cast<std::size_t>((lastStored - spanBegin + block - 1) / block);
    // The keep-blocks decision must see every CPU write to the stored span up to now, so it
    // bypasses the per-packet collect memo (a Refresh collect earlier in the same packet would
    // satisfy it); the unselected layers keep their own generations and are collected by their
    // own refreshes. One tracker lock then reads the span's stamps against the layers' generations.
    GuestMemory::CollectWritesUncached(firstStored, static_cast<std::size_t>(lastStored - firstStored));
    std::vector<std::uint64_t> generations(spanBlocks, 0);
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layers[layer]) continue;
        const auto end = layerBegin(layer) + layerBytes(layer);
        for (auto at = layerBegin(layer) & ~(block - 1); at < end; at += block) generations[static_cast<std::size_t>((at - spanBegin) / block)] = layerGeneration[layer];
    }
    std::vector<std::uint8_t> changedBlocks(spanBlocks);
    const bool tracked = GuestMemory::ChangedBlocks(firstStored, static_cast<std::size_t>(lastStored - firstStored), generations, changedBlocks);
    const bool compared = !tracked && compareUntracked(firstStored, static_cast<std::size_t>(lastStored - firstStored), changedBlocks);
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layers[layer]) continue;
        const auto begin = layerBegin(layer);
        const auto end = begin + layerBytes(layer);
        if (!compared && (!tracked || layerGeneration[layer] == 0)) {
            keep.emplace_back(begin, end);
            continue;
        }
        for (auto at = begin & ~(block - 1); at < end; at += block) {
            const auto from = std::max(at, begin);
            const auto to = std::min(at + block, end);
            if (from >= to) continue;
            const bool edge = from != at || to != at + block;
            if (changedBlocks[static_cast<std::size_t>((at - spanBegin) / block)] == GuestMemory::BlockWritten && (compared || !edge || GuestMemory::StoredOver(from, static_cast<std::size_t>(to - from), layerGeneration[layer])) && !unchangedSinceBaseline(from, to)) {
                skippedAny = true;
                skippedLayer[layer] = true;
                ++skipped;
                continue;
            }
            if (!keep.empty() && keep.back().second == from) keep.back().second = to;
            else keep.emplace_back(from, to);
        }
    }
    // Debug aid: APS5_TRACE_FLUSH also names the blocks a write-back leaves to the CPU.
    static const bool traceKept = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    if (traceKept && skippedAny) {
        const auto first = keep.empty() ? descriptor.baseAddress + guestBytes : keep.front().first;
        std::fprintf(stderr, "[flush] image 0x%llx+0x%llx keeps %zu CPU-written 64 KiB blocks (first stored byte at +0x%llx, %zu ranges stored, generation %llu)\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), skipped, static_cast<unsigned long long>(first - descriptor.baseAddress), keep.size(), static_cast<unsigned long long>(generation));
    }
    // Taken before this write-back stamps anything (see advanceAdjacent), with the 64 KiB block span
    // its stamps cover.
    const auto firstBlock = firstStored / block;
    const auto lastBlock = (lastStored - 1) / block;
    const auto adjacent = adjacentPendingUnchanged(firstBlock, lastBlock);
    // After the store: the stored layers are in sync at a fresh generation unless blocks were kept
    // for the CPU (then the layer is stale there, keeps its old generation and re-uploads at its
    // next use); their results are no longer pending either way. The collect covers the stored
    // span, which holds every stored layer's pages. Pieces retiled into the import's unit shadow
    // (`shadowed`, with its import) are fresh at the generation taken after this write-back's
    // own stamps, so every later import writer stamps newer.
    std::vector<ShadowedRange> shadowed;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> imported;
    const HostImport* shadowImport = nullptr;
    const auto settle = [&](bool memoizedCollect) {
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (layers[layer]) layerPending[layer] = false;
        }
        const bool allKept = std::none_of(skippedLayer.begin(), skippedLayer.end(), [](bool kept) { return kept; });
        if (allKept || !adjacent.empty()) {
            const auto now = memoizedCollect ? GuestMemory::CollectWrites(firstStored, static_cast<std::size_t>(lastStored - firstStored)) : GuestMemory::CollectWritesUncached(firstStored, static_cast<std::size_t>(lastStored - firstStored));
            for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
                if (layers[layer] && !skippedLayer[layer]) layerGeneration[layer] = now;
            }
            refreshGeneration();
            advanceAdjacent(adjacent, now, firstBlock, lastBlock);
            if (!shadowed.empty()) MarkShadowed(*shadowImport, shadowed, now);
        } else if (!shadowed.empty()) {
            MarkShadowed(*shadowImport, shadowed, GuestMemory::TrackerGeneration());
        }
    };
    if (keep.empty()) {
        // Every selected block was written by the CPU since the layer's generation: nothing to
        // store, the keys stay as they are, and the image is stale there (re-uploaded at its next
        // use, never from a matching `original`).
        emptyWriteBacks.fetch_add(1, std::memory_order_relaxed);
        originalValid = false;
        settle(true);
        return;
    }
    if (const auto* import = HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
        if (blockUnits) {
            // The kept blocks alone pass through the retiler (windows of their slices).
            shadowImport = import;
            const auto storedBytes = writeBackWindows(*import, keep, firstStored, lastStored, shadowed, imported);
            countStorageWriteBack(storedBytes, true);
            if (profile) Profile().storageGpu += timer.lap();
            originalValid = false;
            traceKeyStore("block write-back", descriptor, guestBytes);
            if (!IsDccClear(filledKeys)) MarkDccUncompressed(context, descriptor.dccAddress, guestBytes, DccKeyCount(descriptor, guestBytes));
            uploadedKeys = DccKeys::Uncompressed;
            for (const auto& [from, to] : keep) GuestMemory::MarkWritten(from, static_cast<std::size_t>(to - from));
            settle(true);
            ++Profile().storageDirectWriteBacks;
            return;
        }
        // The whole-layer copies below overwrite the interior units of the span; the edge units
        // keep a neighbour's shadowed results in the rest of their bytes, which reach the import
        // first (this write-back's stamps then make those units stale).
        PublishShadow(firstStored, static_cast<std::size_t>(lastStored - firstStored), PublishScope::PartialUnits, PublishReason::Upload);
        // The surface lives in host-imported memory: the retiler writes into device scratch and the
        // untouched blocks are copied into the imported bytes in place, recorded behind the work that
        // produced the image; nothing crosses to the CPU.
        auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(sliceLinearBytes * arrayLayers), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        auto tiledScratch = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(guestBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        std::vector<CopiedBytes> copied;
        for (const auto& [from, to] : keep) copied.push_back({from - descriptor.baseAddress, to - descriptor.baseAddress, from - descriptor.baseAddress});
        const auto padding = paddingSeeds(*import, std::move(copied));
        detiler.BeginBatch();
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        auto timing = Recorder::NoTiming;
        std::uint64_t storedBytes = 0;
        for (const auto& [from, to] : keep) storedBytes += to - from;
        if (recorder != nullptr) {
            recorder->FlushKeyStoresOverlapping(firstStored, static_cast<std::size_t>(lastStored - firstStored));
            recorder->FlushStoresOverlapping(firstStored, static_cast<std::size_t>(lastStored - firstStored));
            commands = recorder->Commands();
            timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageWriteBack);
            recorder->Keep(linear, linear->Size());
            recorder->Keep(tiledScratch, tiledScratch->Size());
            for (const auto& slab : padding.slabs) recorder->Keep(slab);
            // The image itself must outlive the recorded retile: the cache may evict it right after.
            if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
            Recorder::CountBarriers(Recorder::CommandClass::StorageWriteBack, padding.copies.empty() ? 4 : 5);
            if (Recorder::BarrierValidate()) {
                const std::pair<VkImage, bool> read{image, false};
                recorder->NoteAccess(Recorder::CommandClass::StorageWriteBack, Recorder::Access{padding.importReads, keep, std::span(&read, 1), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT});
            }
        } else {
            batch = std::make_unique<CommandBatch>(context);
            commands = batch->Handle();
        }
        const auto importOffset = descriptor.baseAddress - import->base;
        // The tracked layers are the array layers when there are several; one tracked layer means
        // the whole surface.
        const auto* storedLayers = trackedLayers == arrayLayers && trackedLayers > 1 ? &layers : nullptr;
        VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toSource.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSource.image = image;
        toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
        if (!padding.copies.empty()) {
            // The current bytes of the copied ranges no element holds go under the retile (every
            // earlier writer of the import, host stores included, first; the import-ready barrier
            // below orders the scratch writes).
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            for (const auto& [source, copies] : padding.copies) context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, source, tiledScratch->Handle(), static_cast<std::uint32_t>(copies.size()), copies.data());
        }
        const auto regions = CopyRegions(storedLayers);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear->Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
        const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        const VkMemoryBarrier importReady{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &importReady, 1, &linearRead, 0, nullptr);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            if (storedLayers != nullptr && !(*storedLayers)[layer]) continue;
            for (std::uint32_t level = 0; level < mips.size(); ++level) {
                if (!geometry.HasLayer(level, layer)) continue;
                const auto& mip = mips[level];
                detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear->Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, tiledScratch->Handle(), geometry.GuestLayerOffset(layer) + mip.tiledOffset, mip, true, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
            }
        }
        {
            const auto scratchDone = WholeBufferBarrier(tiledScratch->Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &scratchDone, 0, nullptr);
            std::vector<VkBufferCopy> copies;
            for (const auto& [from, to] : keep) copies.push_back({from - descriptor.baseAddress, importOffset + (from - descriptor.baseAddress), to - from});
            if (!copies.empty()) context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, tiledScratch->Handle(), import->buffer, static_cast<std::uint32_t>(copies.size()), copies.data());
        }
        VkImageMemoryBarrier backToGeneral = toSource;
        backToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        backToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        backToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        backToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        const VkMemoryBarrier stored{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &stored, 0, nullptr, 1, &backToGeneral);
        if (batch) {
            batch->SubmitAndWait();
        } else {
            recorder->EndGpuTiming(timing, storedBytes);
            recorder->MarkShaderReadsCovered();
            recorder->NotePendingWrite(firstStored, static_cast<std::size_t>(lastStored - firstStored));
        }
        countStorageWriteBack(storedBytes, true);
        if (profile) Profile().storageGpu += timer.lap();
        // The guest bytes now differ from `original`; other caches of the range see the write.
        originalValid = false;
        // The keys go the same way as the texels: a fill recorded behind the retile when the
        // metadata is host-imported (no CPU wait for the title's key-writing kernels), else a CPU
        // store (APS5_CPU_DCC_KEYS=1 keeps the CPU store; see DccMetadata.hpp).
        traceKeyStore("layer write-back", descriptor, guestBytes);
        if (!IsDccClear(filledKeys)) MarkDccUncompressed(context, descriptor.dccAddress, guestBytes, DccKeyCount(descriptor, guestBytes));
        uploadedKeys = DccKeys::Uncompressed;
        for (const auto& [from, to] : keep) GuestMemory::MarkWritten(from, static_cast<std::size_t>(to - from));
        // The walk covers this surface's pages only: an adjacent image's later CPU write is stamped
        // newer than this value when its own range is collected. No CPU wrote the pages here
        // (MarkWritten made the only stamps), so the memoized collect is exact.
        settle(true);
        ++Profile().storageDirectWriteBacks;
        return;
    }
    // A surface whose memory is no longer registered (freed, or re-registered under another
    // allocation: no readable registered range contains it) has nothing to receive its results;
    // the round trip through the CPU below would store stale texels over whatever the memory
    // holds now. Its selected units are dropped instead. APS5_NO_UNREGISTERED_DROP=1 stores.
    static const bool unregisteredDrop = std::getenv("APS5_NO_UNREGISTERED_DROP") == nullptr;
    if (unregisteredDrop && !RegisteredReadableCovers(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 4) std::fprintf(stderr, "[gpu] storage image 0x%llx+0x%llx: memory no longer registered; %zu pending ranges dropped\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), keep.size());
        unregisteredDropped.fetch_add(1, std::memory_order_relaxed);
        originalValid = false;
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (layers[layer]) layerPending[layer] = false;
        }
        return;
    }
    // The store below compares against the guest bytes, read now when `original` does not hold
    // them (a GPU clear, a dropped or kept unit). Reading them says nothing about the image: a
    // stale unit (dropped, or kept for the CPU) still differs from them, so `original` vouches for
    // the content again only where it did before, or once every unit is stored.
    const bool wasValid = originalValid;
    if (!wasValid) GuestMemory::ReadCommitted(descriptor.baseAddress, original);
    DeviceBuffer linear(context, static_cast<std::size_t>(sliceLinearBytes * arrayLayers), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    DeviceBuffer tiled(context, original.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer host(context, original.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (profile) Profile().storageAlloc += timer.lap();
    // Start from the uploaded bytes so padding and untouched texels keep their guest values.
    std::memcpy(host.Bytes().data(), original.data(), original.size());
    if (profile) Profile().storageHostCopy += timer.lap();
    detiler.BeginBatch();
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    CopyBuffer(context, commands, host.Handle(), 0, tiled.Handle(), 0, original.size());
    VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSource.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.image = image;
    toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
    const auto regions = CopyRegions();
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear.Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
    const VkBufferMemoryBarrier toShader[] = {WholeBufferBarrier(linear.Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT), WholeBufferBarrier(tiled.Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 2, toShader, 0, nullptr);
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        for (std::uint32_t level = 0; level < mips.size(); ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            const auto& mip = mips[level];
            detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear.Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, tiled.Handle(), geometry.GuestLayerOffset(layer) + mip.tiledOffset, mip, true, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
        }
    }
    // Debug aid: APS5_DUMP_STORAGE=<hex address> saves that storage image's first mip after each of
    // its first 8 write-backs as storage_<address>_<n>.raw (u32 width, height, VkFormat, then rows).
    static const std::uint64_t dumpAddress = [] { const char* text = std::getenv("APS5_DUMP_STORAGE"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();
    static int dumps = 0;
    std::unique_ptr<Buffer> dump;
    if (dumpAddress != 0 && descriptor.baseAddress == dumpAddress && dumps < 8) {
        dump = std::make_unique<Buffer>(context, static_cast<std::size_t>(mips[0].linearSize), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        CopyBuffer(context, commands, linear.Handle(), mips[0].linearOffset, dump->Handle(), 0, mips[0].linearSize);
    }
    const auto toCopy = WholeBufferBarrier(tiled.Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkImageMemoryBarrier backToGeneral = toSource;
    backToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    backToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    backToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    backToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &toCopy, 1, &backToGeneral);
    CopyBuffer(context, commands, tiled.Handle(), 0, host.Handle(), 0, original.size());
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    APS5_LOG_CHARS_OUT("StorageTexture writeback submit");
    batch.SubmitAndWait();
    APS5_LOG_CHARS_OUT("StorageTexture writeback done");
    countStorageWriteBack(guestBytes, false);
    if (profile) Profile().storageGpu += timer.lap();
    if (dump) {
        char name[64];
        std::snprintf(name, sizeof(name), "storage_%llx_%d.raw", static_cast<unsigned long long>(descriptor.baseAddress), dumps++);
        if (std::FILE* file = std::fopen(name, "wb")) {
            const std::uint32_t header[3] = {mips[0].pitchBytes / static_cast<std::uint32_t>(elementBytes), mips[0].height, static_cast<std::uint32_t>(storageFormat)};
            std::fwrite(header, sizeof(header), 1, file);
            std::fwrite(dump->Bytes().data(), 1, dump->Bytes().size(), file);
            std::fclose(file);
        }
    }
    // Store only the kept ranges' blocks that changed, so concurrent CPU writes to untouched texels
    // survive; `original` follows for the stored layers (the others' guest bytes are unchanged).
    const auto current = host.Bytes();
    for (const auto& [from, to] : keep) {
        const auto offset = static_cast<std::size_t>(from - descriptor.baseAddress);
        const auto length = static_cast<std::size_t>(to - from);
        GuestMemory::WriteChangedCommitted(from, current.subspan(offset, length), std::span<const std::byte>(original).subspan(offset, length));
        std::memcpy(original.data() + offset, current.data() + offset, length);
    }
    // The texels now hold the whole image, so later reads must see them rather than a fast clear
    // (the keys may be host-imported although the texels were not: then a recorded fill, else a CPU store).
    traceKeyStore("cpu write-back", descriptor, guestBytes);
    if (!IsDccClear(filledKeys)) MarkDccUncompressed(context, descriptor.dccAddress, guestBytes, DccKeyCount(descriptor, guestBytes));
    uploadedKeys = DccKeys::Uncompressed;
    // The store above is the only write to these pages, so `original` is current at a fresh
    // generation, unless blocks were kept for the CPU: then the image is stale there.
    originalValid = !skippedAny && (wasValid || std::all_of(layers.begin(), layers.end(), [](bool selected) { return selected; }));
    // As in the GPU-direct path, but the store's memcpy dirtied this surface's pages in the write
    // watch: a memoized collect would leave them for the next walk to stamp newer than this value
    // (the shared edge block included, undoing the advance), so the walk is made here and
    // consumes them.
    settle(false);
    if (profile) Profile().storageStore += timer.lap();
}

StorageTexture::~StorageTexture() {
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        std::erase(live.textures, this);
    }
    // Cache eviction flushes first; anything still pending here is being torn down with the device.
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        if (dirty) {
            dirty = false;
            pending.textures.remove(this);
            BumpPendingSerial();
            static std::atomic<int> reports{0};
            if (reports.fetch_add(1) < 4) std::fprintf(stderr, "[gpu] storage image 0x%llx destroyed with GPU results pending\n", static_cast<unsigned long long>(descriptor.baseAddress));
        }
    }
    release();
}

void StorageTexture::release() noexcept {
    for (const auto& [mip, extra] : extraViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, extra, nullptr);
    extraViews.clear();
    for (const auto& [mip, extra] : firstLayerViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, extra, nullptr);
    firstLayerViews.clear();
    for (const auto& [key, atomic] : atomicViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, atomic, nullptr);
    atomicViews.clear();
    for (const auto& [key, uint] : uintViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, uint, nullptr);
    uintViews.clear();
    if (elementView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, elementView, nullptr);
    elementView = VK_NULL_HANDLE;
    for (const auto& [format, attachment] : attachmentViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, attachment, nullptr);
    attachmentViews.clear();
    if (proxyView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, proxyView, nullptr);
    if (proxyImage) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, proxyImage, nullptr);
    if (proxyMemory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, proxyMemory, nullptr);
    if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
    if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
}

std::uint64_t StorageTexture::GuestBytes() const {
    return guestBytes;
}

VkImageView StorageTexture::View() const {
    return view;
}

}
