#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Check;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Side = 4096;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint64_t SurfaceBytes = std::uint64_t{Side} * Side * 4u;
constexpr std::uint64_t Stride = 0x10000;
constexpr std::uint64_t FixedBudget = 2048ull << 20u;
constexpr std::uint64_t EvictionLimit = 4ull << 30u;
constexpr std::uint32_t MaxSurfaces = 80;

alignas(256) constexpr std::array<std::uint32_t, 10> LoadStoreCode{
    0x7e040280, 0x7e060280, 0xf0001108, 0x00010402, 0xbf8c3f70, 0xf0200108, 0x00010402, 0xe0700000, 0x80000400, 0xbf810000,
};

alignas(256) std::array<std::uint32_t, 64> Output{};

std::uint32_t Marker(std::uint32_t surface) {
    return 0x5a000000u | surface;
}

template<typename TFunction>
TFunction InstanceFunction(PFN_vkGetInstanceProcAddr resolve, VkInstance instance, const char* name) {
    const auto result = reinterpret_cast<TFunction>(resolve(instance, name));
    Require(result != nullptr, name);
    return result;
}

struct SelectedDevice {
    VkPhysicalDeviceMemoryProperties memory{};
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
};

SelectedDevice SelectDevice() {
#ifdef _WIN32
    void* library = SDL_LoadObject("vulkan-1.dll");
#else
    void* library = SDL_LoadObject("libvulkan.so.1");
#endif
    Require(library != nullptr, "cannot load Vulkan");
    const auto resolve = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
    VkInstance instance = VK_NULL_HANDLE;
    SelectedDevice chosen;
    try {
        Require(resolve != nullptr, "missing Vulkan instance resolver");
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo = &application;
        Check(InstanceFunction<PFN_vkCreateInstance>(resolve, VK_NULL_HANDLE, "vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
        const auto enumerate = InstanceFunction<PFN_vkEnumeratePhysicalDevices>(resolve, instance, "vkEnumeratePhysicalDevices");
        std::uint32_t count = 0;
        Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
        std::vector<VkPhysicalDevice> devices(count);
        Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
        devices.resize(count);
        const auto properties = InstanceFunction<PFN_vkGetPhysicalDeviceProperties>(resolve, instance, "vkGetPhysicalDeviceProperties");
        const auto queues = InstanceFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(resolve, instance, "vkGetPhysicalDeviceQueueFamilyProperties");
        constexpr auto graphicsCompute = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        VkPhysicalDevice selected = VK_NULL_HANDLE;
        int selectedRank = -1;
        for (const auto physical : devices) {
            VkPhysicalDeviceProperties described{};
            properties(physical, &described);
            const int rank = described.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : described.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : described.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? 1 : 0;
            if (described.apiVersion < VK_API_VERSION_1_1 || rank <= selectedRank) continue;
            std::uint32_t familyCount = 0;
            queues(physical, &familyCount, nullptr);
            std::vector<VkQueueFamilyProperties> families(familyCount);
            queues(physical, &familyCount, families.data());
            if (std::none_of(families.begin(), families.end(), [](const VkQueueFamilyProperties& family) { return family.queueCount != 0 && (family.queueFlags & graphicsCompute) == graphicsCompute; })) continue;
            selected = physical;
            selectedRank = rank;
            chosen.type = described.deviceType;
        }
        Require(selected != VK_NULL_HANDLE, "no Vulkan 1.1 graphics and compute device");
        InstanceFunction<PFN_vkGetPhysicalDeviceMemoryProperties>(resolve, instance, "vkGetPhysicalDeviceMemoryProperties")(selected, &chosen.memory);
    } catch (...) {
        if (instance != VK_NULL_HANDLE) InstanceFunction<PFN_vkDestroyInstance>(resolve, instance, "vkDestroyInstance")(instance, nullptr);
        SDL_UnloadObject(library);
        throw;
    }
    InstanceFunction<PFN_vkDestroyInstance>(resolve, instance, "vkDestroyInstance")(instance, nullptr);
    SDL_UnloadObject(library);
    return chosen;
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint64_t address) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::shared_ptr<StorageTexture> Load(AgcDriver::VulkanDevice& device, std::uint64_t base, std::uint32_t surface) {
    const auto address = base + surface * Stride;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu};
    const auto texture = TextureDescriptor(address);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(LoadStoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    StorageTexture::FlushPending(address, static_cast<std::size_t>(SurfaceBytes), nullptr, "test");
    device.WaitIdle();
    Require(Output[0] == Marker(surface), "surface " + std::to_string(surface) + ": the shader read " + std::to_string(Output[0]) + " instead of its first texel");
    return StorageTexture::FindLive(address, SurfaceBytes);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto selected = SelectDevice();
        const auto budget = AgcDriver::Graphics::TextureCacheBudget(selected.memory);
        std::vector<std::uint32_t> storage((SurfaceBytes + MaxSurfaces * Stride + Stride) / 4u, 0u);
        auto* texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + Stride - 1u) & ~std::uintptr_t{Stride - 1u});
        for (std::uint32_t surface = 0; surface < MaxSurfaces; ++surface) texels[surface * Stride / 4u] = Marker(surface);
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
        const auto blockBytes = static_cast<std::size_t>(SurfaceBytes + (MaxSurfaces - 1u) * Stride);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(texels, blockBytes, true, true);
        }
        const auto release = [&] {
            AgcDriver::Graphics::ClearCachedTextures(device->Device());
            GuestAllocations::Mutation mutation;
            mutation.Remove(texels);
        };
        try {
            const auto first = Load(*device, base, 0);
            Require(first != nullptr, "surface 0 has no cached storage image");
            const auto held = std::max<std::uint64_t>(first->AllocationBytes(), first->GuestBytes());
            const auto reused = static_cast<std::uint32_t>(FixedBudget / held) + 1u;
            std::printf("texture cache budget %llu MiB; each 4096x4096 32_UINT surface holds %llu MiB; cyclic set of %u surfaces (%llu MiB)\n", static_cast<unsigned long long>(budget >> 20u), static_cast<unsigned long long>(held >> 20u), reused, static_cast<unsigned long long>((reused * held) >> 20u));
            if (budget < reused * held) {
                std::printf("skipped, the device-local heap gives the texture caches less than the cyclic set\n");
                release();
                return VulkanTestSkipped;
            }
            std::vector<std::weak_ptr<StorageTexture>> images(reused);
            images[0] = first;
            for (std::uint32_t surface = 1; surface < reused; ++surface) images[surface] = Load(*device, base, surface);
            std::uint32_t remade = 0;
            for (std::uint32_t surface = 0; surface < reused; ++surface) {
                const auto again = Load(*device, base, surface);
                if (again == nullptr || again != images[surface].lock()) ++remade;
            }
            Require(remade == 0, std::to_string(remade) + " of " + std::to_string(reused) + " storage images were made again on the second pass over a " + std::to_string((reused * held) >> 20u) + " MiB cyclic set under a " + std::to_string(budget >> 20u) + " MiB budget");
            const auto capacity = static_cast<std::uint32_t>(budget / held);
            if (selected.type == VK_PHYSICAL_DEVICE_TYPE_CPU || budget > EvictionLimit || capacity + 2u > MaxSurfaces) {
                std::printf("eviction past the budget not tested on a CPU device or over a 4 GiB budget\n");
            } else {
                for (std::uint32_t surface = reused; surface < capacity + 2u; ++surface) Load(*device, base, surface);
                for (std::uint32_t surface = 0; surface < capacity + 2u; ++surface) {
                    const bool cached = StorageTexture::FindLive(base + surface * Stride, SurfaceBytes) != nullptr;
                    Require(cached == (surface >= 2u), "surface " + std::to_string(surface) + (cached ? " is still cached" : " was evicted") + " after " + std::to_string(capacity + 2u) + " surfaces under a budget of " + std::to_string(capacity) + " of them");
                }
            }
        } catch (...) {
            release();
            throw;
        }
        release();
        std::puts("storage cache budget tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
