#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
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

class DepthSurface {
public:
    DepthSurface(const Context& context, const DepthTarget& target) : context(context), target(target), hostFormat(HostDepthFormat(context, target.format)) {
        this->context.bufferPool.reset();
        VkFormatProperties properties{};
        context.formatProperties(context.physical, hostFormat, &properties);
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0, "depth/stencil format " + std::to_string(hostFormat) + " cannot be an attachment on this device");
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0, "depth/stencil format " + std::to_string(hostFormat) + " cannot be sampled on this device");
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
            VkImageFormatProperties sampledProperties{};
            sampleable = target.samples == 1 || (context.imageFormatProperties(context.physical, hostFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, info.usage | VK_IMAGE_USAGE_SAMPLED_BIT, info.flags, &sampledProperties) == VK_SUCCESS && (sampledProperties.sampleCounts & info.samples) != 0);
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
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = hostFormat;
            viewInfo.subresourceRange = {aspects, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth");
            auto* recorder = RecorderFor(context);
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

    void ApplyFastClear() {
        const auto aspects = pendingClear & (VK_IMAGE_ASPECT_DEPTH_BIT | (target.stencilAddress != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u));
        pendingClear = 0;
        if (aspects == 0) return;
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
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
    }

    std::shared_ptr<Texture> Sampled(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components) {
        std::array<std::uint32_t, 12> key{};
        std::copy_n(words.begin(), std::min<std::size_t>(words.size(), 8), key.begin());
        key[8] = components.r;
        key[9] = components.g;
        key[10] = components.b;
        key[11] = components.a;
        if (const auto found = textures.find(key); found != textures.end()) return found->second;
        Require(target.samples == 1, "sampling a multisampled depth surface is not implemented");
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const auto format = ResolveTextureFormat(resource.format);
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        const bool oneSlice = resource.dimension == TextureDimension::k2D || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray == 0);
        if ((format != expected && !depthBits) || !oneSlice || resource.width != target.extent.width || resource.height != target.extent.height || resource.baseLevel != 0 || resource.lastLevel != 0 || resource.baseArray != 0) {
            char text[448];
            std::snprintf(text, sizeof(text), "AGC graphics: sampling the %s plane of depth surface 0x%llx (%ux%u, vk format %d) as a %ux%u texture of guest format %u (vk %d), tile mode %u, dimension %d, levels %u-%u, slice %u is not implemented (T# %08x %08x %08x %08x %08x %08x %08x %08x)",
                          stencil ? "stencil" : "depth", static_cast<unsigned long long>(target.address), target.extent.width, target.extent.height, static_cast<int>(target.format), resource.width, resource.height, resource.format, static_cast<int>(format),
                          static_cast<unsigned>(resource.tileMode), static_cast<int>(resource.dimension), resource.baseLevel, resource.lastLevel, resource.baseArray, key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
            throw std::runtime_error(text);
        }
        const auto viewType = resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        auto texture = std::make_shared<Texture>(context, image, hostFormat, stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, components, viewType);
        textures.emplace(key, texture);
        return texture;
    }

    void CopySample(std::uint32_t sample, StorageTexture& destination, VkImageView destinationView) {
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "depth sample copies need an active recorder");
        Require(sampleable, "DB depth copy from a depth surface the device cannot sample at its sample count");
        ApplyFastClear();
        if (depthView == VK_NULL_HANDLE) {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = hostFormat;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &depthView), "vkCreateImageView depth sample copy");
        }
        Require(context.detiler != nullptr, "depth sample copies need the texture detiler");
        context.detiler->DispatchDepthSample(recorder->Commands(), depthView, destinationView, target.extent.width, target.extent.height, sample);
        Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
        destination.MarkDirty();
    }

    bool Holds(const Texture* texture) const {
        return std::any_of(textures.begin(), textures.end(), [&](const auto& entry) { return entry.second.get() == texture; });
    }

    std::uint64_t written = ++depthWrites;

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
        copyPlane(plane, stencil, true);
        return plane.storage;
    }

    bool StoreBack(const StorageTexture& candidate) {
        for (const bool stencil : {false, true}) {
            auto& plane = stencil ? stencilPlane : depthPlane;
            if (plane.storage.get() == &candidate) {
                copyPlane(plane, stencil, false);
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

    const Context context;
    const DepthTarget target;
    const VkFormat hostFormat;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    bool sampleable = false;

private:
    struct PlaneStorage {
        std::shared_ptr<StorageTexture> storage;
        std::vector<std::weak_ptr<StorageTexture>> replaced;
        std::unique_ptr<DeviceBuffer> staging;
    };
    std::map<std::array<std::uint32_t, 12>, std::shared_ptr<Texture>> textures;
    PlaneStorage depthPlane;
    PlaneStorage stencilPlane;

    void copyPlane(PlaneStorage& plane, bool stencil, bool toStorage) {
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "depth surface storage copies need an active recorder");
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto bytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * (stencil ? 1u : d16 ? 2u : 4u);
        if (plane.staging == nullptr) plane.staging = std::make_unique<DeviceBuffer>(context, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder->Commands();
        recorder->Keep(plane.storage);
        const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
        VkBufferImageCopy planeRegion{0, 0, 0, {stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1}, {0, 0, 0}, {target.extent.width, target.extent.height, 1}};
        VkBufferImageCopy colorRegion = planeRegion;
        colorRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        const auto sourceImage = toStorage ? image : plane.storage->Image();
        const auto destinationImage = toStorage ? plane.storage->Image() : image;
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
        textures.clear();
        if (depthView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, depthView, nullptr);
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
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
            staging = std::make_unique<DeviceBuffer>(context, static_cast<std::size_t>(sliceBytes()) * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        } catch (...) {
            release();
            throw;
        }
    }
    ~DepthPlaneCopy() { release(); }
    DepthPlaneCopy(const DepthPlaneCopy&) = delete;
    DepthPlaneCopy& operator=(const DepthPlaneCopy&) = delete;

    std::shared_ptr<Texture> Refresh(std::span<const DepthPlaneSlice> slices, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType viewType) {
        Require(slices.size() == layers, "depth plane copy slices do not match its layers");
        const auto geometry = DescribeSurface(resource);
        Require(geometry.layers == layers && geometry.sliceLinearBytes == sliceBytes() && !geometry.mips.empty(), "depth plane copy geometry does not match its layers");
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
        const std::array<std::uint32_t, 5> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a), static_cast<std::uint32_t>(viewType)};
        auto& texture = textures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, image, format, aspect(), components, viewType);
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
    std::map<std::array<std::uint32_t, 5>, std::shared_ptr<Texture>> textures;
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
        const auto region = [&](std::uint32_t layer, VkImageAspectFlags aspectMask, std::uint32_t imageLayer) {
            VkBufferImageCopy copy{};
            copy.bufferOffset = sliceBytes() * layer;
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
    LayeredDepthStorage(const Context& context, const GuestTextureResource& resource, const DepthTarget& base, std::uint32_t bytesPerTexel, std::uint32_t layers) : context(context), base(base), extent(base.extent), bytesPerTexel(bytesPerTexel), layers(layers) {
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
    const std::uint32_t layers;
    std::shared_ptr<StorageTexture> storage;

private:
    std::unique_ptr<DeviceBuffer> staging;

    VkDeviceSize sliceBytes() const {
        return static_cast<VkDeviceSize>(extent.width) * extent.height * bytesPerTexel;
    }
};

std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, int, int>, std::unique_ptr<LayeredDepthStorage>>& layeredStorages() {
    static auto* storages = new std::map<std::tuple<VkDevice, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, int, int>, std::unique_ptr<LayeredDepthStorage>>();
    return *storages;
}

bool sameSurface(const DepthTarget& a, const DepthTarget& b) {
    return a.address == b.address && a.stencilAddress == b.stencilAddress && a.extent.width == b.extent.width && a.extent.height == b.extent.height && a.format == b.format && a.samples == b.samples;
}

std::mutex& surfacesMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::unique_ptr<DepthSurface>>& surfaces() {
    static auto* list = new std::vector<std::unique_ptr<DepthSurface>>();
    return *list;
}

std::set<std::uint64_t>& clearedHtiles() {
    static auto* cleared = new std::set<std::uint64_t>();
    return *cleared;
}

const DepthSurface* drawnDepth = nullptr;

}

std::uint64_t DepthSliceBytes(VkExtent2D extent, std::uint32_t bytesPerTexel) {
    const std::uint32_t blockWidth = bytesPerTexel == 4 ? 128u : 256u;
    const std::uint32_t blockHeight = bytesPerTexel == 1 ? 256u : 128u;
    const auto width = static_cast<std::uint64_t>((extent.width + blockWidth - 1) / blockWidth * blockWidth);
    const auto height = static_cast<std::uint64_t>((extent.height + blockHeight - 1) / blockHeight * blockHeight);
    return width * height * bytesPerTexel;
}

bool SampleLocationsCompatibleDepth(const Context& context, VkFormat format, std::uint32_t samples) {
    if (samples <= 1 || (context.sampleLocationSampleCounts & samples) == 0) return false;
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
    std::lock_guard lock(surfacesMutex());
    const bool cleared = target.htileAddress != 0 && clearedHtiles().erase(target.htileAddress) != 0;
    for (const auto& surface : surfaces()) {
        if (surface->context.device != context.device || !sameSurface(surface->target, target)) continue;
        surface->clearDepth = target.clearDepth;
        surface->clearStencil = target.clearStencil;
        if (cleared) surface->pendingClear |= VK_IMAGE_ASPECT_DEPTH_BIT;
        surface->ApplyFastClear();
        surface->written = ++depthWrites;
        drawnDepth = surface.get();
        return surface->view;
    }
    surfaces().push_back(std::make_unique<DepthSurface>(context, target));
    surfaces().back()->clearDepth = target.clearDepth;
    surfaces().back()->clearStencil = target.clearStencil;
    drawnDepth = surfaces().back().get();
    return surfaces().back()->view;
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

void NoteHtileDepthClear(std::uint64_t htileAddress) {
    std::lock_guard lock(surfacesMutex());
    bool existing = false;
    for (const auto& surface : surfaces()) {
        if (surface->target.htileAddress != htileAddress) continue;
        surface->pendingClear |= VK_IMAGE_ASPECT_DEPTH_BIT;
        existing = true;
    }
    if (!existing) clearedHtiles().insert(htileAddress);
}

void ClearDepthSurfaces(VkDevice device) {
    std::lock_guard lock(surfacesMutex());
    drawnDepth = nullptr;
    std::erase_if(surfaces(), [&](const auto& surface) { return surface->context.device == device; });
    std::erase_if(planeCopies(), [&](const auto& entry) { return std::get<0>(entry.first) == device; });
    std::erase_if(layeredStorages(), [&](const auto& entry) { return std::get<0>(entry.first) == device; });
}

std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components) {
    const auto held = [&](const auto& surface) {
        return surface->context.device == context.device && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    };
    {
        std::lock_guard lock(surfacesMutex());
        if (std::none_of(surfaces().begin(), surfaces().end(), held)) return nullptr;
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
        return surface->context.device == context.device && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    });
    if (found == list.rend()) return nullptr;
    const auto& base = (*found)->target;
    if (resource.width != base.extent.width || resource.height != base.extent.height) return nullptr;
    const bool stencil = base.stencilAddress != 0 && resource.baseAddress == base.stencilAddress;
    const bool d16 = base.format == VK_FORMAT_D16_UNORM || base.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto format = ResolveTextureFormat(resource.format);
    const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
    const bool integer = d16 && format == VK_FORMAT_R16_UINT && !depthBits;
    if (!stencil && format != (d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT) && !depthBits && !integer) return nullptr;
    if (stencil && BytesPerElement(resource.format) != 1u) return nullptr;
    (*found)->ApplyFastClear();
    const bool cube = resource.dimension == TextureDimension::kCube;
    const bool layered = cube || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray != 0);
    if (stencil || (!layered && !integer)) return (*found)->Sampled(words, resource, components);
    Require((*found)->hostFormat == base.format, "sampling several slices or an integer view of a depth plane that the device stores in another depth format is not implemented");
    const auto copyFormat = integer ? VK_FORMAT_R16_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
    const auto layers = cube ? 6u : layered ? resource.depthOrLastArray + 1u : 1u;
    if ((format != copyFormat && !depthBits) || resource.width != base.extent.width || resource.height != base.extent.height || resource.baseLevel != 0 || resource.lastLevel != 0 || resource.baseArray != 0 || (cube && resource.depthOrLastArray != 0 && resource.depthOrLastArray != 5) || (resource.dimension != TextureDimension::k2D && resource.dimension != TextureDimension::k2DArray && !cube)) {
        char text[320];
        std::snprintf(text, sizeof(text), "AGC graphics: sampling the depth planes of depth surface 0x%llx (%ux%u, vk format %d) as a %ux%u texture of guest format %u (vk %d), dimension %d, %u slices, levels %u-%u, base slice %u is not implemented", static_cast<unsigned long long>(base.address), base.extent.width, base.extent.height, static_cast<int>(base.format), resource.width, resource.height, resource.format, static_cast<int>(format), static_cast<int>(resource.dimension), layers, resource.baseLevel, resource.lastLevel, resource.baseArray);
        throw std::runtime_error(text);
    }
    std::vector<DepthPlaneSlice> slices;
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        const auto address = base.address + static_cast<std::uint64_t>(layer) * DepthSliceBytes(base.extent, d16 ? 2u : 4u);
        const auto slice = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) { return surface->context.device == context.device && surface->target.address == address && surface->target.extent.width == base.extent.width && surface->target.extent.height == base.extent.height && surface->target.format == base.format; });
        if (slice != list.rend()) (*slice)->ApplyFastClear();
        if (slice == list.rend()) slices.push_back({});
        else slices.push_back({(*slice)->image, slice->get() == drawnDepth ? 0u : (*slice)->written});
    }
    const auto imageFormat = integer ? VK_FORMAT_R16_UINT : d16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
    auto& copy = planeCopies()[{context.device, base.address, base.extent.width, base.extent.height, imageFormat, layers}];
    if (copy == nullptr) {
        copy = std::make_unique<DepthPlaneCopy>(context, base.extent, imageFormat, layers);
    }
    return copy->Refresh(slices, resource, components, cube || resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
}

