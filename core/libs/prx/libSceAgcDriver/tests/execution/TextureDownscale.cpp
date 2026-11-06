#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t maxMip;
    std::uint32_t lastLevel;
    std::uint32_t minLod = 0;
};

struct Expected {
    std::uint32_t levels;
    std::uint32_t width;
    std::uint32_t height;
};

std::array<std::uint32_t, 8> Descriptor(const Shape& shape) {
    return {
        static_cast<std::uint32_t>(Address >> 8u),
        static_cast<std::uint32_t>((Address >> 40u) & 0xffu) | (shape.minLod << 8u) | (shape.format << 20u) | (((shape.width - 1u) & 3u) << 30u),
        ((shape.width - 1u) >> 2u) | ((shape.height - 1u) << 14u),
        0xfacu | (shape.lastLevel << 16u) | (TileS64KB << 20u) | (Type2D << 28u),
        0u,
        shape.maxMip << 4u,
        0u,
        0u,
    };
}

struct Made {
    std::uint32_t levels;
    VkExtent3D extent;
    VkDeviceSize bytes;
    float minLod;
};

Made Make(const Context& context, TextureDetiler& detiler, const Shape& shape) {
    const auto words = Descriptor(shape);
    const auto descriptor = DecodeTextureResource(words);
    const std::vector<std::byte> snapshot(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
    Texture texture(context, detiler, descriptor, VkComponentMapping{}, snapshot);
    return {texture.SampledViewRange(false).levels, texture.ImageExtent(), texture.AllocationBytes(), texture.ViewMinLod()};
}

std::string Text(const Shape& shape) {
    return std::to_string(shape.width) + "x" + std::to_string(shape.height) + " format " + std::to_string(shape.format) + " levels 0.." + std::to_string(shape.lastLevel) + " of " + std::to_string(shape.maxMip + 1u);
}

Made Check(const Context& context, TextureDetiler& detiler, std::uint32_t n, const Shape& shape, const Expected& expected) {
    const auto made = Make(context, detiler, shape);
    const auto what = "APS5_TEXTURE_DOWNSCALE=" + std::to_string(n) + ", " + Text(shape) + ": ";
    Require(made.levels == expected.levels, what + "the view has " + std::to_string(made.levels) + " levels, expected " + std::to_string(expected.levels));
    Require(made.extent.width == expected.width && made.extent.height == expected.height && made.extent.depth == 1u, what + "the image is " + std::to_string(made.extent.width) + "x" + std::to_string(made.extent.height) + "x" + std::to_string(made.extent.depth) + ", expected " + std::to_string(expected.width) + "x" + std::to_string(expected.height) + "x1");
    return made;
}

void Run(const Context& context, TextureDetiler& detiler, std::uint32_t n) {
    const auto side = 1024u >> n;
    const auto dropped = Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 10, 10}, {11u - n, side, side});
    const auto reference = Check(context, detiler, n, {FormatBc1Unorm, side, side, 10u - n, 10u - n}, {11u - n, side, side});
    Require(dropped.bytes == reference.bytes, "APS5_TEXTURE_DOWNSCALE=" + std::to_string(n) + ": a downscaled 1024x1024 BC1 texture holds " + std::to_string(dropped.bytes) + " bytes, a " + std::to_string(side) + "x" + std::to_string(side) + " one below the minimum " + std::to_string(reference.bytes));

    Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 10, 1}, {1, 512, 512});
    Check(context, detiler, n, {FormatBc1Unorm, 1024, 8, 10, 10}, n == 1u ? Expected{10, 512, 4} : Expected{11, 1024, 8});

    Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 0, 0}, {1, 1024, 1024});
    Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 10, 0}, {1, 1024, 1024});
    Check(context, detiler, n, {Format8888Unorm, 1024, 1024, 10, 10}, {11, 1024, 1024});

    if (context.imageViewMinLod) {
        const auto clamped = Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 10, 10, 10u * 256u}, {11u - n, side, side});
        Require(clamped.minLod == static_cast<float>(10u - n), "APS5_TEXTURE_DOWNSCALE=" + std::to_string(n) + ": a minimum LOD clamp of 10.0 on 11 levels became " + std::to_string(clamped.minLod) + " on the view, expected " + std::to_string(10u - n));
    }
}

void Refused(const Context& context, TextureDetiler& detiler, std::string_view name) {
    try {
        Make(context, detiler, {FormatBc1Unorm, 1024, 1024, 10, 10});
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(name) != std::string_view::npos, std::string("the refusal does not name ") + std::string(name) + ": " + error.what());
        return;
    }
    throw std::runtime_error(std::string("an invalid ") + std::string(name) + " made a texture");
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    std::uint32_t n = 1;
    if (mode == "--levels" && argc > 2) n = static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10));
    if (mode == "--refuse-levels") {
        SetEnvironment("APS5_TEXTURE_DOWNSCALE", "-1");
    } else if (mode == "--refuse-minimum") {
        SetEnvironment("APS5_TEXTURE_DOWNSCALE", "1");
        SetEnvironment("APS5_TEXTURE_DOWNSCALE_MIN", "large");
    } else {
        SetEnvironment("APS5_TEXTURE_DOWNSCALE", std::to_string(n).c_str());
    }
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
            if (mode == "--refuse-levels") {
                Refused(context, detiler, "APS5_TEXTURE_DOWNSCALE");
            } else if (mode == "--refuse-minimum") {
                Refused(context, detiler, "APS5_TEXTURE_DOWNSCALE_MIN");
            } else {
                Run(context, detiler, n);
            }
            if (auto* recorder = Recorder::Active()) recorder->Sync();
        }
        std::printf("texture downscale tests passed (%s)\n", mode.empty() ? "--levels 1" : (mode + (mode == "--levels" ? " " + std::to_string(n) : "")).c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
