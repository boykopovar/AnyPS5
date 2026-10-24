#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#include <vector>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Side = 64;
constexpr std::size_t BlockBytes = 65536;
alignas(4096) std::array<std::uint8_t, BlockBytes * 2> Storage{};
alignas(4096) std::array<std::uint8_t, BlockBytes * 2> CopyStorage{};
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Format8Uint = 5;

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


template<typename TAction>
void ExpectFailure(TAction action, const char* expected, const char* message) {
    try {
        action();
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, error.what());
        return;
    }
    throw std::runtime_error(message);
}

GuestTextureResource PlaneResource(std::uint64_t address, std::uint32_t format, std::uint32_t side = Side) {
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2D;
    resource.format = format;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    return resource;
}

std::size_t CountFloats(const std::uint8_t* bytes, float value) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < Side * Side; ++i) {
        float texel = 0;
        std::memcpy(&texel, bytes + i * sizeof(float), sizeof(texel));
        if (texel == value) ++count;
    }
    return count;
}

void Run(Context context, std::uint8_t* block) {
    const auto address = reinterpret_cast<std::uint64_t>(block);
    const auto stencilAddress = address + BlockBytes / 2;
    const auto submit = [&](Recorder& recorder) {
        recorder.Submit();
        Check(context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue), "vkQueueWaitIdle");
    };
    Recorder recorder(context);
    recorder.Activate();
    const auto readBack = [&](StorageTexture& texture) {
        texture.MarkDirty();
        texture.WriteBack();
        submit(recorder);
    };
    const DepthTarget target{address, stencilAddress, {Side, Side}, VK_FORMAT_D32_SFLOAT_S8_UINT, 0.25f, 7};
    Require(DepthSurfaceView(context, target) != VK_NULL_HANDLE, "depth surface was not created");
    Require(DepthSurfaceStorage(context, {}, PlaneResource(address, Format32Float, Side / 2), 0) == nullptr, "a storage view of another extent over the depth memory was taken for the depth surface");
    Require(DepthSurfaceTexture(context, {}, PlaneResource(address, Format32Float, Side / 2), {}) == nullptr, "a sampled view of another extent over the depth memory was taken for the depth surface");
    Require(DepthSurfaceStorage(context, {}, PlaneResource(address, Format8Uint), 0) == nullptr, "an 8-bit storage view of the depth plane was taken for the depth surface");
    Require(DepthSurfaceStorage(context, {}, PlaneResource(address, Format32Float), 1) == nullptr, "a storage access to a later level over the depth memory was taken for the depth surface");
    auto later = PlaneResource(address, Format32Float);
    later.mipCount = 2;
    later.baseLevel = 1;
    later.lastLevel = 1;
    Require(DepthSurfaceStorage(context, {}, later, 0) == nullptr, "a storage view of a later level over the depth memory was taken for the depth surface");
    Require(DepthSurfaceTexture(context, {}, later, {}) == nullptr, "a sampled view of a later level over the depth memory was taken for the depth surface");
    const auto depth = DepthSurfaceStorage(context, {}, PlaneResource(address, Format32Float), 0);
    Require(depth != nullptr, "the depth plane has no storage image");
    submit(recorder);
    readBack(*depth);
    Require(CountFloats(block, 0.25f) == Side * Side, "the storage image did not receive the cleared depth");
    const std::array<std::uint32_t, 4> pattern{std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f)};
    const char* refusal = nullptr;
    Require(depth->FillClear(pattern, StorageTexture::WholeImage, refusal), refusal != nullptr ? refusal : "the storage image could not be filled");
    Require(DepthSurfaceStoreBack(context, *depth), "the depth plane's storage image was not stored back");
    submit(recorder);
    Require(DepthSurfaceStorage(context, {}, PlaneResource(address, Format32Float), 0) == depth, "the depth plane's storage image was not reused");
    submit(recorder);
    readBack(*depth);
    Require(CountFloats(block, 0.75f) == Side * Side, "the stored depth did not come back from the depth surface");
    const auto stencil = DepthSurfaceStorage(context, {}, PlaneResource(stencilAddress, Format8Uint), 0);
    Require(stencil != nullptr && stencil != depth, "the stencil plane has no storage image of its own");
    submit(recorder);
    readBack(*stencil);
    const auto stencilBytes = DescribeSurface(PlaneResource(stencilAddress, Format8Uint)).guestBytes;
    Require(stencilBytes <= BlockBytes / 2 && std::count(block + BlockBytes / 2, block + BlockBytes / 2 + stencilBytes, 7) == static_cast<std::ptrdiff_t>(Side * Side), "the storage image did not receive the cleared stencil");
    const std::array<std::uint32_t, 4> half{std::bit_cast<std::uint32_t>(0.5f), std::bit_cast<std::uint32_t>(0.5f), std::bit_cast<std::uint32_t>(0.5f), std::bit_cast<std::uint32_t>(0.5f)};
    Require(depth->FillClear(half, StorageTexture::WholeImage, refusal), refusal != nullptr ? refusal : "the storage image could not be filled");
    Require(DepthSurfaceStoreBack(context, *depth), "the depth plane's storage image was not stored back");
    submit(recorder);
    auto layeredDepth = PlaneResource(address, Format32Float);
    layeredDepth.dimension = TextureDimension::k2DArray;
    const auto arrayDepth = DepthSurfaceStorage(context, {}, layeredDepth, 0);
    Require(arrayDepth != nullptr && arrayDepth != depth, "a one-layer 2D array view of the depth plane was not taken for the depth surface as a storage image of its own");
    ExpectFailure([&] { static_cast<void>(DepthSurfaceStoreBack(context, *depth)); }, "through a view that another view of the plane", "a store back through the 2D view that the one-layer 2D array view replaced was accepted");
    submit(recorder);
    readBack(*arrayDepth);
    Require(CountFloats(block, 0.5f) == Side * Side, "the one-layer 2D array view did not receive the depth plane");
    std::fill(block + BlockBytes / 2, block + BlockBytes / 2 + stencilBytes, std::uint8_t{0});
    auto layeredStencil = PlaneResource(stencilAddress, Format8Uint);
    layeredStencil.dimension = TextureDimension::k2DArray;
    const auto arrayStencil = DepthSurfaceStorage(context, {}, layeredStencil, 0);
    Require(arrayStencil != nullptr, "a one-layer 2D array view of the stencil plane was not taken for the depth surface");
    submit(recorder);
    readBack(*arrayStencil);
    Require(std::count(block + BlockBytes / 2, block + BlockBytes / 2 + stencilBytes, 7) == static_cast<std::ptrdiff_t>(Side * Side), "the one-layer 2D array view did not receive the stencil plane");
    layeredStencil.depthOrLastArray = 1;
    ExpectFailure([&] { static_cast<void>(DepthSurfaceStorage(context, {}, layeredStencil, 0)); }, "storage image access to depth/stencil surface", "a two-layer 2D array view of the stencil plane was accepted");
    layeredDepth.depthOrLastArray = 1;
    const auto layerPitch = static_cast<std::uint64_t>(DescribeSurface(layeredDepth).GuestLayerOffset(1));
    const auto sliceBytes = DepthSliceBytes({Side, Side}, 4u);
    Require(layerPitch != sliceBytes, "the test's layered depth view has the depth slice size as its layer pitch");
    const auto layered = DepthSurfaceStorage(context, {}, layeredDepth, 0);
    Require(layered != nullptr && layered->ImageLayers() == 2, "a two-layer 2D array view over the depth plane's guest layer pitch was not taken as a storage image");
    submit(recorder);
    readBack(*layered);
    Require(CountFloats(block + layerPitch, 0.f) == Side * Side, "the two-layer 2D array view did not take the layer that only overlaps the depth slice from guest memory");
    Require(CountFloats(block, 0.5f) == Side * Side, "the two-layer 2D array view did not receive the layer the depth surface holds");
    layeredDepth.baseArray = 1;
    Require(DepthSurfaceStorage(context, {}, layeredDepth, 0) != nullptr, "a 2D array view from layer 1 of the depth plane was not taken as a storage image");
    const auto own = StorageTexture::Create(context, *context.detiler, PlaneResource(address, Format32Float), 0);
    const std::array<std::uint32_t, 4> eighth{std::bit_cast<std::uint32_t>(0.125f), std::bit_cast<std::uint32_t>(0.125f), std::bit_cast<std::uint32_t>(0.125f), std::bit_cast<std::uint32_t>(0.125f)};
    Require(own->FillClear(eighth, StorageTexture::WholeImage, refusal), refusal != nullptr ? refusal : "the storage image could not be filled");
    Require(DepthSurfaceStoreBack(context, *own), "a storage image over the depth plane that the depth surface did not hand out was not stored back");
    submit(recorder);
    const auto reread = DepthSurfaceStorage(context, {}, PlaneResource(address, Format32Float), 0);
    Require(reread != nullptr, "the depth plane has no storage image");
    submit(recorder);
    readBack(*reread);
    Require(CountFloats(block, 0.125f) == Side * Side, "a storage image written over the depth plane did not reach the depth surface");
    const float overlapping = 0.375f;
    for (std::size_t i = 0; i < Side * Side; ++i) std::memcpy(block + layerPitch + i * sizeof(overlapping), &overlapping, sizeof(overlapping));
    auto swizzled = PlaneResource(address, Format32Float);
    swizzled.tileMode = TextureTileMode::kStandard64KB;
    const auto tiled = DepthSurfaceStorage(context, {}, swizzled, 0);
    Require(tiled != nullptr, "a 64 KiB depth plane has no storage image");
    const std::array<std::uint32_t, 4> pendingFill{std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f)};
    Require(tiled->FillClear(pendingFill, StorageTexture::WholeImage, refusal), refusal != nullptr ? refusal : "the 64 KiB depth plane's storage image could not be filled");
    submit(recorder);
    auto spanning = layeredDepth;
    spanning.baseArray = 0;
    spanning.depthOrLastArray = 3;
    const auto spanningView = DepthSurfaceStorage(context, {}, spanning, 0);
    Require(spanningView != nullptr && spanningView->ImageLayers() == 4, "a four-layer 2D array view over a 64 KiB depth plane was not taken as a storage image");
    Require(CountFloats(block, 0.75f) == Side * Side, "the pending depth result was not stored into guest memory before the layered view was created");
    submit(recorder);
    readBack(*spanningView);
    Require(CountFloats(block + layerPitch, overlapping) == Side * Side, "the layer that only overlaps the depth surface did not hold the guest bytes that were there");
    Require(!DepthSurfaceStoreBack(context, *StorageTexture::Create(context, *context.detiler, PlaneResource(address, Format32Float, Side / 2), 0)), "a storage image of another extent over the depth memory was stored into the depth surface");
}

