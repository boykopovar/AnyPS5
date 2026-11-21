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
VkFormat Texture::DepthCopyFormat(VkFormat colorFormat) {
    switch (colorFormat) {
        case VK_FORMAT_R32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case VK_FORMAT_R16_UNORM: return VK_FORMAT_D16_UNORM;
        default: return VK_FORMAT_UNDEFINED;
    }
}

bool Texture::CanDepthCopyFrom(const StorageTexture& source, const GuestTextureResource& descriptor) {
    if (!CanCopyFrom(source, descriptor) || descriptor.dimension == TextureDimension::k3D) return false;
    if (DepthCopyFormat(ResolveTextureFormat(descriptor.format)) == VK_FORMAT_UNDEFINED) return false;
    if (ResolveTextureFormat(source.Descriptor().format) != ResolveTextureFormat(descriptor.format)) return false;
    const auto geometry = DescribeSurface(descriptor);
    return geometry.imageDepth == 1 && source.ImageDepth() == 1 && source.ImageLayers() >= geometry.imageLayers && source.Descriptor().mipCount >= descriptor.mipCount;
}

bool Texture::CanBlockCopyFrom(const StorageTexture& source, const GuestTextureResource& descriptor) {
    const auto& from = source.Descriptor();
    if (descriptor.gcnPitch != 0 && from.gcnPitch == descriptor.gcnPitch && (descriptor.width != from.width || descriptor.height != from.height)) {
        return !IsBlockCompressed(descriptor.format) && !IsBlockCompressed(from.format) && !IsSrgbTextureFormat(descriptor.format) && BytesPerElement(from.format) == BytesPerElement(descriptor.format)
            && descriptor.baseAddress == from.baseAddress && descriptor.tileMode == from.tileMode && descriptor.dimension == TextureDimension::k2D && from.dimension == TextureDimension::k2D
            && descriptor.mipCount == 1u && from.mipCount == 1u && descriptor.width <= from.width && descriptor.height <= from.height
            && source.ImageDepth() == 1 && DescribeSurface(descriptor).guestBytes == source.GuestBytes();
    }
    if (!IsBlockCompressed(descriptor.format) || IsBlockCompressed(from.format) || BytesPerElement(from.format) != BytesPerElement(descriptor.format)) return false;
    if (descriptor.dimension != TextureDimension::k2D && descriptor.dimension != TextureDimension::k2DArray) return false;
    if (descriptor.baseAddress != from.baseAddress || descriptor.pipeBankXor != from.pipeBankXor || descriptor.dimension != from.dimension || descriptor.tileMode != from.tileMode || descriptor.depthOrLastArray != from.depthOrLastArray || descriptor.mipCount > from.mipCount) return false;
    for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
        const auto width = std::max(descriptor.width >> level, 1u);
        const auto height = std::max(descriptor.height >> level, 1u);
        if (width % 4u != 0 || height % 4u != 0 || std::max(from.width >> level, 1u) != width / 4u || std::max(from.height >> level, 1u) != height / 4u) return false;
    }
    const auto geometry = DescribeSurface(descriptor);
    return geometry.imageDepth == 1 && source.ImageDepth() == 1 && source.ImageLayers() >= geometry.imageLayers;
}

bool Texture::CanRepackCopyFrom(const StorageTexture& source, const GuestTextureResource& descriptor) {
    const auto& from = source.Descriptor();
    if (!IsRepackedTextureFormat(descriptor.format) || descriptor.dimension == TextureDimension::k3D || ResolveTextureFormat(from.format) != ResolveTextureFormat(descriptor.format)) return false;
    if (descriptor.baseAddress != from.baseAddress || descriptor.pipeBankXor != from.pipeBankXor || descriptor.width != from.width || descriptor.height != from.height || !ArrayLayersServeView(from, descriptor) || descriptor.tileMode != from.tileMode || descriptor.mipCount > from.mipCount) return false;
    const auto geometry = DescribeSurface(descriptor);
    return geometry.imageDepth == 1 && source.ImageDepth() == 1 && source.ImageLayers() >= geometry.imageLayers;
}

