#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "ImageTableAbi.hpp"
#include "Recompiler.hpp"
#include "RuntimeAbi.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
namespace Abi = ShaderRecompiler::ImageTableAbi;
using Shape = ShaderRecompiler::DescriptorImageShape;

constexpr std::uint32_t FormatR8G8B8A8Unorm = 56;
constexpr std::uint32_t FormatR16G16B16A16Float = 71;
constexpr std::uint32_t SwModeLinear = 0;
constexpr std::uint32_t SwMode64KBS = 9;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t MaxArrayLayers = 2048;
constexpr std::uint32_t Elements = 6;
constexpr VkDeviceSize KiB = 1024;
constexpr VkDeviceSize MiB = 1024 * KiB;

struct MockDevice {
    std::uintptr_t next = 0x10000;
    std::map<VkImage, VkImageCreateInfo> images;
    std::map<VkBuffer, VkDeviceSize> bufferSizes;
    std::map<VkBuffer, std::pair<VkDeviceMemory, VkDeviceSize>> bufferMemory;
    std::map<VkDeviceMemory, std::vector<std::byte>> memories;
    std::map<std::uint32_t, VkDescriptorBufferInfo> bufferWrites;
    std::vector<std::string> missing;
    VkResult imageResult = VK_SUCCESS;
    VkResult imageFormatResult = VK_SUCCESS;
    std::uint32_t imageFormatQueries = 0;
    VkExtent3D maxExtent{32768, 32768, 1};
    VkDeviceSize maxResourceSize = VkDeviceSize{1} << 40u;
    std::vector<VkDeviceSize> bufferCreations;
    std::array<VkDeviceSize, VK_MAX_MEMORY_HEAPS> heapBudget{};
    std::array<VkDeviceSize, VK_MAX_MEMORY_HEAPS> heapUsage{};
};

MockDevice mock;

template<typename THandle>
THandle newHandle() {
    return reinterpret_cast<THandle>(mock.next += 0x10);
}

VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice, const VkImageCreateInfo* info, const VkAllocationCallbacks*, VkImage* image) {
    if (mock.imageResult != VK_SUCCESS) return mock.imageResult;
    *image = newHandle<VkImage>();
    mock.images[*image] = *info;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL imageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements* requirements) {
    *requirements = {65536, 1024, 0x3};
}

VKAPI_ATTR VkResult VKAPI_CALL bindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice, VkImage, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL createImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView* view) {
    *view = newHandle<VkImageView>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice, const VkBufferCreateInfo* info, const VkAllocationCallbacks*, VkBuffer* buffer) {
    *buffer = newHandle<VkBuffer>();
    mock.bufferSizes[*buffer] = info->size;
    mock.bufferCreations.push_back(info->size);
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

VKAPI_ATTR VkResult VKAPI_CALL bindBufferMemory(VkDevice, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) {
    mock.bufferMemory[buffer] = {memory, offset};
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize, VkMemoryMapFlags, void** data) {
    *data = mock.memories.at(memory).data() + offset;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL unmapMemory(VkDevice, VkDeviceMemory) {}

VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*) {
    mock.bufferSizes.erase(buffer);
    mock.bufferMemory.erase(buffer);
}

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

VKAPI_ATTR void VKAPI_CALL updateDescriptorSets(VkDevice, std::uint32_t count, const VkWriteDescriptorSet* writes, std::uint32_t, const VkCopyDescriptorSet*) {
    for (std::uint32_t index = 0; index < count; ++index) {
        if (writes[index].pBufferInfo != nullptr && writes[index].descriptorCount != 0) mock.bufferWrites[writes[index].dstBinding] = writes[index].pBufferInfo[0];
    }
}

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
VKAPI_ATTR void VKAPI_CALL cmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL cmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, std::uint32_t, const VkBufferCopy*) {}
VKAPI_ATTR void VKAPI_CALL cmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, std::uint32_t, const VkBufferImageCopy*) {}
VKAPI_ATTR void VKAPI_CALL cmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL cmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, std::uint32_t, std::uint32_t, const VkDescriptorSet*, std::uint32_t, const std::uint32_t*) {}
VKAPI_ATTR void VKAPI_CALL cmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) {}
VKAPI_ATTR void VKAPI_CALL cmdDispatch(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t) {}

VKAPI_ATTR void VKAPI_CALL formatProperties(VkPhysicalDevice, VkFormat, VkFormatProperties* properties) {
    *properties = {};
    properties->optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
}

