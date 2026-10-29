#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
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
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = std::size_t{Side} * Side * 4u;
constexpr std::size_t Surfaces = 12;
constexpr std::size_t BlockBytes = SurfaceBytes * Surfaces;
constexpr std::size_t RegistrationGapBytes = 65536;
constexpr std::size_t AllocationBytes = BlockBytes + RegistrationGapBytes;
constexpr std::uint64_t Budget = 1ull << 20u;
constexpr std::uint64_t Ceiling = Budget + Budget / 4u;

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
    std::puts("sampled-cache device selection CPU tests passed");
}

class Device {
public:
    explicit Device(bool enableStorageImageMultisample = true) {
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
            const auto physicalProperties = function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
            std::vector<VkPhysicalDeviceProperties> deviceProperties(devices.size());
            for (std::size_t index = 0; index < devices.size(); ++index) physicalProperties(devices[index], &deviceProperties[index]);
            const char* requestedName = std::getenv("ANYPS5_GPU");
            const auto selectedIndex = selectDevice(deviceProperties, requestedName != nullptr ? requestedName : "");
            Require(selectedIndex.has_value(), requestedName != nullptr && *requestedName != '\0' ? std::string("no Vulkan 1.1 device matching ANYPS5_GPU=\"") + requestedName + "\"" : "no Vulkan 1.1 device");
            context.physical = devices[*selectedIndex];
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
            VkPhysicalDeviceFeatures supported{};
            function<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(context.physical, &supported);
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            enabled.shaderStorageImageMultisample = enableStorageImageMultisample && supported.shaderStorageImageMultisample;
            context.storageImageMultisample = enabled.shaderStorageImageMultisample == VK_TRUE;
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
            std::printf("sampled-cache fixture Vulkan device: %s vendor=0x%04x device=0x%04x\n", properties.deviceName, properties.vendorID, properties.deviceID);
            context.limits = properties.limits;
            context.bufferDeviceAddress = true;
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.imageFormatProperties = function<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties");
            if (hasExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) context.memoryProperties2 = function<PFN_vkGetPhysicalDeviceMemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2");
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

    void VerifySelection() const {
        VkPhysicalDeviceProperties properties{};
        function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
        const char* request = std::getenv("ANYPS5_GPU");
        Require(selectDevice(std::span(&properties, 1), request != nullptr ? request : "").has_value(), "the sampled-cache fixture physical device does not match ANYPS5_GPU");
        std::puts("sampled-cache fixture physical device selection verified");
    }

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
        memory = VirtualAlloc(nullptr, AllocationBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        memory = std::aligned_alloc(65536, AllocationBytes);
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

class WatchedSurface {
public:
    explicit WatchedSurface(const Context& context, std::size_t bytes = SurfaceBytes) : context(context), span(bytes) {
#ifdef _WIN32
        memory = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
        if (memory != nullptr) GuestArena::GuestArenaCommit_nid_postfix(memory, bytes, PAGE_READWRITE, bytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
            memory = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, bytes);
        }
#endif
        Require(memory != nullptr, "cannot allocate the watched surface");
        Require(AgcDriver::GuestMemory::Watched(Address(), bytes), "the watched surface is not write-watched");
        std::memset(memory, 0x5a, bytes);
        GuestAllocations::Mutation().Add(memory, bytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), bytes);
    }

    ~WatchedSurface() {
        ClearCachedTextures(context.device);
        GuestAllocations::Mutation().Remove(memory);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(memory, span);
        GuestArena::GuestArenaRelease_nid_postfix(memory, span);
#else
        munmap(memory, span);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(memory, span);
#endif
    }

    WatchedSurface(const WatchedSurface&) = delete;
    WatchedSurface& operator=(const WatchedSurface&) = delete;

    std::uint64_t Address() const { return reinterpret_cast<std::uint64_t>(memory); }

private:
    const Context& context;
    std::size_t span = 0;
    std::uint8_t* memory = nullptr;
};

std::array<std::uint32_t, 8> Descriptor(std::uint64_t address, std::uint32_t side = Side, std::uint32_t format = Format32UInt) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::shared_ptr<Texture> Sampled(const Context& context, std::uint64_t address, std::uint32_t side = Side, std::uint32_t format = Format32UInt) {
    const auto words = Descriptor(address, side, format);
    return CachedSampledTexture(context, words);
}

GuestTextureResource Resource(std::uint64_t address, std::uint32_t side = Side, std::uint32_t format = Format32UInt) {
    const auto words = Descriptor(address, side, format);
    return DecodeTextureResource(words);
}

std::shared_ptr<ShaderResources> ImageResources(const Context& context, const Block& block, std::size_t first, std::uint32_t count, bool sampled) {
    ShaderRecompiler::DescriptorBinding binding{};
    binding.kind = sampled ? ShaderRecompiler::DescriptorKind::SampledImage : ShaderRecompiler::DescriptorKind::StorageImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = (sampled ? ShaderRecompiler::RuntimeAbi::FirstImageBinding : ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding) + PlaceholderDimensionsPerClass + 2u;
    binding.count = count;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageWritten.assign(count, false);
    binding.imageSamplers.assign(count, 0u);
    for (std::uint32_t element = 0; element < count; ++element) {
        const auto words = Descriptor(block.Surface(first + element));
        binding.guestDescriptor.insert(binding.guestDescriptor.end(), words.begin(), words.end());
    }
    ShaderRecompiler::RecompileResult program;
    program.bindings.push_back(std::move(binding));
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    return std::make_shared<ShaderResources>(context, shader);
}

void recorderEvictionTests(const Context& context, const Block& block) {
    auto& cache = SharedResourceCache();
    cache.Clear();
    ClearCachedTextures(context.device);
    auto sampled = ImageResources(context, block, 0, 1, true);
    const auto images = sampled->StorageImages();
    Require(images.size() == 1, "the eviction fixture has no sampled storage image");
    const std::weak_ptr<ShaderResources> resources = sampled;
    const std::weak_ptr<Texture> view = sampled->SampledTextures().front();
    const std::weak_ptr<StorageTexture> image = sampled->SampledTextures().front()->SharedStorageSource();
    cache.Insert({1}, sampled);
    Require(cache.Find({1}) == sampled, "the recorder fixture's descriptor set was not cached");
    sampled.reset();
    Buffer readback(context, sizeof(std::uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        Recorder recorder(context);
        recorder.Activate();
        recorder.Commands();
        for (std::size_t surface = 1; surface < Surfaces; ++surface) CachedStorageSurface(context, Resource(block.Surface(surface)));
        Require(cache.Find({1}) == nullptr, "storage eviction left the recorded descriptor set cached");
        Require(!resources.expired() && !view.expired() && !image.expired(), "eviction destroyed the recorder's descriptor set, view or image");
        Require(!image.lock()->Cached(), "the recorder fixture did not evict the source image");
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {1, 1, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, images.front().first, VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Sync();
        std::uint32_t pixel = 0;
        std::memcpy(&pixel, readback.Bytes().data(), sizeof(pixel));
        Require(pixel == 0x5a5a5a5a, "eviction changed the recorder's storage image pixel");
    }
    Require(resources.expired() && view.expired() && image.expired(), "an evicted image outlived its completed recorder");
    std::puts("storage eviction recorder lifetime and pixel readback passed");
}

bool resourceCacheTests(Context context, const std::string& mode) {
    PlaceholderImages placeholders(context);
    context.placeholderImages = &placeholders;
    TextureCache textures(context);
    context.textureCache = &textures;
    Block block(context);
    if (context.hostImportAlignment == 0 || HostImportFor(context, block.Surface(0), BlockBytes) == nullptr) {
        Require(std::getenv("ANYPS5_REQUIRE_VULKAN") == nullptr, "resource cache tests require host imports");
        std::puts("skipped, resource cache tests require host imports");
        return false;
    }
    auto& cache = SharedResourceCache();
    cache.Clear();
    try {
        if (mode == "resource-clear") {
            auto storage = ImageResources(context, block, 0, 1, false);
            auto sampled = ImageResources(context, block, 0, 1, true);
            Require(sampled->SampledTextures().front()->ViewsStorageImage(), "the sampled resource is not a storage-image view");
            const std::weak_ptr<StorageTexture> image = StorageTexture::FindLive(block.Surface(0), SurfaceBytes);
            const std::weak_ptr<ShaderResources> storageSet = storage;
            const std::weak_ptr<ShaderResources> sampledSet = sampled;
            cache.Insert({1}, storage);
            cache.Insert({2}, sampled);
            Require(cache.Size() == 2, "the resource cache did not accept the live descriptor sets");
            sampled.reset();
            ClearCachedTextures(context.device);
            Require(cache.Find({1}) == nullptr && cache.Find({2}) == nullptr && cache.Size() == 0, "clearing storage images left their resources cached");
            Require(sampledSet.expired(), "cache clear kept the sampled descriptor set alive");
            Require(!image.expired() && !storageSet.expired(), "cache clear destroyed an active caller's resources");
            storage.reset();
            Require(image.expired() && storageSet.expired(), "cleared storage image outlived its last active resource");
        } else if (mode == "resource-oversized") {
            for (const bool sampledImages : {false, true}) {
                const std::weak_ptr<StorageTexture> early = CachedStorageSurface(context, Resource(block.Surface(0)));
                auto oversized = ImageResources(context, block, 0, static_cast<std::uint32_t>(Surfaces), sampledImages);
                const std::weak_ptr<ShaderResources> rejected = oversized;
                auto image = early.lock();
                Require(oversized->Reusable(), "the oversized descriptor set is not eligible for resource caching");
                Require(image != nullptr && !image->Cached(), "the oversized descriptor set did not evict an early image while building");
                if (sampledImages) Require(oversized->SampledTextures().size() == Surfaces && oversized->SampledTextures().front()->StorageSource() == image.get(), "the oversized sampled descriptor set does not retain the early storage image");
                std::vector<std::shared_ptr<ShaderResources>> displaced;
                cache.Insert({1}, oversized, &displaced);
                Require(cache.Size() == 0 && displaced.empty(), sampledImages ? "an oversized sampled descriptor set entered the resource cache" : "an oversized storage descriptor set entered the resource cache");
                image.reset();
                oversized.reset();
                Require(rejected.expired() && early.expired(), "the rejected oversized descriptor set retained an evicted storage image");
                std::puts(sampledImages ? "resource cache oversized sampled lifetime passed" : "resource cache oversized storage lifetime passed");
                ClearCachedTextures(context.device);
            }
        } else {
            auto storage = ImageResources(context, block, 0, 1, false);
            auto sampled = ImageResources(context, block, 0, 1, true);
            Require(storage->Reusable() && sampled->Reusable(), "the stale descriptor sets are not eligible for resource caching");
            Require(sampled->SampledTextures().front()->ViewsStorageImage(), "the sampled resource is not a storage-image view");
            auto image = StorageTexture::FindLive(block.Surface(0), SurfaceBytes);
            const std::weak_ptr<StorageTexture> staleImage = image;
            const std::weak_ptr<ShaderResources> storageSet = storage;
            const std::weak_ptr<ShaderResources> sampledSet = sampled;
            Require(image != nullptr && image->Cached(), "the resource has no cached storage image");
            for (std::size_t surface = 1; surface < Surfaces && image->Cached(); ++surface) CachedStorageSurface(context, Resource(block.Surface(surface)));
            Require(!image->Cached(), "the test never evicted the oldest storage image");
            auto live = ImageResources(context, block, Surfaces - 1, 1, false);
            cache.Insert({1}, live);
            Require(cache.Find({1}) == live, "the resource cache rejected a live descriptor set");
            std::vector<std::shared_ptr<ShaderResources>> displaced;
            for (const auto& stale : {storage, sampled}) {
                cache.Insert({2}, stale, &displaced);
                Require(cache.Find({2}) == nullptr && cache.Size() == 1 && displaced.empty(), "an uncached image was admitted to the resource cache");
                cache.Insert({1}, stale, &displaced);
                Require(cache.Find({1}) == live && cache.Size() == 1 && displaced.empty(), "an uncached image replaced a live resource-cache entry");
            }
            image.reset();
            storage.reset();
            Require(storageSet.expired() && !sampledSet.expired() && !staleImage.expired(), "stale storage release did not preserve the sampled caller's image");
            std::puts("resource cache stale storage caller release passed");
            sampled.reset();
            Require(sampledSet.expired() && staleImage.expired(), "the rejected descriptor sets retained an evicted storage image");
            std::puts("resource cache stale sampled caller release passed");
        }
        if (mode == "resource-admission") recorderEvictionTests(context, block);
    } catch (...) {
        cache.Clear();
        throw;
    }
    cache.Clear();
    return true;
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
    const std::weak_ptr<StorageTexture> storage = view->SharedStorageSource();
    const auto after = TextureCacheUsage();
    Require(after.sampledEntries == before.sampledEntries + 1u, "the view did not enter the sampled texture cache");
    Require(after.sampledBytes == before.sampledBytes, "a view of a cached storage image counted " + std::to_string(after.sampledBytes - before.sampledBytes) + " bytes against the sampled texture cache");
    Require(after.storageBytes > before.storageBytes, "the storage image of the view is not counted by the storage image cache");
    Require(Sampled(context, block.Surface(0)) == view, "the second lookup of the surface did not hit its view");
    view.reset();
    for (std::size_t surface = 1; surface < Surfaces; ++surface) CachedStorageSurface(context, Resource(block.Surface(surface)));
    Require(viewed.expired() && storage.expired(), "the sampled texture cache kept the view of an evicted storage image");
    const auto evicted = TextureCacheUsage();
    Require(evicted.sampledEntries == after.sampledEntries - 1u && evicted.sampledBytes == before.sampledBytes, "dropping the view of the evicted storage image changed other sampled entries");
    auto again = Sampled(context, block.Surface(0));
    Require(again->ViewsStorageImage() && again->StorageSource() != nullptr && StorageImageCached(context, again->StorageSource()), "the surface's next lookup does not view its new cached storage image");
    const std::weak_ptr<Texture> heldView = again;
    const std::weak_ptr<StorageTexture> heldStorage = again->SharedStorageSource();
    for (std::size_t surface = 1; surface < Surfaces; ++surface) CachedStorageSurface(context, Resource(block.Surface(surface)));
    Require(!StorageImageCached(context, again->StorageSource()), "the held view's source was not evicted");
    Require(!heldView.expired() && !heldStorage.expired(), "eviction destroyed an active caller's view or image");
    again.reset();
    Require(heldView.expired() && heldStorage.expired(), "the evicted image outlived its last caller without a sampled lookup");
}

ShaderRecompiler::RecompileResult SampledProgram(std::uint64_t address) {
    ShaderRecompiler::DescriptorBinding binding{};
    binding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = ShaderRecompiler::RuntimeAbi::FirstImageBinding + PlaceholderDimensionsPerClass + 2u;
    binding.count = 1;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageWritten.assign(1, false);
    binding.imageSamplers.assign(1, 0u);
    const auto words = Descriptor(address);
    binding.guestDescriptor.assign(words.begin(), words.end());
    ShaderRecompiler::RecompileResult program;
    program.bindings.push_back(std::move(binding));
    return program;
}

void replacedEntryTests(Context context) {
    PlaceholderImages placeholders(context);
    context.placeholderImages = &placeholders;
    TextureCache textures(context);
    context.textureCache = &textures;
    WatchedSurface surface(context);
    ClearCachedTextures(context.device);
    const auto program = SampledProgram(surface.Address());
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    auto first = std::make_shared<ShaderResources>(context, shader);
    Require(first->Reusable(), "the sampled snapshot's descriptor set is not eligible for resource caching");
    Require(first->SampledTextures().size() == 1, "the sampled descriptor set binds no texture");
    const auto bound = first->SampledTextures().front();
    Require(!bound->ViewsStorageImage(), "the replaced-entry fixture is not a snapshot");
    Require(Sampled(context, surface.Address()) == bound, "the descriptor set does not bind the cache's texture");
    ShaderResources::ProofReport report{};
    Require(first->Revalidate(shader), "a template over an unchanged, still cached texture did not prove current");
    Require(first->Revalidate(shader, &report), "a template over an unchanged, still cached texture did not prove current twice");
    Require(report.path == ShaderResources::ProofPath::Fast, "the fixture's template did not take the fast proof");
    Require(first->SampledTextures().front() == bound, "the proof changed the bound texture");
    ClearCachedTextures(context.device);
    Require(TextureCacheUsage().sampledEntries == 0, "clearing the sampled cache left entries");
    Require(!first->Revalidate(shader, &report), "a template binding a texture whose cache entry left still proved current");
    const auto replacement = Sampled(context, surface.Address());
    Require(replacement != bound, "the lookup after the clear handed back the old texture");
    Require(!first->Revalidate(shader), "a template binding the replaced texture proved current against the new entry");
    auto rebuilt = std::make_shared<ShaderResources>(context, shader);
    Require(rebuilt->SampledTextures().front() == replacement, "the rebuilt descriptor set does not bind the cache's new texture");
    Require(rebuilt->Revalidate(shader), "the rebuilt template did not prove current over its cached texture");
    first.reset();
    rebuilt.reset();
    ClearCachedTextures(context.device);
    std::puts("template over a replaced sampled cache entry passed");
}

bool lookupChangedTests(Context context) {
    PlaceholderImages placeholders(context);
    context.placeholderImages = &placeholders;
    TextureCache textures(context);
    context.textureCache = &textures;
    if (context.hostImportAlignment == 0) {
        Require(std::getenv("ANYPS5_REQUIRE_VULKAN") == nullptr, "the lookup-changed test requires host imports");
        std::puts("skipped, host imports are unavailable");
        return false;
    }
    WatchedSurface surface(context);
    ClearCachedTextures(context.device);
    const auto program = SampledProgram(surface.Address());
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    auto first = std::make_shared<ShaderResources>(context, shader);
    Require(first->Reusable() && first->SampledTextures().size() == 1, "the lookup-changed template is not reusable");
    const auto bound = first->SampledTextures().front();
    Require(!bound->ViewsStorageImage(), "the lookup-changed fixture is not a snapshot before the import");
    ShaderResources::ProofReport report{};
    Require(first->Revalidate(shader), "the lookup-changed template did not prove current");
    Require(first->Revalidate(shader, &report) && report.path == ShaderResources::ProofPath::Fast, "the lookup-changed template did not take the fast proof");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    Require(first->Revalidate(shader, &report), "the audit refused an unchanged template");
    Require(report.path == ShaderResources::ProofPath::Full, "a proof past the audit interval did not take the full walk");
    Require(first->SampledTextures().front() == bound, "the audit changed the bound texture");
    Require(first->Revalidate(shader, &report) && report.path == ShaderResources::ProofPath::Fast, "the proof right after an audit did not take the fast proof");
    if (HostImportFor(context, surface.Address(), SurfaceBytes) == nullptr) {
        Require(std::getenv("ANYPS5_REQUIRE_VULKAN") == nullptr, "the host import of the watched surface was refused");
        std::puts("skipped, host imports are unavailable");
        return false;
    }
    Require(!bound->Released(), "the snapshot left the cache without a lookup");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    Require(!first->Revalidate(shader, &report), "a template binding a snapshot of a surface its lookup now views through a storage image still proved current past the audit interval");
    const auto view = Sampled(context, surface.Address());
    Require(view != bound && view->ViewsStorageImage(), "the lookup of the imported surface does not view its storage image");
    auto rebuilt = std::make_shared<ShaderResources>(context, shader);
    Require(rebuilt->SampledTextures().front() == view, "the rebuilt template does not bind the storage view");
    first.reset();
    rebuilt.reset();
    ClearCachedTextures(context.device);
    std::puts("template over a changed sampled lookup passed");
    return true;
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

void recycleTests(const Context& context) {
    Block block(context);
    ClearCachedTextures(context.device);
    const auto address = block.Surface(0);
    std::memset(reinterpret_cast<void*>(address), 0xa1, SurfaceBytes);
    const auto first = Sampled(context, address);
    Require(Sampled(context, address) == first, "an unchanged surface did not serve its snapshot");
    std::memset(reinterpret_cast<void*>(address), 0xb2, SurfaceBytes);
    const auto second = Sampled(context, address);
    Require(second != first, "a changed surface did not replace its snapshot");
    std::memset(reinterpret_cast<void*>(address), 0xa1, SurfaceBytes);
    const auto third = Sampled(context, address);
    Require(third != second, "the replaced snapshot's size-equal buffer was not reread from guest memory");
    Require(Sampled(context, address) == third, "an unchanged surface did not serve its remade snapshot");
    constexpr std::uint32_t BigSide = Side * 2;
    constexpr std::size_t BigBytes = std::size_t{BigSide} * BigSide * 4u;
    const WatchedSurface watchedFirst(context, BigBytes);
    const WatchedSurface watchedSecond(context, BigBytes);
    const auto bigFirst = Sampled(context, watchedFirst.Address(), BigSide);
    Require(TextureCacheUsage().spareEntries == 1 && TextureCacheUsage().spareBytes == BigBytes, "the first big snapshot did not park its dropped " + std::to_string(BigBytes) + "-byte buffer in the spare pool");
    const auto bigSecond = Sampled(context, watchedSecond.Address(), BigSide);
    Require(TextureCacheUsage().spareEntries == 1 && TextureCacheUsage().spareBytes == BigBytes, "the second big snapshot did not take the parked spare of its size");
}

void spareAgeTests(const Context& context) {
    Block block(context);
    ClearCachedTextures(context.device);
    constexpr std::uint32_t BigSide = Side * 2;
    constexpr std::size_t BigBytes = std::size_t{BigSide} * BigSide * 4u;
    {
        const WatchedSurface watched(context, BigBytes);
        const auto first = Sampled(context, watched.Address(), BigSide);
        Require(TextureCacheUsage().spareEntries == 1 && TextureCacheUsage().spareBytes == BigBytes, "the big snapshot did not park its dropped buffer in the spare pool");
        for (std::uint64_t present = 0; present <= SpareSnapshotPresents; ++present) Recorder::CountPresent();
        const auto small = Sampled(context, block.Surface(0));
        Require(TextureCacheUsage().spareEntries == 0 && TextureCacheUsage().spareBytes == 0, "a spare idle for " + std::to_string(SpareSnapshotPresents + 1u) + " presents was not released");
    }
    ClearCachedTextures(context.device);
    {
        const WatchedSurface watchedFirst(context, BigBytes);
        const WatchedSurface watchedSecond(context, BigBytes);
        const auto second = Sampled(context, watchedFirst.Address(), BigSide);
        Require(TextureCacheUsage().spareEntries == 1 && TextureCacheUsage().spareBytes == BigBytes, "the second big snapshot did not park its dropped buffer in the spare pool");
        for (std::uint64_t present = 0; present < SpareSnapshotPresents; ++present) Recorder::CountPresent();
        const auto third = Sampled(context, watchedSecond.Address(), BigSide);
        Require(TextureCacheUsage().spareEntries == 1 && TextureCacheUsage().spareBytes == BigBytes, "a spare taken within " + std::to_string(SpareSnapshotPresents) + " presents was released instead of reused");
    }
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

void releasedTests(Context context) {
    PlaceholderImages placeholders(context);
    context.placeholderImages = &placeholders;
    TextureCache textures(context);
    context.textureCache = &textures;
    ClearCachedTextures(context.device);
    auto& cache = SharedResourceCache();
    cache.Clear();
    try {
        Block block(context);
        auto pinned = ImageResources(context, block, 0, 1, true);
        Require(pinned->SampledTextures().size() == 1 && !pinned->SampledTextures().front()->ViewsStorageImage(), "the pinned descriptor set does not hold a sampled snapshot");
        const std::weak_ptr<Texture> texture = pinned->SampledTextures().front();
        const std::weak_ptr<ShaderResources> pinnedSet = pinned;
        cache.Insert({1}, pinned);
        Require(cache.Find({1}) == pinned, "the resource cache rejected a descriptor set whose texture is cached");
        pinned.reset();
        ClearCachedTextures(context.device);
        Require(!texture.expired() && texture.lock()->Released(), "the sampled texture cache dropped a texture the resource cache holds without marking it released");
        auto live = ImageResources(context, block, 1, 1, true);
        std::vector<std::shared_ptr<ShaderResources>> displaced;
        cache.Insert({2}, live, &displaced);
        Require(cache.Find({1}) == nullptr && cache.Find({2}) == live && cache.Size() == 1, "the resource cache kept an entry holding a released texture");
        Require(displaced.size() == 1 && displaced.front() == pinnedSet.lock(), "the entry holding a released texture was not handed back to the caller");
        displaced.clear();
        Require(pinnedSet.expired() && texture.expired(), "the resource cache kept a released texture alive");
        auto stale = ImageResources(context, block, 2, 1, true);
        ClearCachedTextures(context.device);
        Require(stale->SampledTextures().front()->Released(), "a held texture the sampled cache dropped is not marked released");
        cache.Insert({3}, stale, &displaced);
        Require(cache.Find({3}) == nullptr, "a descriptor set holding a released texture entered the resource cache");
        Require(cache.Size() == 0 && displaced.size() == 1 && displaced.front() == live, "the live entry whose texture was released stayed in the resource cache");
        displaced.clear();
    } catch (...) {
        cache.Clear();
        throw;
    }
    cache.Clear();
    ClearCachedTextures(context.device);
    std::puts("resource cache released texture tests passed");
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
    const auto snapshotBytes = TextureCacheUsage().sampledBytes;
    Require(snapshotBytes != 0, "the snapshot fixture unexpectedly became a view of a cached storage image");
    const auto capacity = static_cast<std::size_t>(Budget / snapshotBytes);
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
    auto refused = Sampled(context, block.Surface(frame - 1u));
    Require(Sampled(context, block.Surface(frame - 1u)) != refused, "a texture used a second time in the frame was cached in place of one of the frame's textures");
    Require(TextureCacheUsage().sampledEntries == capacity && TextureCacheUsage().sampledBytes <= Budget, "the texture used a second time left the cache over its budget");
    refused.reset();
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

void scanTests(const Context& context) {
    Block block(context);
    ClearCachedTextures(context.device);
    Recorder::CountPresent();
    const auto bytes = Sampled(context, block.Surface(0))->AllocationBytes();
    const auto capacity = static_cast<std::size_t>(Budget / bytes);
    const auto working = 2u * capacity;
    Require(capacity >= 2u && working <= Surfaces && TextureCacheUsage().sampledEntries == 1u, "a snapshot of the 256 KiB surface holds " + std::to_string(bytes) + " bytes, which the test's 1 MiB budget does not fit as planned");
    constexpr std::size_t Presents = 6;
    constexpr std::size_t Uses = 2;
    std::vector<std::weak_ptr<Texture>> last(working);
    std::size_t hits = 0;
    std::size_t lookups = 0;
    for (std::size_t present = 0; present < Presents; ++present) {
        Recorder::CountPresent();
        for (std::size_t surface = 0; surface < working; ++surface) {
            for (std::size_t use = 0; use < Uses; ++use) {
                const auto texture = Sampled(context, block.Surface(surface));
                if (present != 0u) {
                    hits += texture == last[surface].lock();
                    ++lookups;
                }
                last[surface] = texture;
            }
        }
    }
    Require(TextureCacheUsage().sampledBytes <= Budget, "the sampled texture cache holds more than its budget after a scan");
    const auto wanted = (Presents - 1u) * Uses * capacity / 2u;
    Require(hits >= wanted, "a cyclic scan using each of " + std::to_string(working) + " textures twice in a row, twice the " + std::to_string(capacity) + " the budget holds, found " + std::to_string(hits) + " of " + std::to_string(lookups) + " lookups cached, fewer than " + std::to_string(wanted));
}

void shrinkTests(const Context& context) {
    Block block(context);
    ClearCachedTextures(context.device);
    Recorder::CountPresent();
    OverrideCacheBudgetForTest(2u * Budget);
    const auto bytes = Sampled(context, block.Surface(0))->AllocationBytes();
    const auto capacity = static_cast<std::size_t>((2u * Budget) / bytes);
    Require(capacity >= 4u && capacity + 1u <= Surfaces, "a snapshot of the 256 KiB surface holds " + std::to_string(bytes) + " bytes, which the test's 2 MiB budget does not fit as planned");
    for (std::size_t surface = 1; surface < capacity; ++surface) Sampled(context, block.Surface(surface));
    Require(TextureCacheUsage().sampledEntries == capacity, "the 2 MiB budget did not keep " + std::to_string(capacity) + " snapshots");
    Recorder::CountPresent();
    OverrideCacheBudgetForTest(Budget);
    for (std::size_t surface = 0; surface < capacity; ++surface) Sampled(context, block.Surface(surface));
    Sampled(context, block.Surface(capacity));
    Require(TextureCacheUsage().sampledBytes <= Ceiling, "after the budget fell to 1 MiB, one insert left " + std::to_string(TextureCacheUsage().sampledBytes) + " bytes cached with every entry in use this frame, past a quarter over the budget");
    OverrideCacheBudgetForTest(0);
}

void secondUseTests(const Context& context) {
    Block block(context);
    ClearCachedTextures(context.device);
    Recorder::CountPresent();
    const auto bytes = Sampled(context, block.Surface(0))->AllocationBytes();
    const auto capacity = static_cast<std::size_t>(Budget / bytes);
    Require(capacity >= 2u && capacity < Surfaces && TextureCacheUsage().sampledEntries == 1u, "a snapshot of the 256 KiB surface holds " + std::to_string(bytes) + " bytes, which the test's 1 MiB budget does not fit as planned");
    for (std::size_t surface = 1; surface < capacity; ++surface) Sampled(context, block.Surface(surface));
    Recorder::CountPresent();
    auto first = Sampled(context, block.Surface(capacity));
    Require(TextureCacheUsage().sampledEntries == capacity && first.use_count() == 1, "the first use in a frame of a texture the cache does not hold was cached in place of one the last frame used");
    const auto second = Sampled(context, block.Surface(capacity));
    Require(Sampled(context, block.Surface(capacity)) == second, "a texture used a second time in a frame whose least recently used entry is from the last frame was not cached");
    Require(TextureCacheUsage().sampledEntries == capacity && TextureCacheUsage().sampledBytes <= Budget, "the texture admitted on its second use left the cache over its budget");
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
    return SampledTextureBudget(context.memory, &reported, SampledTextureMemory() + TextureCacheUsage().storageBytes, TextureCacheUsage().storageBytes);
}

std::uint64_t ReadBudget(const Context& context) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    return SampledTextureCacheBudget(context);
}

constexpr std::uint64_t PressedUsage = ReportedBudget - 9ull * (1ull << 30u);

void VKAPI_PTR ReportPressure(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
    auto* reported = static_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(properties->pNext);
    for (std::uint32_t heap = 0; heap < VK_MAX_MEMORY_HEAPS; ++heap) {
        reported->heapBudget[heap] = ReportedBudget;
        reported->heapUsage[heap] = PressedUsage;
    }
}

std::uint64_t ReadStorageBudget(const Context& context) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    return StorageTextureCacheBudget(context);
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

void storageMemoryTests(Context context) {
    constexpr std::uint64_t GiB = 1ull << 30u;
    context.memoryProperties2 = &ReportBudget;
    Block block(context);
    auto held = Sampled(context, block.Surface(0));
    const auto heap = LargestDeviceLocalHeap(context.memory);
    Require(heap.has_value(), "the device has no device-local heap");
    context.memory.memoryHeaps[*heap].size = std::max<VkDeviceSize>(context.memory.memoryHeaps[*heap].size, ReportedBudget);
    const auto half = std::min<std::uint64_t>(context.memory.memoryHeaps[*heap].size / 2u, ReportedBudget / 2u);
    Require(ReadStorageBudget(context) == half, "52 GiB free of a 64 GiB budget does not leave the storage image cache half of the heap or of the budget");
    context.memoryProperties2 = &ReportPressure;
    const auto pressed = ReadStorageBudget(context);
    const auto expected = GiB + TextureCacheUsage().storageBytes;
    Require(pressed == expected, "55 GiB in use of a 64 GiB budget leaves the storage image cache " + std::to_string(pressed >> 20u) + " MiB, not the " + std::to_string(expected >> 20u) + " MiB left after the use outside the texture caches, the sampled textures and 8 GiB of headroom");
    std::printf("reported heap budget %llu MiB with %llu MiB in use: storage image cache budget %llu MiB\n", static_cast<unsigned long long>(ReportedBudget >> 20u), static_cast<unsigned long long>(PressedUsage >> 20u), static_cast<unsigned long long>(pressed >> 20u));
}

struct RecoveryProbe {
    PFN_vkGetDeviceProcAddr deviceProc = nullptr;
    std::uint64_t released = 0;
    unsigned requests = 0;
    unsigned failures = 0;
    bool pressure = false;
};

RecoveryProbe recovery;

VkDeviceSize ReclaimForTest(VkDeviceSize bytes, bool newImage) {
    Require(newImage, "an image allocation did not identify itself to the recovery callback");
    ++recovery.requests;
    Require(ReleaseTextureCaches(bytes, false) == 0, "a buffer allocation was allowed to reclaim the cache held by image creation");
    const auto released = ReleaseTextureCaches(bytes, newImage);
    recovery.released += released;
    return released;
}

VKAPI_ATTR VkResult VKAPI_CALL AllocateForTest(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* memory) {
    if (recovery.pressure && recovery.released == 0) {
        ++recovery.failures;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    return reinterpret_cast<PFN_vkAllocateMemory>(recovery.deviceProc(device, "vkAllocateMemory"))(device, info, allocator, memory);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL RecoveryProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(AllocateForTest);
    return recovery.deviceProc(device, name);
}

void recoveryTests(Context context, const std::string& mode) {
    recovery = RecoveryProbe{};
    recovery.deviceProc = context.deviceProc;
    context.deviceProc = RecoveryProc;
    context.cacheReclaim = ReclaimForTest;
    context.bufferPool.reset();
    Block block(context);
    const bool storage = mode == "recover-storage";
    if (storage) {
        auto oldest = CachedStorageSurface(context, Resource(block.Surface(0)));
        const auto allocated = oldest->AllocationBytes();
        std::weak_ptr<StorageTexture> released = oldest;
        oldest.reset();
        auto pending = CachedStorageSurface(context, Resource(block.Surface(1)));
        pending->MarkDirty();
        recovery.pressure = true;
        auto made = CachedStorageSurface(context, Resource(block.Surface(2)));
        recovery.pressure = false;
        Require(made != nullptr && released.expired(), "storage allocation recovery did not release its own oldest clean image");
        Require(pending->Dirty() && StorageImageCached(context, pending.get()), "storage allocation recovery changed an image with pending results");
        Require(recovery.released == allocated, "storage allocation recovery counted memory still held by its caller");
        pending->Flush();
        Require(!pending->Dirty(), "pending storage results could not be stored after memory recovery");
    } else if (mode == "recover-sampled") {
        auto oldest = Sampled(context, block.Surface(0));
        Require(!oldest->ViewsStorageImage(), "the sampled recovery fixture needs a snapshot allocation");
        const auto allocated = oldest->AllocationBytes();
        std::weak_ptr<Texture> released = oldest;
        oldest.reset();
        auto held = Sampled(context, block.Surface(1));
        recovery.pressure = true;
        auto made = Sampled(context, block.Surface(2));
        recovery.pressure = false;
        Require(made != nullptr && released.expired(), "sampled allocation recovery did not release its own oldest unused texture");
        Require(Sampled(context, block.Surface(1)) == held, "sampled allocation recovery removed a texture still held by its caller");
        Require(recovery.released == allocated, "sampled allocation recovery counted memory still held by its caller");
    } else {
        const bool srgb = mode == "recover-srgb";
        if (srgb) {
            context.srgbDecodeFormats = SrgbDecodeFormats(context.formatProperties, context.physical);
            context.srgbCopyFormats = SrgbCopyFormats(context.formatProperties, context.physical, context.srgbDecodeFormats);
            Require(SrgbExpandsToRgba8(context, 128u), "the device cannot serve the sRGB expansion copy fixture");
        }
        auto oldest = Sampled(context, block.Surface(0));
        Require(!oldest->ViewsStorageImage(), "the sampled recovery fixture needs a snapshot allocation");
        const auto allocated = oldest->AllocationBytes();
        std::weak_ptr<Texture> released = oldest;
        oldest.reset();
        auto held = Sampled(context, block.Surface(1));
        const auto side = srgb ? 1024u : 512u;
        const auto format = srgb ? 128u : 43u;
        const auto sourceWords = Descriptor(block.Surface(4), side, format);
        const auto sourceResource = DecodeTextureResource(sourceWords);
        auto source = CachedStorageSurface(context, sourceResource);
        source->MarkDirty();
        if (srgb) {
            Require(Texture::CanSrgbExpandFrom(context, *source, sourceResource), "the storage surface cannot serve an sRGB expansion copy");
        } else {
            Require(Texture::CanRepackCopyFrom(*source, sourceResource), "the storage surface cannot serve a repacked copy");
        }
        recovery.pressure = true;
        auto made = CachedSampledTexture(context, sourceWords);
        recovery.pressure = false;
        Require(made != nullptr && made->SharedStorageSource() == source, "sampled copy allocation recovery did not serve the pending storage image");
        Require(released.expired(), "sampled copy allocation recovery did not release its own oldest unused texture");
        Require(source->Dirty() && StorageImageCached(context, source.get()), "sampled copy allocation recovery changed or evicted an image with pending results");
        Require(!held->Released() && held.use_count() == 2, "sampled copy allocation recovery removed a texture still held by its caller");
        const auto refreshed = Sampled(context, block.Surface(1));
        Require(refreshed != nullptr && (refreshed == held || refreshed->ViewsStorageImage()), "sampled copy cache refresh did not serve the surface");
        Require(recovery.released == allocated, "sampled copy allocation recovery counted memory still held by its caller");
        source->Flush();
        Require(!source->Dirty(), "pending storage results could not be stored after memory recovery");
    }
    Require(recovery.requests == 1 && recovery.failures >= 1 && recovery.failures <= 2, "image allocation recovery was skipped or retried without a bound");
    const char* label = storage ? "storage" : mode == "recover-srgb" ? "sRGB" : mode == "recover-repack" ? "repacked" : "sampled";
    std::printf("%s image allocation recovered after %u injected failures: %llu bytes released\n", label, recovery.failures, static_cast<unsigned long long>(recovery.released));
}

}

int main(int argc, char** argv) {
    try {
        const bool memory = argc > 1 && std::string(argv[1]) == "memory";
        const std::string mode = argc > 1 ? argv[1] : "";
        if (mode == "device-selection-cpu") {
            deviceSelectionTests();
            return 0;
        }
        const bool resources = mode == "resource-admission" || mode == "resource-oversized" || mode == "resource-clear";
        const bool disableStorageImageMultisample = mode == "without-storage-image-multisample";
        const bool recover = mode == "recover-sampled" || mode == "recover-storage" || mode == "recover-srgb" || mode == "recover-repack";
#ifdef _WIN32
        if (!memory) _putenv_s("APS5_TEXTURE_CACHE_MIB", recover ? "16" : "1");
        if (std::getenv("APS5_TEMPLATE_AUDIT_MS") == nullptr) _putenv_s("APS5_TEMPLATE_AUDIT_MS", "250");
#else
        if (!memory) setenv("APS5_TEXTURE_CACHE_MIB", recover ? "16" : "1", 1);
        setenv("APS5_TEMPLATE_AUDIT_MS", "250", 0);
#endif
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>(!disableStorageImageMultisample);
        } catch (const std::exception& error) {
            const char* requestedName = std::getenv("ANYPS5_GPU");
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr || (requestedName != nullptr && *requestedName != '\0')) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        if (mode == "device-selection") {
            device->VerifySelection();
            return 0;
        }
        auto context = device->GetContext();
        if (disableStorageImageMultisample) {
            PlaceholderImages placeholders(context);
            for (std::uint32_t heap = ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding;
                 heap < ShaderRecompiler::RuntimeAbi::FirstImageBinding + ShaderRecompiler::RuntimeAbi::ImageBindingCount; ++heap) {
                const auto shape = PlaceholderImageShapeOf(heap);
                if (shape.samples == VK_SAMPLE_COUNT_1_BIT) continue;
                bool refused = false;
                try {
                    placeholders.View(heap);
                } catch (const std::runtime_error&) {
                    refused = true;
                }
                Require(refused, "a device without storage-image MSAA returned a placeholder for typed multisample storage heap " + std::to_string(heap));
            }
        }
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        if (resources) {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            if (!resourceCacheTests(context, mode)) return VulkanTestSkipped;
            std::puts("resource cache tests passed");
            return 0;
        }
        if (mode == "resource-replaced") {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            replacedEntryTests(context);
            std::puts("resource cache replaced entry tests passed");
            return 0;
        }
        if (mode == "resource-lookup-changed") {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            if (!lookupChangedTests(context)) return VulkanTestSkipped;
            std::puts("resource cache lookup-changed tests passed");
            return 0;
        }
        if (memory) {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            memoryTests(context);
            storageMemoryTests(context);
            ClearCachedTextures(context.device);
            std::puts("sampled texture memory budget tests passed");
            return 0;
        }
        if (recover) {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            recoveryTests(context, mode);
            return 0;
        }
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            reportedBudgetTests(context);
            snapshotTests(context);
            recycleTests(context);
            heldTests(context);
            releasedTests(context);
            rescanTests(context);
            viewEvictionTests(context);
            viewTests(context);
            scanTests(context);
            frameTests(context);
            shrinkTests(context);
            secondUseTests(context);
            earlyMissTests(context);
            collidingKeyTests(context);
            spareAgeTests(context);
            ClearCachedTextures(context.device);
        }
        std::puts("sampled texture cache budget tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
