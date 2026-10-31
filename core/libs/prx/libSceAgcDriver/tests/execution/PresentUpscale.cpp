#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::VulkanDevice;
constexpr std::uint64_t DisplayFormat = 0x8000000000000000ull;
constexpr std::size_t PlaneOffset = 40u * 65536u;
constexpr std::size_t BlockBytes = 80u * 65536u;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "FSR with an overlay plane: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

struct Headless {
    std::uint32_t width;
    std::uint32_t height;
    PFN_vkGetInstanceProcAddr resolve;
};

VkSurfaceKHR createSurface(void* context, VkInstance instance) {
    auto* headless = static_cast<Headless*>(context);
    const auto create = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(headless->resolve(instance, "vkCreateHeadlessSurfaceEXT"));
    Require(create != nullptr, "vkCreateHeadlessSurfaceEXT is unavailable");
    VkHeadlessSurfaceCreateInfoEXT info{VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT};
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Require(create(instance, &info, nullptr, &surface) == VK_SUCCESS, "vkCreateHeadlessSurfaceEXT failed");
    return surface;
}

void drawableSize(void* context, std::uint32_t* width, std::uint32_t* height) {
    const auto* headless = static_cast<const Headless*>(context);
    *width = headless->width;
    *height = headless->height;
}

bool headlessSurfaces(PFN_vkGetInstanceProcAddr resolve) {
    const auto enumerate = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(resolve(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
    if (enumerate == nullptr) return false;
    std::uint32_t count = 0;
    if (enumerate(nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> extensions(count);
    if (enumerate(nullptr, &count, extensions.data()) != VK_SUCCESS) return false;
    for (const auto& extension : extensions) {
        if (std::strcmp(extension.extensionName, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0) return true;
    }
    return false;
}

void setEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void setUpscale(const char* value) {
    setEnvironment("ANYPS5_UPSCALE", value);
}

void expectUpscaled(VulkanDevice& device, std::uint32_t width, std::uint32_t height, bool upscaled, const std::string& failure) {
    const auto before = VulkanDevice::UpscaledPresents();
    const std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height * 4, std::byte{0x40});
    device.PresentPixels(width, height, pixels);
    Require(VulkanDevice::UpscaledPresents() - before == (upscaled ? 1u : 0u), failure);
}

void presentWithOverlay(const AgcDriver::PresentationWindow& window, bool fsr, bool overlay) {
    setUpscale(fsr ? "fsr" : "");
    if (overlay) AgcDriver::PerformanceOverlay::Toggle();
    Require(AgcDriver::PerformanceOverlay::Visible() == overlay, "the performance overlay did not toggle");
    {
        VulkanDevice device(&window);
        const std::string combination = std::string(fsr ? "FSR" : "no FSR") + (overlay ? " with the overlay" : " without the overlay");
        for (int frame = 0; frame < 3; ++frame) expectUpscaled(device, 960, 540, fsr, combination + ": a 960x540 display buffer in a 1084x610 window was " + (fsr ? "not upscaled" : "upscaled"));
    }
    for (int press = 0; overlay && press < 4 && AgcDriver::PerformanceOverlay::Visible(); ++press) AgcDriver::PerformanceOverlay::Toggle();
    Require(!AgcDriver::PerformanceOverlay::Visible(), "the performance overlay did not hide again");
}

void presentWithPlane(const AgcDriver::PresentationWindow& window) {
    setUpscale("fsr");
    GuestBlock block;
    const AgcDriver::DisplayBuffer main{AddressOf(block.Data()), DisplayFormat, 960, 540};
    const AgcDriver::DisplayBuffer plane{AddressOf(block.Data() + PlaneOffset), DisplayFormat, 960, 540};
    Require(AgcDriver::DisplayBufferSize(main) <= PlaneOffset && PlaneOffset + AgcDriver::DisplayBufferSize(plane) <= BlockBytes, "FSR with an overlay plane: the display buffers do not fit the guest block");
    std::memset(block.Data(), 0x40, PlaneOffset);
    std::memset(block.Data() + PlaneOffset, 0x80, AgcDriver::DisplayBufferSize(plane));
    AgcDriver::GuestMemory::BumpCollectEpoch();
    VulkanDevice device(&window);
    const auto upscaled = VulkanDevice::UpscaledPresents();
    const auto planes = VulkanDevice::PresentCounts().planes;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(device.PresentDisplayBuffer(main, &plane), "FSR with an overlay plane: the present was not submitted");
        device.FinishPresent();
        if (!device.PresentsOnOwnQueue()) device.QueuePresent();
    }
    if (device.PresentsOnOwnQueue()) device.QueuePresent();
    Require(VulkanDevice::UpscaledPresents() - upscaled == 1u, "FSR with an overlay plane: a 960x540 display buffer in a 1084x610 window was not upscaled");
    Require(VulkanDevice::PresentCounts().planes - planes == 1u, "FSR with an overlay plane: the plane was not composited over the upscaled output");
}

}

int main() {
    try {
#ifdef _WIN32
        void* library = SDL_LoadObject("vulkan-1.dll");
#else
        void* library = SDL_LoadObject("libvulkan.so.1");
#endif
        const auto resolve = library != nullptr ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr")) : nullptr;
        if (resolve == nullptr || !headlessSurfaces(resolve)) {
            std::printf("skipped, no Vulkan loader with %s\n", VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
            return VulkanTestSkipped;
        }
        Headless headless{1084, 610, resolve};
        const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME};
        const AgcDriver::PresentationWindow window{&headless, extensions, createSurface, drawableSize, 3840, 2160, nullptr};
        setEnvironment("ANYPS5_OVERLAY", "");
        setEnvironment("ANYPS5_OVERLAY_POS", "");
        for (const bool fsr : {false, true}) {
            for (const bool overlay : {false, true}) presentWithOverlay(window, fsr, overlay);
        }
        setUpscale("fsr");
        {
            VulkanDevice device(&window);
            expectUpscaled(device, 3840, 2160, false, "FSR ran on a 3840x2160 display buffer in a 1084x610 window, where it cannot upscale");
            expectUpscaled(device, 1084, 610, false, "FSR ran on a display buffer the size of its window");
            expectUpscaled(device, 960, 540, true, "FSR did not upscale a 960x540 display buffer into a 1084x610 window");
            expectUpscaled(device, 1920, 1080, false, "FSR ran again once the display buffer was larger than the window");
        }
        presentWithPlane(window);
        setUpscale("");
        {
            VulkanDevice device(&window);
            expectUpscaled(device, 960, 540, false, "FSR ran with ANYPS5_UPSCALE empty");
        }
        std::cout << "FSR presents only where it upscales, with and without the performance overlay, and an overlay plane is composited over its output\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
