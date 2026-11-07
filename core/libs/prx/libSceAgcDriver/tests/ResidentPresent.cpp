#include "ResidentPresent.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::DisplayBuffer;
using AgcDriver::ResidentPresent;
using AgcDriver::ResidentPresentSlice;

constexpr std::uint64_t Bgra8 = 0x8000000000000000ull;
constexpr std::uint64_t Rgba8 = 0x8000000022000000ull;
constexpr std::uint64_t TenBit = 0x0100000000000000ull;
constexpr std::uint64_t Rgba16Float = 0xc001000622000000ull;
constexpr std::uint64_t Rgb10Pq = 0x8100070422000000ull;
constexpr std::uint64_t Bgr10Pq = 0x8100070400000000ull;

void decisionTests() {
    using AgcDriver::DisplayTexelFormat;
    using AgcDriver::ResidentPresentPath;
    Require(DisplayTexelFormat(Bgra8) == VK_FORMAT_B8G8R8A8_UNORM && DisplayTexelFormat(Rgba8) == VK_FORMAT_R8G8B8A8_UNORM, "8-bit display texel formats are wrong");
    Require(DisplayTexelFormat(Bgra8 | TenBit) == VK_FORMAT_A2R10G10B10_UNORM_PACK32 && DisplayTexelFormat(Rgba8 | TenBit) == VK_FORMAT_A2B10G10R10_UNORM_PACK32, "10-bit display texel formats are wrong");
    Require(ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, Bgra8, true) == ResidentPresent::Blit && ResidentPresentPath(VK_FORMAT_R8G8B8A8_UNORM, Rgba8, true) == ResidentPresent::Blit, "an 8-bit image in its display's order is not blitted");
    Require(ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, Bgra8, false) == ResidentPresent::Convert, "an 8-bit image that cannot be a blit source is not converted");
    Require(ResidentPresentPath(VK_FORMAT_R8G8B8A8_UNORM, Bgra8, true) == ResidentPresent::Convert && ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, Rgba8, true) == ResidentPresent::Convert, "an 8-bit image in the other order is not converted");
    Require(ResidentPresentPath(VK_FORMAT_R8G8B8A8_SRGB, Rgba8, true) == ResidentPresent::Convert, "an sRGB image is blitted (the blit would decode it)");
    Require(ResidentPresentPath(VK_FORMAT_R8G8B8A8_UINT, Rgba8, true) == ResidentPresent::Convert && ResidentPresentPath(VK_FORMAT_B8G8R8A8_UINT, Bgra8, true) == ResidentPresent::Convert, "an 8-bit integer image is not converted");
    Require(ResidentPresentPath(VK_FORMAT_R8G8B8A8_UINT, Rgba8 | TenBit, true) == ResidentPresent::Convert, "an 8-bit integer image is converted for a 10-bit display");
    Require(ResidentPresentPath(VK_FORMAT_R8G8B8A8_SRGB, Rgba8 | TenBit, true) == ResidentPresent::Convert && ResidentPresentPath(VK_FORMAT_B8G8R8A8_SRGB, Bgra8 | TenBit, true) == ResidentPresent::Convert, "an sRGB image is converted for a 10-bit display");
    for (const auto display : {Bgra8 | TenBit, Rgba8 | TenBit}) {
        for (const auto storage : {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32}) {
            Require(ResidentPresentPath(storage, display, true) == ResidentPresent::Convert, "a 10-bit image is blitted (the blit rounds where the guest path truncates)");
        }
        Require(ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, display, true) == ResidentPresent::Convert, "an 8-bit image presents a 10-bit display");
    }
    Require(DisplayTexelFormat(Bgr10Pq) == VK_FORMAT_A2R10G10B10_UNORM_PACK32 && DisplayTexelFormat(Rgb10Pq) == VK_FORMAT_A2B10G10R10_UNORM_PACK32, "PQ display texel formats are wrong");
    for (const auto display : {Bgr10Pq, Rgb10Pq}) {
        for (const auto storage : {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32}) {
            Require(ResidentPresentPath(storage, display, true) == ResidentPresent::Convert, "a PQ image is blitted (it must be converted to SDR)");
        }
        Require(ResidentPresentPath(VK_FORMAT_B8G8R8A8_UNORM, display, true) == ResidentPresent::None, "an 8-bit image presents a PQ display");
    }
    Require(ResidentPresentPath(VK_FORMAT_A2B10G10R10_UNORM_PACK32, Bgra8, true) == ResidentPresent::None && ResidentPresentPath(VK_FORMAT_R16G16_SFLOAT, Bgra8, true) == ResidentPresent::None, "an image of another type presents an 8-bit display");
    const auto guest = FindGuestColorTargetFormat(VK_FORMAT_A2R10G10B10_UNORM_PACK32, 4);
    Require(guest.has_value() && guest == FindGuestTextureFormat(VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4), "the blue-low 10-bit color target has no storage format");
}

