#include <shared_mutex>
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/FirstSampleCopy.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {

std::uint32_t MultisampledBlockLog2(ColorTileMode mode) {
    switch (mode) {
        case ColorTileMode::Standard4KB: return 12u;
        case ColorTileMode::Standard64KB: case ColorTileMode::Z64KBX: case ColorTileMode::D64KBX: case ColorTileMode::RenderTarget: return 16u;
        default: return 0u;
    }
}

std::uint64_t collectBlocks(std::uint64_t address, std::size_t bytes) {
    constexpr std::uint64_t block = 65536;
    const auto first = address & ~(block - 1);
    const auto stop = (address + bytes + block - 1) & ~(block - 1);
    const auto generation = GuestMemory::CollectWritesUncached(first, static_cast<std::size_t>(stop - first));
    return generation != 0 ? generation : GuestMemory::CollectWritesUncached(address, bytes);
}

GuestTextureResource fmaskSnapshotResource(const GuestTextureResource& resource) {
    Require(resource.format == static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format32UInt) && resource.fmaskCmaskAddress != 0u, "FMASK image format read semantics are not implemented");
    Require(resource.dimension == TextureDimension::k2D || resource.dimension == TextureDimension::k2DArray, "FMASK image dimension is not implemented");
    Require(resource.tileMode == TextureTileMode::kZ64KBX && (resource.pipeBankXor & ~0xcf00u) == 0u, "FMASK image swizzle or pipe/bank XOR is not implemented");
    Require(resource.width != 0u && resource.width <= 16384u && resource.height != 0u && resource.height <= 16384u && resource.depthOrLastArray < 8192u && resource.baseArray <= resource.depthOrLastArray, "invalid FMASK image extent or layer range");
    Require(resource.dimension == TextureDimension::k2DArray || (resource.depthOrLastArray == 0u && resource.baseArray == 0u), "FMASK 2D image has an array range");
    Require(resource.mipCount == 1u && resource.allocatedMipCount == 1u && resource.baseLevel == 0u && resource.lastLevel == 0u && resource.minLod == 0u, "FMASK image mip fields are not implemented");
    Require(resource.fmaskCmaskAddress == 0u || resource.dccPipeAligned, "FMASK image CMASK must be pipe aligned");
    for (const auto selector : {resource.dstSelX, resource.dstSelY, resource.dstSelZ, resource.dstSelW}) Require(selector == 0u || selector == 1u || selector == 4u, "FMASK image selects an unverified channel");
    auto snapshot = resource;
    snapshot.format = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format32UInt);
    snapshot.tileMode = TextureTileMode::kLinear;
    snapshot.pipeBankXor = 0u;
    snapshot.dccAddress = 0u;
    snapshot.fmaskCmaskAddress = 0u;
    snapshot.dccAlphaOnMsb = false;
    snapshot.dccPipeAligned = false;
    return snapshot;
}

GuestTextureResource fmaskResource(const ColorTarget& target) {
    GuestTextureResource resource{};
    resource.baseAddress = target.fmaskAddress;
    resource.width = target.extent.width;
    resource.height = target.extent.height;
    resource.mipCount = 1u;
    resource.allocatedMipCount = 1u;
    resource.tileMode = TextureTileMode::kZ64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format32UInt);
    resource.dstSelX = 4u;
    resource.pipeBankXor = target.fmaskPipeBankXor;
    resource.fmaskCmaskAddress = target.cmaskAddress;
    resource.dccPipeAligned = true;
    return resource;
}

bool sameTarget(const ColorTarget& a, const ColorTarget& b) {
    return a.surfaceAddress == b.surfaceAddress && a.format == b.format && a.extent.width == b.extent.width && a.extent.height == b.extent.height && a.samples == b.samples;
}

constexpr VkImageUsageFlags twinViewUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

VkFormat colorTwinFormat(const Context& context, VkFormat format) {
    if (const auto srgb = SrgbTwinFormat(context, format); srgb != VK_FORMAT_UNDEFINED) return srgb;
    for (const auto unorm : {VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_A8B8G8R8_UNORM_PACK32}) {
        if (SrgbTwinFormat(context, unorm) == format) return unorm;
    }
    return VK_FORMAT_UNDEFINED;
}

bool recreated(const ColorTarget& resident, const ColorTarget& use) {
    return use.dccAddress != 0 && use.pipeBankXor != resident.pipeBankXor && use.address == resident.address && use.dccAddress == resident.dccAddress && use.dccControl == resident.dccControl && use.dccAlphaOnMsb == resident.dccAlphaOnMsb && use.dccPipeAligned == resident.dccPipeAligned && use.cmaskAddress == resident.cmaskAddress && use.cmaskBytes == resident.cmaskBytes && use.tileMode == resident.tileMode && use.elementBytes == resident.elementBytes;
}

VkClearColorValue fastClearColor(const ColorTarget& target) {
    Require(target.elementBytes != 0 && target.elementBytes <= sizeof(target.clearWords) && 16u % target.elementBytes == 0, "the fast clear of a multisampled target over 64-bit texels is not modeled");
    std::array<std::byte, 16> texels{};
    for (std::size_t offset = 0; offset < texels.size(); offset += target.elementBytes) std::memcpy(texels.data() + offset, target.clearWords.data(), target.elementBytes);
    std::array<std::uint32_t, 4> pattern{};
    std::memcpy(pattern.data(), texels.data(), texels.size());
    VkClearColorValue clear{};
    Require(ClearColorForTexel(target.format, target.elementBytes, pattern, clear), "the fast-clear color of a multisampled target has no exact clear value in its format");
    return clear;
}

template<typename Record>
void recordCommands(const Context& context, Record&& record) {
    auto* recorder = Recorder::Active();
    if (recorder != nullptr && recorder->Device() != context.device) recorder = nullptr;
    std::unique_ptr<CommandBatch> batch;
    if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
    record(recorder != nullptr ? recorder->Commands() : batch->Handle(), recorder);
    if (batch) batch->SubmitAndWait();
}

