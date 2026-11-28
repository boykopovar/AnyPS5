#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

namespace Graphics {

struct TextureDetilerTestAccess {
    static bool WordGroupModules(const TextureDetiler& detiler) { return detiler.wordModule != VK_NULL_HANDLE || detiler.imageWordModule != VK_NULL_HANDLE; }
};

}

}

namespace {

using namespace AgcDriver::Graphics;

std::uint32_t parity(std::uint32_t mask, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint32_t>(std::popcount((x & mask & 0xfffu) ^ ((y << 12u) & mask & 0xfff000u))) & 1u;
}

std::uint8_t elementByte(std::uint32_t x, std::uint32_t y, std::uint32_t byte) {
    return static_cast<std::uint8_t>(x * 7u + y * 13u + byte * 101u + (x >> 4u) * 3u + (y >> 3u) * 5u);
}

void submit(const Context& context, CommandBatch& batch) {
    const VkMemoryBarrier toHost{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &toHost, 0, nullptr, 0, nullptr);
    batch.SubmitAndWait();
}

void detile(const Context& context, TextureDetiler& detiler, TextureTileMode mode, std::uint32_t bytes) {
    constexpr std::array<std::uint32_t, 2> formats{1u, 7u};
    const auto geometry = GcnTiling(mode, bytes);
    const auto width = geometry.blockWidth * 2u;
    const auto height = geometry.blockHeight * 2u;
    const auto mip = ComputeGcnMipLayout(mode, formats[std::countr_zero(bytes)], width, height, width, false, 1u).front();
    const auto what = "PS4 tile mode " + std::to_string(static_cast<int>(mode)) + " at " + std::to_string(bytes) + " bytes";
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
            const auto offset = block * geometry.blockBytes + inBlock + static_cast<std::uint64_t>(split) * mip.gcnSplitStride;
            Require(offset + bytes <= tiled.Bytes().size(), what + ": an element lies outside the tiled surface");
            for (std::uint32_t byte = 0; byte < bytes; ++byte) tiled.Bytes()[offset + byte] = static_cast<std::byte>(elementByte(x, y, byte));
        }
    }
    CommandBatch batch(context);
    detiler.BeginBatch();
    detiler.Dispatch(batch.Handle(), mode, bytes, tiled.Handle(), 0, linear.Handle(), 0, mip);
    submit(context, batch);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            for (std::uint32_t byte = 0; byte < bytes; ++byte) {
                const auto got = static_cast<std::uint8_t>(linear.Bytes()[static_cast<std::size_t>(y) * mip.pitchBytes + static_cast<std::size_t>(x) * bytes + byte]);
                if (got != elementByte(x, y, byte)) Require(false, what + ": element " + std::to_string(x) + "," + std::to_string(y) + " byte " + std::to_string(byte) + " detiled as " + std::to_string(got) + ", expected " + std::to_string(elementByte(x, y, byte)));
            }
        }
    }
}

void retile(const Context& context, TextureDetiler& detiler, std::uint32_t bytes) {
    constexpr auto mode = TextureTileMode::kStandard4KB;
    const auto block = ThinBlockLayout(mode, bytes);
    const auto width = block[1] * 2u;
    const auto height = block[2] * 2u;
    const auto mip = ComputeElementMipLayout(mode, bytes, width, height, 1u).front();
    const auto what = "SW_4KB_S at " + std::to_string(bytes) + " bytes";
    Buffer linear(context, static_cast<std::size_t>(mip.linearSize), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer tiled(context, static_cast<std::size_t>(mip.tiledSize), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer back(context, static_cast<std::size_t>(mip.linearSize), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(linear.Bytes().data(), 0, linear.Bytes().size());
    std::memset(tiled.Bytes().data(), 0, tiled.Bytes().size());
    std::memset(back.Bytes().data(), 0xcd, back.Bytes().size());
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            for (std::uint32_t byte = 0; byte < bytes; ++byte) linear.Bytes()[static_cast<std::size_t>(y) * mip.pitchBytes + static_cast<std::size_t>(x) * bytes + byte] = static_cast<std::byte>(elementByte(x, y, byte));
        }
    }
    {
        CommandBatch batch(context);
        detiler.BeginBatch();
        detiler.Dispatch(batch.Handle(), mode, bytes, linear.Handle(), 0, tiled.Handle(), 0, mip, true);
        submit(context, batch);
    }
    {
        CommandBatch batch(context);
        detiler.BeginBatch();
        detiler.Dispatch(batch.Handle(), mode, bytes, tiled.Handle(), 0, back.Handle(), 0, mip);
        submit(context, batch);
    }
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            for (std::uint32_t byte = 0; byte < bytes; ++byte) {
                const auto got = static_cast<std::uint8_t>(back.Bytes()[static_cast<std::size_t>(y) * mip.pitchBytes + static_cast<std::size_t>(x) * bytes + byte]);
                if (got != elementByte(x, y, byte)) Require(false, what + ": element " + std::to_string(x) + "," + std::to_string(y) + " byte " + std::to_string(byte) + " came back as " + std::to_string(got) + ", expected " + std::to_string(elementByte(x, y, byte)));
            }
        }
    }
}

void moveSmallElements(const Context& context, bool wordGroups) {
    TextureDetiler detiler(context);
    using enum TextureTileMode;
    for (const auto bytes : {1u, 2u}) {
        for (const auto mode : {kGcnThin1D, kGcnDisplay2D, kGcnThin2D, kGcnDepth2D64}) detile(context, detiler, mode, bytes);
        retile(context, detiler, bytes);
    }
    Require(AgcDriver::Graphics::TextureDetilerTestAccess::WordGroupModules(detiler) == wordGroups, wordGroups ? "a device that passed the word-group probe did not use the word-group shader for 1- and 2-byte elements" : "a device that failed the word-group probe created a word-group shader");
}

void selection() {
    for (const auto bytes : {1u, 2u, 4u, 8u, 16u}) {
        for (const bool equation : {false, true}) {
            for (const bool retile : {false, true}) {
                for (const bool image : {false, true}) {
                    const bool expected = bytes < 4u && (image ? retile && !equation : !retile || !equation);
                    const auto what = std::to_string(bytes) + " bytes, equation " + std::to_string(equation) + ", retile " + std::to_string(retile) + ", image " + std::to_string(image);
                    Require(TextureDetiler::UsesWordGroups(false, bytes, equation, retile, image) == expected, "passed probe: wrong shader choice for " + what);
                    Require(!TextureDetiler::UsesWordGroups(true, bytes, equation, retile, image), "failed probe: the word-group shader was chosen for " + what);
                }
            }
        }
    }
}

}

int main() {
    try {
        selection();
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        {
            auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
            const auto probe = TextureDetiler::ProbeWordGroups(context);
            Require(probe.compared == 256u * 256u * 3u && probe.mismatches <= probe.compared, "the word-group probe did not compare 256x256 elements of 1 and 2 bytes");
            Require(context.detileWordGroupsFaulty || probe.mismatches == 0u, "the word-group shader passed the device's probe and moved " + std::to_string(probe.mismatches) + " of " + std::to_string(probe.compared) + " bytes wrong in a second run");
            std::printf("word-group probe: %u of %u bytes wrong, device verdict %s\n", probe.mismatches, probe.compared, context.detileWordGroupsFaulty ? "faulty" : "reliable");
            moveSmallElements(context, !context.detileWordGroupsFaulty);
            context.detileWordGroupsFaulty = true;
            moveSmallElements(context, false);
            device->WaitIdle();
        }
        device.reset();
        std::puts("detile word-group tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
