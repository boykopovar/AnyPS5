#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"

namespace AgcDriver {

PresentationScaler::PresentationScaler(const Graphics::Context& context, VkFormat sourceFormat, VkFormat destinationFormat) : context(context), sourceFormat(sourceFormat) {
    Graphics::Require(context.formatProperties != nullptr, "missing Vulkan format property resolver");
    VkFormatProperties sourceProperties{};
    context.formatProperties(context.physical, sourceFormat, &sourceProperties);
    Graphics::Require((sourceProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0, "presentation source format does not support blit sources");
    Graphics::Require((sourceProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0, "presentation source format does not support linear blit filtering");
    VkFormatProperties destinationProperties{};
    context.formatProperties(context.physical, destinationFormat, &destinationProperties);
    Graphics::Require((destinationProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0, "presentation destination format does not support blit targets");
}

PresentationScaler::~PresentationScaler() {
    release();
    if (blackImage != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, blackImage, nullptr);
    if (blackMemory != VK_NULL_HANDLE) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, blackMemory, nullptr);
}

void PresentationScaler::release() noexcept {
    if (sourceImage != VK_NULL_HANDLE) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, sourceImage, nullptr);
    if (sourceMemory != VK_NULL_HANDLE) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, sourceMemory, nullptr);
    sourceImage = VK_NULL_HANDLE;
    sourceMemory = VK_NULL_HANDLE;
    sourceWidth = 0;
    sourceHeight = 0;
}

void PresentationScaler::EnsureSourceImage(std::uint32_t width, std::uint32_t height) {
    Graphics::Require(width != 0 && height != 0, "presentation source extent must be non-zero");
    if (sourceImage != VK_NULL_HANDLE && sourceWidth == width && sourceHeight == height) return;
    release();
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = sourceFormat;
    imageInfo.extent = {width, height, 1u};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &sourceImage), "vkCreateImage presentation source");
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, sourceImage, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &sourceMemory), "vkAllocateMemory presentation source");
    Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, sourceImage, sourceMemory, 0), "vkBindImageMemory presentation source");
    sourceWidth = width;
    sourceHeight = height;
}