class MultisampleTarget : public std::enable_shared_from_this<MultisampleTarget> {
public:
    MultisampleTarget(const Context& context, const ColorTarget& target) : context(context), target(target), cmask(target.cmaskAddress, target.cmaskBytes), colorFill(target) {
        this->context.bufferPool.reset();
        if (target.dccAddress != 0) metadataValid = std::make_shared<std::atomic<bool>>(true);
        const auto initial = initialDccClear(target);
        const auto samples = static_cast<VkSampleCountFlagBits>(target.samples);
        Require((context.limits.framebufferColorSampleCounts & samples) != 0, "the device cannot render color with the target's sample count");
        VkFormatProperties properties{};
        context.formatProperties(context.physical, target.format, &properties);
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0, "multisampled render-target format cannot be a color attachment");
        usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        orderFormat = MayReorderSamples(context, target.samples) ? SampleOrderStorageFormat(context, target.format, target.elementBytes, target.samples) : VK_FORMAT_UNDEFINED;
        const VkImageCreateFlags orderFlags = orderFormat != VK_FORMAT_UNDEFINED ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT : 0u;
        if (orderFormat != VK_FORMAT_UNDEFINED) usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        VkImageFormatProperties supported{};
        sampleable = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 && context.imageFormatProperties(context.physical, target.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage | VK_IMAGE_USAGE_SAMPLED_BIT, orderFlags, &supported) == VK_SUCCESS && (supported.sampleCounts & samples) != 0;
        if (sampleable) usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        Check(context.imageFormatProperties(context.physical, target.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, orderFlags, &supported), "vkGetPhysicalDeviceImageFormatProperties multisampled target");
        Require((supported.sampleCounts & samples) != 0 && target.extent.width <= supported.maxExtent.width && target.extent.height <= supported.maxExtent.height, "multisampled render target exceeds device image limits");
        storageFormat = HostStorageFormat(context, target.format);
        storageFlags = storageFormat != target.format ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT : 0u;
        const auto storageSupports = [&](VkFormat format, VkImageUsageFlags formatUsage, VkImageCreateFlags flags) {
            VkImageFormatProperties properties{};
            return context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, formatUsage, flags, &properties) == VK_SUCCESS && (properties.sampleCounts & samples) != 0;
        };
        if (storageFormat != VK_FORMAT_UNDEFINED && (!context.storageImageMultisample || !storageSupports(storageFormat, VK_IMAGE_USAGE_STORAGE_BIT, 0u) || !storageSupports(target.format, usage | VK_IMAGE_USAGE_STORAGE_BIT, storageFlags))) storageFormat = VK_FORMAT_UNDEFINED;
        twinFormat = colorTwinFormat(context, target.format);
        if (twinFormat != VK_FORMAT_UNDEFINED) {
            VkFormatProperties twinProperties{};
            context.formatProperties(context.physical, twinFormat, &twinProperties);
            const VkImageCreateFlags twinFlags = orderFlags | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            if ((twinProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0 || !storageSupports(target.format, usage, twinFlags) || !storageSupports(twinFormat, twinViewUsage, twinFlags)) twinFormat = VK_FORMAT_UNDEFINED;
        }
        try {
            const auto created = allocate(usage, orderFlags);
            image = created.image;
            memory = created.memory;
            view = created.view;
            allocationBytes = created.bytes;
            recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
                recordToGeneral(commands, image);
                const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                context.Resolved(&DeviceFunctions::cmdClearColorImage, "vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &initial, 1, &range);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
                if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            });
        } catch (...) {
            release();
            throw;
        }
    }
    ~MultisampleTarget() { release(); }
    MultisampleTarget(const MultisampleTarget&) = delete;
    MultisampleTarget& operator=(const MultisampleTarget&) = delete;

    struct Allocation {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceSize bytes = 0;
    };

    Allocation allocate(VkImageUsageFlags imageUsage, VkImageCreateFlags flags) {
        Allocation created;
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.flags = flags;
            const std::array<VkFormat, 2> viewFormats{target.format, twinFormat};
            VkImageFormatListCreateInfo formatList{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};
            if (twinFormat != VK_FORMAT_UNDEFINED) {
                if ((flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) == 0 && context.imageFormatList) {
                    formatList.viewFormatCount = static_cast<std::uint32_t>(viewFormats.size());
                    formatList.pViewFormats = viewFormats.data();
                    info.pNext = &formatList;
                }
                info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            }
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = target.format;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = static_cast<VkSampleCountFlagBits>(target.samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = imageUsage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &created.image), "vkCreateImage multisampled target");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, created.image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            created.bytes = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            created.memory = AllocateImageMemory(context, allocation, requirements.memoryTypeBits, "vkAllocateMemory multisampled target");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, created.image, created.memory, 0), "vkBindImageMemory multisampled target");
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = created.image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = target.format;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            VkImageViewUsageCreateInfo viewUsage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
            viewUsage.usage = imageUsage & ~VK_IMAGE_USAGE_STORAGE_BIT;
            if ((imageUsage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) viewInfo.pNext = &viewUsage;
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created.view), "vkCreateImageView multisampled target");
        } catch (...) {
            destroy(created);
            throw;
        }
        return created;
    }

    void destroy(const Allocation& allocation) noexcept {
        if (allocation.view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, allocation.view, nullptr);
        if (allocation.image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, allocation.image, nullptr);
        if (allocation.memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, allocation.memory, nullptr);
    }

    void recordToGeneral(VkCommandBuffer commands, VkImage destination) {
        VkImageMemoryBarrier toGeneral{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toGeneral.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral.image = destination;
        toGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    }

    void makeStorable(Recorder& recorder) {
        if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) return;
        const auto storable = allocate(usage | VK_IMAGE_USAGE_STORAGE_BIT, storageFlags);
        const auto source = image;
        recordToGeneral(recorder.Commands(), storable.image);
        RecordMemoryBarrier(context, recorder.Commands(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = {target.extent.width, target.extent.height, 1};
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(recorder.Commands(), source, VK_IMAGE_LAYOUT_GENERAL, storable.image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        RecordMemoryBarrier(context, recorder.Commands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::Draw, 3);
        for (auto& [key, texture] : textures) retiredTextures.push_back(std::move(texture));
        textures.clear();
        retired.push_back({VK_NULL_HANDLE, VK_NULL_HANDLE, std::exchange(twinView, VK_NULL_HANDLE)});
        retired.push_back({image, memory, view});
        image = storable.image;
        memory = storable.memory;
        view = storable.view;
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        allocationBytes += storable.bytes;
        recorder.Keep(shared_from_this(), static_cast<std::size_t>(storable.bytes));
    }

    std::shared_ptr<Texture> FmaskSampled(const GuestTextureResource& resource, VkComponentMapping components) {
        const auto snapshot = fmaskSnapshotResource(resource);
        Require(target.samples == 8u && target.dccAddress != 0u && fmaskRange.has_value(), "native FMASK sampling requires a tracked eight-fragment DCC target");
        Require(target.tileMode == ColorTileMode::RenderTarget && target.depth == 1u && target.depthSlice == 0u && target.layers == 1u && target.arrayLastSlice == 0u && target.arraySlice == 0u && target.mip == 0u && target.mipCount == 1u, "native FMASK target layout is not implemented");
        Require(target.fmaskAddress == resource.baseAddress && target.fmaskPipeBankXor == resource.pipeBankXor && target.cmaskAddress == resource.fmaskCmaskAddress, "native FMASK descriptor does not match its resident target");
        Require(target.extent.width == resource.width && target.extent.height == resource.height && resource.baseArray == 0u && resource.depthOrLastArray == 0u, "native FMASK extent or subresource does not match its resident target");
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr && recorder->Device() == context.device && !Recorder::InCompletion(), "native FMASK sampling requires its device's active recorder outside completion");
        ApplyFastClear(target);
        Keep();
        const std::array<std::uint32_t, 5> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a), static_cast<std::uint32_t>(resource.dimension)};
        auto& texture = fmaskTextures[key];
        if (texture == nullptr) {
            const auto geometry = DescribeSurface(snapshot);
            std::vector<std::byte> bytes(geometry.guestBytes);
            constexpr std::uint32_t expanded = 0x76543210u;
            for (std::size_t offset = 0; offset < bytes.size(); offset += sizeof(expanded)) std::memcpy(bytes.data() + offset, &expanded, sizeof(expanded));
            texture = std::make_unique<Texture>(context, *context.detiler, snapshot, components, bytes);
        }
        return {shared_from_this(), texture.get()};
    }

    void Keep() {
        if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->Device() == context.device) {
            recorder->Keep(shared_from_this());
            recorder->KeepBytes(this, allocationBytes);
        }
    }

    void ApplyFastClear(const ColorTarget& use) {
        VkClearColorValue clear{};
        const bool refilled = absorbDccFill(use, clear);
        checkDccCurrent(use, refilled);
        if (!refilled && !TakeMultisampledClear(cmask, colorFill, use, clear)) return;
        Keep();
        recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            context.Resolved(&DeviceFunctions::cmdClearColorImage, "vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        });
    }

    void Recreate(const ColorTarget& use) {
        SettleStorageWrites();
        target = use;
        cmask = MultisampledCmask(use.cmaskAddress, use.cmaskBytes);
        colorFill = MultisampledColorFill(use);
        fromTexture = false;
        dccRanges.clear();
        fmaskRange.reset();
        metadataValid = std::make_shared<std::atomic<bool>>(true);
        const auto initial = initialDccClear(use);
        recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            context.Resolved(&DeviceFunctions::cmdClearColorImage, "vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &initial, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        });
    }

    void SettleStorageWrites() {
        if (!storageWritten) return;
        storageWritten = false;
        recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        });
    }

    VkImageView Storage(VkFormat format) {
        Require(storageFormat != VK_FORMAT_UNDEFINED, "the device cannot write the multisampled color target's format as a storage image at its sample count");
        Require(format == storageFormat, "a storage image access to a multisampled color target in another format than the target's storage form is not implemented");
        Require(cmask.Refusal().empty(), cmask.Refusal());
        Require(htileRefusal.empty(), htileRefusal);
        Require(cmask.Keys() != DccKeys::Clear0000, "a storage image access to a multisampled color target whose CMASK fast clear no draw has applied is not implemented");
        auto* recording = Recorder::Active();
        Require(recording != nullptr && recording->Device() == context.device, "a multisampled color target is only written by recorded dispatches on its device");
        Keep();
        makeStorable(*recording);
        ApplyFastClear(target);
        if (storageView == VK_NULL_HANDLE) {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = storageFormat;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &storageView), "vkCreateImageView multisampled target storage");
        }
        recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        });
        storageWritten = true;
        return storageView;
    }

    bool Storable() const {
        return (usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
    }

    VkImageView OrderView() {
        Require(orderFormat != VK_FORMAT_UNDEFINED, "the multisampled color target has no storage view to put its samples in the guest's order");
        if (orderView != VK_NULL_HANDLE) return orderView;
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = orderFormat;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageViewUsageCreateInfo viewUsage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
        viewUsage.usage = VK_IMAGE_USAGE_STORAGE_BIT;
        viewInfo.pNext = &viewUsage;
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &orderView), "vkCreateImageView multisampled target storage");
        return orderView;
    }

    void NoteMetadataFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern) {
        const auto note = [&](std::uint64_t address, std::uint64_t bytes, std::optional<std::uint32_t>& filled) {
            if (address == 0 || fillAddress >= address + bytes || address >= fillAddress + fillBytes) return;
            filled = fillAddress <= address && fillAddress + fillBytes >= address + bytes ? std::optional<std::uint32_t>(pattern) : std::nullopt;
        };
        note(target.dccAddress, MultisampledDccBytes(target), dccFill);
        note(target.cmaskAddress, target.cmaskBytes, cmaskFill);
    }

    std::shared_ptr<Texture> Sampled(VkComponentMapping components, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D) {
        Require(sampleable, "the device cannot sample the multisampled color target's format at its sample count");
        Require(cmask.Refusal().empty(), cmask.Refusal());
        Require(htileRefusal.empty(), htileRefusal);
        Require(cmask.Keys() != DccKeys::Clear0000, "sampling a multisampled color target whose CMASK fast clear no draw has applied is not implemented");
        auto* recording = Recorder::Active();
        Require(recording != nullptr && recording->Device() == context.device, "a multisampled color target is only sampled by recorded draws and dispatches on its device");
        Keep();
        SettleStorageWrites();
        ApplyFastClear(target);
        recordCommands(context, [&](VkCommandBuffer commands, Recorder*) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        });
        const std::array<std::uint32_t, 5> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a), static_cast<std::uint32_t>(viewType)};
        auto& texture = textures[key];
        if (texture == nullptr) texture = std::make_shared<Texture>(context, image, target.format, VK_IMAGE_ASPECT_COLOR_BIT, components, viewType, 0, std::shared_ptr<OwnedImage>{}, orderFormat != VK_FORMAT_UNDEFINED ? VK_IMAGE_USAGE_SAMPLED_BIT : VkImageUsageFlags{0});
        return {shared_from_this(), texture.get()};
    }

    std::shared_ptr<Texture> FirstSample(VkComponentMapping components) {
        const auto source = Sampled({});
        if (firstSampleCopy == nullptr) firstSampleCopy = std::make_unique<FirstSampleCopy>(context, target.format, target.extent);
        firstSampleCopy->Record(*Recorder::Active(), source->View(), source->Layout());
        const std::array<std::uint32_t, 4> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a)};
        auto& texture = firstSampleTextures[key];
        if (texture == nullptr) {
            texture = std::make_shared<Texture>(context, firstSampleCopy->Image(), target.format, VK_IMAGE_ASPECT_COLOR_BIT, components);
            texture->MarkRefreshedPerUse();
        }
        return {shared_from_this(), texture.get()};
    }

    VkImageView View(VkFormat format) {
        if (format == target.format) return view;
        Require(twinFormat != VK_FORMAT_UNDEFINED && format == twinFormat, "a multisampled color target is only viewed in its own format or its sRGB twin");
        if (twinView == VK_NULL_HANDLE) {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = twinFormat;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            VkImageViewUsageCreateInfo viewUsage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
            viewUsage.usage = twinViewUsage;
            viewInfo.pNext = &viewUsage;
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &twinView), "vkCreateImageView multisampled target twin");
        }
        return twinView;
    }

    bool Serves(const ColorTarget& use) const {
        if (use.format == target.format) return sameTarget(target, use);
        auto twin = use;
        twin.format = target.format;
        return twinFormat != VK_FORMAT_UNDEFINED && use.format == twinFormat && sameTarget(target, twin);
    }

    VkRenderPass ResolvePass(VkFormat format) {
        auto& resolvePass = resolvePasses[format];
        if (resolvePass != VK_NULL_HANDLE) return resolvePass;
        std::array<VkAttachmentDescription, 2> attachments{};
        attachments[0].format = format;
        attachments[0].samples = static_cast<VkSampleCountFlagBits>(target.samples);
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].format = format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        for (auto& attachment : attachments) {
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        const VkAttachmentReference color{0, VK_IMAGE_LAYOUT_GENERAL};
        const VkAttachmentReference resolve{1, VK_IMAGE_LAYOUT_GENERAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color;
        subpass.pResolveAttachments = &resolve;
        VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &info, nullptr, &resolvePass), "vkCreateRenderPass resolve");
        return resolvePass;
    }

    Context context;
    std::shared_ptr<std::atomic<bool>> active = std::make_shared<std::atomic<bool>>(true);
    std::shared_ptr<std::atomic<bool>> metadataValid;
    ColorTarget target;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    std::map<VkFormat, VkRenderPass> resolvePasses;
    MultisampledCmask cmask;
    MultisampledColorFill colorFill;
    bool fromTexture = false;
    VkFormat orderFormat = VK_FORMAT_UNDEFINED;
    VkImageView orderView = VK_NULL_HANDLE;
    std::uint64_t htileAddress = 0;
    std::uint64_t htileBytes = 0;
    std::string htileRefusal;

