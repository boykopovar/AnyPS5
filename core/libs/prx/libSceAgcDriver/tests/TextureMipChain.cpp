#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/DescriptorBindingBuilder.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "PipelineSpecialization.hpp"
#include "RuntimeAbi.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t FormatBc3Srgb = 174;
constexpr std::uint32_t FormatR8G8B8A8Unorm = 56;

struct MockDevice {
    std::uintptr_t next = 0x10000;
    std::map<VkImage, VkImageCreateInfo> images;
    std::vector<std::pair<VkImage, VkImageSubresourceLayers>> copies;
    std::vector<std::pair<VkImage, VkImageSubresourceLayers>> uploads;
    std::vector<std::pair<VkImage, VkImageSubresourceRange>> barriers;
    std::vector<std::pair<VkImage, VkImageSubresourceRange>> views;
    std::vector<std::pair<VkImage, VkImageSubresourceLayers>> readbacks;
    struct Blit {
        VkImage source;
        VkImage target;
        VkImageBlit region;
    };
    std::vector<Blit> blits;
    std::vector<std::pair<VkImage, VkImageSubresourceRange>> clears;
    std::map<VkBuffer, VkDeviceSize> bufferSizes;
    std::map<VkDeviceMemory, std::vector<std::byte>> memories;
    std::map<VkImage, VkDeviceMemory> imageMemories;
    bool failExtraMipAllocation = false;
    std::uint32_t imageAllocationFailures = 0;
    std::vector<bool> imageReclaims;
};

MockDevice mock;

std::uint32_t fullChain(const VkExtent3D& extent) {
    return static_cast<std::uint32_t>(std::bit_width(std::max({extent.width, extent.height, extent.depth, 1u})));
}

template<typename THandle>
THandle newHandle() {
    return reinterpret_cast<THandle>(mock.next += 0x10);
}

VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice, const VkImageCreateInfo* info, const VkAllocationCallbacks*, VkImage* image) {
    *image = newHandle<VkImage>();
    mock.images[*image] = *info;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL imageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements* requirements) {
    *requirements = {65536, 1024, 0x3};
}

VKAPI_ATTR VkResult VKAPI_CALL bindImageMemory(VkDevice, VkImage image, VkDeviceMemory memory, VkDeviceSize) {
    mock.imageMemories[image] = memory;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice, VkImage, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL createImageView(VkDevice, const VkImageViewCreateInfo* info, const VkAllocationCallbacks*, VkImageView* view) {
    *view = newHandle<VkImageView>();
    mock.views.emplace_back(info->image, info->subresourceRange);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice, const VkBufferCreateInfo* info, const VkAllocationCallbacks*, VkBuffer* buffer) {
    *buffer = newHandle<VkBuffer>();
    mock.bufferSizes[*buffer] = info->size;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL bufferMemoryRequirements(VkDevice, VkBuffer buffer, VkMemoryRequirements* requirements) {
    *requirements = {(mock.bufferSizes.at(buffer) + 255u) / 256u * 256u, 256, 0x3};
}

VKAPI_ATTR VkResult VKAPI_CALL allocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    if (mock.failExtraMipAllocation && mock.images.size() == 2u && mock.imageAllocationFailures == 0u) {
        ++mock.imageAllocationFailures;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    *memory = newHandle<VkDeviceMemory>();
    mock.memories[*memory].resize(static_cast<std::size_t>(info->allocationSize));
    return VK_SUCCESS;
}

VkDeviceSize reclaimExtraMipMemory(VkDeviceSize bytes, bool newImage) {
    mock.imageReclaims.push_back(newImage);
    return bytes;
}

VKAPI_ATTR VkResult VKAPI_CALL bindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize, VkMemoryMapFlags, void** data) {
    *data = mock.memories.at(memory).data() + offset;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL unmapMemory(VkDevice, VkDeviceMemory) {}
VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*) { mock.bufferSizes.erase(buffer); }
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) { mock.memories.erase(memory); }

