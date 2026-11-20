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
namespace {
namespace {
namespace {
bool StorageClearAvailable(const Context& context, std::uint32_t guestFormat, DccKeys keys) {
    VkClearColorValue clear{};
    return IsDccClear(keys) && StorageFormatAvailable(context, guestFormat) && ClearColorFor(StorageFormatForGuest(context, guestFormat), keys, clear);
}

VkFormat SrgbTwinFormat(const Context& context, VkFormat format) {
    const auto decoded = [&](ShaderRecompiler::IrBufferFormat srgb) { return (context.srgbDecodeFormats & ShaderRecompiler::SrgbDecodeBit(srgb)) != 0u; };
    switch (format) {
        case VK_FORMAT_R8_UNORM: return decoded(ShaderRecompiler::IrBufferFormat::Format8Srgb) ? VK_FORMAT_UNDEFINED : VK_FORMAT_R8_SRGB;
        case VK_FORMAT_R8G8_UNORM: return decoded(ShaderRecompiler::IrBufferFormat::Format8_8Srgb) ? VK_FORMAT_UNDEFINED : VK_FORMAT_R8G8_SRGB;
        case VK_FORMAT_R8_SNORM: return decoded(ShaderRecompiler::IrBufferFormat::Format8Srgb) ? VK_FORMAT_UNDEFINED : VK_FORMAT_R8_SRGB;
        case VK_FORMAT_R8G8_SNORM: return decoded(ShaderRecompiler::IrBufferFormat::Format8_8Srgb) ? VK_FORMAT_UNDEFINED : VK_FORMAT_R8G8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM: return VK_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case VK_FORMAT_BC2_UNORM_BLOCK: return VK_FORMAT_BC2_SRGB_BLOCK;
        case VK_FORMAT_BC3_UNORM_BLOCK: return VK_FORMAT_BC3_SRGB_BLOCK;
        case VK_FORMAT_BC7_UNORM_BLOCK: return VK_FORMAT_BC7_SRGB_BLOCK;
        default: return VK_FORMAT_UNDEFINED;
    }
}

VkFormat AttachmentProxyFormat(const Context& context, VkFormat format) {
    if (format != VK_FORMAT_R8_SRGB) return VK_FORMAT_UNDEFINED;
    struct Table {
        AgcDriver::Mutex mutex;
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

std::shared_ptr<StorageTexture> StorageTexture::Create(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, std::uint32_t mipLevel) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    InstallUnwatchHook();
    const MapImportGate::PublishHold publication(descriptor.baseAddress, DescribeSurface(descriptor).guestBytes);
    auto texture = std::shared_ptr<StorageTexture>(new StorageTexture(context, detiler, descriptor, mipLevel));
    uploadReason = "first";
    texture->upload();
    texture->defaultMip = mipLevel;
    texture->view = texture->createView(mipLevel, false, texture->storageFormat);
    if (const auto hook = publishHookForTest.load(std::memory_order_acquire)) hook();
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        live.textures.push_back(texture.get());
    }
    if (profile) {
        auto& totals = Profile();
        totals.storageCreate += timer.lap();
        totals.storageBytes += texture->guestBytes;
        if (++totals.storageCount % 50 == 0) std::fprintf(stderr, "[texture] %llu storage images (%.0f MiB): create %.0f ms, write-back %.0f ms (alloc %.0f, host copy %.0f, gpu %.0f, store %.0f), %llu reused, %llu direct uploads, %llu direct write-backs, clears %llu recorded / %llu waited\n", static_cast<unsigned long long>(totals.storageCount), totals.storageBytes / 1048576.0, totals.storageCreate, totals.storageWriteBack, totals.storageAlloc, totals.storageHostCopy, totals.storageGpu, totals.storageStore, static_cast<unsigned long long>(totals.storageReused), static_cast<unsigned long long>(totals.storageDirectUploads), static_cast<unsigned long long>(totals.storageDirectWriteBacks), static_cast<unsigned long long>(totals.storageRecordedClears), static_cast<unsigned long long>(totals.storageWaitedClears));
    }
    return texture;
}

void StorageTexture::clearImage(VkCommandBuffer commands, VkClearColorValue value, const VkImageSubresourceRange& range) {
    if (imageFormat == VK_FORMAT_R8G8B8A8_UINT && storageFormat == VK_FORMAT_R32_UINT) {
        const auto bits = value.uint32[0];
        for (std::uint32_t component = 0; component < 4; ++component) value.uint32[component] = (bits >> (8 * component)) & 0xffu;
    }
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &range);
}

StorageTexture::StorageTexture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, std::uint32_t mipLevel) : context(context), detiler(detiler), descriptor(descriptor) {
    try {
        Require(!IsBlockCompressed(descriptor.format), "block-compressed textures cannot be storage images");
        Require(mipLevel < descriptor.mipCount, "storage texture mip level is outside the texture");
        const auto vkFormat = StorageFormatFor(context, ResolveTextureFormat(descriptor.format));
        storageFormat = vkFormat;
        imageFormat = descriptor.format == static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format11_11_10Float) && storageFormat == VK_FORMAT_R32_UINT ? VK_FORMAT_R8G8B8A8_UINT : storageFormat;
#ifdef APS5_DEBUG_LOG
        AGC_LOG_DEFERRED(stdout, "StorageTexture address=0x%llx %ux%u mips=%u mip=%u layers=%u base=%u dim=%d tile=%d format=%u vk=%d", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, mipLevel,
                     descriptor.depthOrLastArray, descriptor.baseArray, static_cast<int>(descriptor.dimension), static_cast<int>(descriptor.tileMode), descriptor.format, static_cast<int>(vkFormat));
#endif
        geometry = DescribeSurface(descriptor);
        Require(!geometry.levelMajor, "a storage image of a PS4 layered surface with several mip levels (stored level by level) is not implemented");
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
        blockUnits = StorageBlockUnits(descriptor, geometry);
        trackedLayers = blockUnits ? static_cast<std::uint32_t>((guestBytes + 65535) / 65536) : layered && LayerTrackingEnabled() ? geometry.layers : 1u;
        trackedLayerBytes = blockUnits ? 65536 : guestBytes / trackedLayers;
        layerGeneration.assign(trackedLayers, 0);
        layerPendingSince.assign(trackedLayers, 0);
        unwatchHash.assign(trackedLayers, {});
        pendingMiss.assign(trackedLayers, BaselineMiss::None);
        layerPending.assign(trackedLayers, false);
        layerOriginal.assign(trackedLayers, true);
        savedTicket.assign(trackedLayers, 0);
        baselineDue.assign(trackedLayers, false);
        rebaseTicket.assign(trackedLayers, 0);
        sliceLinearBytes = geometry.sliceLinearBytes;
        const auto linearBytes = sliceLinearBytes * arrayLayers;

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        // Sampled views of other same-size formats (sRGB, reinterpretations) read the image directly.
        imageInfo.flags = (descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u) | (descriptor.dimension == TextureDimension::k3D ? VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT : 0u) | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
        imageInfo.imageType = ImageTypeFor(descriptor.dimension);
        imageInfo.format = imageFormat;
        imageInfo.extent = {descriptor.width, descriptor.height, geometry.imageDepth};
        imageInfo.mipLevels = descriptor.mipCount;
        Require(geometry.imageLayers <= context.limits.maxImageArrayLayers, "a texture array with more layers than maxImageArrayLayers is not implemented");
        imageInfo.arrayLayers = geometry.imageLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        {
            VkFormatProperties properties{};
            context.formatProperties(context.physical, vkFormat, &properties);
            attachable = (descriptor.dimension == TextureDimension::k2D || descriptor.dimension == TextureDimension::k3D || descriptor.dimension == TextureDimension::k2DArray) && (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
            if (attachable) imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage storage");
        imageAllocation = AllocateImageMemory(context, image, imageInfo, "vkAllocateMemory storage texture", true, true);
        memoryBytes = imageAllocation.Size();
        this->context.bufferPool = context.bufferPool;
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, imageAllocation.Memory(), imageAllocation.Offset()), "vkBindImageMemory storage");
    } catch (...) {
        release();
        throw;
    }
}