private:
    bool sampleable = false;
    VkImageUsageFlags usage = 0;
    VkFormat storageFormat = VK_FORMAT_UNDEFINED;
    VkImageCreateFlags storageFlags = 0;
    VkFormat twinFormat = VK_FORMAT_UNDEFINED;
    VkImageView twinView = VK_NULL_HANDLE;
    std::vector<Allocation> retired;
    VkImageView storageView = VK_NULL_HANDLE;
    bool storageWritten = false;
    VkDeviceSize allocationBytes = 0;
    std::optional<GuestMemory::UnchangedQuery> fmaskRange;
    std::map<std::array<std::uint32_t, 5>, std::unique_ptr<Texture>> fmaskTextures;
    std::map<std::array<std::uint32_t, 5>, std::shared_ptr<Texture>> textures;
    std::vector<std::shared_ptr<Texture>> retiredTextures;
    std::unique_ptr<FirstSampleCopy> firstSampleCopy;
    std::map<std::array<std::uint32_t, 4>, std::shared_ptr<Texture>> firstSampleTextures;
    std::vector<GuestMemory::UnchangedQuery> dccRanges;
    std::optional<std::uint32_t> dccFill;
    std::optional<std::uint32_t> cmaskFill;

    bool absorbDccFill(const ColorTarget& use, VkClearColorValue& clear) {
        const auto dcc = std::exchange(dccFill, std::nullopt);
        const auto cmaskPattern = std::exchange(cmaskFill, std::nullopt);
        if (dccRanges.size() < 2 || !dcc || use.dccAddress != target.dccAddress || use.cmaskAddress != target.cmaskAddress || use.cmaskBytes != target.cmaskBytes) return false;
        const auto keys = LandedUniformMetadataByte(dccRanges[1].address, dccRanges[1].bytes);
        if (!keys || *dcc != *keys * 0x01010101u) return false;
        std::uint8_t cmaskKeys = 0;
        if (dccRanges.size() > 2) {
            const auto& range = dccRanges[2];
            auto* recorder = Recorder::Active();
            const bool written = !GuestMemory::UnchangedSince(range.address, range.bytes, range.generation) || (recorder != nullptr && recorder->PendingWriteOverlaps(range.address, range.bytes));
            const auto landed = LandedUniformMetadataByte(range.address, range.bytes);
            if (!landed || (written && (!cmaskPattern || *cmaskPattern != *landed * 0x01010101u))) return false;
            cmaskKeys = *landed;
        }
        std::vector<GuestMemory::UnchangedQuery> metadata(dccRanges.begin() + 1, dccRanges.end());
        for (auto& range : metadata) {
            if (!GuestMemory::Watched(range.address, range.bytes)) return false;
            range.generation = collectBlocks(range.address, range.bytes);
            if (range.generation == 0) return false;
        }
        clear = MultisampledDccClear(use, std::vector<std::byte>(dccRanges[1].bytes, std::byte{*keys}), std::vector<std::byte>(use.cmaskBytes, std::byte{cmaskKeys}));
        std::copy(metadata.begin(), metadata.end(), dccRanges.begin() + 1);
        return true;
    }

    VkClearColorValue initialDccClear(const ColorTarget& use) {
        if (use.dccAddress == 0) return {};
        const auto dccBytes = MultisampledDccBytes(use);
        Require((use.dccAddress & 4095u) == 0, "multisampled DCC metadata base alignment is not implemented");
        Require(use.cmaskAddress == 0 ? use.cmaskBytes == 0 : use.cmaskBytes == CmaskBytes(use.extent.width, use.extent.height), "multisampled DCC CMASK footprint is invalid");
        const auto colorBytes = MultisampledColorBytes(use);
        Require(colorBytes != 0, "multisampled DCC color footprint is not implemented");
        dccRanges.push_back({use.address, static_cast<std::size_t>(colorBytes), 0});
        dccRanges.push_back({use.dccAddress, dccBytes, 0});
        if (use.cmaskAddress != 0) dccRanges.push_back({use.cmaskAddress, use.cmaskBytes, 0});
        for (auto& range : dccRanges) {
            GuestMemory::CheckRange(reinterpret_cast<const void*>(range.address), range.bytes, 256);
            Require(GuestMemory::Watched(range.address, range.bytes), "initial multisampled DCC contents require watched guest memory");
            range.generation = collectBlocks(range.address, range.bytes);
            Require(range.generation != 0, "initial multisampled DCC contents have no write generation");
        }
        for (std::size_t first = 0; first < dccRanges.size(); ++first) {
            for (std::size_t second = first + 1; second < dccRanges.size(); ++second) {
                const auto& a = dccRanges[first];
                const auto& b = dccRanges[second];
                Require(a.address >= b.address + b.bytes || b.address >= a.address + a.bytes, "overlapping multisampled DCC color or metadata ranges are not implemented");
            }
        }
        if (use.samples == 8u && use.fmaskAddress != 0u && use.cmaskAddress != 0u) {
            const auto resource = fmaskResource(use);
            static_cast<void>(fmaskSnapshotResource(resource));
            fmaskRange = GuestMemory::UnchangedQuery{use.fmaskAddress, static_cast<std::size_t>(DescribeSurface(resource).guestBytes), 0};
            auto& range = *fmaskRange;
            GuestMemory::CheckRange(reinterpret_cast<const void*>(range.address), range.bytes, 65536u);
            Require(GuestMemory::Watched(range.address, range.bytes), "native FMASK contents require watched guest memory");
            range.generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
            Require(range.generation != 0, "native FMASK contents have no write generation");
            for (const auto& other : dccRanges) Require(range.address >= other.address + other.bytes || other.address >= range.address + range.bytes, "native FMASK overlaps its color or metadata planes");
        }
        std::vector<std::byte> dcc(dccRanges[1].bytes);
        GuestMemory::Read(use.dccAddress, dcc);
        std::vector<std::byte> cmask(use.cmaskBytes);
        if (!cmask.empty()) GuestMemory::Read(use.cmaskAddress, cmask);
        const auto clear = MultisampledDccClear(use, dcc, cmask);
        Require(GuestMemory::UnchangedSinceAll(dccRanges) && (!fmaskRange || GuestMemory::UnchangedSinceAll(std::span(&*fmaskRange, 1u))), "initial multisampled DCC contents changed during import");
        return clear;
    }

    void requireDccRange(bool valid, const GuestMemory::UnchangedQuery& range, const char* reason) const {
        if (valid) return;
        const auto* plane = range.address == target.address ? "color" : range.address == target.dccAddress ? "DCC" : range.address == target.fmaskAddress ? "FMASK" : "CMASK";
        char text[384];
        std::snprintf(text, sizeof(text), "%s (%s range 0x%llx+0x%zx, %u samples)", reason, plane, static_cast<unsigned long long>(range.address), range.bytes, target.samples);
        Require(false, text);
    }

    void checkDccCurrent(const ColorTarget& use, bool metadataRefilled) const {
        if (dccRanges.empty()) {
            Require(use.dccAddress == 0, "adding DCC to a resident multisampled color target is not implemented");
            return;
        }
        Require(metadataValid->load(std::memory_order_acquire), "multisampled DCC metadata was released while its color target remains resident");
        static_cast<void>(MultisampledDccBytes(use));
        Require(use.address == target.address && use.dccAddress == target.dccAddress && use.dccControl == target.dccControl && use.dccAlphaOnMsb == target.dccAlphaOnMsb && use.dccPipeAligned == target.dccPipeAligned && use.cmaskAddress == target.cmaskAddress && use.cmaskBytes == target.cmaskBytes && use.tileMode == target.tileMode && use.elementBytes == target.elementBytes && use.pipeBankXor == target.pipeBankXor, "multisampled DCC target layout or metadata changed");
        const auto ranges = std::span(dccRanges).first(metadataRefilled ? 1u : dccRanges.size());
        for (const auto& range : ranges) requireDccRange(GuestMemory::Watched(range.address, range.bytes), range, "multisampled DCC target reuse requires watched guest memory");
        for (const auto& range : ranges) {
            GuestMemory::CollectWritesUncached(range.address, range.bytes);
            requireDccRange(GuestMemory::UnchangedSince(range.address, range.bytes, range.generation) || !GuestMemory::StoredOver(range.address, range.bytes, range.generation), range, "writes to a resident multisampled DCC target or its metadata are not implemented");
            if (auto* recorder = Recorder::Active(); recorder != nullptr) requireDccRange(!recorder->PendingWriteOverlaps(range.address, range.bytes), range, "pending writes to a resident multisampled DCC target or its metadata are not implemented");
        }
        if (fmaskRange) {
            Require(use.fmaskAddress == target.fmaskAddress && use.fmaskPipeBankXor == target.fmaskPipeBankXor, "native FMASK target layout changed");
            const auto& range = *fmaskRange;
            requireDccRange(GuestMemory::Watched(range.address, range.bytes), range, "native FMASK sampling requires watched guest memory");
            GuestMemory::CollectWritesUncached(range.address, range.bytes);
            requireDccRange(GuestMemory::UnchangedSince(range.address, range.bytes, range.generation) || !GuestMemory::StoredOver(range.address, range.bytes, range.generation), range, "writes to a resident native FMASK are not implemented");
            if (auto* recorder = Recorder::Active(); recorder != nullptr) requireDccRange(!recorder->PendingWriteOverlaps(range.address, range.bytes), range, "pending writes to a resident native FMASK are not implemented");
        }
    }

    void release() noexcept {
        fmaskTextures.clear();
        textures.clear();
        retiredTextures.clear();
        firstSampleTextures.clear();
        firstSampleCopy.reset();
        for (const auto& [format, resolvePass] : resolvePasses) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, resolvePass, nullptr);
        if (orderView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, orderView, nullptr);
        if (twinView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, twinView, nullptr);
        if (storageView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, storageView, nullptr);
        destroy({image, memory, view});
        for (const auto& allocation : retired) destroy(allocation);
    }
};