void RunPairings(Context context, std::uint8_t* block) {
    const auto address = reinterpret_cast<std::uint64_t>(block);
    const auto submit = [&](Recorder& recorder) {
        recorder.Submit();
        Check(context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue), "vkQueueWaitIdle");
    };
    Recorder recorder(context);
    recorder.Activate();
    const auto readBack = [&](StorageTexture& texture) {
        texture.MarkDirty();
        texture.WriteBack();
        submit(recorder);
    };
    const DepthTarget first{address, address + BlockBytes / 2, {Side, Side}, VK_FORMAT_D32_SFLOAT_S8_UINT, 0.25f, 7};
    const DepthTarget second{address, address + BlockBytes / 2 + 8192, {Side, Side}, VK_FORMAT_D32_SFLOAT_S8_UINT, 0.5f, 3};
    Require(DepthSurfaceView(context, first) != VK_NULL_HANDLE && DepthSurfaceView(context, second) != VK_NULL_HANDLE, "the two pairings of the depth plane were not created");
    const auto depth = DepthSurfaceStorage(context, {}, PlaneResource(address, Format32Float), 0);
    Require(depth != nullptr, "the depth plane has no storage image");
    submit(recorder);
    const std::array<std::uint32_t, 4> pattern{std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f), std::bit_cast<std::uint32_t>(0.75f)};
    const char* refusal = nullptr;
    Require(depth->FillClear(pattern, StorageTexture::WholeImage, refusal), refusal != nullptr ? refusal : "the storage image could not be filled");
    Require(DepthSurfaceStoreBack(context, *depth), "the newest pairing's storage image was not stored back");
    submit(recorder);
    const auto reread = DepthSurfaceStorage(context, {}, PlaneResource(address, Format32Float), 0);
    Require(reread != nullptr, "the depth plane has no storage image");
    submit(recorder);
    readBack(*reread);
    Require(CountFloats(block, 0.75f) == Side * Side, "a storage image stored back went into an older pairing instead of the pairing that handed it out");
    auto layered = PlaneResource(address, Format32Float);
    layered.dimension = TextureDimension::k2DArray;
    Require(DepthSurfaceStorage(context, {}, layered, 0) != nullptr, "a one-layer 2D array view of the depth plane was not taken");
    ExpectFailure([&] { static_cast<void>(DepthSurfaceStoreBack(context, *depth)); }, "through a view that another view of the plane", "a store back through a replaced view was taken by an older pairing that covers the plane");
}