void PresentationScaler::RecordUpload(VkCommandBuffer commands, VkBuffer uploadBuffer) {
    Graphics::Require(sourceImage != VK_NULL_HANDLE, "presentation source image has not been created");
    const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = sourceImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {sourceWidth, sourceHeight, 1u};
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, uploadBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void PresentationScaler::RecordClear(VkCommandBuffer commands, const VkClearColorValue& color) {
    Graphics::Require(sourceImage != VK_NULL_HANDLE, "presentation source image has not been created");
    const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = sourceImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &barrier.subresourceRange);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void PresentationScaler::RecordBlitInto(VkCommandBuffer commands, VkImage image, VkImageLayout layout, VkFilter filter) {
    RecordBlitInto(commands, image, layout, filter, sourceWidth, sourceHeight);
}

void PresentationScaler::RecordBlitInto(VkCommandBuffer commands, VkImage image, VkImageLayout layout, VkFilter filter, std::uint32_t width, std::uint32_t height, std::uint32_t layer) {
    Graphics::Require(sourceImage != VK_NULL_HANDLE && image != VK_NULL_HANDLE, "presentation image is unavailable");
    Graphics::Require(width != 0 && height != 0, "presentation blit extent must be non-zero");
    const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = sourceImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
    blit.srcOffsets[1] = {static_cast<std::int32_t>(width), static_cast<std::int32_t>(height), 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[1] = {static_cast<std::int32_t>(sourceWidth), static_cast<std::int32_t>(sourceHeight), 1};
    context.Function<PFN_vkCmdBlitImage>("vkCmdBlitImage")(commands, image, layout, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void PresentationScaler::RecordReadback(VkCommandBuffer commands, VkBuffer destination) {
    Graphics::Require(sourceImage != VK_NULL_HANDLE && destination != VK_NULL_HANDLE, "presentation readback is unavailable");
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {sourceWidth, sourceHeight, 1u};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination, 1, &copy);
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
}

void PresentationScaler::RecordBlit(VkCommandBuffer commands, VkImage destinationImage, std::uint32_t destinationWidth, std::uint32_t destinationHeight) {
    Graphics::Require(sourceImage != VK_NULL_HANDLE, "presentation source image has not been created");
    RecordBlitFrom(context, commands, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sourceWidth, sourceHeight, VK_FILTER_LINEAR, destinationImage, destinationWidth, destinationHeight);
}

void PresentationScaler::RecordBlitFrom(const Graphics::Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout layout, std::uint32_t width, std::uint32_t height, VkFilter filter, VkImage destinationImage, std::uint32_t destinationWidth, std::uint32_t destinationHeight, std::uint32_t layer) {
    Graphics::Require(image != VK_NULL_HANDLE && width != 0 && height != 0, "presentation blit source is unavailable");
    RecordStretchFrom(context, commands, image, layout, width, height, filter, destinationImage, ComputeContainRect_nid_postfix(width, height, destinationWidth, destinationHeight), layer);
}

void PresentationScaler::RecordStretch(VkCommandBuffer commands, VkImage destinationImage, const AspectFitRect& destination) {
    Graphics::Require(sourceImage != VK_NULL_HANDLE, "presentation source image has not been created");
    RecordStretchFrom(context, commands, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sourceWidth, sourceHeight, VK_FILTER_LINEAR, destinationImage, destination);
}

void PresentationScaler::RecordStretchFrom(const Graphics::Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout layout, std::uint32_t width, std::uint32_t height, VkFilter filter, VkImage destinationImage, const AspectFitRect& rect, std::uint32_t layer) {
    Graphics::Require(image != VK_NULL_HANDLE && width != 0 && height != 0, "presentation blit source is unavailable");
    Graphics::Require(destinationImage != VK_NULL_HANDLE && rect.width != 0 && rect.height != 0, "presentation blit destination is unavailable");
    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {static_cast<std::int32_t>(width), static_cast<std::int32_t>(height), 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[0] = {rect.x, rect.y, 0};
    blit.dstOffsets[1] = {rect.x + static_cast<std::int32_t>(rect.width), rect.y + static_cast<std::int32_t>(rect.height), 1};
    context.Function<PFN_vkCmdBlitImage>("vkCmdBlitImage")(commands, image, layout, destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);
}

std::vector<AspectFitRect> PresentationScaler::LetterboxBands(std::uint32_t width, std::uint32_t height, std::uint32_t destinationWidth, std::uint32_t destinationHeight) {
    const auto rect = ComputeContainRect_nid_postfix(width, height, destinationWidth, destinationHeight);
    const auto right = rect.x + static_cast<std::int32_t>(rect.width);
    const auto bottom = rect.y + static_cast<std::int32_t>(rect.height);
    std::vector<AspectFitRect> bands;
    if (rect.y > 0) bands.push_back({0, 0, destinationWidth, static_cast<std::uint32_t>(rect.y)});
    if (bottom < static_cast<std::int32_t>(destinationHeight)) bands.push_back({0, bottom, destinationWidth, destinationHeight - static_cast<std::uint32_t>(bottom)});
    if (rect.x > 0) bands.push_back({0, rect.y, static_cast<std::uint32_t>(rect.x), rect.height});
    if (right < static_cast<std::int32_t>(destinationWidth)) bands.push_back({right, rect.y, destinationWidth - static_cast<std::uint32_t>(right), rect.height});
    return bands;
}

void PresentationScaler::RecordLetterbox(VkCommandBuffer commands, std::uint32_t width, std::uint32_t height, VkImage destinationImage, std::uint32_t destinationWidth, std::uint32_t destinationHeight) {
    const auto bands = LetterboxBands(width, height, destinationWidth, destinationHeight);
    if (bands.empty()) return;
    const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    if (blackImage == VK_NULL_HANDLE) {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        imageInfo.extent = {1u, 1u, 1u};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage letterbox");
        try {
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory letterbox");
            Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory letterbox");
        } catch (...) {
            context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory != VK_NULL_HANDLE) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
            throw;
        }
        blackImage = image;
        blackMemory = memory;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = blackImage;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkClearColorValue black{};
        black.float32[3] = 1.0f;
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, blackImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &barrier.subresourceRange);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    std::vector<VkImageBlit> regions;
    regions.reserve(bands.size());
    for (const auto& band : bands) {
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[1] = {1, 1, 1};
        blit.dstSubresource = blit.srcSubresource;
        blit.dstOffsets[0] = {band.x, band.y, 0};
        blit.dstOffsets[1] = {band.x + static_cast<std::int32_t>(band.width), band.y + static_cast<std::int32_t>(band.height), 1};
        regions.push_back(blit);
    }
    context.Function<PFN_vkCmdBlitImage>("vkCmdBlitImage")(commands, blackImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data(), VK_FILTER_NEAREST);
}

}
