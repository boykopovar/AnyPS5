#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace AgcDriver::Graphics {

VkImage StorageTexture::Image(std::uint32_t mip) const {
    Require(mip < descriptor.mipCount, "storage image mip is outside its guest surface");
    return mip < geometry.imageLevels ? image : extraMipImages.at(mip - geometry.imageLevels)->image;
}

std::uint32_t StorageTexture::ImageMipLevel(std::uint32_t mip) const {
    Require(mip < descriptor.mipCount, "storage image mip is outside its guest surface");
    return mip < geometry.imageLevels ? mip : 0u;
}

void StorageTexture::allocateExtraMipImages(VkImageCreateInfo info) {
    for (auto level = geometry.imageLevels; level < descriptor.mipCount; ++level) {
        info.extent = {std::max(descriptor.width >> level, 1u), std::max(descriptor.height >> level, 1u), std::max(geometry.imageDepth >> level, 1u)};
        Require(info.extent.width == 1u && info.extent.height == 1u && info.extent.depth == 1u, "a storage image level past the physical chain is not 1x1x1");
        info.mipLevels = 1u;
        auto extra = std::make_unique<OwnedImage>(context, VK_NULL_HANDLE, VK_NULL_HANDLE);
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &extra->image), "vkCreateImage extra storage mip");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, extra->image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.DeviceMemoryType(requirements.memoryTypeBits, requirements.size, "extra storage mip");
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &extra->memory), "vkAllocateMemory extra storage mip");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, extra->image, extra->memory, 0u), "vkBindImageMemory extra storage mip");
        Require(memoryBytes <= std::numeric_limits<VkDeviceSize>::max() - requirements.size, "storage image memory accounting overflows");
        memoryBytes += requirements.size;
        extraMipImages.push_back(std::move(extra));
    }
}

void StorageTexture::imageBarrier(VkCommandBuffer commands, VkPipelineStageFlags source, VkPipelineStageFlags destination, VkDependencyFlags flags, std::uint32_t memoryCount, const VkMemoryBarrier* memoryBarriers, std::uint32_t bufferCount, const VkBufferMemoryBarrier* bufferBarriers, std::uint32_t imageCount, const VkImageMemoryBarrier* imageBarriers) const {
    if (extraMipImages.empty()) {
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, source, destination, flags, memoryCount, memoryBarriers, bufferCount, bufferBarriers, imageCount, imageBarriers);
        return;
    }
    Require(imageCount == 1u && imageBarriers != nullptr && imageBarriers->image == image && imageBarriers->subresourceRange.baseMipLevel == 0u && imageBarriers->subresourceRange.levelCount == geometry.imageLevels, "storage image barrier does not cover its physical chain");
    std::array<VkImageMemoryBarrier, 16> barriers{};
    Require(extraMipImages.size() < barriers.size(), "storage image has too many extra mip images");
    barriers[0] = *imageBarriers;
    for (std::size_t index = 0; index < extraMipImages.size(); ++index) {
        auto& barrier = barriers[index + 1u];
        barrier = *imageBarriers;
        barrier.image = extraMipImages[index]->image;
        barrier.subresourceRange.baseMipLevel = 0u;
        barrier.subresourceRange.levelCount = 1u;
    }
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, source, destination, flags, memoryCount, memoryBarriers, bufferCount, bufferBarriers, static_cast<std::uint32_t>(extraMipImages.size() + 1u), barriers.data());
}

void StorageTexture::copyBufferToImage(VkCommandBuffer commands, VkBuffer source, VkImage destination, VkImageLayout layout, std::uint32_t count, const VkBufferImageCopy* regions) const {
    Require(destination == image, "storage upload names another image");
    if (extraMipImages.empty()) {
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, source, destination, layout, count, regions);
        return;
    }
    std::vector<VkBufferImageCopy> selected;
    selected.reserve(count);
    for (std::uint32_t physical = 0; physical <= extraMipImages.size(); ++physical) {
        selected.clear();
        const auto handle = physical == 0u ? image : extraMipImages[physical - 1u]->image;
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto level = regions[index].imageSubresource.mipLevel;
            if (Image(level) != handle) continue;
            selected.push_back(regions[index]);
            selected.back().imageSubresource.mipLevel = ImageMipLevel(level);
        }
        if (!selected.empty()) context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, source, handle, layout, static_cast<std::uint32_t>(selected.size()), selected.data());
    }
}

void StorageTexture::copyImageToBuffer(VkCommandBuffer commands, VkImage source, VkImageLayout layout, VkBuffer destination, std::uint32_t count, const VkBufferImageCopy* regions) const {
    Require(source == image, "storage readback names another image");
    if (extraMipImages.empty()) {
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, source, layout, destination, count, regions);
        return;
    }
    std::vector<VkBufferImageCopy> selected;
    selected.reserve(count);
    for (std::uint32_t physical = 0; physical <= extraMipImages.size(); ++physical) {
        selected.clear();
        const auto handle = physical == 0u ? image : extraMipImages[physical - 1u]->image;
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto level = regions[index].imageSubresource.mipLevel;
            if (Image(level) != handle) continue;
            selected.push_back(regions[index]);
            selected.back().imageSubresource.mipLevel = ImageMipLevel(level);
        }
        if (!selected.empty()) context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, handle, layout, destination, static_cast<std::uint32_t>(selected.size()), selected.data());
    }
}

void StorageTexture::clearColorImage(VkCommandBuffer commands, VkImage destination, VkImageLayout layout, const VkClearColorValue* value, std::uint32_t count, const VkImageSubresourceRange* ranges) const {
    Require(destination == image && count == 1u && ranges != nullptr && ranges->baseMipLevel == 0u && ranges->levelCount == geometry.imageLevels, "storage clear does not cover its physical chain");
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, layout, value, count, ranges);
    auto range = *ranges;
    range.levelCount = 1u;
    for (const auto& extra : extraMipImages) context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, extra->image, layout, value, 1u, &range);
}

}