void DepthCopyCase(Context context, std::uint8_t* block) {
    const auto colorAddress = reinterpret_cast<std::uint64_t>(block);
    const auto depthAddress = colorAddress + BlockBytes / 4;
    Recorder recorder(context);
    recorder.Activate();
    const auto submit = [&] {
        recorder.Submit();
        Check(context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue), "vkQueueWaitIdle");
    };
    ColorTarget color{};
    color.address = colorAddress;
    color.surfaceAddress = colorAddress;
    color.extent = {Side, Side};
    color.surfaceExtent = color.extent;
    color.format = VK_FORMAT_R32_SFLOAT;
    color.elementBytes = 4;
    color.tileMode = ColorTileMode::RenderTarget;
    color.bytes = ColorTargetLayout(Side, Side, ColorTileMode::RenderTarget, 4).Bytes();
    const auto copy = [&](std::uint32_t samples, std::uint32_t sample, VkExtent2D extent) {
        RunDepthCopyPass(context, DepthCopyPass{depthAddress, extent, samples, sample, color});
        submit();
        const auto resident = CachedStorageSurface(context, SurfaceForTarget(color));
        resident->MarkDirty();
        resident->WriteBack();
        submit();
    };
    const auto countBlock = [&](float value) {
        std::size_t count = 0;
        for (std::size_t offset = 0; offset < BlockBytes; offset += sizeof(float)) {
            float texel = 0;
            std::memcpy(&texel, block + offset, sizeof(texel));
            if (texel == value) ++count;
        }
        return count;
    };
    std::fill(block, block + BlockBytes, std::uint8_t{0});
    ExpectFailure([&] { copy(1, 0, {Side, Side}); }, "no draw rendered", "a DB depth copy from memory without a depth surface was accepted");
    const auto sampledCounts = context.limits.sampledImageDepthSampleCounts & context.limits.framebufferDepthSampleCounts;
    const std::uint32_t samples = (sampledCounts & VK_SAMPLE_COUNT_2_BIT) != 0 ? 2u : (sampledCounts & VK_SAMPLE_COUNT_4_BIT) != 0 ? 4u : 0u;
    if (samples != 0) {
        Require(DepthSurfaceView(context, DepthTarget{depthAddress, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 0.375f, 0, 0, false, samples}) != VK_NULL_HANDLE, "the multisample depth surface was not created");
        for (std::uint32_t sample = 0; sample < samples; ++sample) {
            std::fill(block, block + BlockBytes, std::uint8_t{0});
            copy(samples, sample, {Side, Side});
            Require(countBlock(0.375f) == Side * Side, "the DB depth copy of sample " + std::to_string(sample) + " of " + std::to_string(samples) + " did not store the cleared depth in every texel of the R32 float target");
        }
        ExpectFailure([&] { copy(samples, samples, {Side, Side}); }, "does not have", "a DB depth copy of a sample past the last of a multisample surface was accepted");
        ExpectFailure([&] { copy(samples, 0, {Side / 2, Side / 2}); }, "of another size", "a DB depth copy of another extent than the depth surface was accepted");
        ClearDepthSurfaces(context.device);
    }
    if ((context.limits.sampledImageDepthSampleCounts & VK_SAMPLE_COUNT_2_BIT) == 0) {
        Require(DepthSurfaceView(context, DepthTarget{depthAddress, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 0.375f, 0, 0, false, 2}) != VK_NULL_HANDLE, "the unsampleable two-sample depth surface was not created");
        ExpectFailure([&] { copy(2, 0, {Side, Side}); }, "cannot sample at its sample count", "a DB depth copy from an unsampleable two-sample depth surface was accepted");
        ClearDepthSurfaces(context.device);
        std::puts("DB depth copy: unsupported two-sample source refused cleanly");
    }
    Require(DepthSurfaceView(context, DepthTarget{depthAddress, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 0.625f, 0, 0, false, 1}) != VK_NULL_HANDLE, "the one-sample depth surface was not created");
    std::fill(block, block + BlockBytes, std::uint8_t{0});
    copy(1, 0, {Side, Side});
    Require(countBlock(0.625f) == Side * Side, "the DB depth copy of a one-sample depth surface did not store the cleared depth in every texel of the R32 float target");
    ExpectFailure([&] { copy(1, 1, {Side, Side}); }, "does not have", "a DB depth copy of a second sample of a one-sample surface was accepted");
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
        auto context = device->GetContext();
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        auto* block = Storage.data() + (BlockBytes - reinterpret_cast<std::uintptr_t>(Storage.data()) % BlockBytes) % BlockBytes;
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            Run(context, block);
            auto* copyBlock = CopyStorage.data() + (BlockBytes - reinterpret_cast<std::uintptr_t>(CopyStorage.data()) % BlockBytes) % BlockBytes;
            GuestAllocations::Mutation().Add(copyBlock, BlockBytes, true, true, true);
            DepthCopyCase(context, copyBlock);
            ClearCachedTextures(context.device);
            GuestAllocations::Mutation().Remove(copyBlock);
            ClearDepthSurfaces(context.device);
            RunPairings(context, block);
            ClearDepthSurfaces(context.device);
        }
        GuestAllocations::Mutation().Remove(block);
        std::cout << "depth surface storage access tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
