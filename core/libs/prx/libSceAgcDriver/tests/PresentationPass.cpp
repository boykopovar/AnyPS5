#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationUpscaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/UpscaleMode.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "execution/FixtureDeviceSelection.hpp"
#include <SDL_loadso.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::DisplayBuffer;
using AgcDriver::PresentationPass;
using AgcDriver::PresentationScaler;
using AgcDriver::PresentationUpscaler;

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
            timestampBits = families[family].timestampValidBits;
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
            name = properties.deviceName;
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
    const std::string& Name() const { return name; }
    std::uint32_t TimestampBits() const { return timestampBits; }

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
    std::string name;
    std::uint32_t timestampBits = 0;
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

constexpr VkFormat FloatFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

bool refused(const std::function<void()>& action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void upscaleModeTests() {
    using AgcDriver::SelectUpscaleMode;
    const auto unset = SelectUpscaleMode(nullptr, nullptr);
    Require(!unset.fsr && unset.sharpness == 0.2f, "an unset ANYPS5_UPSCALE does not keep today's scaler with the default sharpness");
    Require(!SelectUpscaleMode("", "").fsr, "an empty ANYPS5_UPSCALE is not the same as an unset one");
    const auto fsr = SelectUpscaleMode("fsr", "1.5");
    Require(fsr.fsr && fsr.sharpness == 1.5f, "ANYPS5_UPSCALE=fsr with ANYPS5_FSR_SHARPNESS=1.5 was not selected");
    Require(SelectUpscaleMode("fsr", "0").sharpness == 0.0f && SelectUpscaleMode("fsr", "2.0").sharpness == 2.0f && SelectUpscaleMode("fsr", nullptr).sharpness == 0.2f, "the FSR sharpness range or default is wrong");
    for (const char* value : {"FSR", "fsr1", "bilinear", " fsr", "fsr "}) {
        Require(refused([value] { static_cast<void>(SelectUpscaleMode(value, nullptr)); }), std::string("ANYPS5_UPSCALE accepted ") + value);
    }
    for (const char* value : {"-0.1", "2.01", "abc", "0.5x", "nan", "inf", " 1", "1 "}) {
        Require(refused([value] { static_cast<void>(SelectUpscaleMode("fsr", value)); }), std::string("ANYPS5_FSR_SHARPNESS accepted ") + value);
    }
}

void stageExtentTests() {
    const auto same = [](VkExtent2D a, VkExtent2D b) { return a.width == b.width && a.height == b.height; };
    Require(PresentationUpscaler::Upscales(960, 540, {1920, 1080}) && same(PresentationUpscaler::StageExtent(960, 540, {1920, 1080}), {960, 540}), "a 2x upscale does not stage the display buffer at its own size");
    Require(PresentationUpscaler::Upscales(1920, 1080, {2560, 1600}) && same(PresentationUpscaler::StageExtent(1920, 1080, {2560, 1600}), {1920, 1080}), "a letterboxed upscale does not stage the display buffer at its own size");
    Require(!PresentationUpscaler::Upscales(1920, 1080, {1920, 1080}) && same(PresentationUpscaler::StageExtent(1920, 1080, {1920, 1080}), {1920, 1080}), "a 1:1 presentation upscales");
    Require(!PresentationUpscaler::Upscales(1920, 1080, {1920, 1200}) && same(PresentationUpscaler::StageExtent(1920, 1080, {1920, 1200}), {1920, 1080}), "a letterboxed 1:1 presentation upscales");
    Require(!PresentationUpscaler::Upscales(1920, 1080, {1280, 720}) && same(PresentationUpscaler::StageExtent(1920, 1080, {1280, 720}), {1280, 720}), "a downscale does not stage today's scaled image");
}

void exitDependencyTests() {
    const auto renderedBy = [](const VkSubpassDependency& dependency) { return dependency.srcSubpass == 0 && dependency.dstSubpass == VK_SUBPASS_EXTERNAL && dependency.srcStageMask == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT && dependency.srcAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT && dependency.dependencyFlags == 0; };
    const auto sampled = PresentationPass::ExitDependency(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Require(renderedBy(sampled) && sampled.dstStageMask == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT && sampled.dstAccessMask == VK_ACCESS_SHADER_READ_BIT, "a presentation render pass ending in SHADER_READ_ONLY_OPTIMAL does not order its writes and final layout transition before EASU and RCAS sample the image");
    const auto copied = PresentationPass::ExitDependency(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Require(renderedBy(copied) && copied.dstStageMask == VK_PIPELINE_STAGE_TRANSFER_BIT && copied.dstAccessMask == VK_ACCESS_TRANSFER_READ_BIT, "a presentation render pass ending in TRANSFER_SRC_OPTIMAL does not order its writes before the dump copy");
    const auto presented = PresentationPass::ExitDependency(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    Require(renderedBy(presented) && presented.dstStageMask == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT && presented.dstAccessMask == 0, "the presentation render pass's dependency into the presentation engine changed");
}

struct FloatImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<float> texels;

    float At(std::uint32_t x, std::uint32_t y, std::uint32_t channel = 0) const { return texels[(static_cast<std::size_t>(y) * width + x) * 4 + channel]; }
};

FloatImage grayImage(std::uint32_t width, std::uint32_t height, const std::function<float(std::uint32_t, std::uint32_t)>& value) {
    FloatImage image{width, height, std::vector<float>(static_cast<std::size_t>(width) * height * 4)};
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const float gray = value(x, y);
            auto* texel = image.texels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
            texel[0] = texel[1] = texel[2] = gray;
            texel[3] = 1.0f;
        }
    }
    return image;
}

bool floatTargets(const Context& context) {
    VkFormatProperties properties{};
    context.formatProperties(context.physical, FloatFormat, &properties);
    constexpr VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    return (properties.optimalTilingFeatures & needed) == needed;
}

FloatImage upscaleFloats(const Context& context, PresentationUpscaler& upscaler, const FloatImage& source, VkExtent2D extent, bool sharpen) {
    Buffer upload(context, source.texels.size() * sizeof(float), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::memcpy(upload.Bytes().data(), source.texels.data(), source.texels.size() * sizeof(float));
    Image destination(context, FloatFormat, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    FloatImage result{extent.width, extent.height, std::vector<float>(static_cast<std::size_t>(extent.width) * extent.height * 4)};
    const auto bytes = result.texels.size() * sizeof(float);
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        const auto stage = upscaler.Stage(0, FloatFormat, {source.width, source.height});
        upscaler.RecordStageTransferDestination(commands, 0);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {source.width, source.height, 1};
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, upload.Handle(), stage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        upscaler.RecordStageWritten(commands, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        if (sharpen) upscaler.RecordPresent(commands, 0, source.width, source.height, destination.Handle(), extent, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        else upscaler.RecordUpscale(commands, 0, destination.Handle(), extent, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        readImage(context, commands, destination, readback);
        batch.SubmitAndWait();
    }
    upscaler.Release(0);
    std::memcpy(result.texels.data(), readback.Bytes().data(), bytes);
    return result;
}

float bilinear(const FloatImage& source, std::uint32_t x, std::uint32_t y, VkExtent2D extent) {
    const double positionX = (x + 0.5) * source.width / extent.width - 0.5;
    const double positionY = (y + 0.5) * source.height / extent.height - 0.5;
    const double baseX = std::floor(positionX), baseY = std::floor(positionY);
    const auto texel = [&](double tx, double ty) {
        const auto cx = static_cast<std::uint32_t>(std::clamp(tx, 0.0, static_cast<double>(source.width - 1)));
        const auto cy = static_cast<std::uint32_t>(std::clamp(ty, 0.0, static_cast<double>(source.height - 1)));
        return static_cast<double>(source.At(cx, cy));
    };
    const double weightX = positionX - baseX, weightY = positionY - baseY;
    const double top = texel(baseX, baseY) + (texel(baseX + 1, baseY) - texel(baseX, baseY)) * weightX;
    const double bottom = texel(baseX, baseY + 1) + (texel(baseX + 1, baseY + 1) - texel(baseX, baseY + 1)) * weightX;
    return static_cast<float>(top + (bottom - top) * weightY);
}

double riseDistance(const std::vector<float>& profile, float low, float high) {
    const auto crossing = [&](float level, double after) {
        for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
            if (static_cast<double>(i) + 1.0 <= after) continue;
            if (profile[i] < level && profile[i + 1] >= level) return static_cast<double>(i) + (level - profile[i]) / (profile[i + 1] - profile[i]);
        }
        throw std::runtime_error("an edge profile does not cross its 10% or 90% level");
    };
    const double start = crossing(low + 0.1f * (high - low), -1.0);
    return crossing(low + 0.9f * (high - low), start) - start;
}

FloatImage easuPattern() {
    return grayImage(960, 540, [](std::uint32_t x, std::uint32_t y) {
        if (x < 240) return 0.5f;
        if (x < 480) {
            if (y < 270) return x < 360 ? 0.2f : 0.8f;
            return y < 405 ? 0.2f : 0.8f;
        }
        if (x < 720) return x % 8 == 0 || y % 8 == 0 ? 0.9f : 0.1f;
        return static_cast<float>(x - 720) / 239.0f;
    });
}

void easuTests(const Context& context) {
    PresentationUpscaler upscaler(context, 1, 0.2f);
    const auto source = easuPattern();
    const VkExtent2D extent{1920, 1080};
    const auto upscaled = upscaleFloats(context, upscaler, source, extent, false);
    for (std::size_t i = 0; i < upscaled.texels.size(); ++i) {
        Require(std::isfinite(upscaled.texels[i]), "EASU wrote a NaN or an infinity at texel " + std::to_string(i / 4));
        Require(upscaled.texels[i] >= 0.0f && upscaled.texels[i] <= 1.0f, "EASU left [0, 1] at texel " + std::to_string(i / 4) + ": " + std::to_string(upscaled.texels[i]));
    }
    float flatError = 0.0f;
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        for (std::uint32_t x = 4; x < 472; ++x) {
            for (std::uint32_t channel = 0; channel < 3; ++channel) flatError = std::max(flatError, std::abs(upscaled.At(x, y, channel) - 0.5f));
        }
    }
    Require(flatError <= 1e-5f, "EASU changed a flat region by " + std::to_string(flatError));
    std::vector<float> easuRow, bilinearRow, easuColumn, bilinearColumn;
    for (std::uint32_t x = 700; x < 740; ++x) {
        easuRow.push_back(upscaled.At(x, 270));
        bilinearRow.push_back(bilinear(source, x, 270, extent));
    }
    for (std::uint32_t y = 790; y < 830; ++y) {
        easuColumn.push_back(upscaled.At(600, y));
        bilinearColumn.push_back(bilinear(source, 600, y, extent));
    }
    const double easuVertical = riseDistance(easuRow, 0.2f, 0.8f), bilinearVertical = riseDistance(bilinearRow, 0.2f, 0.8f);
    const double easuHorizontal = riseDistance(easuColumn, 0.2f, 0.8f), bilinearHorizontal = riseDistance(bilinearColumn, 0.2f, 0.8f);
    std::printf("  EASU 960x540 to 1920x1080: flat region within %.2g; 10-90%% edge rise %.2f px across a vertical edge (bilinear %.2f) and %.2f px across a horizontal one (bilinear %.2f)\n", static_cast<double>(flatError), easuVertical, bilinearVertical, easuHorizontal, bilinearHorizontal);
    Require(easuVertical < bilinearVertical && easuHorizontal < bilinearHorizontal, "EASU's edge transition is not narrower than the bilinear scaler's");
    for (std::uint32_t y = 100; y < 1000; y += 300) {
        for (std::uint32_t x = 1448; x < 1912; ++x) Require(upscaled.At(x + 1, y) >= upscaled.At(x, y) - 1e-6f, "EASU made a gradient decrease at " + std::to_string(x) + "," + std::to_string(y));
    }
}

FloatImage rcasPattern(float low, float high) {
    return grayImage(256, 128, [low, high](std::uint32_t x, std::uint32_t y) {
        if (x < 64) return 0.5f;
        if (x < 160) return low + (high - low) / (1.0f + std::exp(-(static_cast<float>(x) - 112.0f) / 1.5f));
        return x % 4 == 0 || y % 4 == 0 ? high : low;
    });
}

std::pair<float, float> flatRange(const FloatImage& image, std::uint32_t width) {
    float low = 1.0f, high = 0.0f;
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            for (std::uint32_t channel = 0; channel < 3; ++channel) {
                low = std::min(low, image.At(x, y, channel));
                high = std::max(high, image.At(x, y, channel));
            }
        }
    }
    return {low, high};
}

void rcasTests(const Context& context) {
    const VkExtent2D extent{256, 128};
    const auto source = rcasPattern(0.3f, 0.7f);
    PresentationUpscaler weakest(context, 1, 2.0f);
    const auto weak = upscaleFloats(context, weakest, source, extent, true);
    float weakDifference = 0.0f;
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        for (std::uint32_t x = 0; x < extent.width; ++x) weakDifference = std::max(weakDifference, std::abs(weak.At(x, y) - source.At(x, y)));
    }
    PresentationUpscaler sharpest(context, 1, 0.2f);
    const auto sharp = upscaleFloats(context, sharpest, source, extent, true);
    const auto [weakLow, weakHigh] = flatRange(weak, 60);
    const auto [sharpLow, sharpHigh] = flatRange(sharp, 60);
    const float sourceContrast = source.At(114, 64) - source.At(110, 64);
    const float weakContrast = weak.At(114, 64) - weak.At(110, 64);
    const float sharpContrast = sharp.At(114, 64) - sharp.At(110, 64);
    std::printf("  RCAS 1:1: sharpness 2.0 changes the image by at most %.4f; edge contrast %.4f in the source, %.4f at sharpness 2.0, %.4f at 0.2; a flat 0.5 region stays uniform at %.5f and %.5f\n", static_cast<double>(weakDifference), static_cast<double>(sourceContrast), static_cast<double>(weakContrast), static_cast<double>(sharpContrast), static_cast<double>(weakLow), static_cast<double>(sharpLow));
    Require(weakDifference <= 0.06f, "RCAS at sharpness 2.0 moved the image by more than 0.06");
    Require(weakHigh - weakLow <= 1e-6f && sharpHigh - sharpLow <= 1e-6f, "RCAS made a flat region uneven");
    Require(std::abs(weakLow - 0.5f) <= 0.5f * 0.0035f && std::abs(sharpLow - 0.5f) <= 0.5f * 0.0035f, "RCAS moved a flat region by more than its approximate reciprocal allows");
    Require(sharpContrast > sourceContrast + 0.005f && sharpContrast > weakContrast, "RCAS at sharpness 0.2 did not raise the edge contrast");
    PresentationUpscaler maximum(context, 1, 0.0f);
    for (const auto& extreme : {rcasPattern(0.0f, 1.0f), easuPattern()}) {
        const VkExtent2D size{extreme.width, extreme.height};
        for (const auto& [output, target] : {std::pair{upscaleFloats(context, maximum, extreme, size, true), size}, std::pair{upscaleFloats(context, maximum, extreme, {size.width * 2, size.height * 2}, true), VkExtent2D{size.width * 2, size.height * 2}}}) {
            for (std::size_t i = 0; i < output.texels.size(); ++i) Require(std::isfinite(output.texels[i]) && output.texels[i] >= 0.0f && output.texels[i] <= 1.0f, "FSR at sharpness 0 left [0, 1] at texel " + std::to_string(i / 4) + " of " + std::to_string(target.width) + "x" + std::to_string(target.height));
        }
    }
}

