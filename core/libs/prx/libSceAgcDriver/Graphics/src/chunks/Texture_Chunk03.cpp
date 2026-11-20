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
bool Texture::refreshBlockCopy() {
    std::vector<VkImageCopy> regions;
    for (std::uint32_t level = 0; level < copyLevels; ++level) {
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, copyLayers};
        region.dstSubresource = region.srcSubresource;
        region.extent = {std::max(copyWidth >> level, 1u) / copyTexelsPerElement, std::max(copyHeight >> level, 1u) / copyTexelsPerElement, 1u};
        regions.push_back(region);
    }
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    if (recorder != nullptr) {
        commands = recorder->Commands();
        recorder->Keep(owned);
        recorder->Keep(storageSource);
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    const auto barrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    const VkMemoryBarrier produced{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
    VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransferDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransferDst.oldLayout = copiedOnce ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.image = image;
    toTransferDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, copyLevels, 0, copyLayers};
    barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &produced, 0, nullptr, 1, &toTransferDst);
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, storageSource->Image(), VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
    VkImageMemoryBarrier toShaderRead = toTransferDst;
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShaderRead);
    if (batch) batch->SubmitAndWait();
    copiedOnce = true;
    copiedVersion = storageSource->Version();
    return true;
}

Texture::DepthCopyCounts Texture::TakeDepthCopyCounts() {
    return {depthCopyTextures.exchange(0, std::memory_order_relaxed), depthCopyRefreshes.exchange(0, std::memory_order_relaxed)};
}

bool Texture::ReuseLookupHolds() const {
    const bool held = reuseLookup().get() == this;
    (held ? reuseLookupsHeld : reuseLookupsChanged).fetch_add(1, std::memory_order_relaxed);
    return held;
}

Texture::ReuseLookupCounts Texture::TakeReuseLookupCounts() {
    return {reuseLookupsHeld.exchange(0, std::memory_order_relaxed), reuseLookupsChanged.exchange(0, std::memory_order_relaxed)};
}

bool Texture::CanCopyFrom(const StorageTexture& source, const GuestTextureResource& descriptor) {
    const auto& from = source.Descriptor();
    if (IsBlockCompressed(descriptor.format) || IsBlockCompressed(from.format)) return false;
    // Same memory, same layout, same texel size: the GPU copy reinterprets the texels exactly as a
    // guest read through the sampled descriptor would.
    if (IsRepackedTextureFormat(descriptor.format)) return false;
    return descriptor.baseAddress == from.baseAddress && descriptor.pipeBankXor == from.pipeBankXor && descriptor.width == from.width && descriptor.height == from.height && descriptor.linearPitch == from.linearPitch && ArrayLayersServeView(from, descriptor) && descriptor.tileMode == from.tileMode && descriptor.mipCount <= from.mipCount && BytesPerElement(descriptor.format) == BytesPerElement(from.format) && BlockWidth(descriptor.format) == BlockWidth(from.format);
}

Texture::Texture(const Context& context, const std::shared_ptr<StorageTexture>& source, const GuestTextureResource& descriptor, VkComponentMapping components) : context(context), storageSource(source) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    try {
        Require(source != nullptr && CanCopyFrom(*source, descriptor), "storage image does not match the sampled texture");
        const auto vkFormat = SampledTextureFormat(context, descriptor.format);
        const auto geometry = DescribeSurface(descriptor);
#ifdef APS5_DEBUG_LOG
        AGC_LOG_DEFERRED(stdout, "Texture address=0x%llx %ux%u mips=%u viewed from storage image (vk=%d)", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, static_cast<int>(vkFormat));
#endif
        // Storage images stay in the general layout; the view samples them there.
        layout = VK_IMAGE_LAYOUT_GENERAL;
        const auto viewLevelCount = std::min(descriptor.lastLevel, descriptor.mipCount - 1u) - descriptor.baseLevel + 1u;
        const auto viewLayerCount = std::min(geometry.imageLayers, source->ImageLayers()) - descriptor.baseArray;
        if (descriptor.dimension == TextureDimension::kCube) {
            Require(viewLayerCount >= 6u, "guest cube texture view holds fewer than 6 array slices");
        }
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = source->Image();
        VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
        usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        viewInfo.pNext = &usage;
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
        prepareBaseLevelView(descriptor, viewInfo);
        prepareSrgbView(descriptor, viewInfo);
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

Texture::Texture(const Context& context, VkImage depthImage, VkFormat depthFormat, VkImageAspectFlags aspect, VkComponentMapping components, VkImageViewType viewType, std::uint32_t baseLayer, std::shared_ptr<OwnedImage> owner, VkImageUsageFlags usage, std::uint32_t layerCount) : context(context), owned(std::move(owner)) {
    Require(owned == nullptr || (owned->image == depthImage && owned->context.device == context.device), "a depth sampled view must retain its own device and image");
    layout = VK_IMAGE_LAYOUT_GENERAL;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    VkImageViewUsageCreateInfo viewUsage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    viewUsage.usage = usage;
    if (usage != 0) viewInfo.pNext = &viewUsage;
    viewInfo.image = depthImage;
    viewInfo.viewType = viewType;
    viewInfo.format = depthFormat;
    viewFormat = depthFormat;
    viewInfo.components = components;
    viewInfo.subresourceRange = {aspect, 0, 1, baseLayer, viewType == VK_IMAGE_VIEW_TYPE_2D ? 1u : layerCount};
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth plane");
    viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
    if (viewType != VK_IMAGE_VIEW_TYPE_2D_ARRAY) return;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.layerCount = 1;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &firstLayerView), "vkCreateImageView depth plane first layer");
    firstLayerRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
}