VKAPI_ATTR VkResult VKAPI_CALL imageFormatProperties(VkPhysicalDevice, VkFormat, VkImageType, VkImageTiling, VkImageUsageFlags, VkImageCreateFlags, VkImageFormatProperties* properties) {
    ++mock.imageFormatQueries;
    *properties = {};
    if (mock.imageFormatResult != VK_SUCCESS) return mock.imageFormatResult;
    *properties = {mock.maxExtent, 16, MaxArrayLayers, VK_SAMPLE_COUNT_1_BIT, mock.maxResourceSize};
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL memoryProperties2(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
    for (auto* next = static_cast<VkBaseOutStructure*>(properties->pNext); next != nullptr; next = next->pNext) {
        if (next->sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT) continue;
        auto& reported = *reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(next);
        std::copy(mock.heapBudget.begin(), mock.heapBudget.end(), reported.heapBudget);
        std::copy(mock.heapUsage.begin(), mock.heapUsage.end(), reported.heapUsage);
    }
}

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
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(cmdBindPipeline)},
        {"vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(cmdBindDescriptorSets)},
        {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(cmdPushConstants)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(cmdDispatch)},
    };
    const auto it = table.find(name);
    if (it != table.end()) return it->second;
    mock.missing.emplace_back(name);
    return nullptr;
}

Context mockContext() {
    static int deviceTag = 0;
    Context context{};
    context.device = reinterpret_cast<VkDevice>(&deviceTag);
    context.deviceProc = deviceProc;
    context.formatProperties = formatProperties;
    context.imageFormatProperties = imageFormatProperties;
    context.memory.memoryTypeCount = 2;
    context.memory.memoryTypes[0] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0};
    context.memory.memoryTypes[1] = {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 1};
    context.memory.memoryHeapCount = 2;
    context.memory.memoryHeaps[0] = {std::uint64_t{1} << 32u, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    context.memory.memoryHeaps[1] = {std::uint64_t{1} << 32u, 0};
    context.nullDescriptors = true;
    context.imageTableCapacity = Elements;
    context.limits.maxMemoryAllocationCount = 4096;
    context.limits.nonCoherentAtomSize = 64;
    context.limits.minStorageBufferOffsetAlignment = 16;
    context.limits.maxStorageBufferRange = 1u << 30u;
    context.limits.maxBoundDescriptorSets = 1;
    context.limits.maxPerStageDescriptorStorageBuffers = 16;
    context.limits.maxDescriptorSetStorageBuffers = 32;
    context.limits.maxPerStageResources = 128;
    context.limits.maxPerStageDescriptorSampledImages = Elements;
    context.limits.maxDescriptorSetSampledImages = Elements;
    context.limits.maxImageDimension1D = 32768;
    context.limits.maxImageDimension2D = 32768;
    context.limits.maxImageDimension3D = 256;
    context.limits.maxImageDimensionCube = 32768;
    context.limits.maxImageArrayLayers = MaxArrayLayers;
    return context;
}

std::array<std::uint32_t, 8> textureWords(std::uint64_t address, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t swizzleMode, std::uint32_t type, std::uint32_t lastSlice) {
    std::array<std::uint32_t, 8> words{};
    const auto base = address >> 8u;
    words[0] = static_cast<std::uint32_t>(base);
    words[1] = static_cast<std::uint32_t>(base >> 32u) | (format << 20u) | (((width - 1u) & 0x3u) << 30u);
    words[2] = ((width - 1u) >> 2u) | ((height - 1u) << 14u) | (1u << 31u);
    words[3] = 4u | (5u << 3u) | (6u << 6u) | (7u << 9u) | (swizzleMode << 20u) | (type << 28u);
    words[4] = lastSlice;
    return words;
}

std::uint64_t addressOf(const void* pointer) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

struct Entry {
    std::array<std::uint32_t, 8> words;
    Shape shape;
};

struct CachedTexturesCleared {
    VkDevice device;
    explicit CachedTexturesCleared(VkDevice device) : device(device) {}
    CachedTexturesCleared(const CachedTexturesCleared&) = delete;
    CachedTexturesCleared& operator=(const CachedTexturesCleared&) = delete;
    ~CachedTexturesCleared() { ClearCachedTextures(device); }
};

struct Block {
    std::byte* data;
    std::size_t bytes;
    explicit Block(std::size_t bytes) : data(static_cast<std::byte*>(::operator new(bytes, std::align_val_t{4096}))), bytes(bytes) { std::memset(data, 0, bytes); }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    ~Block() { ::operator delete(data, std::align_val_t{4096}); }
};

