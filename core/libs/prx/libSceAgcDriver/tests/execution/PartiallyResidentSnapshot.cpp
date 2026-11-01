#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "FixtureDeviceSelection.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "SpirvBackend/SpirvModule.hpp"
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
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t SurfaceBytes = std::size_t{Side} * Side * 4u;
constexpr std::uint32_t LargeSide = 512;
constexpr std::size_t LargeSurfaceBytes = std::size_t{LargeSide} * LargeSide * 4u;
constexpr int Skipped = 77;
constexpr std::uint32_t Format16x4Float = 71;
constexpr std::uint32_t FormatBc1UNorm = 169;
constexpr std::uint32_t TileLinear = 0;
constexpr std::uint32_t TileS64KBX = 0x19;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t VolumeSide = 64;
constexpr std::uint32_t VolumeDepth = 64;
constexpr std::uint32_t CommittedSlabs = 23;
constexpr std::size_t PartialSlabBytes = 16384;
constexpr std::uint32_t AddedSlab = 30;
constexpr std::uint32_t RemovedSlab = 5;
constexpr std::uint32_t LanesPerSlice = 32;
constexpr std::array<std::uint32_t, 8> SampledSlices{0, 5, 21, 22, 23, 30, 40, 63};
constexpr std::uint32_t SampleThreads = LanesPerSlice * SampledSlices.size();
constexpr std::uint32_t SampleWords = 32;
constexpr std::uint32_t SampleResult = 8;
constexpr std::uint32_t LoadResult = 12;
constexpr std::uint64_t ElementSpan = 1024;
constexpr std::size_t PageBytes = 4096;
constexpr std::array<std::uint64_t, 2> KeptSlabs{21, 22};
alignas(256) std::array<std::uint32_t, SampleThreads * SampleWords> Samples{};

alignas(256) constexpr std::array<std::uint32_t, 36> VolumeSampleCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf09c0f10, 0x00610c02,
    0xf0001f10, 0x00011005, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028,
    0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038,
    0x80001201, 0xe070103c, 0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 22> VolumeExplicitLodCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xbf8c3f70, 0xf0900f10, 0x00610c02, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024,
    0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 36> ArraySampleCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf09c0f28, 0x00610c02,
    0xf0001f28, 0x00011005, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028,
    0x80000e01, 0xe070102c, 0x80000f01, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038,
    0x80001201, 0xe070103c, 0x80001301, 0xbf810000,
};

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
            enabled.textureCompressionBC = supported.textureCompressionBC;
            address.pNext = &bytes;
            const std::array<const char*, 2> extensionsEnabled{VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME};
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
            context.textureCompressionBC = supported.textureCompressionBC == VK_TRUE;
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
        if (context.device != VK_NULL_HANDLE) ClearCachedTextures(context.device);
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

class ReservedSurface {
public:
    explicit ReservedSurface(std::size_t bytes) : size(bytes) {
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(size, BlockBytes));
        Require(block != nullptr, "partially resident snapshot: cannot reserve the guest surface");
        GuestAllocations::Mutation().Add(block, size, true, true, true);
    }

    ~ReservedSurface() {
        GuestAllocations::Mutation().Remove(block);
        Decommit(0, size);
        GuestArena::GuestArenaRelease_nid_postfix(block, size);
    }

    ReservedSurface(const ReservedSurface&) = delete;
    ReservedSurface& operator=(const ReservedSurface&) = delete;

    void Commit(std::size_t offset, std::size_t bytes) {
#ifdef _WIN32
        GuestArena::GuestArenaCommit_nid_postfix(block + offset, bytes, PAGE_READWRITE, bytes);
#else
        Require(mmap(block + offset, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED, "partially resident snapshot: cannot commit a page");
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block + offset, bytes);
#endif
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(block + offset), bytes);
        for (std::size_t page = 0; page < bytes; page += 4096) static_cast<void>(static_cast<volatile const std::uint8_t*>(block)[offset + page]);
    }

    void Decommit(std::size_t offset, std::size_t bytes) {
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block + offset, bytes);
#else
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block + offset, bytes);
        Require(mmap(block + offset, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) != MAP_FAILED, "partially resident snapshot: cannot decommit a page");
#endif
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(block + offset), bytes);
    }

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::size_t size = 0;
    std::uint8_t* block = nullptr;
};

std::atomic<std::uint64_t> surfaceBegin{0};
std::atomic<std::uint64_t> surfaceEnd{0};
std::atomic<std::uint32_t> surfaceReads{0};
std::atomic<std::uint64_t> surfaceReadBytes{0};

void CountReads(std::uint64_t address, std::size_t bytes) {
    if (address < surfaceEnd.load() && address + bytes > surfaceBegin.load()) {
        surfaceReads.fetch_add(1);
        surfaceReadBytes.fetch_add(std::min<std::uint64_t>(address + bytes, surfaceEnd.load()) - std::max<std::uint64_t>(address, surfaceBegin.load()));
    }
}

std::array<std::uint32_t, 8> SurfaceDescriptor(std::uint64_t address, std::uint32_t format, std::uint32_t tile, std::uint32_t type, std::uint32_t side, std::uint32_t depth) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        0xfacu | (tile << 20u) | (type << 28u),
        depth - 1u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 8> Descriptor(std::uint64_t address, std::uint32_t side = Side) {
    return SurfaceDescriptor(address, Format32UInt, TileR64KBX, Type2D, side, 1u);
}

