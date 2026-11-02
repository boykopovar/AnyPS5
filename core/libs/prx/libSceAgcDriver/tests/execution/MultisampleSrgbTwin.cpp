#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::ColorTarget;
using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Side = 64;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::byte*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::byte*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "multisampled sRGB twin: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::byte* block = nullptr;
};

GuestBlock& NewBlock() {
    static auto* blocks = new std::deque<GuestBlock>();
    return blocks->emplace_back();
}

ColorTarget MultisampledTarget(const GuestBlock& color, VkFormat format) {
    ColorTarget target{};
    target.address = color.Address();
    target.surfaceAddress = target.address;
    target.extent = {Side, Side};
    target.surfaceExtent = target.extent;
    target.format = format;
    target.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
    target.elementBytes = 4;
    target.bytes = BlockBytes / 4;
    target.samples = 4;
    return target;
}

struct Acquired {
    VkImageView view = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    std::shared_ptr<void> owner;
};

Acquired Acquire(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context, const ColorTarget& target) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Acquired acquired;
    acquired.view = AgcDriver::Graphics::MultisampleTargetView(context, target, acquired.owner, &acquired.image);
    Require(acquired.view != VK_NULL_HANDLE && acquired.image != VK_NULL_HANDLE && acquired.owner != nullptr, "the multisampled target has no view");
    device.SubmitRecorded(true);
    device.WaitIdle();
    return acquired;
}

std::array<std::byte, 4> UniformTexel(const AgcDriver::Graphics::Context& context, VkImage multisampled, VkFormat format) {
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {Side, Side, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage resolved = VK_NULL_HANDLE;
    AgcDriver::Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &resolved), "vkCreateImage resolved texels");
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, resolved, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory memory = VK_NULL_HANDLE;
    AgcDriver::Graphics::Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory resolved texels");
    AgcDriver::Graphics::Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, resolved, memory, 0), "vkBindImageMemory resolved texels");
    AgcDriver::Graphics::Buffer readback(context, static_cast<std::size_t>(Side) * Side * 4u, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        AgcDriver::Graphics::CommandBatch batch(context);
        const auto commands = batch.Handle();
        std::array<VkImageMemoryBarrier, 2> barriers{};
        for (auto& barrier : barriers) {
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        }
        barriers[0].image = multisampled;
        barriers[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].oldLayout = barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[1].image = resolved;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(barriers.size()), barriers.data());
        VkImageResolve region{};
        region.srcSubresource = region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.extent = {Side, Side, 1};
        context.Function<PFN_vkCmdResolveImage>("vkCmdResolveImage")(commands, multisampled, VK_IMAGE_LAYOUT_GENERAL, resolved, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        VkMemoryBarrier toCopy{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        toCopy.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &toCopy, 0, nullptr, 0, nullptr);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {Side, Side, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, resolved, VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        VkMemoryBarrier toHost{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        toHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &toHost, 0, nullptr, 0, nullptr);
        batch.SubmitAndWait();
    }
    readback.Invalidate();
    const auto bytes = readback.Bytes();
    std::array<std::byte, 4> texel{};
    std::copy_n(bytes.begin(), texel.size(), texel.begin());
    for (std::size_t offset = 0; offset < bytes.size(); offset += texel.size()) Require(std::equal(texel.begin(), texel.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset)), "a cleared multisampled target holds different texels");
    context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, resolved, nullptr);
    context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
    return texel;
}

std::array<std::byte, 4> Bytes(std::uint32_t pattern) {
    std::array<std::byte, 4> bytes{};
    std::memcpy(bytes.data(), &pattern, bytes.size());
    return bytes;
}

void FillTakenThroughTwin(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context, VkFormat created, VkFormat used, std::uint32_t pattern) {
    auto& color = NewBlock();
    const auto target = MultisampledTarget(color, created);
    const auto rendered = Acquire(device, context, target);
    const auto bytes = static_cast<std::size_t>(AgcDriver::Graphics::MultisampledColorBytes(target));
    Require(AgcDriver::Graphics::MultisampledColorFillAbsorbs(color.Address(), bytes, pattern), "the color fill is not taken as the target's clear in its creation format");
    AgcDriver::Graphics::NoteColorMetadataFill(color.Address(), bytes, pattern);
    const auto twin = Acquire(device, context, MultisampledTarget(color, used));
    Require(twin.image == rendered.image, "the twin use of the filled target does not reach its samples");
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Require(UniformTexel(context, twin.image, created) == Bytes(pattern), "a fill taken as a clear through the twin format does not store the guest's bytes");
}

bool TwinFormatsShareTheirSamples(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if ((context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0) {
        std::cout << "skipped, the device has no 4x color targets\n";
        return false;
    }
    struct ClearTargets {
        AgcDriver::VulkanDevice& device;
        VkDevice handle;
        ~ClearTargets() {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            device.SubmitRecorded(true);
            device.WaitIdle();
            AgcDriver::Graphics::ClearMultisampleTargets(handle);
        }
    } clearTargets{device, context.device};
    for (const auto [first, second] : {std::pair{VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM}, std::pair{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB}}) {
        if (AgcDriver::Graphics::SrgbTwinFormat(context, first) != second && AgcDriver::Graphics::SrgbTwinFormat(context, second) != first) continue;
        auto& color = NewBlock();
        const auto rendered = Acquire(device, context, MultisampledTarget(color, first));
        const auto twin = Acquire(device, context, MultisampledTarget(color, second));
        Require(twin.image == rendered.image && twin.owner == rendered.owner, "a use of a multisampled surface in its sRGB twin format does not reach the samples rendered in the other format");
        Require(twin.view != rendered.view, "a use of a multisampled surface in its sRGB twin format is given the view of the other format");
        const auto again = Acquire(device, context, MultisampledTarget(color, first));
        Require(again.image == rendered.image && again.view == rendered.view, "returning to the first format does not give back the first format's view");
    }
    if (AgcDriver::Graphics::SrgbTwinFormat(context, VK_FORMAT_R8G8B8A8_UNORM) == VK_FORMAT_R8G8B8A8_SRGB) {
        FillTakenThroughTwin(device, context, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB, 0x80808080u);
        FillTakenThroughTwin(device, context, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, 0xff0000ffu);
    }
    auto& other = NewBlock();
    const auto unorm = Acquire(device, context, MultisampledTarget(other, VK_FORMAT_R8G8B8A8_UNORM));
    const auto floating = Acquire(device, context, MultisampledTarget(other, VK_FORMAT_R16G16_SFLOAT));
    Require(floating.image != unorm.image, "a multisampled surface used in an unrelated format of the same size shares the other format's samples");
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool tested = TwinFormatsShareTheirSamples(*device);
        LibcRunShutdown_nid_postfix();
        if (!tested) return VulkanTestSkipped;
        std::cout << "multisampled sRGB twin tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            LibcRunShutdown_nid_postfix();
        } catch (...) {
        }
        return 1;
    }
}
