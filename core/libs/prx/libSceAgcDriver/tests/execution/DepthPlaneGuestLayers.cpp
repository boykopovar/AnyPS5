#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "FixtureDeviceSelection.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32Float = 22;
constexpr std::size_t LayerBytes = std::size_t{Side} * Side * 4u;
constexpr std::size_t BlockBytes = 2u * LayerBytes;
constexpr int Skipped = 77;

PFN_vkGetDeviceProcAddr realDeviceProc = nullptr;
PFN_vkCmdCopyImageToBuffer realCopyImageToBuffer = nullptr;
PFN_vkCmdCopyBufferToImage realCopyBufferToImage = nullptr;
std::atomic<std::uint32_t> imageToBufferCopies{0};
std::atomic<std::uint32_t> bufferToImageCopies{0};
std::atomic<VkImage> lastFilledImage{VK_NULL_HANDLE};
PFN_vkAllocateMemory realAllocateMemory = nullptr;
std::atomic<std::uint32_t> memoryAllocations{0};
std::atomic<VkDeviceSize> lastAllocationBytes{0};
std::atomic<std::uint32_t> lastAllocationType{0};

VKAPI_ATTR VkResult VKAPI_CALL CountAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* memory) {
    memoryAllocations.fetch_add(1);
    lastAllocationBytes = info->allocationSize;
    lastAllocationType = info->memoryTypeIndex;
    return realAllocateMemory(device, info, allocator, memory);
}

VKAPI_ATTR void VKAPI_CALL CountImageToBuffer(VkCommandBuffer commands, VkImage image, VkImageLayout layout, VkBuffer buffer, std::uint32_t count, const VkBufferImageCopy* regions) {
    imageToBufferCopies.fetch_add(1);
    realCopyImageToBuffer(commands, image, layout, buffer, count, regions);
}