struct Registration {
    void* data;
    Registration(void* data, std::size_t bytes) : data(data) { GuestAllocations::Mutation().Add(data, bytes, true, true, false); }
    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    ~Registration() { GuestAllocations::Mutation().Remove(data); }
};

constexpr std::size_t GuestBytes = std::size_t{MaxArrayLayers} * 2048u + 65536u;

std::vector<std::uint32_t> resolveTable(const Context& context, std::span<const Entry> entries) {
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding map{};
    map.descriptorSet = ShaderRecompiler::RuntimeAbi::DescriptorSet;
    map.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    map.role = ShaderRecompiler::DescriptorRole::ImageTableMap;
    map.binding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap);
    map.count = 1;
    map.readOnly = false;
    map.guestDescriptor.assign(Abi::HeaderWords + entries.size(), 0u);
    map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
    map.guestDescriptor[Abi::HeaderElementCount] = static_cast<std::uint32_t>(entries.size());
    map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
    map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords + static_cast<std::uint32_t>(entries.size());
    program.bindings.push_back(map);
    ShaderRecompiler::DescriptorBinding images{};
    images.descriptorSet = ShaderRecompiler::RuntimeAbi::DescriptorSet;
    images.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    images.role = ShaderRecompiler::DescriptorRole::ImageTable;
    images.binding = ShaderRecompiler::RuntimeAbi::ImageTableBinding;
    images.count = static_cast<std::uint32_t>(entries.size());
    for (const auto& entry : entries) {
        images.guestDescriptor.insert(images.guestDescriptor.end(), entry.words.begin(), entry.words.end());
        images.imageShapes.push_back(entry.shape);
    }
    images.imageDepthCompare.assign(entries.size(), false);
    images.imageSamplers.assign(entries.size(), 0u);
    program.bindings.push_back(std::move(images));
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    mock.bufferWrites.clear();
    ShaderResources resources(context, shader, {}, true);
    resources.Complete();
    const auto write = mock.bufferWrites.find(map.binding);
    Require(write != mock.bufferWrites.end(), "the image table map was not written to its descriptor");
    const auto& [memory, offset] = mock.bufferMemory.at(write->second.buffer);
    const auto& bytes = mock.memories.at(memory);
    std::vector<std::uint32_t> words(Abi::HeaderWords + entries.size());
    Require(offset + write->second.offset + words.size() * sizeof(std::uint32_t) <= bytes.size(), "the image table map buffer is shorter than its map");
    std::memcpy(words.data(), bytes.data() + offset + write->second.offset, words.size() * sizeof(std::uint32_t));
    return words;
}

std::string createdImages() {
    std::string created;
    for (const auto& [image, info] : mock.images) created += " " + std::to_string(info.extent.width) + "x" + std::to_string(info.extent.height) + "x" + std::to_string(info.extent.depth) + "/" + std::to_string(info.arrayLayers) + " layers";
    return created.empty() ? " none" : created;
}

void requireServed(const std::vector<std::uint32_t>& map, std::uint32_t element, const std::string& what) {
    Require(map[Abi::HeaderWords + element] == 0u, what + ": the entry was rejected (fault code " + std::to_string(map[Abi::HeaderWords + element]) + "); images created:" + createdImages() + "; Vulkan functions the mock lacks: " + std::to_string(mock.missing.size()));
}

void requireRejected(const std::vector<std::uint32_t>& map, std::uint32_t element, const std::string& what) {
    Require((map[Abi::HeaderWords + element] & Abi::PoisonFlag) != 0u, what + ": the entry was served instead of faulting when selected; images created:" + createdImages());
}

std::array<std::uint32_t, 8> layeredWords(const Block& guest, std::uint32_t layers) {
    return textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 256, 256, SwModeLinear, Type2DArray, layers - 1u);
}

std::size_t imagesWithLayers(std::uint32_t layers) {
    return static_cast<std::size_t>(std::count_if(mock.images.begin(), mock.images.end(), [&](const auto& image) { return image.second.extent.width == 256 && image.second.arrayLayers == layers; }));
}

VkDeviceSize largestBufferCreated() {
    return mock.bufferCreations.empty() ? 0 : *std::max_element(mock.bufferCreations.begin(), mock.bufferCreations.end());
}