std::vector<std::byte> passStage(const Context& context, PresentationPass& pass, const Source& source, const Image& resident, VkExtent2D extent, VkFormat format) {
    Image destination(context, format, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto bytes = static_cast<std::size_t>(extent.width) * extent.height * 4;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    pass.Bind(0, resident.Handle(), source.width, source.height, source.pixelFormat, AgcDriver::SdrToneMapLuminanceNits);
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        pass.RecordStage(commands, 0, destination.Handle(), format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        readImage(context, commands, destination, readback);
        batch.SubmitAndWait();
    }
    pass.Release(0);
    return bytesOf(readback, bytes);
}

std::vector<std::byte> stretchStage(const Context& context, const Image& resident, std::uint32_t width, std::uint32_t height, VkExtent2D extent, bool whole) {
    Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, extent, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto bytes = static_cast<std::size_t>(extent.width) * extent.height * 4;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    if (whole) PresentationScaler::RecordStretchFrom(context, commands, resident.Handle(), VK_IMAGE_LAYOUT_GENERAL, width, height, VK_FILTER_LINEAR, destination.Handle(), AgcDriver::AspectFitRect{0, 0, extent.width, extent.height});
    else PresentationScaler::RecordBlitFrom(context, commands, resident.Handle(), VK_IMAGE_LAYOUT_GENERAL, width, height, VK_FILTER_LINEAR, destination.Handle(), extent.width, extent.height);
    transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    readImage(context, commands, destination, readback);
    batch.SubmitAndWait();
    return bytesOf(readback, bytes);
}

std::vector<std::byte> cropped(const std::vector<std::byte>& frame, VkExtent2D extent, const AgcDriver::AspectFitRect& rect) {
    std::vector<std::byte> region;
    for (std::uint32_t y = 0; y < rect.height; ++y) {
        const auto* row = frame.data() + (static_cast<std::size_t>(rect.y + static_cast<std::int32_t>(y)) * extent.width + static_cast<std::size_t>(rect.x)) * 4;
        region.insert(region.end(), row, row + static_cast<std::size_t>(rect.width) * 4);
    }
    return region;
}

std::vector<std::byte> fsrPresent(const Context& context, PresentationPass& pass, PresentationUpscaler& upscaler, const Source& source, const Image& resident, VkExtent2D extent, VkFormat format, VkColorSpaceKHR colorSpace, float whiteNits) {
    Image destination(context, format, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto bytes = static_cast<std::size_t>(extent.width) * extent.height * 4;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    pass.Bind(0, resident.Handle(), source.width, source.height, source.pixelFormat, whiteNits);
    const auto stageExtent = PresentationUpscaler::StageExtent(source.width, source.height, extent);
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        const auto stage = upscaler.Stage(0, format, stageExtent);
        pass.RecordStage(commands, 0, stage, format, colorSpace, stageExtent, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        upscaler.RecordPresent(commands, 0, source.width, source.height, destination.Handle(), extent, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        readImage(context, commands, destination, readback);
        batch.SubmitAndWait();
    }
    pass.Release(0);
    upscaler.Release(0);
    return bytesOf(readback, bytes);
}

void defaultPathTests(const Context& context) {
    Require(!AgcDriver::SelectUpscaleMode(nullptr, nullptr).fsr, "the default presentation selects FSR");
    PresentationPass pass(context, 1);
    PresentationUpscaler upscaler(context, 1, 0.2f);
    const auto source = makeSource(Bgra8, VK_FORMAT_B8G8R8A8_UNORM, 200, 130);
    const auto resident = residentImage(context, source);
    for (const auto extent : {VkExtent2D{200, 130}, VkExtent2D{260, 130}, VkExtent2D{400, 260}, VkExtent2D{520, 300}, VkExtent2D{117, 76}}) {
        const auto before = passPresent(context, pass, source, *resident, extent, VK_FORMAT_B8G8R8A8_UNORM);
        static_cast<void>(fsrPresent(context, pass, upscaler, source, *resident, extent, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, AgcDriver::SdrToneMapLuminanceNits));
        Require(passPresent(context, pass, source, *resident, extent, VK_FORMAT_B8G8R8A8_UNORM) == before, "the default presentation changed after FSR presented from the same pass: " + describe(source, extent));
        const auto rect = AgcDriver::ComputeContainRect_nid_postfix(source.width, source.height, extent.width, extent.height);
        if (PresentationUpscaler::Upscales(source.width, source.height, extent)) continue;
        const VkExtent2D staged{rect.width, rect.height};
        Require(passStage(context, pass, source, *resident, staged, VK_FORMAT_B8G8R8A8_UNORM) == cropped(before, extent, rect), "the FSR stage differs from today's presentation pass inside the image: " + describe(source, extent));
        Require(stretchStage(context, *resident, source.width, source.height, staged, true) == cropped(stretchStage(context, *resident, source.width, source.height, extent, false), extent, rect), "the FSR stage differs from today's blit inside the image: " + describe(source, extent));
    }
}

void sdrPresentTests(const Context& context) {
    PresentationPass pass(context, 1);
    PresentationUpscaler upscaler(context, 1, 0.2f);
    Source flat{Bgra8, VK_FORMAT_B8G8R8A8_UNORM, 200, 130, std::vector<std::byte>(200 * 130 * 4), {}};
    for (std::size_t i = 0; i < flat.words.size(); i += 4) {
        flat.words[i] = std::byte{0x30};
        flat.words[i + 1] = std::byte{0x80};
        flat.words[i + 2] = std::byte{0xc0};
        flat.words[i + 3] = std::byte{0xff};
    }
    const auto resident = residentImage(context, flat);
    for (const auto extent : {VkExtent2D{520, 300}, VkExtent2D{200, 160}, VkExtent2D{117, 76}}) {
        const auto rect = AgcDriver::ComputeContainRect_nid_postfix(flat.width, flat.height, extent.width, extent.height);
        const auto frame = fsrPresent(context, pass, upscaler, flat, *resident, extent, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, AgcDriver::SdrToneMapLuminanceNits);
        std::uint32_t first = 0;
        std::memcpy(&first, frame.data() + (static_cast<std::size_t>(rect.y) * extent.width + static_cast<std::size_t>(rect.x)) * 4, 4);
        const auto withinOneLevel = [](std::uint32_t word, std::uint32_t shift, int expected) { return std::abs(static_cast<int>((word >> shift) & 0xffu) - expected) <= 1; };
        Require(withinOneLevel(first, 0, 0x30) && withinOneLevel(first, 8, 0x80) && withinOneLevel(first, 16, 0xc0) && (first >> 24u) == 0xffu, "FSR moved a flat SDR frame by more than one level: " + describe(flat, extent));
        for (std::uint32_t y = 0; y < extent.height; ++y) {
            for (std::uint32_t x = 0; x < extent.width; ++x) {
                std::uint32_t word = 0;
                std::memcpy(&word, frame.data() + (static_cast<std::size_t>(y) * extent.width + x) * 4, 4);
                const bool inside = static_cast<std::int32_t>(x) >= rect.x && static_cast<std::int32_t>(x) < rect.x + static_cast<std::int32_t>(rect.width) && static_cast<std::int32_t>(y) >= rect.y && static_cast<std::int32_t>(y) < rect.y + static_cast<std::int32_t>(rect.height);
                Require(word == (inside ? first : 0xff000000u), "FSR presentation of a flat SDR frame is not uniform inside and black outside: " + describe(flat, extent));
            }
        }
    }
}

void hdr10PresentTests(const Context& context) {
    if (!renderable(context, VK_FORMAT_A2B10G10R10_UNORM_PACK32)) {
        std::printf("  10-bit destination is not renderable here; HDR10 FSR skipped\n");
        return;
    }
    PresentationPass pass(context, 1);
    PresentationUpscaler upscaler(context, 1, 0.0f);
    Source pq{Rgb10Pq, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 200, 130, std::vector<std::byte>(200 * 130 * 4), {}};
    constexpr std::uint32_t dark = 0xc0000000u | (64u << 20u) | (64u << 10u) | 64u;
    constexpr std::uint32_t bright = 0xc0000000u | (1023u << 20u) | (1023u << 10u) | 1023u;
    for (std::uint32_t y = 0; y < pq.height; ++y) {
        for (std::uint32_t x = 0; x < pq.width; ++x) {
            const std::uint32_t word = x < 100 ? dark : (x / 2 + y / 2) % 2 == 0 ? bright : 0xc0000000u;
            std::memcpy(pq.words.data() + (static_cast<std::size_t>(y) * pq.width + x) * 4, &word, 4);
        }
    }
    const auto resident = residentImage(context, pq);
    for (const auto extent : {VkExtent2D{520, 300}, VkExtent2D{200, 130}}) {
        const auto rect = AgcDriver::ComputeContainRect_nid_postfix(pq.width, pq.height, extent.width, extent.height);
        const auto frame = fsrPresent(context, pass, upscaler, pq, *resident, extent, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT, 1000.0f);
        const double scale = static_cast<double>(rect.width) / pq.width;
        std::uint32_t flatWord = 0;
        std::memcpy(&flatWord, frame.data() + (static_cast<std::size_t>(rect.y + 4) * extent.width + static_cast<std::size_t>(rect.x)) * 4, 4);
        for (const std::uint32_t shift : {0u, 10u, 20u}) Require(std::abs(static_cast<int>((flatWord >> shift) & 0x3ffu) - 64) <= 1, "FSR moved the PQ code values of a flat HDR10 region by more than one code: " + describe(pq, extent));
        for (std::uint32_t y = 0; y < extent.height; ++y) {
            for (std::uint32_t x = 0; x < extent.width; ++x) {
                std::uint32_t word = 0;
                std::memcpy(&word, frame.data() + (static_cast<std::size_t>(y) * extent.width + x) * 4, 4);
                const std::int32_t localX = static_cast<std::int32_t>(x) - rect.x, localY = static_cast<std::int32_t>(y) - rect.y;
                const bool inside = localX >= 0 && localX < static_cast<std::int32_t>(rect.width) && localY >= 0 && localY < static_cast<std::int32_t>(rect.height);
                Require((word >> 30u) == 3u, "an HDR10 FSR texel is not opaque: " + describe(pq, extent));
                if (!inside) {
                    Require(word == 0xc0000000u, "an HDR10 FSR letterbox band is not opaque PQ black: " + describe(pq, extent));
                } else if (localX + 4 < static_cast<std::int32_t>(100 * scale) && localY > 3 && localY + 4 < static_cast<std::int32_t>(rect.height)) {
                    Require(word == flatWord, "FSR made a flat HDR10 region uneven at " + std::to_string(x) + "," + std::to_string(y) + ": " + describe(pq, extent));
                }
            }
        }
    }
}

double percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    const auto rank = static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(values.size())));
    return values[std::clamp<std::size_t>(rank, 1, values.size()) - 1];
}