struct GuardedRange {
    VkDevice device;
    std::uint64_t address;
    std::uint64_t bytes;
    std::uint32_t samples;
    std::shared_ptr<std::atomic<bool>> active;
    std::shared_ptr<std::atomic<bool>> metadataValid;
};

class PrimedSharedMutex : public std::shared_mutex {
public:
    PrimedSharedMutex() {
        lock();
        unlock();
    }
};

std::shared_mutex& guardedMutex() {
    static PrimedSharedMutex mutex;
    return mutex;
}

std::vector<GuardedRange>& guarded() {
    static std::vector<GuardedRange> ranges;
    return ranges;
}

std::atomic<bool>& anyGuarded() {
    static std::atomic<bool> any{false};
    return any;
}

void guardReads(std::uint64_t address, std::size_t bytes) {
    RequireNoMultisampleTarget(address, bytes, "a CPU read");
}

struct RecordedSingleClear {
    std::uint64_t surface = 0;
    std::uint32_t pipeBankXor = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t samples = 0;
    std::array<std::uint32_t, 4> color{};
};

std::mutex& singleClearsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<RecordedSingleClear>& singleClears() {
    static std::vector<RecordedSingleClear> list;
    return list;
}

std::atomic<bool>& anySingleClears() {
    static std::atomic<bool> any{false};
    return any;
}

void forgetSingleClears(std::uintptr_t address, std::size_t bytes) {
    std::lock_guard lock(singleClearsMutex());
    std::erase_if(singleClears(), [&](const RecordedSingleClear& clear) { return clear.surface >= address && clear.surface - address < bytes; });
    anySingleClears().store(!singleClears().empty(), std::memory_order_release);
}

void retireUnmapped(std::uintptr_t address, std::size_t bytes) {
    forgetSingleClears(address, bytes);
    RetireUnmappedDepth_nid_no_patch(address, bytes);
    std::unique_lock lock(guardedMutex());
    std::erase_if(guarded(), [&](const auto& range) {
        if (range.metadataValid && address < range.address + range.bytes && range.address < address + bytes) range.metadataValid->store(false, std::memory_order_release);
        if (range.address < address || range.address - address > bytes || range.bytes > bytes - (range.address - address)) return false;
        if (!range.metadataValid) range.active->store(false, std::memory_order_release);
        return true;
    });
    std::erase_if(guarded(), [](const auto& range) { return range.metadataValid && !range.active->load(std::memory_order_acquire); });
    anyGuarded().store(!guarded().empty(), std::memory_order_release);
}

