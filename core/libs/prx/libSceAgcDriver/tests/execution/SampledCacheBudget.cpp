#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
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
#include <span>
#include <algorithm>
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

class Device {
public:
    explicit Device(bool resourceImages = false, bool selectGpu = false) {
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
            if (selectGpu) {
                const char* selected = std::getenv("ANYPS5_GPU");
                if (selected != nullptr) {
                    const std::uint32_t vendor = std::string(selected) == "AMD" ? 0x1002u : std::string(selected) == "NVIDIA" ? 0x10deu : 0u;
                    Require(vendor != 0, "ANYPS5_GPU must be AMD or NVIDIA for this fixture");
                    context.physical = VK_NULL_HANDLE;
                    for (const auto candidate : devices) {
                        VkPhysicalDeviceProperties properties{};
                        function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(candidate, &properties);
                        if (properties.vendorID == vendor) { context.physical = candidate; break; }
                    }
                    Require(context.physical != VK_NULL_HANDLE, "requested fixture GPU is unavailable");
                }
                VkPhysicalDeviceProperties selectedProperties{};
                function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &selectedProperties);
                std::printf("fixture GPU=%s vendor=0x%x device=0x%x\n", selectedProperties.deviceName, selectedProperties.vendorID, selectedProperties.deviceID);
            }
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
            if (resourceImages) {
                VkPhysicalDeviceFeatures supported{};
                function<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(context.physical, &supported);
                enabled.shaderStorageImageMultisample = supported.shaderStorageImageMultisample;
            }
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

std::array<std::uint32_t, 8> Descriptor(std::uint64_t address, std::uint32_t side = Side) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
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


#ifdef _WIN32
void CheckSnapshotWords(std::span<const std::byte> bytes, std::uint32_t expected, const char* label) {
    Require(bytes.size() % 4 == 0, "word oracle has nonword extent");
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        std::uint32_t actual;
        std::memcpy(&actual, bytes.data() + offset, 4);
        if (actual != expected) {
            char detail[180];
            std::snprintf(detail, sizeof(detail), "%s word=%zu expected=0x%08x actual=0x%08x", label, offset / 4, expected, actual);
            throw std::runtime_error(detail);
        }
    }
}

class PendingSnapshotBlock {
public:
    static constexpr std::size_t GuardBytes = 65536;
    static constexpr std::size_t Bytes = 2 * SurfaceBytes + 3 * GuardBytes;
    static constexpr std::uint32_t Initial = 0x15263748u, Guard = 0x6d29c853u;
    explicit PendingSnapshotBlock(const Context& context) : context(context) {
        base = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Bytes, GuardBytes));
        Require(base != nullptr, "pending snapshot: guest allocation failed");
        GuestArena::GuestArenaCommit_nid_postfix(base, Bytes, PAGE_READWRITE, GuardBytes);
        GuestAllocations::Mutation().Add(base, Bytes - 256, true, true, true);
        std::fill_n(reinterpret_cast<std::uint32_t*>(base), Bytes / 4, Guard);
        for (const auto address : {Sample(), Disjoint()})
            std::fill_n(reinterpret_cast<std::uint32_t*>(address), SurfaceBytes / 4, Initial);
    }
    ~PendingSnapshotBlock() {
        ClearCachedTextures(context.device);
        GuestAllocations::Mutation().Remove(base);
        GuestArena::GuestArenaReset_nid_postfix(base, Bytes);
        GuestArena::GuestArenaRelease_nid_postfix(base, Bytes);
    }
    std::uint64_t Sample() const { return reinterpret_cast<std::uint64_t>(base) + GuardBytes; }
    std::uint64_t Disjoint() const { return Sample() + SurfaceBytes + GuardBytes; }
    void Check(std::uint32_t sample, std::uint32_t disjoint) const {
        CheckSnapshotWords({reinterpret_cast<const std::byte*>(Sample()), SurfaceBytes}, sample, "sampled guest backing");
        CheckSnapshotWords({reinterpret_cast<const std::byte*>(Disjoint()), SurfaceBytes}, disjoint, "disjoint guest backing");
        for (const auto offset : {std::size_t{0}, GuardBytes + SurfaceBytes, 2 * GuardBytes + 2 * SurfaceBytes})
            CheckSnapshotWords({base + offset, GuardBytes}, Guard, "pending snapshot guard");
    }
private:
    Context context;
    std::byte* base = nullptr;
};