bool DepthSurfaceHolds(const Context& context, const GuestTextureResource& resource, const Texture* texture) {
    if (resource.dimension != TextureDimension::k2D || resource.baseArray != 0) return false;
    const auto address = resource.baseAddress;
    std::lock_guard lock(surfacesMutex());
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
        return surface->context.device == context.device && (surface->target.address == address || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == address));
    });
    return found != list.rend() && (*found)->Holds(texture);
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
    for (const auto& surface : surfaces()) {
        const auto& target = surface->target;
        if (!HtileFillCovers(target.htileAddress, target.extent, address, bytes)) continue;
        const VkImageAspectFlags written = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
        surface->pendingClear = (surface->pendingClear & ~written) | HtileFillClears(pattern, target.htileStencil);
    }
}

bool DepthSurfaceAt(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource) {
    std::lock_guard lock(surfacesMutex());
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
        return surface->context.device == context.device && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    });
    if (found == list.rend()) return false;
    const auto& target = (*found)->target;
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
    for (std::uint32_t layer = 0; layer < storage.layers; ++layer) {
        const auto address = base.address + static_cast<std::uint64_t>(layer) * DepthSliceBytes(base.extent, storage.bytesPerTexel);
        const auto slice = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) { return surface->context.device == context.device && surface->target.address == address && surface->target.extent.width == base.extent.width && surface->target.extent.height == base.extent.height && surface->target.format == base.format && surface->target.samples == 1; });
        if (slice != list.rend()) slices.push_back({slice->get(), layer});
    }
    return slices;
}

