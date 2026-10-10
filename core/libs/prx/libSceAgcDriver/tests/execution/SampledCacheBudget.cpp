#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format8888UInt = 60;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = std::size_t{Side} * Side * 4u;
constexpr std::size_t Surfaces = 12;
constexpr std::size_t BlockBytes = SurfaceBytes * Surfaces;
constexpr std::uint64_t Budget = 1ull << 20u;
constexpr std::uint64_t Ceiling = Budget + Budget / 4u;

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
            const auto extensions = function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            Check(extensions(context.physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> available(count);
            Check(extensions(context.physical, nullptr, &count, available.data()), "vkEnumerateDeviceExtensionProperties");
            const auto hasExtension = [&](const char* name) {
                for (const auto& extension : available) {
                    if (std::strcmp(extension.extensionName, name) == 0) return true;
                }
                return false;
            };
            auto bytes = AgcDriver::QueryBdaByteFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            auto address = AgcDriver::QueryBdaFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) ++family;
            Require(family < count, "no Vulkan compute queue");
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            address.pNext = &bytes;
            std::vector<const char*> extensionsEnabled{VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME};
            if (hasExtension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME)) {
                extensionsEnabled.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
                VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT};
                VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &hostProperties};
                function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(context.physical, &properties);
                context.hostImportAlignment = hostProperties.minImportedHostPointerAlignment;
            }
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &address};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.enabledExtensionCount = static_cast<std::uint32_t>(extensionsEnabled.size());
            device.ppEnabledExtensionNames = extensionsEnabled.data();
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
            context.bufferDeviceAddress = true;
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.imageFormatProperties = function<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties");
            if (hasExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) context.memoryProperties2 = function<PFN_vkGetPhysicalDeviceMemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2");
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
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

class Block {
public:
    explicit Block(const Context& context) : context(context) {
#ifdef _WIN32
        memory = VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        memory = std::aligned_alloc(65536, BlockBytes);
#endif
        Require(memory != nullptr, "cannot allocate the texture block");
        std::memset(memory, 0x5a, BlockBytes);
        GuestAllocations::Mutation().Add(memory, BlockBytes, true, true, true);
    }

    ~Block() {
        ClearCachedTextures(context.device);
        GuestAllocations::Mutation().Remove(memory);
        HostImportFor(context, Surface(0), BlockBytes);
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        std::free(memory);
#endif
    }

    std::uint64_t Surface(std::size_t surface) const { return reinterpret_cast<std::uint64_t>(memory) + surface * SurfaceBytes; }

private:
    const Context& context;
    void* memory = nullptr;
};

std::array<std::uint32_t, 8> Descriptor(std::uint64_t address, std::uint32_t side = Side, std::uint32_t format = Format32UInt, std::uint32_t dstSel = 0xfacu) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        dstSel | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::shared_ptr<Texture> Sampled(const Context& context, std::uint64_t address, std::uint32_t side = Side) {
    const auto words = Descriptor(address, side);
    return CachedSampledTexture(context, words);
}

GuestTextureResource Resource(std::uint64_t address, std::uint32_t side = Side) {
    const auto words = Descriptor(address, side);
    return DecodeTextureResource(words);
}

void viewTests(const Context& context) {
    if (context.hostImportAlignment == 0) {
        std::puts("host imports unavailable: views of storage images not tested");
        return;
    }
    Block block(context);
    if (HostImportFor(context, block.Surface(0), BlockBytes) == nullptr) {
        std::puts("host import of the texture block refused: views of storage images not tested");
        return;
    }
    const auto before = TextureCacheUsage();
    auto view = Sampled(context, block.Surface(0));
    Require(view->ViewsStorageImage(), "a sampled texture in host-imported memory is not a view of its storage image");
    const std::weak_ptr<Texture> viewed = view;
    const auto* image = view->StorageSource();
    const auto after = TextureCacheUsage();
    Require(after.sampledEntries == before.sampledEntries + 1u, "the view did not enter the sampled texture cache");
    Require(after.sampledBytes == before.sampledBytes, "a view of a cached storage image counted " + std::to_string(after.sampledBytes - before.sampledBytes) + " bytes against the sampled texture cache");
    Require(after.storageBytes > before.storageBytes, "the storage image of the view is not counted by the storage image cache");
    Require(Sampled(context, block.Surface(0)) == view, "the second lookup of the surface did not hit its view");
    view.reset();
    for (std::size_t surface = 1; surface < Surfaces; ++surface) CachedStorageSurface(context, Resource(block.Surface(surface)));
    Require(!StorageImageCached(context, image), "the storage image cache never evicted the viewed image under a 1 MiB budget");
    const auto evicted = TextureCacheUsage();
    Require(viewed.expired(), "the sampled texture cache kept the view of an evicted storage image");
    Require(evicted.sampledEntries == after.sampledEntries - 1u && evicted.sampledBytes == before.sampledBytes, "dropping the view of the evicted storage image changed other sampled entries");
    auto again = Sampled(context, block.Surface(0));
    Require(again->ViewsStorageImage() && again->StorageSource() != nullptr && StorageImageCached(context, again->StorageSource()), "the surface's next lookup does not view its new cached storage image");
}

