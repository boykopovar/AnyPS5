#include "GraphicsTests.hpp"
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
#include <algorithm>
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

// A guest texture descriptor may name more mip levels than its extent halves into: Demon's Souls
// (PPSA01342) samples a 28x1 BC3 array through a T# of 10 levels. A Vulkan image holds at most
// floor(log2(max extent)) + 1 = 5 of them (VUID-VkImageCreateInfo-mipLevels-00947); NVIDIA sizes a
// 28x1 BC3 image of 10 levels exactly like one of 5, so copying the guest's levels 5-9 into it
// wrote past its memory and lost the device (write fault at the end of the image). These tests run
// the sampled texture upload and a storage image's upload, write-back, views and fills against a
// recording mock device and check that the image and every command on it stay inside the levels
// the image can have, and that an access that would write a level past them throws.
namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t FormatBc3Srgb = 174;
constexpr std::uint32_t FormatR8G8B8A8Unorm = 56;

struct MockDevice {
    std::uintptr_t next = 0x10000;
    std::map<VkImage, VkImageCreateInfo> images;
    std::vector<std::pair<VkImage, VkImageSubresourceLayers>> copies;
    std::vector<std::pair<VkImage, VkImageSubresourceRange>> barriers;
    std::vector<std::pair<VkImage, VkImageSubresourceRange>> views;
    std::vector<std::pair<VkImage, VkImageSubresourceLayers>> readbacks;
    std::vector<std::pair<VkImage, VkImageSubresourceRange>> clears;
    std::map<VkBuffer, VkDeviceSize> bufferSizes;
    std::map<VkDeviceMemory, std::vector<std::byte>> memories;
};

MockDevice mock;

// floor(log2(max extent)) + 1: the most levels a Vulkan image of that extent may have.
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

VKAPI_ATTR VkResult VKAPI_CALL bindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize) {
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
    *memory = newHandle<VkDeviceMemory>();
    mock.memories[*memory].resize(static_cast<std::size_t>(info->allocationSize));
    return VK_SUCCESS;
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
    for (std::uint32_t index = 0; index < count; ++index) mock.copies.emplace_back(image, regions[index].imageSubresource);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyImageToBuffer(VkCommandBuffer, VkImage image, VkImageLayout, VkBuffer, std::uint32_t count, const VkBufferImageCopy* regions) {
    for (std::uint32_t index = 0; index < count; ++index) mock.readbacks.emplace_back(image, regions[index].imageSubresource);
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
    mock.copies.clear();
    mock.barriers.clear();
    mock.views.clear();
    mock.readbacks.clear();
    mock.clears.clear();
}

// Checks that every copy into or out of `image`, and every barrier, clear and view of it, stays
// inside the levels and layers it was created with, and that it has no more levels than its extent.
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

// Uploads `descriptor` through the sampled texture constructor and returns its image's create info,
// after checking that every copy, barrier and view of that image stays inside its levels and layers.
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
    // The descriptor of the fault: 28x1 BC3 sRGB, 10 levels (4 layers here, 256 in the title).
    constexpr std::uint32_t layers = 4;
    const auto descriptor = arrayTexture(0x1003000000, FormatBc3Srgb, 28, 1, layers, 10);
    const auto info = upload(context, detiler, descriptor, "28x1 BC3 with 10 levels");
    Require(info.mipLevels == 5, "a 28x1 BC3 texture of 10 guest levels must keep the 5 levels its extent has");
    for (std::uint32_t level = 0; level < 5; ++level) Require(copiesOfLevel(level) == layers, "level " + std::to_string(level) + " of every layer must be uploaded");

    // A view of guest levels past the image's (base 7..9) reads the image's last level.
    auto pastChain = descriptor;
    pastChain.baseLevel = 7;
    upload(context, detiler, pastChain, "a view of levels 7-9 of a 28x1 surface");
    Require(mock.views.front().second.baseMipLevel == 4 && mock.views.front().second.levelCount == 1, "a view of levels past the image's last one must read its last level");

    // 4x1 BC3 with 11 levels (ds-c0078/0080/0081): 3 levels.
    const auto narrow = upload(context, detiler, arrayTexture(0x1021400000, FormatBc3Srgb, 4, 1, 2, 11), "4x1 BC3 with 11 levels");
    Require(narrow.mipLevels == 3, "a 4x1 texture of 11 guest levels must keep 3 levels");
}

void TestFullChainsAreUnchanged(const Context& context, TextureDetiler& detiler) {
    const auto full = upload(context, detiler, arrayTexture(0x1068400000, FormatBc3Srgb, 64, 64, 2, 7), "64x64 BC3 with 7 levels");
    Require(full.mipLevels == 7, "a full 64x64 chain of 7 levels must keep all of them");
    for (std::uint32_t level = 0; level < 7; ++level) Require(copiesOfLevel(level) == 2, "every level of a full chain must be uploaded");
    const auto partial = upload(context, detiler, arrayTexture(0x1068500000, FormatR8G8B8A8Unorm, 64, 32, 1, 3), "64x32 RGBA8 with 3 levels");
    Require(partial.mipLevels == 3, "a partial chain must keep its own level count");
}

// Committed, registered host memory standing for a guest surface (storage images read and store
// guest memory, and drop results over memory no allocation registers).
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

// The single image the storage texture made: its handle and create info.
std::pair<VkImage, VkImageCreateInfo> storageImage(const std::string& what) {
    Require(mock.images.size() == 1, what + ": the storage texture did not create exactly one image");
    return *mock.images.begin();
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
        TestStorageLevelsPastTheFullChain(context, detiler);
        TestStorageFullChainIsUnchanged(context, detiler);
        TestShaderLevelsPastTheView(context, detiler);
    }
    mock = MockDevice{};
}