void pendingSnapshotTests(const Context& context, bool disjoint) {
    namespace GM = AgcDriver::GuestMemory;
    for (const char* name : {"APS5_NO_TEXTURE_CACHE", "APS5_GPU_NO_WRITEBACK"})
        Require(std::getenv(name) == nullptr, std::string("pending snapshot fixture requires unset ") + name);
    PendingSnapshotBlock memory(context);
    Recorder recorder(context);
    recorder.Activate();
    const auto address = memory.Sample();
    const auto words = Descriptor(address);
    Require(DescribeSurface(DecodeTextureResource(words)).guestBytes == SurfaceBytes, "pending snapshot: unexpected descriptor extent");
    GM::BumpCollectEpoch();
    Require(GM::WriteWatched() && GM::CollectWrites(address, SurfaceBytes) != 0, "pending snapshot: full watched proof unavailable");
    auto previous = CachedSampledTexture(context, words);
    Require(previous != nullptr && !previous->ViewsStorageImage(), "pending snapshot: lookup did not create an owned snapshot");
    recorder.Sync();
    Require(CachedSampledTexture(context, words) == previous, "pending snapshot: initial unchanged lookup did not reuse");
    Require(GM::Watched(address, SurfaceBytes), "pending snapshot: snapshot setup removed write watching");
    memory.Check(PendingSnapshotBlock::Initial, PendingSnapshotBlock::Initial);
    std::uint32_t sampleExpected = PendingSnapshotBlock::Initial, disjointExpected = PendingSnapshotBlock::Initial;
    for (const bool submitted : {false, true}) {
        const auto destination = disjoint ? memory.Disjoint() : address;
        const std::uint32_t value = submitted ? 0x392b17a5u : 0x725c183eu;
        const auto copied = std::make_shared<Buffer>(context, SurfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        std::fill(copied->Bytes().begin(), copied->Bytes().end(), std::byte{0xa6});
        const auto landed = std::make_shared<bool>(false);
        const auto epoch = GM::ThreadCollectEpoch();
        const auto commands = recorder.Commands();
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, copied->Handle(), 0, SurfaceBytes, value);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.OnComplete([copied, landed, destination, value] {
            copied->Invalidate();
            CheckSnapshotWords(copied->Bytes(), value, "GPU copied-buffer output");
            GM::StoreOwnBytes(destination, copied->Bytes());
            *landed = true;
        });
        recorder.Keep(copied);
        recorder.NotePendingWrite(destination, SurfaceBytes);
        if (submitted) recorder.Submit();
        Require(!*landed && Recorder::SnapshotWriteOverlaps(destination, SurfaceBytes), "pending snapshot: copied writer was not pending");
        Require(Recorder::SnapshotWriteOverlaps(address, SurfaceBytes) != disjoint, "pending snapshot: overlap geometry differs");
        const auto next = CachedSampledTexture(context, words);
        Require(GM::ThreadCollectEpoch() == epoch, "pending snapshot: lookup changed packet epoch");
        if (disjoint) {
            Require(next == previous && !*landed && Recorder::SnapshotWriteOverlaps(destination, SurfaceBytes),
                    "pending snapshot: disjoint lookup waited or replaced its unchanged snapshot");
            memory.Check(sampleExpected, disjointExpected);
            recorder.Sync();
            disjointExpected = value;
        } else {
            Require(*landed && !Recorder::SnapshotWriteOverlaps(destination, SurfaceBytes),
                    "pending snapshot: overlapping lookup returned before copied write publication");
            Require(next != previous && !next->ViewsStorageImage(), "pending snapshot: overlapping lookup reused stale snapshot");
            sampleExpected = value;
            recorder.Sync();
        }
        Require(*landed && !Recorder::SnapshotWriteOverlaps(destination, SurfaceBytes), "pending snapshot: explicit completion did not publish");
        memory.Check(sampleExpected, disjointExpected);
        Require(CachedSampledTexture(context, words) == next, "pending snapshot: completed snapshot was not reused");
        previous = next;
        std::printf("snapshot_pending_write_case=PASS disjoint=%u submitted=%u full_gpu_and_guest_words=%zu guards=%zu\n",
                    static_cast<unsigned>(disjoint), static_cast<unsigned>(submitted), SurfaceBytes / 4, 3 * PendingSnapshotBlock::GuardBytes / 4);
    }
    recorder.Sync();
}
#endif

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
        const std::string mode = argc > 1 ? argv[1] : "";
        const bool pendingSnapshot = mode == "snapshot-pending-write" || mode == "snapshot-pending-write-disjoint";
        const bool resources = mode == "resource-admission" || mode == "resource-oversized" || mode == "resource-clear";
#ifdef _WIN32
        if (!memory) _putenv_s("APS5_TEXTURE_CACHE_MIB", "1");
#else
        if (!memory) setenv("APS5_TEXTURE_CACHE_MIB", "1", 1);
#endif
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>(resources, pendingSnapshot);
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        auto context = device->GetContext();
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        if (pendingSnapshot) {
#ifdef _WIN32
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            pendingSnapshotTests(context, mode == "snapshot-pending-write-disjoint");
            std::puts("direct sampled snapshot pending write tests passed");
            return 0;
#else
            std::puts("skipped, pending snapshot write-watch control requires Windows");
            return VulkanTestSkipped;
#endif
        }
        if (resources) {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            if (!resourceCacheTests(context, mode)) return VulkanTestSkipped;
            std::puts("resource cache tests passed");
            return 0;
        }
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
            ClearCachedTextures(context.device);
        }
        std::puts("sampled texture cache budget tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