Texture::~Texture() {
    release();
}

void Texture::createFirstLayerView(const GuestTextureResource& descriptor, VkImageViewCreateInfo viewInfo) {
    if (descriptor.dimension == TextureDimension::k2D && viewInfo.viewType == VK_IMAGE_VIEW_TYPE_2D && viewInfo.subresourceRange.layerCount == 1u) {
        for (auto* next = static_cast<const VkBaseInStructure*>(viewInfo.pNext); next != nullptr; next = next->pNext) {
            if (next->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT) arrayMinLod = reinterpret_cast<const VkImageViewMinLodCreateInfoEXT*>(next)->minLod;
        }
        auto info = viewInfo;
        info.pNext = nullptr;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        arrayInfo = info;
    }
    if (descriptor.dimension != TextureDimension::k2DArray && (viewInfo.viewType != VK_IMAGE_VIEW_TYPE_2D_ARRAY || descriptor.dimension != TextureDimension::k1DArray)) return;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.layerCount = 1;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &firstLayerView), "vkCreateImageView first layer");
    firstLayerRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
}

void Texture::prepareBaseLevelView(const GuestTextureResource& descriptor, VkImageViewCreateInfo viewInfo) {
    if (viewInfo.subresourceRange.levelCount == 1u) return;
    const bool depth1D = (viewInfo.subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) && (descriptor.dimension == TextureDimension::k1D || descriptor.dimension == TextureDimension::k1DArray);
    if (!depth1D && descriptor.dimension != TextureDimension::k1D && descriptor.dimension != TextureDimension::k2D && descriptor.dimension != TextureDimension::k2DArray) return;
    viewInfo.pNext = nullptr;
    viewInfo.viewType = (descriptor.dimension == TextureDimension::k1D && !depth1D) ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    baseLevelInfo = viewInfo;
}

void Texture::prepareSrgbView(const GuestTextureResource& descriptor, VkImageViewCreateInfo viewInfo) {
    const auto format = SrgbTwinFormat(context, viewInfo.format);
    if (format == VK_FORMAT_UNDEFINED) return;
    viewInfo.pNext = nullptr;
    viewInfo.format = format;
    srgbInfo = viewInfo;
    srgbMinLod = context.imageViewMinLod ? EffectiveMinLod(descriptor) : 0.0f;
}

VkImageView Texture::SrgbView(bool firstLayer, bool baseLevel) const {
    Require(srgbInfo.has_value(), "AGC graphics: a texture of view format " + std::to_string(viewFormat) + " has no sRGB view for a sampler that forces sRGB decoding");
    std::lock_guard lock(srgbMutex);
    auto& slot = srgbViews[baseLevel ? 2u : firstLayer ? 1u : 0u];
    if (slot != VK_NULL_HANDLE) return slot;
    VkImageViewCreateInfo info = *srgbInfo;
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    info.pNext = &usage;
    VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
    if (baseLevel) {
        info.viewType = info.viewType == VK_IMAGE_VIEW_TYPE_1D || info.viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_2D;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
    } else {
        if (firstLayer) {
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.subresourceRange.layerCount = 1;
        }
        if (srgbMinLod != 0.0f) {
            minLod.minLod = srgbMinLod;
            minLod.pNext = info.pNext;
            info.pNext = &minLod;
        }
    }
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &slot), "vkCreateImageView sRGB");
    return slot;
}

VkImageView Texture::ArrayView() const {
    Require(arrayInfo.has_value(), "a 2D array view of a texture needs a single-layer 2D texture");
    std::lock_guard lock(arrayMutex);
    if (arrayView == VK_NULL_HANDLE) {
        auto info = *arrayInfo;
        VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
        usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        info.pNext = &usage;
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        if (arrayMinLod != 0.0f) {
            minLod.minLod = arrayMinLod;
            minLod.pNext = info.pNext;
            info.pNext = &minLod;
        }
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &arrayView), "vkCreateImageView 2D array");
    }
    return arrayView;
}

VkImageView Texture::BaseLevelView() const {
    Require(baseLevelInfo.has_value(), "a base-level view needs a mipmapped 1D, 2D or 2D array texture");
    std::lock_guard lock(baseLevelMutex);
    if (!baseLevelView) {
        auto info = *baseLevelInfo;
        VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
        usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        info.pNext = &usage;
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &info, nullptr, &baseLevelView), "vkCreateImageView base level");
    }
    return baseLevelView;
}