std::shared_ptr<StorageTexture> layeredDepthStorage(const Context& context, DepthSurface& surface, const GuestTextureResource& resource, std::uint32_t mip) {
    const auto& target = surface.target;
    const auto layers = resource.depthOrLastArray + 1u;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto bytesPerTexel = d16 ? 2u : 4u;
    if (resource.baseArray != 0) {
        char text[256];
        std::snprintf(text, sizeof(text), "AGC graphics: storage image access to the depth planes of depth surface 0x%llx through a 2D array view of slices %u-%u is not implemented", static_cast<unsigned long long>(target.address), resource.baseArray, resource.depthOrLastArray);
        throw std::runtime_error(text);
    }
    surface.RequireStorable(resource, mip);
    const auto geometry = DescribeSurface(resource);
    if (layers > 1 && geometry.GuestLayerOffset(1) != DepthSliceBytes(target.extent, bytesPerTexel)) {
        char text[256];
        std::snprintf(text, sizeof(text), "AGC graphics: storage image access to the depth planes of depth surface 0x%llx with a layer pitch of 0x%llx bytes, not the depth slice size 0x%llx, is not implemented", static_cast<unsigned long long>(target.address), static_cast<unsigned long long>(geometry.GuestLayerOffset(1)), static_cast<unsigned long long>(DepthSliceBytes(target.extent, bytesPerTexel)));
        throw std::runtime_error(text);
    }
    auto& entry = layeredStorages()[{context.device, resource.baseAddress, resource.width, resource.height, layers, resource.format, static_cast<int>(resource.tileMode), static_cast<int>(resource.dimension)}];
    if (entry == nullptr) entry = std::make_unique<LayeredDepthStorage>(context, resource, target, bytesPerTexel, layers);
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
        return surface->context.device == context.device && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    });
    if (found == list.rend()) return nullptr;
    auto& surface = **found;
    const bool stencil = surface.target.stencilAddress != 0 && surface.target.stencilAddress == resource.baseAddress;
    if (stencil || resource.dimension != TextureDimension::k2DArray || (resource.baseArray == 0 && resource.depthOrLastArray == 0)) return surface.Storage(words, resource, mip);
    if (!surface.StorageViewMatches(words, resource)) return nullptr;
    return layeredDepthStorage(context, surface, resource, mip);
}

void CopyDepthSample(const Context& context, std::uint64_t depthAddress, std::uint32_t sample, StorageTexture& destination, VkImageView destinationView, VkExtent2D extent) {
    std::lock_guard lock(surfacesMutex());
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) { return surface->context.device == context.device && surface->target.address == depthAddress; });
    Require(found != list.rend(), "DB depth copy from a depth plane no draw rendered");
    Require((*found)->target.extent.width == extent.width && (*found)->target.extent.height == extent.height, "DB depth copy from a depth surface of another size");
    Require(sample < (*found)->target.samples, "DB depth copy of a sample the depth surface does not have");
    (*found)->CopySample(sample, destination, destinationView);
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
        if (surface->context.device == context.device && surface->StoreBack(storage)) return true;
    }
    return false;
}

bool DepthSurfaceAt(std::uint64_t address) {
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->target.address == address || surface->target.stencilAddress == address; });
}

}
