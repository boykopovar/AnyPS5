#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "FixtureDeviceSelection.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
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
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
            context.bufferDeviceAddress = true;
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

void CountReads(std::uint64_t address, std::size_t bytes) {
    if (address < surfaceEnd.load() && address + bytes > surfaceBegin.load()) surfaceReads.fetch_add(1);
}

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

}

int main(int argc, char** argv) {
    try {
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
            auto context = device->GetContext();
            TextureDetiler detiler(context);
            context.detiler = &detiler;
            const bool large = argc == 2 && std::string_view(argv[1]) == "--large-tracked";
            ReservedSurface surface(large ? LargeSurfaceBytes : SurfaceBytes);
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            if (large) {
                LargeTracked(context, surface, LargeSurfaceBytes);
                LargeTracked(context, surface, BlockBytes);
            } else if (argc == 2 && std::string_view(argv[1]) == "--pending-stores") PendingStores(context, surface);
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
