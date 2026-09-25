#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
// For the declaration of PendingStorageOverlaps, defined below.
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <chrono>
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
#include <unordered_map>

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
};

TextureProfile& Profile() {
    static TextureProfile profile;
    return profile;
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
    return dimension == TextureDimension::k1D ? VK_IMAGE_TYPE_1D : dimension == TextureDimension::k3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
}

VkImageViewType ViewTypeFor(TextureDimension dimension, [[maybe_unused]] std::uint32_t viewLayerCount) {
    switch (dimension) {
        case TextureDimension::k1D: return VK_IMAGE_VIEW_TYPE_1D;
        case TextureDimension::k2D: return VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::k2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        // Shaders address cube maps as 2D arrays of faces.
        case TextureDimension::kCube: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureDimension::k3D: return VK_IMAGE_VIEW_TYPE_3D;
    }
    throw std::runtime_error("AGC graphics: Texture encountered an unknown guest texture dimension");
}

}

Texture::Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot) : context(context) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    try {
        const auto vkFormat = ResolveTextureFormat(descriptor.format);
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
                recorder->Keep(staging);
                recorder->Keep(tiled);
                recorder->Keep(linear);
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
                for (const auto& mip : mips) {
                    detiler.Dispatch(commands, descriptor.tileMode, elementBytes, tiled->Handle(), guestLayerOffset + mip.tiledOffset, linear->Handle(), linearLayerOffset + mip.linearOffset, mip, false, layer, geometry.thick);
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
            toTransferDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, geometry.imageLayers};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearReadBarrier, 1, &toTransferDst);

            std::vector<VkBufferImageCopy> regions;
            regions.reserve(static_cast<std::size_t>(arrayLayers) * mips.size());
            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                const auto linearLayerOffset = static_cast<std::uint64_t>(layer) * sliceLinearBytes;
                for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
                    const auto& mip = mips[level];
                    VkBufferImageCopy region{};
                    region.bufferOffset = linearLayerOffset + mip.linearOffset;
                    region.bufferRowLength = mip.pitchBytes / BytesPerElement(descriptor.format) * BlockWidth(descriptor.format);
                    region.bufferImageHeight = 0;
                    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
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
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView");
        if (profile) {
            auto& totals = Profile();
            totals.view += timer.lap();
            if (++totals.count % 200 == 0) std::fprintf(stderr, "[texture] %llu textures (%llu copied from storage images, %llu uploads recorded): allocate+image %.0f ms, guest read %.0f ms, detile+copy %.0f ms, view+buffer release %.0f ms\n", static_cast<unsigned long long>(totals.count), static_cast<unsigned long long>(totals.fromStorage), static_cast<unsigned long long>(totals.recordedUploads), totals.allocate, totals.read, totals.gpu, totals.view);
        }
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
    return descriptor.baseAddress == from.baseAddress && descriptor.width == from.width && descriptor.height == from.height && descriptor.dimension == from.dimension && descriptor.tileMode == from.tileMode && descriptor.mipCount <= from.mipCount && descriptor.depthOrLastArray == from.depthOrLastArray && BytesPerElement(descriptor.format) == BytesPerElement(from.format) && BlockWidth(descriptor.format) == BlockWidth(from.format);
}

