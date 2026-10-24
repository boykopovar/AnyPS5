#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "execution/FixtureDeviceSelection.hpp"
#include <SDL_loadso.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::DisplayBuffer;
using AgcDriver::PresentationPass;
using AgcDriver::PresentationScaler;

constexpr int VulkanTestSkipped = 77;
constexpr std::uint64_t Bgra8 = 0x8000000000000000ull;
constexpr std::uint64_t Rgba8 = 0x8000000022000000ull;
constexpr std::uint64_t TenBit = 0x0100000000000000ull;
constexpr std::uint64_t Rgba16Float = 0xc001000622000000ull;
constexpr std::uint64_t Rgb10Pq = 0x8100070422000000ull;
constexpr std::uint64_t Bgr10Pq = 0x8100070400000000ull;

class Device {
public:
    Device() {
#ifdef _WIN32
        library = SDL_LoadObject("vulkan-1.dll");
#else
        library = SDL_LoadObject("libvulkan.so.1");
#endif
        Require(library != nullptr, "cannot load Vulkan");
        try {
            instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
            Require(instanceProc != nullptr, "missing Vulkan instance resolver");
            VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            application.apiVersion = VK_API_VERSION_1_1;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            info.pApplicationInfo = &application;
            Check(function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
            std::uint32_t count = 0;
            const auto enumerate = function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
            Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            Require(count != 0, "no Vulkan device");
            std::vector<VkPhysicalDevice> devices(count);
            Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            context.physical = SelectFixturePhysicalDevice(devices, function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties"));
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) ++family;
            Require(family < count, "no Vulkan graphics queue");
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.imageFormatProperties = function<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties");
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool.queueFamilyIndex = family;
            Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.pool), "vkCreateCommandPool");
        } catch (...) {
            release();
            throw;
        }
    }

    ~Device() { release(); }
    const Context& GetContext() const { return context; }

private:
    template<typename TFunction>
    TFunction function(const char* name) const {
        const auto result = reinterpret_cast<TFunction>(instanceProc(instance, name));
        Require(result != nullptr, name);
        return result;
    }

    void release() noexcept {
        if (context.pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(context.device, context.pool, nullptr);
        context.bufferPool.reset();
        if (context.device != VK_NULL_HANDLE) function<PFN_vkDestroyDevice>("vkDestroyDevice")(context.device, nullptr);
        if (instance != VK_NULL_HANDLE) function<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
        if (library != nullptr) SDL_UnloadObject(library);
    }

    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    Context context{};
};

class Image {
public:
    Image(const Context& context, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImageCreateFlags flags = 0, std::uint32_t layers = 1) : context(context), extent(extent) {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.flags = flags;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {extent.width, extent.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = layers;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage resident present test");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        const auto allocated = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory);
        if (allocated != VK_SUCCESS) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        Check(allocated, "vkAllocateMemory resident present test");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory resident present test");
    }
    ~Image() {
        context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
    }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    VkImage Handle() const { return image; }
    VkExtent2D Extent() const { return extent; }

private:
    const Context& context;
    VkExtent2D extent;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

void transition(const Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess, std::uint32_t layers = 1) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void readImage(const Context& context, VkCommandBuffer commands, const Image& image, Buffer& destination) {
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {image.Extent().width, image.Extent().height, 1};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image.Handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination.Handle(), 1, &copy);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
}

std::vector<std::byte> bytesOf(Buffer& buffer, std::size_t size) {
    const auto bytes = buffer.Bytes();
    return {bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)};
}

