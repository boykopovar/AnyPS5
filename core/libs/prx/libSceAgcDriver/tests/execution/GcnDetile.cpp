#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

std::uint32_t parity(std::uint32_t mask, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint32_t>(std::popcount((x & mask & 0xfffu) ^ ((y << 12u) & mask & 0xfff000u))) & 1u;
}

std::uint8_t elementByte(std::uint32_t x, std::uint32_t y, std::uint32_t byte) {
    return static_cast<std::uint8_t>(x * 7u + y * 13u + byte * 101u + (x >> 4u) * 3u + (y >> 3u) * 5u);
}

void detile(const Context& context, TextureDetiler& detiler, TextureTileMode mode, std::uint32_t bytes, std::uint32_t layer) {
    constexpr std::array<std::uint32_t, 5> formats{1u, 7u, 20u, 62u, 75u};
    const auto geometry = GcnTiling(mode, bytes);
    const auto width = geometry.blockWidth * 2u;
    const auto height = geometry.blockHeight * 2u;
    const auto mip = ComputeGcnMipLayout(mode, formats[std::countr_zero(bytes)], width, height, width, false, 1u).front();
    const auto what = "PS4 tile mode " + std::to_string(static_cast<int>(mode)) + " at " + std::to_string(bytes) + " bytes, array layer " + std::to_string(layer);
    const auto banks = geometry.blockWidth == 64u ? 8u : 16u;
    const auto layerBankXor = IsGcnMacroTiled(mode) && !IsGcnPrtTiled(mode) ? static_cast<std::uint64_t>(((banks / 2u - 1u) * layer) & (banks - 1u)) << 11u : 0u;
    Buffer tiled(context, static_cast<std::size_t>(mip.tiledSize), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer linear(context, static_cast<std::size_t>(mip.linearSize), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(tiled.Bytes().data(), 0, tiled.Bytes().size());
    std::memset(linear.Bytes().data(), 0xcd, linear.Bytes().size());
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint32_t inBlock = 0, split = 0;
            for (std::uint32_t bit = 0; bit < 16u; ++bit) inBlock |= parity(geometry.equation[bit], x, y) << bit;
            for (std::uint32_t bit = 0; bit < 4u; ++bit) split |= parity(geometry.splitMasks[bit], x, y) << bit;
            const auto block = static_cast<std::uint64_t>(y / geometry.blockHeight) * mip.blocksPerRow + x / geometry.blockWidth;
            const auto offset = block * geometry.blockBytes + (inBlock ^ layerBankXor) + static_cast<std::uint64_t>(split) * mip.gcnSplitStride;
            Require(offset + bytes <= tiled.Bytes().size(), what + ": an element lies outside the tiled surface");
            for (std::uint32_t byte = 0; byte < bytes; ++byte) tiled.Bytes()[offset + byte] = static_cast<std::byte>(elementByte(x, y, byte));
        }
    }
    CommandBatch batch(context);
    detiler.BeginBatch();
    detiler.Dispatch(batch.Handle(), mode, bytes, tiled.Handle(), 0, linear.Handle(), 0, mip, false, layer);
    const VkMemoryBarrier toHost{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &toHost, 0, nullptr, 0, nullptr);
    batch.SubmitAndWait();
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            for (std::uint32_t byte = 0; byte < bytes; ++byte) {
                const auto got = static_cast<std::uint8_t>(linear.Bytes()[static_cast<std::size_t>(y) * mip.pitchBytes + static_cast<std::size_t>(x) * bytes + byte]);
                if (got != elementByte(x, y, byte)) Require(false, what + ": element " + std::to_string(x) + "," + std::to_string(y) + " byte " + std::to_string(byte) + " detiled as " + std::to_string(got) + ", expected " + std::to_string(elementByte(x, y, byte)));
            }
        }
    }
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        {
            const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
            TextureDetiler detiler(context);
            using enum TextureTileMode;
            for (const auto mode : {kGcnThin1D, kGcnDisplay1D, kGcnDisplay2D, kGcnThin2D, kGcnDepth2D64, kGcnDepth2D256, kGcnDepth2D1K, kGcnDisplayPrt, kGcnThin2DPrt, kGcnDepthPrt256}) {
                for (std::uint32_t bytes = 1u; bytes <= 16u; bytes <<= 1u) {
                    for (const auto layer : {0u, 1u, 2u, 3u, 5u}) detile(context, detiler, mode, bytes, layer);
                }
            }
        }
        device.reset();
        std::puts("PS4 detile tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