std::uint32_t memoryType(const Context& context, std::uint32_t bits, VkMemoryPropertyFlags flags) {
    for (std::uint32_t i = 0; i < context.memory.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) != 0 && (context.memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
    }
    throw std::runtime_error("no device-local memory type for the test image");
}

void sliceTests() {
    constexpr std::uint64_t image = 0x208000000ull;
    constexpr std::uint64_t slice = 0x1fe0000ull;
    Require(ResidentPresentSlice(image, slice, 1, image, slice) == 0u, "a display buffer that is the whole image is not its slice 0");
    Require(ResidentPresentSlice(image, 2 * slice, 2, image, slice) == 0u, "a display buffer at the start of a two-slice image is not its slice 0");
    Require(ResidentPresentSlice(image, 2 * slice, 2, image + slice, slice) == 1u, "a display buffer one slice into a two-slice image is not its slice 1");
    Require(ResidentPresentSlice(image, 6 * slice, 6, image + 5 * slice, slice) == 5u, "a display buffer at the last of six slices is not slice 5");
    Require(!ResidentPresentSlice(image, 2 * slice, 2, image + 2 * slice, slice), "a display buffer past the image's last slice has a slice");
    Require(!ResidentPresentSlice(image, 2 * slice, 2, image + 0x10000, slice), "a display buffer between two slices has a slice");
    Require(!ResidentPresentSlice(image, 2 * slice, 2, image - slice, slice), "a display buffer before the image has a slice");
    Require(!ResidentPresentSlice(image, 2 * slice, 2, image, 2 * slice), "a display buffer the size of two slices has a slice");
    Require(!ResidentPresentSlice(image, 2 * slice, 2, image, slice / 2), "a display buffer of half a slice has a slice");
    Require(!ResidentPresentSlice(image, 2 * slice + 1, 2, image, slice), "an image whose bytes do not divide into its slices has a slice");
    Require(!ResidentPresentSlice(image, 0, 0, image, slice) && !ResidentPresentSlice(image, slice, 1, image, 0), "an empty image or display buffer has a slice");
}

