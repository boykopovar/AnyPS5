#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/ToneMapLuminance.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/General.hpp"
#include <SDL_loadso.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

using namespace AgcDriver::Graphics;
using AgcDriver::OverlayContent;
using AgcDriver::OverlayCorner;
using AgcDriver::OverlayMetrics;
using AgcDriver::OverlayRect;
using AgcDriver::PerformanceOverlay;
using AgcDriver::PresentationPass;

constexpr int VulkanTestSkipped = 77;
constexpr std::uint64_t Bgra8 = 0x8000000000000000ull;

std::optional<std::size_t> selectDevice(std::span<const VkPhysicalDeviceProperties> devices, std::string_view request) {
    const auto rankDeviceType = [](VkPhysicalDeviceType type) {
        switch (type) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
            default: return 0;
        }
    };
    std::optional<std::size_t> selected;
    int selectedRank = -1;
    for (std::size_t index = 0; index < devices.size(); ++index) {
        const auto& candidate = devices[index];
        if (candidate.apiVersion < VK_API_VERSION_1_1) continue;
        const std::string_view name(candidate.deviceName);
        if (!request.empty() && std::search(name.begin(), name.end(), request.begin(), request.end(), [](char left, char right) {
            return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
        }) == name.end()) continue;
        const int rank = request.empty() ? rankDeviceType(candidate.deviceType) : 4;
        if (rank <= selectedRank) continue;
        selected = index;
        selectedRank = rank;
    }
    return selected;
}

void deviceSelectionTests() {
    const auto properties = [](const char* name, VkPhysicalDeviceType type) {
        VkPhysicalDeviceProperties result{};
        result.apiVersion = VK_API_VERSION_1_1;
        result.deviceType = type;
        std::memcpy(result.deviceName, name, std::strlen(name) + 1u);
        return result;
    };
    std::array devices{
        properties("AMD Radeon Graphics", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU),
        properties("NVIDIA GeForce RTX 5080", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU),
        properties("Virtual GPU", VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU),
        properties("CPU", VK_PHYSICAL_DEVICE_TYPE_CPU),
    };
    Require(!selectDevice({}, {}).has_value(), "an empty device list selected a device");
    Require(selectDevice(devices, {}) == 1u, "default selection did not prefer the discrete GPU");
    Require(selectDevice(devices, "AMD") == 0u, "the AMD request did not select the integrated GPU");
    Require(selectDevice(devices, "NVIDIA") == 1u, "the NVIDIA request did not select the discrete GPU");
    Require(selectDevice(devices, "aMd rAdEoN") == 0u, "a requested integrated GPU lost to an unrelated discrete GPU");
    Require(selectDevice(devices, "rtx 5080") == 1u, "a partial GPU name did not match without regard to case");
    Require(selectDevice(devices, "Virtual") == 2u, "a requested virtual GPU was not selected");
    Require(selectDevice(devices, "cpu") == 3u, "a requested CPU device was not selected");
    Require(!selectDevice(devices, "missing").has_value(), "an unmatched request selected another GPU");
    Require(!selectDevice(devices, "NVIDIA GeForce RTX 5080 with a longer name").has_value(), "a request longer than the device name matched");
    Require(selectDevice(std::span(devices).subspan(2), {}) == 0u, "default selection did not prefer the virtual GPU to the CPU");
    devices[1].apiVersion = VK_API_VERSION_1_0;
    Require(selectDevice(devices, {}) == 0u, "default selection did not prefer the integrated GPU to the virtual GPU");
    Require(!selectDevice(devices, "NVIDIA").has_value(), "a requested device without Vulkan 1.1 was selected");
    devices[1] = devices[0];
    Require(selectDevice(devices, {}) == 0u && selectDevice(devices, "AMD") == 0u, "equally ranked matches did not retain enumeration order");
    for (auto& device : devices) device.apiVersion = VK_API_VERSION_1_0;
    Require(!selectDevice(devices, {}).has_value(), "a list without Vulkan 1.1 selected a device");
    std::puts("performance-overlay device selection CPU tests passed");
}