VKAPI_ATTR void VKAPI_CALL CountBufferToImage(VkCommandBuffer commands, VkBuffer buffer, VkImage image, VkImageLayout layout, std::uint32_t count, const VkBufferImageCopy* regions) {
    bufferToImageCopies.fetch_add(1);
    lastFilledImage = image;
    realCopyBufferToImage(commands, buffer, image, layout, count, regions);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL CountingDeviceProc(VkDevice device, const char* name) {
    const auto function = realDeviceProc(device, name);
    if (function == nullptr) return nullptr;
    if (std::strcmp(name, "vkCmdCopyImageToBuffer") == 0) {
        realCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(function);
        return reinterpret_cast<PFN_vkVoidFunction>(&CountImageToBuffer);
    }
    if (std::strcmp(name, "vkAllocateMemory") == 0) {
        realAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(function);
        return reinterpret_cast<PFN_vkVoidFunction>(&CountAllocateMemory);
    }
    if (std::strcmp(name, "vkCmdCopyBufferToImage") == 0) {
        realCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(function);
        return reinterpret_cast<PFN_vkVoidFunction>(&CountBufferToImage);
    }
    return function;
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
            context.physical = SelectFixturePhysicalDevice(devices, function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties"));
            const auto extensions = function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            Check(extensions(context.physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> available(count);
            Check(extensions(context.physical, nullptr, &count, available.data()), "vkEnumerateDeviceExtensionProperties");
            auto bytes = AgcDriver::QueryBdaByteFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            auto address = AgcDriver::QueryBdaFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            constexpr VkQueueFlags needed = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & needed) != needed) ++family;
            Require(family < count, "no Vulkan graphics and compute queue");
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            address.pNext = &bytes;
            const std::array<const char*, 2> extensionsEnabled{VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME};
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &address};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.enabledExtensionCount = static_cast<std::uint32_t>(extensionsEnabled.size());
            device.ppEnabledExtensionNames = extensionsEnabled.data();
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            realDeviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            context.deviceProc = &CountingDeviceProc;
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
            context.bufferDeviceAddress = true;
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
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
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
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

class WatchedBlock {
public:
    WatchedBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "depth plane guest layers: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~WatchedBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    WatchedBlock(const WatchedBlock&) = delete;
    WatchedBlock& operator=(const WatchedBlock&) = delete;

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

class SparseLayers {
public:
    SparseLayers() {
#ifdef _WIN32
        memory = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocateAtOrAbove_nid_postfix(GuestArena::GuestArenaHighestFree_nid_no_patch(BlockBytes, 65536), BlockBytes, 65536));
#else
        constexpr std::uintptr_t alignment = 65536;
        void* raw = mmap(nullptr, BlockBytes + alignment, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "cannot reserve sparse depth layers");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
        if (aligned != begin) munmap(raw, aligned - begin);
        if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
        memory = reinterpret_cast<std::uint8_t*>(aligned);
#endif
        Require(memory != nullptr, "cannot reserve sparse depth layers");
    }

    ~SparseLayers() {
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(memory, BlockBytes);
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(Address(), BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(memory, BlockBytes);
#else
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(memory, BlockBytes);
        munmap(memory, BlockBytes);
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(Address(), BlockBytes);
#endif
    }

    void Commit(std::size_t offset, std::size_t bytes) {
#ifdef _WIN32
        GuestArena::GuestArenaCommit_nid_postfix(memory + offset, bytes, PAGE_READWRITE, 65536);
#else
        Require(mmap(memory + offset, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED, "cannot commit sparse depth layer pages");
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory + offset, bytes);
#endif
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(Address() + offset, bytes);
    }

    void DecommitGuestLayer() {
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(memory + LayerBytes, LayerBytes);
#else
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(memory + LayerBytes, LayerBytes);
        Require(mmap(memory + LayerBytes, LayerBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED, "cannot decommit sparse depth layer pages");
#endif
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(Address() + LayerBytes, LayerBytes);
    }

    std::uint8_t* Data() const { return memory; }
    std::uint64_t Address() const { return reinterpret_cast<std::uint64_t>(memory); }
    SparseLayers(const SparseLayers&) = delete;
    SparseLayers& operator=(const SparseLayers&) = delete;

private:
    std::uint8_t* memory = nullptr;
};

std::atomic<std::uint64_t> watchedBegin{0};
std::atomic<std::uint64_t> watchedEnd{0};
std::atomic<std::uint32_t> watchedReads{0};

void CountReads(std::uint64_t address, std::size_t bytes) {
    if (address < watchedEnd.load() && address + bytes > watchedBegin.load()) watchedReads.fetch_add(1);
}

void FillLayer(std::uint8_t* layer, float depth) {
    for (std::size_t offset = 0; offset < LayerBytes; offset += sizeof(depth)) std::memcpy(layer + offset, &depth, sizeof(depth));
}

GuestTextureResource ArrayOver(std::uint64_t address) {
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = Side;
    resource.height = Side;
    resource.depthOrLastArray = 1;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kZ64KBX;
    resource.dimension = TextureDimension::k2DArray;
    resource.format = Format32Float;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    return resource;
}

constexpr std::array<std::uint32_t, 8> words{};
constexpr VkComponentMapping components{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

void Run(const Context& context, const WatchedBlock& block) {
    const auto address = block.Address();
    FillLayer(block.Data(), 1.0f);
    FillLayer(block.Data() + LayerBytes, 0.25f);
    Require(DepthSliceBytes({Side, Side}, 4u) == LayerBytes, "the depth slice of the test surface is not one layer of the guest block");
    DepthSurfaceView(context, DepthTarget{address, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    const auto resource = ArrayOver(address);
    watchedBegin = address + LayerBytes;
    watchedEnd = address + BlockBytes;
    AgcDriver::GuestMemory::SetFlushHook(&CountReads);
    const auto sample = [&](const char* when) {
        watchedReads = 0;
        const auto texture = DepthSurfaceTexture(context, words, resource, components);
        Require(texture != nullptr, std::string("the 2D array over the depth surface was not sampled through its depth planes ") + when);
        return watchedReads.load();
    };
    try {
        Require(sample("at its first use") != 0, "the first sampling of the depth planes did not read the layer no depth surface holds");
        Require(sample("again") == 0, "sampling the depth planes again read the guest layer no depth surface holds although nothing wrote it");
        FillLayer(block.Data() + LayerBytes, 0.75f);
        Require(sample("after a CPU write") != 0, "sampling the depth planes after a CPU write to the guest layer did not read it again");
        Require(sample("after the re-read") == 0, "sampling the depth planes after the re-read of the written layer read it once more");
        AgcDriver::GuestMemory::MarkWritten(address + LayerBytes, 4096);
        Require(sample("after a driver store") != 0, "sampling the depth planes after a driver store over the guest layer did not read it again");
        const DepthTarget slice{address, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0};
        const DepthTarget other{address + 2 * BlockBytes, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0};
        const auto copies = [&](const char* when, std::uint32_t fromSlices, std::uint32_t intoCopy) {
            imageToBufferCopies = 0;
            bufferToImageCopies = 0;
            sample(when);
            if (imageToBufferCopies.load() != fromSlices || bufferToImageCopies.load() != intoCopy) {
                throw std::runtime_error(std::string("sampling the depth planes ") + when + " recorded " + std::to_string(imageToBufferCopies.load()) + " depth surface copies and " + std::to_string(bufferToImageCopies.load()) + " plane copy fills, expected " + std::to_string(fromSlices) + " and " + std::to_string(intoCopy));
            }
        };
        DepthSurfaceView(context, other);
        copies("after a draw to another depth surface", 1, 1);
        copies("with nothing written since", 0, 0);
        DepthSurfaceView(context, slice);
        copies("while a draw to its depth surface is being recorded", 1, 1);
        copies("again while that draw is the last depth draw", 1, 1);
        DepthSurfaceView(context, other);
        copies("after that draw", 1, 1);
        copies("with nothing written since that draw", 0, 0);
        FillLayer(block.Data() + LayerBytes, 0.5f);
        copies("after a CPU write to the guest layer only", 0, 1);
        copies("with nothing written since the CPU write", 0, 0);
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
}

void SparseGuestLayers(const Context& context) {
    ClearDepthSurfaces(context.device);
    SparseLayers block;
    block.Commit(0, LayerBytes);
    FillLayer(block.Data(), 1.0f);
    const auto address = block.Address();
    DepthSurfaceView(context, DepthTarget{address, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    const auto resource = ArrayOver(address);
    watchedBegin = address + LayerBytes;
    watchedEnd = address + BlockBytes;
    AgcDriver::GuestMemory::SetFlushHook(&CountReads);
    const auto sample = [&](const char* when, bool changed) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        watchedReads = 0;
        Require(DepthSurfaceTexture(context, words, resource, components) != nullptr, "sparse depth layers were not sampled");
        Require((watchedReads.load() != 0) == changed, std::string("sparse depth layer read mismatch ") + when);
    };
    try {
        sample("at first use", true);
        sample("with the guest layer still uncommitted", false);
        block.Commit(LayerBytes, 65536);
        const float value = 0.25f;
        std::memcpy(block.Data() + LayerBytes, &value, sizeof(value));
        sample("after committing part of the guest layer", true);
        block.Data()[LayerBytes + 64] = 1;
        sample("after a CPU write to the committed part", true);
        AgcDriver::GuestMemory::MarkWritten(address + LayerBytes + 128, 4);
        sample("after a driver store to the committed part", true);
        block.DecommitGuestLayer();
        sample("after decommitting the guest layer", true);
        sample("with the guest layer still decommitted", false);
        block.Commit(LayerBytes, 65536);
        sample("after committing the same pages again", true);
        block.DecommitGuestLayer();
        block.Commit(LayerBytes, 65536);
        sample("after replacing a mapping with the same committed range", true);
        block.DecommitGuestLayer();
        block.Commit(LayerBytes, LayerBytes);
        FillLayer(block.Data() + LayerBytes, 0.5f);
        sample("after fully committing the guest layer", true);
        sample("with the fully committed layer unchanged", false);
        block.DecommitGuestLayer();
        sample("after decommitting a fully committed guest layer", true);
        sample("with the fully committed layer now empty", false);
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        ClearDepthSurfaces(context.device);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    ClearDepthSurfaces(context.device);
}

void RecordedUploads(const Context& context, const WatchedBlock& block) {
    const auto resource = ArrayOver(block.Address());
    Recorder recorder(context);
    recorder.Activate();
    for (int round = 1; round <= 3; ++round) {
        FillLayer(block.Data() + LayerBytes, 0.125f * static_cast<float>(round));
        const auto before = recorder.OpenKeptBytes();
        Require(DepthSurfaceTexture(context, words, resource, components) != nullptr, "the 2D array over the depth surface was not sampled through its depth planes while recording");
        const auto kept = recorder.OpenKeptBytes() - before;
        Require(kept >= LayerBytes, "the recorded upload of a written guest layer added " + std::to_string(kept) + " bytes to the open batch's kept bytes, less than the " + std::to_string(LayerBytes) + " bytes of the layer it holds until the batch completes");
    }
    recorder.Sync();
}

void PooledPlaneCopies(const Context& context, const WatchedBlock& first, const WatchedBlock& second) {
    ClearDepthSurfaces(context.device);
    FillLayer(first.Data() + LayerBytes, 0.25f);
    FillLayer(second.Data() + LayerBytes, 0.75f);
    DepthSurfaceView(context, DepthTarget{first.Address(), 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    DepthSurfaceView(context, DepthTarget{second.Address(), 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    const auto sample = [&](const WatchedBlock& block, int round) {
        Recorder::CountPresent();
        Recorder::CountPresent();
        lastFilledImage = VK_NULL_HANDLE;
        bufferToImageCopies = 0;
        Require(DepthSurfaceTexture(context, words, ArrayOver(block.Address()), components) != nullptr, "the 2D array over a depth surface was not sampled through its depth planes in round " + std::to_string(round));
        Require(DepthPlaneCopyCount() == 1, "the depth plane copies outgrew a budget that holds one, two presents after the other was sampled, in round " + std::to_string(round) + ", " + std::to_string(DepthPlaneCopyCount()) + " copies");
        Require(lastFilledImage.load() != VK_NULL_HANDLE && bufferToImageCopies.load() == 1, "a plane copy made again over a full budget was not filled from its surface once in round " + std::to_string(round) + ", " + std::to_string(bufferToImageCopies.load()) + " fills");
    };
    sample(first, 0);
    sample(second, 0);
    const auto keepsCopies = [&] {
        if (context.bufferPool == nullptr) return false;
        const VkMemoryAllocateInfo probe{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, lastAllocationBytes.load(), lastAllocationType.load()};
        const auto memory = context.bufferPool->TakeImageMemory(probe, "vkAllocateMemory pool probe");
        const auto before = context.bufferPool->RetainedImages();
        context.bufferPool->PutImageMemory(memory, probe.allocationSize, probe.memoryTypeIndex);
        const bool kept = context.bufferPool->RetainedImages() > before;
        return kept;
    }();
    memoryAllocations = 0;
    for (int round = 1; round <= 3; ++round) {
        sample(first, round);
        sample(second, round);
    }
    if (!keepsCopies) {
        std::printf("note: the device memory pool does not keep a block of a plane copy's size and memory type here (not device-tiered, or its heap over budget); the reuse of evicted copies' memory is not checked\n");
    } else {
        Require(memoryAllocations.load() == 0, "plane copies evicted and made again over a full budget allocated device memory " + std::to_string(memoryAllocations.load()) + " times instead of taking the evicted copies' memory back");
    }
    ClearDepthSurfaces(context.device);
}

void SharedLayerRanges(const Context& context, const WatchedBlock& block) {
    ClearDepthSurfaces(context.device);
    auto resource = ArrayOver(block.Address());
    resource.depthOrLastArray = 3;
    const auto geometry = DescribeSurface(resource);
    for (std::uint32_t layer = 0; layer < 4; ++layer) {
        DepthSurfaceView(context, DepthTarget{block.Address() + geometry.GuestLayerOffset(layer), 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    }
    DepthSurfaceView(context, DepthTarget{block.Address() + 16u * LayerBytes, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    const auto sample = [&](std::uint32_t lastArray, std::uint32_t baseArray, std::uint32_t fromSlices, std::uint32_t intoCopy) {
        auto range = resource;
        range.depthOrLastArray = lastArray;
        range.baseArray = baseArray;
        imageToBufferCopies = 0;
        bufferToImageCopies = 0;
        const auto texture = DepthSurfaceTexture(context, words, range, components);
        const auto when = "layers " + std::to_string(baseArray) + "-" + std::to_string(lastArray);
        Require(texture != nullptr, "the 2D array over depth surfaces was not sampled through its depth planes for " + when);
        if (imageToBufferCopies.load() != fromSlices || bufferToImageCopies.load() != intoCopy) {
            throw std::runtime_error("sampling " + when + " of the depth surfaces recorded " + std::to_string(imageToBufferCopies.load()) + " depth surface copies and " + std::to_string(bufferToImageCopies.load()) + " plane copy fills, expected " + std::to_string(fromSlices) + " and " + std::to_string(intoCopy));
        }
        const auto view = texture->SampledViewRange(false);
        Require(view.layers == lastArray + 1u - baseArray, "the view sampling " + when + " covers " + std::to_string(view.layers) + " layers, not the texture's own");
        return texture;
    };
    sample(3, 0, 4, 1);
    sample(2, 0, 0, 0);
    sample(1, 0, 0, 0);
    sample(2, 1, 0, 0);
    sample(3, 0, 0, 0);
    ClearDepthSurfaces(context.device);
}

void RunCubeGather(const Context& base, const WatchedBlock& block) {
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.width = Side;
    resource.height = Side;
    resource.depthOrLastArray = 5;
    resource.mipCount = 2;
    resource.tileMode = TextureTileMode::kZ64KBX;
    resource.dimension = TextureDimension::kCube;
    resource.format = Format32Float;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    Require(geometry.layerBytes != LayerBytes, "the cube fixture's per-layer footprint must not match one host depth slice");
    const auto address = block.Address();
    resource.baseAddress = address;
    ClearDepthSurfaces(context.device);
    for (std::uint32_t layer = 0; layer < 6; ++layer) {
        DepthSurfaceView(context, DepthTarget{address + geometry.GuestLayerOffset(layer), 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    }
    imageToBufferCopies = 0;
    bufferToImageCopies = 0;
    const auto texture = DepthSurfaceTexture(context, words, resource, components);
    Require(texture != nullptr, "a cube over six depth surfaces at the descriptor's layer offsets was not sampled through their depth planes");
    if (imageToBufferCopies.load() != 6 || bufferToImageCopies.load() != 1) {
        throw std::runtime_error("sampling a cube over six depth surfaces recorded " + std::to_string(imageToBufferCopies.load()) + " depth surface copies and " + std::to_string(bufferToImageCopies.load()) + " plane copy fills, expected 6 and 1");
    }
    ClearDepthSurfaces(context.device);
}

void RunCubeTiledSlice(const Context& base, const WatchedBlock& block) {
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    constexpr VkExtent2D extent{64, 64};
    GuestTextureResource resource{};
    resource.width = extent.width;
    resource.height = extent.height;
    resource.depthOrLastArray = 5;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kZ64KBX;
    resource.dimension = TextureDimension::kCube;
    resource.format = Format32Float;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    const auto slice = DepthSliceBytes(extent, 4u);
    Require(geometry.layerBytes == slice && geometry.sliceLinearBytes != slice, "the tiled cube fixture must have a layer footprint equal to a depth slice but a different linear footprint");
    const auto address = block.Address();
    resource.baseAddress = address;
    ClearDepthSurfaces(context.device);
    for (std::uint32_t layer = 0; layer < 6; ++layer) {
        DepthSurfaceView(context, DepthTarget{address + geometry.GuestLayerOffset(layer), 0, extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    }
    imageToBufferCopies = 0;
    bufferToImageCopies = 0;
    const auto texture = DepthSurfaceTexture(context, words, resource, components);
    Require(texture != nullptr, "a tiled cube whose linear layer footprint differs from a depth slice was not sampled through its depth planes");
    if (imageToBufferCopies.load() != 6 || bufferToImageCopies.load() != 1) {
        throw std::runtime_error("sampling a tiled cube over six depth surfaces recorded " + std::to_string(imageToBufferCopies.load()) + " depth surface copies and " + std::to_string(bufferToImageCopies.load()) + " plane copy fills, expected 6 and 1");
    }
    ClearDepthSurfaces(context.device);
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_DEPTH_PLANE_COPY_MIB", "1");
#else
        setenv("APS5_DEPTH_PLANE_COPY_MIB", "1", 1);
#endif
        if (!AgcDriver::GuestMemory::WriteWatched()) {
            std::puts("skipped, guest memory has no write watch");
            return Skipped;
        }
        std::optional<Device> device;
        try {
            device.emplace();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return Skipped;
        }
        {
            TextureDetiler detiler(device->GetContext());
            auto context = device->GetContext();
            context.detiler = &detiler;
            WatchedBlock block;
            Require(AgcDriver::GuestMemory::Watched(block.Address(), BlockBytes), "depth plane guest layers: the guest block is not write-watched");
            Run(context, block);
            RecordedUploads(context, block);
            {
                WatchedBlock second;
                PooledPlaneCopies(context, block, second);
            }
            SharedLayerRanges(context, block);
            RunCubeGather(context, block);
            RunCubeTiledSlice(context, block);
            SparseGuestLayers(context);
        }
        device.reset();
        std::puts("depth plane guest layer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