void guard(const Context& context, const ColorTarget& target, const std::shared_ptr<std::atomic<bool>>& active, const std::shared_ptr<std::atomic<bool>>& metadataValid) {
    auto bytes = std::max<std::uint64_t>(target.bytes * target.samples, static_cast<std::uint64_t>(target.extent.width) * target.extent.height * target.elementBytes * target.samples);
    if (MultisampledBlockLog2(target.tileMode) == 16u && target.tileMode != ColorTileMode::Standard64KB && std::has_single_bit(target.elementBytes) && std::has_single_bit(target.samples)) {
        const auto log2Samples = std::countr_zero(target.samples);
        const auto log2Pixels = 16u - std::countr_zero(target.elementBytes) - log2Samples;
        const auto log2Width = (log2Pixels + ((log2Samples & 1u) == 0u ? 1u : 0u)) / 2u;
        const auto blockWidth = 1u << log2Width;
        const auto blockHeight = 1u << (log2Pixels - log2Width);
        const auto align = [](std::uint32_t extent, std::uint32_t block) { return (static_cast<std::uint64_t>(extent) + block - 1u) / block * block; };
        bytes = align(target.extent.width, blockWidth) * align(target.extent.height, blockHeight) * target.elementBytes * target.samples;
    }
    std::unique_lock lock(guardedMutex());
    guarded().push_back({context.device, target.address, bytes, target.samples, active});
    if (target.dccAddress != 0) {
        guarded().push_back({context.device, target.dccAddress, MultisampledDccBytes(target), target.samples, active, metadataValid});
        if (target.cmaskAddress != 0) guarded().push_back({context.device, target.cmaskAddress, target.cmaskBytes, target.samples, active, metadataValid});
        if (target.samples == 8u && target.fmaskAddress != 0u && target.cmaskAddress != 0u) guarded().push_back({context.device, target.fmaskAddress, DescribeSurface(fmaskResource(target)).guestBytes, target.samples, active, metadataValid});
    }
    anyGuarded().store(true, std::memory_order_release);
    InstallResidentImageReadGuard_nid_no_patch();
}

std::mutex& targetsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::shared_ptr<MultisampleTarget>>& targets() {
    static std::vector<std::shared_ptr<MultisampleTarget>> list;
    return list;
}

void discardRetiredTargets() {
    std::erase_if(targets(), [](const auto& target) { return !target->active->load(std::memory_order_acquire); });
}

MultisampleTarget& acquire(const Context& context, const ColorTarget& requested) {
    discardRetiredTargets();
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || !target->Serves(requested)) continue;
        auto use = requested;
        use.format = target->target.format;
        if (recreated(target->target, use)) {
            {
                std::unique_lock lock(guardedMutex());
                std::erase_if(guarded(), [&](const auto& range) { return range.active == target->active; });
                anyGuarded().store(!guarded().empty(), std::memory_order_release);
            }
            target->Recreate(use);
            guard(context, use, target->active, target->metadataValid);
            target->ApplyFastClear(use);
            return *target;
        }
        if (target->fromTexture && use.cmaskAddress != 0) {
            target->fromTexture = false;
            target->cmask = MultisampledCmask(use.cmaskAddress, use.cmaskBytes);
        }
        target->cmask.CheckAddress(use.cmaskAddress);
        target->Keep();
        target->SettleStorageWrites();
        target->ApplyFastClear(use);
        return *target;
    }
    const auto& use = requested;
    auto target = std::make_shared<MultisampleTarget>(context, use);
    target->Keep();
    targets().push_back(std::move(target));
    auto& created = *targets().back();
    guard(context, use, created.active, created.metadataValid);
    if (use.cmaskFastClear && use.dccAddress == 0) created.cmask.SeedCode(CurrentUniformMetadataByte(use.cmaskAddress, static_cast<std::size_t>(use.cmaskBytes)));
    created.ApplyFastClear(use);
    created.Keep();
    return created;
}

MultisampleTarget& createFilled(const Context& context, const GuestTextureResource& resource, VkFormat format, const char* access) {
    const auto refuse = [&](const std::string& reason) {
        char text[192];
        std::snprintf(text, sizeof(text), "AGC graphics: %s the %ux%u multisampled texture 0x%llx of %u samples, which no draw rendered to and ", access, resource.width, resource.height, static_cast<unsigned long long>(resource.baseAddress), resource.samples);
        throw std::runtime_error(text + reason + ", is not implemented");
    };
    ColorTarget use{};
    switch (resource.tileMode) {
        case TextureTileMode::kStandard4KB: use.tileMode = ColorTileMode::Standard4KB; break;
        case TextureTileMode::kStandard64KB: use.tileMode = ColorTileMode::Standard64KB; break;
        case TextureTileMode::kZ64KBX: use.tileMode = ColorTileMode::Z64KBX; break;
        case TextureTileMode::kD64KBX: use.tileMode = ColorTileMode::D64KBX; break;
        case TextureTileMode::kR64KBX: use.tileMode = ColorTileMode::RenderTarget; break;
        default: refuse("whose tile mode " + std::to_string(static_cast<int>(resource.tileMode)) + " has no modeled multisampled color footprint");
    }
    use.address = resource.baseAddress;
    use.surfaceAddress = resource.baseAddress;
    use.extent = {resource.width, resource.height};
    use.surfaceExtent = use.extent;
    use.format = format;
    use.elementBytes = BytesPerElement(resource.format);
    use.pipeBankXor = resource.pipeBankXor;
    use.samples = resource.samples;
    const auto bytes = MultisampledColorBytes(use);
    if (bytes == 0) refuse("whose " + std::to_string(use.elementBytes) + "-byte elements have no modeled multisampled color footprint");
    use.bytes = ColorTargetLayout(use.extent.width, use.extent.height, use.tileMode, use.elementBytes).Bytes();
    char range[64];
    std::snprintf(range, sizeof(range), "0x%llx+0x%llx", static_cast<unsigned long long>(use.address), static_cast<unsigned long long>(bytes));
    RequireNoMultisampleTarget(use.address, static_cast<std::size_t>(bytes), "sampling a multisampled texture no draw rendered to");
    GuestMemory::FlushGpuWrites(use.address, static_cast<std::size_t>(bytes));
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(use.address), static_cast<std::size_t>(bytes))) refuse(std::string("whose color memory ") + range + " is not mapped");
    std::uint64_t htileBytes = 0;
    bool htileCleared = false;
    if (resource.dccAddress != 0) {
        if (use.tileMode != ColorTileMode::Z64KBX) refuse("whose compression metadata over a tile mode " + std::to_string(static_cast<int>(resource.tileMode)) + " surface is not modeled");
        htileBytes = HtileMetadataBytes(use.extent);
        char htileRange[64];
        std::snprintf(htileRange, sizeof(htileRange), "0x%llx+0x%llx", static_cast<unsigned long long>(resource.dccAddress), static_cast<unsigned long long>(htileBytes));
        GuestMemory::FlushGpuWrites(resource.dccAddress, static_cast<std::size_t>(htileBytes));
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(resource.dccAddress), static_cast<std::size_t>(htileBytes))) refuse(std::string("whose HTILE ") + htileRange + " is not mapped");
        std::vector<std::uint32_t> words(static_cast<std::size_t>(htileBytes / sizeof(std::uint32_t)));
        std::memcpy(words.data(), reinterpret_cast<const void*>(resource.dccAddress), words.size() * sizeof(std::uint32_t));
        htileCleared = words.front() == 0u;
        const auto mixed = std::find_if(words.begin(), words.end(), [&](std::uint32_t word) { return htileCleared ? word != 0u : (word & 15u) != 15u; });
        if (mixed != words.end()) {
            char text[96];
            std::snprintf(text, sizeof(text), " holds 0x%08x at +0x%llx after 0x%08x", *mixed, static_cast<unsigned long long>((mixed - words.begin()) * sizeof(std::uint32_t)), words.front());
            refuse(std::string("whose HTILE ") + htileRange + text + ", which is neither all cleared to 0.0 nor all expanded");
        }
    }
    const auto* memory = reinterpret_cast<const std::byte*>(use.address);
    std::uint32_t value = 0;
    if (!htileCleared) std::memcpy(&value, memory, sizeof(value));
    char held[16];
    std::snprintf(held, sizeof(held), "0x%08x", value);
    const auto repeated = htileCleared ? memory + bytes : std::mismatch(memory + sizeof(value), memory + bytes, memory).first;
    if (repeated != memory + bytes) {
        char offset[32];
        std::snprintf(offset, sizeof(offset), "+0x%llx", static_cast<unsigned long long>(repeated - memory));
        refuse(std::string("whose color memory ") + range + " does not hold one repeated 32-bit value (" + held + " until " + offset + ")");
    }
    VkClearColorValue clear{};
    if (!MultisampledFillClearColor(use, value, clear)) refuse(std::string("whose color memory ") + range + " holds " + held + ", which has no exact clear value in its format");
    auto target = std::make_shared<MultisampleTarget>(context, use);
    target->Keep();
    targets().push_back(std::move(target));
    auto& created = *targets().back();
    guard(context, use, created.active, created.metadataValid);
    created.colorFill.NoteFill(use.address, static_cast<std::size_t>(bytes), value);
    created.fromTexture = true;
    created.htileAddress = resource.dccAddress;
    created.htileBytes = htileBytes;
    return created;
}

}

