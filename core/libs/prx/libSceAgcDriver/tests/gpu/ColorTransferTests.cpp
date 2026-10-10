#include "ColorTransferTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include <algorithm>
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
}
