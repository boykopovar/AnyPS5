#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t FormatR8Unorm = 1;
constexpr std::uint32_t FormatR8Uint = 5;
constexpr std::uint32_t FormatR16Unorm = 7;
constexpr std::uint32_t FormatR16Uint = 11;
constexpr std::uint32_t FormatR32Uint = 20;
constexpr std::uint32_t FormatR32Float = 22;
constexpr std::uint32_t FormatR11G11B10Float = 36;
constexpr std::uint32_t FormatR8G8B8A8Unorm = 56;
constexpr std::uint32_t TileStandard64KB = 0x9;
constexpr std::uint32_t TileDepth64KB = 0x18;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;

std::uintptr_t nextHandle = 0x1000;

template<typename THandle>
THandle newHandle() {
    return reinterpret_cast<THandle>(nextHandle += 0x10);
}

VkFormat lastImageFormat = VK_FORMAT_UNDEFINED;
std::vector<VkImage> createdImages;
std::vector<std::tuple<VkImage, VkImage, VkImageAspectFlags>> imageCopies;
VkImage lastImage = VK_NULL_HANDLE;
std::vector<VkImage> clearedImages;
VkImageUsageFlags lastImageUsage = 0;
std::uint32_t imagesDestroyed = 0;
std::vector<std::pair<VkImage, VkImageAspectFlags>> imageReads;
std::vector<std::pair<VkImage, VkImageAspectFlags>> imageWrites;
std::vector<VkImage> destroyedImages;

VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice, const VkImageCreateInfo* info, const VkAllocationCallbacks*, VkImage* image) {
    lastImageFormat = info->format;
    lastImageUsage = info->usage;
    *image = newHandle<VkImage>();
    createdImages.push_back(*image);
    lastImage = *image;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL imageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements* requirements) {
    *requirements = {65536, 65536, 1};
}