Texture::Texture(const Context& context, const std::shared_ptr<StorageTexture>& source, const GuestTextureResource& descriptor, VkComponentMapping components, RepackCopyTag) : context(context), storageSource(source), perUseCopy(PerUseCopy::RefreshedOnReuse), repackCopy(true) {
    try {
        Require(source != nullptr && CanRepackCopyFrom(*source, descriptor), "storage image cannot serve a repacked copy of the sampled texture");
        const auto format = SampledTextureFormat(context, descriptor.format);
        const auto geometry = DescribeSurface(descriptor);
        copyWidth = descriptor.width;
        copyHeight = descriptor.height;
        copyLevels = descriptor.mipCount;
        copyLayers = geometry.imageLayers;
        copyElementBytes = BytesPerElement(descriptor.format);
#ifdef APS5_DEBUG_LOG
        AGC_LOG_DEFERRED(stdout, "Texture address=0x%llx %ux%u mips=%u layers=%u repacked copy of storage image (guest format %u -> vk %d)", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, copyLayers, descriptor.format, static_cast<int>(format));
#endif

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.flags = descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u;
        imageInfo.imageType = ImageTypeFor(descriptor.dimension);
        imageInfo.format = format;
        imageInfo.extent = {descriptor.width, descriptor.height, 1u};
        imageInfo.mipLevels = copyLevels;
        imageInfo.arrayLayers = copyLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        owned = std::make_shared<OwnedImage>(context, VK_NULL_HANDLE, VK_NULL_HANDLE);
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage repacked copy");
        owned->image = image;
        owned->Allocate(context, imageInfo, "vkAllocateMemory repacked copy", true);
        allocationBytes = owned->memoryBytes;
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, owned->allocation.Memory(), owned->allocation.Offset()), "vkBindImageMemory repacked copy");

        const auto viewLevelCount = std::min(descriptor.lastLevel, descriptor.mipCount - 1u) - descriptor.baseLevel + 1u;
        const auto viewLayerCount = geometry.imageLayers - descriptor.baseArray;
        if (descriptor.dimension == TextureDimension::kCube) {
            Require(viewLayerCount >= 6u, "guest cube texture view holds fewer than 6 array slices");
        }
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = ViewTypeFor(descriptor.dimension, viewLayerCount);
        viewInfo.format = format;
        viewFormat = format;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        ChainMinLod(context, descriptor, viewInfo, minLod);
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView repacked copy");
        viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
        createFirstLayerView(descriptor, viewInfo);
        prepareBaseLevelView(descriptor, viewInfo);
    } catch (...) {
        release();
        throw;
    }
}

Texture::Texture(const Context& context, const std::shared_ptr<StorageTexture>& source, const GuestTextureResource& descriptor, VkComponentMapping components, BlockCopyTag) : context(context), storageSource(source), perUseCopy(PerUseCopy::RefreshedOnReuse) {
    try {
        Require(source != nullptr && CanBlockCopyFrom(*source, descriptor), "storage image cannot serve a block copy of the sampled texture");
        blockFormat = SampledTextureFormat(context, descriptor.format);
        const auto geometry = DescribeSurface(descriptor);
        copyTexelsPerElement = IsBlockCompressed(descriptor.format) ? 4u : 1u;
        copyWidth = descriptor.width;
        copyHeight = descriptor.height;
        copyLevels = descriptor.mipCount;
        copyLayers = geometry.imageLayers;
        copyElementBytes = BytesPerElement(descriptor.format);
#ifdef APS5_DEBUG_LOG
        AGC_LOG_DEFERRED(stdout, "Texture address=0x%llx %ux%u mips=%u layers=%u block copy of storage image (guest format %u -> vk %d)", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, copyLayers, source->Descriptor().format, static_cast<int>(blockFormat));
#endif

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = blockFormat;
        imageInfo.extent = {descriptor.width, descriptor.height, 1u};
        imageInfo.mipLevels = copyLevels;
        imageInfo.arrayLayers = copyLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.flags = SrgbTwinFormat(context, blockFormat) != VK_FORMAT_UNDEFINED ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0u;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        owned = std::make_shared<OwnedImage>(context, VK_NULL_HANDLE, VK_NULL_HANDLE);
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage block copy");
        owned->image = image;
        owned->Allocate(context, imageInfo, "vkAllocateMemory block copy", true);
        allocationBytes = owned->memoryBytes;
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, owned->allocation.Memory(), owned->allocation.Offset()), "vkBindImageMemory block copy");

        const auto viewLevelCount = std::min(descriptor.lastLevel, descriptor.mipCount - 1u) - descriptor.baseLevel + 1u;
        const auto viewLayerCount = geometry.imageLayers - descriptor.baseArray;
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = ViewTypeFor(descriptor.dimension, viewLayerCount);
        viewInfo.format = blockFormat;
        viewFormat = blockFormat;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        ChainMinLod(context, descriptor, viewInfo, minLod);
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView block copy");
        viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
        createFirstLayerView(descriptor, viewInfo);
        prepareBaseLevelView(descriptor, viewInfo);
        prepareSrgbView(descriptor, viewInfo);
    } catch (...) {
        release();
        throw;
    }
}

