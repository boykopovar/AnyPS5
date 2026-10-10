#include <Testing/Test.hpp>
#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string_view>

namespace {

using namespace AgcDriver::Graphics;
using Testing::Case;
using Testing::Require;

constexpr std::uint32_t FormatR16Unorm = 7;
constexpr std::uint32_t FormatR16Uint = 11;
constexpr std::uint32_t FormatR32Uint = 20;
constexpr std::uint32_t FormatR32Float = 22;
constexpr std::uint32_t FormatR11G11B10Float = 36;
constexpr std::uint32_t TileDepth64KB = 0x18;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;

std::uintptr_t nextHandle = 0x1000;

template<typename THandle>
THandle newHandle() {
    return reinterpret_cast<THandle>(nextHandle += 0x10);
}

VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage* image) {
    *image = newHandle<VkImage>();
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
VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice, VkImage, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL cmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL cmdClearDepthStencilImage(VkCommandBuffer, VkImage, VkImageLayout, const VkClearDepthStencilValue*, std::uint32_t, const VkImageSubresourceRange*) {}

VKAPI_ATTR void VKAPI_CALL formatProperties(VkPhysicalDevice, VkFormat, VkFormatProperties* properties) {
    *properties = {};
    properties->optimalTilingFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
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
        {"vkCmdClearDepthStencilImage", reinterpret_cast<PFN_vkVoidFunction>(cmdClearDepthStencilImage)}
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
};

std::array<std::uint32_t, 8> tsharp(const View& view) {
    std::array<std::uint32_t, 8> words{};
    const auto base = view.address >> 8u;
    const auto widthMinus1 = view.width - 1u;
    const auto heightMinus1 = view.height - 1u;
    words[0] = static_cast<std::uint32_t>(base);
    words[1] = static_cast<std::uint32_t>((base >> 32u) & 0xffu) | ((view.format & 0x1ffu) << 20u) | ((widthMinus1 & 0x3u) << 30u);
    words[2] = ((widthMinus1 >> 2u) & 0xfffu) | ((heightMinus1 & 0x3fffu) << 14u);
    words[3] = 4u | (5u << 3u) | (6u << 6u) | (7u << 9u) | ((view.tileMode & 0x1fu) << 20u) | ((view.type & 0xfu) << 28u);
    words[4] = view.lastArray & 0x1fffu;
    return words;
}

std::shared_ptr<Texture> lookup(const Context& context, const View& view) {
    const auto words = tsharp(view);
    return DepthSurfaceTexture(context, words, DecodeTextureResource(words), VkComponentMapping{});
}


constexpr std::uint64_t Depth32 = 0x40000000;
constexpr std::uint64_t Depth16 = 0x50000000;

class DepthSurfaces {
public:
    DepthSurfaces() {
        context.device = reinterpret_cast<VkDevice>(&deviceTag);
        context.deviceProc = deviceProc;
        context.formatProperties = formatProperties;
        context.memory.memoryTypeCount = 1;
        context.memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        context.limits.maxFramebufferWidth = 16384;
        context.limits.maxFramebufferHeight = 16384;
        DepthSurfaceView(context, {Depth32, 0, {384, 384}, VK_FORMAT_D32_SFLOAT, 1.0f, 0});
        DepthSurfaceView(context, {Depth16, 0, {256, 256}, VK_FORMAT_D16_UNORM, 1.0f, 0});
    }

    ~DepthSurfaces() {
        ClearDepthSurfaces(context.device);
    }

    DepthSurfaces(const DepthSurfaces&) = delete;
    DepthSurfaces& operator=(const DepthSurfaces&) = delete;

    std::shared_ptr<Texture> Lookup(const View& view) const {
        return lookup(context, view);
    }

private:
    int deviceTag = 0;
    Context context{};
};

const Case sameExtentViews{"DepthSurfaceTexture_SameExtentDepthView_SamplesTheDepthPlane", [] {
    const DepthSurfaces surfaces;
    Require(surfaces.Lookup({Depth32, FormatR32Float, 384, 384}) != nullptr, "an R32F view of a D32 surface's own extent must sample its depth plane");
    Require(surfaces.Lookup({Depth16, FormatR16Unorm, 256, 256}) != nullptr, "an R16 view of a D16 surface's own extent must sample its depth plane");
    Require(surfaces.Lookup({Depth32, FormatR32Uint, 384, 384}) != nullptr, "a 32-bit raw depth bits view of a D32 surface must sample its depth plane");
    Require(surfaces.Lookup({Depth16, FormatR16Uint, 256, 256}) != nullptr, "a 16-bit raw depth bits view of a D16 surface must sample its depth plane");
}};

const Case reusedMemoryViews{"DepthSurfaceTexture_OtherExtentFormatOrLayout_IsReadAsReusedMemory", [] {
    const DepthSurfaces surfaces;
    Require(surfaces.Lookup({Depth32, FormatR32Float, 192, 192}) == nullptr, "a view of another extent over a depth surface must be read as reused memory");
    Require(surfaces.Lookup({Depth32, FormatR11G11B10Float, 384, 384}) == nullptr, "an R11G11B10 view of a D32 surface's own extent must be read as reused memory");
    Require(surfaces.Lookup({Depth32, FormatR32Float, 512, 512, TileDepth64KB, Type2DArray, 5}) == nullptr, "a 2D array view of another extent over a depth surface must be read as reused memory");
    Require(surfaces.Lookup({Depth32, FormatR32Uint, 384, 384, TileRenderTarget64KB}) == nullptr, "an R32 uint view without a depth layout must be read as reused memory");
    Require(surfaces.Lookup({Depth16, FormatR32Float, 256, 256}) == nullptr, "an R32F view of a D16 surface's own extent must be read as reused memory");
}};

} // namespace