void Texture::release() noexcept {
    upload.reset();
    if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    view = VK_NULL_HANDLE;
    if (firstLayerView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, firstLayerView, nullptr);
    firstLayerView = VK_NULL_HANDLE;
    if (baseLevelView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, baseLevelView, nullptr);
    baseLevelView = VK_NULL_HANDLE;
    if (arrayView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, arrayView, nullptr);
    arrayView = VK_NULL_HANDLE;
    for (auto& srgbView : srgbViews) {
        if (srgbView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, srgbView, nullptr);
        srgbView = VK_NULL_HANDLE;
    }
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

VkFormat PackedStorageViewFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
            return VK_FORMAT_R16_UINT;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

// Storage images cannot use sRGB formats; the shader works on the raw encoded values either way.
// The answer is a property of the physical device, so it is computed once per format (every storage
// image lookup asks: a live vkGetPhysicalDeviceFormatProperties call each time was measurable) and
// remembered as VK_FORMAT_UNDEFINED when the format has no storage form.
VkFormat StorageFormatOrUndefined(const Context& context, VkFormat format) {
    struct Table {
        AgcDriver::Mutex mutex;
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
    const auto packed = PackedStorageViewFormat(format);
    VkFormat linear = VK_FORMAT_UNDEFINED;
    switch (format) {
        case VK_FORMAT_R8G8B8A8_SRGB: linear = VK_FORMAT_R8G8B8A8_UNORM; break;
        case VK_FORMAT_B8G8R8A8_SRGB: linear = VK_FORMAT_B8G8R8A8_UNORM; break;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: linear = VK_FORMAT_A8B8G8R8_UNORM_PACK32; break;
        case VK_FORMAT_R8_SRGB: linear = VK_FORMAT_R8_UNORM; break;
        case VK_FORMAT_R8G8_SRGB: linear = VK_FORMAT_R8G8_UNORM; break;
        default: break;
    }
    if (packed != VK_FORMAT_UNDEFINED) {
        if (supports(packed)) storage = format;
    } else if (linear != VK_FORMAT_UNDEFINED) {
        if (supports(linear)) storage = linear;
    } else if (supports(format)) {
        storage = format;
    }
    std::lock_guard lock(table.mutex);
    table.formats.emplace(key, storage);
    return storage;
}

VkFormat UintStorageViewFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8G8_SINT: return VK_FORMAT_R8G8_UINT;
        case VK_FORMAT_R8G8B8A8_SINT: return VK_FORMAT_R8G8B8A8_UINT;
        case VK_FORMAT_R16_SINT: return VK_FORMAT_R16_UINT;
        case VK_FORMAT_R16G16_SINT: return VK_FORMAT_R16G16_UINT;
        case VK_FORMAT_R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_UINT;
        case VK_FORMAT_R32_SINT: return VK_FORMAT_R32_UINT;
        case VK_FORMAT_R32G32_SINT: return VK_FORMAT_R32G32_UINT;
        case VK_FORMAT_R32G32B32A32_SINT: return VK_FORMAT_R32G32B32A32_UINT;
        case VK_FORMAT_R16_SFLOAT: return VK_FORMAT_R16_UINT;
        case VK_FORMAT_R16G16_SFLOAT: return VK_FORMAT_R16G16_UINT;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_UINT;
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

VkFormat HostStorageFormat(const Context& context, VkFormat format) {
    return StorageFormatOrUndefined(context, format);
}

bool IntegerFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_UINT: case VK_FORMAT_R8_SINT: case VK_FORMAT_R8G8_UINT: case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8B8A8_UINT: case VK_FORMAT_R8G8B8A8_SINT: case VK_FORMAT_B8G8R8A8_UINT: case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32: case VK_FORMAT_A8B8G8R8_SINT_PACK32: case VK_FORMAT_A2R10G10B10_UINT_PACK32: case VK_FORMAT_A2B10G10R10_UINT_PACK32: case VK_FORMAT_A2B10G10R10_SINT_PACK32:
        case VK_FORMAT_R16_UINT: case VK_FORMAT_R16_SINT: case VK_FORMAT_R16G16_UINT: case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16B16A16_UINT: case VK_FORMAT_R16G16B16A16_SINT: case VK_FORMAT_R32_UINT: case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32G32_UINT: case VK_FORMAT_R32G32_SINT: case VK_FORMAT_R32G32B32_UINT: case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32A32_UINT: case VK_FORMAT_R32G32B32A32_SINT: case VK_FORMAT_R64_UINT: case VK_FORMAT_R64_SINT:
            return true;
        default: return false;
    }
}

bool ClearColorFor(VkFormat format, DccKeys keys, VkClearColorValue& clear) {
    if (IntegerFormat(format)) {
        if (keys != DccKeys::Clear0000) return false;
        clear.uint32[0] = clear.uint32[1] = clear.uint32[2] = clear.uint32[3] = 0u;
        return true;
    }
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

}
}
}