void letterboxBandTests() {
    const std::array<std::array<std::uint32_t, 4>, 7> cases{{{3840, 2160, 2261, 1272}, {3840, 2160, 3840, 2160}, {3840, 2160, 1000, 1000}, {3840, 2160, 2000, 500}, {200, 130, 260, 130}, {200, 130, 117, 76}, {7, 3, 5, 11}}};
    for (const auto& [width, height, destinationWidth, destinationHeight] : cases) {
        const auto rect = AgcDriver::ComputeContainRect_nid_postfix(width, height, destinationWidth, destinationHeight);
        const auto bands = PresentationScaler::LetterboxBands(width, height, destinationWidth, destinationHeight);
        std::vector<std::uint8_t> covered(static_cast<std::size_t>(destinationWidth) * destinationHeight, 0);
        auto cover = [&](const AgcDriver::AspectFitRect& area) {
            Require(area.width != 0 && area.height != 0, "an empty letterbox band");
            for (std::uint32_t y = 0; y < area.height; ++y) {
                for (std::uint32_t x = 0; x < area.width; ++x) ++covered[static_cast<std::size_t>(area.y + y) * destinationWidth + area.x + x];
            }
        };
        cover(rect);
        for (const auto& band : bands) cover(band);
        Require(std::all_of(covered.begin(), covered.end(), [](std::uint8_t count) { return count == 1; }), "the letterbox bands and the image do not tile the destination: " + std::to_string(width) + "x" + std::to_string(height) + " into " + std::to_string(destinationWidth) + "x" + std::to_string(destinationHeight));
    }
    Require(PresentationScaler::LetterboxBands(3840, 2160, 1920, 1080).empty(), "an exact fit has letterbox bands");
}

struct Source {
    std::uint64_t pixelFormat;
    VkFormat storage;
    std::uint32_t width;
    std::uint32_t height;
    std::vector<std::byte> words;
    std::vector<std::byte> decoded;
};

Source makeSource(std::uint64_t pixelFormat, VkFormat storage, std::uint32_t width, std::uint32_t height, std::uint32_t salt = 0) {
    Source source{pixelFormat, storage, width, height, std::vector<std::byte>(static_cast<std::size_t>(width) * height * 4), {}};
    std::uint32_t seed = 0x9e3779b9u ^ static_cast<std::uint32_t>(storage) ^ static_cast<std::uint32_t>(pixelFormat >> 32u) ^ salt;
    for (auto& byte : source.words) {
        seed = seed * 1664525u + 1013904223u;
        byte = static_cast<std::byte>(seed >> 24u);
    }
    const ColorTargetLayout layout(width, height, ColorTileMode::RenderTarget);
    std::vector<std::byte> storageBytes(layout.Bytes() + 65536);
    auto* aligned = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(storageBytes.data()) + 65535u) & ~std::uintptr_t{65535});
    std::span<std::byte> guest(aligned, layout.Bytes());
    layout.Tile(source.words, guest);
    const DisplayBuffer display{reinterpret_cast<std::uint64_t>(aligned), pixelFormat, width, height};
    Require(AgcDriver::DisplayBufferSize(display) == guest.size(), "test display buffer size differs from the color layout");
    source.decoded = AgcDriver::DecodeDisplayBuffer(display, guest);
    return source;
}

std::unique_ptr<Image> residentImage(const Context& context, const std::vector<const Source*>& slices) {
    const auto& source = *slices.front();
    const auto layers = static_cast<std::uint32_t>(slices.size());
    auto image = std::make_unique<Image>(context, source.storage, VkExtent2D{source.width, source.height}, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT, layers);
    Buffer upload(context, source.words.size() * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::vector<VkBufferImageCopy> regions(layers);
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        std::memcpy(upload.Bytes().data() + source.words.size() * layer, slices[layer]->words.data(), source.words.size());
        regions[layer].bufferOffset = source.words.size() * layer;
        regions[layer].imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
        regions[layer].imageExtent = {source.width, source.height, 1};
    }
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    transition(context, commands, image->Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT, layers);
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, upload.Handle(), image->Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layers, regions.data());
    transition(context, commands, image->Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT, layers);
    batch.SubmitAndWait();
    return image;
}

std::unique_ptr<Image> residentImage(const Context& context, const Source& source) { return residentImage(context, std::vector<const Source*>{&source}); }

