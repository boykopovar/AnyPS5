#include "ColorTransferTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/HalfFloatScanout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <memory>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

void checkConversion(const Context& context, std::uint32_t width, std::uint32_t height, ColorTileMode mode, bool swap) {
    static_assert(std::endian::native == std::endian::little);
    const ColorTargetLayout layout(width, height, mode);
    std::vector<std::byte> storage(layout.Bytes() + layout.Alignment());
    void* aligned = storage.data();
    auto available = storage.size();
    Require(std::align(layout.Alignment(), layout.Bytes(), aligned, available) != nullptr, "test surface alignment failed");
    const auto address = reinterpret_cast<std::uintptr_t>(aligned);
    std::span<std::byte> guest(static_cast<std::byte*>(aligned), layout.Bytes());
    std::fill(guest.begin(), guest.end(), std::byte{0xa5});
    std::vector<std::byte> source(layout.LinearBytes());
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::byte>((i * 73u + i / 257u) & 255u);
    layout.Tile(source, guest);
    GpuColorTransfer transfer(context);
    transfer.Upload(address, width, height, mode);
    Buffer readback(context, source.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        transfer.Detile(commands, swap);
        const VkBufferCopy copy{0, 0, source.size()};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, transfer.LinearBuffer(), readback.Handle(), 1, &copy);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, transfer.LinearBuffer(), 0, source.size(), 0x12345678u);
        transfer.Tile(commands);
        batch.SubmitAndWait();
    }
    auto expectedLinear = source;
    if (swap) {
        for (std::size_t i = 0; i < expectedLinear.size(); i += 4) std::swap(expectedLinear[i], expectedLinear[i + 2]);
    }
    Require(std::equal(expectedLinear.begin(), expectedLinear.end(), readback.Bytes().begin()), "GPU detile differs from CPU reference");
    std::vector<std::byte> expectedTiled(layout.Bytes(), std::byte{0xa5});
    const std::uint32_t fill = 0x12345678u;
    for (std::size_t i = 0; i < source.size(); i += sizeof(fill)) std::memcpy(source.data() + i, &fill, sizeof(fill));
    layout.Tile(source, expectedTiled);
    transfer.WriteBack(address);
    Require(std::equal(expectedTiled.begin(), expectedTiled.end(), guest.begin()), "GPU tile changed pixels or surface padding");
}

std::size_t renderTargetOffset64(std::uint32_t blocksPerRow, std::uint32_t x, std::uint32_t y) {
    const auto* equation = FindTextureSwizzleEquation(27u, 8u);
    Require(equation != nullptr, "no 8-byte SW_64KB_R_X equation");
    std::size_t offset = 0;
    for (std::uint32_t bit = 0; bit < 16u; ++bit) {
        const auto mask = equation->bits[bit];
        offset |= static_cast<std::size_t>(std::popcount((x & mask & 0xfffu) ^ ((y << 12u) & mask & 0xfff000u)) & 1) << bit;
    }
    return (static_cast<std::size_t>(y / 64u) * blocksPerRow + x / 128u) * 65536u + offset;
}

std::uint32_t deviceLocalType(const Context& context, std::uint32_t bits) {
    for (std::uint32_t i = 0; i < context.memory.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) != 0 && (context.memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) return i;
    }
    Require(false, "no device-local memory type for the test image");
    return 0;
}

