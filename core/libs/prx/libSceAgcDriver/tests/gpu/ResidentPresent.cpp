#include <Testing/Test.hpp>
#include "ResidentPresent.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::DisplayBuffer;
using AgcDriver::ResidentPresent;
using Testing::Case;
using Testing::Require;

constexpr std::uint64_t Bgra8 = 0x8000000000000000ull;
constexpr std::uint64_t Rgba8 = 0x8000000022000000ull;
constexpr std::uint64_t TenBit = 0x0100000000000000ull;

const Case displayTexelFormats{"DisplayTexelFormat_EightAndTenBitDisplays_MapToTheirVulkanFormats", [] {
    using AgcDriver::DisplayTexelFormat;
    Require(DisplayTexelFormat(Bgra8) == VK_FORMAT_B8G8R8A8_UNORM && DisplayTexelFormat(Rgba8) == VK_FORMAT_R8G8B8A8_UNORM, "8-bit display texel formats are wrong");
    Require(DisplayTexelFormat(Bgra8 | TenBit) == VK_FORMAT_A2R10G10B10_UNORM_PACK32 && DisplayTexelFormat(Rgba8 | TenBit) == VK_FORMAT_A2B10G10R10_UNORM_PACK32, "10-bit display texel formats are wrong");
}};

const Case presentPaths{"ResidentPresentPath_ImageAndDisplayFormats_BlitOnlyMatchingEightBitImages", [] {
    using AgcDriver::ResidentPresentPath;
    Require(ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, Bgra8, true) == ResidentPresent::Blit && ResidentPresentPath(VK_FORMAT_R8G8B8A8_UNORM, Rgba8, true) == ResidentPresent::Blit, "an 8-bit image in its display's order is not blitted");
    Require(ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, Bgra8, false) == ResidentPresent::Convert, "an 8-bit image that cannot be a blit source is not converted");
    Require(ResidentPresentPath(VK_FORMAT_R8G8B8A8_UNORM, Bgra8, true) == ResidentPresent::Convert && ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, Rgba8, true) == ResidentPresent::Convert, "an 8-bit image in the other order is not converted");
    Require(ResidentPresentPath(VK_FORMAT_R8G8B8A8_SRGB, Rgba8, true) == ResidentPresent::Convert, "an sRGB image is blitted (the blit would decode it)");
    for (const auto display : {Bgra8 | TenBit, Rgba8 | TenBit}) {
        for (const auto storage : {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32}) {
            Require(ResidentPresentPath(storage, display, true) == ResidentPresent::Convert, "a 10-bit image is blitted (the blit rounds where the guest path truncates)");
        }
        Require(ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, display, true) == ResidentPresent::None, "an 8-bit image presents a 10-bit display");
    }
    Require(ResidentPresentPath(VK_FORMAT_A2B10G10R10_UNORM_PACK32, Bgra8, true) == ResidentPresent::None && ResidentPresentPath(VK_FORMAT_R16G16_SFLOAT, Bgra8, true) == ResidentPresent::None, "an image of another type presents an 8-bit display");
}};

const Case blueLowStorage{"FindGuestColorTargetFormat_BlueLowTenBitTarget_HasAStorageFormat", [] {
    const auto guest = FindGuestColorTargetFormat(VK_FORMAT_A2R10G10B10_UNORM_PACK32, 4);
    Require(guest.has_value() && guest == FindGuestTextureFormat(VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4), "the blue-low 10-bit color target has no storage format");
}};

std::uint32_t memoryType(const Context& context, std::uint32_t bits, VkMemoryPropertyFlags flags) {
    for (std::uint32_t i = 0; i < context.memory.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) != 0 && (context.memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
    }
    Testing::Fail("no device-local memory type for the test image");
}