std::vector<std::byte> convertPathPresent(const Context& context, const Source& source, VkExtent2D extent) {
    Buffer upload(context, source.decoded.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::memcpy(upload.Bytes().data(), source.decoded.data(), source.decoded.size());
    PresentationScaler scaler(context, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
    Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, extent, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto bytes = static_cast<std::size_t>(extent.width) * extent.height * 4;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    scaler.EnsureSourceImage(source.width, source.height);
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    scaler.RecordUpload(commands, upload.Handle());
    transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkClearColorValue black{};
    black.float32[3] = 1.0f;
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
    transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    scaler.RecordBlit(commands, destination.Handle(), extent.width, extent.height);
    transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    readImage(context, commands, destination, readback);
    batch.SubmitAndWait();
    return bytesOf(readback, bytes);
}

std::vector<std::byte> passPresent(const Context& context, PresentationPass& pass, const Source& source, const Image& resident, VkExtent2D extent, VkFormat format, std::uint32_t layer = 0, float whiteNits = AgcDriver::SdrToneMapLuminanceNits, VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
    Image destination(context, format, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto bytes = static_cast<std::size_t>(extent.width) * extent.height * 4;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    pass.Bind(0, resident.Handle(), source.width, source.height, source.pixelFormat, whiteNits, layer);
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        pass.RecordPresent(commands, 0, destination.Handle(), format, colorSpace, extent, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        readImage(context, commands, destination, readback);
        batch.SubmitAndWait();
    }
    pass.Release(0);
    return bytesOf(readback, bytes);
}

std::vector<std::byte> passDump(const Context& context, PresentationPass& pass, const Source& source, const Image& resident, std::uint32_t stride, std::uint32_t layer = 0, float whiteNits = AgcDriver::SdrToneMapLuminanceNits) {
    const auto extent = PresentationPass::DumpExtent(source.width, source.height, stride);
    const auto bytes = static_cast<std::size_t>(extent.width) * extent.height * 4;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    pass.Bind(1, resident.Handle(), source.width, source.height, source.pixelFormat, whiteNits, layer);
    {
        CommandBatch batch(context);
        pass.RecordDump(batch.Handle(), 1, stride, readback.Handle());
        batch.SubmitAndWait();
    }
    pass.Release(1);
    return bytesOf(readback, bytes);
}

std::string describe(const Source& source, VkExtent2D extent) {
    return std::string(AgcDriver::DisplayPq(source.pixelFormat) ? "PQ " : AgcDriver::DisplayTenBit(source.pixelFormat) ? "10-bit " : "8-bit ") + (AgcDriver::DisplayRedLow(source.pixelFormat) ? "red-low" : "blue-low") + " display in VkFormat " + std::to_string(static_cast<int>(source.storage)) + " at " + std::to_string(extent.width) + "x" + std::to_string(extent.height);
}

void passMatchesConvertPath(const Context& context, PresentationPass& pass, const Source& source, const Image& resident) {
    const std::array<VkExtent2D, 3> exact{{{source.width, source.height}, {source.width + 60, source.height}, {source.width, source.height + 51}}};
    for (const auto extent : exact) {
        const auto expected = convertPathPresent(context, source, extent);
        const auto drawn = passPresent(context, pass, source, resident, extent, VK_FORMAT_B8G8R8A8_UNORM);
        Require(expected == drawn, "the presentation pass differs from the convert path: " + describe(source, extent));
    }
    const std::array<VkExtent2D, 3> scaled{{{117, 76}, {331, 215}, {150, 150}}};
    for (const auto extent : scaled) {
        const auto expected = convertPathPresent(context, source, extent);
        const auto drawn = passPresent(context, pass, source, resident, extent, VK_FORMAT_B8G8R8A8_UNORM);
        int worst = 0;
        std::size_t differing = 0;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const int difference = std::abs(static_cast<int>(expected[i]) - static_cast<int>(drawn[i]));
            worst = std::max(worst, difference);
            differing += difference != 0;
        }
        std::printf("  %s: scaled, %zu of %zu channels differ, by at most %d\n", describe(source, extent).c_str(), differing, expected.size(), worst);
        Require(worst <= 2, "the scaled presentation pass differs from the convert path's filtered blit by more than two levels: " + describe(source, extent));
    }
}

void passDumpMatchesSubsample(const Context& context, PresentationPass& pass, const Source& source, const Image& resident, std::uint32_t layer = 0) {
    for (const std::uint32_t stride : {1u, 3u, 4u}) {
        const auto extent = PresentationPass::DumpExtent(source.width, source.height, stride);
        std::vector<std::byte> expected(static_cast<std::size_t>(extent.width) * extent.height * 4);
        for (std::uint32_t y = 0; y < extent.height; ++y) {
            for (std::uint32_t x = 0; x < extent.width; ++x) std::memcpy(expected.data() + (static_cast<std::size_t>(y) * extent.width + x) * 4, source.decoded.data() + (static_cast<std::size_t>(y) * stride * source.width + x * stride) * 4, 4);
        }
        Require(passDump(context, pass, source, resident, stride, layer) == expected, "the presentation dump differs from the subsampled guest-path frame: " + describe(source, extent) + " stride " + std::to_string(stride));
    }
}

bool renderable(const Context& context, VkFormat format) {
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    constexpr VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    return (properties.optimalTilingFeatures & needed) == needed;
}

void passPassesPqThrough(const Context& context, PresentationPass& pass, const Source& source, const Image& resident) {
    const bool redLowSource = AgcDriver::DisplayRedLow(source.pixelFormat);
    for (const auto format : {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32}) {
        if (!renderable(context, format)) {
            std::printf("  10-bit destination VkFormat %d is not renderable here; skipped\n", static_cast<int>(format));
            continue;
        }
        const bool redLowDestination = format == VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        const VkExtent2D extent{source.width + 40, source.height};
        const auto rect = AgcDriver::ComputeContainRect_nid_postfix(source.width, source.height, extent.width, extent.height);
        const auto drawn = passPresent(context, pass, source, resident, extent, format, 0, 1000.0f, VK_COLOR_SPACE_HDR10_ST2084_EXT);
        for (std::uint32_t y = 0; y < extent.height; ++y) {
            for (std::uint32_t x = 0; x < extent.width; ++x) {
                std::uint32_t word = 0;
                std::memcpy(&word, drawn.data() + (static_cast<std::size_t>(y) * extent.width + x) * 4, 4);
                if (static_cast<std::int32_t>(x) < rect.x || static_cast<std::int32_t>(x) >= rect.x + static_cast<std::int32_t>(rect.width)) {
                    Require(word == 0xc0000000u, "an HDR10 letterbox band is not opaque PQ black: " + describe(source, extent));
                    continue;
                }
                std::uint32_t sourceWord = 0;
                std::memcpy(&sourceWord, source.words.data() + (static_cast<std::size_t>(y) * source.width + (x - static_cast<std::uint32_t>(rect.x))) * 4, 4);
                const auto red = [](std::uint32_t value, bool redLow) { return redLow ? value & 0x3ffu : (value >> 20u) & 0x3ffu; };
                const auto blue = [](std::uint32_t value, bool redLow) { return redLow ? (value >> 20u) & 0x3ffu : value & 0x3ffu; };
                Require(red(word, redLowDestination) == red(sourceWord, redLowSource) && ((word >> 10u) & 0x3ffu) == ((sourceWord >> 10u) & 0x3ffu) && blue(word, redLowDestination) == blue(sourceWord, redLowSource) && (word >> 30u) == 3u, "the HDR10 presentation changed the PQ code values: " + describe(source, extent) + " into VkFormat " + std::to_string(static_cast<int>(format)));
            }
        }
    }
}

void passRefusesSdrToHdr10(const Context& context, PresentationPass& pass) {
    const auto source = makeSource(Bgra8 | TenBit, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 200, 130);
    const auto resident = residentImage(context, source);
    bool refused = false;
    try {
        static_cast<void>(passPresent(context, pass, source, *resident, {200, 130}, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 0, AgcDriver::SdrToneMapLuminanceNits, VK_COLOR_SPACE_HDR10_ST2084_EXT));
    } catch (const std::exception&) {
        refused = true;
    }
    pass.Release(0);
    Require(refused, "the presentation pass presented an SDR display buffer to an HDR10 destination");
}

void passKeepsTenBits(const Context& context, PresentationPass& pass, const Source& source, const Image& resident) {
    const bool redLowSource = AgcDriver::DisplayRedLow(source.pixelFormat);
    for (const auto format : {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32}) {
        if (!renderable(context, format)) {
            std::printf("  10-bit destination VkFormat %d is not renderable here; skipped\n", static_cast<int>(format));
            continue;
        }
        const bool redLowDestination = format == VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        const VkExtent2D extent{source.width + 40, source.height};
        const auto rect = AgcDriver::ComputeContainRect_nid_postfix(source.width, source.height, extent.width, extent.height);
        const auto converted = convertPathPresent(context, source, extent);
        const auto drawn = passPresent(context, pass, source, resident, extent, format);
        for (std::uint32_t y = 0; y < extent.height; ++y) {
            for (std::uint32_t x = 0; x < extent.width; ++x) {
                const auto index = static_cast<std::size_t>(y) * extent.width + x;
                std::uint32_t word = 0;
                std::memcpy(&word, drawn.data() + index * 4, 4);
                const std::uint32_t low = word & 0x3ffu, green = (word >> 10u) & 0x3ffu, high = (word >> 20u) & 0x3ffu;
                const std::uint32_t red = redLowDestination ? low : high, blue = redLowDestination ? high : low;
                const bool inside = static_cast<std::int32_t>(x) >= rect.x && static_cast<std::int32_t>(x) < rect.x + static_cast<std::int32_t>(rect.width);
                if (!inside) {
                    Require(word == 0xc0000000u, "a 10-bit letterbox band is not opaque black: " + describe(source, extent));
                    continue;
                }
                std::uint32_t sourceWord = 0;
                std::memcpy(&sourceWord, source.words.data() + (static_cast<std::size_t>(y) * source.width + (x - static_cast<std::uint32_t>(rect.x))) * 4, 4);
                const std::uint32_t sourceLow = sourceWord & 0x3ffu, sourceGreen = (sourceWord >> 10u) & 0x3ffu, sourceHigh = (sourceWord >> 20u) & 0x3ffu;
                const std::uint32_t sourceRed = redLowSource ? sourceLow : sourceHigh, sourceBlue = redLowSource ? sourceHigh : sourceLow;
                Require(red == sourceRed && green == sourceGreen && blue == sourceBlue && (word >> 30u) == 3u, "the 10-bit presentation lost bits: " + describe(source, extent) + " into VkFormat " + std::to_string(static_cast<int>(format)));
                const auto* eight = converted.data() + index * 4;
                Require(static_cast<std::uint32_t>(eight[0]) == (blue >> 2u) && static_cast<std::uint32_t>(eight[1]) == (green >> 2u) && static_cast<std::uint32_t>(eight[2]) == (red >> 2u), "the 10-bit presentation's top bits differ from the convert path: " + describe(source, extent));
            }
        }
    }
}

void blitLetterboxMatchesClear(const Context& context) {
    const auto source = makeSource(Bgra8, VK_FORMAT_B8G8R8A8_UNORM, 200, 130);
    const auto resident = residentImage(context, source);
    PresentationScaler scaler(context, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
    for (const auto extent : {VkExtent2D{200, 130}, VkExtent2D{260, 130}, VkExtent2D{200, 181}, VkExtent2D{117, 76}, VkExtent2D{331, 300}}) {
        const auto bytes = static_cast<std::size_t>(extent.width) * extent.height * 4;
        std::array<std::vector<std::byte>, 2> results;
        for (std::size_t banded = 0; banded < 2; ++banded) {
            Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, extent, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            CommandBatch batch(context);
            const auto commands = batch.Handle();
            transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
            if (banded != 0) {
                scaler.RecordLetterbox(commands, source.width, source.height, destination.Handle(), extent.width, extent.height);
            } else {
                VkClearColorValue black{};
                black.float32[3] = 1.0f;
                const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
                transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            }
            PresentationScaler::RecordBlitFrom(context, commands, resident->Handle(), VK_IMAGE_LAYOUT_GENERAL, source.width, source.height, VK_FILTER_LINEAR, destination.Handle(), extent.width, extent.height);
            transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            readImage(context, commands, destination, readback);
            batch.SubmitAndWait();
            results[banded] = bytesOf(readback, bytes);
        }
        Require(results[0] == results[1], "the letterbox bands differ from a full clear: " + describe(source, extent));
    }
}

void passPresentsSlice(const Context& context, PresentationPass& pass) {
    const auto first = makeSource(Bgra8, VK_FORMAT_B8G8R8A8_UNORM, 200, 130);
    const auto second = makeSource(Bgra8, VK_FORMAT_B8G8R8A8_UNORM, 200, 130, 0x5a5a5a5au);
    Require(first.decoded != second.decoded, "the two slices of the array test image hold the same texels");
    const auto resident = residentImage(context, std::vector<const Source*>{&first, &second});
    const std::array<const Source*, 2> slices{&first, &second};
    for (std::uint32_t layer = 0; layer < slices.size(); ++layer) {
        const auto& source = *slices[layer];
        for (const auto extent : {VkExtent2D{source.width, source.height}, VkExtent2D{source.width + 60, source.height}}) {
            Require(passPresent(context, pass, source, *resident, extent, VK_FORMAT_B8G8R8A8_UNORM, layer) == convertPathPresent(context, source, extent), "the presentation pass of slice " + std::to_string(layer) + " of an array image differs from the convert path: " + describe(source, extent));
        }
        passDumpMatchesSubsample(context, pass, source, *resident, layer);
    }
}

void passRefusesHalfFloat(PresentationPass& pass, const Image& resident) {
    bool refused = false;
    try {
        pass.Bind(1, resident.Handle(), 200, 130, Rgba16Float, AgcDriver::SdrToneMapLuminanceNits);
    } catch (const std::exception&) {
        refused = true;
    }
    Require(refused, "the presentation pass took a 16-bit float display buffer");
    refused = false;
    try {
        pass.Bind(1, resident.Handle(), 200, 130, Rgb10Pq, 0.0f);
    } catch (const std::exception&) {
        refused = true;
    }
    Require(refused, "the presentation pass took a white level below 1 nit");
}

int largestDifference(const std::vector<std::byte>& expected, const std::vector<std::byte>& actual) {
    Require(expected.size() == actual.size(), "compared frames differ in size");
    int worst = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) worst = std::max(worst, std::abs(std::to_integer<int>(expected[i]) - std::to_integer<int>(actual[i])));
    return worst;
}

Source convertedAt(const Source& source, float whiteNits) {
    auto converted = source;
    for (std::size_t i = 0; i < converted.decoded.size(); i += 4) {
        std::uint32_t word = 0;
        std::memcpy(&word, source.words.data() + i, 4);
        const auto pixel = AgcDriver::PqToSdrPixel(word, AgcDriver::DisplayRedLow(source.pixelFormat), whiteNits);
        for (std::size_t channel = 0; channel < pixel.size(); ++channel) converted.decoded[i + channel] = static_cast<std::byte>(pixel[channel]);
    }
    return converted;
}

void passConvertsPq(const Context& context, PresentationPass& pass, const Source& guest, const Image& resident, float whiteNits) {
    const auto source = convertedAt(guest, whiteNits);
    for (const auto extent : {VkExtent2D{source.width, source.height}, VkExtent2D{source.width + 60, source.height}, VkExtent2D{source.width, source.height + 51}}) {
        Require(largestDifference(convertPathPresent(context, source, extent), passPresent(context, pass, source, resident, extent, VK_FORMAT_B8G8R8A8_UNORM, 0, whiteNits)) <= 1, "the PQ presentation pass differs from the CPU conversion by more than one level: " + describe(source, extent));
    }
    for (const auto extent : {VkExtent2D{117, 76}, VkExtent2D{331, 215}}) {
        const int worst = largestDifference(convertPathPresent(context, source, extent), passPresent(context, pass, source, resident, extent, VK_FORMAT_B8G8R8A8_UNORM, 0, whiteNits));
        std::printf("  %s, white %.0f nits: scaled, by at most %d\n", describe(source, extent).c_str(), whiteNits, worst);
        Require(worst <= 2, "the scaled PQ presentation pass differs from the convert path's filtered blit by more than two levels: " + describe(source, extent));
    }
    for (const std::uint32_t stride : {1u, 3u}) {
        const auto extent = PresentationPass::DumpExtent(source.width, source.height, stride);
        std::vector<std::byte> expected(static_cast<std::size_t>(extent.width) * extent.height * 4);
        for (std::uint32_t y = 0; y < extent.height; ++y) {
            for (std::uint32_t x = 0; x < extent.width; ++x) std::memcpy(expected.data() + (static_cast<std::size_t>(y) * extent.width + x) * 4, source.decoded.data() + (static_cast<std::size_t>(y) * stride * source.width + x * stride) * 4, 4);
        }
        Require(largestDifference(expected, passDump(context, pass, source, resident, stride, 0, whiteNits)) <= 1, "the PQ presentation dump differs from the subsampled CPU conversion: " + describe(source, extent) + " stride " + std::to_string(stride));
    }
}

void residentPresentTests(const Context& context) {
    PresentationPass pass(context, 2);
    const std::array<std::pair<std::uint64_t, VkFormat>, 5> cases{{
        {Bgra8 | TenBit, VK_FORMAT_A2B10G10R10_UNORM_PACK32},
        {Rgba8 | TenBit, VK_FORMAT_A2B10G10R10_UNORM_PACK32},
        {Bgra8 | TenBit, VK_FORMAT_A2R10G10B10_UNORM_PACK32},
        {Bgra8, VK_FORMAT_R8G8B8A8_UNORM},
        {Rgba8, VK_FORMAT_B8G8R8A8_UNORM},
    }};
    for (const auto& [pixelFormat, storage] : cases) {
        const auto source = makeSource(pixelFormat, storage, 200, 130);
        const auto resident = residentImage(context, source);
        passMatchesConvertPath(context, pass, source, *resident);
        passDumpMatchesSubsample(context, pass, source, *resident);
        if (AgcDriver::DisplayTenBit(pixelFormat)) passKeepsTenBits(context, pass, source, *resident);
        if (pixelFormat == Bgra8) passRefusesHalfFloat(pass, *resident);
    }
    for (const auto& [pixelFormat, storage] : {std::pair{Rgb10Pq, VK_FORMAT_A2B10G10R10_UNORM_PACK32}, std::pair{Bgr10Pq, VK_FORMAT_A2R10G10B10_UNORM_PACK32}, std::pair{Bgr10Pq, VK_FORMAT_A2B10G10R10_UNORM_PACK32}}) {
        const auto source = makeSource(pixelFormat, storage, 200, 130);
        const auto resident = residentImage(context, source);
        passConvertsPq(context, pass, source, *resident, AgcDriver::SdrToneMapLuminanceNits);
        passConvertsPq(context, pass, source, *resident, 1000.0f);
        passPassesPqThrough(context, pass, source, *resident);
    }
    passRefusesSdrToHdr10(context, pass);
    passPresentsSlice(context, pass);
    blitLetterboxMatchesClear(context);
}

}

int main() {
    try {
        letterboxBandTests();
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        residentPresentTests(device->GetContext());
        std::cout << "resident present format, presentation pass and letterbox tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