VKAPI_ATTR VkResult VKAPI_CALL allocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    *memory = newHandle<VkDeviceMemory>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL bindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL createImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView* view) {
    *view = newHandle<VkImageView>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL allocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* commands) {
    *commands = newHandle<VkCommandBuffer>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    *fence = newHandle<VkFence>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL beginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL endCommandBuffer(VkCommandBuffer) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL queueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL waitForFences(VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL getFenceStatus(VkDevice, VkFence) {
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL freeCommandBuffers(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*) {}
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice, VkFence, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL destroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice, VkImage image, const VkAllocationCallbacks*) {
    ++imagesDestroyed;
    destroyedImages.push_back(image);
}
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL cmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL cmdClearDepthStencilImage(VkCommandBuffer, VkImage image, VkImageLayout, const VkClearDepthStencilValue*, std::uint32_t, const VkImageSubresourceRange*) {
    clearedImages.push_back(image);
}
VKAPI_ATTR void VKAPI_CALL cmdCopyImage(VkCommandBuffer, VkImage source, VkImageLayout, VkImage destination, VkImageLayout, std::uint32_t count, const VkImageCopy* regions) {
    for (std::uint32_t i = 0; i < count; ++i) imageCopies.emplace_back(source, destination, regions[i].dstSubresource.aspectMask);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyImageToBuffer(VkCommandBuffer, VkImage source, VkImageLayout, VkBuffer, std::uint32_t count, const VkBufferImageCopy* regions) {
    for (std::uint32_t i = 0; i < count; ++i) imageReads.emplace_back(source, regions[i].imageSubresource.aspectMask);
}

VKAPI_ATTR void VKAPI_CALL cmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage destination, VkImageLayout, std::uint32_t count, const VkBufferImageCopy* regions) {
    for (std::uint32_t i = 0; i < count; ++i) imageWrites.emplace_back(destination, regions[i].imageSubresource.aspectMask);
}

VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer* buffer) {
    *buffer = newHandle<VkBuffer>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL bufferMemoryRequirements(VkDevice, VkBuffer, VkMemoryRequirements* requirements) {
    *requirements = {65536, 256, 1};
}

VKAPI_ATTR VkResult VKAPI_CALL bindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize) {
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**) {
    return VK_ERROR_MEMORY_MAP_FAILED;
}
VKAPI_ATTR void VKAPI_CALL unmapMemory(VkDevice, VkDeviceMemory) {}
VKAPI_ATTR VkResult VKAPI_CALL createDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout* layout) {
    *layout = newHandle<VkDescriptorSetLayout>();
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL createPipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout* layout) {
    *layout = newHandle<VkPipelineLayout>();
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL createShaderModule(VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule* module) {
    *module = newHandle<VkShaderModule>();
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL destroyPipelineLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL destroyShaderModule(VkDevice, VkShaderModule, const VkAllocationCallbacks*) {}

VKAPI_ATTR void VKAPI_CALL formatProperties(VkPhysicalDevice, VkFormat, VkFormatProperties* properties) {
    *properties = {};
    properties->optimalTilingFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
}

VKAPI_ATTR VkResult VKAPI_CALL sampledDepthProperties(VkPhysicalDevice, VkFormat, VkImageType, VkImageTiling, VkImageUsageFlags, VkImageCreateFlags, VkImageFormatProperties* properties) {
    *properties = {};
    properties->maxExtent = {16384, 16384, 1};
    properties->maxArrayLayers = 1;
    properties->maxMipLevels = 1;
    properties->sampleCounts = VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_8_BIT;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL attachmentOnlyProperties(VkPhysicalDevice, VkFormat, VkImageType, VkImageTiling, VkImageUsageFlags, VkImageCreateFlags, VkImageFormatProperties*) {
    return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

VKAPI_ATTR void VKAPI_CALL formatPropertiesWithoutD16S8(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    *properties = {};
    if (format != VK_FORMAT_D16_UNORM_S8_UINT) properties->optimalTilingFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
}

VKAPI_ATTR void VKAPI_CALL formatPropertiesOnlyD32S8(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    *properties = {};
    if (format == VK_FORMAT_D32_SFLOAT_S8_UINT) properties->optimalTilingFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
}

PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(createImage)},
        {"vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(imageMemoryRequirements)},
        {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(allocateMemory)},
        {"vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(bindImageMemory)},
        {"vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(createImageView)},
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(allocateCommandBuffers)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(createFence)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(beginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(endCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(queueSubmit)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(waitForFences)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(getFenceStatus)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(freeCommandBuffers)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(destroyFence)},
        {"vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(destroyImageView)},
        {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(destroyImage)},
        {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(freeMemory)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(cmdPipelineBarrier)},
        {"vkCmdClearDepthStencilImage", reinterpret_cast<PFN_vkVoidFunction>(cmdClearDepthStencilImage)},
        {"vkCmdCopyImage", reinterpret_cast<PFN_vkVoidFunction>(cmdCopyImage)},
        {"vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(cmdCopyImageToBuffer)},
        {"vkCmdCopyBufferToImage", reinterpret_cast<PFN_vkVoidFunction>(cmdCopyBufferToImage)},
        {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(createBuffer)},
        {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(bufferMemoryRequirements)},
        {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(bindBufferMemory)},
        {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(destroyBuffer)},
        {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(mapMemory)},
        {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(unmapMemory)},
        {"vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(createDescriptorSetLayout)},
        {"vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(createPipelineLayout)},
        {"vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(createShaderModule)},
        {"vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(destroyDescriptorSetLayout)},
        {"vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(destroyPipelineLayout)},
        {"vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(destroyShaderModule)}
    };
    const auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}

struct View {
    std::uint64_t address;
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t tileMode = TileDepth64KB;
    std::uint32_t type = Type2D;
    std::uint32_t lastArray = 0;
    std::uint32_t baseLevel = 0;
    std::uint32_t lastLevel = 0;
    std::uint32_t maxMip = 0;
};

std::array<std::uint32_t, 8> tsharp(const View& view) {
    std::array<std::uint32_t, 8> words{};
    const auto base = view.address >> 8u;
    const auto widthMinus1 = view.width - 1u;
    const auto heightMinus1 = view.height - 1u;
    words[0] = static_cast<std::uint32_t>(base);
    words[1] = static_cast<std::uint32_t>((base >> 32u) & 0xffu) | ((view.format & 0x1ffu) << 20u) | ((widthMinus1 & 0x3u) << 30u);
    words[2] = ((widthMinus1 >> 2u) & 0xfffu) | ((heightMinus1 & 0x3fffu) << 14u);
    words[3] = 4u | (5u << 3u) | (6u << 6u) | (7u << 9u) | ((view.baseLevel & 0xfu) << 12u) | ((view.lastLevel & 0xfu) << 16u) | ((view.tileMode & 0x1fu) << 20u) | ((view.type & 0xfu) << 28u);
    words[4] = view.lastArray & 0x1fffu;
    words[5] = (view.maxMip & 0xfu) << 4u;
    return words;
}

std::shared_ptr<Texture> lookup(const Context& context, const View& view) {
    const auto words = tsharp(view);
    return DepthSurfaceTexture(context, words, DecodeTextureResource(words), VkComponentMapping{});
}

void TestStencilUnormView(const Context& context) {
    constexpr std::uint64_t depth = 0x74000000;
    constexpr std::uint64_t stencil = 0x74800000;
    constexpr std::uint64_t other = 0x75000000;
    const auto before = createdImages.size();
    DepthSurfaceView(context, {depth, stencil, {320, 180}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0});
    Require(createdImages.size() == before + 1, "binding a new depth surface must create one image");
    const auto surface = createdImages[before];
    DepthSurfaceView(context, {other, 0, {64, 64}, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    imageReads.clear();
    imageWrites.clear();
    Require(lookup(context, {stencil, FormatR8Uint, 320, 180}) != nullptr && imageReads.empty() && imageWrites.empty(), "an R8_UINT view of a stencil plane must sample it without a copy");
    const auto first = lookup(context, {stencil, FormatR8Unorm, 320, 180});
    Require(first != nullptr && lastImageFormat == VK_FORMAT_R8_UNORM, "an R8_UNORM view of a stencil plane must sample an R8_UNORM copy of it");
    const auto copy = createdImages.back();
    const std::vector<std::pair<VkImage, VkImageAspectFlags>> read{{surface, VK_IMAGE_ASPECT_STENCIL_BIT}};
    const std::vector<std::pair<VkImage, VkImageAspectFlags>> written{{copy, VK_IMAGE_ASPECT_COLOR_BIT}};
    Require(imageReads == read && imageWrites == written, "the R8_UNORM copy must be filled from the stencil aspect of the depth image");
    Require(lookup(context, {stencil, FormatR8Unorm, 320, 180}) == first && imageReads.size() == 1, "sampling the stencil plane again with no write in between must reuse the copy");
    DepthSurfaceView(context, {depth, stencil, {320, 180}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0});
    lookup(context, {stencil, FormatR8Unorm, 320, 180});
    lookup(context, {stencil, FormatR8Unorm, 320, 180});
    Require(imageReads.size() == 3 && imageWrites.size() == 3, "sampling the stencil plane of the bound depth surface must copy it each time");
    DepthSurfaceView(context, {other, 0, {64, 64}, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    lookup(context, {stencil, FormatR8Unorm, 320, 180});
    lookup(context, {stencil, FormatR8Unorm, 320, 180});
    Require(imageReads.size() == 4 && imageWrites.size() == 4, "after the surface is unbound, its stencil copy must be refreshed once");
    Require(createdImages.back() == copy, "refreshing the stencil copy must reuse its image");
    ClearDepthSurfaces(context.device);
}

void TestD16StencilFallback(const Context& context) {
    Require(HostDepthFormat(context, VK_FORMAT_D16_UNORM_S8_UINT) == VK_FORMAT_D16_UNORM_S8_UINT && HostDepthBiasConstant(context, VK_FORMAT_D16_UNORM_S8_UINT, 2.0f) == 2.0f, "a device with D16_UNORM_S8_UINT must keep it and its depth bias");
    static int withoutTag = 0;
    Context without = context;
    without.physical = reinterpret_cast<VkPhysicalDevice>(&withoutTag);
    without.formatProperties = formatPropertiesWithoutD16S8;
    constexpr std::uint64_t d16s8 = 0x70000000;
    constexpr std::uint64_t d16s8Stencil = 0x70800000;
    Require(HostDepthFormat(without, VK_FORMAT_D16_UNORM_S8_UINT) == VK_FORMAT_D24_UNORM_S8_UINT, "a device without D16_UNORM_S8_UINT must store it as D24_UNORM_S8_UINT");
    Require(HostDepthFormat(without, VK_FORMAT_D32_SFLOAT_S8_UINT) == VK_FORMAT_D32_SFLOAT_S8_UINT && HostDepthFormat(without, VK_FORMAT_D16_UNORM) == VK_FORMAT_D16_UNORM, "only D16_UNORM_S8_UINT is replaced");
    DepthSurfaceView(without, {d16s8, d16s8Stencil, {256, 256}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0});
    Require(lastImageFormat == VK_FORMAT_D24_UNORM_S8_UINT, "a D16_UNORM_S8_UINT surface was not created as D24_UNORM_S8_UINT on a device without it");
    Require(lookup(without, {d16s8, FormatR16Unorm, 256, 256}) != nullptr, "an R16 view of a D16 surface stored as D24 must still sample its depth plane");
    Require(lookup(without, {d16s8Stencil, FormatR8Uint, 256, 256}) != nullptr, "an 8-bit view of the stencil plane of a D16 surface stored as D24 must sample it");
    Require(HostDepthBiasConstant(without, VK_FORMAT_D16_UNORM_S8_UINT, 2.0f) == 512.0f, "a 16-bit depth bias must be scaled by 256 for D24 storage");
    Require(HostDepthBiasConstant(without, VK_FORMAT_D32_SFLOAT_S8_UINT, 2.0f) == 2.0f, "a D32 depth bias must not be scaled");
    const auto refused = [&](const View& view) {
        try {
            lookup(without, view);
        } catch (const std::runtime_error& error) {
            return std::string_view(error.what()).find("a depth plane that the device stores in another depth format is not implemented") != std::string_view::npos;
        }
        return false;
    };
    Require(refused({d16s8, FormatR16Unorm, 256, 256, TileDepth64KB, Type2DArray, 1}), "two slices of a D16 surface stored as D24 must be refused, as the plane copy would read the wider image as 16-bit texels");
    Require(refused({d16s8, FormatR16Uint, 256, 256, TileStandard64KB}), "an R16_UINT view of a D16 surface stored as D24 must be refused");
    std::string storageError;
    try {
        const auto words = tsharp({d16s8, FormatR16Unorm, 256, 256});
        DepthSurfaceStorage(without, words, DecodeTextureResource(words), 0);
    } catch (const std::runtime_error& error) {
        storageError = error.what();
    }
    Require(storageError.find("depth plane") != std::string::npos && storageError.find("stored as") != std::string::npos, "storage access to the depth plane of a D16 surface stored as D24 must throw");
    ClearDepthSurfaces(without.device);

    static int onlyTag = 0;
    Context only = context;
    only.physical = reinterpret_cast<VkPhysicalDevice>(&onlyTag);
    only.formatProperties = formatPropertiesOnlyD32S8;
    Require(HostDepthFormat(only, VK_FORMAT_D16_UNORM_S8_UINT) == VK_FORMAT_D32_SFLOAT_S8_UINT, "a device with neither D16 nor D24 stencil formats must store D16_UNORM_S8_UINT as D32_SFLOAT_S8_UINT");
    Require(HostDepthBiasConstant(only, VK_FORMAT_D16_UNORM_S8_UINT, 0.0f) == 0.0f, "a zero depth bias needs no scaling");
    bool threw = false;
    try {
        HostDepthBiasConstant(only, VK_FORMAT_D16_UNORM_S8_UINT, 2.0f);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "a constant 16-bit depth bias on D32_SFLOAT_S8_UINT storage must throw");
}

void TestMultisampleDepthOwnership(Context context) {
    Require(MultisampledDepthBytes_nid_no_patch({3840, 2160}, 4, 8) == 0xff00000u, "the eight-sample D32 guard must cover its complete footprint");
    for (const auto bytes : {2u, 4u}) {
        for (const auto samples : {2u, 4u, 8u}) {
            Require(MultisampledDepthBytes_nid_no_patch({1, 1}, bytes, samples) == 65536u, "a small native depth guard must occupy one sample-aware block");
        }
    }
    context.imageFormatProperties = attachmentOnlyProperties;
    DepthTarget target{0x71000000, 0, {129, 67}, VK_FORMAT_D32_SFLOAT, 0.0f, 0, 0, false, 4};
    Require(DepthSurfaceView(context, target) != VK_NULL_HANDLE, "lack of sampled MSAA support must retain attachment rendering");
    Require((lastImageUsage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0, "unsupported sampled usage must not be requested for a native depth attachment");
    ClearDepthSurfaces(context.device);
    context.imageFormatProperties = sampledDepthProperties;
    Require(DepthSurfaceView(context, target) != VK_NULL_HANDLE, "a supported sampled MSAA format must retain attachment rendering without import eligibility");
    Require((lastImageUsage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0, "a native MSAA depth attachment must keep sampled usage off when the shadow path is available");
    ClearDepthSurfaces(context.device);
    const auto destroyed = imagesDestroyed;
    auto owner = std::make_shared<OwnedImage>(context, newHandle<VkImage>(), newHandle<VkDeviceMemory>());
    auto texture = std::make_shared<Texture>(context, owner->image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, VkComponentMapping{}, VK_IMAGE_VIEW_TYPE_2D, 0, owner);
    owner.reset();
    Require(imagesDestroyed == destroyed, "a depth image must stay alive while its sampled view is held");
    texture.reset();
    Require(imagesDestroyed == destroyed + 1u, "the last sampled view must release its native depth image exactly once");
}

void TestMultisampleStencilAliases(Context context) {
    context.imageFormatProperties = sampledDepthProperties;
    for (const auto format : {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
        for (const auto samples : {2u, 4u, 8u}) {
            DepthTarget target{0x73000000, 0x73800000, {1920, 1080}, format, 1.0f, 0};
            target.samples = samples;
            Require(DepthSurfaceView(context, target) != VK_NULL_HANDLE, "a multisampled stencil alias fixture must retain attachment rendering");
            for (const auto colorFormat : {FormatR11G11B10Float, FormatR8G8B8A8Unorm, FormatR32Float}) {
                Require(lookup(context, {target.stencilAddress, colorFormat, 1920, 1080, TileRenderTarget64KB}) == nullptr, "a non-stencil color format at a multisampled stencil address must remain reused memory");
            }
            const auto depthFormat = format == VK_FORMAT_D16_UNORM_S8_UINT ? FormatR16Unorm : FormatR32Float;
            for (const auto view : {View{target.stencilAddress, FormatR8Uint, 1920, 1080}, View{target.address, depthFormat, 1920, 1080}}) {
                bool refused = false;
                try {
                    lookup(context, view);
                } catch (const std::runtime_error& error) {
                    refused = std::string_view(error.what()).find("only by recorded draws and dispatches") != std::string_view::npos;
                }
                Require(refused, "a genuine multisample stencil or depth reader must retain its recording requirement");
            }
            ClearDepthSurfaces(context.device);
        }
    }
}

}

void TestRetiredSurfaceRelease(const Context& context) {
    ClearDepthSurfaces(context.device);
    constexpr VkExtent2D extent{256, 256};
    const auto bytes = DepthSliceBytes(extent, 4u);
    const auto destroyed = [](VkImage image) { return std::find(destroyedImages.begin(), destroyedImages.end(), image) != destroyedImages.end(); };
    constexpr std::uint64_t moving = 0x90000000;
    std::vector<VkImage> images;
    for (std::uint64_t frame = 0; frame < 4; ++frame) {
        DepthSurfaceView(context, {moving + frame * 0x100000u, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
        images.push_back(lastImage);
        RetireDepthSurfaces(context.device, moving + frame * 0x100000u, bytes);
    }
    const auto released = DepthSurfacesReleased();
    DepthSurfaceView(context, {moving + 4u * 0x100000u, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    Require(std::all_of(images.begin(), images.end(), destroyed), "a retired depth surface must free its image once a new surface is created");
    Require(!destroyed(lastImage), "the new depth surface must keep its image");
    Require(DepthSurfacesReleased() > released, "freeing retired depth surfaces must be counted for the framebuffer cache");

    constexpr std::uint64_t sampled = 0x98000000;
    DepthSurfaceView(context, {sampled, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    const auto sampledImage = lastImage;
    auto view = lookup(context, {sampled, FormatR32Float, 256, 256});
    Require(view != nullptr, "an R32F view of a D32 surface's own extent must sample its depth plane");
    RetireDepthSurfaces(context.device, sampled, bytes);
    DepthSurfaceView(context, {sampled + 0x100000u, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    Require(!destroyed(sampledImage), "a retired depth surface must keep its image while its sampled view is held");
    view.reset();
    DepthSurfaceView(context, {sampled + 0x200000u, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    Require(destroyed(sampledImage), "a retired depth surface must free its image once its sampled view is released");

    constexpr std::uint64_t kept = 0x9c000000;
    DepthSurfaceView(context, {kept, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    const auto keptImage = lastImage;
    DepthSurfaceView(context, {kept + 0x100000u, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    Require(!destroyed(keptImage), "a depth surface that was not retired must keep its image");
    ClearDepthSurfaces(context.device);
}

std::atomic<std::uint64_t> untrackedHtileBegin{0};
std::atomic<std::uint64_t> untrackedHtileEnd{0};
std::atomic<int> untrackedHtileReads{0};

void CountUntrackedHtileReads(std::uint64_t address, std::size_t bytes) {
    if (address < untrackedHtileEnd.load() && address + bytes > untrackedHtileBegin.load()) untrackedHtileReads.fetch_add(1);
}

void TestUntrackedHtileScan(const Context& context) {
    ClearDepthSurfaces(context.device);
    constexpr VkExtent2D extent{1024, 512};
    const auto bytes = static_cast<std::size_t>(HtileMetadataBytes(extent));
    std::vector<std::uint32_t> words(bytes / 4u, 0xffffffffu);
    const auto htile = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(words.data()));
    Require(!AgcDriver::GuestMemory::Watched(htile, bytes), "the HTILE must lie outside the write watch");
    DepthTarget target{0xb4000000, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    target.htileAddress = htile;
    untrackedHtileBegin = htile;
    untrackedHtileEnd = htile + bytes;
    const auto view = [&] {
        untrackedHtileReads = 0;
        DepthSurfaceView(context, target);
        return untrackedHtileReads.load();
    };
    AgcDriver::GuestMemory::SetFlushHook(&CountUntrackedHtileReads);
    try {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Require(view() >= 2, "the first check of an HTILE outside the write watch must read it");
        const auto image = lastImage;
        Require(view() == 1, "an HTILE outside the write watch that held no clear must flush pending GPU results before its stamp and not be read again before the stamp moves");
        std::fill(words.begin(), words.end(), 0u);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        clearedImages.clear();
        Require(view() >= 2, "an HTILE outside the write watch must be read again in a new collect epoch");
        Require(std::ranges::find(clearedImages, image) != clearedImages.end(), "a clear the CPU wrote into an HTILE outside the write watch must be found in the next collect epoch");
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    ClearDepthSurfaces(context.device);
}

void TestUntrackedHtileStencilScan(const Context& context) {
    ClearDepthSurfaces(context.device);
    constexpr VkExtent2D extent{1024, 512};
    const auto bytes = static_cast<std::size_t>(HtileMetadataBytes(extent));
    std::vector<std::uint32_t> words(bytes / 4u, 0x0000000fu);
    const auto htile = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(words.data()));
    Require(!AgcDriver::GuestMemory::Watched(htile, bytes), "the HTILE must lie outside the write watch");
    DepthTarget target{0xb8000000, 0xb8800000, extent, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0};
    target.htileAddress = htile;
    untrackedHtileBegin = htile;
    untrackedHtileEnd = htile + bytes;
    const auto view = [&] {
        untrackedHtileReads = 0;
        DepthSurfaceView(context, target);
        return untrackedHtileReads.load();
    };
    AgcDriver::GuestMemory::SetFlushHook(&CountUntrackedHtileReads);
    try {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Require(view() >= 2, "the first check of an HTILE outside the write watch must read it");
        const auto image = lastImage;
        clearedImages.clear();
        Require(view() == 1, "an HTILE outside the write watch that held no clear without stencil must not be read again for the same lookup");
        Require(std::ranges::find(clearedImages, image) == clearedImages.end(), "HTILE words 0x0000000f without stencil in the HTILE must not clear the surface");
        target.htileStencil = true;
        clearedImages.clear();
        Require(view() >= 2, "an HTILE outside the write watch scanned without stencil must be read again for a lookup with stencil in the same collect epoch");
        Require(std::ranges::find(clearedImages, image) != clearedImages.end(), "HTILE words 0x0000000f with stencil in the HTILE must clear the stencil after a scan without stencil in the same collect epoch");
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    ClearDepthSurfaces(context.device);
}

void TestUntrackedHtileDriverStore(const Context& context) {
    ClearDepthSurfaces(context.device);
    constexpr VkExtent2D extent{1024, 512};
    const auto bytes = static_cast<std::size_t>(HtileMetadataBytes(extent));
    std::vector<std::uint32_t> words(bytes / 4u, 0xffffffffu);
    const auto htile = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(words.data()));
    Require(!AgcDriver::GuestMemory::Watched(htile, bytes), "the HTILE must lie outside the write watch");
    DepthTarget target{0xbc000000, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    target.htileAddress = htile;
    untrackedHtileBegin = htile;
    untrackedHtileEnd = htile + bytes;
    const auto view = [&] {
        untrackedHtileReads = 0;
        DepthSurfaceView(context, target);
        return untrackedHtileReads.load();
    };
    AgcDriver::GuestMemory::SetFlushHook(&CountUntrackedHtileReads);
    try {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Require(view() >= 2, "the first check of an HTILE outside the write watch must read it");
        const auto image = lastImage;
        Require(view() == 1, "an HTILE outside the write watch that held no clear must not be read again before its stamp moves");
        std::fill(words.begin(), words.end(), 0u);
        AgcDriver::GuestMemory::MarkWritten(htile, bytes);
        clearedImages.clear();
        Require(view() >= 2, "an HTILE outside the write watch must be read again after a driver store in the same collect epoch");
        Require(std::ranges::find(clearedImages, image) != clearedImages.end(), "a clear stored by the driver into an HTILE outside the write watch must be found in the same collect epoch");
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    ClearDepthSurfaces(context.device);
}

void TestUnmappedStalePlaneFlush(const Context& context) {
    ClearDepthSurfaces(context.device);
    constexpr VkExtent2D extent{256, 256};
    constexpr std::uint64_t unmapped = 0xa0000000;
    DepthTarget target{unmapped, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    target.depthSwizzle = XorSwizzleMode(TextureTileMode::kZ64KBX);
    Require(DepthSurfaceView(context, target, true) != VK_NULL_HANDLE, "a mirrorable depth surface must render");
    Require(DepthPlanesStale(), "a written mirrorable depth surface must leave its plane stale");
    std::vector<std::pair<std::uint64_t, std::uint64_t>> stored;
    try {
        stored = FlushDepthPlanes(unmapped, DepthSliceBytes(extent, 4u));
    } catch (const std::exception& error) {
        Require(false, std::string("flushing a depth plane whose guest memory is not mapped must drop its results: ") + error.what());
    }
    Require(stored.empty(), "a depth plane without mapped guest memory must not be stored");
    Require(!DepthPlanesStale(), "a dropped depth plane must not stay stale");
    ClearDepthSurfaces(context.device);
}

void TestReadOnlyStalePlaneFlush(const Context& context) {
    ClearDepthSurfaces(context.device);
    constexpr std::size_t bytes = 1024 * 1024;
#ifdef _WIN32
    const auto release = [](void* pointer) { VirtualFree(pointer, 0, MEM_RELEASE); };
    std::unique_ptr<void, decltype(release)> mapping(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY), release);
    Require(mapping != nullptr, "cannot map a read-only depth plane");
#else
    const auto release = [](void* pointer) { munmap(pointer, bytes); };
    std::unique_ptr<void, decltype(release)> mapping(mmap(nullptr, bytes, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), release);
    Require(mapping.get() != MAP_FAILED, "cannot map a read-only depth plane");
#endif
    const auto address = (reinterpret_cast<std::uintptr_t>(mapping.get()) + 65535u) & ~std::uintptr_t{65535u};
    constexpr VkExtent2D extent{256, 256};
    const auto planeBytes = DepthSliceBytes(extent, 4u);
    Require(!AgcDriver::GuestMemory::CommittedRanges(address, planeBytes).empty(), "a read-only depth plane must remain readable");
    Require(AgcDriver::GuestMemory::CommittedRanges(address, planeBytes, true).empty(), "a read-only depth plane must not be writable");
    auto storageContext = context;
    storageContext.formatProperties = [](VkPhysicalDevice, VkFormat, VkFormatProperties* properties) {
        *properties = {};
        properties->optimalTilingFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    };
    TextureDetiler detiler(storageContext);
    storageContext.detiler = &detiler;
    DepthTarget target{address, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    target.depthSwizzle = XorSwizzleMode(TextureTileMode::kZ64KBX);
    Require(DepthSurfaceView(storageContext, target, true) != VK_NULL_HANDLE, "a read-only depth plane must render");
    Require(DepthPlanesStale(), "the read-only depth plane must have pending results");
    bool rejected = false;
    try {
        static_cast<void>(FlushDepthPlanes(address, planeBytes));
    } catch (const std::runtime_error& error) {
        rejected = std::string_view(error.what()).find("storage texture has no committed guest pages") != std::string_view::npos;
    }
    Require(rejected, "a mapped read-only depth plane must retain the unsupported-store rejection");
    Require(DepthPlanesStale(), "a mapped read-only depth plane must retain its pending results");
    ClearDepthSurfaces(context.device);
}

void RunDepthSurfaceReuseTests() {
    static int deviceTag = 0;
    Context context{};
    context.device = reinterpret_cast<VkDevice>(&deviceTag);
    context.deviceProc = deviceProc;
    context.formatProperties = formatProperties;
    context.memory.memoryTypeCount = 1;
    context.memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    context.limits.maxFramebufferWidth = 16384;
    context.limits.maxFramebufferHeight = 16384;
    context.limits.maxFramebufferLayers = 1;
    TestD16StencilFallback(context);
    TestMultisampleDepthOwnership(context);
    TestMultisampleStencilAliases(context);
    TestStencilUnormView(context);
    TestUntrackedHtileScan(context);
    TestUntrackedHtileStencilScan(context);
    TestUntrackedHtileDriverStore(context);
    TestUnmappedStalePlaneFlush(context);
    TestReadOnlyStalePlaneFlush(context);
    constexpr std::uint64_t depth32 = 0x40000000;
    constexpr std::uint64_t depth16 = 0x50000000;
    constexpr std::uint64_t depthStencil = 0x60000000;
    constexpr std::uint64_t stencil = 0x60800000;
    DepthSurfaceView(context, {depth32, 0, {384, 384}, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
    DepthSurfaceView(context, {depth16, 0, {256, 256}, VK_FORMAT_D16_UNORM, 1.0f, 0});
    DepthSurfaceView(context, {depthStencil, stencil, {320, 180}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0});

    Require(lookup(context, {depth32, FormatR32Float, 384, 384}) != nullptr, "an R32F view of a D32 surface's own extent must sample its depth plane");
    Require(lookup(context, {depth16, FormatR16Unorm, 256, 256}) != nullptr, "an R16 view of a D16 surface's own extent must sample its depth plane");
    Require(lookup(context, {depth32, FormatR32Uint, 384, 384}) != nullptr, "a 32-bit raw depth bits view of a D32 surface must sample its depth plane");
    Require(lookup(context, {depth16, FormatR16Uint, 256, 256}) != nullptr, "a 16-bit raw depth bits view of a D16 surface must sample its depth plane");

    Require(lookup(context, {depth32, FormatR32Float, 192, 192}) == nullptr, "a view of another extent over a depth surface must be read as reused memory");
    Require(lookup(context, {depth32, FormatR11G11B10Float, 384, 384}) == nullptr, "an R11G11B10 view of a D32 surface's own extent must be read as reused memory");
    Require(lookup(context, {depth32, FormatR32Float, 512, 512, TileDepth64KB, Type2DArray, 5}) == nullptr, "a 2D array view of another extent over a depth surface must be read as reused memory");
    Require(lookup(context, {depth32, FormatR32Uint, 384, 384, TileRenderTarget64KB}) == nullptr, "an R32 uint view without a depth layout must be read as reused memory");
    Require(lookup(context, {depth16, FormatR32Float, 256, 256}) == nullptr, "an R32F view of a D16 surface's own extent must be read as reused memory");
    Require(lookup(context, {depth32, FormatR32Float, 384, 384, TileDepth64KB, Type2D, 0, 0, 0, 4}) != nullptr, "the first level of a mipmapped R32F view of a D32 surface's own extent must sample its depth plane");
    Require(lookup(context, {depth32, FormatR32Float, 384, 384, TileDepth64KB, Type2D, 0, 1, 1, 4}) == nullptr, "a view that starts past the first level of a D32 surface's own extent must be read as reused memory");

    Require(lookup(context, {stencil, FormatR8Uint, 320, 180}) != nullptr, "an 8-bit view of a stencil plane's own extent must sample the stencil plane");
    Require(lookup(context, {stencil, FormatR8G8B8A8Unorm, 320, 180, TileRenderTarget64KB}) == nullptr, "an RGBA8 view of a stencil plane's own extent must be read as reused memory");
    Require(lookup(context, {stencil, FormatR32Float, 320, 180}) == nullptr, "an R32F view of a stencil plane's own extent must be read as reused memory");

    const auto storageAliasesDepth = [&](const Context& owner, const View& view) {
        const auto words = tsharp(view);
        return DepthSurfaceAt(owner, words, DecodeTextureResource(words));
    };
    Require(storageAliasesDepth(context, {depth32, FormatR32Float, 384, 384}), "storage access to the actual D32 plane must remain rejected");
    Require(storageAliasesDepth(context, {depth16, FormatR16Uint, 256, 256}), "raw D16 storage access must remain rejected");
    Require(!storageAliasesDepth(context, {depth32, FormatR32Float, 128, 128}), "temporary memory reused at a different extent is not a depth storage alias");
    Require(!storageAliasesDepth(context, {depth32, FormatR11G11B10Float, 384, 384}), "temporary memory reused for a color format is not a depth storage alias");
    Require(!storageAliasesDepth(context, {depth32, FormatR32Uint, 384, 384, TileRenderTarget64KB}), "raw integer color memory without a depth layout is not a depth storage alias");
    auto otherDevice = context;
    static int otherDeviceTag = 0;
    otherDevice.device = reinterpret_cast<VkDevice>(&otherDeviceTag);
    Require(!storageAliasesDepth(otherDevice, {depth32, FormatR32Float, 384, 384}), "a depth surface on another device must not block a storage lookup");

    constexpr std::uint64_t stencil16 = 0x60000000;
    DepthSurfaceView(context, {depth16, stencil16, {128, 128}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0});
    Require(!storageAliasesDepth(context, {depth16, FormatR16Unorm, 256, 256}), "an older depth extent at the same address must not override the newest surface");
    Require(storageAliasesDepth(context, {depth16, FormatR16Unorm, 128, 128}), "the newest depth extent must still reject actual storage access");
    Require(storageAliasesDepth(context, {stencil16, FormatR8Uint, 128, 128}), "storage access to the actual stencil plane must remain rejected");
    Require(!storageAliasesDepth(context, {stencil16, FormatR11G11B10Float, 128, 128}), "a stencil address reused for color is not a stencil storage alias");

    constexpr std::uint64_t shared = 0x70000000;
    constexpr std::uint64_t stencilA = 0x71000000;
    constexpr std::uint64_t stencilB = 0x72000000;
    constexpr std::uint64_t otherDepth = 0x73000000;
    const auto pairing = [&](std::uint64_t depth, std::uint64_t stencil) {
        const auto before = createdImages.size();
        DepthSurfaceView(context, {depth, stencil, {256, 256}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0});
        return createdImages.size() != before ? createdImages[before] : VK_NULL_HANDLE;
    };
    const auto withA = pairing(shared, stencilA);
    imageCopies.clear();
    const auto withB = pairing(shared, stencilB);
    Require(withA != VK_NULL_HANDLE && withB != VK_NULL_HANDLE && withA != withB, "each depth/stencil pairing must have its own image");
    Require(imageCopies.size() == 1 && imageCopies[0] == std::make_tuple(withA, withB, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT)), "a depth plane paired with another stencil plane must carry its depth over");
    imageCopies.clear();
    pairing(shared, stencilB);
    Require(imageCopies.empty(), "drawing again with the same pairing must not copy");
    pairing(shared, stencilA);
    Require(imageCopies.size() == 1 && imageCopies[0] == std::make_tuple(withB, withA, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT)), "returning to the first pairing must carry the depth written through the second");
    imageCopies.clear();
    const auto otherWithA = pairing(otherDepth, stencilA);
    Require(imageCopies.size() == 1 && imageCopies[0] == std::make_tuple(withA, otherWithA, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_STENCIL_BIT)), "a stencil plane paired with another depth plane must carry its stencil over");
    {
        constexpr std::uint64_t arrayBase = 0x30000000;
        constexpr std::uint64_t htile = 0x31000000;
        constexpr VkExtent2D extent{1024, 512};
        const auto sliceHtile = HtileMetadataBytes(extent);
        DepthTarget first{arrayBase, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
        first.htileAddress = htile;
        DepthTarget second{arrayBase + DepthSliceBytes(extent, 4u), 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
        second.htileAddress = htile + sliceHtile;
        DepthSurfaceView(context, first);
        const auto firstImage = lastImage;
        DepthSurfaceView(context, second);
        const auto secondImage = lastImage;
        const auto clearsAfter = [&](std::size_t bytes) {
            clearedImages.clear();
            NoteHtileDepthClear(htile, static_cast<std::size_t>(bytes));
            DepthSurfaceView(context, first);
            DepthSurfaceView(context, second);
            return clearedImages;
        };
        Require(clearsAfter(sliceHtile) == std::vector<VkImage>{firstImage}, "an HTILE clear of the first slice must clear that slice alone");
        Require(clearsAfter(2u * sliceHtile) == std::vector<VkImage>{firstImage, secondImage}, "an HTILE clear over both slices must clear both");
    }
    TestRetiredSurfaceRelease(context);

    ClearDepthSurfaces(context.device);
}