Texture::Texture(const Context& context, const std::shared_ptr<StorageTexture>& source, const GuestTextureResource& descriptor, VkComponentMapping components) : context(context), storageSource(source) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    try {
        Require(source != nullptr && CanCopyFrom(*source, descriptor), "storage image does not match the sampled texture");
        const auto vkFormat = ResolveTextureFormat(descriptor.format);
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
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView storage view");
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

Texture::~Texture() {
    release();
}

void Texture::release() noexcept {
    upload.reset();
    if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    view = VK_NULL_HANDLE;
    // The image and its memory go with the last holder: this texture, or the batch still uploading it.
    owned.reset();
    image = VK_NULL_HANDLE;
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

VkFormat StorageFormatFor(const Context& context, VkFormat format) {
    const auto storage = StorageFormatOrUndefined(context, format);
    Require(storage != VK_FORMAT_UNDEFINED, "guest storage texture format " + std::to_string(format) + " cannot be used as a storage image");
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
        sliceLinearBytes = geometry.sliceLinearBytes;
        const auto linearBytes = sliceLinearBytes * arrayLayers;

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        // Sampled views of other same-size formats (sRGB, reinterpretations) read the image directly.
        imageInfo.flags = (descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u) | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
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
            attachable = descriptor.dimension == TextureDimension::k2D && (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
            if (attachable) imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage storage");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory storage texture");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory storage");
        upload();
        defaultMip = mipLevel;
        view = createView(mipLevel);
        if (profile) {
            auto& totals = Profile();
            totals.storageCreate += timer.lap();
            totals.storageBytes += guestBytes;
            if (++totals.storageCount % 50 == 0) std::fprintf(stderr, "[texture] %llu storage images (%.0f MiB): create %.0f ms, write-back %.0f ms (alloc %.0f, host copy %.0f, gpu %.0f, store %.0f), %llu reused, %llu direct uploads, %llu direct write-backs\n", static_cast<unsigned long long>(totals.storageCount), totals.storageBytes / 1048576.0, totals.storageCreate, totals.storageWriteBack, totals.storageAlloc, totals.storageHostCopy, totals.storageGpu, totals.storageStore, static_cast<unsigned long long>(totals.storageReused), static_cast<unsigned long long>(totals.storageDirectUploads), static_cast<unsigned long long>(totals.storageDirectWriteBacks));
        }
    } catch (...) {
        release();
        throw;
    }
}

namespace {

// Storage images whose results have not reached guest memory yet.
struct PendingWrites {
    std::mutex mutex;
    std::vector<StorageTexture*> textures;
    // Images taken out of `textures` by a FlushPending still storing them (see adjacentPendingUnchanged).
    std::vector<StorageTexture*> flushing;
};

PendingWrites& Pending() {
    static PendingWrites pending;
    return pending;
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

VkImageView StorageTexture::createView(std::uint32_t mip) const {
    Require(mip < descriptor.mipCount, "storage texture mip level is outside the texture");
    const auto viewLayerCount = geometry.imageLayers - descriptor.baseArray;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    // Storage views address one mip; cube faces are written as array layers.
    viewInfo.viewType = descriptor.dimension == TextureDimension::k1D ? VK_IMAGE_VIEW_TYPE_1D : descriptor.dimension == TextureDimension::k2D ? VK_IMAGE_VIEW_TYPE_2D : descriptor.dimension == TextureDimension::k3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = storageFormat;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1u, descriptor.baseArray, viewLayerCount};
    VkImageView created = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created), "vkCreateImageView storage");
    return created;
}

VkImageView StorageTexture::AttachmentView(VkFormat format) {
    Require(attachable, "storage image cannot be a color attachment");
    const auto found = attachmentViews.find(format);
    if (found != attachmentViews.end()) return found->second;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1u, descriptor.baseArray, 1u};
    VkImageView created = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created), "vkCreateImageView attachment");
    attachmentViews.emplace(format, created);
    return created;
}

VkImageView StorageTexture::View(std::uint32_t mip) {
    if (mip == defaultMip) return view;
    const auto found = extraViews.find(mip);
    if (found != extraViews.end()) return found->second;
    const auto created = createView(mip);
    extraViews.emplace(mip, created);
    return created;
}