void reportedBudgetTests(const Context& context) {
    if (context.memoryProperties2 == nullptr) {
        std::puts("VK_EXT_memory_budget unavailable: the sampled texture cache keeps a quarter of the device-local heap");
        return;
    }
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    properties.pNext = &reported;
    context.memoryProperties2(context.physical, &properties);
    const auto budget = SampledTextureBudget(context.memory, &reported, 0);
    Require(budget >= (2048ull << 20u), "the sampled texture cache budget is below the 2 GiB floor");
    for (std::uint32_t heap = 0; heap < context.memory.memoryHeapCount; ++heap) {
        if ((context.memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0) continue;
        std::printf("device-local heap %u: %llu MiB, VK_EXT_memory_budget budget %llu MiB, usage %llu MiB\n", heap, static_cast<unsigned long long>(context.memory.memoryHeaps[heap].size >> 20u), static_cast<unsigned long long>(reported.heapBudget[heap] >> 20u), static_cast<unsigned long long>(reported.heapUsage[heap] >> 20u));
    }
    std::printf("sampled texture cache budget %llu MiB, storage image cache budget %llu MiB\n", static_cast<unsigned long long>(budget >> 20u), static_cast<unsigned long long>(TextureCacheBudget(context.memory) >> 20u));
}

void snapshotTests(const Context& context) {
    Block block(context);
    auto first = Sampled(context, block.Surface(0));
    Require(!first->ViewsStorageImage(), "a sampled texture outside host-imported memory is not a snapshot");
    const auto held = TextureCacheUsage().sampledBytes;
    Require(held == first->AllocationBytes(), "a snapshot counted " + std::to_string(held) + " bytes, not its image allocation of " + std::to_string(first->AllocationBytes()));
    const std::weak_ptr<Texture> oldest = first;
    first.reset();
    const auto capacity = static_cast<std::size_t>(Budget / held);
    Require(capacity >= 2u && capacity + 1u < Surfaces, "a snapshot of the 256 KiB surface counts " + std::to_string(held) + " bytes, which the test's 1 MiB budget does not fit as planned");
    for (std::size_t surface = 1; surface < capacity; ++surface) Sampled(context, block.Surface(surface));
    Require(!oldest.expired() && TextureCacheUsage().sampledEntries == capacity, "the sampled texture cache did not keep " + std::to_string(capacity) + " snapshots within its budget");
    Sampled(context, block.Surface(capacity));
    Require(oldest.expired(), "the least recently used snapshot was not evicted past the budget");
    Require(TextureCacheUsage().sampledBytes <= Budget, "the sampled texture cache holds more than its budget");
    ClearCachedTextures(context.device);
    constexpr std::uint32_t TinySide = 16;
    const auto tiny = Sampled(context, block.Surface(0), TinySide);
    const auto guestBytes = DescribeSurface(Resource(block.Surface(0), TinySide)).guestBytes;
    if (tiny->AllocationBytes() >= guestBytes) {
        std::printf("the 16x16 snapshot's image takes %llu bytes, not less than its %llu guest bytes: counting the allocation alone not tested\n", static_cast<unsigned long long>(tiny->AllocationBytes()), static_cast<unsigned long long>(guestBytes));
        return;
    }
    Require(TextureCacheUsage().sampledBytes == tiny->AllocationBytes(), "a snapshot whose " + std::to_string(tiny->AllocationBytes()) + "-byte image is smaller than its " + std::to_string(guestBytes) + " guest bytes counted " + std::to_string(TextureCacheUsage().sampledBytes) + " bytes, not its allocation");
}

void heldTests(const Context& context) {
    Block block(context);
    auto held = Sampled(context, block.Surface(0));
    const auto bytes = TextureCacheUsage().sampledBytes;
    const auto capacity = static_cast<std::size_t>(Budget / bytes);
    Require((capacity + 1u) * bytes <= Ceiling && capacity + 2u < Surfaces, "a snapshot of the 256 KiB surface counts " + std::to_string(bytes) + " bytes, which the test's ceiling does not fit as planned");
    std::vector<std::weak_ptr<Texture>> released;
    for (std::size_t surface = 1; surface < capacity; ++surface) released.push_back(Sampled(context, block.Surface(surface)));
    Sampled(context, block.Surface(capacity));
    Require(Sampled(context, block.Surface(0)) == held, "the sampled texture cache evicted a texture its caller still holds");
    Require(released.front().expired(), "the least recently used texture nobody holds was not evicted in place of the held one");
    held.reset();
    ClearCachedTextures(context.device);
    std::vector<std::shared_ptr<Texture>> all;
    for (std::size_t surface = 0; surface <= capacity; ++surface) all.push_back(Sampled(context, block.Surface(surface)));
    Require(TextureCacheUsage().sampledEntries == capacity + 1u && TextureCacheUsage().sampledBytes > Budget, "textures their callers hold left a cache over its budget");
    for (std::size_t surface = 0; surface < all.size(); ++surface) Require(Sampled(context, block.Surface(surface)) == all[surface], "surface " + std::to_string(surface) + " was made again while its texture was held");
    all.push_back(Sampled(context, block.Surface(capacity + 1u)));
    Require(TextureCacheUsage().sampledEntries == capacity + 1u && TextureCacheUsage().sampledBytes <= Ceiling, "held textures took the sampled texture cache past a quarter over its budget");
    for (std::size_t surface = 1; surface < all.size(); ++surface) Require(Sampled(context, block.Surface(surface)) == all[surface], "past the ceiling, surface " + std::to_string(surface) + " left in place of the least recently used held texture");
    all.clear();
    Sampled(context, block.Surface(capacity + 2u));
    Require(TextureCacheUsage().sampledEntries == capacity && TextureCacheUsage().sampledBytes <= Budget, "released textures were not evicted down to the budget");
}

void rescanTests(const Context& context) {
    constexpr std::uint32_t SmallSide = 128;
    constexpr std::size_t SmallBytes = std::size_t{SmallSide} * SmallSide * 4u;
    Block block(context);
    const auto small = [&](std::size_t surface) { return Sampled(context, block.Surface(0) + surface * SmallBytes, SmallSide); };
    std::vector<std::shared_ptr<Texture>> all{small(0)};
    const auto bytes = TextureCacheUsage().sampledBytes;
    const auto capacity = static_cast<std::size_t>(Budget / bytes);
    Require((capacity + 3u) * bytes <= Ceiling && (capacity + 3u) * SmallBytes <= BlockBytes, "a snapshot of the 64 KiB surface counts " + std::to_string(bytes) + " bytes, which the test's ceiling does not fit as planned");
    for (std::size_t surface = 1; surface <= capacity; ++surface) all.push_back(small(surface));
    const std::weak_ptr<Texture> newest = all.back();
    all.pop_back();
    all.push_back(small(capacity + 1u));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    all.push_back(small(capacity + 2u));
    Require(newest.expired(), "a released texture behind held ones was still cached a second later");
    Require(TextureCacheUsage().sampledEntries == capacity + 2u, "the rescan left " + std::to_string(TextureCacheUsage().sampledEntries) + " entries, not the " + std::to_string(capacity + 2u) + " textures still held");
}

void viewEvictionTests(const Context& context) {
    if (context.hostImportAlignment == 0) return;
    Block snapshots(context);
    Block imported(context);
    if (HostImportFor(context, imported.Surface(0), BlockBytes) == nullptr) return;
    const std::weak_ptr<Texture> view = Sampled(context, imported.Surface(0));
    Require(!view.expired() && TextureCacheUsage().sampledBytes == 0, "the view of a cached storage image did not enter the sampled texture cache at 0 bytes");
    Sampled(context, snapshots.Surface(0));
    const auto capacity = static_cast<std::size_t>(Budget / TextureCacheUsage().sampledBytes);
    for (std::size_t surface = 1; surface <= capacity; ++surface) Sampled(context, snapshots.Surface(surface));
    Require(!view.expired(), "the sampled texture cache evicted a view of a cached storage image, which frees no memory");
}

void frameTests(const Context& context) {
    Block block(context);
    ClearCachedTextures(context.device);
    Recorder::CountPresent();
    const auto bytes = Sampled(context, block.Surface(0))->AllocationBytes();
    const auto capacity = static_cast<std::size_t>(Budget / bytes);
    const auto frame = capacity + 2u;
    Require(capacity >= 2u && 2u * frame <= Surfaces && TextureCacheUsage().sampledEntries == 1u, "a snapshot of the 256 KiB surface holds " + std::to_string(bytes) + " bytes, which the test's 1 MiB budget does not fit as planned");
    std::vector<std::weak_ptr<Texture>> made(frame);
    for (std::size_t surface = 0; surface < frame; ++surface) made[surface] = Sampled(context, block.Surface(surface));
    Require(TextureCacheUsage().sampledEntries == capacity, "a frame of " + std::to_string(frame) + " textures left " + std::to_string(TextureCacheUsage().sampledEntries) + " cached, not the " + std::to_string(capacity) + " the budget holds");
    for (std::size_t surface = capacity; surface < frame; ++surface) Require(made[surface].expired(), "surface " + std::to_string(surface) + ", used once in a frame whose textures fill the cache, was cached in place of an earlier one");
    Recorder::CountPresent();
    std::size_t hits = 0;
    for (std::size_t surface = 0; surface < frame; ++surface) {
        const auto again = Sampled(context, block.Surface(surface));
        hits += again == made[surface].lock();
    }
    Require(hits == capacity, "the next frame over the same " + std::to_string(frame) + " textures found " + std::to_string(hits) + " of them cached, not the " + std::to_string(capacity) + " the cache kept");
    auto admitted = Sampled(context, block.Surface(frame - 1u));
    Require(Sampled(context, block.Surface(frame - 1u)) == admitted, "a texture used a second time in the frame was not cached");
    Require(TextureCacheUsage().sampledEntries == capacity && TextureCacheUsage().sampledBytes <= Budget, "the texture admitted on its second use left the cache over its budget");
    admitted.reset();
    Recorder::CountPresent();
    std::vector<std::weak_ptr<Texture>> moved(frame);
    for (std::size_t surface = 0; surface < frame; ++surface) moved[surface] = Sampled(context, block.Surface(frame + surface));
    for (std::size_t surface = 0; surface < frame; ++surface) Require(moved[surface].expired(), "surface " + std::to_string(frame + surface) + ", first used in the frame after the cached textures were, was cached in place of one of them");
    Require(TextureCacheUsage().sampledEntries == capacity, "a frame over other textures left " + std::to_string(TextureCacheUsage().sampledEntries) + " of the previous frame's textures cached");
    Recorder::CountPresent();
    for (std::size_t surface = 0; surface < frame; ++surface) moved[surface] = Sampled(context, block.Surface(frame + surface));
    for (std::size_t surface = 0; surface < capacity; ++surface) Require(!moved[surface].expired(), "surface " + std::to_string(frame + surface) + " of a second frame over other textures did not replace the textures last used two frames before");
    Require(TextureCacheUsage().sampledEntries == capacity, "a second frame over other textures left " + std::to_string(TextureCacheUsage().sampledEntries) + " entries cached");
}

void earlyMissTests(const Context& context) {
    Block block(context);
    ClearCachedTextures(context.device);
    Recorder::CountPresent();
    const auto bytes = Sampled(context, block.Surface(0))->AllocationBytes();
    const auto capacity = static_cast<std::size_t>(Budget / bytes);
    Require(capacity >= 2u && capacity < Surfaces && TextureCacheUsage().sampledEntries == 1u, "a snapshot of the 256 KiB surface holds " + std::to_string(bytes) + " bytes, which the test's 1 MiB budget does not fit as planned");
    std::vector<std::weak_ptr<Texture>> cached(capacity);
    for (std::size_t surface = 0; surface < capacity; ++surface) cached[surface] = Sampled(context, block.Surface(surface));
    Recorder::CountPresent();
    for (std::size_t surface = 0; surface < capacity; ++surface) {
        const auto again = Sampled(context, block.Surface(surface));
        Require(again == cached[surface].lock(), "surface " + std::to_string(surface) + " missed in a frame over the textures the cache holds");
    }
    Recorder::CountPresent();
    Sampled(context, block.Surface(capacity));
    std::size_t hits = 0;
    for (std::size_t surface = 0; surface < capacity; ++surface) {
        const auto again = Sampled(context, block.Surface(surface));
        hits += again == cached[surface].lock();
    }
    Require(hits == capacity, "a frame that first uses a new texture before its " + std::to_string(capacity) + " cached ones found " + std::to_string(hits) + " of them cached");
}

constexpr std::uint64_t KeyHashBasis = 14695981039346656037ull;
constexpr std::uint64_t KeyHashPrime = 1099511628211ull;

std::array<std::array<std::uint32_t, 8>, 2> CollidingDescriptors(VkDevice device, std::uint64_t address) {
    const auto words = Descriptor(address);
    auto prefix = KeyHashBasis ^ reinterpret_cast<std::uintptr_t>(device);
    for (std::size_t word = 0; word < 6; ++word) prefix = (prefix ^ words[word]) * KeyHashPrime;
    std::unordered_map<std::uint32_t, std::uint32_t> word6ByHighHalf;
    for (std::uint32_t candidate = 0; candidate < 0x200u; ++candidate) {
        const auto word6 = ((candidate >> 1u) << 24u) | (candidate & 1u);
        const auto hash = (prefix ^ word6) * KeyHashPrime;
        const auto [match, added] = word6ByHighHalf.emplace(static_cast<std::uint32_t>(hash >> 32u), word6);
        if (added) continue;
        auto first = words;
        auto second = words;
        first[6] = match->second;
        second[6] = word6;
        second[7] = static_cast<std::uint32_t>(hash ^ ((prefix ^ match->second) * KeyHashPrime));
        return {first, second};
    }
    throw std::runtime_error("no two descriptors of the surface that differ only in word 6's statistics counter and the unused meta address collide in TextureKeyHash");
}

void collidingKeyTests(const Context& context) {
    Block block(context);
    ClearCachedTextures(context.device);
    Recorder::CountPresent();
    const auto bytes = Sampled(context, block.Surface(0))->AllocationBytes();
    const auto capacity = static_cast<std::size_t>(Budget / bytes);
    Require(capacity >= 2u && capacity < Surfaces && TextureCacheUsage().sampledEntries == 1u, "a snapshot of the 256 KiB surface holds " + std::to_string(bytes) + " bytes, which the test's 1 MiB budget does not fit as planned");
    std::vector<std::weak_ptr<Texture>> cached(capacity);
    for (std::size_t surface = 0; surface < capacity; ++surface) cached[surface] = Sampled(context, block.Surface(surface));
    Recorder::CountPresent();
    const auto [first, second] = CollidingDescriptors(context.device, block.Surface(capacity));
    CachedSampledTexture(context, first);
    CachedSampledTexture(context, second);
    std::size_t kept = 0;
    for (const auto& texture : cached) kept += !texture.expired();
    Require(kept == capacity && TextureCacheUsage().sampledEntries == capacity, "the first use of a descriptor whose cache key hash equals that of another descriptor first used in the frame left " + std::to_string(kept) + " of the " + std::to_string(capacity) + " textures the last frame used cached");
}

void depthCompareFormatTests(const Context& context) {
    Block block(context);

    auto tex56 = CachedSampledTexture(context, Descriptor(block.Surface(0), Side, Format8888UNorm), true);
    Require(tex56 != nullptr, "failed to sample format 56 with depth compare");
    Require(tex56->ViewFormat() == VK_FORMAT_D32_SFLOAT, "format 56 depth compare did not produce a D32_SFLOAT view");

    auto tex56Zero = CachedSampledTexture(context, Descriptor(block.Surface(1), Side, Format8888UNorm, 0u), true);
    Require(tex56Zero != nullptr && tex56Zero->ViewFormat() == VK_FORMAT_D32_SFLOAT, "format 56 dstSelX=0 depth compare failed");

    auto tex56One = CachedSampledTexture(context, Descriptor(block.Surface(2), Side, Format8888UNorm, 1u), true);
    Require(tex56One != nullptr && tex56One->ViewFormat() == VK_FORMAT_D32_SFLOAT, "format 56 dstSelX=1 depth compare failed");

    auto tex56Color = CachedSampledTexture(context, Descriptor(block.Surface(3), Side, Format8888UNorm), false);
    Require(tex56Color != nullptr && tex56Color->ViewFormat() == VK_FORMAT_R8G8B8A8_UNORM, "format 56 color sample did not produce R8G8B8A8_UNORM view");

    auto tex32 = CachedSampledTexture(context, Descriptor(block.Surface(4), Side, Format32Float), true);
    Require(tex32 != nullptr && tex32->ViewFormat() == VK_FORMAT_D32_SFLOAT, "format 22 depth compare did not produce D32_SFLOAT view");

    const auto reject = [&](std::uint32_t format, std::size_t surfaceIndex, const char* name) {
        try {
            CachedSampledTexture(context, Descriptor(block.Surface(surfaceIndex), Side, format), true);
            Require(false, std::string(name) + " was unexpectedly accepted for comparison sampling");
        } catch (const std::exception& error) {
            Require(std::string_view(error.what()).find("comparison sampling requires an R32 float or R16 unorm depth texture") != std::string_view::npos,
                    std::string("unexpected rejection error for ") + name + ": " + error.what());
        }
    };
    reject(Format32UInt, 5, "format 20 (R32_UINT)");
    reject(Format8888UInt, 6, "format 60 (R8G8B8A8_UINT)");
}

constexpr std::uint64_t ReportedBudget = 64ull << 30u;
constexpr std::uint64_t ReportedUsage = 12ull << 30u;

void VKAPI_PTR ReportBudget(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
    auto* reported = static_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(properties->pNext);
    for (std::uint32_t heap = 0; heap < VK_MAX_MEMORY_HEAPS; ++heap) {
        reported->heapBudget[heap] = ReportedBudget;
        reported->heapUsage[heap] = ReportedUsage;
    }
}

std::uint64_t ExpectedBudget(const Context& context) {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &reported};
    ReportBudget(context.physical, &properties);
    return SampledTextureBudget(context.memory, &reported, SampledTextureMemory() + TextureCacheUsage().storageBytes);
}

