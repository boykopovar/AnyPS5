#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint64_t DepthAddress = 0x7f0000000000ull;
constexpr VkExtent2D Extent{64, 64};
constexpr std::uint32_t Format32Float = 22;

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
            context.physical = devices.front();
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
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
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
        if (context.device != VK_NULL_HANDLE) ClearDepthSurfaces(context.device);
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

constexpr std::size_t Block = 65536;

void* AllocateWatched(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED, "cannot map the watched block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + Block - 1) & ~static_cast<std::uintptr_t>(Block - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    Require(AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uint64_t>(block), bytes), "the test block is not watched");
    return block;
}

DepthTarget Depth(std::uint64_t address = DepthAddress, VkExtent2D extent = Extent) {
    return {address, 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
}

GuestTextureResource View(std::uint32_t width, std::uint32_t height, std::uint64_t address = DepthAddress, std::uint32_t levels = 1) {
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = width;
    resource.height = height;
    resource.mipCount = levels;
    resource.lastLevel = levels - 1;
    resource.dimension = TextureDimension::k2D;
    resource.format = Format32Float;
    return resource;
}

constexpr VkComponentMapping Identity{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

std::shared_ptr<Texture> Sample(const Context& context, const GuestTextureResource& resource) {
    const std::array<std::uint32_t, 8> words{0, 0, 0, 0, resource.width, resource.height, resource.lastLevel, 0};
    return DepthSurfaceTexture(context, words, resource, Identity);
}

bool Refused(const Context& context, const GuestTextureResource& resource) {
    try {
        Sample(context, resource);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find("is not implemented") != std::string::npos;
    }
    return false;
}

void Run(const Context& context) {
    const auto mips = View(Extent.width, Extent.height, DepthAddress, 2);
    const auto exact = View(Extent.width, Extent.height);
    const auto depthBytes = DepthSliceBytes(Extent, 4);

    DepthSurfaceView(context, Depth());
    Require(DepthSurfaceAt(DepthAddress), "a bound depth surface is not found at its address");
    Require(Refused(context, mips), "depth, then a view with two levels: the live depth surface did not refuse it");
    Require(Sample(context, exact) != nullptr, "depth, then its own view: the depth surface did not serve it");

    RetireDepthSurfaces(context.device, DepthAddress + depthBytes, 0x10000);
    Require(Refused(context, mips), "a write past the depth plane retired the depth surface");

    RetireDepthSurfaces(context.device, DepthAddress + 0x100, 0x1000);
    Require(!DepthSurfaceAt(DepthAddress), "a color target over the depth plane left the depth surface live");
    Require(Sample(context, mips) == nullptr, "depth, then a color target, then a view with two levels: the view was not left to guest memory");
    Require(Sample(context, exact) == nullptr, "depth, then a color target, then its own view: the retired depth surface served it");

    DepthSurfaceView(context, Depth());
    Require(DepthSurfaceAt(DepthAddress), "binding the depth target again did not bring the depth surface back");
    Require(Refused(context, mips), "depth, color target, depth again, then a view with two levels: the live depth surface did not refuse it");

    const auto shared = DepthAddress + 0x1000000;
    const VkExtent2D wideExtent{2 * Extent.width, Extent.height};
    const auto sharedWide = View(wideExtent.width, wideExtent.height, shared);
    const auto sharedExact = View(Extent.width, Extent.height, shared);
    DepthSurfaceView(context, Depth(shared, wideExtent));
    DepthSurfaceView(context, Depth(shared));
    DepthSurfaceView(context, Depth(shared, wideExtent));
    Require(Sample(context, sharedWide) != nullptr, "two depth surfaces at one address, the older one bound last, then its own view: it did not serve it");
    Require(Sample(context, sharedExact) == nullptr, "two depth surfaces at one address, the older one bound last, then the other's view: the overlapped depth surface served it");
    DepthSurfaceView(context, Depth(shared));
    Require(Sample(context, sharedExact) != nullptr, "two depth surfaces at one address, the newer one bound last, then its own view: it did not serve it");
    Require(Sample(context, sharedWide) == nullptr, "two depth surfaces at one address, the newer one bound last, then the other's view: the overlapped depth surface served it");

    if (!AgcDriver::GuestMemory::WriteWatched()) {
        Require(Refused(context, mips), "without write watching, a view with two levels was not refused");
        std::puts("guest memory is not write-watched here: the CPU write order is not exercised");
        return;
    }

    auto* memory = static_cast<volatile std::uint8_t*>(AllocateWatched(2 * Block));
    const auto watched = reinterpret_cast<std::uint64_t>(memory);
    Require(depthBytes <= 2 * Block, "the depth plane does not fit the watched block");
    const auto watchedMips = View(Extent.width, Extent.height, watched, 2);
    const auto watchedExact = View(Extent.width, Extent.height, watched);
    AgcDriver::GuestMemory::CollectWrites(watched, 2 * Block);

    DepthSurfaceView(context, Depth(watched));
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(Refused(context, watchedMips), "depth, then a view with two levels: the live depth surface over watched memory did not refuse it");

    memory[8] = 0x5a;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(Sample(context, watchedExact) != nullptr, "depth, then a CPU write, then its own view: the depth surface did not serve it");
    Require(Sample(context, watchedMips) == nullptr, "depth, then a CPU write, then a view with two levels: the view was not left to guest memory");
    Require(!DepthSurfaceAt(watched), "depth, then a CPU write, then a view with two levels: the depth surface stayed live");

    memory[16] = 0xa5;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    DepthSurfaceView(context, Depth(watched));
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(Refused(context, watchedMips), "a CPU write, then depth, then a view with two levels: the live depth surface did not refuse it");

    AgcDriver::GuestMemory::MarkWritten(watched + Block, 4);
    Require(Refused(context, watchedMips), "a driver store past the depth plane retired the depth surface");
    AgcDriver::GuestMemory::MarkWritten(watched + 0x100, 4);
    Require(Sample(context, watchedMips) == nullptr, "depth, then a driver store, then a view with two levels: the view was not left to guest memory");
}

}

int main() {
    try {
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        Run(device->GetContext());
        std::puts("depth surface last writer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