void Run(const Context& context, ReservedSurface& surface) {
    const auto address = surface.Address();
    const auto words = Descriptor(address);
    Require(DescribeSurface(DecodeTextureResource(words)).guestBytes == SurfaceBytes, "the test descriptor does not describe the reserved surface");
    surface.Commit(0, BlockBytes);
    std::memset(surface.Data(), 0x5a, BlockBytes);
    surfaceBegin = address;
    surfaceEnd = address + SurfaceBytes;
    std::shared_ptr<Texture> previous;
    const auto sample = [&](const std::string& when, bool compared, bool same) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        surfaceReads = 0;
        auto texture = CachedSampledTexture(context, words);
        Require(texture != nullptr && !texture->ViewsStorageImage(), "the partially resident surface was not sampled through a snapshot " + when);
        const auto reads = surfaceReads.load();
        if (compared && reads == 0) throw std::runtime_error("the cached snapshot of the partially resident surface was not compared with guest memory " + when);
        if (!compared && reads != 0) throw std::runtime_error("the cached snapshot of the partially resident surface was compared with guest memory " + when + " although nothing changed (" + std::to_string(reads) + " reads)");
        if (previous != nullptr && same != (texture == previous)) throw std::runtime_error(std::string("sampling the partially resident surface ") + when + (same ? " replaced the snapshot" : " kept the stale snapshot"));
        previous = texture;
    };
    AgcDriver::GuestMemory::SetFlushHook(&CountReads);
    try {
        sample("at its first use", true, false);
        sample("again", false, true);
        surface.Data()[0x100] = 0x11;
        sample("after a CPU write to its committed part", true, false);
        sample("after the re-read of the written part", false, true);
        AgcDriver::GuestMemory::MarkWritten(address + 0x200, 4);
        sample("after a driver store into its committed part", true, true);
        sample("after the compare that followed the driver store", false, true);
        surface.Commit(2 * BlockBytes, BlockBytes);
        sample("after a page inside it was committed", true, true);
        sample("after the compare that followed the commit", false, true);
        std::memset(surface.Data() + 2 * BlockBytes, 0x33, BlockBytes);
        sample("after a CPU write to the newly committed page", true, false);
        sample("after the re-read of the newly committed page", false, true);
        surface.Decommit(2 * BlockBytes, BlockBytes);
        surface.Commit(2 * BlockBytes, BlockBytes);
        sample("after a page inside it was decommitted and committed again", true, false);
        sample("after the re-read of the recommitted page", false, true);
        surface.Decommit(2 * BlockBytes, BlockBytes);
        sample("after a page inside it was decommitted", true, true);
        sample("after the compare that followed the decommit", false, true);
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
}

void LargeTracked(const Context& context, ReservedSurface& surface, std::size_t committedBytes) {
    const auto address = surface.Address();
    const auto words = Descriptor(address, LargeSide);
    const auto layout = committedBytes == LargeSurfaceBytes ? std::string("committed") : std::string("partially resident");
    Require(DescribeSurface(DecodeTextureResource(words)).guestBytes == LargeSurfaceBytes, "the large test descriptor does not describe the reserved surface");
    surface.Commit(0, committedBytes);
    std::memset(surface.Data(), 0x5a, committedBytes);
    surfaceBegin = address;
    surfaceEnd = address + LargeSurfaceBytes;
    std::shared_ptr<Texture> previous;
    const auto sample = [&](const std::string& when, bool uploaded) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        surfaceReads = 0;
        auto texture = CachedSampledTexture(context, words);
        Require(texture != nullptr && !texture->ViewsStorageImage(), "the large " + layout + " surface was not sampled through a snapshot " + when);
        const auto reads = surfaceReads.load();
        if (uploaded && (reads == 0 || texture == previous)) throw std::runtime_error("sampling the large " + layout + " surface " + when + " kept the texture uploaded before it");
        if (!uploaded && (reads != 0 || texture != previous)) throw std::runtime_error("sampling the large " + layout + " surface " + when + " uploaded it again although the write tracker saw no write (" + std::to_string(reads) + " reads)");
        previous = texture;
    };
    AgcDriver::GuestMemory::SetFlushHook(&CountReads);
    try {
        sample("at its first use", true);
        sample("again", false);
        surface.Data()[0x100] = 0x11;
        sample("after a CPU write", true);
        sample("after the upload of the written surface", false);
        surface.Data()[committedBytes - 0x40] = 0x22;
        sample("after a CPU write to its last committed block", true);
        sample("after the upload of the written block", false);
        AgcDriver::GuestMemory::MarkWritten(address + 0x200, 4);
        sample("after a driver store of the bytes already there, which a snapshot it did not keep cannot tell from a change", true);
        sample("after the upload that followed the driver store", false);
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    ClearCachedTextures(context.device);
    surface.Decommit(0, committedBytes);
}