bool StorageTexture::Refresh() {
    // Results of other images pending in this memory must reach it first; this image's own pending
    // results stay on the GPU, where the next dispatch wants them.
    FlushPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), this, "storage refresh");
    // `original` holds the guest bytes the image was last uploaded from or written back as; while the
    // guest memory and the DCC keys still match, the image content is current. Pages nobody wrote
    // since `generation` need no comparison.
    const auto current = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    bool unchanged = false;
    {
        struct Exempt {
            const StorageTexture* previous;
            ~Exempt() { refreshing = previous; }
        } exempt{refreshing};
        refreshing = this;
        const auto equalsOriginal = [&] {
            // Named for the [hooksync] attribution: the compare goes through the flush hook.
            const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureCompare);
            return GuestMemory::EqualsCommitted(descriptor.baseAddress, original);
        };
        unchanged = TextureClearKeys(descriptor, guestBytes) == uploadedKeys && (GuestMemory::UnchangedSince(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), generation) || (originalValid && equalsOriginal()));
    }
    if (unchanged) {
        ++Profile().storageReused;
        generation = current;
        return true;
    }
    // Debug aid: APS5_TRACE_UPLOAD names why a storage image is uploaded again.
    static const bool traceUpload = std::getenv("APS5_TRACE_UPLOAD") != nullptr;
    if (traceUpload) {
        const auto keys = TextureClearKeys(descriptor, guestBytes);
        std::fprintf(stderr, "[upload] 0x%llx+0x%llx %ux%u mips %u: %s (keys %s -> %s, originalValid %d, dirty %d)\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), descriptor.width, descriptor.height, descriptor.mipCount, keys != uploadedKeys ? "DCC keys changed" : "guest memory changed", DccKeysName(uploadedKeys), DccKeysName(keys), originalValid ? 1 : 0, dirty ? 1 : 0);
    }
    bool pendingResults = false;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        if (dirty) {
            dirty = false;
            std::erase(pending.textures, this);
            pendingResults = true;
        }
    }
    if (pendingResults) {
        // The CPU wrote this memory while GPU results were pending: as with an immediate write-back
        // followed by the CPU write, its blocks win and the results land everywhere else.
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] storage image 0x%llx: guest memory changed while GPU results were pending; keeping the CPU's blocks\n", static_cast<unsigned long long>(descriptor.baseAddress));
        writeBack(generation);
    }
    upload();
    return false;
}

void StorageTexture::upload() {
    const auto elementBytes = BytesPerElement(descriptor.format);
    const auto linearBytes = sliceLinearBytes * arrayLayers;
    original.resize(static_cast<std::size_t>(guestBytes));
    generation = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    uploadedKeys = TextureClearKeys(descriptor, guestBytes);
    VkClearColorValue clearValue{};
    if (IsDccClear(uploadedKeys) && ClearColorFor(storageFormat, uploadedKeys, clearValue)) {
        // A fast-cleared surface is cleared on the GPU; its texel memory is neither read nor filled.
        originalValid = false;
        CommandBatch batch(context);
        const auto commands = batch.Handle();
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
        batch.SubmitAndWait();
        ++version;
        return;
    }
    if (const auto* import = uploadedKeys == DccKeys::Uncompressed ? HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) : nullptr) {
        // The surface lives in host-imported memory: the detiler reads it in place, no guest bytes
        // are copied, and write tracking alone validates the image (a change re-runs this).
        originalValid = false;
        auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        detiler.BeginBatch();
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        if (recorder != nullptr) {
            commands = recorder->Commands();
            recorder->Keep(linear);
            // The image itself must outlive the recorded copy: the cache may evict it right after.
            if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
        } else {
            batch = std::make_unique<CommandBatch>(context);
            commands = batch->Handle();
        }
        const auto importOffset = descriptor.baseAddress - import->base;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            for (const auto& mip : mips) {
                detiler.Dispatch(commands, descriptor.tileMode, elementBytes, import->buffer, importOffset + geometry.GuestLayerOffset(layer) + mip.tiledOffset, linear->Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, mip, false, layer, geometry.thick);
            }
        }
        const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
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
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
        VkImageMemoryBarrier toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        if (batch) batch->SubmitAndWait();
        ++Profile().storageDirectUploads;
        ++version;
        return;
    }
    originalValid = true;
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
                for (const auto& mip : mips) {
                    detiler.Dispatch(commands, descriptor.tileMode, elementBytes, tiled.Handle(), geometry.GuestLayerOffset(layer) + mip.tiledOffset, linear.Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, mip, false, layer, geometry.thick);
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
    ++version;
}