void testTableEntries(Context context) {
    static int physicalTag = 0;
    context.physical = reinterpret_cast<VkPhysicalDevice>(&physicalTag);
    const Block guest(GuestBytes), host(4096);
    const Registration registered(guest.data, guest.bytes);
    const CachedTexturesCleared cleared(context.device);
    const auto k1 = textureWords(0x200, FormatR16G16B16A16Float, 256, 256, SwMode64KBS, Type2DArray, 7424);
    const auto decoded = DecodeTextureResource(k1);
    Require(decoded.baseAddress == 0x200 && decoded.width == 256 && decoded.height == 256 && decoded.format == FormatR16G16B16A16Float, "the 256x256 T# at 0x200 decoded to another surface");
    Require(decoded.dimension == TextureDimension::k2DArray && decoded.tileMode == TextureTileMode::kStandard64KB && decoded.depthOrLastArray == 7424 && decoded.mipCount == 1, "the 256x256 T# at 0x200 decoded to other array fields");
    Require(DescribeSurface(decoded).layers == 7425, "the 256x256 T# at 0x200 must describe 7425 layers");
    const std::array<Entry, Elements> entries{{
        {textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 8, 8, SwModeLinear, Type2D, 0), Shape::Image2D},
        {k1, Shape::Image2DArray},
        {textureWords(addressOf(host.data), FormatR8G8B8A8Unorm, 8, 8, SwModeLinear, Type2D, 0), Shape::Image2D},
        {textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 8, 8, SwModeLinear, Type2DArray, MaxArrayLayers), Shape::Image2DArray},
        {textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 8, 8, SwModeLinear, Type2DArray, MaxArrayLayers - 1u), Shape::Image2DArray},
        {textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 16384, 1, SwModeLinear, Type2D, 0), Shape::Image2D},
    }};
    mock.images.clear();
    mock.maxExtent = {8192, 8192, 1};
    const auto map = resolveTable(context, entries);
    mock.maxExtent = {32768, 32768, 1};
    requireServed(map, 0, "an 8x8 texture in registered guest memory");
    requireRejected(map, 1, "the 256x256 2D array of 7425 layers at 0x200");
    requireRejected(map, 2, "an 8x8 texture in host memory no guest allocation holds");
    requireRejected(map, 3, "an 8x8 2D array of 2049 layers in registered guest memory");
    requireServed(map, 4, "an 8x8 2D array of 2048 layers in registered guest memory");
    requireRejected(map, 5, "a 16384x1 texture over a format limit of 8192");
    Require(map[Abi::HeaderPoisonCount] == 4u, "the table holds " + std::to_string(map[Abi::HeaderPoisonCount]) + " rejected entries, expected 4");
    Require(mock.images.size() == 2, "the rejected entries created images:" + createdImages());
    for (const auto& [image, info] : mock.images) {
        Require(info.extent.width == 8 && info.extent.height == 8 && info.extent.depth == 1 && (info.arrayLayers == 1 || info.arrayLayers == MaxArrayLayers), "an image other than the two served entries was created:" + createdImages());
    }
}

void testResourceSize(Context context) {
    static int physicalTag = 0;
    context.physical = reinterpret_cast<VkPhysicalDevice>(&physicalTag);
    const Block guest(65536);
    const Registration registered(guest.data, guest.bytes);
    const CachedTexturesCleared cleared(context.device);
    const std::array<Entry, 2> entries{{
        {textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 32, 32, SwModeLinear, Type2D, 0), Shape::Image2D},
        {textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 64, 64, SwModeLinear, Type2D, 0), Shape::Image2D},
    }};
    mock.images.clear();
    mock.maxResourceSize = 8192;
    const auto map = resolveTable(context, entries);
    mock.maxResourceSize = VkDeviceSize{1} << 40u;
    requireServed(map, 0, "a 32x32 RGBA8 texture of 4096 bytes under a maxResourceSize of 8192");
    requireRejected(map, 1, "a 64x64 RGBA8 texture of 16384 bytes over a maxResourceSize of 8192");
    Require(mock.images.size() == 1, "the texture over maxResourceSize created an image:" + createdImages());
}

void testUnsupportedImageCombination(Context context) {
    static int physicalTag = 0;
    context.physical = reinterpret_cast<VkPhysicalDevice>(&physicalTag);
    const Block guest(4096);
    const Registration registered(guest.data, guest.bytes);
    const CachedTexturesCleared cleared(context.device);
    const std::array<Entry, 1> entries{{
        {textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 8, 8, SwModeLinear, Type2D, 0), Shape::Image2D},
    }};
    mock.images.clear();
    mock.imageFormatResult = VK_ERROR_FORMAT_NOT_SUPPORTED;
    const auto queriesBefore = mock.imageFormatQueries;
    const auto map = resolveTable(context, entries);
    mock.imageFormatResult = VK_SUCCESS;
    Require(mock.imageFormatQueries > queriesBefore, "the unsupported image combination did not reach the format query");
    requireRejected(map, 0, "an 8x8 registered texture whose exact image combination is unsupported");
    Require(mock.images.empty(), "the unsupported image combination reached vkCreateImage:" + createdImages());
}