Texture::Texture(const Context& context, const std::shared_ptr<StorageTexture>& source, const GuestTextureResource& descriptor, VkComponentMapping components, DepthCopyTag) : context(context), storageSource(source), perUseCopy(PerUseCopy::RefreshedOnReuse) {
    try {
        Require(source != nullptr && CanDepthCopyFrom(*source, descriptor), "storage image cannot serve a depth copy of the sampled texture");
        const auto colorFormat = ResolveTextureFormat(descriptor.format);
        depthFormat = DepthCopyFormat(colorFormat);
        const auto geometry = DescribeSurface(descriptor);
        copyWidth = descriptor.width;
        copyHeight = descriptor.height;
        copyLevels = descriptor.mipCount;
        copyLayers = geometry.imageLayers;
        copyElementBytes = BytesPerElement(descriptor.format);
#ifdef APS5_DEBUG_LOG
        AGC_LOG_DEFERRED(stdout, "Texture address=0x%llx %ux%u mips=%u layers=%u depth copy of storage image (vk=%d -> %d)", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, copyLayers, static_cast<int>(colorFormat), static_cast<int>(depthFormat));
#endif

        const bool depth1D = descriptor.dimension == TextureDimension::k1D || descriptor.dimension == TextureDimension::k1DArray;
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.flags = descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u;
        imageInfo.imageType = depth1D ? VK_IMAGE_TYPE_2D : ImageTypeFor(descriptor.dimension);
        imageInfo.format = depthFormat;
        imageInfo.extent = {descriptor.width, depth1D ? 1u : descriptor.height, 1u};
        imageInfo.mipLevels = copyLevels;
        imageInfo.arrayLayers = copyLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        owned = std::make_shared<OwnedImage>(context, VK_NULL_HANDLE, VK_NULL_HANDLE);
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage depth copy");
        owned->image = image;
        owned->Allocate(context, imageInfo, "vkAllocateMemory depth copy", true);
        allocationBytes = owned->memoryBytes;
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, owned->allocation.Memory(), owned->allocation.Offset()), "vkBindImageMemory depth copy");

        const auto viewLevelCount = std::min(descriptor.lastLevel, descriptor.mipCount - 1u) - descriptor.baseLevel + 1u;
        const auto viewLayerCount = geometry.imageLayers - descriptor.baseArray;
        if (descriptor.dimension == TextureDimension::kCube) {
            Require(viewLayerCount >= 6u, "guest cube texture view holds fewer than 6 array slices");
        }
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = depth1D ? (descriptor.dimension == TextureDimension::k1DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D) : ViewTypeFor(descriptor.dimension, viewLayerCount);
        viewInfo.format = depthFormat;
        viewInfo.components = VkComponentMapping{};
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        ChainMinLod(context, descriptor, viewInfo, minLod);
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth copy");
        viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
        createFirstLayerView(descriptor, viewInfo);
        prepareBaseLevelView(descriptor, viewInfo);
        static_cast<void>(components);
        depthCopyTextures.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
        release();
        throw;
    }
}

bool Texture::CanSrgbExpandFrom(const Context& context, const StorageTexture& source, const GuestTextureResource& descriptor) {
    if (!SrgbExpandsToRgba8(context, descriptor.format) || !CanCopyFrom(source, descriptor) || descriptor.dimension == TextureDimension::k3D) return false;
    if (!StorageFormatAvailable(context, source.Descriptor().format) || StorageFormatForGuest(context, source.Descriptor().format) != SampledTextureFormat(context, descriptor.format)) return false;
    const auto geometry = DescribeSurface(descriptor);
    return geometry.imageDepth == 1 && source.ImageDepth() == 1 && source.ImageLayers() >= geometry.imageLayers && source.Descriptor().mipCount >= descriptor.mipCount;
}

Texture::Texture(const Context& context, const std::shared_ptr<StorageTexture>& source, const GuestTextureResource& descriptor, VkComponentMapping components, SrgbExpandTag) : context(context), storageSource(source), perUseCopy(PerUseCopy::RefreshedOnReuse), srgbExpand(true) {
    try {
        Require(source != nullptr && CanSrgbExpandFrom(context, *source, descriptor), "storage image cannot serve an sRGB copy of the sampled texture");
        const auto geometry = DescribeSurface(descriptor);
        copyWidth = descriptor.width;
        copyHeight = descriptor.height;
        copyLevels = descriptor.mipCount;
        copyLayers = geometry.imageLayers;
        APS5_LOG_OUT("Texture address=0x%llx %ux%u mips=%u layers=%u sRGB copy of storage image (guest format %u)", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height, descriptor.mipCount, copyLayers, descriptor.format);
        auto expanded = CreateExpandImage(context, descriptor, 1u, copyLayers);
        image = expanded.image;
        owned = std::move(expanded.owned);
        allocationBytes = expanded.bytes;

        const auto viewLevelCount = std::min(descriptor.lastLevel, descriptor.mipCount - 1u) - descriptor.baseLevel + 1u;
        const auto viewLayerCount = geometry.imageLayers - descriptor.baseArray;
        if (descriptor.dimension == TextureDimension::kCube) {
            Require(viewLayerCount % 6u == 0, "guest cube texture view does not contain a multiple of 6 array slices");
        }
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = ViewTypeFor(descriptor.dimension, viewLayerCount);
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewFormat = viewInfo.format;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        ChainMinLod(context, descriptor, viewInfo, minLod);
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView sRGB copy");
        viewRange = {viewInfo.viewType, viewInfo.subresourceRange.levelCount, viewInfo.subresourceRange.layerCount};
        createFirstLayerView(descriptor, viewInfo);
        prepareBaseLevelView(descriptor, viewInfo);
        sourceDescriptor = descriptor;
        countedBytes = allocationBytes;
        SampledMemoryCounter().fetch_add(countedBytes, std::memory_order_relaxed);
    } catch (...) {
        release();
        throw;
    }
}