std::vector<VkBufferImageCopy> StorageTexture::CopyRegions() const {
    std::vector<VkBufferImageCopy> regions;
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
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

void StorageTexture::MarkDirty() {
    static const bool eager = std::getenv("APS5_EAGER_WRITEBACK") != nullptr || std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
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
    if (dirty) return;
    dirty = true;
    pending.textures.push_back(this);
}

void StorageTexture::Flush() {
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        if (!dirty) return;
        dirty = false;
        std::erase(pending.textures, this);
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    writeBack(generation);
}

bool StorageTexture::FlushPending(std::uint64_t address, std::size_t bytes, const StorageTexture* except, const char* reason) {
    // The images stay alive across the scan: their last owner may be a batch's kept list, which
    // another thread releases outside the GPU mutex once the batch completed.
    std::vector<std::shared_ptr<StorageTexture>> flush;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        for (auto it = pending.textures.begin(); it != pending.textures.end();) {
            auto* texture = *it;
            if (texture != except && texture != refreshing && texture->overlaps(address, bytes)) {
                texture->dirty = false;
                if (auto alive = texture->weak_from_this().lock()) flush.push_back(std::move(alive));
                it = pending.textures.erase(it);
            } else {
                ++it;
            }
        }
        // Still pending for the adjacency rule of writeBack until each one is stored.
        for (const auto& texture : flush) pending.flushing.push_back(texture.get());
    }
    if (flush.empty()) return false;
    struct Unregister {
        const std::vector<std::shared_ptr<StorageTexture>>& flush;
        ~Unregister() {
            auto& pending = Pending();
            std::lock_guard lock(pending.mutex);
            for (const auto& texture : flush) std::erase(pending.flushing, texture.get());
        }
    } unregister{flush};
    // Debug aid: APS5_TRACE_FLUSH names what forces pending results to guest memory.
    static const bool trace = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    if (trace) {
        for (const auto& texture : flush) std::fprintf(stderr, "[flush] image 0x%llx+0x%llx for %s 0x%llx+0x%zx\n", static_cast<unsigned long long>(texture->descriptor.baseAddress), static_cast<unsigned long long>(texture->guestBytes), reason, static_cast<unsigned long long>(address), bytes);
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::exception_ptr failure;
    for (const auto& texture : flush) {
        try {
            texture->writeBack(texture->generation);
        } catch (...) {
            if (!failure) failure = std::current_exception();
        }
    }
    if (failure) std::rethrow_exception(failure);
    return true;
}

void StorageTexture::FlushAllPending(const char* reason) {
    static_cast<void>(FlushPending(0, std::numeric_limits<std::size_t>::max(), nullptr, reason));
}

std::shared_ptr<StorageTexture> StorageTexture::FindPending(std::uint64_t address, std::uint64_t bytes) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    for (auto* texture : pending.textures) {
        // Containment, not equality: a descriptor of a chain's first mips (its own guestBytes are
        // shorter) is served by the chain's image; CanCopyFrom then checks the geometry.
        if (texture->descriptor.baseAddress == address && texture->guestBytes >= bytes) return texture->weak_from_this().lock();
    }
    return nullptr;
}

bool PendingStorageOverlaps(std::uint64_t address, std::size_t bytes, const StorageTexture* except) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    for (const auto* texture : pending.textures) {
        // A free function (declared in ShaderResources.hpp): the overlap is computed from the public
        // surface description rather than the private helper.
        const auto begin = texture->Descriptor().baseAddress;
        if (texture != except && address < begin + texture->GuestBytes() && begin < address + bytes) return true;
    }
    return false;
}

void StorageTexture::WriteBack() {
    writeBack(generation);
}