void InstallResidentImageReadGuard_nid_no_patch() {
    static std::once_flag installed;
    std::call_once(installed, [] { GuestMemory::SetReadGuard(&guardReads); });
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

void UpdateResidentImageUnmapObserver_nid_no_patch() {
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    GuestAllocations::GuestAllocationsSetUnmapObserver_nid_postfix(anyGuarded().load(std::memory_order_acquire) || anySingleClears().load(std::memory_order_acquire) || HasResidentDepthGuards_nid_no_patch() ? &retireUnmapped : nullptr);
}

std::optional<DccKeys> MultisampledCmaskKeys(std::uint8_t code) {
    const auto tile = code & 0xfu;
    if ((code >> 4u) != tile) return std::nullopt;
    if (tile == 0u) return DccKeys::Clear0000;
    if ((tile >> 2u) == 3u) return DccKeys::Uncompressed;
    return std::nullopt;
}

void MultisampledCmask::NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern) {
    if (address == 0 || fillAddress >= address + bytes || address >= fillAddress + fillBytes) return;
    if (fillAddress > address || fillAddress + fillBytes < address + bytes) {
        char text[192];
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx partly covers the CMASK 0x%llx+0x%llx of a multisampled color target, leaving its tiles mixed", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        refusal = text;
        return;
    }
    refusal.clear();
    const auto code = static_cast<std::uint8_t>(pattern);
    if (const auto filled = MultisampledCmaskKeys(code); filled && pattern == code * 0x01010101u) {
        keys = *filled;
    } else {
        char text[224];
        std::snprintf(text, sizeof(text), "a fill of the CMASK 0x%llx+0x%llx of a multisampled color target with 0x%08x leaves its tiles mixed (neither a register color clear 0 nor a color-expanded code such as 0xcc or 0xff)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), pattern);
        refusal = text;
    }
}

void MultisampledCmask::SeedCode(std::optional<std::uint8_t> code) {
    if (const auto seeded = code ? MultisampledCmaskKeys(*code) : std::nullopt) {
        keys = *seeded;
        return;
    }
    char text[192];
    if (code) std::snprintf(text, sizeof(text), "the CMASK 0x%llx of a multisampled color target reads as code 0x%02x, which is not modeled", static_cast<unsigned long long>(address), *code);
    else std::snprintf(text, sizeof(text), "the CMASK 0x%llx of a multisampled color target reads as mixed, which is not modeled", static_cast<unsigned long long>(address));
    refusal = text;
}

void MultisampledCmask::CheckAddress(std::uint64_t cmaskAddress) const {
    if (cmaskAddress == address) return;
    char text[192];
    std::snprintf(text, sizeof(text), "a multisampled color target used CMASK 0x%llx after its first use with CMASK 0x%llx, which is not modeled", static_cast<unsigned long long>(cmaskAddress), static_cast<unsigned long long>(address));
    Require(false, std::string(text));
}

bool MultisampledCmask::TakeClear() {
    Require(refusal.empty(), refusal);
    if (keys != DccKeys::Clear0000) return false;
    keys = DccKeys::Uncompressed;
    return true;
}

std::uint64_t MultisampledColorBytes(const ColorTarget& target) {
    const auto blockLog2 = MultisampledBlockLog2(target.tileMode);
    if (blockLog2 == 0 || !std::has_single_bit(target.elementBytes) || !std::has_single_bit(target.samples)) return 0;
    const auto samplesLog2 = static_cast<std::uint32_t>(std::countr_zero(target.samples));
    const auto elementsLog2 = blockLog2 - static_cast<std::uint32_t>(std::countr_zero(target.elementBytes)) - samplesLog2;
    const auto widthLog2 = (elementsLog2 + ((samplesLog2 & 1u) == 0 ? 1u : 0u)) / 2u;
    const std::uint64_t blockWidth = std::uint64_t{1} << widthLog2;
    const std::uint64_t blockHeight = std::uint64_t{1} << (elementsLog2 - widthLog2);
    return ((target.extent.width + blockWidth - 1) / blockWidth) * ((target.extent.height + blockHeight - 1) / blockHeight) << blockLog2;
}

std::size_t MultisampledDccBytes(const ColorTarget& target) {
    Require(target.samples == 2 || target.samples == 4 || target.samples == 8, "unsupported multisampled DCC fragment count");
    Require(target.tileMode == ColorTileMode::RenderTarget && target.dccPipeAligned, "multisampled DCC requires pipe-aligned SW_64KB_R_X");
    Require(std::has_single_bit(target.elementBytes) && target.elementBytes <= 16, "unsupported multisampled DCC element size");
    Require(target.extent.width != 0 && target.extent.width <= 16384 && target.extent.height != 0 && target.extent.height <= 16384, "invalid multisampled DCC extent");
    Require(target.mipCount == 1 && target.mip == 0 && !target.mipTail && target.depth == 1 && target.depthSlice == 0 && target.arrayLastSlice == 0 && target.arraySlice == 0 && target.layers == 1, "multisampled DCC array, volume or mip contents are not implemented");
    const auto bits = 19u - static_cast<std::uint32_t>(std::countr_zero(target.elementBytes));
    const std::uint64_t width = std::uint64_t{1} << ((bits + 1u) / 2u);
    const std::uint64_t height = std::uint64_t{1} << (bits / 2u);
    return static_cast<std::size_t>(((target.extent.width + width - 1) / width) * ((target.extent.height + height - 1) / height) * 4096u);
}

namespace {

std::optional<std::array<std::uint32_t, 4>> recordedSingleClear(const ColorTarget& target) {
    std::lock_guard lock(singleClearsMutex());
    for (const auto& clear : singleClears()) {
        if (clear.surface == target.surfaceAddress && clear.pipeBankXor == target.pipeBankXor && clear.width == target.extent.width && clear.height == target.extent.height && clear.samples == target.samples) return clear.color;
    }
    return std::nullopt;
}

VkClearColorValue singleClearColor(const ColorTarget& target) {
    const auto color = recordedSingleClear(target);
    Require(color.has_value(), "multisampled DCC comp-to-single keys without a recorded clear color are not implemented");
    const auto elementBytes = static_cast<std::size_t>(target.elementBytes);
    Require(elementBytes == 1 || elementBytes == 2 || elementBytes == 4 || elementBytes == 8 || elementBytes == 16, "multisampled DCC comp-to-single clears of this texel size are not implemented");
    std::array<std::uint32_t, 4> pattern{};
    for (std::size_t offset = 0; offset < sizeof(pattern); offset += elementBytes) std::memcpy(reinterpret_cast<std::byte*>(pattern.data()) + offset, color->data(), elementBytes);
    VkClearColorValue clear{};
    Require(ClearColorForTexel(target.format, target.elementBytes, pattern, clear), "multisampled DCC comp-to-single clear has no exact host value");
    return clear;
}

}

std::optional<MultisampledSingleClear> MatchMultisampledSingleClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads) {
    static constexpr std::array<std::uint32_t, 22> kernel{0xbe800303u, 0xbe830382u, 0xf40c0401u, 0xfa000000u, 0xd7460000u, 0x04010609u, 0xd7460001u, 0x0405060au, 0x7e060208u, 0x7e0c020bu, 0x7e0e0280u, 0xd5690004u, 0x02000100u, 0x7e000205u, 0xd5690005u, 0x02000901u, 0xd7000001u, 0x02000c80u, 0xbf8cc07fu, 0xf0201f38u, 0x00040004u, 0xbf810000u};
    if (userData.size() < 9 || numThreads != std::array<std::uint32_t, 3>{8u, 8u, 1u}) return std::nullopt;
    if (code.size() < kernel.size() || !std::equal(kernel.begin(), kernel.end(), code.begin())) return std::nullopt;
    if (userData[3] == 0 || userData[4] == 0) return std::nullopt;
    return MultisampledSingleClear{userData[2] | (std::uint64_t{2} << 32u), {userData[5], userData[6], userData[7], userData[8]}};
}

void NoteMultisampledSingleClear(const GuestTextureResource& resource, const std::array<std::uint32_t, 4>& color) {
    {
        std::lock_guard lock(singleClearsMutex());
        auto& list = singleClears();
        std::erase_if(list, [&](const RecordedSingleClear& clear) { return clear.surface == resource.baseAddress; });
        list.push_back({resource.baseAddress, resource.pipeBankXor, resource.width, resource.height, resource.samples, color});
        anySingleClears().store(true, std::memory_order_release);
    }
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

bool TakeMultisampledSingleClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads) {
    const auto clear = MatchMultisampledSingleClear(code, userData, numThreads);
    if (!clear) return false;
    std::array<std::uint32_t, 8> words{};
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(clear->descriptor), sizeof(words))) return false;
    GuestMemory::Read(clear->descriptor, std::as_writable_bytes(std::span(words)));
    const auto resource = DecodeTextureResource(words);
    if (resource.samples < 2) return false;
    NoteMultisampledSingleClear(resource, clear->color);
    return true;
}

