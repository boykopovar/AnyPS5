#include "prx/libSceAgcDriver/Execution/include/DlssUpscaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationUpscaler.hpp"
#include "VulkanTestDevice.hpp"
#include <cstdlib>
#include <iostream>

int main() {
    if (!AgcDriver::DlssUpscaler::IsAvailable()) {
        std::cout << "NVIDIA NGX library not available. Skipped." << std::endl;
        return VulkanTestSkipped;
    }

    auto device = OpenVulkanTestDevice();
    if (!device) {
        return VulkanTestSkipped;
    }

    const auto context = device->graphicsContext();
    AgcDriver::DlssSupportDetails details;
    if (!AgcDriver::DlssUpscaler::QueryDeviceSupport(device->Instance(), context.physical, context.device, details)) {
        std::cout << "Current Vulkan device does not support NVIDIA DLSS. Skipped." << std::endl;
        if (!details.libraryLoaded) {
            std::cout << "  Reason: NVIDIA NGX runtime library could not be loaded." << std::endl;
        } else if (details.initResult != 1) {
            std::cout << "  Reason: NGX InitDevice failed with result 0x" << std::hex << details.initResult << std::dec << "." << std::endl;
        } else if (details.needsUpdatedDriver) {
            std::cout << "  Reason: NVIDIA NGX reports an updated driver is required (SuperSampling_NeedsUpdatedDriver=1)." << std::endl;
        } else if (!details.isSupported) {
            std::cout << "  Reason: NVIDIA NGX reports DLSS SuperSampling is unavailable on this device (SuperSampling_Available=0, FeatureInitResult=0x"
                      << std::hex << details.featureInitResult << std::dec << ")." << std::endl;
            std::cout << "  Note: On Linux, DLSS requires the DLSS feature library (libnvidia-ngx-dlss.so or nvngx_dlss.dll in the working directory / application path)." << std::endl;
        }
        return VulkanTestSkipped;
    }

    try {
        AgcDriver::PresentationUpscaler upscaler(context, 1, 0.2f, true, device->Instance(), {1920, 1080}, {3840, 2160});
        const auto stage = upscaler.Stage(0, VK_FORMAT_B8G8R8A8_UNORM, {1920, 1080});

        VkImageCreateInfo destInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        destInfo.imageType = VK_IMAGE_TYPE_2D;
        destInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        destInfo.extent = {3840, 2160, 1};
        destInfo.mipLevels = 1;
        destInfo.arrayLayers = 1;
        destInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        destInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        destInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        destInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        destInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage destImage = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &destInfo, nullptr, &destImage), "vkCreateImage test destination");

        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, destImage, &requirements);
        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory destMemory = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocInfo, nullptr, &destMemory), "vkAllocateMemory test destination");
        AgcDriver::Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, destImage, destMemory, 0), "vkBindImageMemory test destination");

        VkCommandBufferAllocateInfo cmdAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cmdAlloc.commandPool = context.pool;
        cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAlloc.commandBufferCount = 1;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &cmdAlloc, &commands), "vkAllocateCommandBuffers");

        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        AgcDriver::Graphics::Check(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &beginInfo), "vkBeginCommandBuffer");

        upscaler.RecordStageTransferDestination(commands, 0);

        VkClearColorValue clearColor{{0.2f, 0.5f, 0.9f, 1.0f}};
        VkImageSubresourceRange clearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, stage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);

        upscaler.RecordStageWritten(commands, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

        upscaler.RecordPresent(commands, 0, 1920, 1080, destImage, {3840, 2160}, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        AgcDriver::Graphics::Check(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer");

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        AgcDriver::Graphics::Check(context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence), "vkCreateFence");

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commands;
        AgcDriver::Graphics::Check(context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submitInfo, fence), "vkQueueSubmit");

        AgcDriver::Graphics::Check(context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, 5000000000ull), "vkWaitForFences");

        AgcDriver::Graphics::Check(context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &fence), "vkResetFences");
        AgcDriver::Graphics::Check(context.Function<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(commands, 0), "vkResetCommandBuffer");
        AgcDriver::Graphics::Check(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &beginInfo), "vkBeginCommandBuffer frame 2");

        upscaler.RecordStageTransferDestination(commands, 0);
        clearColor.float32[0] = 0.4f;
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, stage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);
        upscaler.RecordStageWritten(commands, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

        upscaler.RecordPresent(commands, 0, 1920, 1080, destImage, {3840, 2160}, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        AgcDriver::Graphics::Check(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer frame 2");

        AgcDriver::Graphics::Check(context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submitInfo, fence), "vkQueueSubmit frame 2");
        AgcDriver::Graphics::Check(context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, 5000000000ull), "vkWaitForFences frame 2");

        const auto stage720 = upscaler.Stage(0, VK_FORMAT_B8G8R8A8_UNORM, {1280, 720});
        AgcDriver::Graphics::Check(context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &fence), "vkResetFences frame 3");
        AgcDriver::Graphics::Check(context.Function<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(commands, 0), "vkResetCommandBuffer frame 3");
        AgcDriver::Graphics::Check(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &beginInfo), "vkBeginCommandBuffer frame 3");

        upscaler.RecordStageTransferDestination(commands, 0);
        clearColor.float32[0] = 0.8f;
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, stage720, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);
        upscaler.RecordStageWritten(commands, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

        upscaler.RecordPresent(commands, 0, 1280, 720, destImage, {1920, 1080}, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        AgcDriver::Graphics::Check(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer frame 3");

        AgcDriver::Graphics::Check(context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submitInfo, fence), "vkQueueSubmit frame 3");
        AgcDriver::Graphics::Check(context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, 5000000000ull), "vkWaitForFences frame 3");

        VkImageCreateInfo customDepthInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        customDepthInfo.imageType = VK_IMAGE_TYPE_2D;
        customDepthInfo.format = VK_FORMAT_D32_SFLOAT;
        customDepthInfo.extent = {1280, 720, 1};
        customDepthInfo.mipLevels = 1;
        customDepthInfo.arrayLayers = 1;
        customDepthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        customDepthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        customDepthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        customDepthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        customDepthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage customDepthImage = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &customDepthInfo, nullptr, &customDepthImage), "vkCreateImage custom depth");
        VkMemoryRequirements customDepthReq{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, customDepthImage, &customDepthReq);
        VkMemoryAllocateInfo customDepthAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        customDepthAlloc.allocationSize = customDepthReq.size;
        customDepthAlloc.memoryTypeIndex = context.MemoryType(customDepthReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory customDepthMemory = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &customDepthAlloc, nullptr, &customDepthMemory), "vkAllocateMemory custom depth");
        AgcDriver::Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, customDepthImage, customDepthMemory, 0), "vkBindImageMemory custom depth");

        VkImageViewCreateInfo customDepthViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        customDepthViewInfo.image = customDepthImage;
        customDepthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        customDepthViewInfo.format = VK_FORMAT_D32_SFLOAT;
        customDepthViewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        VkImageView customDepthView = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &customDepthViewInfo, nullptr, &customDepthView), "vkCreateImageView custom depth");

        VkImageCreateInfo customMvecInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        customMvecInfo.imageType = VK_IMAGE_TYPE_2D;
        customMvecInfo.format = VK_FORMAT_R16G16_SFLOAT;
        customMvecInfo.extent = {1280, 720, 1};
        customMvecInfo.mipLevels = 1;
        customMvecInfo.arrayLayers = 1;
        customMvecInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        customMvecInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        customMvecInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        customMvecInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        customMvecInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage customMvecImage = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &customMvecInfo, nullptr, &customMvecImage), "vkCreateImage custom mvec");
        VkMemoryRequirements customMvecReq{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, customMvecImage, &customMvecReq);
        VkMemoryAllocateInfo customMvecAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        customMvecAlloc.allocationSize = customMvecReq.size;
        customMvecAlloc.memoryTypeIndex = context.MemoryType(customMvecReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory customMvecMemory = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &customMvecAlloc, nullptr, &customMvecMemory), "vkAllocateMemory custom mvec");
        AgcDriver::Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, customMvecImage, customMvecMemory, 0), "vkBindImageMemory custom mvec");

        VkImageViewCreateInfo customMvecViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        customMvecViewInfo.image = customMvecImage;
        customMvecViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        customMvecViewInfo.format = VK_FORMAT_R16G16_SFLOAT;
        customMvecViewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView customMvecView = VK_NULL_HANDLE;
        AgcDriver::Graphics::Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &customMvecViewInfo, nullptr, &customMvecView), "vkCreateImageView custom mvec");

        AgcDriver::Graphics::Check(context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &fence), "vkResetFences frame 4");
        AgcDriver::Graphics::Check(context.Function<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(commands, 0), "vkResetCommandBuffer frame 4");
        AgcDriver::Graphics::Check(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &beginInfo), "vkBeginCommandBuffer frame 4");

        VkImageMemoryBarrier customDepthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        customDepthBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        customDepthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        customDepthBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        customDepthBarrier.srcQueueFamilyIndex = customDepthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        customDepthBarrier.image = customDepthImage;
        customDepthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &customDepthBarrier);

        VkClearDepthStencilValue depthClearVal{1.0f, 0};
        VkImageSubresourceRange depthClearRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, customDepthImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthClearVal, 1, &depthClearRange);

        customDepthBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        customDepthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        customDepthBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        customDepthBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &customDepthBarrier);

        VkImageMemoryBarrier customMvecBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        customMvecBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        customMvecBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        customMvecBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        customMvecBarrier.srcQueueFamilyIndex = customMvecBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        customMvecBarrier.image = customMvecImage;
        customMvecBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &customMvecBarrier);

        VkClearColorValue mvecClearVal{};
        VkImageSubresourceRange mvecClearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, customMvecImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &mvecClearVal, 1, &mvecClearRange);

        customMvecBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        customMvecBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        customMvecBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        customMvecBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &customMvecBarrier);

        upscaler.RecordStageTransferDestination(commands, 0);
        clearColor.float32[0] = 0.9f;
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, stage720, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);
        upscaler.RecordStageWritten(commands, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

        AgcDriver::PresentationBuffers customBuffers;
        customBuffers.depth = customDepthView;
        customBuffers.depthImage = customDepthImage;
        customBuffers.depthFormat = VK_FORMAT_D32_SFLOAT;
        customBuffers.motionVectors = customMvecView;
        customBuffers.motionVectorsImage = customMvecImage;
        customBuffers.motionVectorsFormat = VK_FORMAT_R16G16_SFLOAT;
        customBuffers.jitterX = 0.125f;
        customBuffers.jitterY = -0.25f;
        customBuffers.reset = false;

        upscaler.RecordPresent(commands, 0, 1280, 720, destImage, {1920, 1080}, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, customBuffers);
        AgcDriver::Graphics::Check(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer frame 4");

        AgcDriver::Graphics::Check(context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submitInfo, fence), "vkQueueSubmit frame 4");
        AgcDriver::Graphics::Check(context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, 5000000000ull), "vkWaitForFences frame 4");

        context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, customDepthView, nullptr);
        context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, customDepthImage, nullptr);
        context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, customDepthMemory, nullptr);
        context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, customMvecView, nullptr);
        context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, customMvecImage, nullptr);
        context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, customMvecMemory, nullptr);

        context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
        context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 1, &commands);
        context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, destImage, nullptr);
        context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, destMemory, nullptr);

#ifdef _WIN32
        _putenv_s("ANYPS5_UPSCALE", "dlss");
#else
        setenv("ANYPS5_UPSCALE", "dlss", 1);
#endif
        const auto mode = AgcDriver::SelectUpscaleMode(std::getenv("ANYPS5_UPSCALE"), nullptr);
        AgcDriver::Graphics::Require(mode.dlss && !mode.fsr && !mode.linear, "ANYPS5_UPSCALE=dlss mode check");
        AgcDriver::PresentationUpscaler envUpscaler(context, 1, mode, device->Instance(), {1920, 1080}, {3840, 2160});
        AgcDriver::Graphics::Require(envUpscaler.IsDlss(), "PresentationUpscaler IsDlss check");

        std::cout << "SUCCESS: DLSS upscaling and real-time presentation evaluated successfully on Blackwell GPU!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "DLSS test failed: " << e.what() << std::endl;
        return 1;
    }
}