class Device {
public:
    Device() {
#ifdef _WIN32
        library = SDL_LoadObject("vulkan-1.dll");
#elif defined(__APPLE__)
        library = SDL_LoadObject("libvulkan.1.dylib");
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
#ifdef __APPLE__
            const char* portability = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            info.enabledExtensionCount = 1;
            info.ppEnabledExtensionNames = &portability;
#endif
            Check(function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
            std::uint32_t count = 0;
            const auto enumerate = function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
            Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            Require(count != 0, "no Vulkan device");
            std::vector<VkPhysicalDevice> devices(count);
            Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            const auto physicalProperties = function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
            std::vector<VkPhysicalDeviceProperties> deviceProperties(devices.size());
            for (std::size_t index = 0; index < devices.size(); ++index) physicalProperties(devices[index], &deviceProperties[index]);
            const char* requestedName = std::getenv("ANYPS5_GPU");
            const auto selectedIndex = selectDevice(deviceProperties, requestedName != nullptr ? requestedName : "");
            Require(selectedIndex.has_value(), requestedName != nullptr && *requestedName != '\0' ? std::string("no Vulkan 1.1 device matching ANYPS5_GPU=\"") + requestedName + "\"" : "no Vulkan 1.1 device");
            context.physical = devices[*selectedIndex];
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) ++family;
            Require(family < count, "no Vulkan graphics queue");
            timestampBits = families[family].timestampValidBits;
            const auto enumerateExtensions = function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            Check(enumerateExtensions(context.physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> extensions(count);
            Check(enumerateExtensions(context.physical, nullptr, &count, extensions.data()), "vkEnumerateDeviceExtensionProperties");
            memoryBudget = std::any_of(extensions.begin(), extensions.end(), [](const VkExtensionProperties& extension) { return std::string(extension.extensionName) == VK_EXT_MEMORY_BUDGET_EXTENSION_NAME; });
            const char* const budgetExtension = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.enabledExtensionCount = memoryBudget ? 1 : 0;
            device.ppEnabledExtensionNames = memoryBudget ? &budgetExtension : nullptr;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            if (memoryBudget) context.memoryProperties2 = function<PFN_vkGetPhysicalDeviceMemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2");
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
    bool MemoryBudget() const { return memoryBudget; }

    void VerifySelection() const {
        VkPhysicalDeviceProperties properties{};
        function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
        std::cout << "Selected GPU name=" << properties.deviceName << " vendor=" << properties.vendorID << " device=" << properties.deviceID << '\n';
        const char* request = std::getenv("ANYPS5_GPU");
        Require(selectDevice(std::span(&properties, 1), request != nullptr ? request : "").has_value(), "the performance-overlay fixture physical device does not match ANYPS5_GPU");
    }

private:
    template<typename TFunction>
    TFunction function(const char* functionName) const {
        const auto result = reinterpret_cast<TFunction>(instanceProc(instance, functionName));
        Require(result != nullptr, functionName);
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
    bool memoryBudget = false;
};

class Image {
public:
    Image(const Context& context, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImageCreateFlags flags = 0) : context(context), extent(extent) {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.flags = flags;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {extent.width, extent.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage performance overlay test");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        const auto allocated = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory);
        if (allocated != VK_SUCCESS) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        Check(allocated, "vkAllocateMemory performance overlay test");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory performance overlay test");
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

void transition(const Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void readImage(const Context& context, VkCommandBuffer commands, const Image& image, Buffer& destination) {
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {image.Extent().width, image.Extent().height, 1};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image.Handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination.Handle(), 1, &copy);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
}

std::vector<std::uint32_t> wordsOf(Buffer& buffer, VkExtent2D extent) {
    std::vector<std::uint32_t> words(static_cast<std::size_t>(extent.width) * extent.height);
    std::memcpy(words.data(), buffer.Bytes().data(), words.size() * sizeof(std::uint32_t));
    return words;
}

void setEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

bool inside(const OverlayRect& rect, std::int32_t x, std::int32_t y) {
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}

OverlayContent sixtyFps() {
    AgcDriver::OverlayStatistics statistics;
    const auto start = AgcDriver::OverlayStatistics::Clock::time_point{} + std::chrono::hours(1);
    for (int i = 0; i <= 60; ++i) statistics.AddPresent(start + std::chrono::nanoseconds(16'666'667) * i, 0, 0);
    auto content = AgcDriver::ComposeOverlay(statistics, false, false);
    Require(content.above.front() == "FPS: 60.0", "60 presents a second compose as '" + content.above.front() + "'");
    return content;
}

std::vector<std::uint32_t> clearAndOverlay(const Context& context, PerformanceOverlay* overlay, VkFormat format, VkColorSpaceKHR colorSpace, VkExtent2D extent, const VkClearColorValue& clear, const OverlayContent& content, OverlayCorner corner, OverlayRect* bounds) {
    Image destination(context, format, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    Buffer readback(context, static_cast<std::size_t>(extent.width) * extent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        if (overlay != nullptr) {
            *bounds = overlay->Record(commands, 0, destination.Handle(), format, colorSpace, extent, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, content, corner);
        } else {
            transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        }
        readImage(context, commands, destination, readback);
        batch.SubmitAndWait();
    }
    if (overlay != nullptr) overlay->Release(0);
    return wordsOf(readback, extent);
}

void onlyInside(const std::vector<std::uint32_t>& reference, const std::vector<std::uint32_t>& drawn, VkExtent2D extent, const OverlayRect& bounds, const std::string& what, bool everyPixel = true) {
    std::size_t changed = 0;
    for (std::int32_t y = 0; y < static_cast<std::int32_t>(extent.height); ++y) {
        for (std::int32_t x = 0; x < static_cast<std::int32_t>(extent.width); ++x) {
            const auto index = static_cast<std::size_t>(y) * extent.width + static_cast<std::size_t>(x);
            if (!inside(bounds, x, y)) {
                Require(drawn[index] == reference[index], what + ": the overlay changed pixel " + std::to_string(x) + "," + std::to_string(y) + " outside its panel");
                continue;
            }
            if (drawn[index] != reference[index]) ++changed;
            else Require(!everyPixel, what + ": the overlay left pixel " + std::to_string(x) + "," + std::to_string(y) + " of its panel unchanged");
        }
    }
    Require(changed != 0, what + ": the overlay changed nothing");
}

int channel(std::uint32_t word, unsigned shift) {
    return static_cast<int>((word >> shift) & 0xffu);
}

void requireTextDrawn(const std::vector<std::uint32_t>& drawn, VkExtent2D extent, const OverlayRect& bounds, std::int32_t scale, std::uint32_t frameWord, const std::string& line, std::int32_t row, std::size_t first = 0, std::size_t count = std::string::npos) {
    const auto end = count == std::string::npos ? line.size() : std::min(line.size(), first + count);
    for (std::size_t index = first; index < end; ++index) {
        const auto& glyph = AgcDriver::OverlayGlyph(line[index]);
        const auto column = static_cast<std::int32_t>(index);
        for (std::int32_t gy = 0; gy < OverlayMetrics::GlyphHeight; ++gy) {
            for (std::int32_t gx = 0; gx < OverlayMetrics::Advance; ++gx) {
                const bool set = gx < OverlayMetrics::GlyphWidth && (glyph[static_cast<std::size_t>(gy)] & (0x10u >> gx)) != 0;
                for (std::int32_t py = 0; py < scale; ++py) {
                    for (std::int32_t px = 0; px < scale; ++px) {
                        const auto x = bounds.x + (OverlayMetrics::Padding + column * OverlayMetrics::Advance + gx) * scale + px;
                        const auto y = bounds.y + (row + gy) * scale + py;
                        const auto word = drawn[static_cast<std::size_t>(y) * extent.width + static_cast<std::size_t>(x)];
                        const auto where = std::string("'") + line[index] + "' of '" + line + "' glyph pixel " + std::to_string(gx) + "," + std::to_string(gy);
                        Require(channel(word, 24) == channel(frameWord, 24), where + " changed the alpha");
                        if (set) {
                            Require((word & 0x00ffffffu) == 0x00ffffffu, where + " is not white text: " + Hex(word));
                            continue;
                        }
                        for (const unsigned shift : {0u, 8u, 16u}) {
                            const int expected = static_cast<int>(std::lround(channel(frameWord, shift) * 0.4));
                            Require(std::abs(channel(word, shift) - expected) <= 1, where + " is not the panel over the frame: " + std::to_string(channel(word, shift)) + " for " + std::to_string(expected));
                        }
                    }
                }
            }
        }
    }
}

AgcDriver::OverlayDetails measuredDetails(const Context& context, VkExtent2D display, VkExtent2D window) {
    AgcDriver::OverlayDetails details;
    details.vram = AgcDriver::ReadVramBudget(context);
    details.ram = AgcDriver::ReadProcessPrivateBytes();
    details.display = display;
    details.window = window;
    return details;
}

AgcDriver::OverlayDetails fixedDetails(VkExtent2D display, VkExtent2D window) {
    constexpr double GiB = 1024.0 * 1024.0 * 1024.0;
    AgcDriver::OverlayDetails details;
    details.vram = AgcDriver::OverlayMemory{static_cast<std::uint64_t>(6.1 * GiB), static_cast<std::uint64_t>(7.8 * GiB)};
    details.ram = static_cast<std::uint64_t>(3.2 * GiB);
    details.display = display;
    details.window = window;
    return details;
}

OverlayContent fullPanel(const AgcDriver::OverlayDetails& details) {
    AgcDriver::OverlayStatistics statistics;
    const auto start = AgcDriver::OverlayStatistics::Clock::time_point{} + std::chrono::hours(1);
    for (std::uint64_t i = 0; i <= 60; ++i) {
        AgcDriver::OverlayCounters counters;
        counters.gpuNanoseconds = i * 4'200'000u;
        counters.driverNanoseconds = i * 1'300'000u;
        counters.draws = i * 1240;
        counters.dispatches = i * 85;
        counters.pipelines = i < 50 ? 1500 : 1503;
        statistics.AddPresent(start + std::chrono::nanoseconds(16'666'667) * i, counters);
    }
    auto content = AgcDriver::ComposeOverlay(statistics, true, true, details);
    Require(content.above.size() == 3 && content.above[0] == "PROFILED" && content.above[1] == "FPS: 60.0" && content.below.size() == 5u + (details.vram ? 1u : 0u) + (details.ram ? 1u : 0u) + (details.display.width != 0 ? 1u : 0u), "the full panel composes as '" + content.above.front() + "' with " + std::to_string(content.below.size()) + " lines below its graph");
    return content;
}

void sdrOverlayTest(const Context& context, PerformanceOverlay& overlay) {
    const VkExtent2D extent{1280, 720};
    VkClearColorValue clear{};
    clear.float32[0] = 0.75f;
    clear.float32[1] = 0.5f;
    clear.float32[2] = 0.25f;
    clear.float32[3] = 1.0f;
    const auto content = sixtyFps();
    OverlayRect bounds{};
    const auto reference = clearAndOverlay(context, nullptr, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, clear, content, OverlayCorner::TopLeft, nullptr);
    const auto drawn = clearAndOverlay(context, &overlay, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, clear, content, OverlayCorner::TopLeft, &bounds);
    Require(bounds.x == AgcDriver::LayOutOverlay(content, extent, OverlayCorner::TopLeft).bounds.x && bounds.width > 0 && bounds.height > 0, "the overlay drew another panel than its layout");
    onlyInside(reference, drawn, extent, bounds, "SDR 1280x720");
    const auto scale = AgcDriver::OverlayScale(extent, content);
    const std::string digits = "60.0";
    const auto firstColumn = content.above.front().find(digits);
    Require(firstColumn != std::string::npos, "the FPS line has no '60.0'");
    requireTextDrawn(drawn, extent, bounds, scale, reference.front(), content.above.front(), OverlayMetrics::Padding, firstColumn, digits.size());
    std::printf("SDR overlay: panel %dx%d at %d,%d (scale %d), FPS digits drawn bit for bit\n", bounds.width, bounds.height, bounds.x, bounds.y, scale);
}

void hdrOverlayTest(const Context& context, PerformanceOverlay& overlay) {
    const auto format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) == 0) {
        std::printf("A2B10G10R10 cannot be blended into here; HDR10 overlay not exercised\n");
        return;
    }
    const VkExtent2D extent{1920, 1080};
    VkClearColorValue clear{};
    clear.float32[0] = 0.4f;
    clear.float32[1] = 0.5f;
    clear.float32[2] = 0.6f;
    clear.float32[3] = 1.0f;
    const auto content = sixtyFps();
    OverlayRect bounds{};
    const auto reference = clearAndOverlay(context, nullptr, format, VK_COLOR_SPACE_HDR10_ST2084_EXT, extent, clear, content, OverlayCorner::BottomRight, nullptr);
    const auto drawn = clearAndOverlay(context, &overlay, format, VK_COLOR_SPACE_HDR10_ST2084_EXT, extent, clear, content, OverlayCorner::BottomRight, &bounds);
    onlyInside(reference, drawn, extent, bounds, "HDR10 1920x1080");
    const auto scale = AgcDriver::OverlayScale(extent, content);
    const auto& six = AgcDriver::OverlayGlyph('6');
    const auto column = static_cast<std::int32_t>(content.above.front().find('6'));
    std::size_t checked = 0;
    for (std::int32_t gy = 0; gy < OverlayMetrics::GlyphHeight; ++gy) {
        for (std::int32_t gx = 0; gx < OverlayMetrics::GlyphWidth; ++gx) {
            if ((six[static_cast<std::size_t>(gy)] & (0x10u >> gx)) == 0) continue;
            const auto x = bounds.x + (OverlayMetrics::Padding + column * OverlayMetrics::Advance + gx) * scale + scale / 2;
            const auto y = bounds.y + (OverlayMetrics::Padding + gy) * scale + scale / 2;
            const auto word = drawn[static_cast<std::size_t>(y) * extent.width + static_cast<std::size_t>(x)];
            for (const unsigned shift : {0u, 10u, 20u}) {
                const int code = static_cast<int>((word >> shift) & 0x3ffu);
                Require(std::abs(code - 594) <= 1, "HDR10 text is not at 203 nits: PQ code " + std::to_string(code) + " for 594");
            }
            Require((word >> 30u) == (reference.front() >> 30u), "HDR10 text changed the alpha");
            ++checked;
        }
    }
    Require(checked != 0, "no HDR10 text pixel was checked");
    std::printf("HDR10 overlay: text at PQ code 594 (203 nits), nothing changed outside the %dx%d panel\n", bounds.width, bounds.height);
}

void cornerTest(const Context& context, PerformanceOverlay& overlay) {
    const VkExtent2D extent{640, 360};
    VkClearColorValue clear{};
    clear.float32[1] = 1.0f;
    clear.float32[3] = 1.0f;
    const auto content = sixtyFps();
    const auto reference = clearAndOverlay(context, nullptr, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, clear, content, OverlayCorner::TopLeft, nullptr);
    for (const auto corner : {OverlayCorner::TopLeft, OverlayCorner::TopRight, OverlayCorner::BottomLeft, OverlayCorner::BottomRight}) {
        OverlayRect bounds{};
        const auto drawn = clearAndOverlay(context, &overlay, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, clear, content, corner, &bounds);
        const auto expected = AgcDriver::LayOutOverlay(content, extent, corner).bounds;
        Require(bounds.x == expected.x && bounds.y == expected.y && bounds.width == expected.width && bounds.height == expected.height, "the drawn panel is not where the layout puts it");
        onlyInside(reference, drawn, extent, bounds, "corner " + std::to_string(static_cast<int>(corner)));
    }
    std::printf("overlay corners: tl, tr, bl and br drawn where the layout puts them\n");
}

void fullPanelTest(const Device& device, PerformanceOverlay& overlay) {
    const auto& context = device.GetContext();
    const VkExtent2D extent{1920, 1080};
    auto details = measuredDetails(context, extent, extent);
    Require(details.ram.has_value(), "the process's private memory cannot be read");
    Require(details.vram.has_value() == device.MemoryBudget(), device.MemoryBudget() ? "VK_EXT_memory_budget gives no device-local budget" : "VRAM is read without VK_EXT_memory_budget");
    auto withoutBudget = context;
    withoutBudget.memoryProperties2 = nullptr;
    Require(!AgcDriver::ReadVramBudget(withoutBudget).has_value(), "VRAM is read without vkGetPhysicalDeviceMemoryProperties2");
    VkClearColorValue clear{};
    clear.float32[0] = 0.75f;
    clear.float32[1] = 0.5f;
    clear.float32[2] = 0.25f;
    clear.float32[3] = 1.0f;
    const auto content = fullPanel(details);
    OverlayRect bounds{};
    const auto reference = clearAndOverlay(context, nullptr, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, clear, content, OverlayCorner::TopLeft, nullptr);
    const auto drawn = clearAndOverlay(context, &overlay, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, clear, content, OverlayCorner::TopLeft, &bounds);
    const auto layout = AgcDriver::LayOutOverlay(content, extent, OverlayCorner::TopLeft);
    Require(bounds.x == layout.bounds.x && bounds.y == layout.bounds.y && bounds.width == layout.bounds.width && bounds.height == layout.bounds.height, "the full panel is not drawn where its layout puts it");
    onlyInside(reference, drawn, extent, bounds, "full panel 1920x1080");
    std::int32_t row = OverlayMetrics::Padding;
    for (const auto& line : content.above) {
        requireTextDrawn(drawn, extent, bounds, layout.scale, reference.front(), line, row);
        row += OverlayMetrics::LinePitch;
    }
    row += OverlayMetrics::GraphHeight + OverlayMetrics::GraphGap;
    for (const auto& line : content.below) {
        requireTextDrawn(drawn, extent, bounds, layout.scale, reference.front(), line, row);
        row += OverlayMetrics::LinePitch;
    }
    std::printf("full panel at 1920x1080: %dx%d at %d,%d (scale %d, %.1f%% of the height), every line drawn bit for bit, nothing changed outside it:\n", bounds.width, bounds.height, bounds.x, bounds.y, layout.scale, 100.0 * bounds.height / extent.height);
    for (const auto& line : content.above) std::printf("  %s\n", line.c_str());
    std::printf("  (graph of %zu frame times)\n", content.frameTimes.size());
    for (const auto& line : content.below) std::printf("  %s\n", line.c_str());
}

AgcDriver::OverlayDetailSample pageTwoSample() {
    AgcDriver::OverlayDetailSample sample;
    sample.fps = 59.94;
    sample.frameMs = 16.7;
    sample.busy = {{"Q0", 63}, {"Q20", 4}, {"COMMIT", 12}, {"FINISH", 3}, {"PRESENT", 6}};
    sample.phases = {{"Driver.Packet", 3.2}, {"Vulkan.GraphicsPipeline.pipeline_create", 1.5}, {"Recorder.Sync", 0.4}};
    sample.queueWaits = AgcDriver::OverlayQueueWaits{0.4, 0.2, 1.3};
    sample.renderPassesPerFrame = 12;
    sample.pipelinesCompiling = 3;
    sample.pipelinesPerSecond = 12;
    sample.drawCacheHitPercent = 97.1;
    sample.pipelineCacheHitPercent = 99.9;
    sample.flipsPerSecond = 60;
    sample.presentsPerSecond = 59.94;
    sample.droppedFrames = 0;
    sample.repeatedPresents = 0;
    sample.commit = AgcDriver::OverlayMemory{12ull << 30u, 31ull << 30u};
    sample.pageFaultsPerSecond = 1234;
    sample.hostImports = 123;
    sample.hostImportBytes = AgcDriver::OverlayMemory{1ull << 30u, 6ull << 30u};
    sample.cpuCopyMegabytesPerSecond = 12.3;
    sample.addressCopyCapPercent = 3;
    sample.unimplementedPerSecond = 0;
    sample.unimplementedTotal = 4;
    sample.refusedShaders = 2;
    sample.title = "PPSA01234";
    sample.presentMode = "fifo";
    return sample;
}

void pageTwoTest(const Context& context, PerformanceOverlay& overlay) {
    const VkExtent2D extent{1280, 720};
    VkClearColorValue clear{};
    clear.float32[0] = 0.25f;
    clear.float32[1] = 0.5f;
    clear.float32[2] = 0.75f;
    clear.float32[3] = 1.0f;
    const auto content = AgcDriver::ComposeOverlayDetails(pageTwoSample());
    OverlayRect bounds{};
    const auto reference = clearAndOverlay(context, nullptr, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, clear, content, OverlayCorner::TopRight, nullptr);
    const auto drawn = clearAndOverlay(context, &overlay, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, clear, content, OverlayCorner::TopRight, &bounds);
    const auto layout = AgcDriver::LayOutOverlay(content, extent, OverlayCorner::TopRight);
    Require(bounds.x == layout.bounds.x && bounds.y == layout.bounds.y && bounds.width == layout.bounds.width && bounds.height == layout.bounds.height, "page 2 is not drawn where its layout puts it");
    onlyInside(reference, drawn, extent, bounds, "page 2 at 1280x720");
    std::int32_t row = OverlayMetrics::Padding;
    for (const auto& line : content.below) {
        requireTextDrawn(drawn, extent, bounds, layout.scale, reference.front(), line, row);
        row += OverlayMetrics::LinePitch;
    }
    std::printf("page 2 at 1280x720: %dx%d at %d,%d (scale %d), every line drawn bit for bit, nothing changed outside it:\n", bounds.width, bounds.height, bounds.x, bounds.y, layout.scale);
    for (const auto& line : content.below) std::printf("  |%-46s|\n", line.c_str());
}

void pageCycleTest(const Device& device) {
    const auto& context = device.GetContext();
    const VkExtent2D extent{1280, 720};
    Require(PerformanceOverlay::Page() == 0 && !PerformanceOverlay::Visible(), "the performance overlay does not start hidden without ANYPS5_OVERLAY");
    PerformanceOverlay overlay(context, 1);
    const auto start = AgcDriver::OverlayStatistics::Clock::now();
    const auto present = [&](int frame) { overlay.NotePresent(start + std::chrono::milliseconds(16) * frame, VkExtent2D{640, 360}); };
    const auto record = [&] {
        Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        OverlayRect bounds{};
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        bounds = overlay.Record(commands, 0, destination.Handle(), VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        batch.SubmitAndWait();
        overlay.Release(0);
        return bounds;
    };
    PerformanceOverlay::Toggle();
    Require(PerformanceOverlay::Page() == 1 && PerformanceOverlay::Visible(), "the first F10 does not show page 1");
    for (int frame = 0; frame < 5; ++frame) present(frame);
    Require(overlay.ShownPage() == 1 && overlay.Statistics().FrameTimes().size() == 4, "page 1 did not keep its frames");
    const auto pageOne = record();
    PerformanceOverlay::Toggle();
    Require(PerformanceOverlay::Page() == 2 && PerformanceOverlay::Visible(), "the second F10 does not show page 2");
    present(5);
    Require(overlay.ShownPage() == 2 && overlay.Statistics().FrameTimes().empty(), "changing to page 2 kept page 1's statistics");
    for (int frame = 6; frame < 10; ++frame) present(frame);
    const auto& sample = overlay.DetailSample();
    Require(std::abs(sample.frameMs - 16.0) < 1e-6 && sample.presentsPerSecond.has_value() && sample.pipelinesPerSecond.has_value(), "page 2 did not sample its frames");
    const auto pageTwo = record();
    const auto expected = AgcDriver::LayOutOverlay(AgcDriver::ComposeOverlayDetails(sample), extent, PerformanceOverlay::Corner()).bounds;
    Require(pageTwo.width == expected.width && pageTwo.height == expected.height && pageTwo.width > pageOne.width, "page 2 is not drawn from its own composer: " + std::to_string(pageTwo.width) + "x" + std::to_string(pageTwo.height));
    PerformanceOverlay::Toggle();
    Require(PerformanceOverlay::Page() == 0 && !PerformanceOverlay::Visible(), "the third F10 does not hide the overlay");
    present(10);
    Require(overlay.ShownPage() == 0 && overlay.Statistics().FrameTimes().empty() && overlay.DetailSample().fps == 0, "hiding the overlay kept page 2's state");
    std::printf("F10 cycle: off, page 1 (%dx%d), page 2 (%dx%d), off; each change of page resets the statistics\n", pageOne.width, pageOne.height, pageTwo.width, pageTwo.height);
}

AgcDriver::OverlayDriverCounters fakeDriverCounters() {
    static std::uint64_t calls = 0;
    ++calls;
    return AgcDriver::OverlayDriverCounters{calls * 66, AgcDriver::OverlayHits{calls * 1000, calls * 900}};
}

void pageTwoSamplerTest(const Device& device) {
    using AgcDriver::PerformanceTrace;
    using namespace std::chrono_literals;
    const auto& context = device.GetContext();
    const VkExtent2D extent{1280, 720};
    Require(PerformanceOverlay::Page() == 0 && !PerformanceTrace::Recording(), "the trace records before page 2 is shown");
    PerformanceOverlay overlay(context, 1);
    PerformanceOverlay::SetDriverCounters(&fakeDriverCounters);
    PerformanceOverlay::Toggle();
    Require(!PerformanceTrace::Recording(), "page 1 turns the trace on");
    PerformanceOverlay::Toggle();
    Require(PerformanceOverlay::Page() == 2 && PerformanceTrace::Recording() && PerformanceTrace::OverlayTotals(), "page 2 does not turn the trace totals on");
    const auto start = AgcDriver::OverlayStatistics::Clock::now();
    const auto present = [&](std::chrono::milliseconds at, std::uint64_t address, std::uint64_t drops) { overlay.NotePresent(start + at, VkExtent2D{640, 360}, AgcDriver::OverlayPresentInfo{address, VK_PRESENT_MODE_FIFO_KHR, drops}); };
    present(0ms, 0x1000, 7);
    const auto& sample = overlay.DetailSample();
    Require(sample.commit.has_value() && sample.unimplementedTotal.has_value() && sample.presentMode == "fifo" && sample.pipelinesCompiling == std::optional<std::uint64_t>{0}, "page 2's first sample does not read the commit charge, the unimplemented calls, the compile backlog and the present mode");
    Require(!sample.flipsPerSecond && !sample.pageFaultsPerSecond && !sample.cpuCopyMegabytesPerSecond && sample.busy.empty() && sample.phases.empty(), "page 2's first sample shows rates before a second has passed");
    Require(sample.hostImports.has_value() == (context.hostImportAlignment != 0), "the host import line does not follow the device's host import support");
    std::thread([] {
        PerformanceTrace::BindThread("queue worker 0x0");
        if (auto* trace = PerformanceTrace::Thread()) {
            const auto begin = std::chrono::steady_clock::now();
            trace->Add("Driver.Packet", nullptr, begin, begin + 110ms);
        }
        for (int pass = 0; pass < 6; ++pass) PerformanceTrace::Count(PerformanceTrace::RenderPassScope);
        AgcDriver::Graphics::AddressCopyCounters::ThisThread().Count(std::uint64_t{11} << 20u);
    }).join();
    try {
        NotImplemented_nid_no_patch("page 2 sampler test");
    } catch (const std::runtime_error&) {
    }
    present(16ms, 0x1000, 7);
    present(33ms, 0x2000, 8);
    present(1100ms, 0x2000, 9);
    const auto near = [](const std::optional<double>& value, double expected) { return value.has_value() && std::abs(*value - expected) < 1e-6; };
    Require(near(sample.flipsPerSecond, 60) && near(sample.drawCacheHitPercent, 90), "66 flips and 900 of 1000 draw-cache lookups over 1.1 s are not 60 flips/s and 90 %");
    Require(sample.droppedFrames == std::optional<std::uint64_t>{2} && sample.repeatedPresents == std::optional<std::uint64_t>{2}, "2 dropped and 2 repeated presents over 1.1 s are not shown as 2/s each");
    Require(sample.pageFaultsPerSecond.has_value() && near(sample.unimplementedPerSecond, 1 / 1.1), "the page faults or the unimplemented calls per second are missing");
    Require(near(sample.cpuCopyMegabytesPerSecond, 10), "11 MiB of address copies over 1.1 s are not 10 MB/s");
    if (std::getenv("APS5_ADDRESS_COPY_MAX_MIB") == nullptr) Require(near(sample.addressCopyCapPercent, 100.0 * 11 / 64), "an 11 MiB build is not 17.2 % of the 64 MiB cap");
    Require(near(sample.renderPassesPerFrame, 2), "6 render passes over 3 frames are not 2 per frame");
    const auto queue = std::find_if(sample.busy.begin(), sample.busy.end(), [](const AgcDriver::OverlayThreadBusy& busy) { return busy.name == "Q0"; });
    Require(queue != sample.busy.end() && std::abs(queue->percent - 10) < 1e-6 && sample.queueWaits.has_value(), "110 ms of packets over 1.1 s do not make queue 0 10 % busy");
    Require(!sample.phases.empty() && sample.phases.front().name == "Driver.Packet" && std::abs(sample.phases.front().msPerFrame - 110.0 / 3) < 1e-6, "the top phase is not 110 ms of packets over 3 frames");
    Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    const auto bounds = overlay.Record(commands, 0, destination.Handle(), VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    batch.SubmitAndWait();
    overlay.Release(0);
    const auto content = AgcDriver::ComposeOverlayDetails(sample);
    const auto expected = AgcDriver::LayOutOverlay(content, extent, PerformanceOverlay::Corner()).bounds;
    Require(bounds.width == expected.width && bounds.height == expected.height, "page 2 is not drawn from its sample");
    std::printf("page 2 from its sampler (fake driver counters, a 'queue worker 0x0' thread, this process's OS counters):\n");
    for (const auto& line : content.below) std::printf("  |%-46s|\n", line.c_str());
    PerformanceOverlay::Toggle();
    Require(PerformanceOverlay::Page() == 0 && !PerformanceTrace::Recording() && !PerformanceTrace::OverlayTotals(), "hiding the overlay leaves the trace totals on");
    present(1200ms, 0x3000, 9);
    Require(overlay.ShownPage() == 0 && overlay.DetailSample().fps == 0 && overlay.DetailSample().busy.empty() && !overlay.DetailSample().commit, "hiding the overlay kept page 2's sample");
    PerformanceOverlay::SetDriverCounters(nullptr);
}

void presentCostReport(const Device& device) {
    using AgcDriver::PerformanceCounters;
    using AgcDriver::PerformanceTrace;
    using Clock = AgcDriver::OverlayStatistics::Clock;
    const auto& context = device.GetContext();
    const VkExtent2D extent{1920, 1080};
    Require(PerformanceOverlay::Page() == 0, "the cost report does not start hidden");
    PerformanceOverlay overlay(context, 1);
    const auto start = Clock::now();
    const auto perCall = [](int calls, auto&& body) {
        const auto began = std::chrono::steady_clock::now();
        for (int call = 0; call < calls; ++call) body(call);
        return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - began).count() / calls;
    };
    volatile bool shown = false;
    const auto hiddenNs = perCall(10'000'000, [&](int) { shown = PerformanceOverlay::Visible(); });
    const auto passNs = perCall(10'000'000, [](int) { PerformanceTrace::Count(PerformanceTrace::RenderPassScope); });
    auto& copies = AgcDriver::Graphics::AddressCopyCounters::ThisThread();
    const auto copyNs = perCall(10'000'000, [&](int) { copies.Count(4096); });
    PerformanceOverlay::Toggle();
    const auto pageOneNs = perCall(20'000, [&](int frame) {
        overlay.NotePresent(start + std::chrono::microseconds(16667) * frame, extent);
        static_cast<void>(AgcDriver::LayOutOverlay(AgcDriver::ComposeOverlay(overlay.Statistics(), PerformanceCounters::GpuEnabled(), PerformanceCounters::DriverEnabled(), overlay.Details()), extent, PerformanceOverlay::Corner()));
    });
    PerformanceOverlay::Toggle();
    const auto later = start + std::chrono::hours(1);
    const auto pageTwoNs = perCall(20'000, [&](int frame) {
        overlay.NotePresent(later + std::chrono::microseconds(16667) * frame, extent, AgcDriver::OverlayPresentInfo{0x1000u + static_cast<std::uint64_t>(frame % 2), VK_PRESENT_MODE_FIFO_KHR, 0});
        static_cast<void>(AgcDriver::LayOutOverlay(AgcDriver::ComposeOverlayDetails(overlay.DetailSample()), extent, PerformanceOverlay::Corner()));
    });
    const auto sampled = later + std::chrono::hours(1);
    const auto sampleNs = perCall(200, [&](int second) { overlay.NotePresent(sampled + std::chrono::seconds(second), extent, AgcDriver::OverlayPresentInfo{0x1000, VK_PRESENT_MODE_FIFO_KHR, 0}); });
    PerformanceOverlay::Toggle();
    Require(!PerformanceOverlay::Visible() && !PerformanceTrace::Recording(), "the cost report does not end hidden");
    std::printf("present cost on this host: hidden %.1f ns per present (Visible), %.1f ns per render pass begun (the trace check), %.1f ns per address-copying build (the per-thread counter); page 1 %.2f us per present (NotePresent, compose, layout); page 2 %.2f us per present at 60 fps (NotePresent with its once-a-second sample, compose, layout), %.1f us per present that samples\n", hiddenNs, passNs, copyNs, pageOneNs / 1000, pageTwoNs / 1000, sampleNs / 1000);
}

void notePresentTest(const Device& device) {
    const auto& context = device.GetContext();
    const VkExtent2D extent{1280, 720};
    PerformanceOverlay overlay(context, 1);
    overlay.NotePresent(AgcDriver::OverlayStatistics::Clock::now(), VkExtent2D{640, 360});
    const auto& details = overlay.Details();
    Require(details.ram.has_value() && details.vram.has_value() == device.MemoryBudget() && details.display.width == 640 && details.display.height == 360, "a shown frame did not sample the memory or keep the display buffer size");
    Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    OverlayRect bounds{};
    {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        bounds = overlay.Record(commands, 0, destination.Handle(), VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        batch.SubmitAndWait();
    }
    overlay.Release(0);
    auto shown = details;
    shown.window = extent;
    const auto expected = AgcDriver::ComposeOverlay(overlay.Statistics(), AgcDriver::PerformanceCounters::GpuEnabled(), AgcDriver::PerformanceCounters::DriverEnabled(), shown);
    Require(expected.below.back() == "RES: 640X360 / 1280X720", "the RES line reads '" + expected.below.back() + "'");
    const auto layout = AgcDriver::LayOutOverlay(expected, extent, PerformanceOverlay::Corner());
    Require(bounds.height == layout.bounds.height && bounds.width == layout.bounds.width, "a shown frame's panel does not hold the sampled VRAM, RAM and RES lines");
    std::printf("shown frame: memory sampled, %zu lines below the graph, the last '%s'\n", expected.below.size(), expected.below.back().c_str());
}

class Presenter {
public:
    explicit Presenter(const Context& context) : context(context), pass(context, 1), overlay(context, 1), source(context, VK_FORMAT_B8G8R8A8_UNORM, {256, 144}, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) {
        Buffer upload(context, static_cast<std::size_t>(256) * 144 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::uint32_t seed = 0x2545f491u;
        for (auto& byte : upload.Bytes()) {
            seed = seed * 1664525u + 1013904223u;
            byte = static_cast<std::byte>(seed >> 24u);
        }
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        transition(context, commands, source.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {256, 144, 1};
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, upload.Handle(), source.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        transition(context, commands, source.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        batch.SubmitAndWait();
    }

    std::vector<std::uint32_t> Drawn(OverlayRect* bounds) {
        Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, Extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        Buffer readback(context, static_cast<std::size_t>(Extent.width) * Extent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const bool visible = PerformanceOverlay::Visible();
        pass.Bind(0, source.Handle(), 256, 144, Bgra8, AgcDriver::SdrToneMapLuminanceNits);
        {
            CommandBatch batch(context);
            const auto commands = batch.Handle();
            pass.RecordPresent(commands, 0, destination.Handle(), VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, Extent, visible ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            if (visible) *bounds = overlay.Record(commands, 0, destination.Handle(), VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, Extent, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fullPanel(fixedDetails({256, 144}, Extent)), OverlayCorner::TopLeft);
            readImage(context, commands, destination, readback);
            batch.SubmitAndWait();
        }
        pass.Release(0);
        overlay.Release(0);
        return wordsOf(readback, Extent);
    }

    std::vector<std::uint32_t> Blitted(OverlayRect* bounds) {
        Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, Extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        Buffer readback(context, static_cast<std::size_t>(Extent.width) * Extent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const bool visible = PerformanceOverlay::Visible();
        {
            CommandBatch batch(context);
            const auto commands = batch.Handle();
            transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
            transition(context, commands, source.Handle(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkClearColorValue black{};
            black.float32[3] = 1.0f;
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
            transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            AgcDriver::PresentationScaler::RecordBlitFrom(context, commands, source.Handle(), VK_IMAGE_LAYOUT_GENERAL, 256, 144, VK_FILTER_LINEAR, destination.Handle(), Extent.width, Extent.height);
            if (visible) {
                *bounds = overlay.Record(commands, 0, destination.Handle(), VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, Extent, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fullPanel(fixedDetails({256, 144}, Extent)), OverlayCorner::TopLeft);
            } else {
                transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            }
            transition(context, commands, source.Handle(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
            readImage(context, commands, destination, readback);
            batch.SubmitAndWait();
        }
        overlay.Release(0);
        return wordsOf(readback, Extent);
    }

    static constexpr VkExtent2D Extent{400, 300};

private:
    const Context& context;
    PresentationPass pass;
    PerformanceOverlay overlay;
    Image source;
};

void disabledIsUnchangedTest(const Context& context) {
    Require(!PerformanceOverlay::Visible(), "the performance overlay is shown without ANYPS5_OVERLAY=1");
    Presenter presenter(context);
    OverlayRect unused{};
    const auto drawnBefore = presenter.Drawn(&unused);
    const auto blittedBefore = presenter.Blitted(&unused);
    PerformanceOverlay::Toggle();
    Require(PerformanceOverlay::Visible(), "F10 does not show the performance overlay");
    OverlayRect bounds{};
    onlyInside(drawnBefore, presenter.Drawn(&bounds), Presenter::Extent, bounds, "presentation pass with the overlay", false);
    onlyInside(blittedBefore, presenter.Blitted(&bounds), Presenter::Extent, bounds, "blit with the overlay", false);
    PerformanceOverlay::Toggle();
    Require(PerformanceOverlay::Visible() && PerformanceOverlay::Page() == 2, "a second F10 does not show page 2");
    onlyInside(drawnBefore, presenter.Drawn(&bounds), Presenter::Extent, bounds, "presentation pass with page 2", false);
    PerformanceOverlay::Toggle();
    Require(!PerformanceOverlay::Visible(), "a third F10 does not hide the performance overlay again");
    Require(presenter.Drawn(&unused) == drawnBefore, "the presentation pass output differs after the overlay was hidden");
    Require(presenter.Blitted(&unused) == blittedBefore, "the blit output differs after the overlay was hidden");
    std::printf("overlay hidden: presentation pass and blit output bit-identical before and after showing it\n");
}

void costReport(const Device& device, PerformanceOverlay& overlay) {
    const auto& context = device.GetContext();
    if (device.TimestampBits() == 0) {
        std::printf("the graphics queue has no timestamps; overlay cost not measured\n");
        return;
    }
    VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = 2;
    VkQueryPool queries = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(context.device, &info, nullptr, &queries), "vkCreateQueryPool overlay cost");
    const auto sampled = std::chrono::steady_clock::now();
    for (int run = 0; run < 100; ++run) static_cast<void>(AgcDriver::ReadVramBudget(context));
    const auto vramUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - sampled).count() / 100;
    std::printf("VRAM budget read in %.1f us (%s), once a second while the overlay is shown\n", vramUs, device.MemoryBudget() ? "VK_EXT_memory_budget" : "no VK_EXT_memory_budget");
    for (const VkExtent2D extent : {VkExtent2D{1920, 1080}, VkExtent2D{3840, 2160}}) {
        Image destination(context, VK_FORMAT_B8G8R8A8_UNORM, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        const auto content = fullPanel(fixedDetails(extent, extent));
        std::vector<double> gpuMs;
        std::vector<double> cpuMs;
        for (int run = 0; run < 12; ++run) {
            CommandBatch batch(context);
            const auto commands = batch.Handle();
            context.Function<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(commands, queries, 0, 2);
            transition(context, commands, destination.Handle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
            const auto recordStart = std::chrono::steady_clock::now();
            static_cast<void>(overlay.Record(commands, 0, destination.Handle(), VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, content, OverlayCorner::TopLeft));
            cpuMs.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - recordStart).count());
            context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
            batch.SubmitAndWait();
            overlay.Release(0);
            std::array<std::uint64_t, 2> stamps{};
            Check(context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(context.device, queries, 0, 2, sizeof(stamps), stamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "vkGetQueryPoolResults overlay cost");
            if (run >= 2) gpuMs.push_back(static_cast<double>(stamps[1] - stamps[0]) * context.limits.timestampPeriod / 1e6);
        }
        std::sort(gpuMs.begin(), gpuMs.end());
        std::sort(cpuMs.begin(), cpuMs.end());
        std::printf("full panel cost on %s at %ux%u: GPU median %.3f ms (min %.3f, max %.3f), recording median %.3f ms\n", device.Name().c_str(), extent.width, extent.height, gpuMs[gpuMs.size() / 2], gpuMs.front(), gpuMs.back(), cpuMs[cpuMs.size() / 2]);
    }
    context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, queries, nullptr);
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "";
        Require(argc <= 2 && (mode.empty() || mode == "device-selection" || mode == "device-selection-cpu"), "unknown performance-overlay test mode");
        if (mode == "device-selection-cpu") {
            deviceSelectionTests();
            return 0;
        }
        setEnvironment("ANYPS5_OVERLAY", "");
        setEnvironment("ANYPS5_OVERLAY_POS", "");
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            const char* requestedName = std::getenv("ANYPS5_GPU");
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr || (requestedName != nullptr && *requestedName != '\0')) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        device->VerifySelection();
        if (mode == "device-selection") return 0;
        const auto& context = device->GetContext();
        {
            PerformanceOverlay overlay(context, 1);
            sdrOverlayTest(context, overlay);
            hdrOverlayTest(context, overlay);
            cornerTest(context, overlay);
            fullPanelTest(*device, overlay);
            pageTwoTest(context, overlay);
            costReport(*device, overlay);
        }
        notePresentTest(*device);
        pageCycleTest(*device);
        pageTwoSamplerTest(*device);
        presentCostReport(*device);
        disabledIsUnchangedTest(context);
        std::cout << "performance overlay tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