VkClearColorValue MultisampledDccClear(const ColorTarget& target, std::span<const std::byte> dcc, std::span<const std::byte> cmask) {
    Require(target.dccAddress != 0 && (target.dccControl & 0x3ff80u) == 0 && (target.pipeBankXor & ~0xcf00u) == 0, "multisampled DCC address, color transform, lossy mode or XOR is not implemented");
    Require(dcc.size() == MultisampledDccBytes(target), "multisampled DCC metadata snapshot size mismatch");
    Require(std::ranges::all_of(dcc, [&](std::byte key) { return key == dcc.front(); }), "mixed multisampled DCC metadata is not implemented");
    Require(target.cmaskAddress == 0 ? cmask.empty() && target.cmaskBytes == 0 : cmask.size() == target.cmaskBytes && target.cmaskBytes == CmaskBytes(target.extent.width, target.extent.height), "multisampled DCC CMASK snapshot size mismatch");
    if (!cmask.empty() && target.cmaskFastClear) {
        const auto code = std::to_integer<std::uint32_t>(cmask.front());
        const auto expanded = target.samples == 2 ? 0xddu : target.samples == 4 ? 0xeeu : 0xffu;
        Require(std::ranges::all_of(cmask, [&](std::byte key) { return key == cmask.front(); }) && (code == 0xccu || code == expanded), "multisampled DCC CMASK compression or register clear is not implemented");
    }
    if (dcc.front() == std::byte{0x10}) return singleClearColor(target);
    DccKeys keys;
    switch (std::to_integer<std::uint32_t>(dcc.front())) {
        case 0x00u: keys = DccKeys::Clear0000; break;
        case 0x40u: keys = DccKeys::Clear0001; break;
        case 0x80u: keys = DccKeys::Clear1110; break;
        case 0xc0u: keys = DccKeys::Clear1111; break;
        default: throw std::runtime_error("initial multisampled DCC contents are not a supported uniform fast clear");
    }
    std::array<std::byte, 16> texels{};
    Require(FillDccClear(target.format, keys, target.dccAlphaOnMsb, texels), "multisampled DCC clear format is not implemented");
    std::array<std::uint32_t, 4> pattern{};
    std::memcpy(pattern.data(), texels.data(), texels.size());
    VkClearColorValue clear{};
    Require(ClearColorForTexel(target.format, target.elementBytes, pattern, clear), "multisampled DCC fast clear has no exact host value");
    return clear;
}

bool MultisampledFillClearColor(const ColorTarget& target, std::uint32_t fill, VkClearColorValue& clear) {
    const std::array<std::uint32_t, 4> pattern{fill, fill, fill, fill};
    VkFormat unorm = VK_FORMAT_UNDEFINED;
    switch (target.format) {
        case VK_FORMAT_R8G8B8A8_SRGB: unorm = VK_FORMAT_R8G8B8A8_UNORM; break;
        case VK_FORMAT_B8G8R8A8_SRGB: unorm = VK_FORMAT_B8G8R8A8_UNORM; break;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: unorm = VK_FORMAT_A8B8G8R8_UNORM_PACK32; break;
        default: return ClearColorForTexel(target.format, target.elementBytes, pattern, clear);
    }
    if (!ClearColorForTexel(unorm, target.elementBytes, pattern, clear)) return false;
    return std::all_of(clear.float32, clear.float32 + 3, [](float value) { return value == 0.0f || value == 1.0f; });
}

MultisampledColorFill::MultisampledColorFill(const ColorTarget& target) : address(target.address), bytes(MultisampledColorBytes(target)) {}

void MultisampledColorFill::NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern) {
    if (bytes == 0 ? fillAddress > address || address >= fillAddress + fillBytes : fillAddress >= address + bytes || address >= fillAddress + fillBytes) return;
    char text[192];
    if (bytes == 0) {
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx covers the color memory at 0x%llx of a multisampled target whose footprint is not modeled", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address));
        refusal = text;
        return;
    }
    if (fillAddress > address || fillAddress + fillBytes < address + bytes) {
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx partly covers the color memory 0x%llx+0x%llx of a multisampled target, leaving its samples mixed", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        refusal = text;
        return;
    }
    refusal.clear();
    filled = pattern;
}

std::optional<std::uint32_t> MultisampledColorFill::TakeFill() {
    Require(refusal.empty(), refusal);
    return std::exchange(filled, std::nullopt);
}

void MultisampledColorFill::Discard() {
    refusal.clear();
    filled.reset();
}

bool TakeMultisampledClear(MultisampledCmask& cmask, MultisampledColorFill& colorFill, const ColorTarget& use, VkClearColorValue& clear) {
    if (cmask.TakeClear()) {
        colorFill.Discard();
        clear = fastClearColor(use);
        return true;
    }
    const auto fill = colorFill.TakeFill();
    if (!fill) return false;
    Require(MultisampledFillClearColor(use, *fill, clear), "a fill over the color memory of a multisampled target has no exact clear value in its format");
    return true;
}

SampleOrderTarget MultisampleTargetOrderTarget(const Context& context, const ColorTarget& target) {
    std::lock_guard lock(targetsMutex());
    auto& found = acquire(context, target);
    return {found.image, found.OrderView(), found.orderFormat, target.extent, target.samples, SampleOrderMethod::Storage, found.shared_from_this()};
}

VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target, VkImage* image) {
    std::lock_guard lock(targetsMutex());
    auto& acquired = acquire(context, target);
    if (image != nullptr) *image = acquired.image;
    return acquired.View(target.format);
}

VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target, std::shared_ptr<void>& owner, VkImage* image) {
    std::lock_guard lock(targetsMutex());
    auto& acquired = acquire(context, target);
    owner = acquired.shared_from_this();
    if (image != nullptr) *image = acquired.image;
    return acquired.View(target.format);
}

void KeepMultisampleTarget_nid_no_patch(const std::shared_ptr<void>& owner) {
    std::static_pointer_cast<MultisampleTarget>(owner)->Keep();
}

namespace {

bool redBlueSwapped(VkFormat rendered, VkFormat sampled) {
    const auto pair = [&](VkFormat a, VkFormat b) { return (rendered == a && sampled == b) || (rendered == b && sampled == a); };
    return pair(VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM) || pair(VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB) || pair(VK_FORMAT_B8G8R8A8_SNORM, VK_FORMAT_R8G8B8A8_SNORM) || pair(VK_FORMAT_B8G8R8A8_UINT, VK_FORMAT_R8G8B8A8_UINT) || pair(VK_FORMAT_B8G8R8A8_SINT, VK_FORMAT_R8G8B8A8_SINT);
}

VkComponentMapping swapRedBlue(VkComponentMapping components) {
    const auto swapped = [](VkComponentSwizzle swizzle, VkComponentSwizzle identity) {
        if (swizzle == VK_COMPONENT_SWIZZLE_IDENTITY) swizzle = identity;
        if (swizzle == VK_COMPONENT_SWIZZLE_R) return VK_COMPONENT_SWIZZLE_B;
        if (swizzle == VK_COMPONENT_SWIZZLE_B) return VK_COMPONENT_SWIZZLE_R;
        return swizzle;
    };
    return {swapped(components.r, VK_COMPONENT_SWIZZLE_R), swapped(components.g, VK_COMPONENT_SWIZZLE_G), swapped(components.b, VK_COMPONENT_SWIZZLE_B), swapped(components.a, VK_COMPONENT_SWIZZLE_A)};
}

bool sameStorageForm(const Context& context, VkFormat rendered, VkFormat requested) {
    if (rendered == requested) return true;
    const auto storage = HostStorageFormat(context, rendered);
    return storage != VK_FORMAT_UNDEFINED && storage == HostStorageFormat(context, requested);
}

}

std::shared_ptr<Texture> MultisampleTargetTexture(const Context& context, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType viewType) {
    std::lock_guard lock(targetsMutex());
    discardRetiredTargets();
    const auto format = ResolveTextureFormat(resource.format);
    const MultisampleTarget* other = nullptr;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || target->target.surfaceAddress != resource.baseAddress) continue;
        const auto& rendered = target->target;
        const bool sameShape = rendered.extent.width == resource.width && rendered.extent.height == resource.height && rendered.samples == resource.samples && rendered.mip == 0;
        if (sameShape && rendered.format == format) return target->Sampled(components, viewType);
        if (sameShape && redBlueSwapped(rendered.format, format)) return target->Sampled(swapRedBlue(components), viewType);
        other = target.get();
    }
    if (other == nullptr) {
        Require(Recorder::Active() != nullptr, "a multisampled texture is only sampled by recorded draws and dispatches");
        return createFilled(context, resource, format, "sampling").Sampled(components, viewType);
    }
    char text[320];
    std::snprintf(text, sizeof(text), "AGC graphics: sampling multisampled color target 0x%llx (%ux%u, %u samples, vk format %d) as a %ux%u texture of %u samples and guest format %u (vk %d) is not implemented", static_cast<unsigned long long>(resource.baseAddress), other->target.extent.width, other->target.extent.height, other->target.samples, static_cast<int>(other->target.format), resource.width, resource.height, resource.samples, resource.format, static_cast<int>(format));
    throw std::runtime_error(text);
}