std::uint64_t ReadBudget(const Context& context) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    return SampledTextureCacheBudget(context);
}

void memoryTests(Context context) {
    context.memoryProperties2 = &ReportBudget;
    const auto before = SampledTextureMemory();
    Block block(context);
    auto held = Sampled(context, block.Surface(0));
    Require(!held->ViewsStorageImage() && held->AllocationBytes() != 0, "the test texture is not a snapshot with an image of its own");
    Require(SampledTextureMemory() == before + held->AllocationBytes(), "the sampled texture memory grew by " + std::to_string(SampledTextureMemory() - before) + " bytes, not by the snapshot's allocation of " + std::to_string(held->AllocationBytes()));
    Require(ReadBudget(context) == ExpectedBudget(context), "the sampled texture cache budget does not count its cached texture as the cache's own memory");
    ClearCachedTextures(context.device);
    Require(TextureCacheUsage().sampledEntries == 0 && SampledTextureMemory() == before + held->AllocationBytes(), "a texture its caller still holds stopped counting as sampled texture memory when it left the cache");
    const auto budget = ReadBudget(context);
    Require(budget == ExpectedBudget(context), "the sampled texture cache budget counts a texture its caller still holds as memory in use outside the texture caches");
    std::printf("reported heap budget %llu MiB with %llu MiB in use: sampled texture cache budget %llu MiB with a %llu KiB texture held outside the cache\n", static_cast<unsigned long long>(ReportedBudget >> 20u), static_cast<unsigned long long>(ReportedUsage >> 20u), static_cast<unsigned long long>(budget >> 20u), static_cast<unsigned long long>(held->AllocationBytes() >> 10u));
    held.reset();
    Require(SampledTextureMemory() == before, "a released texture still counts as sampled texture memory");
}

}

int main(int argc, char** argv) {
    try {
        const bool memory = argc > 1 && std::string(argv[1]) == "memory";
#ifdef _WIN32
        if (!memory) _putenv_s("APS5_TEXTURE_CACHE_MIB", "1");
#else
        if (!memory) setenv("APS5_TEXTURE_CACHE_MIB", "1", 1);
#endif
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        auto context = device->GetContext();
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        if (memory) {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            memoryTests(context);
            ClearCachedTextures(context.device);
            std::puts("sampled texture memory budget tests passed");
            return 0;
        }
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            reportedBudgetTests(context);
            snapshotTests(context);
            heldTests(context);
            rescanTests(context);
            viewEvictionTests(context);
            viewTests(context);
            frameTests(context);
            earlyMissTests(context);
            collidingKeyTests(context);
            depthCompareFormatTests(context);
            ClearCachedTextures(context.device);
        }
        std::puts("sampled texture cache budget tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