std::string spread(const std::vector<double>& values, int precision) {
    if (values.empty()) return "-";
    char text[64];
    std::snprintf(text, sizeof(text), "%.*f/%.*f", precision, percentile(values, 0.5), precision, percentile(values, 0.95));
    return text;
}

int fsrCost(const Device& device) {
    const auto& context = device.GetContext();
    if (device.TimestampBits() == 0 || !(context.limits.timestampPeriod > 0.0f)) {
        std::printf("FSR cost on %s: the graphics queue has no timestamps\n", device.Name().c_str());
        return VulkanTestSkipped;
    }
    constexpr std::uint32_t WarmUp = 20;
    constexpr std::uint32_t Frames = 200;
    constexpr std::uint32_t Stamps = 6;
    constexpr VkFormat Format = VK_FORMAT_B8G8R8A8_UNORM;
    const std::uint64_t mask = device.TimestampBits() >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << device.TimestampBits()) - 1;
    const double nanoseconds = static_cast<double>(context.limits.timestampPeriod);
    const std::array<std::pair<VkExtent2D, VkExtent2D>, 6> cases{{{{1920, 1080}, {2560, 1440}}, {{1920, 1080}, {3840, 2160}}, {{1920, 1080}, {1920, 1080}}, {{3840, 2160}, {3840, 2160}}, {{1280, 720}, {3840, 2160}}, {{3840, 2160}, {2560, 1440}}}};
    VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryInfo.queryCount = Stamps;
    VkQueryPool queries = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(context.device, &queryInfo, nullptr, &queries), "vkCreateQueryPool FSR cost");
    const auto destroyQueries = [&] { context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, queries, nullptr); };
    try {
        const auto timestamp = context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp");
        const auto resetQueries = context.Function<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool");
        const auto queryResults = context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults");
        for (const auto& [size, window] : cases) {
            Source pattern{Bgra8, Format, size.width, size.height, std::vector<std::byte>(static_cast<std::size_t>(size.width) * size.height * 4), {}};
            std::uint32_t seed = size.width * 7919u + window.width;
            for (auto& byte : pattern.words) {
                seed = seed * 1664525u + 1013904223u;
                byte = static_cast<std::byte>(seed >> 24u);
            }
            const auto source = residentImage(context, pattern);
            const auto rect = AgcDriver::ComputeContainRect_nid_postfix(size.width, size.height, window.width, window.height);
            const VkExtent2D fitted{rect.width, rect.height};
            const bool upscales = PresentationUpscaler::Upscales(size.width, size.height, window);
            PresentationUpscaler upscaler(context, 2, 0.2f);
            Image easuOutput(context, Format, fitted, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            Image rcasOutput(context, Format, window, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            Image presented(context, Format, window, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            Image blitted(context, Format, window, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            const auto writeStage = [&](VkCommandBuffer commands, std::size_t slot, VkExtent2D extent) {
                const auto stage = upscaler.Stage(slot, Format, extent);
                upscaler.RecordStageTransferDestination(commands, slot);
                PresentationScaler::RecordStretchFrom(context, commands, source->Handle(), VK_IMAGE_LAYOUT_GENERAL, size.width, size.height, VK_FILTER_LINEAR, stage, AgcDriver::AspectFitRect{0, 0, extent.width, extent.height});
                upscaler.RecordStageWritten(commands, slot, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            };
            {
                CommandBatch batch(context);
                writeStage(batch.Handle(), 1, fitted);
                batch.SubmitAndWait();
            }
            std::vector<double> stageMs, presentedMs, easuMs, rcasMs, totalMs, blitMs, fsrCpuUs, blitCpuUs;
            for (std::uint32_t frame = 0; frame < WarmUp + Frames; ++frame) {
                CommandBatch batch(context);
                const auto commands = batch.Handle();
                resetQueries(commands, queries, 0, Stamps);
                timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 0);
                const auto fsrStart = std::chrono::steady_clock::now();
                writeStage(commands, 0, PresentationUpscaler::StageExtent(size.width, size.height, window));
                timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
                upscaler.RecordPresent(commands, 0, size.width, size.height, presented.Handle(), window, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                const auto fsrRecorded = std::chrono::steady_clock::now() - fsrStart;
                timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 2);
                if (upscales) upscaler.RecordUpscale(commands, 0, easuOutput.Handle(), fitted, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 3);
                upscaler.RecordPresent(commands, 1, fitted.width, fitted.height, rcasOutput.Handle(), window, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 4);
                const auto blitStart = std::chrono::steady_clock::now();
                transition(context, commands, blitted.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
                PresentationScaler::RecordBlitFrom(context, commands, source->Handle(), VK_IMAGE_LAYOUT_GENERAL, size.width, size.height, VK_FILTER_LINEAR, blitted.Handle(), window.width, window.height);
                const auto blitRecorded = std::chrono::steady_clock::now() - blitStart;
                timestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 5);
                batch.SubmitAndWait();
                const auto releaseStart = std::chrono::steady_clock::now();
                upscaler.Release(0);
                const auto released = std::chrono::steady_clock::now() - releaseStart;
                upscaler.Release(1);
                std::array<std::uint64_t, Stamps> stamps{};
                Check(queryResults(context.device, queries, 0, Stamps, sizeof(stamps), stamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "vkGetQueryPoolResults FSR cost");
                if (frame < WarmUp) continue;
                const auto milliseconds = [&](std::size_t from, std::size_t to) { return static_cast<double>((stamps[to] - stamps[from]) & mask) * nanoseconds / 1e6; };
                stageMs.push_back(milliseconds(0, 1));
                presentedMs.push_back(milliseconds(1, 2));
                totalMs.push_back(milliseconds(0, 2));
                if (upscales) easuMs.push_back(milliseconds(2, 3));
                rcasMs.push_back(milliseconds(3, 4));
                blitMs.push_back(milliseconds(4, 5));
                fsrCpuUs.push_back(std::chrono::duration<double, std::micro>(fsrRecorded + released).count());
                blitCpuUs.push_back(std::chrono::duration<double, std::micro>(blitRecorded).count());
            }
            const char* mode = !upscales ? (fitted.width == size.width && fitted.height == size.height ? "RCAS only" : "downscale, RCAS only") : fitted.width > size.width * 2 || fitted.height > size.height * 2 ? "EASU beyond 2x + RCAS" : "EASU + RCAS";
            std::printf("FSR cost on %s: %ux%u into %ux%u (%s): GPU ms median/p95: stage %s, EASU %s, RCAS %s, EASU + RCAS as presented %s, FSR total %s; bilinear blit %s. CPU us median/p95: FSR recording and release %s, bilinear blit recording %s. %u frames after %u warm-up\n", device.Name().c_str(), size.width, size.height, window.width, window.height, mode, spread(stageMs, 2).c_str(), spread(easuMs, 2).c_str(), spread(rcasMs, 2).c_str(), spread(presentedMs, 2).c_str(), spread(totalMs, 2).c_str(), spread(blitMs, 2).c_str(), spread(fsrCpuUs, 0).c_str(), spread(blitCpuUs, 0).c_str(), Frames, WarmUp);
            std::fflush(stdout);
        }
    } catch (...) {
        destroyQueries();
        throw;
    }
    destroyQueries();
    return 0;
}

void fsrTests(const Context& context) {
    if (!floatTargets(context)) throw std::runtime_error("the device cannot render, sample and blit R32G32B32A32_SFLOAT images");
    easuTests(context);
    rcasTests(context);
    defaultPathTests(context);
    sdrPresentTests(context);
    hdr10PresentTests(context);
}

}

int main(int argc, char** argv) {
    try {
        const bool fsr = argc == 2 && std::string_view(argv[1]) == "--fsr";
        const bool fsrCostMode = argc == 2 && std::string_view(argv[1]) == "--fsr-cost";
        if (fsr) {
            upscaleModeTests();
            stageExtentTests();
            exitDependencyTests();
        } else if (!fsrCostMode) {
            letterboxBandTests();
        }
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        if (fsrCostMode) return fsrCost(*device);
        if (fsr) {
            fsrTests(device->GetContext());
            std::cout << "FSR 1 upscale mode, EASU, RCAS, default path and HDR10 tests passed\n";
            return 0;
        }
        residentPresentTests(device->GetContext());
        std::cout << "resident present format, presentation pass and letterbox tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