void testPartlyRegisteredSurface(const Context& context) {
    const Block straddling(8192);
    const Registration registered(straddling.data + 4096, 4096);
    const CachedTexturesCleared cleared(context.device);
    const std::array<Entry, 1> entries{{
        {textureWords(addressOf(straddling.data), FormatR8G8B8A8Unorm, 64, 32, SwModeLinear, Type2D, 0), Shape::Image2D},
    }};
    mock.images.clear();
    const auto map = resolveTable(context, entries);
    requireServed(map, 0, "a texture whose second page lies in a registered guest allocation");
}

void testVulkanErrorPropagates(const Context& context) {
    const Block guest(4096);
    const Registration registered(guest.data, guest.bytes);
    const CachedTexturesCleared cleared(context.device);
    const std::array<Entry, 1> entries{{
        {textureWords(addressOf(guest.data), FormatR8G8B8A8Unorm, 8, 8, SwModeLinear, Type2D, 0), Shape::Image2D},
    }};
    mock.images.clear();
    mock.imageResult = VK_ERROR_DEVICE_LOST;
    std::optional<VkResult> thrown;
    std::string message;
    try {
        static_cast<void>(resolveTable(context, entries));
    } catch (const VulkanError& error) {
        thrown = error.result;
        message = error.what();
    }
    mock.imageResult = VK_SUCCESS;
    Require(thrown == VK_ERROR_DEVICE_LOST, "VK_ERROR_DEVICE_LOST from an image table entry's vkCreateImage was turned into a faulting entry instead of reaching the caller");
    Require(message.find("vkCreateImage: Vulkan result -4") != std::string::npos, "the propagated device loss names another call: " + message);
}

void testHeapSize(Context context) {
    static int physicalTag = 0;
    context.physical = reinterpret_cast<VkPhysicalDevice>(&physicalTag);
    context.memoryProperties2 = memoryProperties2;
    const Block guest(MiB);
    const Registration registered(guest.data, guest.bytes);
    const CachedTexturesCleared cleared(context.device);
    const std::array<Entry, 2> entries{{
        {layeredWords(guest, 2), Shape::Image2DArray},
        {layeredWords(guest, 3), Shape::Image2DArray},
    }};
    mock.images.clear();
    mock.heapBudget = {64 * MiB, 64 * MiB};
    mock.heapUsage = {64 * MiB - 512 * KiB, 64 * MiB - 256 * KiB};
    auto map = resolveTable(context, entries);
    mock.heapBudget = {};
    mock.heapUsage = {};
    requireServed(map, 1, "a 256x256 RGBA8 array of 3 layers (786432 bytes) in heaps of 4 GiB whose VK_EXT_memory_budget budgets leave 524288 and 262144 bytes");
    Require(imagesWithLayers(3) == 1, "the 3-layer entry within the heaps' sizes created no image:" + createdImages());
    context.memory.memoryHeaps[0].size = 512 * KiB;
    context.memory.memoryHeaps[1].size = 256 * KiB;
    ClearCachedTextures(context.device);
    mock.images.clear();
    mock.bufferCreations.clear();
    map = resolveTable(context, entries);
    requireServed(map, 0, "a 256x256 RGBA8 array of 2 layers (524288 bytes) in heaps of 524288 and 262144 bytes");
    requireRejected(map, 1, "a 256x256 RGBA8 array of 3 layers (786432 bytes) in heaps of 524288 and 262144 bytes");
    Require(imagesWithLayers(3) == 0, "the 3-layer entry larger than every heap created an image:" + createdImages());
    Require(largestBufferCreated() < 3 * 256 * KiB, "the 3-layer entry larger than every heap reached the texture upload: a buffer of " + std::to_string(largestBufferCreated()) + " bytes was created");
}

}

void RunImageTableEntryValidationTests() {
    Require(Recorder::Active() == nullptr, "the image table entry validation tests record into their own command batches");
    mock = MockDevice{};
    {
        auto context = mockContext();
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        const CachedTexturesCleared cleared(context.device);
        testTableEntries(context);
        testResourceSize(context);
        testUnsupportedImageCombination(context);
        testPartlyRegisteredSurface(context);
        testVulkanErrorPropagates(context);
        testHeapSize(context);
        Require(mock.missing.empty(), "the mock lacks the Vulkan function " + (mock.missing.empty() ? std::string{} : mock.missing.front()));
    }
    mock = MockDevice{};
}