// See AdjacentGenerationEnabled above.
std::vector<StorageTexture::Adjacent> StorageTexture::adjacentPendingUnchanged() const {
    std::vector<Adjacent> adjacent;
    if (!AdjacentGenerationEnabled() || guestBytes == 0) return adjacent;
    constexpr std::uint64_t block = 65536;
    const auto firstBlock = descriptor.baseAddress / block;
    const auto lastBlock = (descriptor.baseAddress + guestBytes - 1) / block;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        const auto consider = [&](StorageTexture* texture) {
            if (texture == this || texture->guestBytes == 0 || texture->overlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) return;
            const auto begin = texture->descriptor.baseAddress;
            if (begin / block > lastBlock || (begin + texture->guestBytes - 1) / block < firstBlock) return;
            if (auto alive = texture->weak_from_this().lock()) adjacent.push_back({std::move(alive), texture->generation});
        };
        for (auto* texture : pending.textures) consider(texture);
        for (auto* texture : pending.flushing) consider(texture);
    }
    // Only an image whose memory has not changed since its generation can be advanced: the collect
    // stamps the CPU's writes so far, and a later one lands at a newer generation either way.
    std::erase_if(adjacent, [](const Adjacent& entry) {
        const auto& texture = entry.texture;
        GuestMemory::CollectWritesUncached(texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes));
        return !GuestMemory::UnchangedSince(texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes), texture->generation);
    });
    return adjacent;
}