void ConversionTest(std::uint64_t pixelFormat, VkFormat format) {
    std::lock_guard<AgcDriver::GuestMemory::GpuMutexType> gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto& context = ResidentPresentTestContext();
    constexpr std::uint32_t width = 200;
    constexpr std::uint32_t height = 130;
    std::vector<std::byte> words(static_cast<std::size_t>(width) * height * 4);
    std::uint32_t seed = 0x9e3779b9u ^ static_cast<std::uint32_t>(format);
    for (std::size_t i = 0; i < words.size(); ++i) {
        seed = seed * 1664525u + 1013904223u;
        words[i] = static_cast<std::byte>(seed >> 24u);
    }
    const ColorTargetLayout layout(width, height, ColorTileMode::RenderTarget);
    std::vector<std::byte> storage(layout.Bytes() + 65536);
    auto* aligned = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 65535u) & ~std::uintptr_t{65535});
    std::span<std::byte> guest(aligned, layout.Bytes());
    layout.Tile(words, guest);
    const DisplayBuffer display{reinterpret_cast<std::uint64_t>(aligned), pixelFormat, width, height};
    Require(AgcDriver::DisplayBufferSize(display) == guest.size(), "test display buffer size differs from the color layout");
    const auto expected = AgcDriver::DecodeDisplayBuffer(display, guest);

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage resident present test");
    struct Release {
        const Context& context;
        VkImage& image;
        VkDeviceMemory& memory;
        ~Release() {
            if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
        }
    } release{context, image, memory};
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType(context, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory resident present test");
    Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory resident present test");

    Buffer upload(context, words.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::memcpy(upload.Bytes().data(), words.data(), words.size());
    Buffer fromImage(context, expected.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer fromGuest(context, expected.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    GpuColorTransfer transfer(context);
    const bool redLow = AgcDriver::DisplayRedLow(pixelFormat);
    const bool tenBit = AgcDriver::DisplayTenBit(pixelFormat);
    const auto barrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    const auto readback = [&](VkCommandBuffer commands, Buffer& destination) {
        const VkBufferCopy copy{0, 0, expected.size()};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, transfer.LinearBuffer(), destination.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
    };
    transfer.Upload(display.address, width, height, ColorTileMode::RenderTarget);
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {width, height, 1};
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, upload.Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        auto toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        transfer.Detile(commands, redLow, tenBit);
        readback(commands, fromGuest);
        transfer.DetileImage(commands, image, VK_IMAGE_LAYOUT_GENERAL, width, height, ColorTileMode::RenderTarget, redLow, tenBit);
        readback(commands, fromImage);
        batch.SubmitAndWait();
    }
    const auto name = std::string(tenBit ? "10-bit " : "8-bit ") + (redLow ? "red-low" : "blue-low") + " display from VkFormat " + std::to_string(static_cast<int>(format));
    Require(std::equal(expected.begin(), expected.end(), fromGuest.Bytes().begin()), "the GPU detile differs from the CPU decode: " + name);
    Require(std::equal(expected.begin(), expected.end(), fromImage.Bytes().begin()), "the resident image presents other pixels than guest memory: " + name);
}

const Case tenBitBgraFromA2B10G10R10{"ResidentPresent_TenBitBgraDisplayFromA2B10G10R10_MatchesGuestMemory", [] {
    ConversionTest(Bgra8 | TenBit, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
}};

const Case tenBitRgbaFromA2B10G10R10{"ResidentPresent_TenBitRgbaDisplayFromA2B10G10R10_MatchesGuestMemory", [] {
    ConversionTest(Rgba8 | TenBit, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
}};

const Case tenBitBgraFromA2R10G10B10{"ResidentPresent_TenBitBgraDisplayFromA2R10G10B10_MatchesGuestMemory", [] {
    ConversionTest(Bgra8 | TenBit, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
}};

const Case eightBitBgraFromRgba{"ResidentPresent_EightBitBgraDisplayFromRgbaImage_MatchesGuestMemory", [] {
    ConversionTest(Bgra8, VK_FORMAT_R8G8B8A8_UNORM);
}};

const Case eightBitRgbaFromBgra{"ResidentPresent_EightBitRgbaDisplayFromBgraImage_MatchesGuestMemory", [] {
    ConversionTest(Rgba8, VK_FORMAT_B8G8R8A8_UNORM);
}};

} // namespace