#ifdef _WIN32
void UntrackedHint(const Context& context, ReservedSurface& surface) {
    const auto address = surface.Address();
    const auto words = Descriptor(address);
    surface.Commit(0, SurfaceBytes);
    std::memset(surface.Data(), 0x5a, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    AgcDriver::GuestMemory::Unwatch(address, SurfaceBytes);
    Require(!AgcDriver::GuestMemory::Watched(address, SurfaceBytes) && AgcDriver::GuestMemory::UntrackedHint(address, SurfaceBytes) != 0, "the surface did not leave the write watch with an untracked hint");
    surfaceBegin = address;
    surfaceEnd = address + SurfaceBytes;
    std::shared_ptr<Texture> previous;
    const auto sample = [&](const std::string& when, bool compared, bool same) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        surfaceReads = 0;
        auto texture = CachedSampledTexture(context, words);
        Require(texture != nullptr && !texture->ViewsStorageImage(), "the untracked surface was not sampled through a snapshot " + when);
        const auto reads = surfaceReads.load();
        if (compared && reads == 0) throw std::runtime_error("the cached snapshot of the untracked surface was not compared with guest memory " + when);
        if (!compared && reads != 0) throw std::runtime_error("the cached snapshot of the untracked surface was compared with guest memory " + when + " although its untracked hint did not move (" + std::to_string(reads) + " reads)");
        if (previous != nullptr && same != (texture == previous)) throw std::runtime_error(std::string("sampling the untracked surface ") + when + (same ? " replaced the snapshot" : " kept the stale snapshot"));
        previous = texture;
    };
    AgcDriver::GuestMemory::SetFlushHook(&CountReads);
    try {
        sample("at its first use", true, false);
        for (int epoch = 0; epoch < 50; ++epoch) sample("in a collect epoch without a store", false, true);
        surface.Data()[0x100] = 0x11;
        sample("after a CPU write", true, false);
        sample("after the re-read of the written block", false, true);
        auto* stored = static_cast<volatile std::uint8_t*>(surface.Data()) + 2 * BlockBytes + 0x40;
        *stored = *stored;
        sample("after a CPU store of the bytes already there", true, true);
        sample("after the compare that followed that store", false, true);
        AgcDriver::GuestMemory::MarkWritten(address + 0x200, 4);
        sample("after a driver store", true, true);
        sample("after the compare that followed the driver store", false, true);
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
}
#endif

#ifdef _WIN32
std::vector<std::uint32_t> TexelFetchShader(std::uint32_t side) {
    ShaderRecompiler::SpirvModule module;
    module.EmitCapability(spv::CapabilityShader);
    module.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
    const auto u32 = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto i32 = module.Type(spv::OpTypeInt, 32u, 1u);
    const auto v4 = module.Type(spv::OpTypeVector, u32, 4u);
    const auto v3 = module.Type(spv::OpTypeVector, u32, 3u);
    const auto v2 = module.Type(spv::OpTypeVector, i32, 2u);
    const auto voidType = module.Type(spv::OpTypeVoid);
    const auto constant = [&](std::uint32_t value) { return module.Constant(spv::OpConstant, u32, value); };
    const auto array = module.Type(spv::OpTypeRuntimeArray, v4);
    module.AddAnnotation(spv::OpDecorate, array, spv::DecorationArrayStride, 16u);
    const auto block = module.Type(spv::OpTypeStruct, array);
    module.AddAnnotation(spv::OpDecorate, block, spv::DecorationBlock);
    module.AddAnnotation(spv::OpMemberDecorate, block, 0u, spv::DecorationOffset, 0u);
    const auto buffer = module.DefineGlobalVariable(module.Type(spv::OpTypePointer, spv::StorageClassStorageBuffer, block), spv::StorageClassStorageBuffer);
    module.AddAnnotation(spv::OpDecorate, buffer, spv::DecorationDescriptorSet, 0u);
    module.AddAnnotation(spv::OpDecorate, buffer, spv::DecorationBinding, 1u);
    const auto imageType = module.Type(spv::OpTypeImage, u32, spv::Dim2D, 0u, 0u, 0u, 1u, spv::ImageFormatUnknown);
    const auto image = module.DefineGlobalVariable(module.Type(spv::OpTypePointer, spv::StorageClassUniformConstant, imageType), spv::StorageClassUniformConstant);
    module.AddAnnotation(spv::OpDecorate, image, spv::DecorationDescriptorSet, 0u);
    module.AddAnnotation(spv::OpDecorate, image, spv::DecorationBinding, 0u);
    const auto group = module.DefineGlobalVariable(module.Type(spv::OpTypePointer, spv::StorageClassInput, v3), spv::StorageClassInput);
    module.AddAnnotation(spv::OpDecorate, group, spv::DecorationBuiltIn, spv::BuiltInWorkgroupId);
    const auto main = module.AllocateId();
    module.AddFunction(spv::OpFunction, voidType, main, spv::FunctionControlMaskNone, module.Type(spv::OpTypeFunction, voidType));
    module.AddFunction(spv::OpLabel, module.AllocateId());
    const auto loaded = module.AllocateId();
    module.AddFunction(spv::OpLoad, v3, loaded, group);
    const auto index = module.AllocateId();
    module.AddFunction(spv::OpCompositeExtract, u32, index, loaded, 0u);
    const auto binary = [&](spv::Op op, std::uint32_t a, std::uint32_t b) {
        const auto result = module.AllocateId();
        module.AddFunction(op, u32, result, a, b);
        return result;
    };
    const auto signedValue = [&](std::uint32_t value) {
        const auto result = module.AllocateId();
        module.AddFunction(spv::OpBitcast, i32, result, value);
        return result;
    };
    const auto coordinate = module.AllocateId();
    module.AddFunction(spv::OpCompositeConstruct, v2, coordinate, signedValue(binary(spv::OpUMod, index, constant(side))), signedValue(binary(spv::OpUDiv, index, constant(side))));
    const auto object = module.AllocateId();
    module.AddFunction(spv::OpLoad, imageType, object, image);
    const auto value = module.AllocateId();
    module.AddFunction(spv::OpImageFetch, v4, value, object, coordinate, spv::ImageOperandsLodMask, constant(0u));
    const auto pointer = module.AllocateId();
    module.AddFunction(spv::OpAccessChain, module.Type(spv::OpTypePointer, spv::StorageClassStorageBuffer, v4), pointer, buffer, constant(0u), index);
    module.AddFunction(spv::OpStore, pointer, value);
    module.AddFunction(spv::OpReturn);
    module.AddFunction(spv::OpFunctionEnd);
    module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", {group});
    module.AddExecutionMode(main, spv::ExecutionModeLocalSize, 1u, 1u, 1u);
    return module.Finalize();
}

class TexelFetch {
public:
    TexelFetch(const Context& context, const Texture& texture, Buffer& output, std::span<const std::uint32_t> code) : context(context) {
        try {
            const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}}};
            VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            info.bindingCount = 2u;
            info.pBindings = bindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &info, nullptr, &setLayout), "texel fetch descriptor layout");
            const std::array<VkDescriptorPoolSize, 2> sizes{{{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u}}};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1u;
            poolInfo.poolSizeCount = 2u;
            poolInfo.pPoolSizes = sizes.data();
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "texel fetch descriptor pool");
            const VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1u, &setLayout};
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocate, &set), "texel fetch descriptor set");
            const VkDescriptorImageInfo image{VK_NULL_HANDLE, texture.View(), texture.Layout()};
            const VkDescriptorBufferInfo buffer{output.Handle(), 0u, output.Bytes().size()};
            const std::array<VkWriteDescriptorSet, 2> writes{{{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &image, nullptr, nullptr}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buffer, nullptr}}};
            context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 2u, writes.data(), 0u, nullptr);
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1u;
            layoutInfo.pSetLayouts = &setLayout;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "texel fetch pipeline layout");
            VkShaderModuleCreateInfo shaderInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            shaderInfo.codeSize = code.size_bytes();
            shaderInfo.pCode = code.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &shaderInfo, nullptr, &module), "texel fetch shader module");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.layout = layout;
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0u, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &pipeline), "texel fetch pipeline");
        } catch (...) {
            release();
            throw;
        }
    }
    ~TexelFetch() { release(); }
    TexelFetch(const TexelFetch&) = delete;
    TexelFetch& operator=(const TexelFetch&) = delete;

    void Run(VkCommandBuffer commands, std::uint32_t groups) const {
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0u, 1u, &set, 0u, nullptr);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, groups, 1u, 1u);
    }

