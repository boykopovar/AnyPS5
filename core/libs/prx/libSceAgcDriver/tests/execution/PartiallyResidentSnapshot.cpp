#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "FixtureDeviceSelection.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "SpirvBackend/SpirvModule.hpp"
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
#include <chrono>
#include <cstddef>
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

ShaderRecompiler::RecompileResult SampledProgram(const std::array<std::uint32_t, 8>& words) {
    ShaderRecompiler::DescriptorBinding binding{};
    binding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = ShaderRecompiler::RuntimeAbi::FirstImageBinding + PlaceholderDimensionsPerClass + 2u;
    binding.count = 1;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageWritten.assign(1, false);
    binding.imageSamplers.assign(1, 0u);
    binding.guestDescriptor.assign(words.begin(), words.end());
    ShaderRecompiler::RecompileResult program;
    program.bindings.push_back(std::move(binding));
    return program;
}

void DepthTransition(Context context, ReservedSurface& surface) {
    PlaceholderImages placeholders(context);
    context.placeholderImages = &placeholders;
    TextureCache textures(context);
    context.textureCache = &textures;
    surface.Commit(0, SurfaceBytes);
    std::memset(surface.Data(), 0, SurfaceBytes);
    const auto address = surface.Address();
    auto words = Descriptor(address);
    constexpr std::uint32_t Format32Float = 22;
    words[1] = (words[1] & ~(0x3ffu << 20u)) | (Format32Float << 20u);
    auto program = SampledProgram(words);
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto generation = AgcDriver::GuestMemory::CollectWrites(address, SurfaceBytes);
    Require(generation != 0, "the depth transition surface is not write-tracked");
    ShaderResources resources(context, shader);
    Require(resources.Reusable() && resources.Revalidate(shader), "the ordinary sampled resources did not reuse before depth registration");
    const auto before = resources.SampledTextures().front();
    try {
        const DepthTarget target{address, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
        Require(DepthSurfaceView(context, target, false) != VK_NULL_HANDLE, "the depth owner was not registered");
        Require(FlushDepthPlanes(address, SurfaceBytes).empty(), "the depth registration unexpectedly made the guest bytes stale");
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Require(AgcDriver::GuestMemory::CollectWrites(address, SurfaceBytes) != 0 && AgcDriver::GuestMemory::UnchangedSince(address, SurfaceBytes, generation), "the depth registration changed the guest bytes");
        ShaderResources::ProofReport report;
        Require(!resources.Revalidate(shader, &report), "sampled resources reused a stale texture after depth registration without a guest write");
        Require(report.fullWalks == 0, "the definitive stale depth refusal ran an unnecessary full walk");
        ShaderResources rebuilt(context, shader);
        const auto after = rebuilt.SampledTextures().front();
        Require(after != before && DepthSurfaceHolds(context, DecodeTextureResource(words), after.get()), "the new depth owner did not replace the ordinary sampled texture");
        Require(rebuilt.Revalidate(shader), "the rebuilt depth resources did not reuse");
    } catch (...) {
        ClearDepthSurfaces(context.device);
        throw;
    }
    ClearDepthSurfaces(context.device);
}

void DepthOverlap(Context context, ReservedSurface& surface) {
    PlaceholderImages placeholders(context);
    context.placeholderImages = &placeholders;
    TextureCache textures(context);
    context.textureCache = &textures;
    surface.Commit(0, SurfaceBytes);
    std::memset(surface.Data(), 0, SurfaceBytes);
    const auto address = surface.Address() + BlockBytes;
    const auto words = Descriptor(address, Side / 2u);
    const auto bytes = DescribeSurface(DecodeTextureResource(words)).guestBytes;
    auto program = SampledProgram(words);
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    try {
        const DepthTarget target{surface.Address(), 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
        Require(DepthSurfaceView(context, target, false) != VK_NULL_HANDLE, "the overlapping depth owner was not registered");
        Require(DepthSurfaceOverlaps(context.device, address, bytes) && !DepthSurfaceAt(address), "the sampled texture does not overlap a depth surface at a different base");
        AgcDriver::GuestMemory::BumpCollectEpoch();
        ShaderResources resources(context, shader);
        Require(resources.Reusable(), "the overlapping sampled resources are not reusable");
        const auto texture = resources.SampledTextures().front();
        Require(texture != nullptr && !texture->ViewsStorageImage() && !DepthSurfaceHolds(context, DecodeTextureResource(words), texture.get()), "the overlapping sampled texture did not remain an ordinary snapshot");
        Require(CachedSampledTexture(context, words) == texture, "the full lookup replaced the overlapping sampled texture");
        for (std::size_t use = 0; use < 3; ++use) {
            Require(DepthSurfaceAt(target.address) && DepthSurfaceOverlaps(context.device, address, bytes), "the overlapping depth surface did not stay live");
            AgcDriver::GuestMemory::BumpCollectEpoch();
            ShaderResources::ProofReport report;
            Require(resources.Revalidate(shader, &report), "the overlapping sampled resources failed revalidation");
            Require(report.path == ShaderResources::ProofPath::Fast, "the overlapping sampled texture left the fast proof for the full walk");
            Require(resources.SampledTextures().front() == texture, "the fast proof replaced the overlapping sampled texture");
        }
    } catch (...) {
        ClearDepthSurfaces(context.device);
        throw;
    }
    ClearDepthSurfaces(context.device);
}

void DepthNonservable(Context context, ReservedSurface& surface, bool dimensions, bool retry, bool ownSource = false) {
    PlaceholderImages placeholders(context);
    context.placeholderImages = &placeholders;
    TextureCache textures(context);
    context.textureCache = &textures;
    surface.Commit(0, SurfaceBytes);
    std::memset(surface.Data(), 0, SurfaceBytes);
    auto words = Descriptor(surface.Address(), dimensions ? Side / 2u : Side);
    if (dimensions) words[1] = (words[1] & ~(0x3ffu << 20u)) | (22u << 20u);
    auto program = SampledProgram(words);
    ReservedSurface storage(SurfaceBytes);
    storage.Commit(0, SurfaceBytes);
    std::memset(storage.Data(), 0, SurfaceBytes);
    const auto storageWords = Descriptor(storage.Address());
    if (retry) {
        auto binding = program.bindings.front();
        binding.kind = ShaderRecompiler::DescriptorKind::StorageImage;
        binding.binding = ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding + PlaceholderDimensionsPerClass + 2u;
        binding.imageWritten.assign(1, true);
        binding.guestDescriptor.assign(storageWords.begin(), storageWords.end());
        program.bindings.push_back(std::move(binding));
    }
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::shared_ptr<StorageTexture> source;
    if (ownSource) {
        source = CachedStorageSurface(context, DecodeTextureResource(words));
        source->MarkDirty();
    }
    ShaderResources resources(context, shader);
    Require(resources.Reusable(), "the non-servable depth fixture is not reusable");
    const auto texture = resources.SampledTextures().front();
    try {
        const DepthTarget target{surface.Address(), 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
        Require(DepthSurfaceView(context, target, false) != VK_NULL_HANDLE, "the non-servable depth owner was not registered");
        const auto resource = DecodeTextureResource(words);
        Require(DepthSurfaceAt(surface.Address()), "the non-servable depth owner has another base");
        Require(DepthSurfaceTexture(context, words, resource, {}) == nullptr, "the depth owner unexpectedly serves the sampled descriptor");
        if (!ownSource) Require(!PendingStorageOverlaps(surface.Address(), DescribeSurface(resource).guestBytes, nullptr), "the sampled fixture has a pending overlap");
        std::shared_ptr<StorageTexture> foreign;
        if (ownSource) {
            Require(texture->StorageSource() == source.get(), "the sampled fixture does not view its own source");
            foreign = StorageTexture::Create(context, *context.detiler, DecodeTextureResource(Descriptor(surface.Address() + BlockBytes, Side / 2u)), 0);
            foreign->MarkDirty();
            source->MarkDirty();
            Require(PendingStorageOverlaps(surface.Address(), DescribeSurface(resource).guestBytes, source.get()), "the own-source fixture has no foreign overlap");
            Require(StorageTexture::FindPending(surface.Address(), DescribeSurface(resource).guestBytes) == source, "the sampled lookup does not select its own source");
        } else if (retry) {
            foreign = StorageTexture::Create(context, *context.detiler, DecodeTextureResource(storageWords), 0);
            foreign->MarkDirty();
            Require(PendingStorageOverlaps(storage.Address(), SurfaceBytes, nullptr), "the T1 fixture has no foreign pending storage");
        }
        for (std::size_t use = 0; use < 3; ++use) {
            AgcDriver::GuestMemory::BumpCollectEpoch();
            ShaderResources::ProofReport report;
            Require(resources.Revalidate(shader, &report), "a non-servable depth owner refused sampled reuse");
            const auto expected = retry && use == 0 ? ShaderResources::ProofPath::OwnRefreshed : ShaderResources::ProofPath::Fast;
            Require(report.path == expected, "a non-servable depth owner bypassed the fast or T1 proof");
            Require(report.fullWalks == 0, "an empty sampled overlap caused an unnecessary full walk");
            Require(resources.SampledTextures().front() == texture, "a non-servable depth owner replaced the sampled texture");
        }
    } catch (...) {
        ClearDepthSurfaces(context.device);
        ClearCachedTextures(context.device);
        throw;
    }
    ClearDepthSurfaces(context.device);
    ClearCachedTextures(context.device);
}

PFN_vkGetDeviceProcAddr profileResolver = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL ProfileWait(VkDevice device, std::uint32_t count, const VkFence* fences, VkBool32 all, std::uint64_t timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    return reinterpret_cast<PFN_vkWaitForFences>(profileResolver(device, "vkWaitForFences"))(device, count, fences, all, timeout);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL ProfileResolve(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkWaitForFences") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&ProfileWait);
    return profileResolver(device, name);
}

void RevalidationProfile(Context context, ReservedSurface& surface) {
    Require(LookupOutcomes::Profiled(), "the revalidation profiling test needs APS5_PROFILE_DRAW");
    profileResolver = context.deviceProc;
    context.deviceProc = &ProfileResolve;
    PlaceholderImages placeholders(context);
    context.placeholderImages = &placeholders;
    TextureCache textures(context);
    context.textureCache = &textures;
    surface.Commit(0, SurfaceBytes);
    std::memset(surface.Data(), 0, SurfaceBytes);
    const auto words = Descriptor(surface.Address());
    auto program = SampledProgram(words);
    const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    ShaderResources resources(context, shader);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    surface.Data()[0] = 1;
    const auto uploads = ThreadLookupOutcomes().counts[LookupOutcomes::SnapshotUpload];
    const auto waits = Recorder::ThreadWaitCount();
    const auto waitMs = Recorder::ThreadWaitedMs();
    ShaderResources::ProofReport report;
    Require(!resources.Revalidate(shader, &report) && report.fullWalks == 1, "changed sampled bytes did not run one rejecting full walk");
    Require(ThreadLookupOutcomes().counts[LookupOutcomes::SnapshotUpload] == uploads + 1, "the full walk did not count its snapshot upload");
    Require(Recorder::ThreadWaitCount() > waits && Recorder::ThreadWaitedMs() - waitMs >= 20.0, "the full walk did not count its CommandBatch wait");
    ShaderResources rebuilt(context, shader);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(rebuilt.Revalidate(shader, &report) && report.fullWalks == 0, "the unchanged sampled proof ran a full walk");
    Require(rebuilt.Revalidate(shader, &report) && report.fullWalks == 0, "epoch proof reuse counted as a full walk");
    {
        Recorder recorder(context);
        recorder.Activate();
        recorder.Keep(std::make_shared<int>(0), 3 * Recorder::KeptBytesBudget);
        AgcDriver::GuestMemory::MarkWritten(surface.Address(), 4);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        const auto before = Recorder::ThreadWaitCount();
        const auto beforeMs = Recorder::ThreadWaitedMs();
        Require(rebuilt.Revalidate(shader, &report) && report.fullWalks == 1, "a stamped snapshot did not run its comparison walk");
        Require(Recorder::ThreadWaitCount() > before && Recorder::ThreadWaitedMs() - beforeMs >= 20.0, "the full walk did not count its retained-budget recorder wait");
        Require(recorder.InFlightKeptBytes() <= 2 * Recorder::KeptBytesBudget, "the full lookup did not bound retained bytes");
        recorder.Sync();
    }
}

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
            else if (argc == 2 && std::string_view(argv[1]) == "--depth-transition") DepthTransition(context, surface);
            else if (argc == 2 && std::string_view(argv[1]) == "--depth-overlap") DepthOverlap(context, surface);
            else if (argc == 2 && std::string_view(argv[1]) == "--depth-size") DepthNonservable(context, surface, true, false);
            else if (argc == 2 && std::string_view(argv[1]) == "--depth-format") DepthNonservable(context, surface, false, false);
            else if (argc == 2 && std::string_view(argv[1]) == "--depth-retry") DepthNonservable(context, surface, false, true);
            else if (argc == 2 && std::string_view(argv[1]) == "--depth-own-source") DepthNonservable(context, surface, false, false, true);
            else if (argc == 2 && std::string_view(argv[1]) == "--revalidation-profile") RevalidationProfile(context, surface);
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