namespace {

class PendingList {
public:
    using iterator = std::vector<StorageTexture*>::iterator;
    using Ranges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
    iterator begin() { return textures.begin(); }
    iterator end() { return textures.end(); }
    void push_back(StorageTexture* texture) {
        textures.push_back(texture);
        changed();
    }
    iterator erase(iterator it) {
        const auto next = textures.erase(it);
        changed();
        return next;
    }
    void remove(const StorageTexture* texture) {
        if (std::erase(textures, texture) != 0) changed();
    }
    bool MayOverlap(std::uint64_t address, std::size_t bytes) {
        if (indexed != version) rebuild();
        return !textures.empty() && Overlaps(ranges, address, bytes);
    }
    bool SurelyClear(std::uint64_t address, std::size_t bytes) const {
        const auto current = published.load(std::memory_order_acquire);
        return current != nullptr && current->version == changes.load(std::memory_order_acquire) && !Overlaps(current->ranges, address, bytes);
    }
    bool ClearOnThread(std::uint64_t address, std::size_t bytes) const {
        struct CopyTag {};
        auto& copy = ThreadScratch<std::shared_ptr<const Snapshot>, CopyTag>();
        const auto version = changes.load(std::memory_order_acquire);
        if (copy == nullptr || copy->version != version) {
            auto current = published.load(std::memory_order_acquire);
            if (current == nullptr || current->version != version) return false;
            copy = std::move(current);
        }
        return !Overlaps(copy->ranges, address, bytes);
    }

private:
    struct Snapshot {
        std::uint64_t version = 0;
        Ranges ranges;
    };