bool Texture::refreshSrgbExpand() {
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
    const VkMemoryBarrier produced{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT};
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
    RecordExpandBlit(context, commands, storageSource->Image(), VK_IMAGE_LAYOUT_GENERAL, image, sourceDescriptor, 1u, copyLayers);
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

bool Texture::RefreshDepthCopy() {
    if (perUseCopy != PerUseCopy::RefreshedOnReuse || storageSource == nullptr) return false;
    if (copiedOnce && storageSource->Version() == copiedVersion) return false;
    if (blockFormat != VK_FORMAT_UNDEFINED) return refreshBlockCopy();
    if (srgbExpand) return refreshSrgbExpand();
    std::uint64_t linearBytes = 0;
    for (std::uint32_t level = 0; level < copyLevels; ++level) linearBytes += static_cast<std::uint64_t>(std::max(copyWidth >> level, 1u)) * std::max(copyHeight >> level, 1u) * copyElementBytes * copyLayers;
    auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | (repackCopy ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0u));
    std::vector<VkBufferImageCopy> fromSource, toDepth;
    std::uint64_t offset = 0;
    for (std::uint32_t level = 0; level < copyLevels; ++level) {
        const VkExtent3D extent{std::max(copyWidth >> level, 1u), std::max(copyHeight >> level, 1u), 1u};
        VkBufferImageCopy region{};
        region.bufferOffset = offset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, copyLayers};
        region.imageOffset = {0, 0, 0};
        region.imageExtent = extent;
        fromSource.push_back(region);
        region.imageSubresource.aspectMask = repackCopy ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
        toDepth.push_back(region);
        offset += static_cast<std::uint64_t>(extent.width) * extent.height * copyElementBytes * copyLayers;
    }
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    if (recorder != nullptr) {
        commands = recorder->Commands();
        recorder->Keep(linear, linear->Size());
        recorder->Keep(owned);
        recorder->Keep(storageSource);
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    const auto barrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    const VkMemoryBarrier produced{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT};
    VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransferDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransferDst.oldLayout = copiedOnce ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.image = image;
    toTransferDst.subresourceRange = {static_cast<VkImageAspectFlags>(repackCopy ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT), 0, copyLevels, 0, copyLayers};
    barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &produced, 0, nullptr, 1, &toTransferDst);
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, storageSource->Image(), VK_IMAGE_LAYOUT_GENERAL, linear->Handle(), static_cast<std::uint32_t>(fromSource.size()), fromSource.data());
    VkBufferMemoryBarrier linearReady{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    linearReady.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    linearReady.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    linearReady.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    linearReady.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    linearReady.buffer = linear->Handle();
    linearReady.offset = 0;
    linearReady.size = VK_WHOLE_SIZE;
    if (repackCopy) {
        Require(context.detiler != nullptr, "a repacked texture copy needs the texture detiler");
        linearReady.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &linearReady, 0, nullptr);
        for (const auto& region : fromSource) {
            const auto bytes = static_cast<std::uint64_t>(region.imageExtent.width) * region.imageExtent.height * copyElementBytes * copyLayers;
            context.detiler->DispatchRepack(commands, linear->Handle(), region.bufferOffset, linear->Handle(), region.bufferOffset, bytes);
        }
        linearReady.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        linearReady.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearReady, 0, nullptr);
    } else {
        barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearReady, 0, nullptr);
    }
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(toDepth.size()), toDepth.data());
    VkImageMemoryBarrier toShaderRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.image = image;
    toShaderRead.subresourceRange = toTransferDst.subresourceRange;
    barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShaderRead);
    if (batch) batch->SubmitAndWait();
    copiedOnce = true;
    copiedVersion = storageSource->Version();
    if (!repackCopy) depthCopyRefreshes.fetch_add(1, std::memory_order_relaxed);
    return true;
}

}
}
}
