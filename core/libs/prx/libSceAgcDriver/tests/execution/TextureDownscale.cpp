#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint64_t Address = 0x10000000ull;
constexpr std::uint32_t FormatBc1Unorm = 169;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileS64KB = 0x09;
constexpr std::uint32_t Type2D = 9;

struct Shape {
    std::uint32_t format;
    std::uint32_t side;
    std::uint32_t maxMip;
    std::uint32_t lastLevel;
    std::uint32_t minLod = 0;
};

std::array<std::uint32_t, 8> Descriptor(const Shape& shape) {
    return {
        static_cast<std::uint32_t>(Address >> 8u),
        static_cast<std::uint32_t>((Address >> 40u) & 0xffu) | (shape.minLod << 8u) | (shape.format << 20u) | (((shape.side - 1u) & 3u) << 30u),
        ((shape.side - 1u) >> 2u) | ((shape.side - 1u) << 14u),
        0xfacu | (shape.lastLevel << 16u) | (TileS64KB << 20u) | (Type2D << 28u),
        0u,
        shape.maxMip << 4u,
        0u,
        0u,
    };
}

struct Made {
    std::uint32_t levels;
    VkDeviceSize bytes;
};

Made Make(const Context& context, TextureDetiler& detiler, const Shape& shape) {
    const auto words = Descriptor(shape);
    const auto descriptor = DecodeTextureResource(words);
    const std::vector<std::byte> snapshot(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
    Texture texture(context, detiler, descriptor, VkComponentMapping{}, snapshot);
    return {texture.SampledViewRange(false).levels, texture.AllocationBytes()};
}

void Run(const Context& context, TextureDetiler& detiler) {
    const auto dropped = Make(context, detiler, {FormatBc1Unorm, 1024, 10, 10});
    const auto half = Make(context, detiler, {FormatBc1Unorm, 512, 9, 9});
    Require(dropped.levels == 10, "a 1024x1024 BC1 texture with 11 levels kept " + std::to_string(dropped.levels) + " levels, expected 10");
    Require(half.levels == 10, "a 512x512 BC1 texture below the downscale minimum lost levels: " + std::to_string(half.levels));
    Require(dropped.bytes == half.bytes, "a downscaled 1024x1024 BC1 texture holds " + std::to_string(dropped.bytes) + " bytes, a 512x512 one " + std::to_string(half.bytes));

    Require(Make(context, detiler, {FormatBc1Unorm, 1024, 0, 0}).levels == 1, "a single-level BC1 texture lost its only level");
    Require(Make(context, detiler, {FormatBc1Unorm, 1024, 10, 0}).levels == 1, "a view of a BC1 texture's top level was downscaled");
    Require(Make(context, detiler, {Format8888Unorm, 1024, 10, 10}).levels == 11, "an uncompressed texture was downscaled");

    if (context.imageViewMinLod) {
        Require(Make(context, detiler, {FormatBc1Unorm, 1024, 10, 10, 10u * 256u}).levels == 10, "a BC1 texture with a minimum LOD clamp kept every level");
    }
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_TEXTURE_DOWNSCALE", "1");
#else
    setenv("APS5_TEXTURE_DOWNSCALE", "1", 1);
#endif
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto context = device->graphicsContext();
        if (!context.textureCompressionBC) {
            std::puts("skipped, the device has no BC texture compression");
            return VulkanTestSkipped;
        }
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            Run(context, detiler);
            if (auto* recorder = Recorder::Active()) recorder->Sync();
        }
        std::puts("texture downscale tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