std::shared_ptr<Texture> MultisampleTargetFirstSample(const Context& context, const GuestTextureResource& resource, VkComponentMapping components) {
    std::lock_guard lock(targetsMutex());
    discardRetiredTargets();
    const auto format = ResolveTextureFormat(resource.format);
    const MultisampleTarget* other = nullptr;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || target->target.surfaceAddress != resource.baseAddress) continue;
        const auto& rendered = target->target;
        const bool sameShape = rendered.extent.width == resource.width && rendered.extent.height == resource.height && rendered.samples == resource.samples && rendered.mip == 0;
        if (sameShape && rendered.format == format) return target->FirstSample(components);
        if (sameShape && redBlueSwapped(rendered.format, format)) return target->FirstSample(swapRedBlue(components));
        other = target.get();
    }
    if (other == nullptr) {
        Require(Recorder::Active() != nullptr, "a multisampled texture is only sampled by recorded draws and dispatches");
        return createFilled(context, resource, format, "sampling").FirstSample(components);
    }
    char text[320];
    std::snprintf(text, sizeof(text), "AGC graphics: reading sample 0 of multisampled color target 0x%llx (%ux%u, %u samples, vk format %d) as a %ux%u texture of %u samples and guest format %u (vk %d) is not implemented", static_cast<unsigned long long>(resource.baseAddress), other->target.extent.width, other->target.extent.height, other->target.samples, static_cast<int>(other->target.format), resource.width, resource.height, resource.samples, resource.format, static_cast<int>(format));
    throw std::runtime_error(text);
}

void MaterializeMultisampleTargetClear(const Context& context, const ColorTarget& target) {
    std::lock_guard lock(targetsMutex());
    acquire(context, target);
}

VkImageView MultisampleTargetStorageView(const Context& context, const GuestTextureResource& resource) {
    std::lock_guard lock(targetsMutex());
    discardRetiredTargets();
    const auto format = ResolveTextureFormat(resource.format);
    const MultisampleTarget* other = nullptr;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || target->target.surfaceAddress != resource.baseAddress) continue;
        const auto& rendered = target->target;
        if (sameStorageForm(context, rendered.format, format) && rendered.extent.width == resource.width && rendered.extent.height == resource.height && rendered.samples == resource.samples && rendered.mip == 0) return target->Storage(StorageFormatForGuest(context, resource.format));
        other = target.get();
    }
    if (other == nullptr) {
        Require(Recorder::Active() != nullptr, "a multisampled color target is only written by recorded dispatches");
        return createFilled(context, resource, format, "writing").Storage(StorageFormatForGuest(context, resource.format));
    }
    char text[320];
    std::snprintf(text, sizeof(text), "AGC graphics: storage image access to multisampled color target 0x%llx (%ux%u, %u samples, vk format %d) as a %ux%u texture of %u samples and guest format %u (vk %d) is not implemented", static_cast<unsigned long long>(resource.baseAddress), other->target.extent.width, other->target.extent.height, other->target.samples, static_cast<int>(other->target.format), resource.width, resource.height, resource.samples, resource.format, static_cast<int>(format));
    throw std::runtime_error(text);
}

bool MultisampleTargetStorable(const Context& context, const ColorTarget& target) {
    std::lock_guard lock(targetsMutex());
    return std::any_of(targets().begin(), targets().end(), [&](const auto& candidate) {
        return candidate->active->load(std::memory_order_acquire) && candidate->context.device == context.device && sameTarget(candidate->target, target) && candidate->Storable();
    });
}

void ResolveMultisampleTarget(const Context& context, const ColorTarget& source, StorageTexture& destination, VkImageView destinationView) {
    std::lock_guard lock(targetsMutex());
    auto& target = acquire(context, source);
    target.Keep();
    const std::array<VkImageView, 2> views{target.View(source.format), destinationView};
    auto framebuffer = std::make_shared<Framebuffer>(context, target.ResolvePass(source.format), views, source.extent);
    recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = target.ResolvePass(source.format);
        begin.framebuffer = framebuffer->Handle();
        begin.renderArea = {{0, 0}, source.extent};
        context.Resolved(&DeviceFunctions::cmdBeginRenderPass, "vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        context.Resolved(&DeviceFunctions::cmdEndRenderPass, "vkCmdEndRenderPass")(commands);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
        if (recorder != nullptr) {
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            recorder->Keep(framebuffer);
        }
    });
    destination.MarkDirty();
}

bool MultisampledColorFillAbsorbs(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    if (bytes == 0) return false;
    std::lock_guard lock(targetsMutex());
    return std::any_of(targets().begin(), targets().end(), [&](const auto& target) {
        VkClearColorValue clear{};
        return target->active->load(std::memory_order_acquire) && target->target.address == address && MultisampledColorBytes(target->target) == bytes && MultisampledFillClearColor(target->target, pattern, clear);
    });
}

void NoteColorMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    std::lock_guard lock(targetsMutex());
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        target->colorFill.NoteFill(address, bytes, pattern);
        if (target->htileBytes != 0 && address < target->htileAddress + target->htileBytes && target->htileAddress < address + bytes) {
            if (pattern == 0u && address <= target->htileAddress && address + bytes >= target->htileAddress + target->htileBytes) {
                target->htileRefusal.clear();
                target->colorFill.NoteFill(target->target.address, static_cast<std::size_t>(MultisampledColorBytes(target->target)), 0u);
            } else {
                char text[224];
                std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx with 0x%08x over the HTILE 0x%llx+0x%llx of a sampled multisampled depth-layout texture is not modeled", static_cast<unsigned long long>(address), bytes, pattern, static_cast<unsigned long long>(target->htileAddress), static_cast<unsigned long long>(target->htileBytes));
                target->htileRefusal = text;
            }
        }
        if (target->target.dccAddress != 0) {
            target->NoteMetadataFill(address, bytes, pattern);
            continue;
        }
        const auto before = target->cmask.Keys();
        target->cmask.NoteFill(address, bytes, pattern);
        if (before == DccKeys::Clear0000 || target->cmask.Keys() != DccKeys::Clear0000) continue;
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] multisampled color target 0x%llx fast-cleared through its CMASK 0x%llx (fill 0x%llx+0x%zx)\n", static_cast<unsigned long long>(target->target.surfaceAddress), static_cast<unsigned long long>(target->target.cmaskAddress), static_cast<unsigned long long>(address), bytes);
    }
}

std::shared_ptr<Texture> MultisampleTargetFmaskTexture_nid_no_patch(const Context& context, const GuestTextureResource& resource, VkComponentMapping components) {
    std::lock_guard lock(targetsMutex());
    discardRetiredTargets();
    std::shared_ptr<MultisampleTarget> owner;
    for (const auto& candidate : targets()) {
        if (!candidate->active->load(std::memory_order_acquire) || candidate->context.device != context.device) continue;
        const auto& target = candidate->target;
        if (target.fmaskAddress != resource.baseAddress && (resource.fmaskCmaskAddress == 0u || target.cmaskAddress != resource.fmaskCmaskAddress)) continue;
        Require(owner == nullptr, "FMASK descriptor refers to more than one resident target");
        owner = candidate;
    }
    return owner != nullptr ? owner->FmaskSampled(resource, components) : nullptr;
}

void ClearMultisampleTargets(VkDevice device) {
    std::lock_guard lock(targetsMutex());
    std::erase_if(targets(), [&](const auto& target) { return target->context.device == device; });
    std::unique_lock guardLock(guardedMutex());
    std::erase_if(guarded(), [&](const auto& range) { return range.device == device; });
    anyGuarded().store(!guarded().empty(), std::memory_order_release);
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

void RequireNoMultisampleTarget(std::uint64_t address, std::size_t bytes, const char* access) {
    RequireNoResidentDepthRead_nid_no_patch(address, bytes);
    RequireNoMultisampleColorTarget(address, bytes, access);
}

void RequireNoMultisampleColorTarget(std::uint64_t address, std::size_t bytes, const char* access) {
    if (bytes == 0 || !anyGuarded().load(std::memory_order_acquire)) return;
    std::shared_lock lock(guardedMutex());
    for (const auto& range : guarded()) {
        if (address >= range.address + range.bytes || range.address >= address + bytes) continue;
        char text[256];
        std::snprintf(text, sizeof(text), "AGC graphics: %s of 0x%llx+0x%zx overlaps multisampled color target 0x%llx+0x%llx (%u samples), whose samples are only on the host: guest access to it is not implemented", access, static_cast<unsigned long long>(address), bytes, static_cast<unsigned long long>(range.address), static_cast<unsigned long long>(range.bytes), range.samples);
        throw std::runtime_error(text);
    }
}

}