VKAPI_ATTR VkResult VKAPI_CALL createDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout* layout) {
    *layout = newHandle<VkDescriptorSetLayout>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL createPipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout* layout) {
    *layout = newHandle<VkPipelineLayout>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyPipelineLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL createShaderModule(VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule* module) {
    *module = newHandle<VkShaderModule>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyShaderModule(VkDevice, VkShaderModule, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL createComputePipelines(VkDevice, VkPipelineCache, std::uint32_t count, const VkComputePipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline* pipelines) {
    for (std::uint32_t index = 0; index < count; ++index) pipelines[index] = newHandle<VkPipeline>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL createDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool* pool) {
    *pool = newHandle<VkDescriptorPool>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL resetDescriptorPool(VkDevice, VkDescriptorPool, VkDescriptorPoolResetFlags) { return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL allocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo* info, VkDescriptorSet* sets) {
    for (std::uint32_t index = 0; index < info->descriptorSetCount; ++index) sets[index] = newHandle<VkDescriptorSet>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL updateDescriptorSets(VkDevice, std::uint32_t, const VkWriteDescriptorSet*, std::uint32_t, const VkCopyDescriptorSet*) {}

VKAPI_ATTR VkResult VKAPI_CALL allocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* commands) {
    for (std::uint32_t index = 0; index < info->commandBufferCount; ++index) commands[index] = newHandle<VkCommandBuffer>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL freeCommandBuffers(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*) {}

VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    *fence = newHandle<VkFence>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice, VkFence, const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL beginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL endCommandBuffer(VkCommandBuffer) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL queueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL waitForFences(VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL getFenceStatus(VkDevice, VkFence) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL queueWaitIdle(VkQueue) { return VK_SUCCESS; }

VKAPI_ATTR void VKAPI_CALL cmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t count, const VkImageMemoryBarrier* barriers) {
    for (std::uint32_t index = 0; index < count; ++index) mock.barriers.emplace_back(barriers[index].image, barriers[index].subresourceRange);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, std::uint32_t, const VkBufferCopy*) {}

VKAPI_ATTR void VKAPI_CALL cmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage image, VkImageLayout, std::uint32_t count, const VkBufferImageCopy* regions) {
    for (std::uint32_t index = 0; index < count; ++index) {
        mock.copies.emplace_back(image, regions[index].imageSubresource);
        mock.uploads.emplace_back(image, regions[index].imageSubresource);
    }
}

VKAPI_ATTR void VKAPI_CALL cmdCopyImageToBuffer(VkCommandBuffer, VkImage image, VkImageLayout, VkBuffer, std::uint32_t count, const VkBufferImageCopy* regions) {
    for (std::uint32_t index = 0; index < count; ++index) mock.readbacks.emplace_back(image, regions[index].imageSubresource);
}

VKAPI_ATTR void VKAPI_CALL cmdBlitImage(VkCommandBuffer, VkImage source, VkImageLayout, VkImage target, VkImageLayout, std::uint32_t count, const VkImageBlit* regions, VkFilter) {
    for (std::uint32_t index = 0; index < count; ++index) {
        mock.readbacks.emplace_back(source, regions[index].srcSubresource);
        mock.copies.emplace_back(target, regions[index].dstSubresource);
        mock.blits.push_back({source, target, regions[index]});
    }
}

VKAPI_ATTR void VKAPI_CALL cmdClearColorImage(VkCommandBuffer, VkImage image, VkImageLayout, const VkClearColorValue*, std::uint32_t count, const VkImageSubresourceRange* ranges) {
    for (std::uint32_t index = 0; index < count; ++index) mock.clears.emplace_back(image, ranges[index]);
}

VKAPI_ATTR void VKAPI_CALL cmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL cmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, std::uint32_t, std::uint32_t, const VkDescriptorSet*, std::uint32_t, const std::uint32_t*) {}
VKAPI_ATTR void VKAPI_CALL cmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) {}
VKAPI_ATTR void VKAPI_CALL cmdDispatch(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t) {}

PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(createImage)},
        {"vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(imageMemoryRequirements)},
        {"vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(bindImageMemory)},
        {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(destroyImage)},
        {"vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(createImageView)},
        {"vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(destroyImageView)},
        {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(createBuffer)},
        {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(bufferMemoryRequirements)},
        {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(allocateMemory)},
        {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(bindBufferMemory)},
        {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(mapMemory)},
        {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(unmapMemory)},
        {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(destroyBuffer)},
        {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(freeMemory)},
        {"vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(createDescriptorSetLayout)},
        {"vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(destroyDescriptorSetLayout)},
        {"vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(createPipelineLayout)},
        {"vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(destroyPipelineLayout)},
        {"vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(createShaderModule)},
        {"vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(destroyShaderModule)},
        {"vkCreateComputePipelines", reinterpret_cast<PFN_vkVoidFunction>(createComputePipelines)},
        {"vkDestroyPipeline", reinterpret_cast<PFN_vkVoidFunction>(destroyPipeline)},
        {"vkCreateDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(createDescriptorPool)},
        {"vkDestroyDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(destroyDescriptorPool)},
        {"vkResetDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(resetDescriptorPool)},
        {"vkAllocateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(allocateDescriptorSets)},
        {"vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(updateDescriptorSets)},
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(allocateCommandBuffers)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(freeCommandBuffers)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(createFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(destroyFence)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(beginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(endCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(queueSubmit)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(waitForFences)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(getFenceStatus)},
        {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(queueWaitIdle)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(cmdPipelineBarrier)},
        {"vkCmdCopyBuffer", reinterpret_cast<PFN_vkVoidFunction>(cmdCopyBuffer)},
        {"vkCmdCopyBufferToImage", reinterpret_cast<PFN_vkVoidFunction>(cmdCopyBufferToImage)},
        {"vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(cmdCopyImageToBuffer)},
        {"vkCmdBlitImage", reinterpret_cast<PFN_vkVoidFunction>(cmdBlitImage)},
        {"vkCmdClearColorImage", reinterpret_cast<PFN_vkVoidFunction>(cmdClearColorImage)},
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(cmdBindPipeline)},
        {"vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(cmdBindDescriptorSets)},
        {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(cmdPushConstants)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(cmdDispatch)},
    };
    const auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}

void formatProperties(VkPhysicalDevice, VkFormat, VkFormatProperties* properties) {
    *properties = {};
    properties->optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
}

Context mockContext() {
    static int deviceTag = 0;
    static int physicalTag = 0;
    Context context{};
    context.device = reinterpret_cast<VkDevice>(&deviceTag);
    context.physical = reinterpret_cast<VkPhysicalDevice>(&physicalTag);
    context.formatProperties = formatProperties;
    context.deviceProc = deviceProc;
    context.memory.memoryTypeCount = 2;
    context.memory.memoryTypes[0] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0};
    context.memory.memoryTypes[1] = {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 1};
    context.memory.memoryHeapCount = 2;
    context.memory.memoryHeaps[0] = {std::uint64_t{1} << 32u, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    context.memory.memoryHeaps[1] = {std::uint64_t{1} << 32u, 0};
    context.limits.maxMemoryAllocationCount = 4096;
    context.limits.nonCoherentAtomSize = 64;
    context.limits.minStorageBufferOffsetAlignment = 16;
    context.limits.maxStorageBufferRange = 1u << 30u;
    context.limits.maxImageDimension1D = 16384;
    context.limits.maxImageDimension2D = 16384;
    context.limits.maxImageDimension3D = 2048;
    context.limits.maxImageDimensionCube = 16384;
    context.limits.maxImageArrayLayers = 2048;
    for (auto& count : context.limits.maxComputeWorkGroupCount) count = 65535u;
    context.textureCompressionBC = true;
    return context;
}

GuestTextureResource arrayTexture(std::uint64_t address, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t layers, std::uint32_t mipCount) {
    GuestTextureResource descriptor{};
    descriptor.baseAddress = address;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.depthOrLastArray = layers - 1u;
    descriptor.mipCount = mipCount;
    descriptor.lastLevel = mipCount - 1u;
    descriptor.tileMode = TextureTileMode::kStandard64KB;
    descriptor.dimension = TextureDimension::k2DArray;
    descriptor.format = format;
    descriptor.dstSelX = 4;
    descriptor.dstSelY = 5;
    descriptor.dstSelZ = 6;
    descriptor.dstSelW = 7;
    return descriptor;
}

void clearRecords() {
    mock.images.clear();
    mock.imageMemories.clear();
    mock.copies.clear();
    mock.uploads.clear();
    mock.barriers.clear();
    mock.views.clear();
    mock.readbacks.clear();
    mock.blits.clear();
    mock.clears.clear();
}

void checkInsideImage(VkImage image, const VkImageCreateInfo& info, const std::string& what) {
    const auto levels = std::to_string(info.mipLevels);
    Require(info.mipLevels <= fullChain(info.extent), what + ": the image was created with " + levels + " mip levels, more than its " + std::to_string(info.extent.width) + "x" + std::to_string(info.extent.height) + " extent can hold (" + std::to_string(fullChain(info.extent)) + ")");
    const auto checkLayers = [&](const VkImageSubresourceLayers& layers, const char* access) {
        Require(layers.mipLevel < info.mipLevels, what + ": " + access + " level " + std::to_string(layers.mipLevel) + " of an image of " + levels + " levels");
        Require(layers.baseArrayLayer + layers.layerCount <= info.arrayLayers, what + ": " + access + " past the image's layers");
    };
    const auto checkRange = [&](const VkImageSubresourceRange& range, const char* access) {
        Require(range.levelCount != VK_REMAINING_MIP_LEVELS && range.levelCount != 0 && range.baseMipLevel + range.levelCount <= info.mipLevels, what + ": " + access + " names levels " + std::to_string(range.baseMipLevel) + "+" + std::to_string(range.levelCount) + " of an image of " + levels + " levels");
        Require(range.layerCount != 0 && (range.layerCount == VK_REMAINING_ARRAY_LAYERS ? range.baseArrayLayer < info.arrayLayers : range.baseArrayLayer + range.layerCount <= info.arrayLayers), what + ": " + access + " names layers " + std::to_string(range.baseArrayLayer) + "+" + std::to_string(range.layerCount) + " of an image of " + std::to_string(info.arrayLayers) + " layers");
    };
    for (const auto& [target, layers] : mock.copies) {
        if (target == image) checkLayers(layers, "a copy wrote");
    }
    for (const auto& [target, layers] : mock.readbacks) {
        if (target == image) checkLayers(layers, "a copy read");
    }
    for (const auto& blit : mock.blits) {
        if (blit.source == image) checkLayers(blit.region.srcSubresource, "a blit read");
        if (blit.target == image) checkLayers(blit.region.dstSubresource, "a blit wrote");
    }
    for (const auto& [target, range] : mock.barriers) {
        if (target == image) checkRange(range, "a barrier");
    }
    for (const auto& [target, range] : mock.clears) {
        if (target == image) checkRange(range, "a clear");
    }
    for (const auto& [target, range] : mock.views) {
        if (target == image) checkRange(range, "a view");
    }
}

VkImageCreateInfo upload(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, const std::string& what) {
    clearRecords();
    const auto geometry = DescribeSurface(descriptor);
    const std::vector<std::byte> snapshot(static_cast<std::size_t>(geometry.guestBytes));
    VkImageCreateInfo info{};
    {
        const auto texture = std::make_shared<Texture>(context, detiler, descriptor, VkComponentMapping{}, snapshot);
        Require(mock.images.size() == 1, what + ": the upload did not create exactly one image");
        const auto image = mock.images.begin()->first;
        info = mock.images.begin()->second;
        for (const auto& [target, layers] : mock.copies) Require(target == image, what + ": a copy went to another image");
        Require(!mock.views.empty(), what + ": the texture made no view");
        for (const auto& [target, range] : mock.views) Require(target == image, what + ": a view names another image");
        checkInsideImage(image, info, what);
    }
    return info;
}

std::size_t copiesOfLevel(std::uint32_t level) {
    std::size_t count = 0;
    for (const auto& [target, layers] : mock.copies) count += layers.mipLevel == level ? 1u : 0u;
    return count;
}

void TestLevelsPastTheFullChain(const Context& context, TextureDetiler& detiler) {
    constexpr std::uint32_t layers = 4;
    const auto descriptor = arrayTexture(0x1003000000, FormatBc3Srgb, 28, 1, layers, 10);
    const auto info = upload(context, detiler, descriptor, "28x1 BC3 with 10 levels");
    Require(info.mipLevels == 5, "a 28x1 BC3 texture of 10 guest levels must keep the 5 levels its extent has");
    for (std::uint32_t level = 0; level < 5; ++level) Require(copiesOfLevel(level) == layers, "level " + std::to_string(level) + " of every layer must be uploaded");

    auto pastChain = descriptor;
    pastChain.baseLevel = 7;
    upload(context, detiler, pastChain, "a view of levels 7-9 of a 28x1 surface");
    Require(mock.views.front().second.baseMipLevel == 4 && mock.views.front().second.levelCount == 1, "a view of levels past the image's last one must read its last level");

    const auto narrow = upload(context, detiler, arrayTexture(0x1021400000, FormatBc3Srgb, 4, 1, 2, 11), "4x1 BC3 with 11 levels");
    Require(narrow.mipLevels == 3, "a 4x1 texture of 11 guest levels must keep 3 levels");
}

void TestClearedMipRanges(const Context& context, TextureDetiler& detiler) {
    for (const auto [width, height, levels, expected] : {std::array{28u, 1u, 10u, 5u}, std::array{64u, 32u, 7u, 7u}, std::array{64u, 32u, 3u, 3u}}) {
        const auto descriptor = arrayTexture(0x1068600000, FormatR8G8B8A8Unorm, width, height, 4u, levels);
        clearRecords();
        VkClearColorValue clear{};
        clear.float32[0] = 0.5f;
        const auto texture = std::make_shared<Texture>(context, detiler, descriptor, VkComponentMapping{}, std::span<const std::byte>{}, false, &clear);
        Require(mock.images.size() == 1u, "a sampled GPU clear did not create exactly one image");
        const auto [image, info] = *mock.images.begin();
        Require(info.mipLevels == expected, "a sampled GPU clear allocated the wrong mip count");
        Require(mock.clears.size() == 1u && mock.clears.front().first == image, "a sampled GPU clear did not clear its image exactly once");
        Require(mock.copies.empty(), "a sampled GPU clear uploaded guest texels");
        checkInsideImage(image, info, "a GPU-cleared sampled texture");
        Require(mock.clears.front().second.baseMipLevel == 0u && mock.clears.front().second.levelCount == expected && mock.clears.front().second.baseArrayLayer == 0u && mock.clears.front().second.layerCount == 4u, "a sampled GPU clear did not cover every allocated mip and layer");
    }
}

void TestFullChainsAreUnchanged(const Context& context, TextureDetiler& detiler) {
    const auto full = upload(context, detiler, arrayTexture(0x1068400000, FormatBc3Srgb, 64, 64, 2, 7), "64x64 BC3 with 7 levels");
    Require(full.mipLevels == 7, "a full 64x64 chain of 7 levels must keep all of them");
    for (std::uint32_t level = 0; level < 7; ++level) Require(copiesOfLevel(level) == 2, "every level of a full chain must be uploaded");
    const auto partial = upload(context, detiler, arrayTexture(0x1068500000, FormatR8G8B8A8Unorm, 64, 32, 1, 3), "64x32 RGBA8 with 3 levels");
    Require(partial.mipLevels == 3, "a partial chain must keep its own level count");
}

class GuestSurface {
public:
    explicit GuestSurface(std::size_t bytes) : bytes((bytes + Alignment - 1) / Alignment * Alignment), data(::operator new(this->bytes, std::align_val_t{Alignment})) {
        std::memset(data, 0x5a, this->bytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(data, this->bytes, true, true, true);
    }
    ~GuestSurface() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
        ::operator delete(data, std::align_val_t{Alignment});
    }
    GuestSurface(const GuestSurface&) = delete;
    GuestSurface& operator=(const GuestSurface&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }

private:
    static constexpr std::size_t Alignment = 65536;
    std::size_t bytes;
    void* data;
};

GuestTextureResource storageSurface(std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    GuestTextureResource descriptor{};
    descriptor.width = width;
    descriptor.height = height;
    descriptor.mipCount = mipCount;
    descriptor.lastLevel = mipCount - 1u;
    descriptor.tileMode = TextureTileMode::kStandard64KB;
    descriptor.dimension = TextureDimension::k2D;
    descriptor.format = FormatR8G8B8A8Unorm;
    descriptor.dstSelX = 4;
    descriptor.dstSelY = 5;
    descriptor.dstSelZ = 6;
    descriptor.dstSelW = 7;
    return descriptor;
}

void requireThrows(const std::function<void()>& action, const std::string& expected, const std::string& what) {
    try {
        action();
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find(expected) != std::string_view::npos, what + ": threw \"" + error.what() + "\" instead of \"" + expected + "\"");
        return;
    }
    Require(false, what + ": did not throw");
}

std::pair<VkImage, VkImageCreateInfo> storageImage(const std::string& what) {
    Require(mock.images.size() == 1, what + ": the storage texture did not create exactly one image");
    return *mock.images.begin();
}

void checkStorageCopyImages(const StorageTexture& source, const std::string& what) {
    const auto geometry = DescribeSurface(source.Descriptor());
    Require(mock.images.size() == 2u + source.Descriptor().mipCount - geometry.imageLevels, what + ": the storage source and copy have an unexpected image count");
    for (const auto& [image, info] : mock.images) {
        bool extra = false;
        for (auto level = geometry.imageLevels; level < source.Descriptor().mipCount; ++level) extra = extra || source.Image(level) == image;
        Require(info.mipLevels == (extra ? 1u : geometry.imageLevels), what + ": the source or copy recreated the overlong mip chain");
        checkInsideImage(image, info, what);
    }
}

void TestSrgbExpansionLevels(const Context& context, TextureDetiler& detiler) {
    auto expanded = context;
    const auto format = ShaderRecompiler::IrBufferFormat::Format8Srgb;
    expanded.srgbDecodeFormats = ShaderRecompiler::SrgbDecodeBit(format);
    expanded.srgbCopyFormats = expanded.srgbDecodeFormats;
    for (const auto base : {0u, 7u}) {
        auto descriptor = arrayTexture(0x1004000000u, static_cast<std::uint32_t>(format), 28u, 1u, 4u, 10u);
        descriptor.baseLevel = base;
        const auto geometry = DescribeSurface(descriptor);
        const std::vector<std::byte> snapshot(static_cast<std::size_t>(geometry.guestBytes));
        clearRecords();
        const auto texture = std::make_shared<Texture>(expanded, detiler, descriptor, VkComponentMapping{}, snapshot);
        Require(mock.images.size() == 2u, "sRGB expansion did not create an upload and expanded image");
        Require(!mock.readbacks.empty(), "sRGB expansion did not record a blit");
        for (const auto& [image, info] : mock.images) {
            Require(info.mipLevels == 5u, "sRGB expansion recreated the overlong mip chain");
            checkInsideImage(image, info, "sRGB sampled expansion");
        }
    }
    auto descriptor = storageSurface(28u, 1u, 10u);
    descriptor.format = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format8UNorm);
    GuestSurface memory(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
    descriptor.baseAddress = memory.Address();
    clearRecords();
    const auto source = StorageTexture::Create(expanded, detiler, descriptor, 0u);
    descriptor.format = static_cast<std::uint32_t>(format);
    descriptor.baseLevel = 7u;
    Require(Texture::CanSrgbExpandFrom(expanded, *source, descriptor), "sRGB storage expansion fixture is not eligible");
    const auto texture = std::make_shared<Texture>(expanded, source, descriptor, VkComponentMapping{}, Texture::SrgbExpandTag{});
    Require(texture->RefreshDepthCopy(), "sRGB storage expansion did not refresh");
    Require(!mock.readbacks.empty(), "sRGB storage expansion did not blit its copy");
    checkStorageCopyImages(*source, "sRGB storage expansion");
}

void TestRepackCopyLevels(const Context& context, TextureDetiler& detiler) {
    auto copied = context;
    copied.detiler = &detiler;
    for (const auto base : {0u, 7u}) {
        auto descriptor = storageSurface(28u, 1u, 10u);
        descriptor.format = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format32UInt);
        GuestSurface memory(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
        descriptor.baseAddress = memory.Address();
        clearRecords();
        const auto source = StorageTexture::Create(copied, detiler, descriptor, 0u);
        descriptor.format = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format10_11_11Float);
        descriptor.baseLevel = base;
        Require(Texture::CanRepackCopyFrom(*source, descriptor), "repacked storage copy fixture is not eligible");
        const auto texture = std::make_shared<Texture>(copied, source, descriptor, VkComponentMapping{}, Texture::RepackCopyTag{});
        Require(texture->RefreshDepthCopy(), "repacked storage copy did not refresh");
        Require(!mock.readbacks.empty(), "repacked storage copy did not read its source");
        checkStorageCopyImages(*source, "repacked storage copy");
    }
}

void TestExtraMipAllocationRecovery() {
    auto context = mockContext();
    context.cacheReclaim = reclaimExtraMipMemory;
    const auto pool = GetBufferPool(context);
    TextureDetiler detiler(context);
    auto descriptor = storageSurface(28u, 1u, 10u);
    GuestSurface memory(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
    descriptor.baseAddress = memory.Address();
    clearRecords();
    mock.imageAllocationFailures = 0u;
    mock.imageReclaims.clear();
    mock.failExtraMipAllocation = true;
    {
        const auto texture = StorageTexture::Create(context, detiler, descriptor, 0u);
        mock.failExtraMipAllocation = false;
        Require(mock.imageAllocationFailures == 1u && mock.imageReclaims.size() == 1u, "extra-mip allocation recovery did not exercise its allocation failure");
        Require(texture->AllocationBytes() == 6u * 65536u, "extra-mip allocation bytes do not include every physical image");
    }
    const auto retained = pool->RetainedImages();
    std::size_t matched = 0;
    std::vector<std::unique_ptr<OwnedImage>> reused;
    for (std::uint32_t index = 0; index < 6u; ++index) {
        auto image = std::make_unique<OwnedImage>(context, VK_NULL_HANDLE, VK_NULL_HANDLE);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = 65536u;
        allocation.memoryTypeIndex = 0u;
        image->Allocate(context, allocation, 1u, "extra-mip allocation reuse", true);
        matched += std::ranges::any_of(mock.imageMemories, [&](const auto& entry) { return entry.second == image->memory; });
        reused.push_back(std::move(image));
    }
    Require(mock.imageReclaims.front() && retained == 6u && matched == 6u,
        "extra-mip allocation recovery omitted new-image eligibility or allocation identity: new-image=" + std::to_string(mock.imageReclaims.front()) + ", retained=" + std::to_string(retained) + "/6, reused=" + std::to_string(matched) + "/6");
}

void TestStorageLevelsPastTheFullChain(const Context& context, TextureDetiler& detiler) {
    auto descriptor = storageSurface(28, 1, 10);
    GuestSurface memory(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
    descriptor.baseAddress = memory.Address();
    clearRecords();
    {
        const auto texture = StorageTexture::Create(context, detiler, descriptor, 0);
        const auto image = texture->Image();
        const auto info = mock.images.at(image);
        Require(info.mipLevels == 5 && mock.images.size() == 6, "a 28x1 storage surface needs its five-level chain and five extra 1x1 images");
        for (std::uint32_t level = 0; level < 5; ++level) {
            Require(std::count_if(mock.copies.begin(), mock.copies.end(), [&](const auto& copy) { return copy.first == image && copy.second.mipLevel == level; }) == 1, "every level of the core storage chain must be uploaded once");
        }
        checkInsideImage(image, info, "28x1 core storage image");
        for (std::uint32_t level = 5; level < 10; ++level) {
            const auto extra = texture->Image(level);
            Require(extra != image, "an extra guest mip aliases the physical chain");
            const auto& created = mock.images.at(extra);
            Require(created.extent.width == 1 && created.extent.height == 1 && created.mipLevels == 1, "an extra guest mip is not backed by a legal 1x1 image");
            Require(std::count_if(mock.copies.begin(), mock.copies.end(), [&](const auto& copy) { return copy.first == extra && copy.second.mipLevel == 0; }) == 1, "an extra guest mip was not uploaded exactly once");
            static_cast<void>(texture->StorageView(level, false));
            static_cast<void>(texture->AttachmentView(VK_FORMAT_R8G8B8A8_UNORM, level));
            checkInsideImage(extra, created, "28x1 extra storage mip");
        }
        requireThrows([&] { static_cast<void>(texture->StorageView(10, false)); }, "storage texture mip level is outside the texture", "a storage view outside the guest chain");
        const std::array<std::uint32_t, 4> pattern{0, 0, 0, 0};
        const char* refusal = nullptr;
        Require(!texture->FillClear(pattern, StorageTexture::WholeImage, refusal), "the existing full-surface fill fast path must keep its extra-level admission rule");
        Require(refusal != nullptr && std::string_view(refusal) == "levels past the image", "the fill refusal changed");
        auto otherDescriptor = descriptor;
        GuestSurface otherMemory(static_cast<std::size_t>(DescribeSurface(otherDescriptor).guestBytes));
        otherDescriptor.baseAddress = otherMemory.Address();
        const auto other = StorageTexture::Create(context, detiler, otherDescriptor, 0);
        refusal = nullptr;
        Require(!texture->CopyFrom(*other, refusal), "the existing full-surface copy fast path must keep its extra-level admission rule");
        Require(refusal != nullptr && std::string_view(refusal) == "levels past the image", "the copy refusal changed");
        mock.readbacks.clear();
        texture->MarkDirty();
        texture->WriteBack();
        for (std::uint32_t level = 0; level < 10; ++level) {
            const auto physical = texture->Image(level);
            Require(std::any_of(mock.readbacks.begin(), mock.readbacks.end(), [&](const auto& readback) { return readback.first == physical && readback.second.mipLevel == texture->ImageMipLevel(level); }), "the write-back did not read every guest mip from its physical image");
            checkInsideImage(physical, mock.images.at(physical), "28x1 storage write-back");
        }
    }
    clearRecords();
    const auto texture = StorageTexture::Create(context, detiler, descriptor, 7);
    Require(mock.images.size() == 6, "creating a storage view of an extra mip lost its sibling backing");
    Require(!mock.views.empty() && mock.views.back().first == texture->Image(7) && mock.views.back().second.baseMipLevel == 0, "the default storage view did not select its extra mip image");
}

void TestStorageFullChainIsUnchanged(const Context& context, TextureDetiler& detiler) {
    auto descriptor = storageSurface(64, 32, 7);
    GuestSurface memory(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
    descriptor.baseAddress = memory.Address();
    clearRecords();
    const auto texture = StorageTexture::Create(context, detiler, descriptor, 6);
    const auto [image, info] = storageImage("64x32 storage with 7 levels");
    Require(info.mipLevels == 7, "a full 64x32 storage chain of 7 levels must keep all of them");
    for (std::uint32_t level = 0; level < 7; ++level) Require(copiesOfLevel(level) == 1, "every level of a full storage chain must be uploaded");
    checkInsideImage(image, info, "64x32 storage upload");
}

void TestSrgbLevelsPastTheFullChain(const Context& original) {
    for (const auto format : {ShaderRecompiler::IrBufferFormat::Format8Srgb, ShaderRecompiler::IrBufferFormat::Format8_8Srgb}) {
        auto context = original;
        context.srgbDecodeFormats |= ShaderRecompiler::SrgbDecodeBit(format);
        context.srgbCopyFormats |= ShaderRecompiler::SrgbDecodeBit(format);
        TextureDetiler detiler(context);
        for (const auto levels : {5u, 10u}) {
            auto descriptor = arrayTexture(0x1070000000, static_cast<std::uint32_t>(format), 28, 1, 2, levels);
            const std::vector<std::byte> snapshot(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
            clearRecords();
            {
                const Texture texture(context, detiler, descriptor, VkComponentMapping{}, snapshot);
                Require(mock.images.size() == 2, "an sRGB upload needs the source and expanded images");
                Require(mock.blits.size() == 5, "an sRGB upload must blit each physical level once");
                for (const auto& [image, info] : mock.images) {
                    Require(info.mipLevels == 5 && info.arrayLayers == 2, "an sRGB upload lost its physical mip chain or layers");
                    checkInsideImage(image, info, "sRGB sampled upload");
                }
                for (std::uint32_t level = 0; level < 5; ++level) {
                    const auto& blit = mock.blits[level];
                    Require(blit.region.srcSubresource.mipLevel == level && blit.region.dstSubresource.mipLevel == level && blit.region.srcSubresource.layerCount == 2 && blit.region.dstSubresource.layerCount == 2, "an sRGB upload skipped a physical mip or layer");
                    for (const auto& [target, copied] : mock.uploads) Require(target == blit.source, "an sRGB upload copied guest bytes into its expanded image");
                    Require(blit.source != blit.target, "an sRGB upload blitted its image onto itself");
                }
            }
            descriptor = storageSurface(28, 1, levels);
            descriptor.format = static_cast<std::uint32_t>(ShaderRecompiler::SrgbUnormFormat(format));
            GuestSurface memory(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
            descriptor.baseAddress = memory.Address();
            clearRecords();
            {
                const auto source = StorageTexture::Create(context, detiler, descriptor, 0);
                descriptor.format = static_cast<std::uint32_t>(format);
                for (const auto base : {0u, 4u}) {
                    descriptor.baseLevel = base;
                    mock.blits.clear();
                    Texture texture(context, source, descriptor, VkComponentMapping{}, Texture::SrgbExpandTag{});
                    Require(mock.views.back().second.baseMipLevel == base && mock.views.back().second.levelCount == 5u - base, "an sRGB storage copy view names nonphysical levels");
                    Require(texture.RefreshDepthCopy(), "an sRGB storage copy did not refresh");
                    Require(mock.blits.size() == 5, "an sRGB storage copy must blit each physical level once");
                    for (std::uint32_t level = 0; level < 5; ++level) {
                        const auto& blit = mock.blits[level];
                        Require(blit.source == source->Image() && blit.source != blit.target, "an sRGB storage copy read the wrong image");
                        Require(blit.region.srcSubresource.mipLevel == level && blit.region.dstSubresource.mipLevel == level, "an sRGB storage copy skipped a physical mip");
                    }
                    for (const auto& [image, info] : mock.images) checkInsideImage(image, info, "sRGB storage copy");
                }
            }
        }
    }
}

ShaderRecompiler::DescriptorValue imageWords(std::uint32_t type, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t base, std::uint32_t last, std::uint32_t maxMip) {
    return {{0x00010000u, (FormatR8G8B8A8Unorm << 20u) | (((width - 1u) & 3u) << 30u), ((width - 1u) >> 2u) | ((height - 1u) << 14u), 0xfacu | (base << 12u) | (last << 16u) | (type << 28u), depth - 1u, maxMip << 4u, 0u, 0u}, 8u};
}

void TestShaderLevelsPastTheView(const Context& context, TextureDetiler& detiler) {
    struct Case {
        const char* what;
        ShaderRecompiler::DescriptorValue words;
        std::uint32_t past;
        bool base;
    };
    const std::array cases{
        Case{"28x1 2D array of levels 0-9", imageWords(13, 28, 1, 4, 0, 9, 9), 5u, false},
        Case{"28x1 2D array of levels 7-9", imageWords(13, 28, 1, 4, 7, 9, 9), 2u, true},
        Case{"28x1 2D array of level 7", imageWords(13, 28, 1, 4, 7, 7, 9), 0u, true},
        Case{"28x1 2D of levels 2-6 of 10", imageWords(9, 28, 1, 1, 2, 6, 9), 2u, false},
        Case{"4x1 2D of levels 0-10", imageWords(9, 4, 1, 1, 0, 10, 10), 8u, false},
        Case{"4x4x16 3D of levels 0-7", imageWords(10, 4, 4, 16, 0, 7, 7), 3u, false},
        Case{"64x64 2D of levels 0-6", imageWords(9, 64, 64, 1, 0, 6, 6), 0u, false},
        Case{"64x32 2D naming levels 0-9 of 7", imageWords(9, 64, 32, 1, 0, 9, 6), 0u, false},
    };
    for (const auto& item : cases) {
        const auto descriptor = DecodeTextureResource(item.words.dwords);
        static_cast<void>(upload(context, detiler, descriptor, item.what));
        const auto& view = mock.views.front().second;
        const auto guestLevels = descriptor.lastLevel - descriptor.baseLevel + 1u;
        const auto past = ShaderRecompiler::ResourceMaterializer::LevelsPastChain(item.words);
        Require(past.levels == item.past && guestLevels - view.levelCount == item.past, std::string(item.what) + ": the shader adds " + std::to_string(past.levels) + " levels to the view's " + std::to_string(view.levelCount) + ", the T# names " + std::to_string(guestLevels));
        Require(past.base == item.base && (view.baseMipLevel != descriptor.baseLevel) == item.base, std::string(item.what) + ": the shader and the view disagree on whether the base level lies past the chain");
    }

    ShaderRecompiler::ImageResource image{};
    image.resourceClass = ShaderRecompiler::ImageResourceClass::Sampled;
    image.numericClass = ShaderRecompiler::IrTextureNumericClass::Float;
    image.dimension = ShaderRecompiler::RdnaImageDimension::Dim2DArray;
    image.read = true;
    image.levelUses = ShaderRecompiler::ImageLevelUseQuery;
    ShaderRecompiler::ShaderInfo info;
    info.images = {image};
    info.runtimeImageModes = {ShaderRecompiler::ResourceMaterializer::RuntimeImageModes(image)};
    ShaderRecompiler::ResourceSnapshot snapshot;
    snapshot.images = {cases[0].words};
    ShaderRecompiler::IrBindingLayout layout;
    layout.descriptors = {{ShaderRecompiler::DescriptorBindingForImage(image), {0u}}};
    const auto direct = ShaderRecompiler::DescriptorBindingBuilder{}.Prepare(layout, info, ShaderRecompiler::IrShaderStage::Compute, snapshot);
    Require(std::ranges::count(direct.specialization, ShaderRecompiler::PipelineSpecializationConstant{ShaderRecompiler::PipelineSpecialization::LevelsPastViewBase, 5u}) == 1, "image_get_resinfo of a 28x1 T# of levels 0-9: the pipeline is not specialized with the 5 levels its view lacks");
    layout.runtimeImageCount = 1u;
    const auto registration = ShaderRecompiler::DescriptorBindingBuilder{}.Prepare(layout, info, ShaderRecompiler::IrShaderStage::Compute, snapshot, {}, true);
    Require(registration.imageMetadata.size() == 1u && ((registration.imageMetadata[0].metadata.flags >> ShaderRecompiler::RuntimeAbi::LevelsPastViewShift) & ShaderRecompiler::RuntimeAbi::LevelsPastViewMask) == 5u, "image_get_resinfo of a 28x1 T# of levels 0-9: the runtime image metadata does not carry the 5 levels its view lacks");

    const auto accepts = [&](std::uint8_t uses, std::uint32_t table, const ShaderRecompiler::DescriptorValue& words) {
        auto used = image;
        used.levelUses = uses;
        used.table = table;
        const auto modes = ShaderRecompiler::ResourceMaterializer::RuntimeImageModes(used);
        const auto match = ShaderRecompiler::ResourceMaterializer::TryRuntimeImageMode(used, words, modes);
        Require(match.mode.has_value() || (match.reason == ShaderRecompiler::ImageTableAbi::PoisonReason::UnsupportedOperation && match.detail.find("past its extent's full mip chain") != std::string::npos), "a T# past its chain was refused for another reason: " + match.detail);
        return match.mode.has_value();
    };
    Require(accepts(ShaderRecompiler::ImageLevelUseQuery, ShaderRecompiler::NoTable, cases[0].words), "image_get_resinfo of a T# whose levels reach past its chain must be served");
    Require(accepts(ShaderRecompiler::ImageLevelUseLoad, ShaderRecompiler::NoTable, cases[0].words), "image_load at base level 0 of a T# whose levels reach past its chain reads a level its view holds and must be served");
    Require(!accepts(ShaderRecompiler::ImageLevelUseLoad, ShaderRecompiler::NoTable, cases[1].words), "image_load at base level 7 of a 28x1 T# reads a level its view does not hold and must be refused");
    Require(!accepts(ShaderRecompiler::ImageLevelUseMipLoad, ShaderRecompiler::NoTable, cases[0].words), "image_load_mip of a T# whose levels reach past its chain must be refused");
    Require(!accepts(ShaderRecompiler::ImageLevelUseLodQuery, ShaderRecompiler::NoTable, cases[0].words), "image_get_lod of a T# whose levels reach past its chain must be refused");
    Require(!accepts(ShaderRecompiler::ImageLevelUseQuery, 0u, cases[0].words), "image_get_resinfo through an image table of a T# whose levels reach past its chain must be refused");
    Require(accepts(ShaderRecompiler::ImageLevelUseMipLoad | ShaderRecompiler::ImageLevelUseLodQuery | ShaderRecompiler::ImageLevelUseLoad, ShaderRecompiler::NoTable, imageWords(13, 64, 64, 4, 0, 6, 6)), "a full 64x64 chain must be served to every access");
    const auto single = imageWords(13, 28, 1, 4, 7, 7, 9);
    Require(accepts(ShaderRecompiler::ImageLevelUseQuery, ShaderRecompiler::NoTable, single), "a single-level query past the chain must retain its one level");
    Require(accepts(ShaderRecompiler::ImageLevelUseQuery, 0u, single), "a single-level image table query past the chain needs no missing-level metadata");
    for (const auto use : {ShaderRecompiler::ImageLevelUseLoad, ShaderRecompiler::ImageLevelUseMipLoad, ShaderRecompiler::ImageLevelUseLodQuery}) Require(!accepts(use, ShaderRecompiler::NoTable, single), "a load or LOD query of a single unrepresented base level must be refused");
}

}

void RunTextureMipChainTests() {
    Require(Recorder::Active() == nullptr, "the texture mip chain tests record into their own command batches");
    mock = MockDevice{};
    const auto context = mockContext();
    {
        TextureDetiler detiler(context);
        TestLevelsPastTheFullChain(context, detiler);
        TestFullChainsAreUnchanged(context, detiler);
        TestClearedMipRanges(context, detiler);
        TestSrgbExpansionLevels(context, detiler);
        TestRepackCopyLevels(context, detiler);
        TestExtraMipAllocationRecovery();
        TestStorageLevelsPastTheFullChain(context, detiler);
        TestStorageFullChainIsUnchanged(context, detiler);
        TestShaderLevelsPastTheView(context, detiler);
        TestSrgbLevelsPastTheFullChain(context);
    }
    mock = MockDevice{};
}