private:
    void release() noexcept {
        if (pipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (module != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (layout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }

    Context context;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

std::vector<std::uint32_t> FetchTexels(const Context& context, const Texture& texture, std::uint32_t side) {
    const auto count = side * side;
    Buffer output(context, std::size_t{count} * 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(output.Bytes().data(), 0xa5, output.Bytes().size());
    const TexelFetch fetch(context, texture, output, TexelFetchShader(side));
    CommandBatch batch(context);
    fetch.Run(batch.Handle(), count);
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    batch.SubmitAndWait();
    output.Invalidate();
    std::vector<std::uint32_t> texels(count);
    for (std::uint32_t index = 0; index < count; ++index) std::memcpy(&texels[index], output.Bytes().data() + std::size_t{index} * 16u, sizeof(std::uint32_t));
    return texels;
}

void ComparedShared(const Context& context) {
    constexpr std::uint32_t side = 128;
    constexpr std::size_t bytes = 2 * BlockBytes;
    constexpr std::uint32_t before = 0x5a5a5a5au;
    constexpr std::uint32_t stored = 0x13579bdfu;
    struct Direct {
        std::uint8_t* memory = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(2 * bytes, BlockBytes));
        HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, bytes, nullptr);
        bool registered = false;
        ~Direct() {
            if (registered) GuestAllocations::Mutation().Remove(memory);
            if (memory != nullptr) {
                GuestArena::GuestArenaReset_nid_postfix(memory, 2 * bytes);
                GuestArena::GuestArenaRelease_nid_postfix(memory, 2 * bytes);
            }
            if (section == nullptr) return;
            GuestArena::GuestArenaForgetSection_nid_postfix(section);
            CloseHandle(section);
        }
    } direct;
    Require(direct.memory != nullptr && direct.section != nullptr, "compared shared snapshot: cannot create the direct memory");
    GuestArena::GuestArenaForgetSection_nid_postfix(direct.section);
    GuestArena::GuestArenaMapDirect_nid_postfix(direct.memory, bytes, direct.section, 0, PAGE_READWRITE);
    GuestAllocations::Mutation().Add(direct.memory, bytes, true, true, true);
    direct.registered = true;
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(direct.memory));
    std::fill_n(reinterpret_cast<std::uint32_t*>(direct.memory), bytes / sizeof(std::uint32_t), before);
    Require(AgcDriver::GuestMemory::Watched(address, bytes), "compared shared snapshot: the direct memory is not watched");
    AgcDriver::GuestMemory::Unwatch(address, BlockBytes);
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> imported{{{address, address + BlockBytes}}};
    AgcDriver::GuestMemory::Rewatch(imported);
    Require(AgcDriver::GuestMemory::Watched(address, BlockBytes), "compared shared snapshot: the rewatched block is not watched");
    const auto words = Descriptor(address, side);
    Require(DescribeSurface(DecodeTextureResource(words)).guestBytes == BlockBytes, "compared shared snapshot: the descriptor does not describe the rewatched block");
    const auto lookup = [&] {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        return CachedSampledTexture(context, words);
    };
    const auto cached = lookup();
    Require(cached != nullptr && !cached->ViewsStorageImage(), "compared shared snapshot: the rewatched block was not sampled through a snapshot");
    Require(lookup() == cached, "compared shared snapshot: the unchanged rewatched block did not reuse its snapshot");
    const auto initial = FetchTexels(context, *cached, side);
    Require(std::all_of(initial.begin(), initial.end(), [](std::uint32_t texel) { return texel == before; }), "compared shared snapshot: the GPU did not read the cached texels");
    GuestArena::GuestArenaMapDirect_nid_postfix(direct.memory + bytes, bytes, direct.section, 0, PAGE_READWRITE);
    *reinterpret_cast<volatile std::uint32_t*>(direct.memory) = stored;
    const auto fresh = lookup();
    Require(fresh != nullptr && fresh != cached, "the first lookup after a CPU store into shared compared memory returned the snapshot cached before the sharing");
    const auto texels = FetchTexels(context, *fresh, side);
    Require(texels.front() == stored, "the GPU read the old texel on the first lookup after a CPU store into shared compared memory");
    Require(std::all_of(texels.begin() + 1, texels.end(), [](std::uint32_t texel) { return texel == before; }), "the GPU read texels the CPU store into shared compared memory did not write");
    Require(lookup() == fresh, "the snapshot of shared compared memory was not reused after its compare");
    ClearCachedTextures(context.device);
}
#endif

void PendingStores(const Context& context, ReservedSurface& surface) {
    const auto address = surface.Address();
    const auto words = Descriptor(address);
    surface.Commit(0, BlockBytes);
    std::memset(surface.Data(), 0x5a, BlockBytes);
    Recorder recorder(context);
    recorder.Activate();
    const auto lookup = [&] {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        return CachedSampledTexture(context, words);
    };
    auto previous = lookup();
    Require(previous != nullptr && !previous->ViewsStorageImage(), "the pending-store surface did not produce a snapshot");
    Require(lookup() == previous, "the unchanged pending-store surface did not reuse its snapshot");
    for (const bool submitted : {false, true}) {
        const auto landed = std::make_shared<bool>(false);
        const auto value = submitted ? std::byte{0x99} : std::byte{0x77};
        std::vector<std::byte> original(reinterpret_cast<const std::byte*>(surface.Data()), reinterpret_cast<const std::byte*>(surface.Data()) + 0x1000);
        recorder.NotePendingWrite(address, 0x1000);
        recorder.OnComplete([address, landed, value, original = std::move(original)] {
            const std::vector<std::byte> current(0x1000, value);
            AgcDriver::GuestMemory::WriteChanged(address, current, original);
            *landed = true;
        });
        if (submitted) recorder.Submit();
        const auto next = lookup();
        Require(*landed, "the partially resident lookup returned before its pending copied store completed");
        Require(next != previous, "the partially resident lookup reused the snapshot from before its pending copied store");
        Require(surface.Data()[0] == std::to_integer<std::uint8_t>(value), "the pending copied store did not update guest memory");
        Require(lookup() == next, "the partially resident lookup did not reuse the snapshot after its pending copied store");
        previous = next;
    }
    const auto unrelated = std::make_shared<bool>(false);
    recorder.NotePendingWrite(address + SurfaceBytes, BlockBytes);
    recorder.OnComplete([unrelated] { *unrelated = true; });
    Require(lookup() == previous, "an unrelated pending write replaced the partially resident snapshot");
    Require(!*unrelated, "a partially resident lookup waited for an unrelated pending write");
    recorder.Sync();
    Require(*unrelated, "the unrelated pending completion did not run when synchronized");
}

struct CommittedCase {
    std::string name;
    std::uint32_t format = Format16x4Float;
    std::uint32_t tile = TileR64KBX;
    std::uint32_t type = Type3D;
    bool whole = false;
    std::uint32_t side = VolumeSide;
    std::uint32_t committed = CommittedSlabs;
    std::uint32_t levels = 1;
};

std::optional<CommittedCase> CommittedCaseFor(std::string_view mode) {
    if (mode.ends_with("-samples")) mode.remove_suffix(std::string_view("-samples").size());
    if (mode == "--committed-volume") return CommittedCase{"the partially committed volume"};
    if (mode == "--committed-volume-whole") return CommittedCase{"the committed volume", Format16x4Float, TileR64KBX, Type3D, true};
    if (mode == "--committed-volume-thick") return CommittedCase{"the partially committed thick volume", Format16x4Float, TileS64KBX, Type3D, false, VolumeSide, 2u};
    if (mode == "--committed-volume-linear") return CommittedCase{"the partially committed linear volume", Format16x4Float, TileLinear};
    if (mode == "--committed-volume-compare") return CommittedCase{"the partially committed small volume", Format16x4Float, TileR64KBX, Type3D, false, VolumeSide, 3u};
    if (mode == "--committed-volume-mips") return CommittedCase{"the partially committed mipmapped volume", Format16x4Float, TileR64KBX, Type3D, false, 2u * VolumeSide, CommittedSlabs, 2u};
    if (mode == "--committed-volume-bc") return CommittedCase{"the partially committed BC1 volume", FormatBc1UNorm, TileR64KBX, Type3D, false, 4u * VolumeSide};
    if (mode == "--committed-array") return CommittedCase{"the partially committed 2D array", Format16x4Float, TileR64KBX, Type2DArray};
    if (mode == "--committed-volume-empty") return CommittedCase{"the uncommitted volume", Format16x4Float, TileR64KBX, Type3D, false, VolumeSide, 0u};
    return std::nullopt;
}

std::array<std::uint32_t, 8> CaseDescriptor(const CommittedCase& surface, std::uint64_t address) {
    auto words = SurfaceDescriptor(address, surface.format, surface.tile, surface.type, surface.side, VolumeDepth);
    words[3] |= (surface.levels - 1u) << 16u;
    words[5] = (surface.levels - 1u) << 4u;
    return words;
}

SurfaceGeometry DescribeCase(const CommittedCase& surface, std::uint64_t address) {
    return DescribeSurface(DecodeTextureResource(CaseDescriptor(surface, address)));
}

std::uint16_t HalfOf(std::uint32_t value) {
    const auto exponent = static_cast<std::uint32_t>(std::bit_width(value)) - 1u;
    return static_cast<std::uint16_t>(((exponent + 15u) << 10u) | ((value << (10u - exponent)) & 0x3ffu));
}

std::array<std::uint32_t, 3> Bc1Color(std::uint64_t slab) {
    return {static_cast<std::uint32_t>(slab % 31u) + 1u, 63u - static_cast<std::uint32_t>(slab % 64u), 31u};
}

std::array<std::byte, 8> PatternElement(std::uint32_t format, std::uint64_t slab, std::uint64_t element) {
    const auto color = Bc1Color(slab);
    const auto mixed = static_cast<std::uint32_t>((element * 0x9e3779b1ull) >> 21u) & 0x7ffu;
    const auto words = format == FormatBc1UNorm ? std::array<std::uint16_t, 4>{static_cast<std::uint16_t>((color[0] << 11u) | mixed), 0u, 0u, 0u}
                                                : std::array<std::uint16_t, 4>{HalfOf(static_cast<std::uint32_t>(slab) + 1u), HalfOf(static_cast<std::uint32_t>(element % ElementSpan) + 1u), HalfOf(static_cast<std::uint32_t>(element / ElementSpan) + 1u), HalfOf(1u)};
    return std::bit_cast<std::array<std::byte, 8>>(words);
}

std::uint64_t ElementOffset(const std::array<float, 4>& value) {
    return (static_cast<std::uint64_t>(value[1]) - 1u + (static_cast<std::uint64_t>(value[2]) - 1u) * ElementSpan) * 8u;
}

bool SameSlab(std::uint32_t format, const std::array<float, 4>& value, std::uint64_t slab) {
    if (format != FormatBc1UNorm) return value[0] == static_cast<float>(slab + 1u) && value[3] == 1.0f;
    return std::fabs(value[0] - static_cast<float>(Bc1Color(slab)[0]) / 31.0f) <= 1.0f / 128.0f && value[3] == 1.0f;
}

std::array<float, 4> ZeroTexel(std::uint32_t format) {
    return {0.0f, 0.0f, 0.0f, format == FormatBc1UNorm ? 1.0f : 0.0f};
}

void CommitSlab(ReservedSurface& surface, const SurfaceGeometry& geometry, std::uint32_t format, std::uint64_t slab, std::size_t bytes) {
    const auto offset = static_cast<std::size_t>(slab * geometry.layerBytes);
    surface.Commit(offset, bytes);
    for (std::size_t at = 0; at < bytes; at += 8u) {
        const auto element = PatternElement(format, slab, at / 8u);
        std::memcpy(surface.Data() + offset + at, element.data(), element.size());
    }
}

struct Commitment {
    std::uint64_t slabs = 0;
    std::set<std::uint64_t> full;
    std::optional<std::uint64_t> partial;
    std::size_t partialBytes = 0;
    std::optional<std::pair<std::uint64_t, std::uint64_t>> hole;
};

Commitment CommitCase(ReservedSurface& surface, const SurfaceGeometry& geometry, const CommittedCase& layout) {
    const auto thick = geometry.blockDepth > 1u;
    Commitment result{geometry.guestBytes / geometry.layerBytes, {}, std::nullopt, static_cast<std::size_t>(thick ? geometry.layerBytes / 2u : geometry.mips.front().tiledOffset + PartialSlabBytes)};
    const auto committed = layout.whole ? result.slabs : layout.committed;
    for (std::uint64_t slab = 0; slab < committed; ++slab) {
        const bool partial = !layout.whole && slab + 1u == committed;
        CommitSlab(surface, geometry, layout.format, slab, partial ? result.partialBytes : static_cast<std::size_t>(geometry.layerBytes));
        if (partial) result.partial = slab;
        else result.full.insert(slab);
    }
    return result;
}

std::uint64_t UploadedLayers(const SurfaceGeometry& geometry, const std::set<std::uint64_t>& slabs) {
    std::uint64_t layers = 0;
    for (std::uint32_t layer = 0; layer < geometry.layers; ++layer) layers += slabs.contains(geometry.GuestLayerOffset(layer) / geometry.layerBytes) ? 1u : 0u;
    return layers;
}

struct Lookup {
    std::shared_ptr<Texture> texture;
    std::uint64_t readBytes = 0;
};

Lookup LookUp(const Context& context, std::span<const std::uint32_t> words, const std::string& name, const std::string& when) {
    AgcDriver::GuestMemory::BumpCollectEpoch();
    surfaceReads = 0;
    surfaceReadBytes = 0;
    auto texture = CachedSampledTexture(context, words);
    Require(texture != nullptr && !texture->ViewsStorageImage(), name + " was not sampled through a snapshot " + when);
    return {std::move(texture), surfaceReadBytes.load()};
}

void RequireRead(const std::string& name, const std::string& when, const Lookup& lookup, std::uint64_t expected, std::uint64_t guestBytes) {
    if (lookup.readBytes == expected) return;
    throw std::runtime_error(name + " read " + std::to_string(lookup.readBytes) + " bytes of guest memory " + when + ", expected " + std::to_string(expected) + " of its " + std::to_string(guestBytes));
}

void CommittedSurface(const Context& context, const CommittedCase& layout, bool changes, bool compare) {
    const auto probe = DescribeCase(layout, BlockBytes);
    Require(probe.layerBytes != 0 && probe.guestBytes % probe.layerBytes == 0 && probe.GuestLayerOffset(probe.layers - 1u) + probe.layerBytes == probe.guestBytes, layout.name + ": the test descriptor does not split into whole slabs");
    ReservedSurface surface(static_cast<std::size_t>(probe.guestBytes));
    const auto words = CaseDescriptor(layout, surface.Address());
    const auto geometry = DescribeCase(layout, surface.Address());
    auto commitment = CommitCase(surface, geometry, layout);
    const bool sliced = layout.type == Type3D && !layout.whole;
    const auto uploaded = [&] {
        auto slabs = commitment.full;
        if (commitment.partial.has_value()) slabs.insert(*commitment.partial);
        return slabs;
    };
    const auto expectedRead = [&] { return sliced ? uploaded().size() * geometry.layerBytes : geometry.guestBytes; };
    surfaceBegin = surface.Address();
    surfaceEnd = surface.Address() + geometry.guestBytes;
    AgcDriver::GuestMemory::SetFlushHook(&CountReads);
    try {
        auto previous = LookUp(context, words, layout.name, "at its first use");
        RequireRead(layout.name, "at its first use", previous, expectedRead(), geometry.guestBytes);
        const auto expectKept = [&](const std::string& when) {
            const auto next = LookUp(context, words, layout.name, when);
            Require(next.texture == previous.texture && next.readBytes == 0, layout.name + " was rebuilt or compared " + when + " although its memory did not change");
        };
        const auto expectRebuilt = [&](const std::string& when) {
            const auto next = LookUp(context, words, layout.name, when);
            Require(next.texture != previous.texture, layout.name + " kept its texture " + when);
            RequireRead(layout.name, when, next, expectedRead(), geometry.guestBytes);
            previous = next;
        };
        expectKept("again");
        if (changes) {
            CommitSlab(surface, geometry, layout.format, AddedSlab, static_cast<std::size_t>(geometry.layerBytes));
            commitment.full.insert(AddedSlab);
            expectRebuilt("after an uncommitted slice was committed");
            expectKept("again after the commit");
            surface.Decommit(static_cast<std::size_t>(RemovedSlab * geometry.layerBytes), static_cast<std::size_t>(geometry.layerBytes));
            commitment.full.erase(RemovedSlab);
            expectRebuilt("after a committed slice was decommitted");
            expectKept("again after the decommit");
            const auto partial = static_cast<std::size_t>(*commitment.partial * geometry.layerBytes);
            surface.Commit(partial + commitment.partialBytes, static_cast<std::size_t>(geometry.layerBytes) - commitment.partialBytes);
            expectRebuilt("after the rest of its partially committed slice was committed");
            expectKept("again after the rest was committed");
        }
        if (compare) {
            const auto packed = uploaded().size() * geometry.layerBytes;
            const auto partial = static_cast<std::size_t>(*commitment.partial * geometry.layerBytes);
            surface.Commit(partial + commitment.partialBytes, static_cast<std::size_t>(geometry.layerBytes) - commitment.partialBytes);
            auto next = LookUp(context, words, layout.name, "after zero pages were committed in it");
            Require(next.texture == previous.texture && next.readBytes == packed, layout.name + " was not kept by a compare of its committed slices after zero pages were committed in them (" + std::to_string(next.readBytes) + " bytes read)");
            expectKept("again after the zero pages were committed");
            surface.Data()[geometry.layerBytes + 0x100] ^= 0xffu;
            next = LookUp(context, words, layout.name, "after a CPU write to a committed slice");
            Require(next.texture != previous.texture && next.readBytes == 2u * packed, layout.name + " was not compared and rebuilt after a CPU write to a committed slice (" + std::to_string(next.readBytes) + " bytes read)");
            previous = next;
            expectKept("again after the CPU write");
        }
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(nullptr);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    ClearCachedTextures(context.device);
    const auto slabs = uploaded();
    const auto layers = sliced ? UploadedLayers(geometry, slabs) : geometry.layers;
    const auto staged = sliced ? slabs.size() * geometry.layerBytes : geometry.guestBytes;
    const auto expected = 2u * staged + layers * geometry.sliceLinearBytes + (layers < geometry.layers ? geometry.sliceLinearBytes : 0u);
    Recorder recorder(context);
    recorder.Activate();
    const auto before = recorder.OpenKeptBytes();
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(CachedSampledTexture(context, words) != nullptr, layout.name + " was not sampled with a recorder");
    const auto kept = recorder.OpenKeptBytes() - before;
    if (kept != expected) throw std::runtime_error(layout.name + " kept " + std::to_string(kept) + " bytes of upload buffers, expected " + std::to_string(expected) + " (staging and tiled bytes of " + std::to_string(staged) + " guest bytes and " + std::to_string(layers) + " linear slices)");
    recorder.Sync();
    std::printf("%s read %llu of its %llu guest bytes and kept %llu bytes of upload buffers\n", layout.name.c_str(), static_cast<unsigned long long>(expectedRead()), static_cast<unsigned long long>(geometry.guestBytes), static_cast<unsigned long long>(kept));
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Sample(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, bool volume, std::uint32_t side, std::uint32_t lod) {
    Samples.fill(0xdeadbeefu);
    const auto width = side >> lod;
    for (std::uint32_t tid = 0; tid < SampleThreads; ++tid) {
        const auto slice = SampledSlices[tid / LanesPerSlice] >> lod;
        const auto lane = tid % LanesPerSlice;
        const auto x = (lane * 7u + 3u) % width;
        const auto y = (lane * 13u + 5u) % width;
        const auto r = volume ? (static_cast<float>(slice) + 0.5f) / static_cast<float>(VolumeDepth >> lod) : static_cast<float>(slice);
        const std::array<std::uint32_t, 6> words{Bits((static_cast<float>(x) + 0.5f) / static_cast<float>(width)), Bits((static_cast<float>(y) + 0.5f) / static_cast<float>(width)), Bits(r), lod != 0 ? Bits(static_cast<float>(lod)) : x, y, slice};
        std::copy(words.begin(), words.end(), &Samples[tid * SampleWords]);
    }
    const std::span<const std::uint32_t> code = lod != 0 ? std::span<const std::uint32_t>(VolumeExplicitLodCode) : volume ? std::span<const std::uint32_t>(VolumeSampleCode) : std::span<const std::uint32_t>(ArraySampleCode);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Samples.data(), static_cast<std::uint32_t>(Samples.size() * 4u));
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{SampleThreads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

bool SameTexel(std::uint32_t format, const std::array<float, 4>& value, const std::array<float, 4>& expected) {
    for (std::size_t component = 0; component < value.size(); ++component) {
        if (format == FormatBc1UNorm ? std::fabs(value[component] - expected[component]) > 1.0f / 128.0f : value[component] != expected[component]) return false;
    }
    return true;
}

std::string TexelText(const std::array<float, 4>& value) {
    return "(" + std::to_string(value[0]) + ", " + std::to_string(value[1]) + ", " + std::to_string(value[2]) + ", " + std::to_string(value[3]) + ")";
}

std::array<float, 4> SampledTexel(const std::vector<std::uint32_t>& samples, std::uint32_t tid, std::uint32_t result) {
    std::array<float, 4> value{};
    for (std::uint32_t component = 0; component < 4u; ++component) value[component] = std::bit_cast<float>(samples[tid * SampleWords + result + component]);
    return value;
}

void CheckSamples(const std::string& name, const SurfaceGeometry& geometry, std::uint32_t format, const Commitment& commitment, std::uint32_t lod, const std::vector<std::uint32_t>& reference) {
    std::map<std::uint64_t, std::array<std::uint32_t, 2>> partial;
    std::array<std::uint32_t, 2> holed{};
    std::uint64_t hash = 0xcbf29ce484222325ull;
    const auto results = lod != 0 ? std::vector<std::uint32_t>{SampleResult} : std::vector<std::uint32_t>{SampleResult, LoadResult};
    for (std::uint32_t tid = 0; tid < SampleThreads; ++tid) {
        const auto slice = SampledSlices[tid / LanesPerSlice] >> lod;
        const auto slab = geometry.GuestLayerOffset(slice) / geometry.layerBytes;
        for (const auto result : results) {
            std::array<float, 4> value{};
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto word = Samples[tid * SampleWords + result + component];
                value[component] = std::bit_cast<float>(word);
                hash = (hash ^ word) * 0x100000001b3ull;
            }
            const auto expected = SampledTexel(reference, tid, result);
            Require(SameSlab(format, expected, slab), name + ": the fully committed reference reads " + TexelText(expected) + " at level " + std::to_string(lod) + " slice " + std::to_string(slice) + " (thread " + std::to_string(tid) + "), not a texel of slab " + std::to_string(slab));
            const bool texels = SameTexel(format, value, expected);
            const bool zeros = SameTexel(format, value, ZeroTexel(format));
            const auto where = name + ": " + (result == SampleResult ? "sample" : "load") + " of level " + std::to_string(lod) + " slice " + std::to_string(slice) + " (thread " + std::to_string(tid) + ") reads " + TexelText(value);
            if (commitment.hole.has_value() && commitment.hole->first == slab) {
                const bool inHole = ElementOffset(expected) / PageBytes * PageBytes == commitment.hole->second;
                Require(inHole ? zeros : texels, where + (inHole ? ", expected the zeros of its decommitted page " + TexelText(ZeroTexel(format)) : ", expected its texel " + TexelText(expected)));
                ++holed[inHole ? 1u : 0u];
            } else if (commitment.partial == slab) {
                Require(texels || zeros, where + ", neither its texels nor zeros");
                ++partial[slab][texels ? 0u : 1u];
            } else if (commitment.full.contains(slab)) {
                Require(texels, where + ", expected its texel " + TexelText(expected));
            } else {
                Require(zeros, where + ", expected the zeros of uncommitted memory " + TexelText(ZeroTexel(format)));
            }
        }
    }
    if (commitment.hole.has_value()) Require(holed[0] != 0 && holed[1] != 0, name + ": slab " + std::to_string(commitment.hole->first) + " was not sampled both inside and outside its decommitted page (" + std::to_string(holed[1]) + " inside, " + std::to_string(holed[0]) + " outside)");
    for (const auto& [slab, counts] : partial) Require(lod != 0 || (counts[0] != 0 && counts[1] != 0), name + ": the partially committed slab " + std::to_string(slab) + " did not read both its texels and zeros (" + std::to_string(counts[0]) + " texels, " + std::to_string(counts[1]) + " zeros)");
    std::printf("%s: level %u sample results hash %016llx\n", name.c_str(), lod, static_cast<unsigned long long>(hash));
}

std::vector<std::vector<std::uint32_t>> ReferenceSamples(AgcDriver::VulkanDevice& device, const CommittedCase& layout) {
    const auto probe = DescribeCase(layout, BlockBytes);
    ReservedSurface surface(static_cast<std::size_t>(probe.guestBytes));
    const auto words = CaseDescriptor(layout, surface.Address());
    const auto geometry = DescribeCase(layout, surface.Address());
    for (std::uint64_t slab = 0; slab < geometry.guestBytes / geometry.layerBytes; ++slab) CommitSlab(surface, geometry, layout.format, slab, static_cast<std::size_t>(geometry.layerBytes));
    std::vector<std::vector<std::uint32_t>> reference;
    for (std::uint32_t lod = 0; lod < layout.levels; ++lod) {
        Sample(device, words, layout.type == Type3D, layout.side, lod);
        reference.emplace_back(Samples.begin(), Samples.end());
    }
    return reference;
}

int SampleDecommittedPage() {
    const auto device = OpenVulkanTestDevice();
    if (!device) return Skipped;
    const CommittedCase layout{"the volume with a page decommitted in a committed slab"};
    const auto reference = ReferenceSamples(*device, layout);
    const auto probe = DescribeCase(layout, BlockBytes);
    ReservedSurface surface(static_cast<std::size_t>(probe.guestBytes));
    const auto words = CaseDescriptor(layout, surface.Address());
    const auto geometry = DescribeCase(layout, surface.Address());
    Commitment commitment{geometry.guestBytes / geometry.layerBytes};
    for (const auto slab : KeptSlabs) {
        CommitSlab(surface, geometry, layout.format, slab, static_cast<std::size_t>(geometry.layerBytes));
        commitment.full.insert(slab);
    }
    Sample(*device, words, true, layout.side, 0u);
    CheckSamples(layout.name, geometry, layout.format, commitment, 0u, reference[0]);
    const auto holeSlab = KeptSlabs.back();
    std::uint32_t first = 0;
    while (first < SampleThreads && geometry.GuestLayerOffset(SampledSlices[first / LanesPerSlice]) / geometry.layerBytes != holeSlab) ++first;
    Require(first < SampleThreads, layout.name + ": no sampled slice lies in slab " + std::to_string(holeSlab));
    const auto page = ElementOffset(SampledTexel(reference[0], first, SampleResult)) / PageBytes * PageBytes;
    surface.Decommit(static_cast<std::size_t>(holeSlab * geometry.layerBytes + page), PageBytes);
    commitment.hole = std::pair{holeSlab, page};
    Sample(*device, words, true, layout.side, 0u);
    CheckSamples(layout.name + " after the page at byte " + std::to_string(page) + " of slab " + std::to_string(holeSlab) + " was decommitted", geometry, layout.format, commitment, 0u, reference[0]);
    return 0;
}

int SampleCommittedSurface(const CommittedCase& layout, bool changes) {
    const auto device = OpenVulkanTestDevice();
    if (!device) return Skipped;
    const auto reference = ReferenceSamples(*device, layout);
    const auto probe = DescribeCase(layout, BlockBytes);
    ReservedSurface surface(static_cast<std::size_t>(probe.guestBytes));
    const auto words = CaseDescriptor(layout, surface.Address());
    const auto geometry = DescribeCase(layout, surface.Address());
    auto commitment = CommitCase(surface, geometry, layout);
    const bool volume = layout.type == Type3D;
    for (std::uint32_t lod = 0; lod < layout.levels; ++lod) {
        Sample(*device, words, volume, layout.side, lod);
        CheckSamples(layout.name, geometry, layout.format, commitment, lod, reference[lod]);
    }
    if (changes) {
        CommitSlab(surface, geometry, layout.format, AddedSlab, static_cast<std::size_t>(geometry.layerBytes));
        commitment.full.insert(AddedSlab);
        Sample(*device, words, volume, layout.side, 0u);
        CheckSamples(layout.name + " after slice " + std::to_string(AddedSlab) + " was committed", geometry, layout.format, commitment, 0u, reference[0]);
        surface.Decommit(static_cast<std::size_t>(RemovedSlab * geometry.layerBytes), static_cast<std::size_t>(geometry.layerBytes));
        commitment.full.erase(RemovedSlab);
        Sample(*device, words, volume, layout.side, 0u);
        CheckSamples(layout.name + " after slice " + std::to_string(RemovedSlab) + " was decommitted", geometry, layout.format, commitment, 0u, reference[0]);
    }
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        if (!AgcDriver::GuestMemory::WriteWatched()) {
            std::puts("skipped, guest memory has no write watch");
            return Skipped;
        }
        const std::string_view mode = argc == 2 ? std::string_view(argv[1]) : std::string_view();
        if (mode == "--committed-volume-page-samples") {
            const auto status = SampleDecommittedPage();
            if (status == 0) std::puts("partially resident snapshot tests passed");
            return status;
        }
        const auto committed = CommittedCaseFor(mode);
        if (committed.has_value() && mode.ends_with("-samples")) {
            const auto status = SampleCommittedSurface(*committed, mode == "--committed-volume-samples");
            if (status == 0) std::puts("partially resident snapshot tests passed");
            return status;
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
            auto context = device->GetContext();
            TextureDetiler detiler(context);
            context.detiler = &detiler;
            const bool large = argc == 2 && std::string_view(argv[1]) == "--large-tracked";
            ReservedSurface surface(large ? LargeSurfaceBytes : SurfaceBytes);
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            if (committed.has_value() && committed->format == FormatBc1UNorm && !context.textureCompressionBC) {
                std::puts("skipped, the device has no BC texture compression");
                return Skipped;
            }
            if (committed.has_value()) CommittedSurface(context, *committed, mode == "--committed-volume", mode == "--committed-volume-compare");
            else if (large) {
                LargeTracked(context, surface, LargeSurfaceBytes);
                LargeTracked(context, surface, BlockBytes);
            } else if (argc == 2 && std::string_view(argv[1]) == "--pending-stores") PendingStores(context, surface);
            else if (argc == 2 && std::string_view(argv[1]) == "--compared-shared") {
#ifdef _WIN32
                ComparedShared(context);
#else
                std::puts("skipped, only Windows direct memory is compared once shared");
                return Skipped;
#endif
            }
            else if (argc == 2 && std::string_view(argv[1]) == "--untracked-hint") {
#ifdef _WIN32
                UntrackedHint(context, surface);
#else
                std::puts("skipped, only Windows host imports leave the write watch with a hint");
                return Skipped;
#endif
            } else Run(context, surface);
            ClearCachedTextures(context.device);
        }
        device.reset();
        std::puts("partially resident snapshot tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