void conversionTest(const Context& context, std::uint64_t pixelFormat, VkFormat format, std::uint32_t layer = 0, float whiteNits = AgcDriver::SdrToneMapLuminanceNits) {
    constexpr std::uint32_t width = 200;
    constexpr std::uint32_t height = 130;
    const auto texelBytes = AgcDriver::DisplayTexelBytes(pixelFormat);
    std::vector<std::byte> words(static_cast<std::size_t>(width) * height * texelBytes);
    std::uint32_t seed = 0x9e3779b9u ^ static_cast<std::uint32_t>(format);
    for (std::size_t i = 0; i < words.size(); ++i) {
        seed = seed * 1664525u + 1013904223u;
        words[i] = static_cast<std::byte>(seed >> 24u);
    }
    const ColorTargetLayout layout(width, height, ColorTileMode::RenderTarget, texelBytes);
    std::vector<std::byte> storage(layout.Bytes() + 65536);
    auto* aligned = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 65535u) & ~std::uintptr_t{65535});
    std::span<std::byte> guest(aligned, layout.Bytes());
    const DisplayBuffer display{reinterpret_cast<std::uint64_t>(aligned), pixelFormat, width, height};
    Require(AgcDriver::DisplayBufferSize(display) == guest.size(), "test display buffer size differs from the color layout");
    if (texelBytes == 4u) {
        layout.Tile(words, guest);
    } else {
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) std::memcpy(guest.data() + AgcDriver::DisplayBufferOffset(display, x, y), words.data() + (static_cast<std::size_t>(y) * width + x) * texelBytes, texelBytes);
        }
    }
    auto expected = AgcDriver::DecodeDisplayBuffer(display, guest);
    if (AgcDriver::DisplayPq(pixelFormat) && whiteNits != AgcDriver::SdrToneMapLuminanceNits) {
        for (std::size_t i = 0; i < expected.size(); i += 4) {
            std::uint32_t word = 0;
            std::memcpy(&word, words.data() + i, 4);
            const auto pixel = AgcDriver::PqToSdrPixel(word, AgcDriver::DisplayRedLow(pixelFormat), whiteNits);
            for (std::size_t channel = 0; channel < pixel.size(); ++channel) expected[i + channel] = static_cast<std::byte>(pixel[channel]);
        }
    }

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = layer + 1u;
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
    const bool pq = AgcDriver::DisplayPq(pixelFormat);
    const bool tenBit = AgcDriver::DisplayTenBit(pixelFormat) && !pq;
    const bool halfFloat = AgcDriver::DisplayHalfFloat(pixelFormat);
    const auto pqWhiteNits = pq ? std::optional<float>(whiteNits) : std::nullopt;
    const auto barrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    const auto readback = [&](VkCommandBuffer commands, Buffer& destination) {
        const VkBufferCopy copy{0, 0, expected.size()};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, transfer.LinearBuffer(), destination.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
    };
    transfer.Upload(display.address, width, height, ColorTileMode::RenderTarget, texelBytes);
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layer + 1u};
        barrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
        region.imageExtent = {width, height, 1};
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, upload.Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        if (layer != 0) {
            const VkClearColorValue other{};
            const VkImageSubresourceRange lower{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layer};
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &other, 1, &lower);
        }
        auto toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        transfer.Detile(commands, redLow, tenBit, halfFloat, pqWhiteNits);
        readback(commands, fromGuest);
        if (!pq) {
            transfer.DetileImage(commands, image, VK_IMAGE_LAYOUT_GENERAL, width, height, ColorTileMode::RenderTarget, redLow, tenBit, halfFloat, layer);
            readback(commands, fromImage);
        }
        batch.SubmitAndWait();
    }
    const auto name = std::string(halfFloat ? "16-bit float " : pq ? "PQ " : tenBit ? "10-bit " : "8-bit ") + (redLow ? "red-low" : "blue-low") + " display from layer " + std::to_string(layer) + " of VkFormat " + std::to_string(static_cast<int>(format));
    const auto matches = [&](std::span<const std::byte> actual) {
        return std::equal(expected.begin(), expected.end(), actual.begin(), [&](std::byte want, std::byte got) { return std::abs(std::to_integer<int>(want) - std::to_integer<int>(got)) <= (pq ? 1 : 0); });
    };
    Require(matches(fromGuest.Bytes()), "the GPU detile differs from the CPU decode: " + name);
    Require(pq || matches(fromImage.Bytes()), "the resident image presents other pixels than guest memory: " + name);
}

}

void RunResidentPresentTests(const Context& context) {
    decisionTests();
    sliceTests();
    conversionTest(context, Bgra8 | TenBit, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    conversionTest(context, Rgba8 | TenBit, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    conversionTest(context, Bgra8 | TenBit, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    conversionTest(context, Bgra8, VK_FORMAT_R8G8B8A8_UNORM);
    conversionTest(context, Rgba8, VK_FORMAT_B8G8R8A8_UNORM);
    conversionTest(context, Rgba16Float, VK_FORMAT_R16G16B16A16_SFLOAT);
    conversionTest(context, Rgba8, VK_FORMAT_B8G8R8A8_UNORM, 1);
    conversionTest(context, Bgra8 | TenBit, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 2);
    conversionTest(context, Rgb10Pq, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    conversionTest(context, Bgr10Pq, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    conversionTest(context, Rgb10Pq, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 0, 1000.0f);
    conversionTest(context, Bgr10Pq, VK_FORMAT_A2R10G10B10_UNORM_PACK32, 0, 150.0f);
    std::cout << "resident present format and conversion tests passed\n";
}