void StorageTexture::advanceAdjacent(const std::vector<Adjacent>& adjacent, std::uint64_t now, std::uint64_t firstBlock, std::uint64_t lastBlock) {
    static const bool trace = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    constexpr std::uint64_t block = 65536;
    for (const auto& [texture, seen] : adjacent) {
        // An image stored or refreshed meanwhile set its own generation (an old one on purpose when
        // it kept blocks for the CPU): only the value seen at the check is advanced.
        if (texture->generation != seen || now <= seen) continue;
        // A CPU write to the neighbour landing since the check may have been stamped by another
        // thread's walk of its range at a value in (seen, now]; the advance would hide it. Its blocks
        // outside this write-back's span carry no stamp of ours, so they are re-checked against the
        // seen generation (a stamp scan, no walk). A write inside the shared block itself in that
        // window is indistinguishable from this write-back's stamp, as it is for the block rule.
        const auto begin = texture->descriptor.baseAddress;
        const auto end = begin + texture->guestBytes;
        const auto beforeEnd = std::min(end, firstBlock * block);
        const auto afterBegin = std::max(begin, (lastBlock + 1) * block);
        if (begin < beforeEnd && !GuestMemory::UnchangedSince(begin, static_cast<std::size_t>(beforeEnd - begin), seen)) continue;
        if (afterBegin < end && !GuestMemory::UnchangedSince(afterBegin, static_cast<std::size_t>(end - afterBegin), seen)) continue;
        if (trace) std::fprintf(stderr, "[flush] adjacent pending image 0x%llx+0x%llx advanced past a write-back's stamps (generation %llu -> %llu)\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(texture->guestBytes), static_cast<unsigned long long>(seen), static_cast<unsigned long long>(now));
        texture->generation = now;
    }
}

void StorageTexture::writeBack(std::uint64_t skipWrittenSince) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    struct Account {
        bool enabled;
        PhaseTimer& timer;
        ~Account() { if (enabled) Profile().storageWriteBack += timer.lap(); }
    } account{profile, timer};
    const auto elementBytes = BytesPerElement(descriptor.format);
    // 64 KiB blocks the CPU wrote since the image was last in sync keep the CPU's bytes: the game may
    // have reused the memory for something else entirely (see skipWrittenSince). Zero means no
    // tracking, and everything is stored.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> keep;
    bool skippedAny = false;
    std::size_t skipped = 0;
    {
        const auto begin = descriptor.baseAddress;
        const auto end = begin + guestBytes;
        if (skipWrittenSince != 0) {
            constexpr std::uint64_t block = 65536;
            // The keep-blocks decision must see every CPU write up to now, so it bypasses the
            // per-packet collect memo (a Refresh collect earlier in the same packet would satisfy it).
            GuestMemory::CollectWritesUncached(begin, static_cast<std::size_t>(guestBytes));
            for (auto at = begin & ~(block - 1); at < end; at += block) {
                const auto from = std::max(at, begin);
                const auto to = std::min(at + block, end);
                if (from >= to) continue;
                if (!GuestMemory::UnchangedSince(from, static_cast<std::size_t>(to - from), skipWrittenSince)) {
                    skippedAny = true;
                    ++skipped;
                    continue;
                }
                if (!keep.empty() && keep.back().second == from) keep.back().second = to;
                else keep.emplace_back(from, to);
            }
        } else {
            keep.emplace_back(begin, end);
        }
    }
    // Debug aid: APS5_TRACE_FLUSH also names the blocks a write-back leaves to the CPU.
    static const bool traceKept = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    if (traceKept && skippedAny) {
        const auto first = keep.empty() ? descriptor.baseAddress + guestBytes : keep.front().first;
        std::fprintf(stderr, "[flush] image 0x%llx+0x%llx keeps %zu CPU-written 64 KiB blocks (first stored byte at +0x%llx, %zu ranges stored, generation %llu)\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), skipped, static_cast<unsigned long long>(first - descriptor.baseAddress), keep.size(), static_cast<unsigned long long>(skipWrittenSince));
    }
    // Taken before this write-back stamps anything (see advanceAdjacent), with the 64 KiB block span
    // its stamps cover.
    const auto adjacent = adjacentPendingUnchanged();
    const auto firstBlock = descriptor.baseAddress / 65536;
    const auto lastBlock = guestBytes == 0 ? firstBlock : (descriptor.baseAddress + guestBytes - 1) / 65536;
    if (const auto* import = HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
        // The surface lives in host-imported memory: the retiler writes into device scratch and the
        // untouched blocks are copied into the imported bytes in place, recorded behind the work that
        // produced the image; nothing crosses to the CPU.
        auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(sliceLinearBytes * arrayLayers), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        auto tiledScratch = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(guestBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        detiler.BeginBatch();
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        if (recorder != nullptr) {
            commands = recorder->Commands();
            recorder->Keep(linear);
            recorder->Keep(tiledScratch);
            // The image itself must outlive the recorded retile: the cache may evict it right after.
            if (auto self = weak_from_this().lock()) recorder->Keep(std::move(self));
        } else {
            batch = std::make_unique<CommandBatch>(context);
            commands = batch->Handle();
        }
        const auto importOffset = descriptor.baseAddress - import->base;
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
        const auto regions = CopyRegions();
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear->Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
        const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        const VkMemoryBarrier importReady{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &importReady, 1, &linearRead, 0, nullptr);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            for (const auto& mip : mips) {
                detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear->Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, tiledScratch->Handle(), geometry.GuestLayerOffset(layer) + mip.tiledOffset, mip, true, layer, geometry.thick);
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
        if (batch) batch->SubmitAndWait();
        else recorder->NotePendingWrite(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
        if (profile) Profile().storageGpu += timer.lap();
        // The guest bytes now differ from `original`; other caches of the range see the write. With
        // every block stored the image is current at a fresh generation; with blocks kept for the CPU
        // the image is stale there, so the old generation stays and the next use re-uploads.
        originalValid = false;
        // The keys go the same way as the texels: a fill recorded behind the retile when the
        // metadata is host-imported (no CPU wait for the title's key-writing kernels), else a CPU
        // store (APS5_CPU_DCC_KEYS=1 keeps the CPU store; see DccMetadata.hpp).
        MarkDccUncompressed(context, descriptor.dccAddress, guestBytes);
        uploadedKeys = DccKeys::Uncompressed;
        for (const auto& [from, to] : keep) GuestMemory::MarkWritten(from, static_cast<std::size_t>(to - from));
        if (!skippedAny || !adjacent.empty()) {
            // The walk covers this surface's pages only: an adjacent image's later CPU write is
            // stamped newer than this value when its own range is collected. No CPU wrote the pages
            // here (MarkWritten made the only stamps), so the memoized collect is exact.
            const auto now = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
            if (!skippedAny) generation = now;
            advanceAdjacent(adjacent, now, firstBlock, lastBlock);
        }
        ++Profile().storageDirectWriteBacks;
        return;
    }
    if (!originalValid) {
        // The image was cleared on the GPU without reading the guest bytes; the store below compares
        // against them, so read them now.
        GuestMemory::ReadCommitted(descriptor.baseAddress, original);
        originalValid = true;
    }
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
        for (const auto& mip : mips) {
            detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear.Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, tiled.Handle(), geometry.GuestLayerOffset(layer) + mip.tiledOffset, mip, true, layer, geometry.thick);
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
    // Store only blocks that changed so concurrent CPU writes to untouched texels survive.
    const auto current = host.Bytes();
    if (skipWrittenSince == 0) {
        GuestMemory::WriteChangedCommitted(descriptor.baseAddress, current, original);
    } else {
        constexpr std::uint64_t block = 65536;
        const auto begin = descriptor.baseAddress;
        const auto end = begin + guestBytes;
        for (auto at = begin & ~(block - 1); at < end; at += block) {
            const auto from = std::max(at, begin);
            const auto to = std::min(at + block, end);
            if (from >= to || !GuestMemory::UnchangedSince(from, static_cast<std::size_t>(to - from), skipWrittenSince)) continue;
            const auto offset = static_cast<std::size_t>(from - begin);
            const auto length = static_cast<std::size_t>(to - from);
            GuestMemory::WriteChangedCommitted(from, current.subspan(offset, length), std::span<const std::byte>(original).subspan(offset, length));
        }
    }
    std::memcpy(original.data(), current.data(), original.size());
    // The texels now hold the whole image, so later reads must see them rather than a fast clear
    // (the keys may be host-imported although the texels were not: then a recorded fill, else a CPU store).
    MarkDccUncompressed(context, descriptor.dccAddress, guestBytes);
    uploadedKeys = DccKeys::Uncompressed;
    // The store above is the only write to these pages, so `original` is current at a fresh
    // generation, unless blocks were kept for the CPU: then the image is stale there and the old
    // generation stays so the next use re-uploads.
    if (skippedAny) originalValid = false;
    if (!skippedAny || !adjacent.empty()) {
        // As in the GPU-direct path, but the store's memcpy dirtied this surface's pages in the write
        // watch: a memoized collect would leave them for the next walk to stamp newer than this value
        // (the shared edge block included, undoing the advance), so the walk is made here and
        // consumes them.
        const auto now = GuestMemory::CollectWritesUncached(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
        if (!skippedAny) generation = now;
        advanceAdjacent(adjacent, now, firstBlock, lastBlock);
    }
    if (profile) Profile().storageStore += timer.lap();
}

StorageTexture::~StorageTexture() {
    // Cache eviction flushes first; anything still pending here is being torn down with the device.
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        if (dirty) {
            dirty = false;
            std::erase(pending.textures, this);
            static std::atomic<int> reports{0};
            if (reports.fetch_add(1) < 4) std::fprintf(stderr, "[gpu] storage image 0x%llx destroyed with GPU results pending\n", static_cast<unsigned long long>(descriptor.baseAddress));
        }
    }
    release();
}

void StorageTexture::release() noexcept {
    for (const auto& [mip, extra] : extraViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, extra, nullptr);
    extraViews.clear();
    for (const auto& [format, attachment] : attachmentViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, attachment, nullptr);
    attachmentViews.clear();
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