    static bool Overlaps(const Ranges& ranges, std::uint64_t address, std::size_t bytes) {
        if (bytes == 0) return false;
        const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
        const auto it = std::upper_bound(ranges.begin(), ranges.end(), address, [](std::uint64_t value, const std::pair<std::uint64_t, std::uint64_t>& range) { return value < range.second; });
        return it != ranges.end() && it->first < end;
    }

    void changed() {
        ++version;
        changes.store(version, std::memory_order_release);
    }

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
        published.store(std::make_shared<const Snapshot>(Snapshot{version, ranges}), std::memory_order_release);
    }
    std::vector<StorageTexture*> textures;
    Ranges ranges;
    std::uint64_t version = 0;
    std::uint64_t indexed = ~std::uint64_t{0};
    std::atomic<std::uint64_t> changes{0};
    AtomicSharedPtr<const Snapshot> published;
};

// Storage images whose results have not reached guest memory yet.
struct PendingWrites {
    MapImportGate::LeafMutex<SlimMutex> mutex;
    PendingList textures;
    // Images taken out of `textures` by a FlushPending still storing them (see adjacentPendingUnchanged).
    std::vector<StorageTexture*> flushing;
    std::atomic<std::size_t> flushingCount{0};
    struct Drawn {
        StorageTexture* texture;
        std::uint64_t bytes;
        std::chrono::steady_clock::time_point since;
    };
    std::vector<Drawn> drawn;
    std::atomic<std::size_t> drawnCount{0};
};

PendingWrites& Pending() {
    static PendingWrites pending;
    return pending;
}

DrawnPending drawnPending() {
    DrawnPending result;
    auto& pending = Pending();
    if (pending.drawnCount.load(std::memory_order_acquire) == 0) return result;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(pending.mutex);
    result.images = pending.drawn.size();
    for (const auto& entry : pending.drawn) {
        result.bytes += entry.bytes;
        result.oldestMs = std::max(result.oldestMs, std::chrono::duration<double, std::milli>(now - entry.since).count());
    }
    return result;
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
struct RefreshingTag;
using Refreshing = HostThreadSlot<const StorageTexture*, RefreshingTag>;

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
    static AgcDriver::Mutex mutex;
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
                if (memory) MemoryAccounting::Free(context, memory);
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
        Check(MemoryAccounting::Allocate(context, allocation, "denormal clear probe", &memory), "vkAllocateMemory denormal clear probe");
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

}
}
}
}