void checkHalfFloatDetile(const Context& context, std::uint32_t width, std::uint32_t height, bool redLow) {
    const ColorTargetLayout layout(width, height, ColorTileMode::RenderTarget, 8);
    std::vector<std::byte> storage(layout.Bytes() + layout.Alignment());
    void* aligned = storage.data();
    auto available = storage.size();
    Require(std::align(layout.Alignment(), layout.Bytes(), aligned, available) != nullptr, "test surface alignment failed");
    std::span<std::byte> guest(static_cast<std::byte*>(aligned), layout.Bytes());
    std::fill(guest.begin(), guest.end(), std::byte{0xa5});
    std::vector<std::byte> texels(static_cast<std::size_t>(width) * height * 8);
    std::vector<std::byte> expected(static_cast<std::size_t>(width) * height * 4);
    std::uint32_t seed = redLow ? 0x2545f491u : 0x9e3779b9u;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::array<std::uint16_t, 4> halves{};
            for (auto& half : halves) {
                seed = seed * 1664525u + 1013904223u;
                half = static_cast<std::uint16_t>(seed >> 16u);
            }
            const auto index = static_cast<std::size_t>(y) * width + x;
            std::memcpy(texels.data() + index * 8, halves.data(), sizeof(halves));
            std::memcpy(guest.data() + renderTargetOffset64(layout.BlocksPerRow(), x, y), halves.data(), sizeof(halves));
            auto* pixel = expected.data() + index * 4;
            pixel[0] = static_cast<std::byte>(HalfFloatScanoutCode(halves[redLow ? 2 : 0], false));
            pixel[1] = static_cast<std::byte>(HalfFloatScanoutCode(halves[1], false));
            pixel[2] = static_cast<std::byte>(HalfFloatScanoutCode(halves[redLow ? 0 : 2], false));
            pixel[3] = static_cast<std::byte>(HalfFloatScanoutCode(halves[3], true));
        }
    }
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage half-float detile test");
    struct Release {
        const Context& context;
        VkImage image;
        VkDeviceMemory& memory;
        ~Release() {
            context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
            if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
        }
    } release{context, image, memory};
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = deviceLocalType(context, requirements.memoryTypeBits);
    Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory half-float detile test");
    Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory half-float detile test");
    Buffer upload(context, texels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::memcpy(upload.Bytes().data(), texels.data(), texels.size());
    Buffer fromGuest(context, expected.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer fromImage(context, expected.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    GpuColorTransfer transfer(context);
    const auto barrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    const auto readback = [&](VkCommandBuffer commands, Buffer& destination) {
        const VkBufferCopy copy{0, 0, expected.size()};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, transfer.LinearBuffer(), destination.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
    };
    transfer.Upload(reinterpret_cast<std::uintptr_t>(aligned), width, height, ColorTileMode::RenderTarget, 8);
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
        transfer.Detile(commands, redLow, false, true);
        readback(commands, fromGuest);
        transfer.DetileImage(commands, image, VK_IMAGE_LAYOUT_GENERAL, width, height, ColorTileMode::RenderTarget, redLow, false, true);
        readback(commands, fromImage);
        batch.SubmitAndWait();
    }
    Require(std::equal(expected.begin(), expected.end(), fromGuest.Bytes().begin()), "GPU 16-bit float detile from guest memory differs from the reference");
    Require(std::equal(expected.begin(), expected.end(), fromImage.Bytes().begin()), "GPU 16-bit float detile from an image differs from the reference");
}

void checkBufferReuse(const Context& context) {
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceAddress address = 0;
    {
        Buffer original(context, 12348, usage);
        handle = original.Handle();
        address = original.DeviceAddress();
    }
    Buffer reused(context, 12348, usage);
    Require(reused.Handle() == handle && reused.DeviceAddress() == address, "buffer cache did not reuse a completed allocation");
    Buffer simultaneous(context, 12348, usage);
    Require(simultaneous.Handle() != reused.Handle(), "buffer cache reused an active allocation");
}

void checkDeviceBuffer(const Context& context) {
    constexpr std::size_t bytes = 4096;
    constexpr auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer handle = VK_NULL_HANDLE;
    {
        Buffer local(context, bytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        handle = local.Handle();
        bool rejected = false;
        try { static_cast<void>(local.Bytes()); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected, "GPU-only buffer exposed a CPU mapping");
        rejected = false;
        try { local.Invalidate(); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected, "GPU-only buffer accepted host invalidation");
        Buffer upload(context, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        std::fill(upload.Bytes().begin(), upload.Bytes().end(), std::byte{0x5a});
        CommandBatch batch(context);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        const auto sync = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        sync(batch.Handle(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        const VkBufferCopy copy{0, 0, bytes};
        const auto transfer = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
        transfer(batch.Handle(), upload.Handle(), local.Handle(), 1, &copy);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sync(batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        transfer(batch.Handle(), local.Handle(), readback.Handle(), 1, &copy);
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        sync(batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        batch.SubmitAndWait();
        Require(std::equal(upload.Bytes().begin(), upload.Bytes().end(), readback.Bytes().begin()), "device-local buffer transfer corrupted data");
    }
    Buffer reused(context, bytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Require(reused.Handle() == handle, "device-local buffer was not retained for reuse");
}

}

void RunColorTransferTests(const AgcDriver::Graphics::Context& context) {
    checkBufferReuse(context);
    checkDeviceBuffer(context);
    checkConversion(context, 130, 129, AgcDriver::Graphics::ColorTileMode::RenderTarget, false);
    checkConversion(context, 257, 17, AgcDriver::Graphics::ColorTileMode::RenderTarget, true);
    checkConversion(context, 192, 13, AgcDriver::Graphics::ColorTileMode::Linear, false);
    checkHalfFloatDetile(context, 300, 130, true);
    checkHalfFloatDetile(context, 129, 70, false);
}
