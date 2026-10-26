#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/PipelineLinkOptimization.hpp"
#include "Triangle_frag_spv.h"
#include "Triangle_vert_spv.h"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "ResidentPresent.hpp"
#include "ImageTableAbi.hpp"
#include "HostImportLifetime.hpp"
#include "ShaderCacheDirectory.hpp"
#include "SampleLod_spv.h"
#include "BufferUpdate_spv.h"
#include "SampleArray_spv.h"
#include "Cover_vert_spv.h"
#include "Cover_frag_spv.h"
#include "VertexRange_vert_spv.h"
#include "VertexRangeProducer_frag_spv.h"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "BdaAbi.hpp"
#include "ShaderCacheSourceVersion.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#include <psapi.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <functional>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::GuestMemory::GpuMutex;

void* AllocateWatched(std::size_t bytes, std::size_t alignment) {
    if (!AgcDriver::GuestMemory::WriteWatched()) return nullptr;
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, alignment);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) throw std::runtime_error("cannot map the watched block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    if (!AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uint64_t>(block), bytes)) throw std::runtime_error("the watched block is not watched");
    return block;
}

void ReleaseWatched(void* block, std::size_t bytes) {
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#else
    munmap(block, bytes);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
#endif
}

bool collectMappingHistoryTests() {
    constexpr std::size_t bytes = 131072;
    auto* memory = static_cast<std::uint8_t*>(AllocateWatched(bytes, 65536));
    if (memory == nullptr) return false;
    GuestAllocations::Mutation().Add(memory, bytes, true, true, false);
    struct Cleanup {
        std::uint8_t* memory;
        ~Cleanup() {
            GuestAllocations::Mutation().Remove(memory);
            ReleaseWatched(memory, bytes);
        }
    } cleanup{memory};
    const auto address = reinterpret_cast<std::uint64_t>(memory);
    std::memset(memory, 0x11, bytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::GuestMemory::CollectWrites(address, bytes);
    Require(before != 0, "the mapping fixture has no complete write history");
    memory[4096] = 0x22;
    GuestAllocations::Mutation().Remove(memory);
    GuestAllocations::Mutation().Add(memory, bytes, true, true, false);
    Require(AgcDriver::GuestMemory::CollectWrites(address, bytes) != 0, "the replacement mapping lost write tracking");
    Require(!AgcDriver::GuestMemory::UnchangedSince(address, bytes, before), "a cached collect hid a CPU store across allocation replacement");
    Require(memory[4096] == 0x22, "collecting the replacement mapping changed its CPU bytes");
    return true;
}

bool partialWatchRestorationTests() {
#ifdef _WIN32
    constexpr std::size_t bytes = 131072;
    constexpr std::size_t page = 4096;
    constexpr std::size_t restoredOffset = 4 * page;
    constexpr std::size_t restoredBytes = 4 * page;
    if (!AgcDriver::GuestMemory::WriteWatched()) return false;
    auto* memory = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
    if (memory == nullptr) return false;
    GuestArena::GuestArenaCommit_nid_postfix(memory, bytes, PAGE_READWRITE, restoredBytes);
    struct Cleanup {
        std::uint8_t* memory;
        ~Cleanup() { ReleaseWatched(memory, bytes); }
    } cleanup{memory};
    const auto address = reinterpret_cast<std::uint64_t>(memory);
    AgcDriver::GuestMemory::Unwatch(address, 65536);
    Require(!AgcDriver::GuestMemory::Watched(address, page), "the partial remap fixture was not excluded");
    memory[page] = 0x22;
    AgcDriver::GuestMemory::MarkWritten(address + page, 1);
    Require(AgcDriver::GuestMemory::UntrackedHint(address + page, 1) != 0, "the excluded remap fixture has no driver hint");
    GuestArena::GuestArenaReset_nid_postfix(memory + restoredOffset, restoredBytes);
    GuestArena::GuestArenaCommit_nid_postfix(memory + restoredOffset, restoredBytes, PAGE_READWRITE, restoredBytes);
    Require(AgcDriver::GuestMemory::Watched(address + restoredOffset, restoredBytes), "a partial private remap did not restore its write watch");
    Require(!AgcDriver::GuestMemory::Watched(address, restoredOffset), "a partial remap restored an excluded prefix");
    Require(!AgcDriver::GuestMemory::Watched(address + restoredOffset + restoredBytes, page), "a partial remap restored an excluded suffix");
    Require(AgcDriver::GuestMemory::UntrackedHint(address + page, 1) == 0, "a partial remap retained a stale driver hint");
    std::memset(memory + restoredOffset, 0x33, restoredBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto generation = AgcDriver::GuestMemory::CollectWrites(address + restoredOffset, restoredBytes);
    Require(generation != 0, "the private remap has no collected CPU write history");
    memory[restoredOffset + page] = 0x44;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(AgcDriver::GuestMemory::CollectWrites(address + restoredOffset, restoredBytes) != 0, "the private remap stopped collecting CPU writes");
    Require(!AgcDriver::GuestMemory::UnchangedSince(address + restoredOffset, restoredBytes, generation), "the private remap hid a genuine CPU write");
    GuestArena::GuestArenaReset_nid_postfix(memory, restoredBytes);
    GuestArena::GuestArenaCommit_nid_postfix(memory, restoredBytes, PAGE_READWRITE, restoredBytes);
    Require(AgcDriver::GuestMemory::Watched(address, restoredBytes), "the first private pages did not regain coverage");
    Require(!AgcDriver::GuestMemory::Watched(address + 12 * page, page), "restoring the first pages restored an excluded tail");
    memory[12 * page] = 0x55;
    AgcDriver::GuestMemory::MarkWritten(address + 12 * page, 1);
    AgcDriver::GuestMemory::Unwatch(address + 12 * page, page);
    Require(AgcDriver::GuestMemory::UntrackedHint(address + 12 * page, 1) == 0, "excluding a partially watched block invented a write-watch hint");
    return true;
#else
    return false;
#endif
}

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

bool hintMappingHistoryTests() {
#ifdef _WIN32
    constexpr std::size_t bytes = 131072;
    auto* memory = static_cast<std::uint8_t*>(AllocateWatched(bytes, 65536));
    if (memory == nullptr) return false;
    GuestAllocations::Mutation().Add(memory, bytes, true, true, false);
    struct Cleanup {
        std::uint8_t* memory;
        ~Cleanup() {
            GuestAllocations::Mutation().Remove(memory);
            ReleaseWatched(memory, bytes);
        }
    } cleanup{memory};
    const auto address = reinterpret_cast<std::uint64_t>(memory);
    std::memset(memory, 0x11, bytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    AgcDriver::GuestMemory::Unwatch(address, bytes);
    const auto before = AgcDriver::GuestMemory::UntrackedHint(address, bytes);
    Require(before != 0, "the hint mapping fixture has no write hint");
    Require(AgcDriver::GuestMemory::UntrackedHint(address, bytes) == before, "an unchanged hint range advanced its history");
    memory[4096] = 0x22;
    GuestAllocations::Mutation().Remove(memory);
    GuestAllocations::Mutation().Add(memory, bytes, true, true, false);
    const auto after = AgcDriver::GuestMemory::UntrackedHint(address, bytes);
    Require(after > before, "a cached hint walk hid a CPU store across allocation replacement");
    Require(memory[4096] == 0x22 && memory[bytes - 4096] == 0x11, "collecting replacement hints changed guest bytes");
    GuestAllocations::Mutation().Remove(memory);
    GuestAllocations::Mutation().Add(memory, bytes, true, true, false);
    Require(AgcDriver::GuestMemory::UntrackedHint(address, bytes) == after, "allocation replacement invented a CPU write hint");
    return true;
#else
    return false;
#endif
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
    Require(selectDevice(devices, "aMd rAdEoN") == 0u, "a requested integrated GPU lost to an unrelated discrete GPU");
    Require(selectDevice(devices, "rtx 5080") == 1u, "a partial GPU name did not match without regard to case");
    Require(selectDevice(devices, "Virtual") == 2u, "a requested virtual GPU was not selected");
    Require(selectDevice(devices, "cpu") == 3u, "a requested CPU device was not selected");
    Require(!selectDevice(devices, "missing").has_value(), "an unmatched request selected another GPU");
    Require(!selectDevice(devices, "NVIDIA GeForce RTX 5080 with a longer name").has_value(), "a request longer than the device name matched");
    devices[0].apiVersion = VK_API_VERSION_1_0;
    Require(!selectDevice(devices, "AMD").has_value(), "a requested device without Vulkan 1.1 was selected");
    Require(selectDevice(devices, {}) == 1u, "an unsupported device changed default selection");
    devices[0] = devices[1];
    Require(selectDevice(devices, {}) == 0u && selectDevice(devices, "NVIDIA") == 0u, "equally ranked matches did not retain enumeration order");
    for (auto& device : devices) device.apiVersion = VK_API_VERSION_1_0;
    Require(!selectDevice(devices, {}).has_value(), "a list without Vulkan 1.1 selected a device");
}

// A compute-capable device with host imports (VK_EXT_external_memory_host) when the host offers
// them, as the driver creates its own; the recorder's batches need a real queue and real fences.
class Device {
public:
    explicit Device(bool settleOnOtherFamily = false) {
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
            const auto& selected = deviceProperties[*selectedIndex];
            std::cout << "Vulkan device: " << selected.deviceName << '\n';
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
            memoryBudget = hasExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
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
            const float priorities[2]{1, 1};
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkDeviceQueueCreateInfo settle{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            settle.queueCount = 1;
            settle.pQueuePriorities = &priority;
            constexpr std::uint32_t noSettleFamily = UINT32_MAX;
            std::uint32_t settleFamily = noSettleFamily;
#ifdef _WIN32
            if (!settleOnOtherFamily && families[family].queueCount >= 2) {
                queue.queueCount = 2;
                queue.pQueuePriorities = priorities;
                settleFamily = family;
            } else {
                const auto copies = [&](std::uint32_t index) { return index != family && families[index].queueCount != 0 && (families[index].queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) != 0; };
                for (std::uint32_t index = 0; index < families.size() && settleFamily == noSettleFamily; ++index) {
                    if (copies(index) && (families[index].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) == 0) settleFamily = index;
                }
                for (std::uint32_t index = 0; index < families.size() && settleFamily == noSettleFamily; ++index) {
                    if (copies(index)) settleFamily = index;
                }
                settle.queueFamilyIndex = settleFamily;
            }
#endif
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            graphics = (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            VkPhysicalDeviceFeatures supported{};
            function<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(context.physical, &supported);
            enabled.occlusionQueryPrecise = supported.occlusionQueryPrecise;
            context.occlusionQueryPrecise = supported.occlusionQueryPrecise == VK_TRUE;
            enabled.shaderStorageImageReadWithoutFormat = supported.shaderStorageImageReadWithoutFormat;
            enabled.shaderStorageImageWriteWithoutFormat = supported.shaderStorageImageWriteWithoutFormat;
            enabled.shaderStorageImageMultisample = supported.shaderStorageImageMultisample;
            context.storageImageMultisample = supported.shaderStorageImageMultisample == VK_TRUE;
            context.singlePassStorage = supported.shaderStorageImageReadWithoutFormat == VK_TRUE && supported.shaderStorageImageWriteWithoutFormat == VK_TRUE;
            address.pNext = &bytes;
            std::vector<const char*> extensionsEnabled{VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME};
            VkPhysicalDeviceDriverProperties driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
            if (hasExtension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME)) {
                extensionsEnabled.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
                VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT, &driverProperties};
                VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &hostProperties};
                function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(context.physical, &properties);
                context.hostImportAlignment = hostProperties.minImportedHostPointerAlignment;
                context.discreteGpu = properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            }
            if (driverProperties.driverID != VK_DRIVER_ID_NVIDIA_PROPRIETARY && hasExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) && hasExtension(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME)) {
                extensionsEnabled.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
                extensionsEnabled.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
                context.dmaBufImport = true;
            }
            VkPhysicalDeviceImageViewMinLodFeaturesEXT minLod{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT};
            if (hasExtension(VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME)) {
                VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &minLod};
                function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(context.physical, &features);
                context.imageViewMinLod = minLod.minLod == VK_TRUE;
            }
            minLod = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT};
            minLod.minLod = VK_TRUE;
            if (context.imageViewMinLod) {
                extensionsEnabled.push_back(VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME);
                minLod.pNext = address.pNext;
                address.pNext = &minLod;
            }
            VkPhysicalDeviceDynamicRenderingFeaturesKHR rendering{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};
            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamicState{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT, &rendering};
            VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT library{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT, &dynamicState};
            const std::array<const char*, 6> libraryExtensions{VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME, VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};
            if (std::getenv("APS5_NO_GPL") == nullptr && std::all_of(libraryExtensions.begin(), libraryExtensions.end(), hasExtension)) {
                VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &library};
                function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(context.physical, &features);
                VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT libraryProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT};
                VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &libraryProperties};
                function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(context.physical, &properties);
                context.graphicsPipelineLibrary = library.graphicsPipelineLibrary == VK_TRUE && dynamicState.extendedDynamicState == VK_TRUE && rendering.dynamicRendering == VK_TRUE && libraryProperties.graphicsPipelineLibraryFastLinking == VK_TRUE && properties.properties.limits.maxPushConstantsSize >= PipelinePushConstantBytes;
                if (context.graphicsPipelineLibrary && hasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
                    VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
                    VkPhysicalDeviceProperties2 driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driver};
                    function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(context.physical, &driverProperties);
                    context.pipelineLinkTimeOptimization = AgcDriver::PipelineLinkTimeOptimizationUsable(driver.driverID);
                }
            }
            rendering = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR, address.pNext, VK_TRUE};
            library = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT, &dynamicState, VK_TRUE};
            dynamicState = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT, &rendering, VK_TRUE};
            if (context.graphicsPipelineLibrary) {
                extensionsEnabled.insert(extensionsEnabled.end(), libraryExtensions.begin(), libraryExtensions.end());
                address.pNext = &library;
            }
            VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
            if (hasExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
                VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexing};
                function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(context.physical, &features);
                context.descriptorIndexing = indexing.shaderSampledImageArrayNonUniformIndexing == VK_TRUE && indexing.shaderStorageImageArrayNonUniformIndexing == VK_TRUE;
                context.partiallyBound = indexing.descriptorBindingPartiallyBound == VK_TRUE;
                context.variableDescriptorCount = indexing.descriptorBindingVariableDescriptorCount == VK_TRUE;
                context.runtimeDescriptorArray = indexing.runtimeDescriptorArray == VK_TRUE;
            }
            indexing = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
            if (context.descriptorIndexing && context.partiallyBound && context.variableDescriptorCount && context.runtimeDescriptorArray) {
                extensionsEnabled.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
                indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
                indexing.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
                indexing.descriptorBindingPartiallyBound = VK_TRUE;
                indexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
                indexing.runtimeDescriptorArray = VK_TRUE;
                indexing.pNext = address.pNext;
                address.pNext = &indexing;
            } else {
                context.descriptorIndexing = context.partiallyBound = context.variableDescriptorCount = context.runtimeDescriptorArray = false;
            }
            VkPhysicalDeviceRobustness2FeaturesEXT robustness{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
            if (hasExtension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
                VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &robustness};
                function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(context.physical, &features);
                nullDescriptors = robustness.nullDescriptor == VK_TRUE;
            }
            robustness = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
            robustness.nullDescriptor = VK_TRUE;
            if (nullDescriptors) {
                extensionsEnabled.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
                robustness.pNext = address.pNext;
                address.pNext = &robustness;
            }
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &address};
            const std::array<VkDeviceQueueCreateInfo, 2> queueInfos{queue, settle};
            device.queueCreateInfoCount = settleFamily != noSettleFamily && settleFamily != family ? 2 : 1;
            device.pQueueCreateInfos = queueInfos.data();
            device.enabledExtensionCount = static_cast<std::uint32_t>(extensionsEnabled.size());
            device.ppEnabledExtensionNames = extensionsEnabled.data();
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            std::cout << "Vulkan device: " << properties.deviceName << '\n';
            context.limits = properties.limits;
            context.bufferDeviceAddress = true;
            context.imageTableCapacity = ImageTableCapacity(context);
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.imageFormatProperties = function<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties");
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            context.queueFamily = family;
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool.queueFamilyIndex = family;
            Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.pool), "vkCreateCommandPool");
#ifdef _WIN32
            if (settleFamily != noSettleFamily) {
                context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, settleFamily, settleFamily == family ? 1 : 0, &context.settleQueue);
                pool.queueFamilyIndex = settleFamily;
                Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.settlePool), "vkCreateCommandPool settle");
                context.settleFamily = settleFamily;
                context.settleQueueMutex = &settleMutex;
            }
#endif
        } catch (...) {
            release();
            throw;
        }
    }

    ~Device() { release(); }
    bool NullDescriptors() const { return nullDescriptors; }
    const Context& GetContext() const { return context; }
    bool Graphics() const { return graphics; }
    bool Timestamps() const {
        std::uint32_t count = 0;
        const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        queues(context.physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        queues(context.physical, &count, families.data());
        return families.at(context.queueFamily).timestampValidBits != 0;
    }
    VkPhysicalDeviceProperties Properties() const {
        VkPhysicalDeviceProperties properties{};
        function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
        return properties;
    }
    void WaitQueue() const { Check(context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue), "vkQueueWaitIdle"); }
    struct HeapUsage {
        std::uint64_t deviceLocal = 0;
        std::uint64_t host = 0;
    };
    std::optional<HeapUsage> MemoryUsage() const {
        if (!memoryBudget) return std::nullopt;
        VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
        VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &budget};
        function<PFN_vkGetPhysicalDeviceMemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2")(context.physical, &properties);
        HeapUsage usage;
        for (std::uint32_t heap = 0; heap < properties.memoryProperties.memoryHeapCount; ++heap) {
            ((properties.memoryProperties.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0 ? usage.deviceLocal : usage.host) += budget.heapUsage[heap];
        }
        return usage;
    }

private:
    template<typename TFunction>
    TFunction function(const char* name) const {
        const auto result = reinterpret_cast<TFunction>(instanceProc(instance, name));
        Require(result != nullptr, name);
        return result;
    }

    void release() noexcept {
        if (context.device != VK_NULL_HANDLE) ClearHostImports(context.device);
        if (context.settlePool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(context.device, context.settlePool, nullptr);
        if (context.pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(context.device, context.pool, nullptr);
        context.bufferPool.reset();
        if (context.device != VK_NULL_HANDLE) {
            DestroyShadows(context.device);
            function<PFN_vkDestroyDevice>("vkDestroyDevice")(context.device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) function<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
        if (library != nullptr) SDL_UnloadObject(library);
    }

    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    Context context{};
    bool graphics = false;
    std::mutex settleMutex;
    bool nullDescriptors = false;
    bool memoryBudget = false;
};

using Kind = Recorder::ReadKind;

void copiedDirectOrderingTests(const Device& device, Recorder& recorder) {
    auto context = device.GetContext();
    context.nullDescriptors = device.NullDescriptors();
    if (context.hostImportAlignment == 0 || !context.nullDescriptors || !AgcDriver::GuestMemory::WriteWatched()) {
        std::cout << "Host imports, null descriptors or write watching unavailable: copied/direct ordering test skipped\n";
        return;
    }
    recorder.Sync();
    const auto bytes = static_cast<std::size_t>(std::max<VkDeviceSize>(131072, context.hostImportAlignment));
    auto* block = static_cast<std::uint32_t*>(AllocateWatched(bytes, bytes));
    Require(block != nullptr, "allocate copied/direct ordering memory");
    const auto address = reinterpret_cast<std::uint64_t>(block);
    std::vector<std::shared_ptr<ShaderResources>> writers;
    context.copiedWriters = &writers;
    struct Release {
        const Context& context;
        Recorder& recorder;
        std::uint32_t* block;
        std::size_t bytes;
        std::vector<std::shared_ptr<ShaderResources>>& writers;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkShaderModule module = VK_NULL_HANDLE;
        std::shared_ptr<ShaderResources> layoutOwner;
        ~Release() {
            recorder.Sync();
            writers.clear();
            DrawCopiedWriters()->clear();
            if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
            if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
            if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            static_cast<void>(HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes));
            recorder.Sync();
            ReleaseWatched(block, bytes);
        }
    } release{context, recorder, block, bytes, writers};
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    std::memset(block, 0, bytes);
    Require(HostImportFor(context, address, bytes) != nullptr, "import copied/direct ordering memory");
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, 1024, 0x31000000u};
    binding.bufferWritten = {true};
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    auto direct = std::make_shared<ShaderResources>(context, compute);
    release.layoutOwner = direct;
    Require(!direct->HasCopiedWrites(), "ordering consumer did not bind directly");
    const auto setLayout = direct->Layout();
    const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(std::uint32_t)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &release.layout), "ordering pipeline layout");
    VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleInfo.codeSize = sizeof(BUFFER_UPDATE_SPV);
    moduleInfo.pCode = BUFFER_UPDATE_SPV;
    Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &release.module), "ordering shader module");
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, release.module, "main", nullptr};
    pipelineInfo.layout = release.layout;
    Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &release.pipeline), "ordering pipeline");
    const auto update = [&](const std::shared_ptr<ShaderResources>& resources, std::uint32_t delta, std::uint32_t count = 1) {
        const auto commands = recorder.Commands();
        recorder.Keep(resources);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, release.pipeline);
        resources->Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, release.layout);
        context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, release.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(delta), &delta);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, count, 1, 1);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
        resources->MarkGpuWrites(recorder);
    };
    for (int mode = 0; mode < 3; ++mode) {
        *block = 10;
        auto copiedContext = context;
        copiedContext.hostImportAlignment = 0;
        copiedContext.dmaBufImport = false;
        auto copied = std::make_shared<ShaderResources>(copiedContext, compute);
        Require(copied->HasCopiedWrites(), "ordering producer did not take the copied path");
        auto& pending = mode == 2 ? *DrawCopiedWriters() : writers;
        pending.push_back(copied);
        update(copied, 1);
        recorder.OnComplete([copied, &pending] {
            copied->WriteBackBuffers();
            std::erase(pending, copied);
        });
        Require(!recorder.Idle() && *block == 10, "copied producer was completed before the consumer test");
        if (mode == 0) direct = std::make_shared<ShaderResources>(context, compute);
        else Require(direct->Revalidate(std::span(&compute, 1)), "the imported template could not be revalidated");
        update(direct, 2);
        recorder.Sync();
        Require(*block == 13, mode == 0 ? "fresh direct consumer ran before copied write-back" : "reused direct consumer ran before copied write-back");
    }
    *block = 20;
    block[1024] = 30;
    auto copiedContext = context;
    copiedContext.hostImportAlignment = 0;
    copiedContext.dmaBufImport = false;
    auto copied = std::make_shared<ShaderResources>(copiedContext, compute);
    writers.push_back(copied);
    update(copied, 1);
    recorder.OnComplete([copied, &writers] {
        copied->WriteBackBuffers();
        std::erase(writers, copied);
    });
    auto separateProgram = program;
    separateProgram.bindings[0].guestDescriptor[0] += 4096;
    const CompiledShader separateCompute{ShaderRecompiler::ShaderStage::Compute, &separateProgram, 0};
    auto separate = std::make_shared<ShaderResources>(context, separateCompute);
    Require(!writers.empty() && *block == 20, "a disjoint consumer waited for copied writes");
    update(separate, 2);
    recorder.Sync();
    Require(*block == 21 && block[1024] == 32, "disjoint copied and direct writes changed each other");
    const std::uint32_t label = 40;
    static_cast<void>(recorder.Commands());
    recorder.AfterCompletions(address, std::as_bytes(std::span(&label, 1)), 1000, 0, false);
    Require(direct->Revalidate(std::span(&compute, 1)), "template reuse after a CPU completion label failed");
    update(direct, 2);
    recorder.Sync();
    Require(*block == 42, "direct consumer preceded an overlapping CPU completion label");
    update(direct, 1);
    const auto submissions = recorder.Submissions();
    Require(direct->Revalidate(std::span(&compute, 1)) && recorder.Submissions() == submissions, "ordinary direct work incurred a CPU write-back wait");
    update(direct, 2);
    recorder.Sync();
    Require(*block == 45, "direct consumers lost GPU-ordered updates");
    {
        std::array<std::uint32_t, 1024> unregistered{};
        struct Finish {
            Recorder& recorder;
            ~Finish() { recorder.Sync(); }
        } finish{recorder};
        const auto copiedAddress = reinterpret_cast<std::uint64_t>(unregistered.data());
        auto mixedProgram = program;
        auto& mixedBinding = mixedProgram.bindings[0];
        mixedBinding.count = 2;
        mixedBinding.guestDescriptor.insert(mixedBinding.guestDescriptor.end(), {static_cast<std::uint32_t>(copiedAddress), static_cast<std::uint32_t>(copiedAddress >> 32u) & 0xffffu, 1024, 0x31000000u});
        mixedBinding.bufferWritten = {true, true};
        const CompiledShader mixedCompute{ShaderRecompiler::ShaderStage::Compute, &mixedProgram, 0};
        for (const bool draw : {false, true}) {
            auto mixed = std::make_shared<ShaderResources>(context, mixedCompute);
            Require(mixed->HasCopiedWrites() && mixed->WritesOverlap(address, 4) && !mixed->CopiedWritesOverlap(address, 4) && mixed->CopiedWritesOverlap(copiedAddress, 4), "the mixed producer did not retain direct and CPU-copied writes");
            auto& pending = draw ? *DrawCopiedWriters() : writers;
            static_cast<void>(recorder.Commands());
            recorder.Keep(mixed);
            mixed->MarkGpuWrites(recorder);
            pending.push_back(mixed);
            auto completed = std::make_shared<unsigned>(0);
            recorder.OnComplete([mixed, &pending, completed] {
                mixed->WriteBackBuffers();
                std::erase(pending, mixed);
                ++*completed;
            });
            Require(!CompletePendingCpuWrites(context, address, 4), "a direct write in a mixed producer caused a CPU write-back wait");
            Require(*completed == 0 && !pending.empty(), "a disjoint copied write-back was completed while binding a direct buffer");
            Require(CompletePendingCpuWrites(context, copiedAddress, 4), "a CPU-copied write in a mixed producer was not completed");
            Require(*completed == 1 && pending.empty(), "the required CPU write-back did not release the mixed producer");
        }
    }
    {
        TextureDetiler detiler(context);
        auto imageContext = context;
        imageContext.detiler = &detiler;
        TextureCache textureCache(imageContext);
        imageContext.textureCache = &textureCache;
        Buffer readback(context, 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        struct Clear {
            const Context& context;
            Recorder& recorder;
            ~Clear() { recorder.Sync(); ClearCachedTextures(context.device); }
        } clear{context, recorder};
        ShaderRecompiler::RecompileResult imageProgram;
        ShaderRecompiler::DescriptorBinding imageBinding;
        imageBinding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
        imageBinding.role = ShaderRecompiler::DescriptorRole::GuestImages;
        imageBinding.descriptorSet = 0;
        imageBinding.binding = ShaderRecompiler::RuntimeAbi::FirstImageBinding;
        imageBinding.count = 1;
        imageBinding.imageSamplers = {0};
        imageBinding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
        imageBinding.guestDescriptor = {static_cast<std::uint32_t>(address >> 8), static_cast<std::uint32_t>(address >> 40) | (56u << 20) | (3u << 30), 15u | (63u << 14), 0x91b00fac, 0, 0, 0, 0};
        imageProgram.bindings.push_back(imageBinding);
        const CompiledShader imageCompute{ShaderRecompiler::ShaderStage::Compute, &imageProgram, 0};
        auto sampled = std::make_shared<ShaderResources>(imageContext, imageCompute);
        recorder.Sync();
        for (int reuse = 0; reuse < 2; ++reuse) {
            auto producer = std::make_shared<ShaderResources>(copiedContext, compute);
            writers.push_back(producer);
            update(producer, 1);
            recorder.OnComplete([producer, &writers] {
                producer->WriteBackBuffers();
                std::erase(writers, producer);
            });
            if (reuse == 0) sampled = std::make_shared<ShaderResources>(imageContext, imageCompute);
            else Require(sampled->Revalidate(std::span(&imageCompute, 1)), "sampled image template did not revalidate");
            const auto images = sampled->StorageImages();
            Require(images.size() == 1, "sampled image did not use imported texture storage");
            const auto commands = recorder.Commands();
            recorder.Keep(sampled);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {1, 1, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, images.front().first, VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Sync();
            std::uint32_t value = 0;
            std::memcpy(&value, readback.Bytes().data(), sizeof(value));
            Require(value == static_cast<std::uint32_t>(46 + reuse), "cached sampled image preceded a copied producer");
        }
        sampled.reset();
        ClearCachedTextures(context.device);
        const auto keys = address + 65536;
        const auto keyBytes = DccKeyBytes(65536);
        imageProgram.bindings[0].guestDescriptor[6] = (1u << 21) | (static_cast<std::uint32_t>((keys >> 8) & 0xffu) << 24);
        imageProgram.bindings[0].guestDescriptor[7] = static_cast<std::uint32_t>(keys >> 16);
        auto keyProgram = program;
        keyProgram.bindings[0].guestDescriptor = {static_cast<std::uint32_t>(keys), static_cast<std::uint32_t>(keys >> 32u) & 0xffffu, static_cast<std::uint32_t>(keyBytes), 0x31000000u};
        const CompiledShader keyCompute{ShaderRecompiler::ShaderStage::Compute, &keyProgram, 0};
        for (int reuse = 0; reuse < 2; ++reuse) {
            std::memset(reinterpret_cast<void*>(keys), 0, keyBytes);
            AgcDriver::GuestMemory::MarkWritten(keys, keyBytes);
            sampled = std::make_shared<ShaderResources>(imageContext, imageCompute);
            recorder.Sync();
            auto producer = std::make_shared<ShaderResources>(copiedContext, keyCompute);
            writers.push_back(producer);
            update(producer, 0x40404040, static_cast<std::uint32_t>(keyBytes / 4));
            recorder.OnComplete([producer, &writers] {
                producer->WriteBackBuffers();
                std::erase(writers, producer);
            });
            if (reuse == 0) sampled = std::make_shared<ShaderResources>(imageContext, imageCompute);
            else Require(sampled->Revalidate(std::span(&imageCompute, 1)), "metadata-only producer invalidated the sampled template");
            const auto images = sampled->StorageImages();
            Require(images.size() == 1, "metadata texture is not resident");
            const auto commands = recorder.Commands();
            recorder.Keep(sampled);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {1, 1, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, images.front().first, VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Sync();
            std::uint32_t value = 0;
            std::memcpy(&value, readback.Bytes().data(), sizeof(value));
            Require(value == 0xff000000, "cached sampled image ignored copied compression-metadata writes");
        }
    }
    std::cout << "Copied producers precede fresh and reused direct consumers\n";
}

void readTrackingTests(const Device& device, Recorder& recorder) {
    Require(Recorder::ReadTracking(), "read tracking is off (APS5_COPY_READ_TRACKING=0 set?)");
    Require(recorder.Idle() && !recorder.PendingReadOverlaps(0x10000, 16), "a fresh recorder reports a read");
    recorder.NotePendingRead(0x10000, 0x100, Kind::DispatchElement);
    Require(recorder.Recording(), "a read note did not open a batch");
    Require(recorder.PendingReadOverlaps(0x10080, 4) && recorder.PendingReadOverlaps(0xff00, 0x101) && recorder.PendingReadOverlaps(0x100ff, 1), "an open batch's read is not seen");
    Require(!recorder.PendingReadOverlaps(0x10100, 4) && !recorder.PendingReadOverlaps(0xff00, 0x100) && !recorder.PendingReadOverlaps(0x10000, 0), "a disjoint range is reported as read");
    const auto open = recorder.DescribePendingRead(0x10000, 4);
    Require(open.has_value() && open->open && !open->signaled && open->kind == Kind::DispatchElement && open->serial == recorder.Submissions() + 1, "the open batch's read is described wrongly");
    recorder.Submit();
    Require(!recorder.Recording() && !recorder.Idle(), "the batch is not in flight after Submit");
    Require(recorder.PendingReadOverlaps(0x10000, 4, false), "the raw scan does not see the in-flight batch's read");
    // An empty batch completes at once: once its fence signaled, its read is no reader any more,
    // although the batch stays in flight until it is reaped.
    device.WaitQueue();
    Require(!recorder.PendingReadOverlaps(0x10000, 4), "a signaled batch's read still refuses");
    Require(!recorder.Idle(), "an overlap query reaped the batch");
    const auto signaled = recorder.DescribePendingRead(0x10000, 4);
    Require(signaled.has_value() && !signaled->open && signaled->signaled && signaled->serial == recorder.Submissions(), "the signaled batch's read is described wrongly");
    const auto counts = Recorder::ReadCounts();
    Require(counts.noted == 1 && counts.staleIgnored >= 1 && counts.hits[static_cast<std::size_t>(Kind::DispatchElement)] >= 3 && counts.queries >= 6, "read counters are off");
    recorder.Sync();
    Require(recorder.Idle() && !recorder.PendingReadOverlaps(0x10000, 4, false) && !recorder.DescribePendingRead(0x10000, 4).has_value(), "a read outlived its batch");
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges{{0x20000, 0x21000}, {0x30000, 0x30010}, {0x40000, 0x40000}};
    recorder.NotePendingReads(ranges, Kind::AddressBased);
    Require(recorder.PendingReadOverlaps(0x20fff, 1) && recorder.PendingReadOverlaps(0x30000, 16) && !recorder.PendingReadOverlaps(0x21000, 16) && !recorder.PendingReadOverlaps(0x40000, 16), "noted ranges are not seen as reads");
    const auto batch = recorder.DescribePendingRead(0x30008, 4);
    Require(batch.has_value() && batch->kind == Kind::AddressBased && batch->open, "a noted range has the wrong reader kind");
    Require(Recorder::ReadCounts().hits[static_cast<std::size_t>(Kind::AddressBased)] >= 2, "address-based hits are not counted");
    recorder.Sync();
    Require(recorder.Idle() && !recorder.PendingReadOverlaps(0x20000, 0x1000, false), "noted ranges outlived their batch");
}

Recorder::WriteRanges expectedWriteUnion(Recorder::WriteRanges ranges) {
    std::sort(ranges.begin(), ranges.end());
    Recorder::WriteRanges result;
    for (const auto& [begin, end] : ranges) {
        if (!result.empty() && begin <= result.back().second) result.back().second = std::max(result.back().second, end);
        else result.emplace_back(begin, end);
    }
    return result;
}

void pendingWriteSnapshotTests(const Device& device, Recorder& recorder) {
    recorder.Sync();
    Recorder::WriteRanges expected;
    const auto check = [&] {
        const auto snapshot = Recorder::PendingWriteSnapshot();
        Require(snapshot != nullptr && *snapshot == expectedWriteUnion(expected), "pending write snapshot differs from the full union");
    };
    const auto note = [&](std::uint64_t address, std::size_t bytes) {
        const auto previous = Recorder::PendingWriteSnapshot();
        const auto saved = previous == nullptr ? Recorder::WriteRanges{} : *previous;
        recorder.NotePendingWrite(address, bytes);
        if (bytes != 0) expected.emplace_back(address, address + bytes);
        check();
        Require(previous == nullptr || *previous == saved, "publishing a new write modified an older reader's snapshot");
    };
    note(0x10000, 16);
    note(0x10020, 16);
    note(0x10010, 16);
    note(0x10004, 4);
    note(0x10000, 0);
    note(0, 1);
    note(UINT64_MAX - 16, 16);
    const Recorder::WriteRanges additions{{0x20020, 0x20040}, {0x20000, 0x20020}, {0x20010, 0x20030}, {0x20040, 0x20040}, {0x20050, 0x20040}};
    recorder.NotePendingWrites(additions);
    for (const auto& range : additions) if (range.second > range.first) expected.push_back(range);
    check();
    recorder.NotePendingFill(0x30000, 16, 0xA5);
    expected.emplace_back(0x30000, 0x30010);
    check();
    alignas(64) static std::array<std::byte, 64> labels{};
    const auto labelAddress = reinterpret_cast<std::uint64_t>(labels.data());
    const std::array value{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    recorder.NoteLabel(labelAddress, value, 1, 0);
    expected.emplace_back(labelAddress, labelAddress + value.size());
    check();
    recorder.Submit();
    device.WaitQueue();
    recorder.AfterCompletions(labelAddress + 8, value, 2, 0, true);
    expected.emplace_back(labelAddress + 8, labelAddress + 8 + value.size());
    check();
    note(0x40000, 16);
    bool completing = false;
    recorder.OnComplete([&] {
        const auto snapshot = Recorder::PendingWriteSnapshot();
        Require(Recorder::SnapshotOverlaps(snapshot.get(), 0x40000, 16), "a finishing batch left the snapshot before its completion");
        completing = true;
    });
    recorder.Sync();
    Require(completing && Recorder::PendingWriteSnapshot()->empty(), "completed writes remained published");
    std::mt19937_64 random(0x41505335u);
    for (int trial = 0; trial < 16; ++trial) {
        expected.clear();
        for (int step = 0; step < 64; ++step) note(0x80000 + random() % 4096, 1 + random() % 128);
        recorder.Sync();
        Require(Recorder::PendingWriteSnapshot()->empty(), "random writes survived their GPU batch");
    }
    expected.clear();
    note(0x90000, 16);
    {
        std::atomic<bool> invalid{false};
        std::atomic<unsigned> ready{0};
        std::atomic<std::uint64_t> reads{0};
        std::array<std::jthread, 2> readers;
        for (auto& reader : readers) reader = std::jthread([&](std::stop_token stop) {
            ready.fetch_add(1, std::memory_order_release);
            while (!stop.stop_requested()) {
                const auto snapshot = Recorder::PendingWriteSnapshot();
                if (snapshot == nullptr || !Recorder::SnapshotOverlaps(snapshot.get(), 0x90000, 16)) invalid.store(true, std::memory_order_relaxed);
                if (snapshot != nullptr) {
                    for (std::size_t i = 0; i < snapshot->size(); ++i) {
                        const auto [begin, end] = (*snapshot)[i];
                        if (end <= begin || (i != 0 && (*snapshot)[i - 1].second >= begin)) invalid.store(true, std::memory_order_relaxed);
                    }
                }
                reads.fetch_add(1, std::memory_order_relaxed);
            }
        });
        while (ready.load(std::memory_order_acquire) != readers.size()) std::this_thread::yield();
        for (std::uint64_t i = 0; i < 256; ++i) note(0x91000 + i * 128, 32);
        for (auto& reader : readers) reader.request_stop();
        for (auto& reader : readers) reader.join();
        Require(reads.load() != 0 && !invalid.load(), "concurrent snapshot readers observed an invalid or incomplete union");
    }
    recorder.Sync();
    expected.clear();
    note(UINT64_MAX - 3, 8);
    note(0x50000, 16);
    recorder.Sync();
    recorder.NotePendingWrite(0x58000, 16);
    {
        Recorder other(device.GetContext());
        other.Activate();
        recorder.NotePendingWrite(0x59000, 16);
        recorder.Activate();
        recorder.NotePendingWrite(0x5A000, 16);
        Require(*Recorder::PendingWriteSnapshot() == Recorder::WriteRanges{{0x58000, 0x58010}, {0x59000, 0x59010}, {0x5A000, 0x5A010}}, "reactivation missed writes recorded while inactive");
        other.Activate();
        recorder.Sync();
        recorder.Activate();
        recorder.NotePendingWrite(0x5B000, 16);
        Require(*Recorder::PendingWriteSnapshot() == Recorder::WriteRanges{{0x5B000, 0x5B010}}, "reactivation retained writes completed while inactive");
        recorder.Sync();
        other.Activate();
        other.NotePendingWrite(0x60000, 16);
        const auto otherSnapshot = Recorder::PendingWriteSnapshot();
        recorder.Activate();
        recorder.NotePendingWrite(0x70000, 16);
        const auto ownSnapshot = Recorder::PendingWriteSnapshot();
        Require(ownSnapshot != nullptr && *ownSnapshot == Recorder::WriteRanges{{0x70000, 0x70010}}, "a recorder extended another recorder's snapshot");
        Require(*otherSnapshot == Recorder::WriteRanges{{0x60000, 0x60010}}, "a replaced owner's retained snapshot changed");
        other.Sync();
        Require(Recorder::PendingWriteSnapshot() == ownSnapshot, "an inactive recorder replaced the active snapshot");
        recorder.Sync();
    }
    Require(Recorder::PendingWriteSnapshot()->empty(), "snapshot ownership tests left pending writes");
    std::cout << "Pending write snapshot differential, lifetime, completion and ownership tests passed\n";
}

void pendingWriteSnapshotBenchmark(Recorder& recorder) {
    for (std::uint64_t distinct : {8u, 32u, 512u}) {
        for (int duplicates : {0, 8, 64}) {
            std::array<double, 5> samples{};
            for (auto& sample : samples) {
                recorder.Sync();
                static_cast<void>(recorder.Commands());
                const auto start = std::chrono::steady_clock::now();
                for (std::uint64_t i = 0; i < distinct; ++i) {
                    const auto address = 0x100000 + i * 4096;
                    recorder.NotePendingWrite(address, 256);
                    for (int repeat = 0; repeat < duplicates; ++repeat) recorder.NotePendingWrite(address, 256);
                }
                sample = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                const auto snapshot = Recorder::PendingWriteSnapshot();
                Require(snapshot != nullptr && snapshot->size() == distinct, "snapshot benchmark produced another union");
            }
            std::sort(samples.begin(), samples.end());
            std::cout << "snapshot-note benchmark: " << distinct << " distinct, " << duplicates << " repeats, median " << samples[2] << " ms\n";
        }
    }
    recorder.Sync();
}

void readListTests(const Device& device, Recorder& recorder) {
    using Ranges = Recorder::ReadList;
    Require(Recorder::ValidReadList({{0x1000, 0x2000}, {0x2000, 0x2100}}) && Recorder::ValidReadList({}), "a sorted disjoint list is refused");
    Require(!Recorder::ValidReadList({{0x2000, 0x2100}, {0x1000, 0x2000}}) && !Recorder::ValidReadList({{0x1000, 0x2001}, {0x2000, 0x2100}}) && !Recorder::ValidReadList({{0x1000, 0x1000}}), "an unsorted, overlapping or empty list is accepted");
    std::uint64_t seed = 0x9e3779b97f4a7c15ull;
    const auto next = [&](std::uint64_t bound) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        return (seed >> 33u) % bound;
    };
    const auto makeList = [&](std::uint64_t base, std::size_t count) {
        auto list = std::make_shared<Ranges>();
        auto at = base;
        for (std::size_t index = 0; index < count; ++index) {
            at += next(3) * 0x100;
            const auto bytes = (next(16) + 1) * 0x10;
            list->emplace_back(at, at + bytes);
            at += bytes;
        }
        return list;
    };
    const auto first = makeList(0x100000, 300);
    const auto second = makeList(0x100000, 120);
    const Ranges before{{0x100400, 0x100410}, {0x200000, 0x200100}};
    const Ranges between{{0x100800, 0x100900}, {0x300000, 0x300040}};
    const Ranges after{{0x100000, 0x180000}};
    const auto note = [&](bool listed) {
        recorder.NotePendingReads(before, Kind::DispatchElement);
        if (listed) recorder.NotePendingReadList(first, Kind::AddressBased);
        else recorder.NotePendingReads(*first, Kind::AddressBased);
        recorder.NotePendingReads(between, Kind::GpuCopy);
        if (listed) {
            recorder.NotePendingReadList(second, Kind::Indirect);
            recorder.NotePendingReadList(first, Kind::AddressBased);
        } else {
            recorder.NotePendingReads(*second, Kind::Indirect);
        }
        recorder.NotePendingReads(after, Kind::CopySource);
    };
    std::vector<std::pair<std::uint64_t, std::uint64_t>> queries;
    for (const auto* list : {first.get(), second.get()}) {
        for (const auto& [begin, end] : *list) {
            for (const auto at : {begin - 1, begin, end - 1, end}) queries.emplace_back(at, 1 + next(0x80));
        }
    }
    for (int query = 0; query < 2000; ++query) queries.emplace_back(0xff000 + next(0x300000), 1 + next(0x2000));
    const auto answers = [&] {
        std::vector<int> result;
        for (const auto& [address, bytes] : queries) {
            const auto described = recorder.DescribePendingRead(address, bytes);
            result.push_back(recorder.PendingReadOverlaps(address, bytes) ? 1 : 0);
            result.push_back(described.has_value() ? 10 + static_cast<int>(described->kind) : -1);
        }
        recorder.Submit();
        for (const auto& [address, bytes] : queries) result.push_back(recorder.PendingReadOverlaps(address, bytes, false) ? 1 : 0);
        recorder.Sync();
        Require(recorder.Idle() && !recorder.PendingReadOverlaps(0x100000, 0x200000, false), "listed reads outlived their batch");
        return result;
    };
    const auto notedBefore = Recorder::ReadCounts().noted;
    note(false);
    const auto plain = answers();
    const auto notedPlain = Recorder::ReadCounts().noted - notedBefore;
    note(true);
    const auto listed = answers();
    const auto notedListed = Recorder::ReadCounts().noted - notedBefore - notedPlain;
    Require(plain == listed, "reads noted by reference answer a query differently from the same ranges noted one by one");
    Require(notedListed == notedPlain + first->size(), "a list's ranges are not counted as noted");
    for (const auto kind : {Kind::DispatchElement, Kind::AddressBased, Kind::CopySource}) Require(std::count(plain.begin(), plain.end(), 10 + static_cast<int>(kind)) != 0, "the queries miss a reader kind: the comparison proves too little");
    recorder.NotePendingReadList(std::make_shared<Ranges>(), Kind::AddressBased);
    recorder.NotePendingReadList(nullptr, Kind::AddressBased);
    Require(!recorder.Recording(), "an empty list opened a batch");
    static_cast<void>(device);
}

struct TestRange {
    std::uint64_t begin;
    std::uint64_t end;
};

void overlappedRangeTests(Recorder& recorder) {
    Require(recorder.Idle(), "the recorder is busy before the overlap tests");
    recorder.NotePendingWrite(0x210000, 0x100);
    recorder.NotePendingWrite(0x210400, 0x10);
    recorder.Submit();
    recorder.NotePendingWrite(0x210800, 0x200);
    recorder.NotePendingWrite(0x210900, 0x200);
    const std::array<TestRange, 10> ranges{{{0x20f000, 0x210000}, {0x210000, 0x210080}, {0x210080, 0x210400}, {0x210400, 0x210410}, {0x210410, 0x210800}, {0x210800, 0x210801}, {0x210801, 0x210a00}, {0x210a00, 0x210b00}, {0x210b00, 0x211000}, {0x300000, 0x300010}}};
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    recorder.PendingWriteRanges(writes);
    Require(writes.size() == 4, "the pending write ranges miss a batch's writes");
    std::vector<std::size_t> indices;
    std::size_t matched = 0;
    for (const std::size_t first : {0u, 3u, 6u, 10u}) {
        OverlappedRanges(std::span<const TestRange>(ranges), first, writes, indices);
        Require(std::is_sorted(indices.begin(), indices.end()) && std::adjacent_find(indices.begin(), indices.end()) == indices.end(), "the overlapped ranges are not sorted and unique");
        for (std::size_t index = 0; index < ranges.size(); ++index) {
            const bool listed = std::find(indices.begin(), indices.end(), index) != indices.end();
            const bool pending = index >= first && recorder.PendingWriteOverlaps(ranges[index].begin, static_cast<std::size_t>(ranges[index].end - ranges[index].begin));
            Require(listed == pending, "a range is listed as overlapped differently from PendingWriteOverlaps");
            matched += listed ? 1 : 0;
        }
    }
    Require(matched == 12, "the overlap comparison covered too few overlapped ranges");
    recorder.Sync();
    recorder.PendingWriteRanges(writes);
    Require(writes.empty(), "pending write ranges outlived their batches");
}

void spaceWriteSyncTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: address space pending writes not tested\n";
        return;
    }
    constexpr std::size_t bytes = 0x10000;
    constexpr std::size_t count = 4;
    void* block = ::operator new(2 * count * bytes, std::align_val_t{65536});
    std::memset(block, 0, 2 * count * bytes);
    auto* memory = static_cast<std::byte*>(block);
    const auto registry = [&](bool add) {
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < count; ++index) {
            if (add) mutation.Add(memory + 2 * index * bytes, bytes, true, false, true);
            else mutation.Remove(memory + 2 * index * bytes);
        }
    };
    const auto rangeAt = [&](std::size_t index) { return reinterpret_cast<std::uint64_t>(memory + 2 * index * bytes); };
    const auto build = [&] {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.AddReadable(rangeAt(0) + 64, 16);
        leased.Upload(true);
        const auto reads = leased.DeviceReads();
        leased.WriteBack();
        return reads.size();
    };
    registry(true);
    Require(recorder.Idle(), "the recorder is busy before the address space tests");
    const auto regions = build();
    if (regions < count) {
        registry(false);
        ::operator delete(block, std::align_val_t{65536});
        std::cout << "the registered ranges are not served in place: address space pending writes not tested\n";
        return;
    }
    const std::array<std::byte, 4> value{std::byte{0x5a}, std::byte{0xa5}, std::byte{0x5a}, std::byte{0xa5}};
    const auto landed = [&](std::uint64_t address) { return std::memcmp(reinterpret_cast<const void*>(address), value.data(), value.size()) == 0; };
    const auto inside = rangeAt(2) + 0x100;
    static_cast<void>(recorder.Commands());
    recorder.AfterCompletions(inside, value, 7000, 0, false);
    Require(Recorder::PendingCompletionLabels() == 1 && !landed(inside), "the label inside a registered range is not pending");
    build();
    Require(landed(inside) && Recorder::PendingCompletionLabels() == 0, "a build over the address space did not complete the label pending in one of its ranges");
    recorder.Sync();
    const auto gap = rangeAt(2) + bytes + 0x100;
    static_cast<void>(recorder.Commands());
    recorder.AfterCompletions(gap, value, 7001, 0, false);
    build();
    Require(!landed(gap) && Recorder::PendingCompletionLabels() == 1, "a build over the address space completed a label outside its ranges");
    recorder.Sync();
    Require(landed(gap) && Recorder::PendingCompletionLabels() == 0, "the label outside the ranges did not land");
    static_cast<void>(recorder.Commands());
    recorder.AfterCompletions(rangeAt(1) + 0x40, value, 7002, 0, false);
    recorder.Submit();
    static_cast<void>(recorder.Commands());
    recorder.AfterCompletions(rangeAt(3) + 0x40, value, 7003, 0, false);
    build();
    Require(landed(rangeAt(1) + 0x40) && landed(rangeAt(3) + 0x40) && Recorder::PendingCompletionLabels() == 0, "a build over the address space missed a label behind an earlier sync");
    recorder.Sync();
    static_cast<void>(recorder.Commands());
    recorder.AfterCompletions(rangeAt(1) + 0x80, value, 7004, 0, false);
    bool noted = false;
    recorder.OnComplete([&] {
        static_cast<void>(recorder.Commands());
        recorder.AfterCompletions(rangeAt(3) + 0x80, value, 7005, 0, false);
        noted = true;
    });
    build();
    Require(noted && landed(rangeAt(1) + 0x80) && landed(rangeAt(3) + 0x80) && Recorder::PendingCompletionLabels() == 0, "a build over the address space missed a label a completion noted during its sync");
    recorder.Sync();
    registry(false);
    ::operator delete(block, std::align_val_t{65536});
}

void writeSettledTests(const Device& device, Recorder& recorder) {
    Require(recorder.PendingWriteSettled(0x50000, 0x100), "an unwritten range is not settled");
    recorder.NotePendingWrite(0x50000, 0x100);
    Require(recorder.PendingWriteOverlaps(0x50000, 0x100) && !recorder.PendingWriteSettled(0x50000, 0x100) && recorder.PendingWriteSettled(0x50100, 0x100), "an open batch's write counts as settled");
    recorder.Submit();
    device.WaitQueue();
    Require(recorder.PendingWriteOverlaps(0x50000, 0x100) && recorder.PendingWriteSettled(0x50000, 0x100), "a signaled batch's write is not settled");
    recorder.NotePendingWrite(0x50080, 0x10);
    Require(!recorder.PendingWriteSettled(0x50000, 0x100) && recorder.PendingWriteSettled(0x50000, 0x80), "a later open write over the range still counts as settled");
    recorder.Sync();
    Require(recorder.Idle() && !recorder.PendingWriteOverlaps(0x50000, 0x100), "writes outlived their batches");
}

// Completion counting: a write-back completion (OnComplete) is pending until its batch finished;
// a completion label the GPU also stored (AfterCompletions storedOnGpu) is pending only once a CPU
// write-back overlapped it (NoteWrittenBack, once per label), one the GPU has no view of from its
// registration; both counts return to 0 when the batch finishes.
void completionCountTests(const Device& device, Recorder& recorder) {
    alignas(64) static std::uint32_t memory[64];
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const std::array<std::byte, 4> value{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    Require(recorder.Idle() && Recorder::PendingCompletionLabels() == 0 && Recorder::PendingWriteBackCompletions() == 0, "a fresh recorder reports pending completions");
    int ran = 0;
    recorder.OnComplete([&] { ++ran; });
    Require(Recorder::PendingWriteBackCompletions() == 1 && recorder.HasCompletions(), "a write-back completion is not pending");
    // APS5_COUNT_ALL_COMPLETION_LABELS=1 (or APS5_LABEL_STORE_ALWAYS=1) counts the GPU-stored
    // labels at registration, as before; the write-back then changes nothing.
    const bool countAll = std::getenv("APS5_COUNT_ALL_COMPLETION_LABELS") != nullptr || std::getenv("APS5_LABEL_STORE_ALWAYS") != nullptr;
    const std::uint64_t registered = countAll ? 2 : 0;
    recorder.AfterCompletions(base, value, 3, 0, true);
    recorder.AfterCompletions(base + 8, value, 4, 0, true);
    Require(Recorder::PendingCompletionLabels() == registered, "a GPU-stored completion label counts before any write-back");
    Require(Recorder::PendingCompletionStores() == registered, "a GPU-stored completion label counts as a completion store before any write-back");
    recorder.AfterCompletions(base + 0x40, value, 5, 0, false);
    Require(Recorder::PendingCompletionLabels() == registered + 1, "a label the GPU has no view of is not pending");
    Require(Recorder::PendingCompletionStores() == registered + 1, "a label the GPU has no view of is not a pending completion store");
    // A late lookup (stamp <= afterStamp) of a label whose value reaches memory only through the
    // batch's completion action is refused as BehindCompletion (a poller reaps for it); a GPU-stored
    // one keeps the ordinary reason (unclosed group, or trust off) until a write-back overlaps it.
    using Refusal = Recorder::LabelRefusal;
    const Refusal ordinary = Recorder::LateTrust() ? Refusal::Unclosed : Refusal::TrustOff;
    Refusal refusal{};
    Require(!Recorder::LookupLabel(base + 0x40, 4, 5, &refusal).has_value() && refusal == Refusal::BehindCompletion, "a not-imported completion label is not refused as behind a completion");
    Require(!Recorder::LookupLabel(base, 4, 3, &refusal).has_value() && refusal == (countAll ? Refusal::BehindCompletion : ordinary), "a GPU-stored label is refused as behind a completion before any write-back");
    Require(Recorder::LookupLabel(base, 4, 2, &refusal).has_value() && refusal == Refusal::None, "an early lookup of a completion label is refused");
    Recorder::NoteWrittenBack(base + 0x80, 0x40);
    Require(Recorder::PendingCompletionLabels() == registered + 1, "a disjoint write-back counted a label");
    Recorder::NoteWrittenBack(base, 4);
    Recorder::NoteWrittenBack(base + 2, 4);
    const std::uint64_t counted = countAll ? 3 : 2;
    Require(Recorder::PendingCompletionLabels() == counted, "a write-back over a GPU-stored label did not count it exactly once");
    Require(Recorder::PendingCompletionStores() == counted, "a write-back over a GPU-stored label did not make it a completion store exactly once");
    int delivered = 0;
    Require(recorder.AfterRecordedWork([&] { ++delivered; }), "an action after recorded work was refused");
    Require(Recorder::PendingCompletionLabels() == counted + 1 && Recorder::PendingCompletionStores() == counted, "an action after recorded work counted as a completion store");
    Require(!Recorder::LookupLabel(base, 4, 3, &refusal).has_value() && refusal == Refusal::BehindCompletion, "a written-back GPU-stored label is not refused as behind a completion");
    Require(!Recorder::LookupLabel(base + 8, 4, 4, &refusal).has_value() && refusal == (countAll ? Refusal::BehindCompletion : ordinary), "an untouched GPU-stored label became behind a completion");
    recorder.Submit();
    device.WaitQueue();
    Require(Recorder::PendingCompletionLabels() == counted + 1 && Recorder::PendingCompletionStores() == counted && Recorder::PendingWriteBackCompletions() == 1, "counts dropped before the batch finished");
    memory[0] = 7;
    memory[2] = 7;
    memory[16] = 7;
    recorder.Sync();
    Require(ran == 1 && delivered == 1 && Recorder::PendingCompletionLabels() == 0 && Recorder::PendingCompletionStores() == 0 && Recorder::PendingWriteBackCompletions() == 0 && recorder.Idle(), "counts did not return to 0 at finish");
    const bool storeAlways = std::getenv("APS5_LABEL_STORE_ALWAYS") != nullptr;
    Require(memory[0] == 1 && memory[2] == (storeAlways ? 1u : 7u) && memory[16] == 1, "completion stores ran for the wrong labels (overlapped and not-imported ones store, the untouched GPU-stored one skips)");
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }

    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

std::size_t DeferredFailureLines(Recorder& recorder, std::size_t count, const std::function<void()>& failure, std::string* log = nullptr) {
    StderrCapture capture;
    for (std::size_t index = 0; index < count; ++index) recorder.OnComplete(failure);
    recorder.Sync();
    const auto text = capture.Finish();
    if (log != nullptr) *log = text;
    std::size_t lines = 0;
    for (auto at = text.find("deferred write-back failed"); at != std::string::npos; at = text.find("deferred write-back failed", at + 1)) ++lines;
    return lines;
}

void deferredFailureReportTests(Recorder& recorder) {
    std::uint32_t serial = 0;
    const auto plain = [&serial] { throw std::runtime_error("image table: poisoned entry " + std::to_string(serial++)); };
    const auto unaligned = [](std::uint32_t instruction) { return [instruction] { throw BdaUnalignedAccess("BDA access failed: unaligned", instruction); }; };
    Require(DeferredFailureLines(recorder, 12, plain) == 12, "a deferred failure other than an unaligned BDA access was not printed every time");
    Require(DeferredFailureLines(recorder, 12, unaligned(0x5c8)) == 8, "the first 8 deferred reports of an unaligned BDA access were not the only ones printed");
    std::string log;
    Require(DeferredFailureLines(recorder, 1012, unaligned(0x5c8), &log) == 1 && log.find("pc 0x5c8 1024 times so far") != std::string::npos, "the 1024th deferred report of an unaligned BDA access was not the only one printed");
    Require(DeferredFailureLines(recorder, 12, unaligned(0x7a0)) == 8, "unaligned BDA reports at one instruction limited the printing of the reports at another");
    Require(DeferredFailureLines(recorder, 12, plain) == 12, "unaligned BDA reports limited the printing of other deferred failures");
}

void directCompletionLabelTests(Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t size = 65536;
    struct Release {
        Recorder& recorder;
        void* mapping = nullptr;
        std::int64_t physical = -1;
        ~Release() {
            recorder.Sync();
            if (mapping != nullptr) sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), size);
            if (physical >= 0) sceKernelReleaseDirectMemory(physical, size);
        }
    } release{recorder};
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, size, size, 0, &release.physical) == 0, "allocate completion label backing");
    Require(sceKernelMapDirectMemory(&release.mapping, size, 3, 0, release.physical, size) == 0, "map completion label backing");
    auto* memory = static_cast<std::byte*>(release.mapping);
    std::memset(memory, 7, size);
    const auto address = reinterpret_cast<std::uint64_t>(memory) + 4092;
    std::array<std::byte, 16> value;
    for (const auto bytes : {4u, 8u, 16u}) {
        value.fill(static_cast<std::byte>(bytes));
        const auto before = CollectWritesUncached(address, bytes);
        recorder.NotePendingWrite(address, bytes);
        recorder.AfterCompletions(address, std::span(value).first(bytes), 100 + bytes, 0, false);
        value.fill(std::byte{99});
        bool observed = false;
        Require(recorder.AfterRecordedWork([&] {
            observed = std::all_of(memory + 4092, memory + 4092 + bytes, [bytes](auto b) { return b == static_cast<std::byte>(bytes); });
        }), "completion observer was not recorded");
        recorder.Sync();
        Require(observed, "a following completion observed incorrect label bytes");
        Require(memory[4091] == std::byte{7} && memory[4108] == std::byte{7}, "completion label changed neighboring bytes");
        Require(recorder.Idle() && Recorder::PendingCompletionLabels() == 0, "completion label remained pending after sync");
        if (before != 0) {
            Require(StoredOver(address, bytes, before), "completion label was not stamped");
            Require(UnchangedSinceCollected(address, bytes, before), "completion label dirtied the guest page");
            const auto after = CollectWritesUncached(address, bytes);
            memory[4092] = std::byte{81};
            Require(!UnchangedSinceCollected(address, bytes, after), "completion store disabled subsequent guest tracking");
        }
    }
}

void afterRecordedWorkTests(const Device& device, Recorder& recorder) {
    alignas(64) static std::uint32_t memory[16];
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const std::array<std::byte, 4> value{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    recorder.Sync();
    std::vector<int> ran;
    std::uint32_t seen = 0;
    Require(recorder.Idle() && !recorder.AfterRecordedWork([&] { ran.push_back(0); }) && ran.empty() && Recorder::PendingCompletionLabels() == 0, "an idle recorder kept an action for recorded work");
    recorder.NotePendingWrite(0x52000, 0x100);
    Require(!Recorder::PendingLabelSince().has_value(), "a write started the label flush deadline");
    Require(recorder.AfterRecordedWork([&] { ran.push_back(1); }) && Recorder::PendingCompletionLabels() == 1 && Recorder::PendingLabelSince().has_value(), "an action behind the open batch is not pending under the label flush deadline");
    recorder.AfterCompletions(base, value, 6, 0, false);
    Require(recorder.AfterRecordedWork([&] { seen = memory[0]; ran.push_back(2); }) && Recorder::PendingCompletionLabels() == 3, "an action behind a completion label is not pending");
    recorder.Submit();
    Require(recorder.AfterRecordedWork([&] { ran.push_back(3); }) && Recorder::PendingCompletionLabels() == 4, "an action behind an in-flight batch is not pending");
    device.WaitQueue();
    Require(ran.empty(), "an action ran before its batch was reaped");
    recorder.Sync();
    Require(ran == std::vector<int>{1, 2, 3} && Recorder::PendingCompletionLabels() == 0 && recorder.Idle(), "actions behind recorded work did not run once each, in order");
    Require(seen == 1, "an action ran before the completion label recorded ahead of it");
}

int gpuTimingCompletionTests(const Device& device, bool incomplete) {
    const auto& context = device.GetContext();
    if (!device.Timestamps()) {
        std::cout << "skipped, queue timestamp queries unavailable\n";
        return 77;
    }
    Require(Recorder::GpuTimingEnabled(), "GPU timing completion test requires APS5_PROFILE_GPU");
    std::unique_lock gpu(GpuMutex());
    int completed = 0;
    Recorder recorder(context);
    recorder.Activate();
    for (int round = 0; round < 3; ++round) {
        const bool missingEnd = incomplete && round == 1;
        const auto first = recorder.BeginGpuTiming(0x12340);
        Require(first != Recorder::NoTiming, "first GPU timing range was not allocated");
        if (!missingEnd) recorder.EndGpuTiming(first);
        const auto second = recorder.BeginGpuTiming(0x12341);
        Require(second != Recorder::NoTiming && second != first, "second GPU timing range was not allocated");
        recorder.EndGpuTiming(second);
        recorder.OnComplete([&] { ++completed; });
        const auto serial = recorder.Submissions() + 1;
        recorder.Submit();
        recorder.Sync();
        Require(completed == round + 1 && recorder.Idle(), "GPU timing prevented batch completion");
        std::size_t missing = 0;
        const auto batches = recorder.CompletedBatches(serial - 1, serial, missing);
        Require(missing == 0 && batches.size() == 1 && batches.front().serial == serial, "GPU timing batch completion record is missing");
        const auto& batch = batches.front();
        if (missingEnd) {
            Require(batch.gpuStartNs == 0 && batch.gpuEndNs == 0, "incomplete query batch published GPU timings");
        } else {
            Require(batch.gpuStartNs > 0 && batch.gpuEndNs >= batch.gpuStartNs, "complete query batch lost GPU timings");
        }
    }
    std::cout << "GPU timing batch completion checked, incomplete=" << incomplete << '\n';
    return 0;
}

void batchStampTests(Recorder& recorder) {
    Require(recorder.Idle(), "batch stamps: the recorder is busy");
    double previousEnd = 0;
    for (int round = 0; round < 3; ++round) {
        recorder.NotePendingWrite(0x60000, 0x100);
        const auto serial = recorder.Submissions() + 1;
        recorder.Submit();
        recorder.Sync();
        std::size_t missing = 0;
        const auto batches = recorder.CompletedBatches(serial - 1, serial, missing);
        Require(missing == 0 && batches.size() == 1 && batches.front().serial == serial, "a finished batch has no completion record");
        const auto& batch = batches.front();
        if (Recorder::BatchStampsEnabled()) {
            Require(batch.gpuStartNs > 0 && batch.gpuEndNs >= batch.gpuStartNs && batch.gpuStartNs >= previousEnd, "batch stamps are missing or out of order");
            previousEnd = batch.gpuEndNs;
        } else {
            Require(batch.gpuStartNs == 0 && batch.gpuEndNs == 0, "batch stamps were written with profiling off");
        }
    }
    std::cout << "batch stamps " << (Recorder::BatchStampsEnabled() ? "checked" : "off") << '\n';
}

void labelTests(Recorder& recorder) {
    const std::array<std::byte, 4> value{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    Require(!recorder.PendingLabelIn(0x60000, 0x100), "an empty table reports a label");
    // A label this worker queued but has not recorded yet is inside the range for a CPU store decision.
    Recorder::NoteQueuedLabel(0x60010, value, 1, AgcDriver::GuestMemory::GpuLockThreadTag());
    Require(recorder.PendingLabelIn(0x60010, 4) && recorder.PendingLabelIn(0x60000, 0x100) && !recorder.PendingLabelIn(0x60014, 4) && !recorder.PendingLabelIn(0x60000, 0x10), "a queued label is not seen by PendingLabelIn");
    Recorder::ForgetQueuedLabels();
    Require(!recorder.PendingLabelIn(0x60000, 0x100), "a forgotten queued label is still pending");
    recorder.NoteLabel(0x60020, value, 2, 0);
    Require(recorder.PendingLabelIn(0x60020, 4) && recorder.PendingLabelIn(0x60000, 0x100) && !recorder.PendingLabelIn(0x60000, 0x20), "a recorded label is not seen by PendingLabelIn");
    recorder.Sync();
    Require(!recorder.PendingLabelIn(0x60000, 0x100), "a recorded label outlived its batch");
}

// The late rule (Recorder::NoteLabel): an entry with stamp <= afterStamp is served once its group
// closed, unless a non-label write covered it, it is queued, or APS5_LABEL_TRUST_LATE=0 (run the
// binary with that set for the switch-off case).
void lateLabelTests(Recorder& recorder) {
    using Refusal = Recorder::LabelRefusal;
    const std::array<std::byte, 4> one{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    const std::array<std::byte, 4> two{std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}};
    const std::array<std::byte, 8> pair{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}};
    const auto tag = AgcDriver::GuestMemory::GpuLockThreadTag();
    constexpr std::uint64_t a = 0x70000, b = 0x70100, c = 0x70200, d = 0x70300;
    Refusal refusal{};
    const auto before = Recorder::LateCounts();
    recorder.NoteLabel(a, one, 10, 0);
    const auto early = Recorder::LookupLabel(a, 4, 5, &refusal);
    Require(early.has_value() && !early->late && early->value == 1 && early->queue == 0 && early->stamp == 10 && early->generation == 0 && refusal == Refusal::None, "an early lookup is wrong");
    auto late = Recorder::LookupLabel(a, 4, 10, &refusal);
    if (!Recorder::LateTrust()) {
        Require(!late.has_value() && refusal == Refusal::TrustOff, "(1) a late lookup is not refused with the switch off");
        Require(Recorder::LateCounts().candidates == before.candidates + 1, "(1) the late candidate is not counted with the switch off");
        Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
        Require(!Recorder::LookupLabel(a, 4, 10, &refusal).has_value() && refusal == Refusal::TrustOff, "(1) a closed group is served with the switch off");
        Require(Recorder::LookupLabel(a, 4, 5).has_value(), "(1) the early lookup broke with the switch off");
        recorder.Sync();
        Require(!Recorder::LookupLabel(a, 4, 5).has_value(), "(7) the entry outlived its batch");
        std::cout << "late label tests ran with APS5_LABEL_TRUST_LATE=0\n";
        return;
    }
    Require(!late.has_value() && refusal == Refusal::Unclosed, "(2) an unclosed group is served");
    Require(Recorder::LateCounts().candidates == before.candidates + 1 && Recorder::LateCounts().unclosed == before.unclosed + 1, "(2) the unclosed refusal is not counted");
    const auto generation = AgcDriver::GuestMemory::TrackerGeneration();
    Recorder::CloseLabelGroup(generation);
    late = Recorder::LookupLabel(a, 4, 10, &refusal);
    Require(late.has_value() && late->late && late->value == 1 && late->generation == generation && late->stamp == 10 && refusal == Refusal::None, "(2) a closed group is not served");
    Require(Recorder::LookupLabel(a, 4, 9)->late == false, "(2) an early lookup became late");
    // (3) A disjoint write leaves the entry alone; an overlapping non-label write refuses the late
    // lookup only (the early one still composes what memory will hold in order).
    recorder.NotePendingWrite(a + 4, 16);
    Require(Recorder::LookupLabel(a, 4, 10).has_value(), "(3) a disjoint write flagged the entry");
    recorder.NotePendingWrite(a - 8, 12);
    late = Recorder::LookupLabel(a, 4, 10, &refusal);
    Require(!late.has_value() && refusal == Refusal::Overwritten, "(3) an overwritten entry is served late");
    Require(Recorder::LookupLabel(a, 4, 5).has_value(), "(3) an overwrite refused the early lookup");
    // The label's own note (inside NoteLabel) is no overwrite: an 8-byte label closed as a group.
    recorder.NoteLabel(b, pair, 20, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    const auto wide = Recorder::LookupLabel(b, 8, 20, &refusal);
    Require(wide.has_value() && wide->late && wide->value == 0x200000001ull && refusal == Refusal::None, "(3) the label's own note refused it");
    // (4) A later label on the dword replaces the entry: unclosed until its own group closes.
    recorder.NoteLabel(a, two, 30, 0);
    Require(!Recorder::LookupLabel(a, 4, 40, &refusal).has_value() && refusal == Refusal::Unclosed, "(4) a replaced entry kept the old close or flag");
    const auto replaced = Recorder::LookupLabel(a, 4, 25);
    Require(replaced.has_value() && !replaced->late && replaced->value == 2 && replaced->stamp == 30, "(4) the replaced entry's early lookup is wrong");
    const auto generation2 = AgcDriver::GuestMemory::TrackerGeneration();
    Recorder::CloseLabelGroup(generation2);
    late = Recorder::LookupLabel(a, 4, 40, &refusal);
    Require(late.has_value() && late->late && late->value == 2 && late->generation == generation2, "(4) the replaced entry is not late-trustable after its close");
    // (5) Queued entries: another queue's is never seen, this queue's is early-only.
    const std::uint32_t other = tag == 7 ? 8 : 7;
    Recorder::NoteQueuedLabel(d, one, 50, other);
    Require(!Recorder::LookupLabel(d, 4, 60, &refusal).has_value() && refusal == Refusal::None && !Recorder::LookupLabel(d, 4, 40).has_value(), "(5) another queue's queued label is served");
    Recorder::NoteQueuedLabel(d, one, 51, tag);
    Recorder::ForgetQueuedLabels();
    Recorder::NoteQueuedLabel(c, one, 50, tag);
    Require(Recorder::LookupLabel(c, 4, 40).has_value() && !Recorder::LookupLabel(c, 4, 40)->late, "(5) this queue's queued label is not served early");
    Require(!Recorder::LookupLabel(c, 4, 60, &refusal).has_value() && refusal == Refusal::Queued, "(5) a queued entry is late-trusted");
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(!Recorder::LookupLabel(c, 4, 60, &refusal).has_value() && refusal == Refusal::Queued, "(5) a close closed a queued entry");
    Recorder::ForgetQueuedLabels();
    Require(!Recorder::LookupLabel(c, 4, 40).has_value(), "(5) a forgotten queued entry is still served");
    const auto after = Recorder::LateCounts();
    Require(after.queued == before.queued + 2 && after.overwritten == before.overwritten + 1 && after.unclosed == before.unclosed + 2 && after.candidates >= before.candidates + 6, "late counters are off");
    // (7) Entries leave the table with their batch.
    recorder.Sync();
    Require(!Recorder::LookupLabel(a, 4, 5).has_value() && !Recorder::LookupLabel(b, 8, 5).has_value() && !recorder.PendingLabelIn(0x70000, 0x400), "(7) entries outlived their batch");
}

void largeLabelTests(Recorder& recorder) {
    constexpr std::uint64_t slot = 0x71000, small = slot + 0x10, outside = slot - 4, wide = 0x72000;
    const std::array<std::byte, 4> one{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    std::array<std::byte, 256> clear{};
    std::array<std::byte, Recorder::LabelTableBytes> limit{};
    limit.fill(std::byte{3});
    recorder.Sync();
    Require(recorder.PendingLabels() == 0 && !Recorder::PendingLabelSince().has_value() && !Recorder::WideLabelIn(0, ~0ull), "the table is not empty before the large label tests");
    recorder.NoteLabel(small, one, 100, 0);
    recorder.NoteLabel(outside, one, 100, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(recorder.PendingLabels() == 2 && Recorder::LookupLabel(small, 4, 50).has_value(), "4-byte labels did not enter the table");
    recorder.NoteLabel(slot, clear, 101, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(recorder.PendingLabels() == 1 && Recorder::LookupLabel(outside, 4, 50).has_value(), "a label larger than the table bound entered dwords or removed one outside its range");
    Require(!Recorder::LookupLabel(small, 4, 50).has_value() && !Recorder::LookupLabel(slot, 8, 0).has_value(), "an older label under a larger one still composes the value");
    Require(Recorder::WideLabelIn(slot, 4) && Recorder::WideLabelIn(slot + clear.size() - 4, 4) && Recorder::WideLabelIn(slot - 0x100, 0x104) && !Recorder::WideLabelIn(slot - 0x100, 0x100) && !Recorder::WideLabelIn(slot + clear.size(), 4), "a large label's range is wrong");
    Require(recorder.PendingLabelIn(slot + 0x20, 0x20) && recorder.PendingWriteOverlaps(slot, clear.size()), "a large label is not pending");
    Require(Recorder::PendingLabelSince().has_value(), "a large label did not start the label flush deadline");
    recorder.NoteLabel(small, one, 102, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    const auto later = Recorder::LookupLabel(small, 4, 101);
    Require(later.has_value() && later->value == 1 && later->stamp == 102, "a label after a large one is not served");
    recorder.NoteLabel(wide, limit, 103, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(recorder.PendingLabels() == 2 + Recorder::LabelTableBytes / 4 && !Recorder::WideLabelIn(wide, limit.size()), "a label at the table bound did not enter every dword");
    const auto bound = Recorder::LookupLabel(wide + Recorder::LabelTableBytes - 8, 8, 0);
    Require(bound.has_value() && bound->value == 0x0303030303030303ull && bound->stamp == 103, "a label at the table bound is not served");
    recorder.Sync();
    Require(recorder.PendingLabels() == 0 && !Recorder::WideLabelIn(0, ~0ull) && !recorder.PendingLabelIn(slot, clear.size()) && !recorder.PendingWriteOverlaps(slot, clear.size()), "large label tests left entries, ranges or writes behind");
}

// (6) GuestMemory::UnchangedSinceCollected: false outside the watched arena, and around a
// MarkWritten or a CPU write of the block inside it.
void unchangedSinceTests() {
    using namespace AgcDriver::GuestMemory;
    static std::uint32_t outside[16];
    Require(!UnchangedSinceCollected(reinterpret_cast<std::uint64_t>(outside), 4, TrackerGeneration()), "(6) a range outside the watched memory is unchanged");
    Require(!Watched(reinterpret_cast<std::uint64_t>(outside), 4) && CollectWrites(reinterpret_cast<std::uint64_t>(outside), 4) == 0, "(6) a range outside the watched memory is watched");
    constexpr std::size_t bytes = 65536;
    void* block = AllocateWatched(bytes, bytes);
    if (block == nullptr) {
        std::cout << "no write watching: UnchangedSinceCollected is always false\n";
        return;
    }
    auto* words = static_cast<volatile std::uint32_t*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    words[0] = 0;
    CollectWritesUncached(address, 4);
    const auto generation = TrackerGeneration();
    Require(UnchangedSinceCollected(address, 4, generation), "(6) an untouched range is not unchanged");
    Require(UnchangedSinceCollected(address, 4, generation), "(6) the check itself dirtied the range");
    Require(!UnchangedSinceCollected(address, 4, 0), "(6) generation 0 is unchanged");
    MarkWritten(address, 4);
    Require(UnchangedSinceCollected(address, 4, generation), "(6) a MarkWritten of the block (a GPU label record) refuses");
    Require(!UnchangedSince(address, 4, generation), "(6) a MarkWritten of the block is not seen by UnchangedSince");
    const auto generation2 = TrackerGeneration();
    Require(UnchangedSinceCollected(address, 4, generation2), "(6) unchanged after the stamp fails");
    words[1] = 1;
    Require(!UnchangedSinceCollected(address, 4, generation2), "(6) a CPU write to the page is not seen");
    Require(UnchangedSinceCollected(address, 4, TrackerGeneration()), "(6) the collect did not reset the page");
    const auto generation3 = TrackerGeneration();
    words[1] = 2;
    CollectWritesUncached(address, 4);
    Require(!UnchangedSinceCollected(address, 4, generation3), "(6) a CPU write collected by another caller is not seen");
    ReleaseWatched(block, bytes);
}

// (9) ProvedClearKeys: a surface's DCC keys are scanned once and answered from the proof after,
// until the key range is stamped (a GPU key store's MarkWritten) or the CPU writes it; a scan made
// while recorded work still writes the keys is not kept. With APS5_NO_KEY_FAST_PATH=1 every call
// scans and no proof is stored.
void keyProofTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t bytes = 65536;
    void* block = AllocateWatched(bytes, bytes);
    if (block == nullptr) {
        std::cout << "no write watching: key proofs not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    constexpr std::size_t keyCount = 1024;
    constexpr std::uint64_t surfaceBytes = keyCount * 256;
    auto* keys = static_cast<std::uint8_t*>(block);
    std::memset(keys, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    GuestTextureResource resource{};
    resource.baseAddress = address + 8192;
    resource.width = 256;
    resource.height = 256;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dccAddress = address;
    DccKeyProof proof;
    auto before = KeyProofCounts();
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000, "(9) 0x00 keys are not a 0000 clear");
    auto after = KeyProofCounts();
    Require(after.scanned == before.scanned + 1 && after.proved == before.proved, "(9) the first call did not scan");
    if (!KeyFastPath()) {
        Require(proof.generation == 0, "(9) APS5_NO_KEY_FAST_PATH stored a proof");
        Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000 && proof.generation == 0, "(9) APS5_NO_KEY_FAST_PATH proved keys");
        std::cout << "key fast path off: every call scans\n";
        return;
    }
    Require(proof.generation != 0 && proof.keys == DccKeys::Clear0000 && after.unstable == before.unstable, "(9) a stable scan left no proof");
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000, "(9) the proof answers other keys");
    before = KeyProofCounts();
    Require(before.proved == after.proved + 1 && before.scanned == after.scanned, "(9) the second call scanned");
    const auto generation = proof.generation;
    MarkWritten(address, keyCount);
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000, "(9) unchanged bytes read differently");
    after = KeyProofCounts();
    Require(after.scanned == before.scanned + 1 && after.proved == before.proved && proof.generation > generation, "(9) a stamped key range was proved");
    std::memset(keys, 0x40, keyCount);
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001, "(9) a CPU write of the keys was not seen");
    before = KeyProofCounts();
    Require(before.scanned == after.scanned + 1 && proof.keys == DccKeys::Clear0001, "(9) the CPU write did not make a scan");
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && KeyProofCounts().proved == before.proved + 1, "(9) the new keys were not proved");
    // A recorded write over the keys (noted and stamped as the driver's key stores are): the scans
    // are not kept until its batch signaled.
    recorder.NotePendingWrite(address, keyCount);
    MarkWritten(address, keyCount);
    before = KeyProofCounts();
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && proof.generation == 0, "(9) a scan under a pending write was kept");
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && proof.generation == 0, "(9) a second scan under a pending write was kept");
    after = KeyProofCounts();
    Require(after.scanned == before.scanned + 2 && after.unstable == before.unstable + 2 && after.proved == before.proved, "(9) unstable scans were miscounted");
    recorder.Submit();
    device.WaitQueue();
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && proof.generation != 0, "(9) a signaled write kept the scan unstable");
    recorder.Sync();
    before = KeyProofCounts();
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && KeyProofCounts().proved == before.proved + 1, "(9) the proof did not hold after the batch finished");
}

void targetKeyProofTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t bytes = 65536;
    void* block = AllocateWatched(bytes, bytes);
    if (block == nullptr) {
        std::cout << "no write watching: target key proofs not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    constexpr std::size_t keyCount = 1024;
    constexpr std::uint64_t surfaceBytes = keyCount * 256;
    auto* keys = static_cast<std::uint8_t*>(block);
    std::memset(keys, 0x20, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    const auto scans = [] { return KeyProofCounts().rangeScanned; };
    const auto proofs = [] { return KeyProofCounts().rangeProved; };
    DccRangeProof proof;
    auto scanned = scans();
    auto proved = proofs();
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::ClearRegister, "register clear keys read as another code");
    if (!KeyFastPath()) {
        Require(proof.generation == 0 && scans() == scanned && proofs() == proved, "target key proofs were kept while disabled");
        std::cout << "key fast path off: every target key read scans\n";
        return;
    }
    Require(scans() == scanned + 1 && proofs() == proved && proof.generation != 0 && proof.keys == DccKeys::ClearRegister, "the first read of a settled key range left no proof");
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::ClearRegister && scans() == scanned + 1 && proofs() == proved + 1, "an unchanged key range was scanned again");
    MarkWritten(address, keyCount);
    scanned = scans();
    proved = proofs();
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::ClearRegister && scans() == scanned + 1 && proofs() == proved, "a key store over the range was answered from the proof");
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::ClearRegister && proofs() == proved + 1, "the rescan after a key store left no proof");
    std::memset(keys, 0xff, keyCount);
    scanned = scans();
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::Uncompressed && scans() == scanned + 1, "a CPU write of the keys was not seen");
    scanned = scans();
    proved = proofs();
    Require(ProvedCurrentDccKeys(address, surfaceBytes / 2, proof) == DccKeys::Uncompressed && scans() == scanned + 1 && proofs() == proved, "the proof of another key range answered");
    Require(ProvedCurrentDccKeys(address + 256, surfaceBytes / 2, proof) == DccKeys::Uncompressed && scans() == scanned + 2 && proofs() == proved, "the proof of a key range at another address answered");
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::Uncompressed && scans() == scanned + 3, "a proof taken over a shorter range answered the whole range");
    recorder.NotePendingWrite(address, keyCount);
    MarkWritten(address, keyCount);
    NoteKeysFillOnGpu(address, keyCount, DccKeys::Clear0000);
    scanned = scans();
    proved = proofs();
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::Clear0000 && proof.generation == 0, "a pending key fill was not the answer, or its answer was kept");
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::Clear0000 && scans() == scanned + 2 && proofs() == proved, "a pending key fill was answered from a proof");
    std::memset(keys, 0x00, keyCount);
    recorder.Submit();
    device.WaitQueue();
    recorder.Sync();
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::Clear0000 && proof.generation != 0, "the landed fill was not proved");
    proved = proofs();
    Require(ProvedCurrentDccKeys(address, surfaceBytes, proof) == DccKeys::Clear0000 && proofs() == proved + 1, "the proof after the fill landed did not hold");
}

// (8) The group close against a collect in progress: a CPU store made after the close must refuse
// the entry although another thread's resetting walk of the label's page (over a large range) may
// absorb the store and stamp the block with the generation it took before the close. And an entry
// whose batch was submitted before the close stays unclosed.
void closeRaceTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    using Refusal = Recorder::LabelRefusal;
    const std::array<std::byte, 4> one{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    Refusal refusal{};
    if (!Recorder::LateTrust()) return;
    recorder.NoteLabel(0x80000, one, 70, 0);
    recorder.Submit();
    Recorder::CloseLabelGroup(TrackerGeneration());
    Require(!Recorder::LookupLabel(0x80000, 4, 70, &refusal).has_value() && refusal == Refusal::Unclosed, "(8) a submitted batch's entry was closed");
    device.WaitQueue();
    recorder.Sync();
    constexpr std::size_t bytes = 1u << 20;
    void* block = AllocateWatched(bytes, 65536);
    if (block == nullptr) return;
    const auto base = reinterpret_cast<std::uint64_t>(block);
    // Late in the range, so the walker is usually still ahead of the page when the store lands.
    const auto label = base + 4096 * 250;
    auto* word = reinterpret_cast<volatile std::uint32_t*>(label);
    *word = 0;
    CollectWritesUncached(base, bytes);
    // Joined on every exit, so a failed Require reports instead of terminating on the thread.
    struct Walker {
        std::atomic<bool> stop{false};
        std::thread thread;
        ~Walker() {
            stop.store(true, std::memory_order_relaxed);
            if (thread.joinable()) thread.join();
        }
    } walker;
    walker.thread = std::thread([&] {
        while (!walker.stop.load(std::memory_order_relaxed)) CollectWritesUncached(base, bytes);
    });
    for (std::uint32_t i = 0; i < 4000; ++i) {
        const std::uint64_t stamp = 100 + i;
        recorder.NoteLabel(label, one, stamp, 0);
        Recorder::CloseLabelGroup(TrackerGeneration());
        *word = i + 2;
        const auto hit = Recorder::LookupLabel(label, 4, stamp, &refusal);
        Require(hit.has_value() && hit->late && hit->generation != 0, "(8) the closed entry is not served late");
        Require(!UnchangedSinceCollected(label, 4, hit->generation), "(8) a store after the close is hidden by a racing collect");
    }
    walker.stop.store(true, std::memory_order_relaxed);
    walker.thread.join();
    recorder.Sync();
    Require(!Recorder::LookupLabel(label, 4, 5).has_value(), "(8) the entry outlived its batch");
    ReleaseWatched(block, bytes);
}

// A resource build over host-imported guest memory notes its in-place reads when its writes are
// marked, so a CPU copy into that memory is refused until the batch ran.
// Per-batch store runs (Recorder::RecordStore): the stores of a batch queue until Submit, a
// later writer or reader of a queued store's bytes has the run recorded in place first, and the
// stores land in the import's memory once the batch ran. With APS5_LABEL_RUNS_INLINE=1 or
// APS5_NO_LABEL_RUNS=1 (run the binary with either set) nothing queues and the stores land alike.
void storeRunTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: store runs not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the store test block");
    std::memset(block, 0, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr) {
        std::cout << "host import of the store test block refused: store runs not tested\n";
        return;
    }
    const bool perBatch = std::getenv("APS5_LABEL_RUNS_INLINE") == nullptr && std::getenv("APS5_NO_LABEL_RUNS") == nullptr && std::getenv("APS5_NO_LABEL_BATCHING") == nullptr;
    auto* words = static_cast<volatile std::uint32_t*>(block);
    const auto store = [&](std::size_t word, std::uint32_t value) {
        const std::array<std::byte, 4> value4{std::byte{static_cast<unsigned char>(value)}, std::byte{static_cast<unsigned char>(value >> 8u)}, std::byte{static_cast<unsigned char>(value >> 16u)}, std::byte{static_cast<unsigned char>(value >> 24u)}};
        recorder.RecordStore(import->buffer, address + word * 4 - import->base, value4, address + word * 4);
    };
    Require(recorder.Idle() && !recorder.HasQueuedStores(), "a fresh recorder has queued stores");
    store(0, 1);
    store(1, 2);
    store(4, 3);
    store(4, 4);
    Require(recorder.Recording(), "a store did not open a batch");
    Require(recorder.HasQueuedStores() == perBatch, "per-batch runs do not queue (or inline runs do)");
    Require(recorder.QueuedStoreOverlaps(address, 8) == perBatch && recorder.QueuedStoreOverlaps(address + 16, 4) == perBatch && !recorder.QueuedStoreOverlaps(address + 8, 8), "queued store ranges are wrong");
    const auto before = Recorder::StoreCounts();
    // A reader or writer of untouched bytes forces nothing; one over a queued store records the run.
    recorder.FlushStoresOverlapping(address + 8, 8);
    Require(recorder.HasQueuedStores() == perBatch, "a disjoint range flushed the run");
    recorder.FlushStoresOverlapping(address + 16, 4);
    Require(!recorder.HasQueuedStores() && Recorder::StoreCounts().runsForced == before.runsForced + (perBatch ? 1 : 0), "an overlapping writer did not record the run in place");
    store(2, 5);
    Require(recorder.HasQueuedStores() == perBatch, "a store after a forced run did not start a new run");
    recorder.Submit();
    Require(!recorder.HasQueuedStores() && Recorder::StoreCounts().runsAtSubmit == before.runsAtSubmit + (perBatch ? 1 : 0), "Submit did not record the run");
    recorder.Sync();
    Require(words[0] == 1 && words[1] == 2 && words[2] == 5 && words[4] == 4 && words[3] == 0, "the stores did not land (or the later store of a dword lost)");
    const auto after = Recorder::StoreCounts();
    Require(after.stores == before.stores + 1 && after.replaced == before.replaced + 0 && after.joined == before.joined + 0, "store counters are off");
    store(8, 1);
    recorder.NotePendingWrite(address + 32, 4);
    auto* commandWords = static_cast<std::uint32_t*>(block);
    CommandBuffer disjoint{commandWords + 12, commandWords + 16, commandWords + 12, nullptr, nullptr, nullptr, 0};
    Agc::Command::Emit(&disjoint, 0x7a, {0x20000243, 0x400}, __func__);
    Require(recorder.Recording(), "a disjoint command allocation drained the GPU store");
    CommandBuffer reused{commandWords + 6, commandWords + 10, commandWords + 6, nullptr, nullptr, nullptr, 0};
    Agc::Command::Emit(&reused, 0x7a, {0x20000243, 0x400}, __func__);
    recorder.Sync();
    Require(words[8] == 0x400, "a pending GPU label overwrote a reused command word");
    // The covered-access mask: reported once by the next Commands() call, then cleared.
    VkAccessFlags covered = 0xffffffffu;
    recorder.Commands(&covered);
    Require(covered == 0, "a new batch starts covered");
    recorder.MarkCovered(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    recorder.Commands(&covered);
    Require(covered == (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT), "the covered mask is not reported");
    recorder.Commands(&covered);
    Require(covered == 0, "the covered mask survived a Commands() call");
    recorder.Sync();
    // The hazard tracker (counting mode only): accesses are noted without effect on the batch.
    const std::pair<std::uint64_t, std::uint64_t> range{address, address + 64};
    recorder.NoteAccess(Recorder::CommandClass::Fill, Recorder::Access{{}, std::span(&range, 1), {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    recorder.NoteAccess(Recorder::CommandClass::DispatchLeading, Recorder::Access{std::span(&range, 1), {}, {}, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT});
    Require(recorder.Recording() == Recorder::BarrierValidate(), "a noted access opened a batch with the tracker off (or none with it on)");
    recorder.Sync();
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, bytes);
}

#ifdef _WIN32
bool deferredSettleTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    constexpr std::size_t unit = 65536;
    constexpr std::size_t settledBytes = std::size_t{1} << 20;
    if (context.hostImportAlignment == 0 || context.hostImportAlignment > unit || context.settleQueue == VK_NULL_HANDLE || !WriteWatched()) {
        std::cout << "host imports within 64 KiB, a settle queue or write watching unavailable: deferred settles not tested\n";
        return false;
    }
    const auto registerRange = [](void* block, std::size_t bytes) {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    };
    const auto unregisterRange = [](void* block) {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    };
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(context);
    void* large = AllocateWatched(settledBytes, unit);
    void* spacer = AllocateWatched(unit, unit);
    void* small = AllocateWatched(unit, unit);
    struct Release {
        void* large;
        void* spacer;
        void* small;
        ~Release() {
            ReleaseWatched(large, settledBytes);
            ReleaseWatched(spacer, unit);
            ReleaseWatched(small, unit);
        }
    } release{large, spacer, small};
    const auto largeAddress = reinterpret_cast<std::uint64_t>(large);
    Require(reinterpret_cast<std::uint64_t>(small) != largeAddress + settledBytes && reinterpret_cast<std::uint64_t>(small) + unit != largeAddress, "(d) the 64 KiB block is next to the 1 MiB block, so their imports would join");
    registerRange(large, settledBytes);
    const auto* settled = HostImportFor(context, largeAddress, settledBytes);
    Require(settled != nullptr, "(d) the 1 MiB import was refused");
    if (settled->unwatched) {
        std::cout << "this device compares every Windows import: deferred settles not tested\n";
        unregisterRange(large);
        HostImportFor(context, largeAddress, settledBytes);
        return false;
    }
    const auto address = reinterpret_cast<std::uint64_t>(small);
    std::memset(small, 0x11, unit);
    registerRange(small, unit);
    const auto poll = [&](const std::function<bool()>& done) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            HostImportFor(context, address, unit);
        }
        return done();
    };
    const auto* import = HostImportFor(context, address, unit);
    Require(import != nullptr && import->unwatched && !Watched(address, unit), "(d) a 64 KiB import was not compared at first");
    Require(poll([&] { return Watched(address, unit); }), "(d) a 64 KiB import did not return to the write watch after its settle");
    const auto* back = HostImportFor(context, address, unit);
    Require(back == import && !back->unwatched, "(d) the import back in the write watch is still marked compared");
    BumpCollectEpoch();
    const auto collected = CollectWrites(address, unit);
    Require(collected != 0, "(d) the import back in the write watch is not collected");
    static_cast<volatile std::uint8_t*>(small)[4096] = 0x22;
    CollectWritesUncached(address, unit);
    Require(!UnchangedSince(address, unit, collected), "(d) a CPU store after the import returned to the write watch was missed");
    gpu.unlock();
    const auto beforeRounds = HostImportSettles();
    for (int round = 0; round < 64; ++round) {
        unregisterRange(small);
        registerRange(small, unit);
        const auto* again = HostImportFor(context, address, unit);
        Require(again != nullptr && again->unwatched && !Watched(address, unit), "(d) a 64 KiB buffer mapped again off the device lock was not imported compared");
    }
    const auto afterRounds = HostImportSettles();
    Require(afterRounds.submitted == beforeRounds.submitted && afterRounds.waited == beforeRounds.waited, "(d) importing 64 KiB buffers off the device lock submitted or waited for a settle");
    gpu.lock();
    const auto beforeLocked = HostImportSettles();
    for (int round = 0; round < 64; ++round) {
        unregisterRange(small);
        registerRange(small, unit);
        const auto* again = HostImportFor(context, address, unit);
        Require(again != nullptr && again->unwatched && !Watched(address, unit), "(d) a 64 KiB buffer mapped again under the device lock was not imported compared");
    }
    const auto afterLocked = HostImportSettles();
    Require(afterLocked.submitted == beforeLocked.submitted && afterLocked.waited == beforeLocked.waited, "(d) mapping a 64 KiB buffer again under the device lock, with no lookup between, submitted or waited for a settle");
    Require(poll([&] { return Watched(address, unit); }), "(d) the last buffer mapped again did not return to the write watch");
    unregisterRange(small);
    registerRange(small, unit);
    Require(HostImportFor(context, address, unit) != nullptr, "(d) a buffer mapped again after it returned to the write watch was not imported");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    HostImportFor(context, address, unit);
    unregisterRange(small);
    registerRange(small, unit);
    const auto* replaced = HostImportFor(context, address, unit);
    Require(replaced != nullptr && replaced->unwatched, "(d) a buffer mapped again while its settle may be in flight was not imported");
    Require(poll([&] { return Watched(address, unit); }), "(d) the buffer mapped again while its settle was in flight did not return to the write watch");
    unregisterRange(large);
    registerRange(large, unit);
    const auto* piece = HostImportFor(context, largeAddress, unit);
    Require(piece != nullptr && piece->unwatched, "(d) the first 64 KiB of the large block was not imported compared");
    unregisterRange(large);
    registerRange(large, settledBytes);
    const auto* whole = HostImportFor(context, largeAddress, settledBytes);
    Require(whole != nullptr && whole->bytes == settledBytes, "(d) the large block mapped whole again was not imported whole");
    Require(poll([&] { return Watched(largeAddress, settledBytes); }), "(d) a settled import over a piece that had left the write watch did not return to it");
    SetHostImportDeferredSettles(false);
    unregisterRange(small);
    registerRange(small, unit);
    const auto* kept = HostImportFor(context, address, unit);
    Require(kept != nullptr && kept->unwatched, "(d) with deferred settles off, a 64 KiB buffer mapped again was not imported compared");
    for (int round = 0; round < 20; ++round) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        HostImportFor(context, address, unit);
    }
    const bool stayed = !Watched(address, unit);
    SetHostImportDeferredSettles(true);
    Require(stayed, "(d) with deferred settles off, a 64 KiB import returned to the write watch");
    unregisterRange(small);
    unregisterRange(large);
    HostImportFor(context, address, unit);
    std::cout << "deferred settle tests passed\n";
    return true;
}

bool settledImportsWatchPrivate(const Context& context) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t settledBytes = std::size_t{1} << 20;
    void* large = AllocateWatched(settledBytes, 65536);
    const auto address = reinterpret_cast<std::uint64_t>(large);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(large, settledBytes, true, true, true);
    }
    const auto* settled = HostImportFor(context, address, settledBytes);
    const bool watched = settled != nullptr && !settled->unwatched;
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(large);
    }
    HostImportFor(context, address, settledBytes);
    ReleaseWatched(large, settledBytes);
    Require(settled != nullptr, "the 1 MiB import was refused");
    return watched;
}

bool settleFirstUseRaced(const Context& context, int copies) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t unit = 65536;
    void* small = AllocateWatched(unit, unit);
    struct Release {
        void* small;
        ~Release() { ReleaseWatched(small, unit); }
    } release{small};
    const auto address = reinterpret_cast<std::uint64_t>(small);
    std::memset(small, 0x11, unit);
    Recorder recorder(context);
    recorder.Activate();
    Buffer scratch(context, unit, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(small, unit, true, true, true);
    }
    const auto* import = HostImportFor(context, address, unit);
    Require(import != nullptr && import->unwatched && !Watched(address, unit), "(f) a 64 KiB import was not compared at first");
    const auto commands = recorder.Commands();
    for (int copy = 0; copy < copies; ++copy) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        CopyBuffer(context, commands, import->buffer, 0, scratch.Handle(), 0, unit);
    }
    recorder.Submit();
    const auto submittedAt = std::chrono::steady_clock::now();
    const auto deadline = submittedAt + std::chrono::seconds(5);
    std::uint64_t collected = 0;
    bool returned = false;
    auto returnedAt = submittedAt;
    while (!returned && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        HostImportFor(context, address, unit);
        if (Watched(address, unit)) {
            returnedAt = std::chrono::steady_clock::now();
            BumpCollectEpoch();
            collected = CollectWrites(address, unit);
            returned = true;
        }
    }
    recorder.Reap();
    const bool racing = returned && recorder.InFlightBatches() != 0;
    while (recorder.InFlightBatches() != 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        recorder.Reap();
    }
    const auto reapedAt = std::chrono::steady_clock::now();
    recorder.Sync();
    CollectWritesUncached(address, unit);
    const bool unchanged = returned && UnchangedSince(address, unit, collected);
    const auto since = [&](std::chrono::steady_clock::time_point at) { return std::chrono::duration<double, std::milli>(at - submittedAt).count(); };
    std::cout << "(f) settled import back in the write watch " << since(returnedAt) << " ms after " << copies << " main-queue copies of it were submitted, copies reaped by " << since(reapedAt) << " ms; driver marks after its return=" << !unchanged << '\n';
    Require(returned, "(f) a settled import did not return to the write watch");
    Require(unchanged, "(f) the driver marked a settled import's pages after its settle read completed while earlier main-queue copies of it ran");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(small);
    }
    HostImportFor(context, address, unit);
    return racing;
}

bool settleWriteOrderTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    constexpr std::size_t unit = 65536;
    if (context.hostImportAlignment == 0 || context.hostImportAlignment > unit || context.settleQueue == VK_NULL_HANDLE || !WriteWatched()) {
        std::cout << "host imports within 64 KiB, a settle queue or write watching unavailable: settle write order not tested\n";
        return false;
    }
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(context);
    if (!settledImportsWatchPrivate(context)) {
        std::cout << "this device compares every Windows import: settle write order not tested\n";
        return false;
    }
    void* block = AllocateWatched(unit, unit);
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, unit); }
    } release{block};
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, unit, true, true, true);
    }
    Recorder recorder(context);
    recorder.Activate();
    const auto* import = HostImportFor(context, address, unit);
    Require(import != nullptr && import->unwatched, "the write-order import was not initially compared");
    const auto fill = [&](std::uint32_t value) {
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, import->buffer, HostImportOffset(*import, address), unit, value);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        recorder.NotePendingWrite(address, unit);
        recorder.Submit();
    };
    const auto before = HostImportSettles().submitted;
    fill(0x22222222u);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (HostImportSettles().submitted == before && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(HostImportFor(context, address, unit) == import, "the write-order import changed while settling");
    }
    Require(HostImportSettles().submitted > before, "the write-order import did not submit a settle");
    fill(0x33333333u);
    recorder.Sync();
    while (!Watched(address, unit) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        HostImportFor(context, address, unit);
    }
    Require(Watched(address, unit), "the write-order import did not return to the write watch");
    const auto* bytes = static_cast<const std::uint8_t*>(block);
    Require(std::all_of(bytes, bytes + unit, [](std::uint8_t value) { return value == 0x33; }), "the GPU store after the settle did not preserve its bytes");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, unit);
    std::cout << "settle read between main-queue GPU writes passed\n";
    return true;
}

struct SettleWaitFailure {
    PFN_vkGetDeviceProcAddr resolve;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    std::uint64_t timeout = 0;
    unsigned waits = 0;
    unsigned destroys = 0;
    unsigned frees = 0;
};

SettleWaitFailure* settleWaitFailure = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL settleWaitForFences(VkDevice device, std::uint32_t count, const VkFence* fences, VkBool32 all, std::uint64_t timeout) {
    auto& failure = *settleWaitFailure;
    if (failure.result != VK_SUCCESS) {
        failure.timeout = timeout;
        ++failure.waits;
        return failure.result;
    }
    return reinterpret_cast<PFN_vkWaitForFences>(failure.resolve(device, "vkWaitForFences"))(device, count, fences, all, timeout);
}

VKAPI_ATTR void VKAPI_CALL settleDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* allocator) {
    auto& failure = *settleWaitFailure;
    if (buffer == failure.buffer) ++failure.destroys;
    reinterpret_cast<PFN_vkDestroyBuffer>(failure.resolve(device, "vkDestroyBuffer"))(device, buffer, allocator);
}

VKAPI_ATTR void VKAPI_CALL settleFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    auto& failure = *settleWaitFailure;
    if (memory == failure.memory) ++failure.frees;
    reinterpret_cast<PFN_vkFreeMemory>(failure.resolve(device, "vkFreeMemory"))(device, memory, allocator);
}

PFN_vkVoidFunction VKAPI_CALL settleWaitDeviceProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkWaitForFences") == 0) return reinterpret_cast<PFN_vkVoidFunction>(settleWaitForFences);
    if (std::strcmp(name, "vkDestroyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(settleDestroyBuffer);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(settleFreeMemory);
    return settleWaitFailure->resolve(device, name);
}

bool settleWaitFailureTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    auto context = device.GetContext();
    constexpr std::size_t unit = 65536;
    if (context.hostImportAlignment == 0 || context.hostImportAlignment > unit || context.settleQueue == VK_NULL_HANDLE || !WriteWatched()) return false;
    std::lock_guard gpu(GpuMutex());
    PrepareImportWatch(context);
    if (!settledImportsWatchPrivate(context)) return false;
    ClearHostImports(context.device);
    SettleWaitFailure failure{context.deviceProc};
    settleWaitFailure = &failure;
    context.deviceProc = settleWaitDeviceProc;
    context.functions = nullptr;
    void* block = AllocateWatched(unit, unit);
    Require(block != nullptr, "no watched block for settle wait failure");
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, unit, true, true, true);
    }
    struct Release {
        Context& context;
        SettleWaitFailure& failure;
        void* block;
        ~Release() {
            failure.result = VK_SUCCESS;
            ClearHostImports(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            ReleaseWatched(block, unit);
            settleWaitFailure = nullptr;
        }
    } release{context, failure, block};
    const auto address = reinterpret_cast<std::uint64_t>(block);
    const auto* import = HostImportFor(context, address, unit);
    Require(import != nullptr && import->unwatched, "wait failure import was not initially compared");
    failure.buffer = import->buffer;
    failure.memory = import->memory;
    const auto before = HostImportSettles().submitted;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (HostImportSettles().submitted == before && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        HostImportFor(context, address, unit);
    }
    Require(HostImportSettles().submitted > before, "wait failure did not submit a settle");
    Check(context.Function<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(context.device), "vkDeviceWaitIdle before settle failure");
    for (const auto result : {VK_TIMEOUT, VK_ERROR_DEVICE_LOST}) {
        failure.result = result;
        const auto waits = failure.waits;
        std::string error;
        try {
            ClearHostImports(context.device);
        } catch (const std::runtime_error& caught) {
            error = caught.what();
        }
        failure.result = VK_SUCCESS;
        Require(failure.waits == waits + 1 && failure.timeout > 0 && failure.timeout <= 60'000'000'000ull, "settle retirement did not use a bounded fence wait");
        Require(error == "AGC graphics: vkWaitForFences (settle batch): Vulkan result " + std::to_string(result), "settle retirement did not propagate its Vulkan failure");
        Require(failure.destroys == 0 && failure.frees == 0 && HostImportCovers(context, address, unit), "failed settle wait released its host import");
    }
    ClearHostImports(context.device);
    Require(failure.destroys == 1 && failure.frees == 1, "successful settle retirement did not release the retained import exactly once");
    std::cout << "bounded settle wait preserves imports on timeout and device loss; successful retry releases once\n";
    return true;
}

bool settleFirstUseTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    constexpr std::size_t unit = 65536;
    if (context.hostImportAlignment == 0 || context.hostImportAlignment > unit || context.settleQueue == VK_NULL_HANDLE || !WriteWatched()) {
        std::cout << "host imports within 64 KiB, a settle queue or write watching unavailable: the first use of a settled import not tested\n";
        return false;
    }
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(context);
    if (!settledImportsWatchPrivate(context)) {
        std::cout << "this device compares every Windows import: the first use of a settled import not tested\n";
        return false;
    }
    Require(!settleFirstUseRaced(context, 4096), "(f) the import returned to the write watch while its earlier main-queue reads were still in flight");
    std::cout << "settled import first use ordering tests passed\n";
    return true;
}

bool settleEarlierImportTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    constexpr std::size_t unit = 65536;
    constexpr int copies = 4096;
    if (context.hostImportAlignment == 0 || context.hostImportAlignment > unit || context.settleQueue == VK_NULL_HANDLE || !WriteWatched()) {
        std::cout << "host imports within 64 KiB, a settle queue or write watching unavailable: an earlier import's first use after a settled import's return not tested\n";
        return false;
    }
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(context);
    if (!settledImportsWatchPrivate(context)) {
        std::cout << "this device compares every Windows import: an earlier import's first use after a settled import's return not tested\n";
        return false;
    }
    void* pair = AllocateWatched(2 * unit, unit);
    struct Release {
        void* pair;
        ~Release() {
            SetHostImportRangeWindow(std::uint64_t{64} << 20u);
            ReleaseWatched(pair, 2 * unit);
        }
    } release{pair};
    const auto address = reinterpret_cast<std::uint64_t>(pair);
    std::memset(pair, 0x11, 2 * unit);
    Recorder recorder(context);
    recorder.Activate();
    Buffer scratch(context, unit, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    SetHostImportRangeWindow(unit);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(pair, 2 * unit, true, true, true);
    }
    const auto* earlier = HostImportFor(context, address, 16);
    Require(earlier != nullptr && earlier->base == address && earlier->bytes == unit && earlier->unwatched, "(e) the first window of the range was not imported compared");
    const auto commands = recorder.Commands();
    for (int copy = 0; copy < copies; ++copy) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        CopyBuffer(context, commands, earlier->buffer, 0, scratch.Handle(), 0, unit);
    }
    const auto* whole = HostImportFor(context, address + unit - 16, 32);
    Require(whole != nullptr && whole->base == address && whole->bytes == 2 * unit && whole->unwatched, "(e) an access across the windows did not replace the first window's import");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!Watched(address, 2 * unit) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        HostImportFor(context, address + unit - 16, 32);
    }
    Require(Watched(address, 2 * unit), "(e) the range's import did not return to the write watch");
    Require(recorder.Recording(), "(e) the earlier import's copies were submitted before the range's import returned to the write watch");
    BumpCollectEpoch();
    const auto collected = CollectWrites(address, 2 * unit);
    const auto submittedAt = std::chrono::steady_clock::now();
    recorder.Sync();
    const auto syncedAt = std::chrono::steady_clock::now();
    CollectWritesUncached(address, 2 * unit);
    const bool unchanged = UnchangedSince(address, 2 * unit, collected);
    std::cout << "(e) the range's import back in the write watch before the replaced window's first use ran (" << std::chrono::duration<double, std::milli>(syncedAt - submittedAt).count() << " ms of copies after it); driver marks after its return=" << !unchanged << '\n';
    Require(unchanged, "(e) the driver marked the pages of a settled import while a replaced import of them ran its first use");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(pair);
    }
    HostImportFor(context, address, 16);
    std::cout << "earlier import first use tests passed\n";
    return true;
}

bool settleRetireTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    constexpr std::size_t unit = 65536;
    constexpr int rounds = 64;
    if (context.hostImportAlignment == 0 || context.hostImportAlignment > unit || context.settleQueue == VK_NULL_HANDLE || !WriteWatched()) {
        std::cout << "host imports within 64 KiB, a settle queue or write watching unavailable: imports retired during their settle read not tested\n";
        return false;
    }
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(context);
    if (!settledImportsWatchPrivate(context)) {
        std::cout << "this device compares every Windows import: imports retired during their settle read not tested\n";
        return false;
    }
    void* pair = AllocateWatched(2 * unit, unit);
    struct Release {
        void* pair;
        ~Release() {
            SetHostImportRangeWindow(std::uint64_t{64} << 20u);
            ReleaseWatched(pair, 2 * unit);
        }
    } release{pair};
    const auto address = reinterpret_cast<std::uint64_t>(pair);
    std::memset(pair, 0x11, 2 * unit);
    const auto registerPair = [&] {
        GuestAllocations::Mutation mutation;
        mutation.Add(pair, 2 * unit, true, true, true);
    };
    const auto unregisterPair = [&] {
        GuestAllocations::Mutation mutation;
        mutation.Remove(pair);
    };
    Recorder recorder(context);
    recorder.Activate();
    SetHostImportRangeWindow(unit);
    registerPair();
    const auto before = HostImportSettles();
    for (int round = 0; round < rounds; ++round) {
        const auto* cell = HostImportFor(context, address, 16);
        Require(cell != nullptr && cell->base == address && cell->bytes == unit && cell->unwatched, "(r) the first window of the range was not imported compared");
        std::this_thread::sleep_for(std::chrono::microseconds(150));
        HostImportFor(context, address, 16);
        const auto* grown = HostImportFor(context, address + unit - 16, 32);
        Require(grown != nullptr && grown->base == address && grown->bytes == 2 * unit, "(r) an access across the windows did not replace the first window's import");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        HostImportFor(context, address + unit - 16, 32);
        recorder.Sync();
        recorder.FlushDeferredReleases();
        unregisterPair();
        registerPair();
    }
    const auto after = HostImportSettles();
    const auto* last = HostImportFor(context, address + unit - 16, 32);
    Require(last != nullptr && last->bytes == 2 * unit && last->unwatched, "(r) the range was not imported whole and compared after the rounds");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!Watched(address, 2 * unit) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        HostImportFor(context, address + unit - 16, 32);
    }
    const bool back = Watched(address, 2 * unit);
    std::cout << "(r) " << rounds << " window imports replaced after their settle read was submitted: " << after.parked - before.parked << " while the read ran, " << after.waited - before.waited << " waits, " << after.unmapWaits - before.unmapWaits << " unmaps waited for a read\n";
    Require(after.waited == before.waited, "(r) replacing an import during its settle read waited for the read");
    Require(after.parked != before.parked, "(r) no import was replaced while its settle read ran");
    Require(back, "(r) the import that replaced the windows did not return to the write watch");
    recorder.Sync();
    recorder.FlushDeferredReleases();
    unregisterPair();
    HostImportFor(context, address, 16);
    std::cout << "imports retired during their settle read tests passed\n";
    return true;
}

bool settleStorageImageTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& base = device.GetContext();
    constexpr std::size_t unit = 65536;
    constexpr std::array<std::uint8_t, 4> red{255, 0, 0, 255};
    if (base.hostImportAlignment == 0 || base.hostImportAlignment > unit || base.settleQueue == VK_NULL_HANDLE || !WriteWatched()) {
        std::cout << "host imports within 64 KiB, a settle queue or write watching unavailable: a settled import under a storage image not tested\n";
        return false;
    }
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(base);
    if (!settledImportsWatchPrivate(base)) {
        std::cout << "this device compares every Windows import: a settled import under a storage image not tested\n";
        return false;
    }
    void* block = AllocateWatched(unit, unit);
    Require(block != nullptr, "(i) no watched block although write watching is on");
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, unit); }
    } release{block};
    const auto address = reinterpret_cast<std::uint64_t>(block);
    std::memset(block, 0x5a, unit);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, unit, true, true, true);
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = 128;
    resource.height = 128;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    Require(DescribeSurface(resource).guestBytes == unit, "(i) unexpected surface size");
    Require(Watched(address, unit) && ExistingHostImportFor(context, address, unit) == nullptr, "(i) the block was imported or out of the write watch before the image was made");
    Recorder recorder(context);
    recorder.Activate();
    const auto before = HostImportSettles();
    {
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const auto* import = ExistingHostImportFor(context, address, unit);
        Require(import != nullptr && import->unwatched && !Watched(address, unit), "(i) the image's first upload did not import its block compared");
        const auto decided = [&] {
            const auto now = HostImportSettles();
            return now.rewatched != before.rewatched || now.keptCompared != before.keptCompared;
        };
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!decided() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            HostImportFor(context, address, unit);
        }
        Require(decided(), "(i) the settle read of the image's import did not finish");
        const auto after = HostImportSettles();
        const bool returned = Watched(address, unit);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const VkClearColorValue value{{1.0f, 0.0f, 0.0f, 1.0f}};
        const auto commands = recorder.Commands();
        recorder.Keep(image);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        image->MarkDirty();
        image->Refresh();
        image->WriteBack();
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        std::vector<std::byte> read(unit);
        AgcDriver::GuestMemory::Read(address, read);
        std::size_t lost = 0;
        for (std::size_t at = 0; at < unit; at += 4) {
            for (std::size_t c = 0; c < 4; ++c) {
                if (std::to_integer<std::uint8_t>(read[at + c]) == red[c]) continue;
                ++lost;
                break;
            }
        }
        std::cout << "(i) the image's import " << (returned ? "returned to the write watch" : "stayed compared") << " after its settle read (" << after.rewatched - before.rewatched << " back in the write watch, " << after.keptCompared - before.keptCompared << " kept compared); the refresh after the image's GPU results lost " << lost << " of " << unit / 4 << " texels\n";
        Require(lost == 0, "(i) the refresh after the image's GPU results dropped " + std::to_string(lost) + " of " + std::to_string(unit / 4) + " texels: the import's return to the write watch stamped its block as a CPU store after the image took its generation");
        Require(!returned && after.keptCompared == before.keptCompared + 1, "(i) the settled import under a live storage image returned to the write watch");
    }
    recorder.Sync();
    recorder.FlushDeferredReleases();
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, unit);
    std::cout << "a settled import under a storage image tests passed\n";
    return true;
}

bool settleInitialDccTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& base = device.GetContext();
    constexpr std::size_t unit = 65536;
    constexpr std::size_t tail = 0x4000;
    if ((base.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0 || !device.Graphics() || base.hostImportAlignment == 0 || base.hostImportAlignment > tail || base.settleQueue == VK_NULL_HANDLE || !WriteWatched()) {
        std::cout << "4x color targets, graphics, host imports within 16 KiB, a settle queue or write watching unavailable: an initial multisampled DCC import over a pending settle not tested\n";
        return false;
    }
    {
        std::lock_guard gpu(GpuMutex());
        PrepareImportWatch(base);
        if (!settledImportsWatchPrivate(base)) {
            std::cout << "this device compares every Windows import: an initial multisampled DCC import over a pending settle not tested\n";
            return false;
        }
    }
    std::cout << "write watch: Windows private imports stay watched after a settle read; imports under 1 MiB wait for a deferred settle\n";
    for (const std::size_t count : {256u, 257u, 1025u}) {
        const auto bytes = count * unit;
        auto* block = static_cast<std::uint8_t*>(AllocateWatched(bytes, unit));
        Require(block != nullptr, "(m) cannot allocate adjacent pending imports");
        const auto address = reinterpret_cast<std::uint64_t>(block);
        struct Release {
            const Context& context;
            std::uint8_t* block;
            std::size_t bytes;
            std::size_t registered = 0;
            ~Release() {
                {
                    GuestAllocations::Mutation mutation;
                    for (std::size_t index = 0; index < registered; ++index) mutation.Remove(block + index * unit);
                }
                HostImportFor(context, reinterpret_cast<std::uint64_t>(block), unit);
                SetHostImportWindow(0);
                ReleaseWatched(block, bytes);
            }
        } release{base, block, bytes};
        std::memset(block, 0x11, bytes);
        SetHostImportWindow(unit);
        {
            GuestAllocations::Mutation mutation;
            for (std::size_t index = 0; index < count; ++index) {
                mutation.Add(block + index * unit, unit, true, true, true);
                ++release.registered;
            }
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto at = address + index * unit;
            const auto* import = HostImportFor(base, at, unit);
            Require(import != nullptr && import->base == at && import->bytes == unit && import->unwatched,
                    "(m) an adjacent 64 KiB import did not start with its settle pending");
        }
        {
            std::lock_guard gpu(GpuMutex());
            Require(!Watched(address, bytes), "(m) adjacent pending imports were already watched");
            const auto before = HostImportSettles();
            Require(WatchedAfterSettle(base, address, bytes),
                    "(m) one synchronous settle left part of " + std::to_string(count) + " adjacent imports unwatched");
            const auto after = HostImportSettles();
            BumpCollectEpoch();
            const auto collected = CollectWrites(address, bytes);
            Require(collected != 0, "(m) adjacent settled imports were not collected");
            static_cast<volatile std::uint8_t*>(block)[bytes - 1] = 0x22;
            CollectWritesUncached(address, bytes);
            Require(!UnchangedSince(address, bytes, collected), "(m) a CPU write after the final import settled was missed");
            std::cout << "(m) " << count << " adjacent pending imports settled: " << after.submitted - before.submitted << " reads submitted\n";
        }
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    Recorder recorder(context);
    recorder.Activate();
    enum class Case { Pending, PartialBlock, ComparedNeighbour, DeferredOff, UnwatchedKeys, PendingFmask, PendingFmaskAlone };
    const auto run = [&](Case kind, std::uint32_t height) {
        const bool fmaskAlone = kind == Case::PendingFmaskAlone;
        const bool eightSamples = kind == Case::PendingFmask || fmaskAlone;
        if (eightSamples && (!base.storageImageMultisample || (base.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_8_BIT) == 0)) {
            std::cout << "(m) 8x color or multisampled storage images unavailable: an initial FMASK import over a pending settle not tested\n";
            return;
        }
        const bool partial = kind == Case::PartialBlock || kind == Case::ComparedNeighbour;
        ColorTarget target{};
        target.extent = {128, height};
        target.surfaceExtent = target.extent;
        target.format = VK_FORMAT_R8G8B8A8_UNORM;
        target.elementBytes = 4;
        target.samples = eightSamples ? 8 : 4;
        target.tileMode = ColorTileMode::RenderTarget;
        target.dccPipeAligned = true;
        const auto colorBytes = static_cast<std::size_t>(MultisampledColorBytes(target));
        const auto dccBytes = MultisampledDccBytes(target);
        const auto importBytes = colorBytes + (partial ? tail : unit);
        const auto reserved = (importBytes + unit - 1) & ~(unit - 1);
        const auto fmaskBytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * 4u;
        const auto fmaskReserved = (fmaskBytes + unit - 1) & ~(unit - 1);
        const auto regionBytes = unit + reserved + unit + (kind == Case::UnwatchedKeys ? unit : eightSamples ? fmaskReserved + unit : 0);
        auto* region = static_cast<std::uint8_t*>(AllocateWatched(regionBytes, unit));
        void* block = region != nullptr ? region + unit : nullptr;
        void* keys = region != nullptr && kind == Case::UnwatchedKeys ? region + unit + reserved + unit : nullptr;
        void* fmask = region != nullptr && eightSamples ? region + unit + reserved + unit : nullptr;
        const auto destBytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * 4u;
        void* dest = VirtualAlloc(nullptr, destBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(block != nullptr && dest != nullptr && (kind != Case::UnwatchedKeys || keys != nullptr) && (!eightSamples || fmask != nullptr), "(m) cannot allocate the target");
        const auto address = reinterpret_cast<std::uint64_t>(block);
        const auto neighbour = address + importBytes + tail;
        const auto describe = [](const HostImport* found, std::uint64_t at, std::size_t bytes) {
            char text[256];
            if (found == nullptr) std::snprintf(text, sizeof(text), ": no import of 0x%llx+0x%zx, watched %d", static_cast<unsigned long long>(at), bytes, Watched(at, bytes) ? 1 : 0);
            else std::snprintf(text, sizeof(text), ": import 0x%llx+0x%llx, unwatched %d, settled on demand %d, for 0x%llx+0x%zx, watched %d", static_cast<unsigned long long>(found->base), static_cast<unsigned long long>(found->bytes), found->unwatched ? 1 : 0, found->settleOnDemand ? 1 : 0, static_cast<unsigned long long>(at), bytes, Watched(at, bytes) ? 1 : 0);
            return std::string(text);
        };
        struct Release {
            const Context& context;
            Recorder& recorder;
            void* region;
            std::size_t regionBytes;
            void* block;
            void* keys;
            void* fmask;
            void* dest;
            std::size_t reserved;
            std::uint64_t neighbour;
            ~Release() {
                {
                    std::lock_guard gpu(GpuMutex());
                    recorder.Sync();
                    ClearMultisampleTargets(context.device);
                    ClearCachedTextures(context.device);
                    recorder.FlushDeferredReleases();
                }
                {
                    GuestAllocations::Mutation mutation;
                    mutation.Remove(block);
                    mutation.Remove(dest);
                    if (keys != nullptr) mutation.Remove(keys);
                    if (fmask != nullptr) mutation.Remove(fmask);
                    if (neighbour < reinterpret_cast<std::uint64_t>(block) + reserved) mutation.Remove(reinterpret_cast<void*>(neighbour));
                }
                HostImportFor(context, reinterpret_cast<std::uint64_t>(block), 16);
                SetHostImportDeferredSettles(true);
                ReleaseWatched(region, regionBytes);
                VirtualFree(dest, 0, MEM_RELEASE);
            }
        } release{context, recorder, region, regionBytes, block, keys, fmask, dest, reserved, kind == Case::ComparedNeighbour ? neighbour : std::numeric_limits<std::uint64_t>::max()};
        target.address = address;
        target.surfaceAddress = address;
        target.bytes = colorBytes;
        target.dccAddress = kind == Case::UnwatchedKeys ? reinterpret_cast<std::uint64_t>(keys) : address + colorBytes;
        std::memset(block, 0, importBytes);
        std::memset(reinterpret_cast<void*>(target.dccAddress), 0xc0, dccBytes);
        std::memset(dest, 0x5a, destBytes);
        if (fmask != nullptr) {
            target.cmaskAddress = target.dccAddress + ((dccBytes + 4095) & ~std::size_t{4095});
            target.cmaskBytes = CmaskBytes(target.extent.width, target.extent.height);
            target.cmaskFastClear = true;
            target.fmaskAddress = reinterpret_cast<std::uint64_t>(fmask);
            Require(target.cmaskAddress + target.cmaskBytes <= address + importBytes, "(m) the CMASK does not fit the target's import");
            std::memset(reinterpret_cast<void*>(target.cmaskAddress), 0xcc, static_cast<std::size_t>(target.cmaskBytes));
            std::memset(fmask, 0x11, fmaskBytes);
        }
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(block, importBytes, true, true, true);
            mutation.Add(dest, destBytes, true, true, true);
            if (keys != nullptr) mutation.Add(keys, unit, true, true, true);
            if (fmask != nullptr) mutation.Add(fmask, fmaskBytes, true, true, true);
            if (kind == Case::ComparedNeighbour) mutation.Add(reinterpret_cast<void*>(neighbour), reserved - (importBytes + tail), true, true, true);
        }
        if (kind == Case::UnwatchedKeys) Unwatch(target.dccAddress, dccBytes);
        if (kind == Case::DeferredOff) SetHostImportDeferredSettles(false);
        if (kind == Case::ComparedNeighbour) {
            const auto* other = HostImportFor(context, neighbour, reserved - (importBytes + tail));
            Require(other != nullptr && other->unwatched && !Watched(neighbour, reserved - (importBytes + tail)), "(m) the import sharing the target's last 64 KiB block was not compared" + describe(other, neighbour, reserved - (importBytes + tail)));
        }
        if (fmask != nullptr && !fmaskAlone) {
            const auto* fmaskImport = HostImportFor(context, target.fmaskAddress, fmaskBytes);
            Require(fmaskImport != nullptr && fmaskImport->base == target.fmaskAddress && fmaskImport->bytes == fmaskBytes && fmaskImport->unwatched && !Watched(target.fmaskAddress, fmaskBytes), "(m) the FMASK's import was not compared at first" + describe(fmaskImport, target.fmaskAddress, fmaskBytes));
        }
        const auto* import = HostImportFor(context, address, importBytes);
        Require(import != nullptr && import->base == address && import->bytes == importBytes && import->unwatched && !Watched(address, importBytes), "(m) the target's import of " + std::to_string(importBytes) + " bytes was not compared at first" + describe(import, address, importBytes));
        HostImportSettleCounts quiet{};
        if (fmaskAlone) {
            {
                std::lock_guard gpu(GpuMutex());
                const auto start = HostImportSettles();
                Require(WatchedAfterSettle(context, address, importBytes), "(m) the target's import did not settle before the FMASK's import" + describe(ExistingHostImportFor(context, address, importBytes), address, importBytes));
                const auto settledTarget = HostImportSettles();
                Require(settledTarget.submitted > start.submitted && settledTarget.rewatched > start.rewatched, "(m) the target's import returned to the write watch without a settle read");
            }
            const std::array<std::pair<std::uint64_t, std::size_t>, 3> planes{{{address, colorBytes}, {target.dccAddress, dccBytes}, {target.cmaskAddress, static_cast<std::size_t>(target.cmaskBytes)}}};
            for (const auto& [at, bytes] : planes) {
                const auto* covering = ExistingHostImportFor(context, at, bytes);
                Require(Watched(at, bytes) && (covering == nullptr || !covering->unwatched), "(m) a color, DCC or CMASK plane is not watched before the FMASK's import" + describe(covering, at, bytes));
            }
            quiet = HostImportSettles();
            const auto* fmaskImport = HostImportFor(context, target.fmaskAddress, fmaskBytes);
            Require(fmaskImport != nullptr && fmaskImport->base == target.fmaskAddress && fmaskImport->bytes == fmaskBytes && fmaskImport->unwatched && !fmaskImport->settleOnDemand && !Watched(target.fmaskAddress, fmaskBytes), "(m) the FMASK's import is not compared with its deferred settle wanted" + describe(fmaskImport, target.fmaskAddress, fmaskBytes));
        }
        std::unique_lock gpu(GpuMutex());
        if (partial) {
            for (int round = 0; round < 20; ++round) {
                gpu.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                gpu.lock();
                HostImportFor(context, address, importBytes);
            }
            Require(!Watched(address, importBytes), "(m) a deferred settle returned an import that is not a whole number of 64 KiB blocks to the write watch");
        }
        const auto before = HostImportSettles();
        Require(!fmaskAlone || before.submitted == quiet.submitted, "(m) a settle read was submitted between the FMASK's import and the target's first use");
        std::string refusal;
        try {
            Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "(m) the multisampled DCC target has no view");
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        const auto after = HostImportSettles();
        const auto* settled = ExistingHostImportFor(context, address, importBytes);
        const char* name = kind == Case::Pending ? "a pending settle" : kind == Case::PartialBlock ? "an import of whole and partial 64 KiB blocks" : kind == Case::ComparedNeighbour ? "a compared import in its last 64 KiB block" : kind == Case::DeferredOff ? "deferred settles off" : kind == Case::UnwatchedKeys ? "explicitly unwatched DCC keys" : kind == Case::PendingFmask ? "an eight-sample target whose color and FMASK imports both have pending settles" : "an eight-sample target whose FMASK alone has a pending settle";
        std::cout << "(m) initial DCC over " << name << " (import " << importBytes << " bytes): " << (refusal.empty() ? "accepted" : refusal) << "; " << after.submitted - before.submitted << " settle reads submitted, " << after.rewatched - before.rewatched << " back in the write watch\n";
        if (kind == Case::ComparedNeighbour || kind == Case::DeferredOff || kind == Case::UnwatchedKeys) {
            Require(refusal.find("initial multisampled DCC contents require watched guest memory") != std::string::npos, std::string("(m) an initial multisampled DCC import over ") + name + " was not refused: " + refusal);
            Require(kind == Case::UnwatchedKeys || !Watched(address, importBytes), std::string("(m) ") + name + " returned the target's import to the write watch");
            Require(kind != Case::UnwatchedKeys || (Watched(address, importBytes) && !Watched(target.dccAddress, dccBytes)), "(m) the refusal over explicitly unwatched DCC keys did not come from the keys" + describe(settled, address, importBytes));
            return;
        }
        Require(refusal.empty(), std::string("(m) an initial multisampled DCC import over ") + name + " was refused: " + refusal);
        Require(!fmaskAlone || (after.submitted - before.submitted == 1 && after.rewatched - before.rewatched == 1), "(m) the first use over a pending FMASK settle alone submitted " + std::to_string(after.submitted - before.submitted) + " settle reads, not the FMASK's alone");
        Require(after.submitted > before.submitted && after.rewatched > before.rewatched && Watched(address, importBytes) && settled != nullptr && !settled->unwatched, std::string("(m) the initial DCC import over ") + name + " did not settle the import and return it to the write watch");
        if (fmask != nullptr) {
            const auto* settledFmask = ExistingHostImportFor(context, target.fmaskAddress, fmaskBytes);
            Require(Watched(target.fmaskAddress, fmaskBytes) && settledFmask != nullptr && !settledFmask->unwatched, "(m) the FMASK's import did not settle back into the write watch" + describe(settledFmask, target.fmaskAddress, fmaskBytes));
        }
        ColorTarget destination{};
        destination.address = reinterpret_cast<std::uint64_t>(dest);
        destination.surfaceAddress = destination.address;
        destination.extent = target.extent;
        destination.surfaceExtent = target.extent;
        destination.format = VK_FORMAT_R8G8B8A8_UNORM;
        destination.elementBytes = 4;
        destination.bytes = destBytes;
        destination.samples = 1;
        destination.tileMode = ColorTileMode::RenderTarget;
        RunColorMetadataPass(context, {ColorMetadataPass::Mode::Resolve, {destination}, target});
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        std::vector<std::uint32_t> resolved(destBytes / 4);
        AgcDriver::GuestMemory::Read(destination.address, std::as_writable_bytes(std::span(resolved)));
        Require(std::ranges::all_of(resolved, [](std::uint32_t word) { return word == 0xffffffffu; }), std::string("(m) the target made over ") + name + " did not start from its DCC fast clear");
        BumpCollectEpoch();
        if (fmask != nullptr) {
            const auto collectedFmask = CollectWrites(target.fmaskAddress, fmaskBytes);
            Require(collectedFmask != 0, "(m) the settled FMASK import is not collected");
            static_cast<volatile std::uint8_t*>(fmask)[0] = 0x22;
            std::string fmaskReuse;
            try {
                MultisampleTargetView(context, target);
            } catch (const std::exception& error) {
                fmaskReuse = error.what();
            }
            Require(fmaskReuse.find("writes to a resident native FMASK are not implemented") != std::string::npos, "(m) the resident target missed the CPU store to its FMASK: " + fmaskReuse);
            return;
        }
        const auto collected = CollectWrites(address, importBytes);
        Require(collected != 0, "(m) the settled import is not collected");
        static_cast<volatile std::uint8_t*>(block)[importBytes - 1] = 0x77;
        static_cast<volatile std::uint8_t*>(block)[colorBytes] = 0x00;
        CollectWritesUncached(address, importBytes);
        Require(!UnchangedSince(address + importBytes - 1, 1, collected) && !UnchangedSince(address + colorBytes, 1, collected), "(m) a CPU store after the initial DCC import was missed");
        std::string reuse;
        try {
            MultisampleTargetView(context, target);
        } catch (const std::exception& error) {
            reuse = error.what();
        }
        Require(reuse.find("writes to a resident multisampled DCC target or its metadata are not implemented") != std::string::npos, "(m) the resident target missed the CPU store to its DCC keys: " + reuse);
    };
    run(Case::Pending, 128);
    run(Case::PartialBlock, 384);
    run(Case::ComparedNeighbour, 384);
    run(Case::DeferredOff, 128);
    run(Case::UnwatchedKeys, 128);
    run(Case::PendingFmask, 128);
    run(Case::PendingFmaskAlone, 128);
    std::cout << "initial multisampled DCC over a pending settle tests passed\n";
    return true;
}
#endif

void remappedImportTests(const Device& device) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: remapped imports not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the remap test block");
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the remap test block refused: remapped imports not tested\n";
        return;
    }
    const auto first = HostImportSerial(context, address, bytes, true);
    Require(first != 0 && HostImportSerial(context, address, bytes, true) == first, "an unchanged range lost its import");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
        mutation.Add(block, bytes, true, true, true);
    }
    Require(HostImportFor(context, address, bytes) != nullptr, "the remapped range was not imported again");
    const auto second = HostImportSerial(context, address, bytes, true);
    Require(second != 0 && second != first, "the import of a range mapped again was kept");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, bytes);
}

void hostImportRetirementTests(const Device& device) {
    const auto& context = device.GetContext();
    constexpr std::size_t UnitBytes = 2 * 65536;
    constexpr std::size_t AllocationBytes = 4 * UnitBytes;
    constexpr std::size_t CopyBytes = 256;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, AllocationBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(UnitBytes, AllocationBytes);
#endif
    Require(block != nullptr, "cannot allocate the host import retirement block");
    const auto release = [](void* pointer) {
#ifdef _WIN32
        VirtualFree(pointer, 0, MEM_RELEASE);
#else
        std::free(pointer);
#endif
    };
    std::unique_ptr<void, decltype(release)> allocation(block, release);
    auto* memory = static_cast<std::byte*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(context);
    Recorder recorder(context);
    recorder.Activate();
    Buffer readback(context, CopyBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::array<bool, 4> registered{};
    const auto cleanup = [&] {
        if (!gpu.owns_lock()) gpu.lock();
        recorder.Sync();
        recorder.FlushDeferredReleases();
        ClearHostImports(context.device);
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 0; index < registered.size(); ++index) {
            if (!registered[index]) continue;
            mutation.Remove(memory + index * UnitBytes);
            registered[index] = false;
        }
        SetHostImportWindow(0);
    };
    const auto add = [&](std::size_t index) {
        GuestAllocations::Mutation mutation;
        mutation.Add(memory + index * UnitBytes, UnitBytes, true, true, true);
        registered[index] = true;
    };
    const auto mapped = [&](std::size_t index) {
        GuestAllocations::Mapped result;
        for (const auto& range : GuestAllocations::GuestAllocationsAcquire_nid_postfix()) {
            if (range->address == address + index * UnitBytes) result.push_back(range);
        }
        Require(result.size() == 1, "host import retirement lost its registered range");
        return result;
    };
    bool deferredEveryRetirement = true;
    bool preservedEveryRecorder = true;
    try {
        const std::array<const char*, 3> shapes{"same-base growth", "earlier-base overlap", "predecessor split"};
        for (std::size_t shape = 0; shape < shapes.size(); ++shape) {
            SetHostImportWindow(0);
            Require(ImportMappedRanges(context, {}, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), true), "host import retirement could not adopt the device");
            const std::size_t first = shape == 1 ? 1 : 0;
            add(first);
            if (shape == 2) add(1);
            for (std::size_t index = 0; index < AllocationBytes; ++index) memory[index] = static_cast<std::byte>((index * 37u + shape * 19u + 11u) & 0xffu);
            const auto initial = mapped(first);
            const auto initialGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
            gpu.unlock();
            const bool fresh = ImportMappedRanges(context, initial, initialGeneration, false);
            const bool unchanged = ImportMappedRanges(context, initial, initialGeneration, false);
            gpu.lock();
            Require(fresh && unchanged, "host import retirement blocked a fresh or unchanged fast import");
            const auto sourceAddress = address + (shape == 2 ? UnitBytes : first * UnitBytes);
            const auto* imported = HostImportFor(context, sourceAddress, CopyBytes);
            Require(imported != nullptr, "host import retirement could not import its source");
            const auto source = imported->buffer;
            const auto sourceOffset = sourceAddress - imported->base;
            std::array<std::byte, CopyBytes> expected{};
            std::memcpy(expected.data(), reinterpret_cast<const void*>(sourceAddress), CopyBytes);
            recorder.Commands();
            recorder.Submit();
            device.WaitQueue();
            Require(!recorder.Recording() && recorder.InFlightBatches() == 1, "host import retirement requires a submitted batch without an open batch");
            std::size_t changed = 0;
            if (shape == 0) {
                changed = 1;
                add(changed);
            } else if (shape == 1) {
                add(changed);
            } else {
                SetHostImportWindow(UnitBytes);
                add(3);
                changed = 1;
            }
            const auto ranges = mapped(changed);
            const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
            gpu.unlock();
            const bool deferred = !ImportMappedRanges(context, ranges, generation, false);
            gpu.lock();
            const bool preserved = !recorder.Recording() && recorder.InFlightBatches() == 1;
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, source, sourceOffset, readback.Handle(), 0, CopyBytes);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            Require(ImportMappedRanges(context, ranges, generation, true), "host import retirement refused its retry under the GPU lock");
            Require(HostImportFor(context, sourceAddress, CopyBytes) != nullptr, "host import retirement lost the replacement import");
            recorder.Sync();
            readback.Invalidate();
            Require(std::equal(expected.begin(), expected.end(), readback.Bytes().begin()), "host import retirement changed bytes copied from the retired import");
            std::cout << "host import retirement: " << shapes[shape] << " GPU readback verified; deferred=" << deferred << "; recorder preserved=" << preserved << '\n';
            deferredEveryRetirement = deferredEveryRetirement && deferred;
            preservedEveryRecorder = preservedEveryRecorder && preserved;
            cleanup();
        }
    } catch (...) {
        cleanup();
        throw;
    }
    Require(deferredEveryRetirement, "unlocked mapping retired an import instead of requesting the GPU lock");
    Require(preservedEveryRecorder, "unlocked mapping changed recorder batches");
    std::cout << "Host import retirement locking and GPU readback tests passed\n";
}
#ifdef _WIN32
void secondMappingImportTests(const Device& device) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: imports after a second mapping not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
    constexpr std::size_t page = 16384;
    struct Release {
        void* mapping = nullptr;
        void* alias = nullptr;
        std::int64_t physical = -1;
        ~Release() {
            if (alias != nullptr) sceKernelMunmap(reinterpret_cast<std::uint64_t>(alias), page);
            if (mapping != nullptr) sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), bytes);
            if (physical >= 0) sceKernelReleaseDirectMemory(physical, bytes);
        }
    } release;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, bytes, 0, &release.physical) == 0, "allocate the second mapping test backing");
    Require(sceKernelMapDirectMemory(&release.mapping, bytes, 0x33, 0, release.physical, bytes) == 0, "map the second mapping test backing");
    const auto address = reinterpret_cast<std::uint64_t>(release.mapping);
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of singly mapped direct memory refused: imports after a second mapping not tested\n";
        return;
    }
    const auto first = HostImportSerial(context, address, bytes, true);
    Require(first != 0 && HostImportSerial(context, address, bytes, true) == first, "singly mapped direct memory lost its import");
    Require(sceKernelMapDirectMemory(&release.alias, page, 0x33, 0, release.physical, page) == 0, "map the first page a second time");
    Require(HostImportFor(context, address, bytes) != nullptr, "direct memory was not imported again after a second mapping of its first page");
    const auto second = HostImportSerial(context, address, bytes, true);
    Require(second != 0 && second != first, "the import of direct memory whose pages were replaced was kept");
    HostImportFor(context, address, bytes);
}

void requireImportServesGuest(const Context& context, Recorder& recorder, const HostImport& import, std::uint64_t begin, std::uint64_t end, std::uint32_t salt, const std::string& what) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t page = 4096;
    constexpr std::size_t probe = 256;
    const auto bytes = static_cast<std::size_t>(end - begin);
    Require(begin >= import.base && end <= import.base + import.bytes, what + " is not inside its import");
    const auto collected = CollectWritesUncached(begin, bytes);
    Require(collected != 0 && Watched(begin, bytes), what + " left the write watch");
    Require(UnchangedSinceCollected(begin, page, collected) && UnchangedSinceCollected(end - page, page, collected), what + " reports writes nobody made");
    auto* first = reinterpret_cast<std::byte*>(begin);
    auto* last = reinterpret_cast<std::byte*>(end - page);
    for (std::size_t index = 0; index < probe; ++index) {
        first[index] = static_cast<std::byte>((index * 7u + salt) & 0xffu);
        last[index] = static_cast<std::byte>((index * 13u + salt * 5u + 1u) & 0xffu);
    }
    Require(!UnchangedSinceCollected(begin, page, collected) && !UnchangedSinceCollected(end - page, page, collected), "a CPU store into " + what + " was not collected");
    Buffer readback(context, 2 * probe, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto commands = recorder.Commands();
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    CopyBuffer(context, commands, import.buffer, begin - import.base, readback.Handle(), 0, probe);
    CopyBuffer(context, commands, import.buffer, end - page - import.base, readback.Handle(), probe, probe);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    recorder.Sync();
    readback.Invalidate();
    const auto seen = readback.Bytes();
    Require(std::equal(first, first + probe, seen.begin()) && std::equal(last, last + probe, seen.begin() + probe), "the GPU reads other bytes than the guest's through " + what);
}

int reimportBench(const Device& device) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0 || context.settleQueue == VK_NULL_HANDLE || !AgcDriver::GuestMemory::WriteWatched()) {
        std::cout << "skipped, the re-import bench requires host imports, a settle queue and write watching\n";
        return 77;
    }
    constexpr std::size_t bytes = 1126ull << 20u;
    constexpr std::size_t step = 1440ull << 10u;
    constexpr std::size_t guard = 16ull << 10u;
    constexpr std::size_t inside = 256ull << 20u;
    constexpr std::size_t moves = 5;
    constexpr std::size_t page = 4096;
    constexpr std::size_t alignment = 2ull << 20u;
    constexpr std::size_t requiredImportBytes = bytes + inside;
    const auto availableImportBytes = HostImportBudgetRemaining(context);
    if (availableImportBytes < requiredImportBytes) {
        std::cout << "skipped, the re-import bench needs " << (requiredImportBytes >> 20u) << " MiB of simultaneous host imports; the current budget has " << (availableImportBytes >> 20u) << " MiB available\n";
        return 77;
    }
    struct Release {
        void* mapping = nullptr;
        std::int64_t physical = -1;
        ~Release() {
            if (mapping != nullptr) sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), bytes);
            if (physical >= 0) sceKernelReleaseDirectMemory(physical, bytes);
        }
    };
    enum class Cut : std::uint8_t { Head, Tail, Inside };
    struct Pattern {
        Cut cut;
        const char* name;
        const char* moving;
    };
    constexpr std::array<Pattern, 3> patterns{{
        {Cut::Head, "head", "the head loses GPU access 1440 KiB at a time"},
        {Cut::Tail, "tail", "the tail loses GPU access 1440 KiB at a time"},
        {Cut::Inside, "inside", "a 16 KiB page inside the kept side loses all access, 256 MiB in and then 1440 KiB above the one before"},
    }};
    const auto milliseconds = [](std::chrono::steady_clock::time_point from, std::chrono::steady_clock::time_point to) { return std::chrono::duration<double, std::milli>(to - from).count(); };
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(context);
    Recorder recorder(context);
    recorder.Activate();
    const auto cleanup = [&] {
        if (!gpu.owns_lock()) gpu.lock();
        recorder.Sync();
        recorder.FlushDeferredReleases();
        ClearHostImports(context.device);
    };
    std::uint32_t salt = 0;
    const auto measure = [&](const Pattern& pattern) {
        Release release;
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, alignment, 0, &release.physical) == 0, "allocate the re-import bench backing");
        Require(sceKernelMapDirectMemory(&release.mapping, bytes, 0x33, 0, release.physical, alignment) == 0, "map the re-import bench backing");
        const auto address = reinterpret_cast<std::uint64_t>(release.mapping);
        auto* memory = static_cast<std::byte*>(release.mapping);
        for (std::size_t offset = 0; offset < bytes; offset += 4 * page) std::memset(memory + offset, 0x5a, 64);
        std::cout << "re-import bench, " << pattern.name << " cuts: " << (bytes >> 20u) << " MiB of singly mapped direct memory at 0x" << std::hex << address << std::dec << ", watched=" << AgcDriver::GuestMemory::Watched(address, bytes) << ", 1 of 4 pages nonzero, " << pattern.moving << '\n';
        gpu.lock();
        auto begin = address;
        auto end = address + bytes;
        std::size_t again = 0;
        double slowest = 0;
        for (std::size_t move = 0; move <= moves; ++move) {
            double moveMs = 0;
            std::uint64_t smaller = 0;
            std::uint64_t smallerEnd = 0;
            if (move != 0) {
                std::uint64_t from = 0;
                std::uint64_t to = 0;
                if (pattern.cut == Cut::Head) {
                    from = begin;
                    to = begin = from + step;
                } else if (pattern.cut == Cut::Tail) {
                    to = end;
                    from = end = to - step;
                } else {
                    from = move == 1 ? address + inside : begin + step;
                    to = from + guard;
                    smaller = begin;
                    smallerEnd = from;
                    begin = to;
                }
                gpu.unlock();
                const auto moving = std::chrono::steady_clock::now();
                Require(sceKernelMprotect(reinterpret_cast<void*>(from), static_cast<std::size_t>(to - from), pattern.cut == Cut::Inside ? 0 : 0x03) == 0, "cut the re-import bench range");
                moveMs = milliseconds(moving, std::chrono::steady_clock::now());
                gpu.lock();
            }
            const auto started = std::chrono::steady_clock::now();
            const auto* import = HostImportFor(context, begin, static_cast<std::size_t>(end - begin));
            const auto imported = std::chrono::steady_clock::now();
            Require(import != nullptr && import->base == begin && import->base + import->bytes == end, "the re-import bench range was not imported whole");
            const char* kind = import->alias != nullptr ? "through a shared alias" : import->unwatched ? "compared" : "settled and watched";
            const bool kept = import->memoryBase == address && import->pinnedBytes == bytes;
            if (move != 0) {
                again += kept ? 0 : 1;
                slowest = std::max(slowest, milliseconds(started, imported));
            }
            double smallerMs = 0;
            if (smallerEnd != smaller) {
                const auto importing = std::chrono::steady_clock::now();
                Require(HostImportFor(context, smaller, static_cast<std::size_t>(smallerEnd - smaller)) != nullptr, "the smaller side of a cut inside the re-import bench range was not imported");
                smallerMs = milliseconds(importing, std::chrono::steady_clock::now());
            }
            const auto syncing = std::chrono::steady_clock::now();
            recorder.Sync();
            recorder.FlushDeferredReleases();
            const auto synced = std::chrono::steady_clock::now();
            std::printf("re-import %s %zu: %.1f MiB at +%.2f MiB, %s, %s, cut %.2f ms, import %.2f ms", pattern.name, move, static_cast<double>(end - begin) / 1048576.0, static_cast<double>(begin - address) / 1048576.0, kind, move == 0 ? "first import" : kept ? "kept" : "imported again", moveMs, milliseconds(started, imported));
            if (smallerEnd != smaller) std::printf(", smaller side %.2f MiB imported in %.2f ms", static_cast<double>(smallerEnd - smaller) / 1048576.0, smallerMs);
            std::printf(", recorder sync after it %.2f ms, %.1f MiB imported in all\n", milliseconds(syncing, synced), static_cast<double>(HostImportLiveBytes()) / 1048576.0);
            std::fflush(stdout);
        }
        const auto* tail = HostImportFor(context, begin, static_cast<std::size_t>(end - begin));
        Require(tail != nullptr && tail->base == begin && tail->base + tail->bytes == end, "the re-import bench lost the import of its kept side");
        requireImportServesGuest(context, recorder, *tail, begin, end, ++salt, std::string("the kept side after the ") + pattern.name + " cuts");
        std::printf("re-import bench, %s cuts: the kept side was imported again after %zu of %zu cuts, slowest import after a cut %.2f ms; CPU stores into it are collected and the GPU reads them through its buffer\n", pattern.name, again, moves, slowest);
        cleanup();
        gpu.unlock();
        return again;
    };
    std::string importedAgain;
    gpu.unlock();
    try {
        for (const auto& pattern : patterns) {
            if (measure(pattern) != 0) importedAgain += std::string(importedAgain.empty() ? "" : ", ") + pattern.name;
        }
    } catch (...) {
        cleanup();
        throw;
    }
    gpu.lock();
    Require(importedAgain == "head, tail, inside", "a cut kept memory imported across the changed host range instead of importing the remaining side again: " + importedAgain);
    std::printf("re-import bench: every cut at the head, at the tail and inside the range retired the original import before importing the remaining side\n");
    return 0;
}

int hostImportCutTests(const Device& device) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0 || context.settleQueue == VK_NULL_HANDLE || !AgcDriver::GuestMemory::WriteWatched()) {
        std::cout << "skipped, the host import cut tests require host imports, a settle queue and write watching\n";
        return 77;
    }
    constexpr std::size_t bytes = 32ull << 20u;
    constexpr std::size_t run = 2ull << 20u;
    constexpr std::size_t guard = 16ull << 10u;
    constexpr std::size_t page = 4096;
    constexpr std::size_t inside = 6ull << 20u;
    struct Case {
        const char* name;
        std::size_t offset;
        std::size_t length;
        bool unmap;
        std::size_t kept;
        std::size_t keptEnd;
        std::size_t smaller;
        std::size_t smallerEnd;
    };
    constexpr std::array<Case, 5> cases{{
        {"a page made inaccessible inside the import, nearer its start", inside + guard, guard, false, inside + 2 * guard, bytes, 0, inside + guard},
        {"a page made inaccessible inside the import, nearer its end", bytes - inside - 2 * guard, guard, false, 0, bytes - inside - 2 * guard, bytes - inside - guard, bytes},
        {"an unmap of a 2 MiB run inside the import", inside, run, true, inside + run, bytes, 0, inside},
        {"an unmap of the import's first 2 MiB run", 0, run, true, run, bytes, 0, 0},
        {"an unmap of 1440 KiB at the start of the import's first 2 MiB run", 0, 1440ull << 10u, true, 1440ull << 10u, bytes, 0, 0},
    }};
    struct Backing {
        void* mapping = nullptr;
        std::int64_t physical = -1;
        std::size_t hole = 0;
        std::size_t holeBytes = 0;
        ~Backing() {
            auto* memory = static_cast<std::byte*>(mapping);
            if (memory != nullptr && hole != 0) sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory), hole);
            if (memory != nullptr && hole + holeBytes < bytes) sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory + hole + holeBytes), bytes - hole - holeBytes);
            if (physical >= 0) sceKernelReleaseDirectMemory(physical, bytes);
        }
    };
    std::unique_lock gpu(GpuMutex());
    PrepareImportWatch(context);
    Recorder recorder(context);
    recorder.Activate();
    const auto cleanup = [&] {
        if (!gpu.owns_lock()) gpu.lock();
        recorder.Sync();
        recorder.FlushDeferredReleases();
        ClearHostImports(context.device);
    };
    std::uint32_t salt = 0;
    const auto check = [&](const Case& test) {
        const std::string name = test.name;
        Backing backing;
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, run, 0, &backing.physical) == 0, "allocate the host import cut test backing");
        Require(sceKernelMapDirectMemory(&backing.mapping, bytes, 0x33, 0, backing.physical, run) == 0, "map the host import cut test backing");
        const auto address = reinterpret_cast<std::uint64_t>(backing.mapping);
        auto* memory = static_cast<std::byte*>(backing.mapping);
        for (std::size_t offset = 0; offset < bytes; offset += page) std::memset(memory + offset, static_cast<int>(0x40u + (offset / page) % 64u), 64);
        gpu.lock();
        const auto* whole = HostImportFor(context, address, bytes);
        Require(whole != nullptr && whole->base == address && whole->bytes == bytes && whole->alias == nullptr && !whole->unwatched, name + ": the range was not imported whole, settled and watched");
        auto* cut = memory + test.offset;
        gpu.unlock();
        if (test.unmap) {
            Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(cut), test.length) == 0, name + ": the unmap failed");
            backing.hole = test.offset;
            backing.holeBytes = test.length;
        } else {
            Require(sceKernelMprotect(cut, test.length, 0) == 0, name + ": the protection failed");
        }
        gpu.lock();
        const auto begin = address + test.kept;
        const auto end = address + test.keptEnd;
        const auto* kept = HostImportFor(context, begin, static_cast<std::size_t>(end - begin));
        Require(kept != nullptr && kept->base == begin && kept->base + kept->bytes == end, name + ": the larger side is not imported whole");
        Require(kept->memoryBase == begin && kept->pinnedBytes == end - begin, "the side of " + name + " is served from the import that pinned the changed host range");
        requireImportServesGuest(context, recorder, *kept, begin, end, ++salt, "the larger side of " + name);
        if (test.smallerEnd != test.smaller) {
            const auto* smaller = HostImportFor(context, address + test.smaller, test.smallerEnd - test.smaller);
            Require(smaller != nullptr && smaller->base == address + test.smaller && smaller->bytes == test.smallerEnd - test.smaller && smaller->memoryBase == address + test.smaller && smaller->pinnedBytes == test.smallerEnd - test.smaller, "the smaller side of " + name + " is not imported apart from the larger one");
            requireImportServesGuest(context, recorder, *smaller, address + test.smaller, address + test.smallerEnd, ++salt, "the smaller side of " + name);
            Require(HostImportFor(context, begin, static_cast<std::size_t>(end - begin)) == kept && kept->memoryBase == begin && kept->pinnedBytes == end - begin, "importing the smaller side of " + name + " replaced the safe import of the larger one");
        }
        if (test.unmap) {
            gpu.unlock();
            void* again = cut;
            Require(sceKernelMapDirectMemory(&again, test.length, 0x33, 0x10, backing.physical + static_cast<std::int64_t>(test.offset), guard) == 0 && again == cut, name + ": the pages could not be mapped again");
            backing.hole = 0;
            backing.holeBytes = 0;
            gpu.lock();
            const auto from = reinterpret_cast<std::uint64_t>(cut);
            const auto* remapped = HostImportFor(context, address, bytes);
            Require(remapped != nullptr && remapped->base == address && remapped->bytes == bytes, name + ": the range mapped again is not imported whole");
            Require(remapped->memoryBase == address && remapped->pinnedBytes == bytes, "after " + name + " the pages mapped again are served from the import that pinned the old ones");
            requireImportServesGuest(context, recorder, *remapped, from, from + test.length, ++salt, "the pages mapped again after " + name);
            requireImportServesGuest(context, recorder, *remapped, begin, end, ++salt, "the larger side after " + name + " and a new mapping");
        }
        cleanup();
        gpu.unlock();
        std::cout << "host import after cut: " << name << ": the changed import was retired before the remaining range was imported, and the GPU and write watch see the guest's bytes" << (test.unmap ? ", also after the pages were mapped again" : "") << '\n';
    };
    const auto remapWhileTrimmed = [&] {
        const std::string name = "an unmap and a new mapping of the retained side while the old import is retired";
        Backing backing;
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, run, 0, &backing.physical) == 0, "allocate the host import cut test backing");
        Require(sceKernelMapDirectMemory(&backing.mapping, bytes, 0x33, 0, backing.physical, run) == 0, "map the host import cut test backing");
        const auto address = reinterpret_cast<std::uint64_t>(backing.mapping);
        auto* memory = static_cast<std::byte*>(backing.mapping);
        for (std::size_t offset = 0; offset < bytes; offset += page) std::memset(memory + offset, static_cast<int>(0x40u + (offset / page) % 64u), 64);
        gpu.lock();
        const auto* whole = HostImportFor(context, address, bytes);
        Require(whole != nullptr && whole->base == address && whole->bytes == bytes && whole->alias == nullptr && !whole->unwatched, name + ": the range was not imported whole, settled and watched");
        gpu.unlock();
        Require(sceKernelMprotect(memory, run, 0x03) == 0, name + ": the protection of the head failed");
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory + run), bytes - run) == 0, name + ": the unmap of the kept side failed");
        backing.hole = run;
        backing.holeBytes = bytes - run;
        void* again = memory + run;
        Require(sceKernelMapDirectMemory(&again, bytes - run, 0x33, 0x10, backing.physical + static_cast<std::int64_t>(run), guard) == 0 && again == memory + run, name + ": the kept side could not be mapped again");
        backing.hole = 0;
        backing.holeBytes = 0;
        gpu.lock();
        const auto begin = address + run;
        const auto end = address + bytes;
        const auto* remapped = HostImportFor(context, begin, static_cast<std::size_t>(end - begin));
        Require(remapped != nullptr && remapped->base == begin && remapped->base + remapped->bytes == end, name + ": the pages mapped again are not imported whole");
        requireImportServesGuest(context, recorder, *remapped, begin, end, ++salt, "the pages mapped again after " + name);
        Require(remapped->memoryBase == begin && remapped->pinnedBytes == end - begin, "after " + name + " the pages mapped again are served from the import that pinned the old ones");
        cleanup();
        gpu.unlock();
        std::cout << "host import cut: " << name << ": the pages mapped again were imported again and the GPU and the write watch see the guest's bytes\n";
    };
    gpu.unlock();
    try {
        for (const auto& test : cases) check(test);
        remapWhileTrimmed();
    } catch (...) {
        cleanup();
        throw;
    }
    gpu.lock();
    std::cout << "Host import cut lifetime tests passed\n";
    return 0;
}
#endif

void movedMetadataTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: moved DCC metadata not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the moved metadata block");
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* firstKeys = texels + surfaceBytes;
    auto* secondKeys = firstKeys + 4096;
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the moved metadata block refused: moved DCC metadata not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    Require(DescribeSurface(resource).guestBytes == surfaceBytes, "the moved metadata surface has an unexpected size");
    const auto first = address + surfaceBytes;
    const auto second = first + 4096;
    const auto withKeys = [&](std::uint64_t keys) {
        auto described = resource;
        described.dccAddress = keys;
        return described;
    };
    const auto holds = [&](const StorageTexture& image, std::array<std::uint8_t, 4> texel) {
        Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {side, side, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image.Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        const auto pixels = readback.Bytes();
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            if (std::to_integer<std::uint8_t>(pixels[i]) != texel[i % 4]) return false;
        }
        return true;
    };
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(firstKeys, 0xff, keyCount);
    std::memset(secondKeys, 0x00, keyCount);
    const auto original = CachedStorageSurface(context, withKeys(first));
    Require(original->Descriptor().dccAddress == first && holds(*original, {0x55, 0x55, 0x55, 0x55}), "the first image does not hold the stored texels");
    const auto moved = CachedStorageSurface(context, withKeys(second));
    Require(moved->Descriptor().dccAddress == second, "the storage image kept the keys the surface no longer names");
    Require(!StorageImageCached(context, original.get()), "the image of the old keys is still the surface's");
    Require(holds(*moved, {0, 0, 0, 0}), "a fast clear of the moved keys did not reach the image");
    Require(CachedStorageSurface(context, resource) == moved && moved->Descriptor().dccAddress == second, "a descriptor without metadata replaced the image");
    const auto back = CachedStorageSurface(context, withKeys(first));
    Require(back != moved && back->Descriptor().dccAddress == first, "the keys moving back did not remake the image");
    Require(holds(*back, {0x55, 0x55, 0x55, 0x55}), "the image remade under the first keys does not hold the stored texels");
}

void viewPastLastMipTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: views past the last mip not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t bytes = 0x60000;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the past-last-mip block");
    auto* texels = static_cast<std::uint8_t*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the past-last-mip block refused: views past the last mip not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    const auto view = [&](std::uint32_t level) {
        const std::array<std::uint32_t, 8> words{
            static_cast<std::uint32_t>(address >> 8u),
            static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (((side - 1u) & 3u) << 30u),
            ((side - 1u) >> 2u) | ((side - 1u) << 14u),
            0xfacu | (level << 12u) | (level << 16u) | (0x1bu << 20u) | (9u << 28u),
            0u,
            2u << 4u,
            0u,
            0u,
        };
        return DecodeTextureResource(words);
    };
    const auto texel = [&](const StorageTexture& image, std::uint32_t level) {
        Buffer readback(context, 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
        copy.imageExtent = {1, 1, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image.Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        std::uint32_t value = 0;
        std::memcpy(&value, readback.Bytes().data(), sizeof(value));
        return value;
    };
    std::memset(texels, 0x55, bytes);
    std::memset(texels + 0x8400, 0x66, 4);
    std::memset(texels + 0x4800, 0x77, 4);
    const auto allocatedView = view(2);
    const auto pastView = view(3);
    Require(allocatedView.mipCount == 3 && pastView.mipCount == 4 && DescribeSurface(pastView).guestBytes == bytes, "a 256x256 32 bpp SW_64KB_R_X surface viewed past MAX_MIP 2 decoded wrongly");
    const auto allocated = CachedStorageSurface(context, allocatedView);
    Require(allocated->Descriptor().mipCount == 3, "the allocated levels' storage image has an unexpected level count");
    const auto past = CachedStorageSurface(context, pastView);
    Require(past->Descriptor().mipCount == 4, "the storage image of a view past the last mip lacks the level it names");
    Require(!StorageImageCached(context, allocated.get()), "the allocated levels' image is still the surface's after a view past its last mip");
    Require(CachedStorageSurface(context, allocatedView) == past, "a view of the allocated levels took another image than the view past the last mip");
    Require(texel(*past, 2) == 0x66666666u, "level 2 was not read from its addrlib tail slot");
    Require(texel(*past, 3) == 0x77777777u, "the level past the last mip was not read from its addrlib tail slot");
}

void resourceReadTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: resource read notes not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the test block");
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the test block refused: resource read notes not tested\n";
        return;
    }
    const auto element = address + 4096;
    constexpr std::size_t elementBytes = 1024;
    {
        GuestBufferMemory memory(context);
        memory.AddReadable(element, elementBytes);
        memory.Upload(false);
        const auto reads = memory.InPlaceReads();
        Require(reads.size() == 1 && reads[0].first == element && reads[0].second == element + elementBytes, "an imported region is not an in-place read");
        Require(memory.Writes().empty(), "a readable element counts as written");
    }
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {static_cast<std::uint32_t>(element), static_cast<std::uint32_t>(element >> 32u) & 0xffffu, elementBytes, 0x31000000u};
    binding.bufferWritten = {false};
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    {
        ShaderResources resources(context, compute);
        Require(recorder.Idle() && !recorder.PendingReadOverlaps(element, 16), "the build itself noted a read");
        resources.MarkGpuWrites(recorder);
        Require(recorder.PendingReadOverlaps(element, 16) && recorder.PendingReadOverlaps(element + elementBytes - 4, 4) && !recorder.PendingReadOverlaps(element + elementBytes, 16), "the build's in-place read was not noted");
        Require(!recorder.PendingWriteOverlaps(element, elementBytes), "a read-only element was noted as written");
        const auto info = recorder.DescribePendingRead(element, 16);
        Require(info.has_value() && info->kind == Kind::DispatchElement && info->open, "the build's read has the wrong kind");
        recorder.Submit();
        device.WaitQueue();
        Require(!recorder.PendingReadOverlaps(element, 16), "the build's read refuses after its batch ran");
        recorder.Sync();
    }
    std::weak_ptr<ShaderResources::DrawBindings> snapshotLifetime;
    {
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, compute);
        auto first = resources.PrepareDrawBindings(snapshotRecorder);
        Require(first != nullptr && first->snapshots.size() == 1, "read-only draw input was not snapshotted");
        std::memset(reinterpret_cast<void*>(element), 0x22, elementBytes);
        AgcDriver::GuestMemory::MarkWritten(element, elementBytes);
        auto second = resources.PrepareDrawBindings(snapshotRecorder);
        Require(second != nullptr && second->snapshots.size() == 1, "cached draw input was not snapshotted again");
        Require(first->allocation.set != second->allocation.set, "in-flight draws share mutable descriptor bindings");
        std::memset(reinterpret_cast<void*>(element), 0x33, elementBytes);
        AgcDriver::GuestMemory::MarkWritten(element, elementBytes);
        auto downloaded = std::make_shared<Buffer>(snapshotContext, elementBytes * 2, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = snapshotRecorder.Commands();
        RecordMemoryBarrier(snapshotContext, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        const auto copy = snapshotContext.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
        VkBufferCopy region{first->snapshots[0].offset, 0, elementBytes};
        copy(commands, first->snapshots[0].buffer->Handle(), downloaded->Handle(), 1, &region);
        region.srcOffset = second->snapshots[0].offset;
        region.dstOffset = elementBytes;
        copy(commands, second->snapshots[0].buffer->Handle(), downloaded->Handle(), 1, &region);
        RecordMemoryBarrier(snapshotContext, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        snapshotLifetime = first;
        first.reset();
        second.reset();
        Require(!snapshotLifetime.expired(), "draw snapshot was released before its GPU batch");
        snapshotRecorder.Sync();
        const auto contents = downloaded->Bytes();
        Require(std::all_of(contents.begin(), contents.begin() + elementBytes, [](std::byte value) { return value == std::byte{0x11}; }), "first draw observed overwritten input");
        Require(std::all_of(contents.begin() + elementBytes, contents.end(), [](std::byte value) { return value == std::byte{0x22}; }), "second draw observed overwritten input");
        snapshotRecorder.NotePendingWrite(element, elementBytes);
        Require(resources.PrepareDrawBindings(snapshotRecorder) == nullptr, "GPU-produced input was replaced with stale CPU memory");
        snapshotRecorder.Sync();
    }
    Require(snapshotLifetime.expired(), "draw snapshots outlived their recorder");
    recorder.Activate();
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    // The import is retired by the next reconcile; the block itself is left to the process.
    HostImportFor(context, address, bytes);
}

bool bdaStagingTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0 || !context.bufferDeviceAddress || !AddressSpaceCounters().enabled || std::getenv("APS5_NO_BDA_STAGING") != nullptr) {
        std::cout << "skipped, host imports are unavailable (no device addresses or address space cache)\n";
        return false;
    }
    constexpr std::size_t bytes = 4u << 20u;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the test block");
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr || import->address == 0) {
        std::cout << "skipped, host imports are unavailable: the test block's import was refused\n";
        return false;
    }
    const auto importBase = import->base;
    const auto importAddress = import->address;
    const auto importBuffer = import->buffer;
    auto* words = static_cast<std::uint32_t*>(block);
    const auto fill = context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer");
    const auto element = address + 8192;
    constexpr std::size_t elementBytes = 64u << 10u;
    constexpr std::size_t first = 8192 / 4;
    const auto checkTable = [&](const std::vector<ShaderRecompiler::BdaAbi::Range>& ranges, bool staged) {
        for (std::size_t index = 1; index < ranges.size(); ++index) Require(ranges[index - 1].end <= ranges[index].begin, "the BDA table of a staged build has overlapping or unsorted ranges");
        bool covered = false;
        std::uint64_t reached = address;
        for (const auto& range : ranges) {
            if (range.end <= address || range.begin >= address + bytes) continue;
            Require(range.begin == reached, "the BDA table leaves a hole in the registered block");
            reached = range.end;
            const bool inImport = range.deviceAddress == importAddress + (range.begin - importBase);
            const bool overElement = range.begin < element + elementBytes && element < range.end;
            covered = covered || overElement;
            if (overElement && staged) {
                Require(range.begin == element && range.end == element + elementBytes && !inImport, "the staged element's BDA range does not send its addresses to the shadow");
                Require((range.permissions & ShaderRecompiler::BdaAbi::Write) != 0u, "the staged element's BDA range is not writable");
            } else {
                Require(inImport, "a BDA range outside the staged element left the import");
            }
        }
        Require(covered, "no BDA range covers the written element");
        Require(reached == address + bytes, "the BDA table does not reach the end of the registered block");
    };
    {
        GuestBufferMemory memory(context);
        memory.AllowDeviceStaging();
        memory.AcquireRegistered();
        memory.AddWritable(element, elementBytes, false, false);
        memory.AddReadable(element + 16, 64);
        memory.AddReadable(address + (1u << 20u), 64);
        memory.Upload(true);
        checkTable(memory.AddressRanges(), true);
        const auto cached = memory.CachedAddressTable();
        Require(cached.has_value() && cached->ranges != nullptr && cached->ranges->size() == memory.AddressRanges().size(), "the cached BDA table of a staged build differs from its ranges");
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(element, elementBytes, adjustment);
        Require(view.buffer != importBuffer, "the written element of an address-based build is bound in place");
        std::uint32_t innerAdjustment = 0;
        Require(memory.Descriptor(element + 16, 64, innerAdjustment).buffer == view.buffer, "a read inside the staged element does not read its shadow");
        recorder.Sync();
        auto commands = recorder.Commands();
        fill(commands, view.buffer, view.offset + adjustment + 64u * 4u, 64, 0x22222222u);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
        memory.RecordCopyBacks(recorder);
        recorder.Sync();
        memory.MarkDirectWrites();
        std::size_t wrong = 0;
        for (std::size_t index = 0; index < bytes / 4u; ++index) {
            const auto at = index >= first ? index - first : bytes;
            const auto expected = at >= 64 && at < 80 ? 0x22222222u : 0x11111111u;
            if (words[index] != expected && wrong++ < 8) std::cout << "address-based staging copy-back dword " << index << " holds 0x" << std::hex << words[index] << ", expected 0x" << expected << std::dec << "\n";
        }
        Require(wrong == 0, "an address-based staged copy-back left the wrong guest bytes");
        memory.WriteBack();
    }
    std::memset(block, 0x11, bytes);
    {
        GuestBufferMemory memory(context);
        memory.AllowDeviceStaging();
        memory.AcquireRegistered();
        memory.AddReadable(element - 16, 64);
        memory.AddWritable(element, elementBytes, false, false);
        memory.Upload(true);
        checkTable(memory.AddressRanges(), false);
        std::uint32_t adjustment = 0;
        Require(memory.Descriptor(element, elementBytes, adjustment).buffer == importBuffer, "an element crossed by an in-place read was staged");
        memory.RecordCopyBacks(recorder);
        recorder.Sync();
        memory.WriteBack();
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, bytes);
    return true;
}

bool readWrittenStagingTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: read and written staging not tested\n";
        return false;
    }
    constexpr std::size_t bytes = 40u << 20u;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the test block");
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the test block refused: read and written staging not tested\n";
        return false;
    }
    const auto boundInPlace = [&](std::uint64_t element, std::size_t elementBytes, bool read) {
        GuestBufferMemory memory(context);
        memory.AllowDeviceStaging();
        memory.AddWritable(element, elementBytes, false, read);
        memory.Upload(false);
        const auto reads = memory.InPlaceReads();
        const bool direct = reads.size() == 1 && reads[0].first == element && reads[0].second == element + elementBytes;
        Require(direct || reads.empty(), "an element was bound neither in place nor staged");
        memory.RecordCopyBacks(recorder);
        recorder.Sync();
        return direct;
    };
    Require(!boundInPlace(address + 4096, 1u << 20u, false), "a written 1 MiB element was not staged");
    Require(boundInPlace(address + 4096, 4u << 20u, false) != context.discreteGpu, context.discreteGpu ? "a written-only 4 MiB element was bound in place on a discrete GPU" : "a written-only 4 MiB element was staged past the written window");
    Require(boundInPlace(address + 4096, 12u << 20u, false), "a written-only 12 MiB element was staged past the discrete written window");
    Require(!boundInPlace(address + 4096, 4u << 20u, true), "a read and written 4 MiB element was bound in place");
    const auto buildInPlace = [&](std::uint64_t dispatchThreads, bool read, std::uint32_t stride, std::uint32_t extent, std::uint32_t elementBytes = 12u << 20u) {
        const auto element = address + 4096;
        const std::uint32_t records = stride != 0 ? elementBytes / stride : elementBytes;
        ShaderRecompiler::RecompileResult program;
        ShaderRecompiler::DescriptorBinding binding;
        binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
        binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
        binding.descriptorSet = 0;
        binding.binding = 0;
        binding.count = 1;
        binding.guestDescriptor = {static_cast<std::uint32_t>(element), (static_cast<std::uint32_t>(element >> 32u) & 0xffffu) | (stride << 16u), records, 0x31000000u};
        binding.bufferWritten = {true};
        binding.bufferRead = {read};
        binding.bufferExtent = {extent};
        program.bindings.push_back(binding);
        const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
        ShaderResources resources(context, compute, {}, false, dispatchThreads);
        const auto reads = resources.InPlaceReads();
        const bool direct = std::any_of(reads.begin(), reads.end(), [&](const auto& range) { return range.first <= element && range.second >= element + elementBytes; });
        resources.MarkGpuWrites(recorder);
        recorder.Sync();
        return direct;
    };
    Require(!buildInPlace((12u << 20u) / 16u, true, 16, 0), "a dispatch sweeping a read and written 12 MiB element bound it in place");
    Require(buildInPlace(1024, true, 16, 0), "a dispatch touching 16 KiB of a read and written 12 MiB element staged all of it");
    Require(buildInPlace(0, true, 16, 0), "a dispatch of unknown size staged a read and written 12 MiB element");
    Require(buildInPlace((12u << 20u) / 32u, false, 32, 0) != context.discreteGpu, context.discreteGpu ? "a dispatch sweeping a written-only 12 MiB element of 32-byte records bound it in place on a discrete GPU" : "a dispatch sweeping a written-only 12 MiB element of 32-byte records staged it on a GPU without separate VRAM");
    Require(buildInPlace((12u << 20u) / 16u, false, 16, 0), "a dispatch sweeping a written-only 12 MiB element of 16-byte records, which its stores write contiguously, staged it");
    Require(buildInPlace(1024, false, 32, 0), "a dispatch touching 32 KiB of a written-only 12 MiB element staged all of it");
    Require(buildInPlace((12u << 20u) / 32u, false, 0, 32), "a dispatch writing a written-only 12 MiB raw element staged it");
    Require(buildInPlace(204800, false, 160, 0, 204800u * 160u) != context.discreteGpu, context.discreteGpu ? "a dispatch sweeping a written-only 31.25 MiB element of 160-byte records bound it in place on a discrete GPU" : "a dispatch sweeping a written-only 31.25 MiB element of 160-byte records staged it on a GPU without separate VRAM");
    Require(buildInPlace(204800, true, 160, 0, 204800u * 160u) != context.discreteGpu, context.discreteGpu ? "a dispatch sweeping a read and written 31.25 MiB element bound it in place on a discrete GPU" : "a dispatch sweeping a read and written 31.25 MiB element staged it past the 16 MiB cap on a GPU without separate VRAM");
    Require(!buildInPlace((12u << 20u) / 32u, true, 0, 32), "a dispatch sweeping a read and written 12 MiB raw element bound it in place");
    Require(buildInPlace(1024, true, 0, 32), "a dispatch touching 32 KiB of a read and written 12 MiB raw element staged all of it");
    Require(buildInPlace((12u << 20u) / 32u, true, 0, 0), "a raw element without an access extent was staged");
    Require(std::all_of(static_cast<const std::byte*>(block), static_cast<const std::byte*>(block) + bytes, [](std::byte value) { return value == std::byte{0x11}; }), "a staged copy-back changed the guest bytes");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, bytes);
    return true;
}

bool sparseRunImportTests(const Device& device, bool copied) {
    const auto& context = device.GetContext();
    constexpr std::size_t unit = 65536;
    constexpr std::size_t heap = 8 * unit;
    struct Heap {
        const Context& context;
        void* area = nullptr;
        std::uint8_t* guest = nullptr;
        std::size_t mapped = 0;
        ~Heap() {
            ClearHostImports(context.device);
            {
                GuestAllocations::Mutation mutation;
                for (std::size_t index = 0; index < mapped; ++index) mutation.Remove(guest + index * unit);
            }
            SetHostImportWindow(0);
#ifdef _WIN32
            if (area != nullptr) VirtualFree(area, 0, MEM_RELEASE);
#else
            if (area != nullptr) munmap(area, heap + unit);
#endif
        }
    } state{context};
#ifdef _WIN32
    state.area = VirtualAlloc(nullptr, heap, MEM_RESERVE, PAGE_NOACCESS);
    Require(state.area != nullptr, "cannot reserve the sparse run heap");
    state.guest = static_cast<std::uint8_t*>(state.area);
#else
    void* area = mmap(nullptr, heap + unit, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(area != MAP_FAILED, "cannot reserve the sparse run heap");
    state.area = area;
    state.guest = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(area) + unit - 1) & ~std::uintptr_t{unit - 1});
#endif
    auto* guest = state.guest;
    const auto address = reinterpret_cast<std::uint64_t>(guest);
    const auto map = [&] {
        auto* piece = guest + state.mapped * unit;
#ifdef _WIN32
        Require(VirtualAlloc(piece, unit, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot map a piece of the sparse run heap");
#else
        Require(mprotect(piece, unit, PROT_READ | PROT_WRITE) == 0, "cannot map a piece of the sparse run heap");
#endif
        for (std::size_t at = 0; at < unit; ++at) piece[at] = static_cast<std::uint8_t>((state.mapped * unit + at) * 7u + 3u);
        GuestAllocations::Mutation mutation;
        mutation.Add(piece, unit, true, true, true);
        ++state.mapped;
    };
    for (int piece = 0; piece < 3; ++piece) map();
    PrepareImportWatch(context);
    SetHostImportWindow(unit);
    if (HostImportFor(context, address, unit) == nullptr) {
        std::cout << "host import of the sparse run heap refused: sparse run imports not tested\n";
        return false;
    }
    Recorder recorder(context);
    recorder.Activate();
    const auto run = [&] { return HostImportFor(context, address, state.mapped * unit); };
    const auto bound = [&](GuestBufferMemory& memory, std::uint64_t begin, std::uint64_t bytes) {
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(begin, bytes, adjustment);
        const auto* import = run();
        return import != nullptr && view.buffer == import->buffer;
    };
    const auto upload = [&](bool written, std::uint64_t pastRun, const std::function<void(GuestBufferMemory&)>& check) {
        GuestBufferMemory memory(context);
        if (written) memory.AddWritable(address, heap);
        else memory.AddReadable(address, heap);
        if (pastRun != 0) memory.AddReadable(address + pastRun, 16);
        memory.Upload(true);
        check(memory);
        memory.WriteBack();
    };
    const auto committed = 3 * unit;
    upload(false, 0, [&](GuestBufferMemory& memory) {
        Require(!bound(memory, address, heap) && memory.InPlaceReads().empty(), "a sparse run heap was joined the first time its run was seen");
    });
    if (copied) {
        upload(false, 5 * unit, [&](GuestBufferMemory& memory) {
            std::uint32_t adjustment = 0;
            const auto view = memory.Descriptor(address, heap, adjustment);
            Require(!bound(memory, address, heap) && memory.InPlaceReads().empty(), "a sparse run heap with a view past its stable run is bound in place");
            Buffer readback(context, committed, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            {
                std::unique_lock gpu(GpuMutex());
                const auto commands = recorder.Commands();
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                CopyBuffer(context, commands, view.buffer, view.offset, readback.Handle(), 0, committed);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
                recorder.Sync();
            }
            readback.Invalidate();
            Require(std::memcmp(readback.Bytes().data(), guest, committed) == 0, "the GPU does not read a copied sparse run heap's mapped bytes");
        });
        recorder.Sync();
        std::cout << "sparse run import tests passed (copied)\n";
        return true;
    }
    upload(false, 0, [&](GuestBufferMemory& memory) {
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(address, heap, adjustment);
        const auto ranges = memory.AddressRanges();
        Require(adjustment == 0 && view.range == committed && ranges.size() == 1 && ranges[0].begin == address && ranges[0].end == address + committed, "a sparse run heap's view or address range is not cut at its last committed byte");
        const auto* import = run();
        Require(import != nullptr && view.buffer == import->buffer && view.offset == address - import->base && ranges[0].deviceAddress == import->address + (address - import->base), "a sparse run heap is not bound in place from its run's import");
        const auto direct = memory.DirectRegions();
        const auto reads = memory.InPlaceReads();
        const auto cuts = memory.CutRegions();
        Require(direct.has_value() && direct->size() == 1 && direct->front() == std::pair<std::uint64_t, std::uint64_t>{address, address + committed} && reads.size() == 1 && reads.front() == direct->front(), "a sparse run heap bound in place is not named up to its import's end for reuse");
        Require(cuts.size() == 1 && cuts.front().begin == address && cuts.front().end == address + heap && cuts.front().committedEnd == address + committed, "a sparse run heap bound in place does not report its cut");
        Buffer readback(context, committed, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        {
            std::unique_lock gpu(GpuMutex());
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, view.buffer, view.offset, readback.Handle(), 0, committed);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Sync();
        }
        readback.Invalidate();
        Require(std::memcmp(readback.Bytes().data(), guest, committed) == 0, "the GPU does not read a sparse run heap's bytes through its in-place view");
    });
    upload(true, 0, [&](GuestBufferMemory& memory) {
        Require(!bound(memory, address, heap), "a written sparse run heap is bound in place");
    });
    upload(false, 5 * unit, [&](GuestBufferMemory& memory) {
        Require(!bound(memory, address, heap), "a sparse run heap with a view past its run is bound in place");
    });
    {
        auto regionContext = context;
        DescriptorCache cache(regionContext);
        regionContext.descriptorCache = &cache;
        Recorder regionRecorder(regionContext);
        regionRecorder.Activate();
        ShaderRecompiler::RecompileResult program;
        ShaderRecompiler::DescriptorBinding binding;
        binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
        binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
        binding.descriptorSet = 0;
        binding.binding = 0;
        binding.count = 1;
        binding.guestDescriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, static_cast<std::uint32_t>(heap), 0x31000000u};
        binding.bufferWritten = {false};
        program.bindings.push_back(binding);
        program.pushConstants.resize(16);
        program.memoryOffsetDword = 0;
        const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
        {
            ShaderResources resources(regionContext, compute);
            const auto reads = resources.InPlaceReads();
            Require(resources.Reusable() && reads.size() == 1 && reads.front() == std::pair<std::uint64_t, std::uint64_t>{address, address + committed}, "a set over a sparse run heap bound in place is not reusable");
            Require(resources.Revalidate(compute), "a set over an unchanged sparse run heap failed its proof");
            map();
            Require(!resources.Revalidate(compute), "a set over a sparse run heap mapped past its cut was reused");
            regionRecorder.Submit();
            device.WaitQueue();
            regionRecorder.Sync();
        }
        recorder.Activate();
        upload(false, 0, [&](GuestBufferMemory& memory) {
            Require(!bound(memory, address, heap), "a grown sparse run heap was joined again the first time its grown run was seen");
        });
        upload(false, 0, [&](GuestBufferMemory& memory) {
            std::uint32_t adjustment = 0;
            const auto view = memory.Descriptor(address, heap, adjustment);
            Require(bound(memory, address, heap) && view.range == 4 * unit, "a grown sparse run heap is not joined again over its grown run");
        });
    }
    recorder.Sync();
    std::cout << "sparse run import tests passed (in place)\n";
    return true;
}

void misalignedRegionTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    const auto alignment = context.limits.minStorageBufferOffsetAlignment;
    constexpr std::size_t bytes = 65536;
    if (alignment < 8 || alignment > 256) {
        std::cout << "storage buffer offset alignment " << alignment << ": misaligned draw regions not tested\n";
        return;
    }
    void* block = context.hostImportAlignment != 0 ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "host imports or write watching unavailable: misaligned draw regions not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    auto* guest = static_cast<std::uint8_t*>(block);
    for (std::size_t at = 0; at < bytes; ++at) guest[at] = static_cast<std::uint8_t>(at * 5u + 1u);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr) {
        std::cout << "host import of the watched block refused: misaligned draw regions not tested\n";
        return;
    }
    constexpr std::uint32_t offset = 4096 + 4;
    constexpr std::size_t elementBytes = 64;
    const auto view = address + offset;
    Require((view - import->base) % alignment != 0 && (view - import->base) % 4 == 0, "the test view is not a DWORD-aligned view off the storage buffer offset alignment");
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {static_cast<std::uint32_t>(view), static_cast<std::uint32_t>(view >> 32u) & 0xffffu, static_cast<std::uint32_t>(elementBytes), 0x31000000u};
    binding.bufferWritten = {false};
    vertex.bindings.push_back(binding);
    vertex.pushConstants.resize(16);
    vertex.memoryOffsetDword = 0;
    const ShaderRecompiler::RecompileResult fragment;
    {
        auto regionContext = context;
        DescriptorCache cache(regionContext);
        regionContext.descriptorCache = &cache;
        Recorder regionRecorder(regionContext);
        regionRecorder.Activate();
        const ColorTarget target{};
        ShaderResources resources(regionContext, vertex, fragment, target, 0, 0);
        Require(resources.Reusable(), "a misaligned read-only draw region bound in place is not reusable");
        const auto reads = resources.InPlaceReads();
        Require(reads.size() == 1 && reads.front().first == view && reads.front().second == view + elementBytes, "the misaligned draw region is not read in place");
        std::array<std::byte, PipelinePushConstantBytes> push{};
        resources.PatchPushConstants(push);
        const auto adjustment = static_cast<std::uint32_t>(push[0]);
        Require(adjustment == (view - import->base) % alignment, "the misaligned region's push constant offset is not its distance from the aligned binding offset");
        const auto check = [&] {
            const auto bindings = resources.PrepareDrawBindings(regionRecorder);
            Require(bindings != nullptr && bindings->snapshots.size() == 1, "the read-only misaligned draw input was not snapshotted");
            const auto contents = bindings->snapshots.front().Bytes();
            Require(bindings->snapshots.front().address == view - adjustment && contents.size() == adjustment + elementBytes, "the draw snapshot does not start at the aligned offset below the view");
            Require(std::memcmp(contents.data() + adjustment, guest + offset, elementBytes) == 0, "the shader's patched offset into the draw snapshot misses the view's bytes");
        };
        check();
        guest[offset + 8] ^= 0xffu;
        AgcDriver::GuestMemory::MarkWritten(view, elementBytes);
        check();
        regionRecorder.Sync();
    }
    recorder.Activate();
}

void misalignedSnapshotTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    const auto alignment = context.limits.minStorageBufferOffsetAlignment;
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: misaligned draw snapshots not tested\n";
        return;
    }
    if (alignment < 8 || alignment > 256) {
        std::cout << "storage buffer offset alignment " << alignment << ": misaligned draw snapshots not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the misaligned snapshot block");
    auto* guest = static_cast<std::uint8_t*>(block);
    for (std::size_t at = 0; at < bytes; ++at) guest[at] = static_cast<std::uint8_t>(at * 7u + 3u);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the test block refused: misaligned draw snapshots not tested\n";
        return;
    }
    constexpr std::uint32_t outer = 4096;
    constexpr std::uint32_t offset = outer + 4;
    constexpr std::size_t outerBytes = 128;
    constexpr std::size_t elementBytes = 64;
    const auto words = [](std::uint64_t at, std::size_t size) { return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(at), static_cast<std::uint32_t>(at >> 32u) & 0xffffu, static_cast<std::uint32_t>(size), 0x31000000u}; };
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 2;
    for (const auto word : words(address + outer, outerBytes)) binding.guestDescriptor.push_back(word);
    for (const auto word : words(address + offset, elementBytes)) binding.guestDescriptor.push_back(word);
    binding.bufferWritten = {false, false};
    program.bindings.push_back(binding);
    program.pushConstants.resize(16);
    program.memoryOffsetDword = 0;
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    {
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, compute);
        std::array<std::byte, PipelinePushConstantBytes> push{};
        resources.PatchPushConstants(push);
        const auto adjustment = std::to_integer<std::uint32_t>(push[1]);
        Require(push[0] == std::byte{0} && adjustment == offset % alignment, "the inner view's shader offset is not its distance from the binding");
        const auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
        Require(bindings != nullptr && bindings->snapshots.size() == 2, "read-only draw inputs were not snapshotted");
        const auto outerContents = bindings->snapshots[0].Bytes();
        Require(outerContents.size() >= outerBytes && std::memcmp(outerContents.data(), guest + outer, outerBytes) == 0, "an aligned draw snapshot misses its view's bytes");
        const auto contents = bindings->snapshots[1].Bytes();
        Require(contents.size() >= adjustment + elementBytes, "a draw snapshot ends before the bytes the shader reads");
        Require(std::memcmp(contents.data() + adjustment, guest + offset, elementBytes) == 0, "the shader's offset into a draw snapshot misses the view's bytes");
        snapshotRecorder.Sync();
    }
    recorder.Activate();
}

class DescriptorUpdates {
public:
    struct Contents {
        std::map<std::pair<std::uint32_t, std::uint32_t>, VkDescriptorBufferInfo> buffers;
        std::map<std::pair<std::uint32_t, std::uint32_t>, VkDescriptorImageInfo> images;
        std::map<std::pair<VkDescriptorSet, std::uint32_t>, std::uint32_t> written;
    };
    explicit DescriptorUpdates(Context& context) : context(context), previous(context.functions),
        update(context.Resolved(&DeviceFunctions::updateDescriptorSets, "vkUpdateDescriptorSets")) {
        Require(active == nullptr, "nested descriptor update capture");
        if (previous != nullptr) functions = *previous;
        functions.updateDescriptorSets = captureUpdate;
        context.functions = &functions;
        active = this;
    }
    ~DescriptorUpdates() { context.functions = previous; active = nullptr; }
    std::vector<Contents> calls;
    std::uint32_t copies = 0;

private:
    static VKAPI_ATTR void VKAPI_CALL captureUpdate(VkDevice device, std::uint32_t count, const VkWriteDescriptorSet* writes, std::uint32_t copyCount, const VkCopyDescriptorSet* copies) {
        Contents contents;
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& write = writes[i];
            contents.written[{write.dstSet, write.dstBinding}] += write.descriptorCount;
            for (std::uint32_t element = 0; element < write.descriptorCount; ++element) {
                const auto key = std::pair{write.dstBinding, write.dstArrayElement + element};
                if (write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) contents.buffers[key] = write.pBufferInfo[element];
                else contents.images[key] = write.pImageInfo[element];
            }
        }
        active->calls.push_back(std::move(contents));
        active->copies += copyCount;
        active->update(device, count, writes, copyCount, copies);
    }
    Context& context;
    const DeviceFunctions* previous;
    DeviceFunctions functions{};
    PFN_vkUpdateDescriptorSets update;
    static inline DescriptorUpdates* active = nullptr;
};

void drawDescriptorWriteTests(const Device& device, Recorder& recorder) {
    auto context = device.GetContext();
    DescriptorUpdates updates(context);
    {
        DescriptorCache cache(context);
        context.descriptorCache = &cache;
        Recorder snapshots(context);
        snapshots.Activate();
        ShaderRecompiler::RecompileResult program;
        ShaderRecompiler::DescriptorBinding data{};
        data.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
        data.role = ShaderRecompiler::DescriptorRole::FlattenedSrt;
        data.binding = 2;
        data.count = 1;
        data.guestDescriptor = {1, 2, 3, 4};
        program.bindings.push_back(data);
        data.binding = 7;
        program.bindings.push_back(data);
        ShaderRecompiler::DescriptorBinding sampler{};
        sampler.kind = ShaderRecompiler::DescriptorKind::Sampler;
        sampler.role = ShaderRecompiler::DescriptorRole::GuestSamplers;
        sampler.binding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Samplers);
        sampler.count = 2;
        sampler.guestDescriptor.resize(8);
        sampler.samplerDepthCompare = {false, false};
        program.bindings.push_back(sampler);
        const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
        ShaderResources resources(context, shader);
        Require(updates.calls.size() == 1, "building resources did not write one descriptor set");
        const auto original = updates.calls.front();
        Require(original.buffers.size() == 2 && original.images.size() == 2, "descriptor test lacks mixed bindings and an image array");
        for (std::size_t allocation = 0; allocation < 2; ++allocation) {
            const std::array<std::uint32_t, 4> words{5, 6, 7, 8};
            const ShaderResources::MovedBuffer moved{allocation, 0, 16, words};
            const auto draw = resources.PrepareDrawBindings(snapshots, std::span(&moved, 1));
            Require(draw != nullptr && draw->snapshots.size() == 1, "moved shader data did not create a draw snapshot");
            Require(updates.calls.size() == allocation + 2 && updates.copies == 0, "draw descriptors copied GPU memory or needed multiple updates");
            const auto& actual = updates.calls.back();
            Require(actual.buffers.size() == original.buffers.size() && actual.images.size() == original.images.size(), "snapshot update omitted unchanged descriptors");
            for (const auto& [key, before] : original.buffers) {
                const auto after = actual.buffers.at(key);
                const auto expected = key.first == (allocation == 0 ? 2u : 7u) ? draw->snapshots.front().buffer->Handle() : before.buffer;
                Require(after.buffer == expected && after.offset == before.offset && after.range == before.range, "snapshot update replaced the wrong buffer or changed its range");
            }
            for (const auto& [key, before] : original.images) {
                const auto after = actual.images.at(key);
                Require(after.sampler == before.sampler && after.imageView == before.imageView && after.imageLayout == before.imageLayout, "snapshot update changed an image descriptor");
            }
        }
        snapshots.Sync();
    }
    recorder.Activate();
}

void drawSnapshotTableSetTests(const Device& device, Recorder& recorder) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    using ShaderRecompiler::DescriptorKind;
    using ShaderRecompiler::DescriptorRole;
    const auto& base = device.GetContext();
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    if (context.imageTableCapacity < 2u) {
        std::cout << "image tables unavailable: draw snapshot table sets not tested\n";
        return;
    }
    constexpr std::uint32_t width = 4;
    constexpr std::uint32_t height = 4;
    alignas(256) static std::array<float, width * height * 16u> texels{};
    texels.fill(1.0f);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels.data()));
    const std::array<std::uint32_t, 8> texture{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (22u << 20u) | (((width - 1u) & 3u) << 30u), ((width - 1u) >> 2u) | ((height - 1u) << 14u), 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
    Require(DescribeSurface(DecodeTextureResource(texture)).guestBytes <= sizeof(texels), "the draw snapshot table set test T# reads past its texels");
    constexpr auto bindingCount = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Count);
    const auto addTable = [&](ShaderRecompiler::RecompileResult& program, ShaderRecompiler::RuntimeAbi::Stage stage, std::uint32_t set, std::uint32_t elements) {
        ShaderRecompiler::DescriptorBinding map{};
        map.kind = DescriptorKind::StorageBuffer;
        map.role = DescriptorRole::ImageTableMap;
        map.descriptorSet = set;
        map.binding = static_cast<std::uint32_t>(stage) * bindingCount + static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap);
        map.count = 1;
        map.guestDescriptor.assign(Abi::HeaderWords + elements, 0u);
        map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
        map.guestDescriptor[Abi::HeaderElementCount] = elements;
        map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
        map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords + elements;
        program.bindings.push_back(map);
        ShaderRecompiler::DescriptorBinding images{};
        images.kind = DescriptorKind::SampledImage;
        images.role = DescriptorRole::ImageTable;
        images.descriptorSet = set;
        images.binding = ShaderRecompiler::RuntimeAbi::ImageTableBinding;
        images.count = elements;
        for (std::uint32_t element = 0; element < elements; ++element) images.guestDescriptor.insert(images.guestDescriptor.end(), texture.begin(), texture.end());
        images.imageShapes.assign(elements, ShaderRecompiler::DescriptorImageShape::Image2D);
        images.imageDepthCompare.assign(elements, false);
        images.imageSamplers.assign(elements, 0u);
        program.bindings.push_back(std::move(images));
    };
    DescriptorUpdates updates(context);
    {
        DescriptorCache cache(context);
        context.descriptorCache = &cache;
        Recorder snapshots(context);
        snapshots.Activate();
        ShaderRecompiler::RecompileResult vertex, fragment;
        ShaderRecompiler::DescriptorBinding data{};
        data.kind = DescriptorKind::StorageBuffer;
        data.role = DescriptorRole::FlattenedSrt;
        data.binding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::FlattenedSrt);
        data.count = 1;
        data.guestDescriptor = {1, 2, 3, 4};
        vertex.bindings.push_back(data);
        addTable(vertex, ShaderRecompiler::RuntimeAbi::Stage::Main, 0u, 1u);
        addTable(fragment, ShaderRecompiler::RuntimeAbi::Stage::Fragment, ShaderRecompiler::RuntimeAbi::FragmentDescriptorSet, 2u);
        const ColorTarget target{};
        ShaderResources resources(context, vertex, fragment, target, 0, 0);
        Require(resources.SplitSets(), "a fragment image table did not get its own descriptor set");
        const std::array<std::uint32_t, 4> movedWords{5, 6, 7, 8};
        const ShaderResources::MovedBuffer moved{0, 0, 16, movedWords};
        const auto draw = resources.PrepareDrawBindings(snapshots, std::span(&moved, 1));
        Require(draw != nullptr && draw->snapshots.size() == 1, "moved shader data did not create a draw snapshot");
        const auto& written = updates.calls.back().written;
        const auto tableWrites = [&](VkDescriptorSet set) {
            const auto it = written.find({set, ShaderRecompiler::RuntimeAbi::ImageTableBinding});
            return it == written.end() ? 0u : it->second;
        };
        Require(tableWrites(draw->allocation.set) == 1u, "the draw snapshot wrote the wrong number of image table descriptors into the vertex set");
        Require(tableWrites(draw->fragmentAllocation.set) == 2u, "the draw snapshot wrote the wrong number of image table descriptors into the fragment set");
        snapshots.Sync();
    }
    ClearCachedTextures(context.device);
    recorder.Activate();
}

void imageTableFirstLayerRevalidateTests(const Device& device, Recorder& recorder) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    using ShaderRecompiler::DescriptorKind;
    using ShaderRecompiler::DescriptorRole;
    const auto& base = device.GetContext();
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    if (context.imageTableCapacity < 1u) {
        std::cout << "image tables unavailable: image table first-layer revalidation not tested\n";
        return;
    }
    constexpr std::uint32_t width = 4;
    constexpr std::uint32_t height = 4;
    constexpr std::uint32_t lastSlice = 3;
    alignas(256) static std::array<float, width * height * (lastSlice + 1u) * 16u> texels{};
    texels.fill(1.0f);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels.data()));
    const std::array<std::uint32_t, 8> texture{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (22u << 20u) | (((width - 1u) & 3u) << 30u), ((width - 1u) >> 2u) | ((height - 1u) << 14u), 0xfacu | (13u << 28u), lastSlice, 0u, 0u, 0u};
    const auto whole = DecodeTextureResource(texture);
    Require(whole.dimension == TextureDimension::k2DArray && whole.depthOrLastArray == lastSlice && DescribeSurface(whole).guestBytes <= sizeof(texels), "the image table test T# is not a 2D array over its texels");
    constexpr auto bindingCount = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Count);
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding map{};
    map.kind = DescriptorKind::StorageBuffer;
    map.role = DescriptorRole::ImageTableMap;
    map.binding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Stage::Main) * bindingCount + static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap);
    map.count = 1;
    map.guestDescriptor.assign(Abi::HeaderWords + 1u, 0u);
    map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
    map.guestDescriptor[Abi::HeaderElementCount] = 1u;
    map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
    map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords + 1u;
    program.bindings.push_back(map);
    ShaderRecompiler::DescriptorBinding images{};
    images.kind = DescriptorKind::SampledImage;
    images.role = DescriptorRole::ImageTable;
    images.binding = ShaderRecompiler::RuntimeAbi::ImageTableBinding;
    images.count = 1;
    images.guestDescriptor.assign(texture.begin(), texture.end());
    images.imageShapes.assign(1, ShaderRecompiler::DescriptorImageShape::Image2D);
    images.imageDepthCompare.assign(1, false);
    images.imageSamplers.assign(1, 0u);
    program.bindings.push_back(std::move(images));
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    {
        ShaderResources resources(context, compute);
        Require(resources.SampledTextures().size() == 1, "a 2D read of a 2D array in an image table bound no texture");
        const auto bound = resources.SampledTextures().front();
        ShaderResources::ProofReport report;
        Require(resources.Revalidate(std::span(&compute, 1), &report), "an image table holding a 2D read of a 2D array did not revalidate");
        Require(report.path == ShaderResources::ProofPath::Full, "an image table holding a 2D read of a 2D array was not proven on the full path");
        Require(resources.SampledTextures().front() == bound, "revalidating an image table holding a 2D read of a 2D array changed its texture");
        Require(resources.Revalidate(std::span(&compute, 1)), "an image table holding a 2D read of a 2D array did not revalidate twice");
        recorder.Sync();
    }
    ClearCachedTextures(context.device);
}

void drawSnapshotReuseTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    constexpr std::size_t bytes = 65536;
    void* block = context.hostImportAlignment != 0 ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "host imports or write watching unavailable: draw snapshot reuse not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the watched block refused: draw snapshot reuse not tested\n";
        return;
    }
    if (!AgcDriver::GuestMemory::Watched(address, bytes)) {
        std::cout << "host imports are compared, not watched: draw snapshot reuse not tested\n";
        return;
    }
    const auto element = address + 4096;
    constexpr std::size_t elementBytes = 8192;
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {static_cast<std::uint32_t>(element), static_cast<std::uint32_t>(element >> 32u) & 0xffffu, elementBytes, 0x31000000u};
    binding.bufferWritten = {false};
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    {
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, compute);
        const auto snapshot = [&](std::byte expected) {
            const auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
            Require(bindings != nullptr && bindings->snapshots.size() == 1, "read-only draw input was not snapshotted");
            const auto buffer = bindings->snapshots[0].buffer;
            const auto contents = buffer->Bytes();
            Require(contents.size() == elementBytes && std::all_of(contents.begin(), contents.end(), [&](std::byte value) { return value == expected; }), "a draw snapshot does not hold the guest bytes of its draw");
            return buffer;
        };
        const auto first = snapshot(std::byte{0x11});
        Require(snapshot(std::byte{0x11}) == first, "an unchanged draw input was copied again");
        std::memset(reinterpret_cast<void*>(element), 0x22, elementBytes);
        const auto afterCpu = snapshot(std::byte{0x22});
        Require(afterCpu != first, "a draw snapshot outlived a CPU store to its range");
        Require(snapshot(std::byte{0x22}) == afterCpu, "the recopied draw input was not kept");
        std::memset(reinterpret_cast<void*>(element), 0x33, elementBytes);
        AgcDriver::GuestMemory::MarkWritten(element, 4);
        const auto afterStore = snapshot(std::byte{0x33});
        Require(afterStore != afterCpu, "a draw snapshot outlived a driver store to its range");
        std::memset(reinterpret_cast<void*>(element), 0x33, elementBytes);
        Require(snapshot(std::byte{0x33}) == afterStore, "a CPU store of the same bytes made the draw input copy again");
        AgcDriver::GuestMemory::MarkWritten(element, 4);
        Require(snapshot(std::byte{0x33}) == afterStore, "a driver store of the same bytes made the draw input copy again");
        reinterpret_cast<std::uint8_t*>(element)[elementBytes - 1] = 0x34;
        const auto tail = resources.PrepareDrawBindings(snapshotRecorder);
        Require(tail != nullptr && tail->snapshots.size() == 1 && tail->snapshots[0].buffer != afterStore && tail->snapshots[0].buffer->Bytes()[elementBytes - 1] == std::byte{0x34}, "a draw snapshot outlived a store that changed its last byte");
        std::memset(reinterpret_cast<void*>(element), 0x33, elementBytes);
        {
            GuestAllocations::Mutation mutation;
        }
        Require(snapshot(std::byte{0x33}) != afterStore, "a draw snapshot outlived a registry mutation");
        snapshotRecorder.Sync();
    }
    recorder.Activate();
}

void drawSnapshotPatchTests(const Device& device) {
    const auto& context = device.GetContext();
    constexpr std::size_t bytes = 4 * 65536;
    std::unique_lock gpu(GpuMutex());
    void* block = context.hostImportAlignment != 0 ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "host imports or write watching unavailable: snapshot patching not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    auto* guest = static_cast<std::uint8_t*>(block);
    for (std::size_t at = 0; at < bytes; ++at) guest[at] = static_cast<std::uint8_t>(at * 13u + 5u);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the watched block refused: snapshot patching not tested\n";
        return;
    }
    if (!AgcDriver::GuestMemory::Watched(address, bytes)) {
        std::cout << "host imports are compared, not watched: snapshot patching not tested\n";
        return;
    }
    constexpr std::size_t elementOffset = 4096;
    constexpr std::size_t elementBytes = 3 * 65536;
    const auto element = address + elementOffset;
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {static_cast<std::uint32_t>(element), static_cast<std::uint32_t>(element >> 32u) & 0xffffu, static_cast<std::uint32_t>(elementBytes), 0x31000000u};
    binding.bufferWritten = {false};
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    auto snapshotContext = context;
    DescriptorCache cache(snapshotContext);
    snapshotContext.descriptorCache = &cache;
    Recorder snapshotRecorder(snapshotContext);
    snapshotRecorder.Activate();
    {
        ShaderResources resources(snapshotContext, compute);
        const auto snapshot = [&] {
            auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
            Require(bindings != nullptr && bindings->snapshots.size() == 1, "read-only draw input was not snapshotted");
            auto buffer = bindings->snapshots[0].buffer;
            const auto contents = buffer->Bytes();
            Require(contents.size() == elementBytes && std::memcmp(contents.data(), reinterpret_cast<const void*>(element), elementBytes) == 0, "a draw snapshot does not hold the guest bytes of its draw");
            return buffer;
        };
        const auto settle = [&](const std::weak_ptr<Buffer>& buffer) {
            snapshotRecorder.Sync();
            gpu.unlock();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (buffer.use_count() > 1 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            gpu.lock();
            Require(buffer.use_count() == 1, "a finished draw still holds its snapshot");
        };
        std::weak_ptr<Buffer> first = snapshot();
        settle(first);
        guest[elementOffset + 10] ^= 0xffu;
        guest[elementOffset + 65536 + 300] ^= 0xffu;
        guest[elementOffset + 2 * 65536 + 7] = guest[elementOffset + 2 * 65536 + 7];
        guest[elementOffset + elementBytes - 1] ^= 0x5au;
        AgcDriver::GuestMemory::BumpCollectEpoch();
        {
            const auto patched = snapshot();
            Require(patched == first.lock(), "a stale snapshot nothing else holds was copied whole instead of patched");
        }
        settle(first);
        std::memset(guest + elementOffset + 65536, 0x77, 65536);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        {
            const auto patched = snapshot();
            Require(patched == first.lock(), "a snapshot whose whole middle block changed was not patched");
        }
        settle(first);
        const auto held = snapshot();
        Require(held == first.lock(), "an unchanged snapshot was not reused");
        const std::vector<std::byte> before(held->Bytes().begin(), held->Bytes().end());
        guest[elementOffset + 2 * 65536 + 100] ^= 0xffu;
        AgcDriver::GuestMemory::BumpCollectEpoch();
        const auto fresh = snapshot();
        Require(fresh != held, "a snapshot a recorded draw still holds was patched");
        Require(std::memcmp(held->Bytes().data(), before.data(), before.size()) == 0, "a held snapshot's bytes changed");
        snapshotRecorder.Sync();
    }
}

void drawSnapshotEpochTests(const Device& device) {
    const auto& context = device.GetContext();
    constexpr std::size_t bytes = 4 * 65536;
    std::unique_lock gpu(GpuMutex());
    void* block = context.hostImportAlignment != 0 ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "host imports unavailable: draw snapshot reuse within a collect epoch not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    auto* guest = static_cast<std::uint8_t*>(block);
    for (std::size_t at = 0; at < bytes; ++at) guest[at] = static_cast<std::uint8_t>(at * 7u + 3u);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the block refused: draw snapshot reuse within a collect epoch not tested\n";
        return;
    }
    constexpr std::size_t elementOffset = 4096;
    constexpr std::size_t elementBytes = 2 * 65536;
    const auto element = address + elementOffset;
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {static_cast<std::uint32_t>(element), static_cast<std::uint32_t>(element >> 32u) & 0xffffu, static_cast<std::uint32_t>(elementBytes), 0x31000000u};
    binding.bufferWritten = {false};
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    auto snapshotContext = context;
    DescriptorCache cache(snapshotContext);
    snapshotContext.descriptorCache = &cache;
    Recorder snapshotRecorder(snapshotContext);
    snapshotRecorder.Activate();
    {
        ShaderResources resources(snapshotContext, compute);
        const auto snapshot = [&] {
            auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
            Require(bindings != nullptr && bindings->snapshots.size() == 1, "read-only draw input was not snapshotted");
            auto buffer = bindings->snapshots[0].buffer;
            const auto contents = buffer->Bytes();
            Require(contents.size() == elementBytes && std::memcmp(contents.data(), reinterpret_cast<const void*>(element), elementBytes) == 0, "a draw snapshot does not hold the guest bytes of its draw");
            return buffer;
        };
        AgcDriver::GuestMemory::BumpCollectEpoch();
        const auto first = snapshot();
        Require(snapshot() == first, "a second draw in the same collect epoch copied its unchanged input again");
        guest[elementOffset + 65536 + 17] ^= 0xffu;
        AgcDriver::GuestMemory::BumpCollectEpoch();
        const auto changed = snapshot();
        Require(changed != first, "a draw after a store and a new collect epoch reused the snapshot taken before the store");
        Require(snapshot() == changed, "a second draw after the store copied its input again");
        snapshotRecorder.Sync();
    }
    std::cout << "draw snapshot reuse within a collect epoch: ok\n";
}

void drawSnapshotCapTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    const char* previous = std::getenv("APS5_DRAW_SNAPSHOT_MAX_MIB");
    const std::string saved = previous != nullptr ? previous : "";
#ifdef _WIN32
    Require(_putenv_s("APS5_DRAW_SNAPSHOT_MAX_MIB", "1") == 0, "cannot set snapshot cap");
#else
    Require(setenv("APS5_DRAW_SNAPSHOT_MAX_MIB", "1", 1) == 0, "cannot set snapshot cap");
#endif
    struct RestoreCap {
        const char* previous;
        std::string saved;
        ~RestoreCap() {
#ifdef _WIN32
            _putenv_s("APS5_DRAW_SNAPSHOT_MAX_MIB", saved.c_str());
#else
            if (previous != nullptr) {
                setenv("APS5_DRAW_SNAPSHOT_MAX_MIB", saved.c_str(), 1);
            } else {
                unsetenv("APS5_DRAW_SNAPSHOT_MAX_MIB");
            }
#endif
        }
    } restore{previous, saved};
    constexpr std::size_t bytes = 3 * 1024 * 1024;
    void* block = context.hostImportAlignment != 0 ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "host imports or write watching unavailable: draw snapshot cap not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the watched block refused: draw snapshot cap not tested\n";
        return;
    }
    const auto smallAddress = address + 4096;
    constexpr std::size_t smallBytes = 1024 * 1024 - 4096;
    const auto largeAddress = address + 1024 * 1024 + 8192;
    constexpr std::size_t largeBytes = 1024 * 1024 + 4096;
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding smallBinding;
    smallBinding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    smallBinding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    smallBinding.descriptorSet = 0;
    smallBinding.binding = 0;
    smallBinding.count = 1;
    smallBinding.guestDescriptor = {static_cast<std::uint32_t>(smallAddress), static_cast<std::uint32_t>(smallAddress >> 32u) & 0xffffu, static_cast<std::uint32_t>(smallBytes), 0x31000000u};
    smallBinding.bufferWritten = {false};
    program.bindings.push_back(smallBinding);
    ShaderRecompiler::DescriptorBinding largeBinding;
    largeBinding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    largeBinding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    largeBinding.descriptorSet = 0;
    largeBinding.binding = 1;
    largeBinding.count = 1;
    largeBinding.guestDescriptor = {static_cast<std::uint32_t>(largeAddress), static_cast<std::uint32_t>(largeAddress >> 32u) & 0xffffu, static_cast<std::uint32_t>(largeBytes), 0x31000000u};
    largeBinding.bufferWritten = {false};
    program.bindings.push_back(largeBinding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    {
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, compute);
        const auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
        Require(bindings != nullptr, "read-only draw bindings were not prepared");
        Require(bindings->snapshots.size() == 1, "an input exceeding the draw snapshot cap was snapshotted");
        Require(bindings->snapshots[0].address == smallAddress, "the snapshotted input does not match the smaller buffer below the cap");
        Require(snapshotRecorder.ReusableDrawSnapshot(largeAddress, largeBytes) == nullptr, "an input exceeding the draw snapshot cap was kept in the snapshot cache");
        if (Watched(address, bytes)) {
            Require(snapshotRecorder.ReusableDrawSnapshot(smallAddress, smallBytes) != nullptr, "an input below the draw snapshot cap was not kept in the snapshot cache");
        }
        const auto contents = bindings->snapshots[0].buffer->Bytes();
        Require(contents.size() == smallBytes && std::all_of(contents.begin(), contents.end(), [](std::byte value) { return value == std::byte{0x11}; }), "a draw snapshot does not hold the guest bytes of its draw");
        std::memset(reinterpret_cast<void*>(smallAddress), 0x22, smallBytes);
        std::memset(reinterpret_cast<void*>(largeAddress), 0x33, largeBytes);
        Require(std::all_of(contents.begin(), contents.end(), [](std::byte value) { return value == std::byte{0x11}; }), "snapshotted input observed CPU overwrite");
        const auto* inPlace = reinterpret_cast<const std::byte*>(largeAddress);
        Require(inPlace[0] == std::byte{0x33} && std::all_of(inPlace, inPlace + largeBytes, [](std::byte value) { return value == std::byte{0x33}; }), "in-place bound input did not observe CPU write");
        std::memset(block, 0x11, bytes);
        snapshotRecorder.Sync();
#ifdef _WIN32
        Require(_putenv_s("APS5_DRAW_SNAPSHOT_MAX_MIB", "64") == 0, "cannot raise snapshot cap");
#else
        Require(setenv("APS5_DRAW_SNAPSHOT_MAX_MIB", "64", 1) == 0, "cannot raise snapshot cap");
#endif
        const auto again = resources.PrepareDrawBindings(snapshotRecorder);
        Require(again != nullptr && again->snapshots.size() == 1 && again->snapshots[0].address == smallAddress, "built resources read the draw snapshot cap again instead of keeping the one they were built with");
        snapshotRecorder.Sync();
#ifdef _WIN32
        Require(_putenv_s("APS5_DRAW_SNAPSHOT_MAX_MIB", "1") == 0, "cannot restore snapshot cap");
#else
        Require(setenv("APS5_DRAW_SNAPSHOT_MAX_MIB", "1", 1) == 0, "cannot restore snapshot cap");
#endif
    }
    const auto firstAddress = address + 4096;
    constexpr std::size_t firstBytes = 600 * 1024;
    const auto secondAddress = address + 600 * 1024 + 8192;
    constexpr std::size_t secondBytes = 600 * 1024;
    ShaderRecompiler::RecompileResult cumulativeProgram;
    ShaderRecompiler::DescriptorBinding firstBinding;
    firstBinding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    firstBinding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    firstBinding.descriptorSet = 0;
    firstBinding.binding = 0;
    firstBinding.count = 1;
    firstBinding.guestDescriptor = {static_cast<std::uint32_t>(firstAddress), static_cast<std::uint32_t>(firstAddress >> 32u) & 0xffffu, static_cast<std::uint32_t>(firstBytes), 0x31000000u};
    firstBinding.bufferWritten = {false};
    cumulativeProgram.bindings.push_back(firstBinding);
    ShaderRecompiler::DescriptorBinding secondBinding;
    secondBinding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    secondBinding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    secondBinding.descriptorSet = 0;
    secondBinding.binding = 1;
    secondBinding.count = 1;
    secondBinding.guestDescriptor = {static_cast<std::uint32_t>(secondAddress), static_cast<std::uint32_t>(secondAddress >> 32u) & 0xffffu, static_cast<std::uint32_t>(secondBytes), 0x31000000u};
    secondBinding.bufferWritten = {false};
    cumulativeProgram.bindings.push_back(secondBinding);
    const CompiledShader cumulativeCompute{ShaderRecompiler::ShaderStage::Compute, &cumulativeProgram, 0};
    {
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, cumulativeCompute);
        const auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
        Require(bindings != nullptr, "read-only draw bindings were not prepared for cumulative test");
        Require(bindings->snapshots.size() == 1, "cumulative cap permitted both inputs to snapshot");
        Require(bindings->snapshots[0].address == firstAddress, "the earlier binding in the layout was not prioritised for the snapshot budget");
        snapshotRecorder.Sync();
    }
    recorder.Activate();
}

void drawSnapshotEvictionTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t bytes = 65536;
    void* block = WriteWatched() ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "write watching unavailable: draw snapshot eviction not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto address = reinterpret_cast<std::uint64_t>(block);
    Recorder cache(device.GetContext());
    const auto buffer = std::make_shared<Buffer>(device.GetContext(), 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto registry = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const auto generation = CollectWrites(address, 4096);
    Require(generation != 0, "the watched block has no generation");
    constexpr auto cap = Recorder::DrawSnapshotEntries;
    cache.KeepDrawSnapshot(address, 16, generation, registry, buffer, Recorder::SnapshotUse::Vertex);
    for (std::size_t size = 1; size <= cap; ++size) cache.KeepDrawSnapshot(address, size, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 1) == buffer, "a kept snapshot is not reusable");
    cache.KeepDrawSnapshot(address, cap + 1, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 2) == nullptr, "the least recently used snapshot survived the cap");
    Require(cache.ReusableDrawSnapshot(address, 1) == buffer && cache.ReusableDrawSnapshot(address, 3) == buffer && cache.ReusableDrawSnapshot(address, cap + 1) == buffer, "eviction dropped a more recently used snapshot");
    cache.KeepDrawSnapshot(address, cap + 2, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 4) == nullptr && cache.ReusableDrawSnapshot(address, 1) == buffer, "the second eviction did not take the next oldest");
    Require(cache.ReusableDrawSnapshot(address, 16, Recorder::SnapshotUse::Vertex) == buffer, "storage snapshots evicted a vertex snapshot");
    cache.KeepDrawSnapshot(address, 1, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 1) == buffer && cache.ReusableDrawSnapshot(address, 5) == buffer, "replacing a kept snapshot evicted another");
    cache.KeepDrawSnapshot(address + 8192, Recorder::DrawSnapshotBudget, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 1) == nullptr && cache.ReusableDrawSnapshot(address, 5) == nullptr, "the byte budget did not evict the older snapshots");
    cache.KeepDrawSnapshot(address, 16, generation, registry, buffer);
    std::memset(block, 0x5a, 16);
    CollectWrites(address, 16);
    Require(cache.ReusableDrawSnapshot(address, 16) == nullptr && cache.ReusableDrawSnapshot(address, 16) == nullptr, "a snapshot outlived a CPU store");
}

void containingSnapshotTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t bytes = 131072;
    void* block = WriteWatched() ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "write watching unavailable: containing draw snapshots not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto address = reinterpret_cast<std::uint64_t>(block);
    const VkDeviceSize alignment = std::max<VkDeviceSize>(device.GetContext().limits.minStorageBufferOffsetAlignment, 4);
    Recorder cache(device.GetContext());
    const auto whole = std::make_shared<Buffer>(device.GetContext(), bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto exact = std::make_shared<Buffer>(device.GetContext(), 4096, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto registry = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const auto generation = CollectWrites(address, bytes);
    Require(generation != 0, "the watched block has no generation");
    cache.KeepDrawSnapshot(address, bytes, generation, registry, whole);
    const auto inner = address + 65536 + 16 * alignment;
    VkDeviceSize offset = 1;
    Require(cache.ReusableDrawSnapshot(inner, 4096) == nullptr, "a lookup without an offset took a containing snapshot");
    Require(cache.ReusableDrawSnapshot(inner, 4096, Recorder::SnapshotUse::Storage, nullptr, 0, &offset) == whole && offset == inner - address, "an unchanged range inside a kept snapshot was not taken from it at its offset");
    Require(cache.ReusableDrawSnapshot(inner, address + bytes - inner + 4, Recorder::SnapshotUse::Storage, nullptr, 0, &offset) == nullptr, "a range past the end of a kept snapshot was taken from it");
    Require(cache.ReusableDrawSnapshot(inner + alignment / 2, 4096, Recorder::SnapshotUse::Storage, nullptr, 0, &offset) == nullptr, "a misaligned range was taken from a kept snapshot");
    Require(cache.ReusableDrawSnapshot(inner, 4096, Recorder::SnapshotUse::Vertex, nullptr, 0, &offset) == nullptr, "a vertex lookup took a storage snapshot");
    cache.KeepDrawSnapshot(inner, 4096, generation, registry, exact);
    offset = 1;
    Require(cache.ReusableDrawSnapshot(inner, 4096, Recorder::SnapshotUse::Storage, nullptr, 0, &offset) == exact && offset == 0, "an exact snapshot lost to a containing one");
    std::memset(block, 0x5a, 16);
    CollectWrites(address, 16);
    Require(cache.ReusableDrawSnapshot(inner + 4096, 4096, Recorder::SnapshotUse::Storage, nullptr, 0, &offset) == whole && offset == inner + 4096 - address, "a store outside the range stopped it from being taken from a containing snapshot");
    Require(cache.ReusableDrawSnapshot(address, 4096, Recorder::SnapshotUse::Storage, nullptr, 0, &offset) == nullptr, "a range over a CPU store was taken from a containing snapshot");
    std::memset(reinterpret_cast<void*>(inner + 4096), 0x5a, 16);
    CollectWrites(inner + 4096, 16);
    Require(cache.ReusableDrawSnapshot(inner + 4096, 4096, Recorder::SnapshotUse::Storage, nullptr, 0, &offset) == nullptr, "a containing snapshot outlived a CPU store into the range");
}

void unwrittenSnapshotTests(const Device& device) {
    Recorder cache(device.GetContext());
    const auto buffer = std::make_shared<Buffer>(device.GetContext(), 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    constexpr std::uint64_t address = 0x10000000;
    const std::array<std::uint64_t, 3> stamp{1, 1, 1};
    const std::array<std::uint64_t, 3> next{1, 2, 1};
    cache.NoteUnwrittenDrawSnapshot(address, 16, stamp, buffer);
    Require(cache.UnwrittenDrawSnapshot(address, 16, stamp) == buffer, "a noted unwritten snapshot is not found under its stamp");
    cache.NoteUnwrittenDrawSnapshot(address + 8192, Recorder::DrawSnapshotBudget, stamp, buffer);
    Require(cache.UnwrittenDrawSnapshot(address + 8192, Recorder::DrawSnapshotBudget, stamp) == nullptr, "unwritten snapshots grew past the byte budget");
    cache.NoteUnwrittenDrawSnapshot(address, Recorder::DrawSnapshotBudget, stamp, buffer);
    Require(cache.UnwrittenDrawSnapshot(address, Recorder::DrawSnapshotBudget, stamp) == buffer, "replacing an unwritten snapshot kept the bytes of the old one");
    Require(cache.UnwrittenDrawSnapshot(address, Recorder::DrawSnapshotBudget, next) == nullptr && cache.UnwrittenDrawSnapshot(address, Recorder::DrawSnapshotBudget, stamp) == nullptr, "an unwritten snapshot outlived a newer stamp");
    for (std::size_t entry = 0; entry < Recorder::UnwrittenSnapshotEntries; ++entry) cache.NoteUnwrittenDrawSnapshot(address + entry * 16, 1, next, buffer);
    cache.NoteUnwrittenDrawSnapshot(address + Recorder::UnwrittenSnapshotEntries * 16, 1, next, buffer);
    Require(cache.UnwrittenDrawSnapshot(address, 1, next) == buffer && cache.UnwrittenDrawSnapshot(address + Recorder::UnwrittenSnapshotEntries * 16, 1, next) == nullptr, "unwritten snapshots grew past the entry cap");
    const std::array<std::uint64_t, 3> later{1, 3, 1};
    cache.NoteUnwrittenDrawSnapshot(address, 16, later, buffer, 256);
    VkDeviceSize offset = 0;
    Require(cache.UnwrittenDrawSnapshot(address, 16, later, &offset) == buffer && offset == 256, "an unwritten snapshot lost its offset in its buffer");
}

void drawSnapshotUploadTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    const auto limit = ShaderResources::DrawUploadLimit();
    if (limit < 256 || limit > 16384) {
        std::cout << "draw upload limit outside 256..16384 bytes: draw snapshot uploads not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
    void* block = context.hostImportAlignment != 0 ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "host imports or write watching unavailable: draw snapshot uploads not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    for (std::size_t index = 0; index < bytes; ++index) static_cast<std::uint8_t*>(block)[index] = static_cast<std::uint8_t>(index * 7u + 1u);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the watched block refused: draw snapshot uploads not tested\n";
        return;
    }
    const std::array<std::uint64_t, 3> elements{address + 4096, address + 8192, address + 32768};
    const std::array<std::size_t, 3> sizes{256, limit, limit + 256};
    ShaderRecompiler::RecompileResult program;
    for (std::uint32_t index = 0; index < elements.size(); ++index) {
        ShaderRecompiler::DescriptorBinding binding;
        binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
        binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
        binding.descriptorSet = 0;
        binding.binding = index;
        binding.count = 1;
        binding.guestDescriptor = {static_cast<std::uint32_t>(elements[index]), static_cast<std::uint32_t>(elements[index] >> 32u) & 0xffffu, static_cast<std::uint32_t>(sizes[index]), 0x31000000u};
        binding.bufferWritten = {false};
        program.bindings.push_back(binding);
    }
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    {
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, compute);
        const auto alignment = std::max<VkDeviceSize>(snapshotContext.limits.minStorageBufferOffsetAlignment, 16);
        const auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
        Require(bindings != nullptr && bindings->snapshots.size() == elements.size(), "read-only draw inputs were not snapshotted");
        for (std::size_t index = 0; index < elements.size(); ++index) {
            const auto& snapshot = bindings->snapshots[index];
            Require(snapshot.address == elements[index] && snapshot.bytes == sizes[index] && snapshot.offset % alignment == 0, "a draw snapshot is not an aligned copy of its input");
            Require(std::memcmp(snapshot.Bytes().data(), reinterpret_cast<const void*>(elements[index]), sizes[index]) == 0, "a draw snapshot does not hold the guest bytes of its draw");
        }
        Require(bindings->snapshots[0].buffer == bindings->snapshots[1].buffer && bindings->snapshots[0].offset != bindings->snapshots[1].offset, "draw inputs up to the upload limit do not share the recorder's upload chunk");
        Require(bindings->snapshots[2].buffer != bindings->snapshots[0].buffer && bindings->snapshots[2].offset == 0, "a draw input above the upload limit was copied into the upload chunk");
        const auto earlier = bindings->snapshots[0].Bytes();
        const std::vector<std::byte> before(earlier.begin(), earlier.end());
        std::memset(reinterpret_cast<void*>(elements[0]), 0x5a, sizes[0]);
        const auto again = resources.PrepareDrawBindings(snapshotRecorder);
        Require(again != nullptr && again->snapshots.size() == elements.size(), "read-only draw inputs were not snapshotted again");
        const auto copied = again->snapshots[0].Bytes();
        Require(again->snapshots[0].buffer != bindings->snapshots[0].buffer || again->snapshots[0].offset != bindings->snapshots[0].offset, "a draw input copied again took the slice an earlier draw reads");
        Require(std::all_of(copied.begin(), copied.end(), [](std::byte value) { return value == std::byte{0x5a}; }), "a draw snapshot copied again misses the CPU store");
        Require(std::equal(before.begin(), before.end(), earlier.begin()), "copying a draw input again changed the slice an earlier draw reads");
        const auto first = snapshotRecorder.DrawUpload(16);
        const auto second = snapshotRecorder.DrawUpload(16);
        Require(second.buffer == first.buffer && second.offset >= first.offset + 16 && second.offset % alignment == 0, "upload slices overlap or are not aligned");
        const auto whole = snapshotRecorder.DrawUpload(Recorder::DrawUploadChunk);
        Require(whole.buffer != second.buffer && whole.offset == 0, "a full upload chunk was not replaced");
        snapshotRecorder.Sync();
    }
    recorder.Activate();
}

void drawInputReuseTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    using Use = Recorder::SnapshotUse;
    constexpr std::size_t bytes = 65536;
    void* block = WriteWatched() ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "write watching unavailable: draw input reuse not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto& context = device.GetContext();
    auto* words = static_cast<std::uint32_t*>(block);
    for (std::uint32_t i = 0; i < bytes / sizeof(std::uint32_t); ++i) words[i] = i * 3;
    const auto address = reinterpret_cast<std::uint64_t>(block);
    constexpr std::size_t size = 256;
    const auto equalsGuest = [&](const DrawInputCopy& copy) {
        const auto contents = copy.buffer->Bytes();
        return contents.size() == size && std::memcmp(contents.data(), block, size) == 0;
    };
    const auto first = CopyDrawInput(context, &recorder, address, size, 4, Use::Index32);
    Require(!first.reused && first.generation != 0 && equalsGuest(first), "the first draw input copy is wrong");
    KeepDrawInput(&recorder, address, first, Use::Index32, 189);
    const auto second = CopyDrawInput(context, &recorder, address, size, 4, Use::Index32);
    Require(second.reused && second.buffer == first.buffer && second.derived == 189, "an unchanged draw input was copied again");
    const auto vertex = CopyDrawInput(context, &recorder, address, size, 1, Use::Vertex);
    const auto narrow = CopyDrawInput(context, &recorder, address, size, 2, Use::Index16);
    Require(!vertex.reused && !narrow.reused && equalsGuest(vertex) && equalsGuest(narrow), "a draw input reused another use's copy");
    KeepDrawInput(&recorder, address, vertex, Use::Vertex, 0);
    Require(CopyDrawInput(context, &recorder, address, size, 1, Use::Vertex).buffer == vertex.buffer && CopyDrawInput(context, &recorder, address, size, 4, Use::Index32).buffer == first.buffer, "the uses' copies displaced each other");
    Require(!CopyDrawInput(context, nullptr, address, size, 4, Use::Index32).reused, "a draw input was reused without a recorder");
    words[5] = 0xdead;
    const auto stored = CopyDrawInput(context, &recorder, address, size, 4, Use::Index32);
    Require(!stored.reused && stored.buffer != first.buffer && equalsGuest(stored), "a draw input outlived a CPU store");
    KeepDrawInput(&recorder, address, stored, Use::Index32, 0xdead);
    Require(CopyDrawInput(context, &recorder, address, size, 4, Use::Index32).derived == 0xdead, "the new copy was not kept");
    MarkWritten(address + 128, 4);
    const auto marked = CopyDrawInput(context, &recorder, address, size, 4, Use::Index32);
    Require(!marked.reused && equalsGuest(marked), "a draw input outlived a stamped GPU store");
    KeepDrawInput(&recorder, address, marked, Use::Index32, 1);
    Require(CopyDrawInput(context, &recorder, address, size, 4, Use::Index32).reused, "the copy after the GPU store was not kept");
    alignas(64) static std::byte other[64];
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(other, sizeof(other), true, true, true);
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(other);
    }
    Require(!CopyDrawInput(context, &recorder, address, size, 4, Use::Index32).reused, "a draw input outlived a registry mutation");
    const auto pool = address + 8192;
    const auto whole = CopyDrawInput(context, &recorder, pool, 12288, 1, Use::Vertex);
    Require(!whole.reused && std::memcmp(whole.buffer->Bytes().data(), reinterpret_cast<const void*>(pool), 12288) == 0, "the vertex pool copy is wrong");
    KeepDrawInput(&recorder, pool, whole, Use::Vertex, 0);
    const auto prefix = CopyDrawInput(context, &recorder, pool, 4096, 1, Use::Vertex);
    Require(prefix.reused && prefix.buffer == whole.buffer, "a shorter vertex read did not reuse the longer snapshot");
    Require(!CopyDrawInput(context, &recorder, pool, 16384, 1, Use::Vertex).reused, "a longer vertex read reused a shorter snapshot");
    Require(!CopyDrawInput(context, &recorder, pool + 4, 4096, 1, Use::Vertex).reused, "a vertex read at another address reused the snapshot");
    Require(!CopyDrawInput(context, &recorder, pool, 4096, 4, Use::Index32).reused, "an index read reused a longer vertex snapshot");
    const auto shorter = CopyDrawInput(context, &recorder, pool, 8192, 4, Use::Index32);
    KeepDrawInput(&recorder, pool, shorter, Use::Index32, 3);
    Require(!CopyDrawInput(context, &recorder, pool, 4096, 4, Use::Index32).reused, "an index read reused a longer index snapshot");
    words[(8192 + 8192 + 16) / 4] = 0xbeef;
    const auto prefixAfter = CopyDrawInput(context, &recorder, pool, 4096, 1, Use::Vertex);
    Require(std::memcmp(prefixAfter.buffer->Bytes().data(), reinterpret_cast<const void*>(pool), 4096) == 0, "a shorter vertex read after a store got other bytes");
    const auto after = CopyDrawInput(context, &recorder, pool, 12288, 1, Use::Vertex);
    Require(!after.reused && std::memcmp(after.buffer->Bytes().data(), reinterpret_cast<const void*>(pool), 12288) == 0, "a vertex read over the store reused the old bytes");
    KeepDrawInput(&recorder, pool, after, Use::Vertex, 0);
    const auto small = CopyDrawInput(context, &recorder, pool, 4096, 1, Use::Vertex);
    Require(small.reused && small.buffer == after.buffer, "the new vertex snapshot does not serve shorter reads");
    recorder.Sync();
}

void drawInputInPlaceTests(const Device& device, Recorder& recorder, bool copied = false) {
    auto context = device.GetContext();
    constexpr std::size_t bytes = 65536;
    void* block = context.hostImportAlignment != 0 ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "host imports or write watching unavailable: in-place draw inputs not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the watched block refused: in-place draw inputs not tested\n";
        return;
    }
    context.recorder = &recorder;
    recorder.Sync();
    const auto vertices = address + 4096;
    context.drawInputsInPlace = !copied;
    if (copied) {
        Require(InPlaceDrawInput(context, vertices, 256, 4) == nullptr, "an imported draw input was bound in place where draw inputs are copied");
        Require(!recorder.PendingReadOverlaps(vertices, 256), "a copied draw input's read was noted as an in-place read");
        return;
    }
    const auto* import = InPlaceDrawInput(context, vertices, 256, 4);
    Require(import != nullptr && import->base <= vertices && vertices + 256 <= import->base + import->bytes, "an imported draw input was not bound in place");
    const auto read = recorder.DescribePendingRead(vertices, 256);
    Require(read.has_value() && read->kind == Recorder::ReadKind::DrawInput && read->open, "an in-place draw input's read was not noted on the open batch");
    Require(InPlaceDrawInput(context, vertices + 2, 256, 4) == nullptr, "a draw input misaligned for its format was bound in place");
    Require(InPlaceDrawInput(context, vertices + 2, 256, 2) != nullptr, "an aligned 16-bit draw input was not bound in place");
    recorder.NotePendingWrite(address + 8192, 16);
    Require(InPlaceDrawInput(context, address + 8192, 256, 4) != nullptr && !recorder.PendingWriteOverlaps(address + 8192, 256), "a draw input under a pending GPU write was bound before the write completed");
    bool behind = false;
    Require(InPlaceDrawInput(context, address + 20480, 256, 4, &behind) != nullptr && !behind, "a vertex input free of pending GPU writes was marked as ordered behind one");
    recorder.NotePendingWrite(address + 20480, 16);
    Require(InPlaceDrawInput(context, address + 20480, 256, 4, &behind) != nullptr && behind && recorder.PendingWriteOverlaps(address + 20480, 256), "a vertex input under a pending GPU write was not bound in place behind it");
    const std::array<std::byte, 4> label{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    recorder.RecordStore(import->buffer, address + 12288 - import->base, label, address + 12288);
    Require(recorder.QueuedStoreOverlaps(address + 12288, 4), "the label store was not queued");
    Require(InPlaceDrawInput(context, address + 12288, 256, 4) == nullptr, "a draw input under a queued label store was bound in place");
    behind = false;
    Require(InPlaceDrawInput(context, address + 12288, 256, 4, &behind) == nullptr, "a vertex input under a queued label store was bound in place");
    Require(InPlaceDrawInput(context, address + 16384, 256, 4) != nullptr, "a queued label store refused a disjoint draw input");
    alignas(64) static std::array<std::byte, 256> unregistered{};
    Require(InPlaceDrawInput(context, reinterpret_cast<std::uint64_t>(unregistered.data()), unregistered.size(), 4) == nullptr, "memory without a host import was bound in place");
    auto unrecorded = context;
    unrecorded.recorder = nullptr;
    Require(InPlaceDrawInput(unrecorded, vertices, 256, 4) == nullptr, "a draw without a recorder bound its input in place");
    recorder.Sync();
    Require(!recorder.PendingReadOverlaps(vertices, 256), "an in-place draw input's read outlived its batch");
    if (!AgcDriver::GuestMemory::Watched(address, bytes)) {
        std::cout << "host imports are compared, not watched: kept highest indices not tested\n";
        return;
    }
    const auto indices = address + 20480;
    constexpr std::size_t kept = Recorder::KeptIndexRangeIndices * 2;
    std::vector<std::uint16_t> first(Recorder::KeptIndexRangeIndices * 2, 1);
    first[0] = 3;
    first[1] = 9;
    first[2] = 0xffff;
    std::memcpy(reinterpret_cast<void*>(indices), first.data(), kept * 2);
    auto range = InPlaceIndexRange(recorder, indices, kept, 2, false);
    Require(!range.reused && range.highest == 0xffffu, "an in-place index range was not scanned at its first use");
    range = InPlaceIndexRange(recorder, indices, kept, 2, false);
    Require(range.reused && range.highest == 0xffffu, "an unchanged in-place index range was scanned again");
    range = InPlaceIndexRange(recorder, indices, kept, 2, true);
    Require(!range.reused && range.highest == 9u, "an in-place index range served a scan made under the other restart rule");
    Require(!InPlaceIndexRange(recorder, indices, kept + 2, 2, true).reused, "an in-place index range served a scan of another size");
    const std::uint16_t raised = 12;
    std::memcpy(reinterpret_cast<void*>(indices + 2), &raised, sizeof(raised));
    range = InPlaceIndexRange(recorder, indices, kept, 2, true);
    Require(!range.reused && range.highest == 12u, "an in-place index range kept its highest index over a guest write");
    range = InPlaceIndexRange(recorder, indices, kept, 2, true);
    Require(range.reused && range.highest == 12u, "a rescanned in-place index range was not kept");
    range = InPlaceIndexRange(recorder, indices, kept - 2, 2, true);
    Require(!range.reused && range.highest == 12u, "an in-place index range below the kept size was not scanned");
    range = InPlaceIndexRange(recorder, indices, kept - 2, 2, true);
    Require(!range.reused && range.highest == 12u, "an in-place index range below the kept size was kept");
    range = InPlaceIndexRange(recorder, indices, kept * 2 - 4, 4, false);
    Require(!range.reused && range.highest == 0x000c0003u, "a 32-bit in-place index range below the kept size was not scanned");
    Require(!InPlaceIndexRange(recorder, indices, kept * 2 - 4, 4, false).reused, "a 32-bit in-place index range below the kept size was kept");
    Require(!InPlaceIndexRange(recorder, indices, kept * 2, 4, false).reused, "a 32-bit in-place index range was not scanned at its first use");
    range = InPlaceIndexRange(recorder, indices, kept * 2, 4, false);
    Require(range.reused && range.highest == 0x000c0003u, "an unchanged 32-bit in-place index range was scanned again");
    const std::vector<std::uint16_t> restarts(Recorder::KeptIndexRangeIndices, 0xffff);
    std::memcpy(reinterpret_cast<void*>(indices + 20480), restarts.data(), kept);
    Require(!InPlaceIndexRange(recorder, indices + 20480, kept, 2, true).highest.has_value(), "an index range of restart indices has a highest index");
    range = InPlaceIndexRange(recorder, indices + 20480, kept, 2, true);
    Require(range.reused && !range.highest.has_value(), "an empty in-place index range was scanned again");
}

// Unit shadows (UnitShadow.hpp) over a host import of write-watched arena memory: a retile piece's
// slab destination and its seeds, freshness from the tracker (a publish never stamps, a CPU write
// makes the unit stale), the scopes, the slab boundary, and the retire publish. With
// APS5_UNIT_SHADOW_MIB=8 (one slab) the second slab's allocation evicts the first with a publish;
// with APS5_NO_UNIT_SHADOW=1 every primitive is inert.
bool unitShadowTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    if (!UnitShadowEnabled()) {
        Require(!AnyShadowedOverlaps(0x10000, 16) && PublishShadow(0x10000, 16, PublishScope::Whole, PublishReason::Hook) == 0, "unit shadows are off but not inert");
        std::cout << "unit shadows off (APS5_NO_UNIT_SHADOW, or no write watching): primitives inert\n";
        return true;
    }
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: unit shadows not tested\n";
        return false;
    }
    constexpr std::uint64_t unit = 65536;
    // Three slabs at the default 8 MiB: units 0..127, 128..255, 256..271.
    constexpr std::size_t bytes = (17u << 20u);
    void* block = AllocateWatched(bytes, 65536);
    Require(block != nullptr, "unit shadows are on without write watching");
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        bool armed = true;
        ~Unregister() {
            if (!armed) return;
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{context, block, address};
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr) {
        std::cout << "host import of the shadow test block refused: unit shadows not tested\n";
        return false;
    }
    if (!Watched(address, bytes)) {
        std::cout << "host imports are compared, not watched: unit shadows not tested\n";
        return false;
    }
    Require(import->base == address && import->bytes == bytes, "the import does not cover the block");
    CollectWritesUncached(address, bytes);
    const auto* words = static_cast<const std::uint8_t*>(block);
    const auto unit3 = address + 3 * unit;
    const auto unit4 = unit3 + unit;
    const auto unit5 = unit4 + unit;
    // A half-unit piece seeds its unit from the import; a whole-unit piece does not; both land in
    // one slab at one offset.
    auto half = ShadowDestinationFor(context, *import, unit3, unit3 + unit / 2);
    Require(half.has_value() && half->seedUnits.size() == 1 && half->seedUnits[0].first == unit3 && half->seedUnits[0].second == unit4, "a half-unit retile piece does not seed its unit");
    auto whole = ShadowDestinationFor(context, *import, unit3, unit4);
    Require(whole.has_value() && whole->seedUnits.empty() && whole->slab == half->slab && whole->offset == half->offset && whole->buffer == half->buffer, "a whole-unit retile piece seeds, or lands elsewhere");
    Require(!AnyShadowedOverlaps(unit3, unit), "a destination alone counts as shadowed");
    const auto firstSlab = half->slab;
    Require(firstSlab->pins.load() == 2 && half->pin != nullptr, "destinations do not pin their slab");
    // The retile as writeBackWindows records it: the seed, then the piece's bytes over its half.
    const auto copyBuffer = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
    const auto retile = [&](const ShadowDestination& destination, std::uint64_t begin, std::uint64_t length, std::uint8_t value, bool seed) {
        auto pattern = std::make_shared<Buffer>(context, static_cast<std::size_t>(length), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memset(pattern->Bytes().data(), value, pattern->Bytes().size());
        const auto commands = recorder.Commands();
        recorder.Keep(pattern);
        recorder.Keep(destination.slab);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        if (seed) {
            const VkBufferCopy seedCopy{begin - import->base, SlabOffset(*import, *destination.slab, begin), unit};
            copyBuffer(commands, import->buffer, destination.buffer, 1, &seedCopy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        }
        const VkBufferCopy piece{0, SlabOffset(*import, *destination.slab, begin), length};
        copyBuffer(commands, pattern->Handle(), destination.buffer, 1, &piece);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        recorder.MarkCovered(VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const ShadowedRange range{begin & ~(unit - 1), (begin & ~(unit - 1)) + unit, destination.slab};
        MarkShadowed(*import, std::span(&range, 1), TrackerGeneration());
    };
    retile(*half, unit3, unit / 2, 0x22, true);
    Require(AnyShadowedOverlaps(unit3, 1) && AnyShadowedOverlaps(unit4 - 1, 1) && AnyShadowedOverlaps(unit3 + 1000, 64), "a fresh unit is not seen as shadowed");
    Require(!AnyShadowedOverlaps(unit3 - 1, 1) && !AnyShadowedOverlaps(unit4, unit) && !AnyShadowedOverlaps(address, 3 * unit), "a range outside the unit is seen as shadowed");
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 2> outside{{{unit3 - 8, unit3}, {unit4, unit4 + unit}}};
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 2> reaching{{{unit3 - 8, unit3}, {unit4 - 4, unit4}}};
    Require(!AnyShadowedOverlaps(outside) && AnyShadowedOverlaps(reaching), "a set of ranges around the fresh unit was answered wrongly");
    {
        // An upload's runs over units 2..4 split at unit 3's freshness; the fresh piece reads the slab.
        const std::pair<std::uint64_t, std::uint64_t> run{2 * unit, 5 * unit};
        const auto sources = ShadowSources(context, *import, address, std::span(&run, 1), {});
        Require(sources.size() == 3, "an upload run is not split at the fresh unit");
        Require(!sources[0].shadow && sources[0].begin == 2 * unit && sources[0].end == 3 * unit && sources[0].buffer == import->buffer && sources[0].offset == 2 * unit, "the import piece before the fresh unit is wrong");
        Require(sources[1].shadow && sources[1].begin == 3 * unit && sources[1].end == 4 * unit && sources[1].buffer == half->buffer && sources[1].offset == half->offset && sources[1].slab == half->slab, "the fresh unit's piece does not read the slab");
        Require(!sources[2].shadow && sources[2].begin == 4 * unit && sources[2].end == 5 * unit, "the import piece after the fresh unit is wrong");
    }
    // A whole publish: the untouched half keeps the seed (the import's bytes), the other the
    // retile's; the copy stamps nothing, and a second publish copies nothing.
    const auto pre = CollectWritesUncached(address, bytes);
    Require(PublishShadow(unit3, unit, PublishScope::Whole, PublishReason::Hook) == 1, "the whole publish did not copy the unit");
    Require(recorder.PendingWriteOverlaps(unit3, unit), "the publish noted no pending write");
    recorder.Sync();
    Require(words[3 * unit] == 0x22 && words[3 * unit + unit / 2 - 1] == 0x22 && words[3 * unit + unit / 2] == 0x11 && words[4 * unit - 1] == 0x11 && words[3 * unit - 1] == 0x11 && words[4 * unit] == 0x11, "the published bytes are wrong");
    Require(!AnyShadowedOverlaps(unit3, unit), "a published unit still counts as shadowed");
    Require(PublishShadow(unit3, unit, PublishScope::Whole, PublishReason::Hook) == 0, "a second publish copied the unit again");
    Require(UnchangedSince(unit3, unit, pre), "the publish stamped the unit");
    {
        // The published unit still reads from the slab (both hold the bytes).
        const std::pair<std::uint64_t, std::uint64_t> run{3 * unit, 4 * unit};
        const auto sources = ShadowSources(context, *import, address, std::span(&run, 1), {});
        Require(sources.size() == 1 && sources[0].shadow, "a published unit does not read from the slab");
    }
    // Partial-unit scope over unit 3 (partly, fresh again) and unit 4 (wholly, fresh): unit 3 only.
    retile(*whole, unit3, unit, 0x33, false);
    auto four = ShadowDestinationFor(context, *import, unit4, unit5);
    Require(four.has_value() && four->seedUnits.empty() && four->slab == half->slab, "unit 4's destination is not in the same slab");
    retile(*four, unit4, unit, 0x44, false);
    Require(AnyShadowedOverlaps(unit3, unit) && AnyShadowedOverlaps(unit4, unit), "re-marked units are not shadowed");
    Require(PublishShadow(unit3 + 100, static_cast<std::size_t>(2 * unit - 100), PublishScope::PartialUnits, PublishReason::Fill) == 1, "the partial publish did not copy exactly the partly covered unit");
    Require(!AnyShadowedOverlaps(unit3, unit) && AnyShadowedOverlaps(unit4, unit), "the partial publish copied the wrong unit");
    recorder.Sync();
    Require(words[3 * unit] == 0x33 && words[4 * unit - 1] == 0x33 && words[4 * unit] == 0x11, "the partial publish's bytes are wrong");
    // A CPU write into a page of unit 4 makes it stale: the publish collects the unit itself (no
    // walk saw the store before it), copies nothing and drops it.
    static_cast<void>(CollectWritesUncached(unit4, unit));
    *static_cast<volatile std::uint8_t*>(static_cast<void*>(static_cast<std::uint8_t*>(block) + 4 * unit + 4096)) = 0x55;
    Require(PublishShadow(unit4, unit, PublishScope::Whole, PublishReason::Hook) == 0, "a stale unit was published");
    Require(!AnyShadowedOverlaps(unit4, unit), "a stale unit still counts as shadowed after its publish");
    recorder.Sync();
    Require(words[4 * unit + 4096] == 0x55 && words[4 * unit] == 0x11, "a stale unit's publish overwrote the CPU's bytes");
    // The slab boundary: a piece over units 127 and 128 is refused, each alone lands in its slab.
    const auto unit127 = address + 127 * unit;
    const auto unit128 = unit127 + unit;
    Require(SlabBoundary(*import, unit127) == unit128 && SlabBoundary(*import, address) == unit128 && SlabBoundary(*import, unit128) == unit128 + 128 * unit, "the slab boundary is wrong");
    Require(!ShadowDestinationFor(context, *import, unit127, unit128 + unit).has_value(), "a piece across a slab boundary was accepted");
    auto low = ShadowDestinationFor(context, *import, unit127, unit128);
    Require(low.has_value() && low->slab == firstSlab && low->offset == 127 * unit, "unit 127 does not land in the first slab");
    retile(*low, unit127, unit, 0x66, false);
    auto high = ShadowDestinationFor(context, *import, unit128, unit128 + unit);
    const char* budgetText = std::getenv("APS5_UNIT_SHADOW_MIB");
    const auto budgetMiB = budgetText != nullptr ? std::strtoull(budgetText, nullptr, 10) : 1024ull;
    if (budgetMiB < 16) {
        // One slab fits: while destinations pin the first slab the second is refused (a
        // write-back in progress must keep its slab); with the pins released, the second slab's
        // allocation evicts the first after publishing its fresh unit 127 (the eviction publish is
        // recorded, so the bytes land at the sync).
        Require(!high.has_value(), "a pinned slab was evicted for a second slab");
        half.reset();
        whole.reset();
        four.reset();
        low.reset();
        Require(firstSlab->pins.load() == 0, "a released destination left its pin");
        high = ShadowDestinationFor(context, *import, unit128, unit128 + unit);
        Require(high.has_value() && high->slab != firstSlab && high->offset == 0, "unit 128 does not land in a second slab under the one-slab budget");
        recorder.Sync();
        Require(words[127 * unit] == 0x66 && !AnyShadowedOverlaps(unit127, unit), "the evicted slab's fresh unit was not published");
        std::cout << "unit shadow eviction under APS5_UNIT_SHADOW_MIB=" << budgetMiB << " verified\n";
    } else {
        Require(high.has_value() && high->slab != firstSlab && high->offset == 0 && AnyShadowedOverlaps(unit127, unit), "unit 128 does not land in a second slab beside the first");
        half.reset();
        whole.reset();
        four.reset();
        low.reset();
    }
    retile(*high, unit128, unit, 0x77, false);
    Require(AnyShadowedOverlaps(unit128, unit), "the second slab's unit is not shadowed");
    high.reset();
    retile(*ShadowDestinationFor(context, *import, unit5, unit5 + unit), unit5, unit, 0x88, false);
    retile(*ShadowDestinationFor(context, *import, address + 2 * unit, unit3), address + 2 * unit, unit, 0x99, false);
    Require(AnyShadowedOverlaps(unit5, unit) && AnyShadowedOverlaps(address + 2 * unit, unit), "units 2 and 5 are not shadowed before the retire");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
        mutation.Add(block, static_cast<std::size_t>(5 * unit), true, true, true);
    }
    Require(HostImportFor(context, address, bytes) == nullptr, "a shrunk registration still imports the old range");
    Require(!AnyShadowedOverlaps(address, bytes), "the retired import's shadow survived");
    recorder.Sync();
    Require(words[2 * unit] == 0x99 && words[3 * unit - 1] == 0x99, "the retire did not publish the unit still registered");
    Require(words[5 * unit] == 0x88 && words[5 * unit + unit - 1] == 0x88, "the retire did not publish a unit covered by the old registration");
    Require(words[127 * unit] == 0x66 && words[128 * unit] == 0x77, "the retire did not publish the second slab's units covered by the old registration");
    return true;
}

void idlePublishWaitTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    constexpr std::uint64_t unit = 65536;
    constexpr std::size_t bytes = 8 * unit;
    void* block = AllocateWatched(bytes, 65536);
    Require(block != nullptr, "unit shadows are on without write watching");
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{context, block, address};
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr || !Watched(address, bytes)) {
        std::cout << "host import of the idle publish test block refused or not watched: idle publish wait not tested\n";
        return;
    }
    recorder.Sync();
    CollectWritesUncached(address, bytes);
    auto* words = static_cast<std::uint8_t*>(block);
    const auto copyBuffer = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
    const auto retile = [&](std::uint64_t begin, std::uint8_t value) {
        auto destination = ShadowDestinationFor(context, *import, begin, begin + unit);
        Require(destination.has_value() && destination->seedUnits.empty(), "a whole-unit retile piece has no destination");
        auto pattern = std::make_shared<Buffer>(context, static_cast<std::size_t>(unit), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memset(pattern->Bytes().data(), value, pattern->Bytes().size());
        const auto commands = recorder.Commands();
        recorder.Keep(pattern);
        recorder.Keep(destination->slab);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkBufferCopy piece{0, destination->offset, unit};
        copyBuffer(commands, pattern->Handle(), destination->buffer, 1, &piece);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        recorder.MarkCovered(VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const ShadowedRange range{begin, begin + unit, destination->slab};
        MarkShadowed(*import, std::span(&range, 1), TrackerGeneration());
        Require(AnyShadowedOverlaps(begin, unit), "the retiled unit is not shadowed");
    };
    retile(address + 2 * unit, 0x22);
    retile(address + 3 * unit, 0x33);
    StorageTexture::FlushAllPending("idle");
    Require(!AnyShadowedOverlaps(address + 2 * unit, static_cast<std::size_t>(2 * unit)), "the idle flush did not publish the shadowed units");
    Require(!recorder.PendingWriteOverlaps(address + 2 * unit, static_cast<std::size_t>(2 * unit)), "the idle flush returned before its publish's copies completed");
    words[2 * unit] = 0x44;
    words[4 * unit - 1] = 0x45;
    recorder.Sync();
    Require(words[2 * unit] == 0x44 && words[4 * unit - 1] == 0x45, "a CPU store made after the idle flush returned was overwritten by its publish");
    Require(words[2 * unit + 1] == 0x22 && words[3 * unit - 1] == 0x22 && words[3 * unit] == 0x33 && words[1 * unit] == 0x11 && words[4 * unit] == 0x11, "the idle publish's bytes are wrong");
    words[5 * unit] = 0x11;
    CollectWritesUncached(address, bytes);
    retile(address + 5 * unit, 0x55);
    StorageTexture::FlushAllPending("label");
    const bool pending = recorder.PendingWriteOverlaps(address + 5 * unit, unit);
    recorder.Sync();
    Require(words[5 * unit] == 0x55 && words[6 * unit - 1] == 0x55, "the label flush's publish did not land: the unit read as written after its retile, so the publish dropped it as stale");
    Require(pending, "a publish of a flush other than the idle one was waited");
    std::cout << "idle publish wait verified\n";
}

void storageRefreshTests(const Device& device, Recorder& recorder, bool watched) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: storage refresh not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
    void* block = nullptr;
    if (watched) {
        block = AllocateWatched(bytes, 65536);
        if (block == nullptr) {
            std::cout << "no write watching: storage refresh in watched memory not tested\n";
            return;
        }
    } else {
#ifdef _WIN32
        block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        block = std::aligned_alloc(65536, bytes);
#endif
    }
    Require(block != nullptr, "cannot allocate the storage refresh block");
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* keys = texels + surfaceBytes;
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(keys, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    const auto* import = HostImportFor(base, address, bytes);
    if (import == nullptr) {
        std::cout << "host import of the storage refresh block refused: storage refresh not tested\n";
        return;
    }
    if (watched && !AgcDriver::GuestMemory::Watched(address, bytes)) std::cout << "host imports are compared, not watched: storage refresh in watched memory runs as unwatched\n";
    watched = watched && AgcDriver::GuestMemory::Watched(address, bytes);
    Require(watched || !ShadowDestinationFor(base, *import, address, address + 65536).has_value(), "a unit shadow was offered for memory the write tracker does not watch");
    if (!AgcDriver::GuestMemory::WriteWatched()) Require(!UnitShadowEnabled(), "unit shadows are on without write watching");
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = address + surfaceBytes;
    Require(DescribeSurface(resource).guestBytes == surfaceBytes, "the test surface has an unexpected size");
    DccKeyProof proof;
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000, "0x00 keys are not a 0000 clear");
    {
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto draw = [&](VkClearColorValue value) {
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
        };
        const auto holds = [&](std::array<std::uint8_t, 4> texel) {
            Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {side, side, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            const auto pixels = readback.Bytes();
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                if (std::to_integer<std::uint8_t>(pixels[i]) != texel[i % 4]) return false;
            }
            return true;
        };
        const auto memoryHolds = [&](std::array<std::uint8_t, 4> texel, std::size_t first = 0, std::uint8_t other = 0) {
            std::vector<std::byte> read(surfaceBytes);
            if (watched) AgcDriver::GuestMemory::Read(address, read);
            else std::memcpy(read.data(), texels, surfaceBytes);
            for (std::size_t i = 0; i < surfaceBytes; ++i) {
                if (std::to_integer<std::uint8_t>(read[i]) != (i < first ? other : texel[i % 4])) return false;
            }
            return true;
        };
        const auto holdsMixed = [&](std::array<std::uint8_t, 4> texel, std::size_t count, std::uint8_t other) {
            Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {side, side, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            const auto pixels = readback.Bytes();
            std::size_t others = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 4) {
                bool isOther = true, isTexel = true;
                for (std::size_t c = 0; c < 4; ++c) {
                    const auto value = std::to_integer<std::uint8_t>(pixels[i + c]);
                    isOther = isOther && value == other;
                    isTexel = isTexel && value == texel[c];
                }
                if (isOther) ++others;
                else if (!isTexel) return false;
            }
            return others == count;
        };
        Require(holds({0, 0, 0, 0}), "a surface under 0000 clear keys was not cleared");
        draw({{1.0f, 0.0f, 0.0f, 1.0f}});
        image->Refresh();
        Require(holds({255, 0, 0, 255}), "a refresh under the image's own clear keys dropped the draw's results");
        if (watched) {
            Require(memoryHolds({255, 0, 0, 255}), "the refresh's store of the results did not reach watched guest memory");
        } else {
            Require(StorageTexture::FindPending(address, surfaceBytes) == image, "an unchanged untracked refresh discarded the pending image");
            Require(StorageTexture::FlushPending(address, surfaceBytes, nullptr, "storage refresh visibility test"), "the pending untracked image was not flushed");
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            Require(memoryHolds({255, 0, 0, 255}), "an explicit flush did not make pending results visible in untracked guest memory");
        }
        if (watched) {
            draw({{0.0f, 0.0f, 1.0f, 1.0f}});
            std::memset(texels, 0x33, 65536);
            image->Refresh();
            Require(holdsMixed({0, 0, 255, 255}, 65536 / 4, 0x33), "a CPU store into a unit with results pending was not seen by the refresh");
            Require(memoryHolds({0, 0, 255, 255}, 65536, 0x33), "guest memory lost the CPU store or the other units' results");
            image->Refresh();
            Require(holdsMixed({0, 0, 255, 255}, 65536 / 4, 0x33), "an unchanged surface changed at a refresh");
        }
        draw({{0.0f, 1.0f, 0.0f, 1.0f}});
        std::memset(keys, 0x40, keyCount);
        image->Refresh();
        Require(holds({0, 0, 0, 255}), "new 0001 clear keys did not clear the image");
        Require(watched ? memoryHolds({0, 0, 255, 255}, 65536, 0x33) : memoryHolds({255, 0, 0, 255}), "results dead under new clear keys were stored");
    }
    recorder.Sync();
}

bool addressShadowTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (!UnitShadowEnabled() || base.hostImportAlignment == 0) {
        std::cout << "skipped, unit shadows or host imports are unavailable\n";
        return false;
    }
    const auto decided = PrepareImportWatch(base);
#ifndef _WIN32
    const auto probe = ProbeImportWriteProtection(base);
    if (probe.failure != nullptr || probe.writtenAtImport != 0 || probe.writtenAfterSubmit != 0) {
        std::cout << "skipped, the device reports watched host memory written when it imports or reads it\n";
        return false;
    }
    SetImportWatch(base, ImportWatch::Watch);
#endif
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{base, decided};
    constexpr std::uint32_t width = 1024;
    constexpr std::uint32_t height = 512;
    constexpr std::size_t bytes = width * height * 4;
    constexpr std::size_t allocated = bytes + 65536;
    void* block = AllocateWatched(allocated, 65536);
    if (block == nullptr) {
        std::cout << "skipped, no write watching\n";
        return false;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, allocated); }
    } release{block};
    std::memset(block, 0x55, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr || !AgcDriver::GuestMemory::Watched(address, bytes)) {
        std::cout << "skipped, the heap is not imported or not watched\n";
        return false;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = width;
    resource.height = height;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    Require(DescribeSurface(resource).guestBytes == bytes, "the heap's surface has an unexpected size");
    const auto* words = static_cast<const std::uint8_t*>(block);
    const auto holds = [&](std::array<std::uint8_t, 4> texel) {
        for (std::size_t i = 0; i < bytes; ++i) {
            if (words[i] != texel[i % 4]) return false;
        }
        return true;
    };
    const auto settle = [&] {
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    };
    struct Build {
        bool copied;
        bool current;
    };
    const auto build = [&](std::array<std::uint8_t, 4> texel) {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        Build result{recorder.PendingWriteOverlaps(address, bytes), false};
        recorder.Submit();
        device.WaitQueue();
        result.current = holds(texel);
        leased.WriteBack();
        recorder.Sync();
        return result;
    };
    const auto none = build({0x55, 0x55, 0x55, 0x55});
    Require(!none.copied && none.current, "an address-based build over a heap with no shadowed units recorded a copy");
    {
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto draw = [&](VkClearColorValue value) {
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
        };
        draw({{1.0f, 0.0f, 0.0f, 1.0f}});
        StorageTexture::FlushPending(address, bytes, nullptr, "storage refresh", PublishScope::None);
        settle();
        if (!AnyShadowedOverlaps(address, bytes)) {
            std::cout << "skipped, the image's write-back did not retile into the unit shadow\n";
            return false;
        }
        Require(holds({0x55, 0x55, 0x55, 0x55}), "a write-back into the unit shadow reached the import");
        static_cast<void>(ShadowReport());
        const auto shadowed = build({255, 0, 0, 255});
        const auto report = ShadowReport();
        Require(shadowed.current, "an address-based build over a heap left the unit shadow's stored results unpublished: the import held the earlier bytes at its submission");
        Require(shadowed.copied && !AnyShadowedOverlaps(address, bytes), "an address-based build published the heap's shadowed units without noting the copy, or left some unpublished");
        Require(report.find(" address 1/") != std::string::npos, "the address-based build's publish was not counted under its own reason:" + report);
        const auto again = build({255, 0, 0, 255});
        Require(!again.copied && again.current, "a second address-based build with nothing newly retiled recorded another copy");
        draw({{0.0f, 1.0f, 0.0f, 1.0f}});
        const auto pending = build({255, 0, 0, 255});
        Require(StorageTexture::FindPending(address, bytes) == image, "an address-based build stored the pending results of an image over its heap");
        Require(!pending.copied && pending.current, "an address-based build changed the heap while an image's results were pending");
        StorageTexture::FlushPending(address, bytes, nullptr, "memory access");
        settle();
    }
    recorder.Sync();
    std::cout << "address-based builds read the unit shadow's stored results (address space cache " << (std::getenv("APS5_NO_ADDRESS_SPACE_CACHE") != nullptr ? "off" : "on") << ")\n";
    return true;
}

bool completionStoreShadowTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (!UnitShadowEnabled() || base.hostImportAlignment == 0) {
        std::cout << "skipped, unit shadows or host imports are unavailable\n";
        return false;
    }
    const auto decided = PrepareImportWatch(base);
#ifndef _WIN32
    const auto probe = ProbeImportWriteProtection(base);
    if (probe.failure != nullptr || probe.writtenAtImport != 0 || probe.writtenAfterSubmit != 0) {
        std::cout << "skipped, the device reports watched host memory written when it imports or reads it\n";
        return false;
    }
    SetImportWatch(base, ImportWatch::Watch);
#endif
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{base, decided};
    constexpr std::uint32_t width = 1024;
    constexpr std::uint32_t height = 512;
    constexpr std::size_t bytes = width * height * 4;
    constexpr std::size_t allocated = bytes + 65536;
    constexpr std::size_t storedOffset = 16;
    void* block = AllocateWatched(allocated, 65536);
    if (block == nullptr) {
        std::cout << "skipped, no write watching\n";
        return false;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, allocated); }
    } release{block};
    std::memset(block, 0x55, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr || !AgcDriver::GuestMemory::Watched(address, bytes)) {
        std::cout << "skipped, the heap is not imported or not watched\n";
        return false;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = width;
    resource.height = height;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    Require(DescribeSurface(resource).guestBytes == bytes, "the heap's surface has an unexpected size");
    const auto settle = [&] {
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    };
    constexpr std::array<std::byte, 4> stored{std::byte{0x77}, std::byte{0x77}, std::byte{0x77}, std::byte{0x77}};
    std::vector<std::byte> read(bytes);
    const auto run = [&](bool shadowed) {
        std::memset(block, 0x55, bytes);
        {
            auto image = StorageTexture::Create(context, detiler, resource, 0);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            const VkClearColorValue red{{1.0f, 0.0f, 0.0f, 1.0f}};
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &red, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
            settle();
            Require(StorageTexture::FindPending(address, bytes) == image, "the cleared image's results are not pending");
            if (shadowed) {
                StorageTexture::FlushPending(address, bytes, nullptr, "storage refresh", PublishScope::None);
                settle();
                if (!AnyShadowedOverlaps(address, bytes)) return false;
                Require(StorageTexture::FindPending(address, bytes) == nullptr, "the image's results stayed pending after their write-back into the unit shadow");
            }
            recorder.OnComplete([&] { AgcDriver::GuestMemory::Write(address + storedOffset, stored, 4); });
            settle();
            AgcDriver::GuestMemory::Read(address, read, 4);
            settle();
        }
        recorder.Sync();
        const std::string over = shadowed ? "a shadowed unit" : "a pending image";
        Require(std::equal(stored.begin(), stored.end(), read.begin() + storedOffset), "a CPU store inside a completion did not reach guest memory over " + over);
        std::size_t lost = 0;
        for (std::size_t i = 0; i < bytes; ++i) {
            if (i >= storedOffset && i < storedOffset + stored.size()) continue;
            const auto expected = std::array<std::uint8_t, 4>{255, 0, 0, 255}[i % 4];
            if (static_cast<std::uint8_t>(read[i]) != expected) ++lost;
        }
        Require(lost == 0, "a CPU store inside a completion over " + over + " lost " + std::to_string(lost) + " bytes of the image's results around it");
        return true;
    };
    run(false);
    if (!run(true)) {
        std::cout << "skipped, the image's write-back did not retile into the unit shadow\n";
        return false;
    }
    std::cout << "a CPU store inside a completion keeps the results of a pending image and of a shadowed unit around it\n";
    return true;
}

bool writtenBufferOrderTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: written buffer order not tested\n";
        return false;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t bytes = side * side * 4;
    void* block = AllocateWatched(bytes, 65536);
    if (block == nullptr) {
        std::cout << "no write watching: written buffer order not tested\n";
        return false;
    }
    std::memset(block, 0x55, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the written buffer order block refused: written buffer order not tested\n";
        return false;
    }
    std::cout << "written buffer order: host imports are " << (AgcDriver::GuestMemory::Watched(address, bytes) ? "watched" : "compared") << "\n";
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    Require(DescribeSurface(resource).guestBytes == bytes, "the written buffer order surface has an unexpected size");
    const auto settle = [&] {
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    };
    {
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const VkClearColorValue red{{1.0f, 0.0f, 0.0f, 1.0f}};
        {
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &red, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
        }
        constexpr std::size_t written = 40800;
        GuestBufferMemory memory(context);
        memory.AcquireRegistered();
        memory.AddWritable(address, written, false, false);
        memory.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(address, written, adjustment);
        Require(adjustment == 0, "the written buffer view is not aligned to its element");
        {
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, view.buffer, view.offset, written, 0xa5a5a5a5u);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        }
        memory.RecordCopyBacks(recorder);
        memory.MarkDirectWrites();
        settle();
        StorageTexture::FlushAllPending("written buffer order test");
        settle();
        std::vector<std::byte> read(bytes);
        AgcDriver::GuestMemory::Read(address, read);
        Require(std::all_of(read.begin(), read.begin() + written, [](std::byte value) { return value == std::byte{0xa5}; }), "storage results pending under a buffer a later use wrote were stored over the buffer's contents");
        for (std::size_t i = written; i < bytes; ++i) Require(std::to_integer<std::uint8_t>(read[i]) == (i % 4 == 1 || i % 4 == 2 ? 0 : 255), "the storage results outside the written buffer did not reach guest memory");
    }
    recorder.Sync();
    return true;
}

void hostImportUnmapTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: import unmap lifetime not tested\n";
        return;
    }
    const auto alignment = static_cast<std::size_t>(std::max<VkDeviceSize>(65536u, context.hostImportAlignment));
    constexpr std::size_t RequestedBytes = 17u << 20u;
    const auto bytes = ((RequestedBytes + alignment - 1u) / alignment) * alignment;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(alignment, bytes);
#endif
    Require(block != nullptr, "cannot allocate the host import unmap block");
    struct Block {
        void* address;
        std::size_t bytes;
        bool registered = true;
        bool released = false;
        void Release() {
#ifdef _WIN32
            Require(VirtualFree(address, 0, MEM_RELEASE) != 0, "cannot release the host import unmap block");
#else
            std::free(address);
#endif
            released = true;
        }
        ~Block() {
            if (registered) {
                try {
                    GuestAllocations::Mutation mutation;
                    mutation.Remove(address);
                } catch (...) {
                    return;
                }
            }
            if (!released) {
#ifdef _WIN32
                VirtualFree(address, 0, MEM_RELEASE);
#else
                std::free(address);
#endif
            }
        }
    } allocation{block, bytes};
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    const HostImport* imported = nullptr;
    {
        std::lock_guard gpu(GpuMutex());
        imported = HostImportFor(context, address, bytes);
    }
    if (imported == nullptr) {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
        allocation.registered = false;
        allocation.Release();
        std::cout << "host import refused: import unmap lifetime not tested\n";
        return;
    }
    bool retiredBeforeRelease = false;
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(block, bytes, [&](const void*, std::size_t, const void*, bool) {
            retiredBeforeRelease = !HostImportCovers(context, address, bytes);
            if (retiredBeforeRelease) allocation.Release();
        });
        allocation.registered = false;
    }
    if (!retiredBeforeRelease) {
        {
            std::lock_guard gpu(GpuMutex());
            static_cast<void>(HostImportFor(context, address, bytes));
            recorder.Sync();
        }
        allocation.Release();
    }
    Require(retiredBeforeRelease, "guest unmap released pages before retiring their host import");
    std::cout << "host import unmap lifetime: ok\n";
}

void parkedImportUnmapTests(const Device& device) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: parked import unmap not tested\n";
        return;
    }
    const auto alignment = static_cast<std::size_t>(std::max<VkDeviceSize>(65536u, context.hostImportAlignment));
    const auto bytes = alignment * 4u;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(alignment, bytes);
#endif
    Require(block != nullptr, "cannot allocate the parked import unmap block");
    const auto release = [&] {
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    };
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    std::unique_ptr<Device> replacement;
    bool imported = false;
    {
        std::lock_guard gpu(GpuMutex());
        imported = HostImportFor(context, address, bytes) != nullptr;
    }
    if (imported) {
        replacement = std::make_unique<Device>();
        std::lock_guard gpu(GpuMutex());
        imported = HostImportFor(replacement->GetContext(), address, bytes) != nullptr;
    }
    if (!imported) {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
        if (replacement) ClearHostImports(replacement->GetContext().device);
        release();
        std::cout << "host import refused: parked import unmap not tested\n";
        return;
    }
    bool unmapped = false;
    try {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(block, bytes, [&](const void*, std::size_t, const void*, bool) { unmapped = true; });
    } catch (const std::exception& error) {
        ClearHostImports(replacement->GetContext().device);
        ClearHostImports(context.device);
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
        release();
        throw std::runtime_error(std::string("guest unmap after a device replacement: ") + error.what());
    }
    ClearHostImports(replacement->GetContext().device);
    ClearImageMirrors(replacement->GetContext().device);
    replacement.reset();
    release();
    Require(unmapped, "guest unmap of a range a replaced device imported did not run");
    std::cout << "parked import unmap: ok\n";
}

void importWatchTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
#ifdef _WIN32
    Require(PrepareImportWatch(context) == ImportWatch::Unwatch, "Windows host imports stayed watched by default");
    std::cout << "import watch decisions: Windows host imports through views use comparisons\n";
    if (context.hostImportAlignment == 0 || !WriteWatched()) {
        std::cout << "host imports or write watching unavailable: the Windows import probe not tested\n";
        return;
    }
    if (context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr) {
        Require(PrepareImportWatch(context) == ImportWatch::Unwatch, "(w) a device without a settle queue kept host imports watched");
        std::cout << "no settle queue: the Windows import probe skipped\n";
        return;
    }
    const auto probe = ProbeImportWriteProtection(context);
    Require(probe.failure == nullptr, std::string("(w) the Windows import probe failed at ") + (probe.failure != nullptr ? probe.failure : "") + " (" + std::to_string(static_cast<int>(probe.result)) + ")");
    std::cout << "Windows import probe: " << probe.writtenAtImport << " of " << probe.pages << " scratch pages reported written at the import, " << probe.writtenDuringSettle << " by its 4-byte settle read, " << probe.writtenAfterSettle << " after it, " << probe.writtenAfterSubmit << " after a full GPU read, " << probe.writtenAfterSecondSubmit << " after a second, " << probe.writtenAfterFill << " after a GPU write, " << probe.writtenByCpu << " after a CPU store\n";
    Require(probe.writtenByCpu != 0, "(w) a CPU store into a host-imported MEM_WRITE_WATCH range was not collected");
    Require(probe.writtenAfterSettle == 0 || PrepareImportWatch(context) == ImportWatch::Unwatch, "(w) a settled import kept reporting writes and stayed watched");
#else
    if (context.hostImportAlignment == 0 || !WriteWatched()) {
        std::cout << "host imports or write watching unavailable: import watch decisions not tested\n";
        return;
    }
    const auto probe = ProbeImportWriteProtection(context);
    Require(probe.failure == nullptr, std::string("(u) the import probe failed at ") + (probe.failure != nullptr ? probe.failure : "") + " (" + std::to_string(static_cast<int>(probe.result)) + ")");
    std::cout << "import probe: " << probe.writtenAfterSubmit << " of " << probe.pages << " scratch pages written after a GPU read, " << probe.writtenAfterFill << " after a GPU write, " << probe.writtenAtImport << " after the import\n";
    Require(probe.writtenByCpu != 0, "(u) a CPU store after the import probe's GPU read was not collected");
    const int udmabuf = context.dmaBufImport ? open("/dev/udmabuf", O_RDWR | O_CLOEXEC) : -1;
    if (udmabuf >= 0) close(udmabuf);
    if (context.dmaBufImport && udmabuf < 0) {
        std::cout << "/dev/udmabuf is not accessible: the dma-buf import probe not tested\n";
    } else if (context.dmaBufImport) {
        const auto dmaBuf = ProbeDmaBufImportWriteProtection(context);
        Require(dmaBuf.failure == nullptr, std::string("(u) the dma-buf import probe failed at ") + (dmaBuf.failure != nullptr ? dmaBuf.failure : "") + " (" + std::to_string(static_cast<int>(dmaBuf.result)) + ")");
        Require(dmaBuf.writtenByCpu != 0, "(u) a CPU store into a dma-buf imported scratch range was not collected");
        std::cout << "dma-buf import probe: " << dmaBuf.writtenAfterSubmit << " of " << dmaBuf.pages << " scratch pages written after a GPU read, " << dmaBuf.writtenAtImport << " after the import, " << dmaBuf.writtenByCpu << " after a CPU store\n";
    } else {
        std::cout << "dma-buf imports unavailable: the dma-buf import probe not tested\n";
    }
    const auto decided = PrepareImportWatch(context);
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{context, decided};
    constexpr std::size_t unit = 65536;
    const auto remap = [](void* block, std::size_t bytes) {
        munmap(block, bytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
        Require(mmap(block, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == block, "(u) cannot map the test block again");
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
    };
    const auto registerRange = [](void* block, std::size_t bytes) {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    };
    const auto unregisterRange = [](void* block) {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    };
    SetImportWatch(context, ImportWatch::Unwatch);
    {
        constexpr std::size_t bytes = 3 * unit;
        void* block = AllocateWatched(bytes, unit);
        const auto address = reinterpret_cast<std::uint64_t>(block);
        std::memset(block, 0x11, bytes);
        const auto before = CollectWrites(address, bytes);
        Require(before != 0, "(u) a watched block is not collected");
        BumpCollectEpoch();
        Require(CollectWrites(address, 2 * unit) != 0, "(u) the memoized collect of a watched block failed");
        registerRange(block, 2 * unit);
        const auto* import = HostImportFor(context, address, 2 * unit);
        Require(import != nullptr && import->unwatched, "(u) an import made under the unwatch decision is not marked unwatched");
        Require(!Watched(address, 2 * unit) && !Watched(address + unit, 4096), "(u) an imported range stays watched");
        Require(CollectWrites(address, 2 * unit) == 0 && CollectWrites(address + 4096, 4096) == 0, "(u) a collect memoized before the import answers for the unwatched range");
        Require(CollectWrites(address + 2 * unit - 4096, 8192) == 0, "(u) a range partly imported is collected");
        Require(Watched(address + 2 * unit, unit), "(u) the unimported neighbour left the watch");
        const auto neighbour = CollectWrites(address + 2 * unit, unit);
        Require(neighbour != 0, "(u) the unimported neighbour is not collected");
        static_cast<volatile std::uint8_t*>(block)[4096] = 0x22;
        Require(!UnchangedSince(address, 2 * unit, before) && !UnchangedSince(address, 4096, TrackerGeneration()) && !UnchangedSinceCollected(address, 4096, TrackerGeneration()), "(u) an unwatched range reports unchanged");
        const std::array<std::uint64_t, 2> generations{before, TrackerGeneration()};
        std::array<std::uint8_t, 2> changed{};
        std::array<std::uint8_t, 2> cpu{};
        Require(!ChangedBlocks(address, 2 * unit, generations, changed, cpu) && changed[0] != 0 && changed[1] != 0, "(u) the stamps of an unwatched range are offered as tracked");
        Require(MarkWritten(address, unit) == 0, "(u) a GPU write into an unwatched range claims a generation");
        Require(MarkWritten(address + 2 * unit, 4096) != 0 && !UnchangedSince(address + 2 * unit, unit, neighbour), "(u) a GPU write into the watched neighbour is not stamped");
        unregisterRange(block);
        remap(block, 2 * unit);
        Require(Watched(address, 2 * unit), "(u) a remapped range is not watched");
        registerRange(block, 2 * unit);
        const auto* kept = HostImportFor(context, address, 2 * unit);
        Require(kept != nullptr && kept->unwatched && !Watched(address, 2 * unit) && CollectWrites(address, 2 * unit) == 0, "(u) an import kept over a remap left the range watched");
        unregisterRange(block);
        remap(block, 2 * unit);
        registerRange(block, unit);
        const auto* again = HostImportFor(context, address, unit);
        Require(again != nullptr && again->unwatched && again->bytes == unit && !Watched(address, unit), "(u) a re-import after an unmap did not follow the decision");
        Require(Watched(address + unit, unit) && CollectWrites(address + unit, unit) != 0, "(u) the unregistered rest of a remapped range is not watched");
        unregisterRange(block);
        Require(HostImportFor(context, address, unit) == nullptr, "(u) an unregistered range still imports");
        ReleaseWatched(block, bytes);
    }
    SetImportWatch(context, ImportWatch::Watch);
    {
        constexpr std::size_t bytes = unit;
        void* block = AllocateWatched(bytes, unit);
        const auto address = reinterpret_cast<std::uint64_t>(block);
        registerRange(block, bytes);
        const auto* import = HostImportFor(context, address, bytes);
        Require(import != nullptr && !import->unwatched && Watched(address, bytes), "(u) an import made under the watch decision left the watch");
        const auto generation = CollectWrites(address, bytes);
        Require(generation != 0 && UnchangedSince(address, bytes, generation), "(u) a watched import is not collected");
        Require(MarkWritten(address, 4096) != 0 && !UnchangedSince(address, bytes, generation), "(u) a GPU write into a watched import is not stamped");
        unregisterRange(block);
        HostImportFor(context, address, bytes);
        ReleaseWatched(block, bytes);
    }
#endif
}

void staleGenerationTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
#ifdef _WIN32
    static_cast<void>(recorder);
    static_cast<void>(base);
    std::cout << "a range leaving the watch: Linux write watch only\n";
#else
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: a range leaving the watch not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
    void* block = AllocateWatched(bytes, 65536);
    if (block == nullptr) {
        std::cout << "no write watching: a range leaving the watch not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto decided = PrepareImportWatch(base);
    SetImportWatch(base, ImportWatch::Unwatch);
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{base, decided};
    auto* texels = static_cast<std::uint8_t*>(block);
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(texels + surfaceBytes, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = address + surfaceBytes;
    {
        Require(AgcDriver::GuestMemory::Watched(address, bytes), "(s) the block is not watched before the storage image takes its generation");
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto commands = recorder.Commands();
        recorder.Keep(image);
        Require(image->Generation() != 0, "(s) the storage image took no generation while its memory was watched");
        const VkClearColorValue red{{1.0f, 0.0f, 0.0f, 1.0f}};
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &red, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        image->MarkDirty();
        const auto* import = HostImportFor(base, address, bytes);
        if (import == nullptr) {
            std::cout << "host import of the stale generation block refused: a range leaving the watch not tested\n";
            recorder.Sync();
            return;
        }
        Require(!AgcDriver::GuestMemory::Watched(address, bytes), "(s) the imported range stayed watched");
        image->Refresh();
        Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto copyCommands = recorder.Commands();
        RecordMemoryBarrier(context, copyCommands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {side, side, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(copyCommands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, copyCommands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        Require(StorageTexture::FindPending(address, surfaceBytes) == image, "(s) the image is not pending after the refresh");
        Require(StorageTexture::FlushPending(address, surfaceBytes, nullptr, "stale generation test"), "(s) the pending image was not flushed");
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        const auto pixels = readback.Bytes();
        constexpr std::array<std::uint8_t, 4> expected{255, 0, 0, 255};
        for (std::size_t i = 0; i < pixels.size(); ++i) Require(std::to_integer<std::uint8_t>(pixels[i]) == expected[i % 4], "(s) results pending when their memory left the watch were dropped");
        for (std::size_t i = 0; i < surfaceBytes; ++i) Require(texels[i] == expected[i % 4], "(s) results pending when their memory left the watch did not reach it");
    }
    recorder.Sync();
#endif
}

// A write-back stores the texels of an image and leaves every other byte of the surface's memory
// as it was: the padding of the tile blocks the surface covers only partly (its last block column
// and row), the tail block of a mip chain, the bytes between the mips and the pitch padding of a
// linear surface. Another surface over the memory, or the CPU, reads those bytes. Each case clears
// every mip of an image over memory holding a pattern no texel equals, stores it and reads the
// memory back (watched memory through the flush hook, after a store into the import's unit shadow):
// each 4-byte word is the clear texel or the pattern, and the texels are exactly the surface's.
// The 64 KiB-aligned tiled cases store through block windows, the others whole layers.
void writeBackPaddingTests(const Device& device, Recorder& recorder, bool watched) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: write-back padding not tested\n";
        return;
    }
    struct Case {
        const char* name;
        TextureTileMode tileMode;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t mipCount;
        std::uint64_t offset;
        bool direct = false;
    };
    constexpr Case cases[] = {
        {"64 KiB R_X 200x150", TextureTileMode::kR64KBX, 200, 150, 1, 0},
        {"64 KiB R_X 200x150, 3 mips", TextureTileMode::kR64KBX, 200, 150, 3, 0},
        {"64 KiB R_X 520x260, 6 mips", TextureTileMode::kR64KBX, 520, 260, 6, 0, true},
        {"64 KiB R_X 200x150 off a 64 KiB boundary, 3 mips", TextureTileMode::kR64KBX, 200, 150, 3, 4096},
        {"4 KiB S 100x70, 2 mips", TextureTileMode::kStandard4KB, 100, 70, 2, 0},
        {"linear 200x150", TextureTileMode::kLinear, 200, 150, 1, 0},
        {"64 KiB R_X 512x512, 3 mips, no padding below the tail", TextureTileMode::kR64KBX, 512, 512, 3, 0, true},
        {"64 KiB R_X 1000x700", TextureTileMode::kR64KBX, 1000, 700, 1, 0, true},
        {"64 KiB R_X 1900x1100, across two 8 MiB slabs", TextureTileMode::kR64KBX, 1900, 1100, 1, 0, true},
    };
    const auto pattern = [](std::size_t i) { return static_cast<std::uint8_t>(i * 131u + 7u); };
    constexpr std::array<std::array<std::uint8_t, 4>, 2> colours{{{255, 0, 0, 255}, {0, 255, 0, 255}}};
    std::string failures;
    for (const auto& test : cases) {
        const std::string what = std::string(watched ? "(watched) " : "") + test.name;
        GuestTextureResource resource{};
        resource.width = test.width;
        resource.height = test.height;
        resource.mipCount = test.mipCount;
        resource.tileMode = test.tileMode;
        resource.dimension = TextureDimension::k2D;
        resource.format = 56;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto geometry = DescribeSurface(resource);
        const auto surfaceBytes = static_cast<std::size_t>(geometry.guestBytes);
        std::size_t texelCount = 0;
        for (const auto& mip : geometry.mips) texelCount += static_cast<std::size_t>(mip.width) * mip.height;
        // The surface, then a 64 KiB unit past it that no store may reach.
        const auto bytes = (static_cast<std::size_t>(test.offset) + surfaceBytes + 65535) / 65536 * 65536 + 65536;
        void* block = nullptr;
        if (watched) {
            block = AllocateWatched(bytes, 65536);
            if (block == nullptr) {
                std::cout << "no write watching: write-back padding in watched memory not tested\n";
                return;
            }
        } else {
#ifdef _WIN32
            block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
            block = std::aligned_alloc(65536, bytes);
#endif
        }
        Require(block != nullptr, "cannot allocate the write-back padding block");
        auto* memory = static_cast<std::uint8_t*>(block);
        for (std::size_t i = 0; i < bytes; ++i) memory[i] = pattern(i);
        const auto blockAddress = reinterpret_cast<std::uint64_t>(block);
        const auto address = blockAddress + test.offset;
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(block, bytes, true, true, true);
        }
        struct Unregister {
            const Context& context;
            void* block;
            std::uint64_t address;
            std::size_t bytes;
            bool watched;
            ~Unregister() {
                {
                    GuestAllocations::Mutation mutation;
                    mutation.Remove(block);
                }
                HostImportFor(context, address, bytes);
                if (watched) ReleaseWatched(block, bytes);
            }
        } unregister{base, block, blockAddress, bytes, watched};
        if (HostImportFor(base, blockAddress, bytes) == nullptr) {
            std::cout << "host import of the write-back padding block refused: write-back padding not tested\n";
            return;
        }
        TextureDetiler detiler(base);
        auto context = base;
        context.detiler = &detiler;
        resource.baseAddress = address;
        {
            auto image = StorageTexture::Create(context, detiler, resource, 0);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, test.mipCount, 0, 1};
            for (std::size_t pass = 0; pass < (test.direct ? colours.size() : 1u); ++pass) {
                const auto& colour = colours[pass];
                const auto commands = recorder.Commands();
                recorder.Keep(image);
                const VkClearColorValue value{{colour[0] / 255.0f, colour[1] / 255.0f, colour[2] / 255.0f, colour[3] / 255.0f}};
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
                context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
                image->MarkDirty();
                const auto slabRetilesBefore = StorageTexture::SlabRetiles();
                image->WriteBack();
                recorder.Submit();
                device.WaitQueue();
                recorder.Sync();
                const auto slabRetiled = StorageTexture::SlabRetiles() - slabRetilesBefore;
                if (test.direct && watched && UnitShadowEnabled() && AgcDriver::GuestMemory::Watched(address, surfaceBytes) && slabRetiled == 0) failures += what + ": no window was retiled straight into its unit-shadow slab\n";
                std::vector<std::byte> read(bytes);
                if (watched) AgcDriver::GuestMemory::Read(blockAddress, read);
                else std::memcpy(read.data(), memory, bytes);
                std::size_t texels = 0, garbage = 0, outside = 0;
                for (std::size_t i = 0; i < bytes; i += 4) {
                    const bool inside = i >= test.offset && i < test.offset + surfaceBytes;
                    bool isTexel = inside, isPattern = true;
                    for (std::size_t c = 0; c < 4; ++c) {
                        const auto byte = std::to_integer<std::uint8_t>(read[i + c]);
                        isTexel = isTexel && byte == colour[c];
                        isPattern = isPattern && byte == pattern(i + c);
                    }
                    if (isTexel) ++texels;
                    else if (!isPattern) ++(inside ? garbage : outside);
                }
                if (garbage != 0 || outside != 0 || texels != texelCount) {
                    failures += what + ", write-back " + std::to_string(pass + 1) + ": stored " + std::to_string(texels) + " texels of " + std::to_string(texelCount) + " and changed " + std::to_string(garbage) + " words no texel holds (" + std::to_string(outside) + " past the surface)\n";
                }
            }
        }
        recorder.Sync();
    }
    if (!failures.empty()) throw std::runtime_error("write-back padding:\n" + failures);
    std::cout << "write-back padding" << (watched ? " (watched)" : "") << ": ok\n";
}

void splitWriteBackTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0 || 4096 % base.hostImportAlignment != 0) {
        std::cout << "host imports unavailable: the split write-back not tested\n";
        return;
    }
    struct Case {
        const char* name;
        TextureTileMode tileMode;
        std::uint32_t mipCount;
        std::uint64_t offset;
        std::uint64_t split;
        bool gpu;
        bool direct;
        bool twoAllocations;
        bool secondStore;
    };
    const Case cases[] = {
        {"64 KiB R_X 200x150, 3 mips, ranges split at 128 KiB", TextureTileMode::kR64KBX, 3, 0, 131072, true, true, false, false},
        {"64 KiB R_X 200x150, 3 mips, off a 64 KiB boundary, ranges split at 132 KiB", TextureTileMode::kR64KBX, 3, 4096, 135168, true, true, false, false},
        {"linear 200x150, ranges split at 64 KiB", TextureTileMode::kLinear, 1, 0, 65536, true, true, false, false},
        {"64 KiB R_X 200x150, 3 mips, second range of another access", TextureTileMode::kR64KBX, 3, 0, 131072, false, false, false, false},
        {"64 KiB R_X 200x150, 3 mips, ranges split inside a 64 KiB unit", TextureTileMode::kR64KBX, 3, 0, 135168, true, !UnitShadowEnabled(), false, false},
        {"64 KiB R_X 200x150, 3 mips, two adjacent allocations split at 128 KiB", TextureTileMode::kR64KBX, 3, 0, 131072, true, true, true, false},
        {"64 KiB R_X 200x150, 3 mips, ranges split at 128 KiB, a second store outside the write watch", TextureTileMode::kR64KBX, 3, 0, 131072, true, true, false, true},
        {"linear 200x150, ranges split at 64 KiB, a second store outside the write watch", TextureTileMode::kLinear, 1, 0, 65536, true, true, false, true},
    };
    SetHostImportWindow(64ull << 10u);
    struct Window {
        ~Window() { SetHostImportWindow(0); }
    } window;
    const auto pattern = [](std::size_t i) { return static_cast<std::uint8_t>(i * 131u + 7u); };
    constexpr std::array<std::uint8_t, 4> red{255, 0, 0, 255};
    std::string failures;
    for (const auto& test : cases) {
        GuestTextureResource resource{};
        resource.width = 200;
        resource.height = 150;
        resource.mipCount = test.mipCount;
        resource.tileMode = test.tileMode;
        resource.dimension = TextureDimension::k2D;
        resource.format = 56;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto geometry = DescribeSurface(resource);
        const auto surfaceBytes = static_cast<std::size_t>(geometry.guestBytes);
        Require(test.offset < test.split && test.split < test.offset + surfaceBytes, "the split test's ranges do not divide its surface");
        std::size_t texelCount = 0;
        for (const auto& mip : geometry.mips) texelCount += static_cast<std::size_t>(mip.width) * mip.height;
        const auto bytes = (static_cast<std::size_t>(test.offset) + surfaceBytes + 65535) / 65536 * 65536 + 65536;
#ifdef _WIN32
        void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        void* block = std::aligned_alloc(65536, bytes);
#endif
        Require(block != nullptr, "cannot allocate the split write-back block");
        auto* memory = static_cast<std::uint8_t*>(block);
        for (std::size_t i = 0; i < bytes; ++i) memory[i] = pattern(i);
        const auto blockAddress = reinterpret_cast<std::uint64_t>(block);
        const auto address = blockAddress + test.offset;
        {
            GuestAllocations::Mutation mutation;
            if (test.twoAllocations) {
                mutation.Add(block, static_cast<std::size_t>(test.split), true, true, true);
                mutation.Add(memory + test.split, static_cast<std::size_t>(bytes - test.split), true, true, test.gpu);
            } else {
                mutation.Add(block, bytes, true, true, true);
                mutation.Protect(memory + test.split, static_cast<std::size_t>(bytes - test.split), true, true, test.gpu, [] {});
            }
        }
        struct Unregister {
            const Context& context;
            void* block;
            std::uint64_t address;
            std::size_t bytes;
            void* second;
            ~Unregister() {
                {
                    GuestAllocations::Mutation mutation;
                    mutation.Remove(block);
                    if (second != nullptr) mutation.Remove(second);
                }
                HostImportFor(context, address, bytes);
#ifdef _WIN32
                VirtualFree(block, 0, MEM_RELEASE);
#else
                std::free(block);
#endif
            }
        } unregister{base, block, blockAddress, bytes, test.twoAllocations ? static_cast<void*>(memory + test.split) : nullptr};
        Require(HostImportFor(base, address, surfaceBytes) == nullptr, std::string(test.name) + ": one import covers both ranges");
        TextureDetiler detiler(base);
        auto context = base;
        context.detiler = &detiler;
        resource.baseAddress = address;
        {
            auto image = StorageTexture::Create(context, detiler, resource, 0);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, test.mipCount, 0, 1};
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            const VkClearColorValue value{{1.0f, 0.0f, 0.0f, 1.0f}};
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
            image->WriteBack();
            const bool direct = !image->GuestSnapshotValid();
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            std::size_t texels = 0, garbage = 0, outside = 0;
            for (std::size_t i = 0; i < bytes; i += 4) {
                const bool inside = i >= test.offset && i < test.offset + surfaceBytes;
                bool isTexel = inside, isPattern = true;
                for (std::size_t c = 0; c < 4; ++c) {
                    isTexel = isTexel && memory[i + c] == red[c];
                    isPattern = isPattern && memory[i + c] == pattern(i + c);
                }
                if (isTexel) ++texels;
                else if (!isPattern) ++(inside ? garbage : outside);
            }
            if (direct != test.direct) failures += std::string(test.name) + ": the write-back took the " + (direct ? "GPU-direct" : "CPU") + " path\n";
            if (direct && !(HostImportCovers(base, blockAddress, static_cast<std::size_t>(test.split)) && HostImportCovers(base, blockAddress + test.split, static_cast<std::size_t>(bytes - test.split)))) failures += std::string(test.name) + ": a range has no import after the GPU-direct write-back\n";
            if (garbage != 0 || outside != 0 || texels != texelCount) failures += std::string(test.name) + ": the write-back stored " + std::to_string(texels) + " texels of " + std::to_string(texelCount) + " and changed " + std::to_string(garbage) + " words no texel holds (" + std::to_string(outside) + " past the surface)\n";
            else if (test.secondStore) {
                const auto cpuPage = static_cast<std::size_t>(test.split) + 16384;
                Require(cpuPage + 4096 <= test.offset + surfaceBytes, std::string(test.name) + ": the CPU's page lies past the surface");
                if (AgcDriver::GuestMemory::Watched(address, surfaceBytes)) failures += std::string(test.name) + ": the surface is write-watched\n";
                if (!image->GuestSnapshotValid()) failures += std::string(test.name) + ": the first write-back left no saved bytes\n";
                for (std::size_t i = cpuPage; i < cpuPage + 4096; ++i) memory[i] = static_cast<std::uint8_t>(~pattern(i));
                const auto again = recorder.Commands();
                const VkClearColorValue greenValue{{0.0f, 1.0f, 0.0f, 1.0f}};
                RecordMemoryBarrier(context, again, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
                context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(again, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &greenValue, 1, &range);
                RecordMemoryBarrier(context, again, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
                image->MarkDirty();
                image->WriteBack();
                recorder.Submit();
                device.WaitQueue();
                recorder.Sync();
                constexpr std::array<std::uint8_t, 4> green{0, 255, 0, 255};
                std::size_t kept = 0, greenBelow = 0, greenAbove = 0;
                for (std::size_t i = cpuPage; i < cpuPage + 4096; ++i) kept += memory[i] == static_cast<std::uint8_t>(~pattern(i)) ? 1 : 0;
                for (std::size_t i = test.offset; i < test.offset + surfaceBytes; i += 4) {
                    if (i >= cpuPage && i < cpuPage + 4096) continue;
                    if (std::equal(green.begin(), green.end(), memory + i)) ++(i < test.split ? greenBelow : greenAbove);
                }
                if (kept != 4096) failures += std::string(test.name) + ": the second write-back overwrote " + std::to_string(4096 - kept) + " of the 4096 bytes the CPU wrote after the first\n";
                if (greenBelow == 0 || greenAbove == 0) failures += std::string(test.name) + ": the second write-back stored " + std::to_string(greenBelow) + " texels below the split and " + std::to_string(greenAbove) + " above it\n";
            }
        }
        recorder.Sync();
    }
    if (!failures.empty()) throw std::runtime_error("split write-back:\n" + failures);
    std::cout << "split write-back: ok\n";
}

void writeBackBlockTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0 || 65536 % base.hostImportAlignment != 0) {
        std::cout << "host imports unavailable: write-back blocks not tested\n";
        return;
    }
    constexpr std::size_t piece = 65536;
    constexpr std::size_t window = std::size_t{8} << 20u;
    struct Case {
        const char* name;
        std::size_t offset;
        std::size_t registeredEnd;
        bool alternate;
        bool direct;
        std::size_t imports;
    };
    const Case cases[] = {
        {"a 4 MiB image over 64 adjacent 64 KiB pieces", 0, 4u << 20u, false, true, 1},
        {"a 4 MiB image at 64 KiB over 66 adjacent 64 KiB pieces", piece, (4u << 20u) + 2 * piece, false, true, 2},
        {"a 4 MiB image over 64 KiB pieces of which every other one is not GPU mapped", 0, 4u << 20u, true, false, 0},
    };
    SetHostImportWindow(window);
    struct Window {
        ~Window() { SetHostImportWindow(0); }
    } restore;
    const auto pattern = [](std::size_t i) { return static_cast<std::uint8_t>(i * 131u + 7u); };
    constexpr std::array<std::uint8_t, 4> red{255, 0, 0, 255};
    std::string failures;
    for (const auto& test : cases) {
        GuestTextureResource resource{};
        resource.width = 1024;
        resource.height = 1024;
        resource.mipCount = 1;
        resource.tileMode = TextureTileMode::kLinear;
        resource.dimension = TextureDimension::k2D;
        resource.format = 56;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto surfaceBytes = static_cast<std::size_t>(DescribeSurface(resource).guestBytes);
        Require(surfaceBytes == (4u << 20u) && test.offset + surfaceBytes <= test.registeredEnd, std::string(test.name) + ": the surface does not fit its pieces");
#ifdef _WIN32
        void* reserved = VirtualAlloc(nullptr, 2 * window, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        void* reserved = std::aligned_alloc(window, 2 * window);
#endif
        Require(reserved != nullptr, "cannot allocate the write-back block");
        auto* memory = static_cast<std::uint8_t*>(reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(reserved) + window - 1) / window * window));
        for (std::size_t i = 0; i < window; ++i) memory[i] = pattern(i);
        std::vector<std::size_t> pieces;
        for (std::size_t offset = 0; offset < test.registeredEnd; offset += piece) pieces.push_back(offset);
        {
            GuestAllocations::Mutation mutation;
            for (const auto offset : pieces) mutation.Add(memory + offset, piece, true, true, !test.alternate || offset / piece % 2 == 0);
        }
        ClearHostImports(base.device);
        const auto address = reinterpret_cast<std::uint64_t>(memory) + test.offset;
        const auto before = HostImportCounters();
        TextureDetiler detiler(base);
        auto context = base;
        context.detiler = &detiler;
        resource.baseAddress = address;
        std::size_t texels = 0;
        bool direct = false;
        {
            auto image = StorageTexture::Create(context, detiler, resource, 0);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            const VkClearColorValue value{{1.0f, 0.0f, 0.0f, 1.0f}};
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
            image->WriteBack();
            direct = !image->GuestSnapshotValid();
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            for (std::size_t i = test.offset; i < test.offset + surfaceBytes; i += 4) texels += std::equal(red.begin(), red.end(), memory + i) ? 1 : 0;
        }
        const auto after = HostImportCounters();
        std::size_t made = 0;
        std::size_t segments = 0;
        for (std::size_t index = 0; index < ImportRequesterCount; ++index) made += after.requesters[index].made - before.requesters[index].made;
        segments = after.requesters[static_cast<std::size_t>(ImportRequester::Segment)].made - before.requesters[static_cast<std::size_t>(ImportRequester::Segment)].made;
        std::printf("%s: %zu imports made (%zu by write-back segments), %s write-back, %zu of %zu texels stored\n", test.name, made, segments, direct ? "GPU-direct" : "CPU", texels, surfaceBytes / 4);
        if (made != test.imports) failures += std::string(test.name) + ": the write-back made " + std::to_string(made) + " imports, expected " + std::to_string(test.imports) + "\n";
        if (direct != test.direct) failures += std::string(test.name) + ": the write-back took the " + (direct ? "GPU-direct" : "CPU") + " path\n";
        if (texels != surfaceBytes / 4) failures += std::string(test.name) + ": the write-back stored " + std::to_string(texels) + " of " + std::to_string(surfaceBytes / 4) + " texels\n";
        {
            GuestAllocations::Mutation mutation;
            for (const auto offset : pieces) mutation.Remove(memory + offset);
        }
        ClearHostImports(base.device);
        recorder.Sync();
#ifdef _WIN32
        VirtualFree(reserved, 0, MEM_RELEASE);
#else
        std::free(reserved);
#endif
    }
    if (!failures.empty()) throw std::runtime_error("write-back blocks:\n" + failures);
    std::cout << "write-back blocks: ok\n";
}

void singlePassTests(const Device& device, Recorder& recorder, bool watched) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0 || !base.singlePassStorage) {
        std::cout << "host imports or storage access without a format unavailable: single-pass storage moves not tested\n";
        return;
    }
    struct Case {
        const char* name;
        std::uint32_t format;
        TextureTileMode tileMode;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t mipCount;
        std::uint32_t pipeBankXor = 0;
    };
    constexpr Case cases[] = {
        {"R8 64 KiB R_X 200x150", 1, TextureTileMode::kR64KBX, 200, 150, 1},
        {"R16 64 KiB R_X 200x150, 3 mips", 7, TextureTileMode::kR64KBX, 200, 150, 3},
        {"RGBA8 64 KiB R_X 520x260, 6 mips", 56, TextureTileMode::kR64KBX, 520, 260, 6},
        {"RGBA16F 64 KiB R_X 384x256", 71, TextureTileMode::kR64KBX, 384, 256, 1},
        {"RGBA16F 64 KiB R_X 256x200, 4 mips", 71, TextureTileMode::kR64KBX, 256, 200, 4},
        {"RGBA32F 64 KiB R_X 136x72, 2 mips", 77, TextureTileMode::kR64KBX, 136, 72, 2},
        {"RGBA8 64 KiB S 300x170", 56, TextureTileMode::kStandard64KB, 300, 170, 1},
        {"RGBA16F 4 KiB S 100x70, 2 mips", 71, TextureTileMode::kStandard4KB, 100, 70, 2},
        {"R8 256 B S 90x40", 1, TextureTileMode::kStandard256B, 90, 40, 1},
        {"RGBA8 64 KiB R_X 520x260, 2 mips, pipe/bank XOR 5", 56, TextureTileMode::kR64KBX, 520, 260, 2, 5},
    };
    std::string failures;
    std::size_t tested = 0;
    for (const auto& test : cases) {
        if (!StorageFormatAvailable(base, test.format)) continue;
        const std::string what = std::string(watched ? "(watched) " : "") + test.name;
        GuestTextureResource resource{};
        resource.width = test.width;
        resource.height = test.height;
        resource.mipCount = test.mipCount;
        resource.tileMode = test.tileMode;
        resource.dimension = TextureDimension::k2D;
        resource.format = test.format;
        resource.pipeBankXor = test.pipeBankXor;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto geometry = DescribeSurface(resource);
        const auto elementBytes = BytesPerElement(test.format);
        const auto bytes = (static_cast<std::size_t>(geometry.guestBytes) + 65535) / 65536 * 65536 + 65536;
        struct Result {
            std::vector<std::uint8_t> uploaded, memory, reuploaded;
            std::uint64_t moves = 0;
        };
        std::array<Result, 2> results;
        bool skipped = false;
        for (int mode = 0; mode < 2 && !skipped; ++mode) {
            auto context = base;
            context.singlePassStorage = mode == 1;
            void* block = nullptr;
            if (watched) {
                block = AllocateWatched(bytes, 65536);
            } else {
#ifdef _WIN32
                block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
                block = std::aligned_alloc(65536, bytes);
#endif
            }
            if (block == nullptr) {
                std::cout << "no write watching: single-pass storage moves in watched memory not tested\n";
                return;
            }
            auto* memory = static_cast<std::uint8_t*>(block);
            std::uint32_t seed = 0x13579bdfu;
            const auto random = [&] {
                seed ^= seed << 13;
                seed ^= seed >> 17;
                seed ^= seed << 5;
                return static_cast<std::uint8_t>(seed >> 9);
            };
            for (std::size_t i = 0; i < bytes; ++i) memory[i] = random();
            const auto address = reinterpret_cast<std::uint64_t>(block);
            {
                GuestAllocations::Mutation mutation;
                mutation.Add(block, bytes, true, true, true);
            }
            struct Unregister {
                const Context& context;
                void* block;
                std::uint64_t address;
                std::size_t bytes;
                bool watched;
                ~Unregister() {
                    {
                        GuestAllocations::Mutation mutation;
                        mutation.Remove(block);
                    }
                    HostImportFor(context, address, bytes);
                    if (watched) ReleaseWatched(block, bytes);
#ifdef _WIN32
                    else VirtualFree(block, 0, MEM_RELEASE);
#else
                    else std::free(block);
#endif
                }
            } unregister{base, block, address, bytes, watched};
            if (HostImportFor(base, address, bytes) == nullptr) {
                std::cout << "host import refused: single-pass storage moves not tested\n";
                return;
            }
            TextureDetiler detiler(base);
            context.detiler = &detiler;
            resource.baseAddress = address;
            std::size_t linearBytes = 0;
            for (const auto& mip : geometry.mips) linearBytes += static_cast<std::size_t>(mip.width) * mip.height * elementBytes;
            const auto readImage = [&](const StorageTexture& image) {
                Buffer readback(context, linearBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                const auto commands = recorder.Commands();
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                std::vector<VkBufferImageCopy> copies;
                std::size_t at = 0;
                for (std::uint32_t level = 0; level < geometry.mips.size(); ++level) {
                    VkBufferImageCopy copy{};
                    copy.bufferOffset = at;
                    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
                    copy.imageExtent = {geometry.mips[level].width, geometry.mips[level].height, 1};
                    copies.push_back(copy);
                    at += static_cast<std::size_t>(geometry.mips[level].width) * geometry.mips[level].height * elementBytes;
                }
                context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image.Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), static_cast<std::uint32_t>(copies.size()), copies.data());
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
                recorder.Submit();
                device.WaitQueue();
                recorder.Sync();
                const auto read = readback.Bytes();
                return std::vector<std::uint8_t>(reinterpret_cast<const std::uint8_t*>(read.data()), reinterpret_cast<const std::uint8_t*>(read.data()) + linearBytes);
            };
            std::vector<std::uint8_t> texels(linearBytes);
            for (auto& value : texels) value = random();
            const auto movesBefore = StorageTexture::SinglePassMoves();
            {
                auto image = StorageTexture::Create(context, detiler, resource, 0);
                recorder.Keep(image);
                results[mode].uploaded = readImage(*image);
                Buffer staging(context, linearBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                std::memcpy(staging.Bytes().data(), texels.data(), linearBytes);
                const auto commands = recorder.Commands();
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
                std::vector<VkBufferImageCopy> copies;
                std::size_t at = 0;
                for (std::uint32_t level = 0; level < geometry.mips.size(); ++level) {
                    VkBufferImageCopy copy{};
                    copy.bufferOffset = at;
                    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
                    copy.imageExtent = {geometry.mips[level].width, geometry.mips[level].height, 1};
                    copies.push_back(copy);
                    at += static_cast<std::size_t>(geometry.mips[level].width) * geometry.mips[level].height * elementBytes;
                }
                context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, staging.Handle(), image->Image(), VK_IMAGE_LAYOUT_GENERAL, static_cast<std::uint32_t>(copies.size()), copies.data());
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
                image->MarkDirty();
                image->WriteBack();
                recorder.Submit();
                device.WaitQueue();
                recorder.Sync();
                auto again = StorageTexture::Create(context, detiler, resource, 0);
                recorder.Keep(again);
                results[mode].reuploaded = readImage(*again);
                std::vector<std::byte> read(bytes);
                if (watched) AgcDriver::GuestMemory::Read(address, read);
                else {
                    StorageTexture::FlushPending(address, bytes, nullptr, "test", PublishScope::Whole);
                    recorder.Submit();
                    device.WaitQueue();
                    recorder.Sync();
                    std::memcpy(read.data(), memory, bytes);
                }
                results[mode].memory.assign(reinterpret_cast<const std::uint8_t*>(read.data()), reinterpret_cast<const std::uint8_t*>(read.data()) + bytes);
            }
            recorder.Sync();
            results[mode].moves = StorageTexture::SinglePassMoves() - movesBefore;
            if (results[mode].reuploaded != texels) failures += what + (mode == 1 ? " (single pass)" : " (copies)") + ": the second image does not hold the texels written back\n";
        }
        if (results[0].moves != 0 || results[1].moves < 3) failures += what + ": single-pass moves " + std::to_string(results[0].moves) + " with the copies and " + std::to_string(results[1].moves) + " in single pass (want 0 and at least 3)\n";
        if (results[0].uploaded != results[1].uploaded) failures += what + ": the first upload differs between the copies and single pass\n";
        if (results[0].memory != results[1].memory) {
            std::size_t differ = 0;
            for (std::size_t i = 0; i < results[0].memory.size(); ++i) differ += results[0].memory[i] != results[1].memory[i];
            failures += what + ": guest memory after the write-back differs in " + std::to_string(differ) + " bytes between the copies and single pass\n";
        }
        ++tested;
    }
    if (!failures.empty()) throw std::runtime_error("single-pass storage moves:\n" + failures);
    std::cout << "single-pass storage moves" << (watched ? " (watched)" : "") << ": ok (" << tested << " surfaces)\n";
}

class StorageUploadProbe {
public:
    StorageUploadProbe(Context& context, bool failView) : context(context), resolver(context.deviceProc), failView(failView) {
        Require(active == nullptr, "nested storage upload probe");
        active = this;
        context.deviceProc = resolve;
    }
    ~StorageUploadProbe() {
        for (const auto view : destroyedViews) function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (imageDestroyed) function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memoryFreed) function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
        context.deviceProc = resolver;
        active = nullptr;
    }
    VkImage Image() const { return image; }
    bool Destroyed() const { return imageDestroyed || memoryFreed; }
    bool Released() const { return imageDestroyed && memoryFreed; }
    bool FailedView() const { return failedView; }
    std::uint32_t Clears() const { return clears; }

private:
    template<typename TFunction>
    TFunction function(const char* name) const { return reinterpret_cast<TFunction>(resolver(context.device, name)); }
    static VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice device, const VkImageCreateInfo* info, const VkAllocationCallbacks* callbacks, VkImage* created) {
        const auto result = active->function<PFN_vkCreateImage>("vkCreateImage")(device, info, callbacks, created);
        if (result == VK_SUCCESS) active->image = *created;
        return result;
    }
    static VKAPI_ATTR VkResult VKAPI_CALL bindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset) {
        const auto result = active->function<PFN_vkBindImageMemory>("vkBindImageMemory")(device, image, memory, offset);
        if (result == VK_SUCCESS && image == active->image) active->memory = memory;
        return result;
    }
    static VKAPI_ATTR VkResult VKAPI_CALL createView(VkDevice device, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* callbacks, VkImageView* created) {
        if (info->image == active->image && active->failView && active->clears != 0) {
            active->failedView = true;
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        const auto result = active->function<PFN_vkCreateImageView>("vkCreateImageView")(device, info, callbacks, created);
        if (result == VK_SUCCESS && info->image == active->image) active->views.push_back(*created);
        return result;
    }
    static VKAPI_ATTR void VKAPI_CALL destroyView(VkDevice device, VkImageView view, const VkAllocationCallbacks* callbacks) {
        if (std::find(active->views.begin(), active->views.end(), view) != active->views.end()) active->destroyedViews.push_back(view);
        else active->function<PFN_vkDestroyImageView>("vkDestroyImageView")(device, view, callbacks);
    }
    static VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* callbacks) {
        if (image == active->image) active->imageDestroyed = true;
        else active->function<PFN_vkDestroyImage>("vkDestroyImage")(device, image, callbacks);
    }
    static VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* callbacks) {
        if (memory == active->memory) active->memoryFreed = true;
        else active->function<PFN_vkFreeMemory>("vkFreeMemory")(device, memory, callbacks);
    }
    static VKAPI_ATTR void VKAPI_CALL clearImage(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearColorValue* value, std::uint32_t count, const VkImageSubresourceRange* ranges) {
        if (image == active->image) ++active->clears;
        active->function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image, layout, value, count, ranges);
    }
    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL resolve(VkDevice device, const char* name) {
        if (std::strcmp(name, "vkCreateImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(createImage);
        if (std::strcmp(name, "vkBindImageMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(bindImageMemory);
        if (std::strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(createView);
        if (std::strcmp(name, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(destroyView);
        if (std::strcmp(name, "vkDestroyImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(destroyImage);
        if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(freeMemory);
        if (std::strcmp(name, "vkCmdClearColorImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(clearImage);
        return active->resolver(device, name);
    }
    Context& context;
    PFN_vkGetDeviceProcAddr resolver;
    bool failView;
    bool failedView = false;
    bool imageDestroyed = false;
    bool memoryFreed = false;
    std::uint32_t clears = 0;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::vector<VkImageView> views;
    std::vector<VkImageView> destroyedViews;
    static inline StorageUploadProbe* active = nullptr;
};

void storageUploadLifetimeTests(const Device& device, Recorder& previous, bool failView) {
    auto context = device.GetContext();
    context.hostImportAlignment = 0;
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.width = 64;
    resource.height = 64;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    const auto guestBytes = static_cast<std::size_t>(DescribeSurface(resource).guestBytes);
    std::vector<std::byte> texels(guestBytes + 256, std::byte{0x55});
    std::vector<std::byte> keys(guestBytes / 256, std::byte{0});
    resource.baseAddress = (reinterpret_cast<std::uintptr_t>(texels.data()) + 255) & ~std::uintptr_t{255};
    resource.dccAddress = reinterpret_cast<std::uintptr_t>(keys.data());
    DccKeyProof proof;
    Require(ProvedClearKeys(resource, guestBytes, proof) == DccKeys::Clear0000, "storage lifetime fixture has no DCC clear");
    StorageUploadProbe probe(context, failView);
    Buffer readback(device.GetContext(), resource.width * resource.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::memset(readback.Bytes().data(), 0x55, readback.Bytes().size());
    std::weak_ptr<StorageTexture> lifetime;
    bool premature = false;
    bool retained = true;
    bool threw = false;
    {
        Recorder recorder(context);
        recorder.Activate();
        try {
            auto image = StorageTexture::Create(context, detiler, resource, 0);
            lifetime = image;
            image.reset();
            retained = !lifetime.expired();
        } catch (const std::exception& error) {
            if (!probe.FailedView() || std::string_view(error.what()).find("vkCreateImageView storage") == std::string_view::npos) throw;
            threw = true;
        }
        premature = probe.Destroyed();
        Require(probe.Clears() == 1 && recorder.Recording(), "storage creation did not record its initial clear");
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {resource.width, resource.height, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, probe.Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Sync();
    }
    previous.Activate();
    Require(threw == failView && probe.FailedView() == failView, "storage view failure injection did not take the requested path");
    Require(std::all_of(readback.Bytes().begin(), readback.Bytes().end(), [](std::byte value) { return value == std::byte{0}; }), "recorded storage clear did not reach every texel");
    Require(lifetime.expired() && probe.Released(), "storage upload resources outlived recorder teardown");
    Require(!premature && retained, failView ? "failed storage creation released an image still referenced by its recorded clear" : "storage creation did not retain its image through the recorded clear");
    std::cout << "Storage upload lifetime" << (failView ? " after view failure" : " after caller release") << ": passed\n";
}

void refreshProofTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: refresh proofs not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
    void* block = AllocateWatched(bytes, 65536);
    if (block == nullptr) {
        std::cout << "no write watching: refresh proofs not tested\n";
        return;
    }
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* keys = texels + surfaceBytes;
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(keys, 0xff, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr || !AgcDriver::GuestMemory::Watched(address, bytes)) {
        std::cout << "no watched host import: refresh proofs not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = address + surfaceBytes;
    const auto proved = [] { return StorageTexture::RefreshesProved(); };
    {
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto draw = [&](VkClearColorValue value) {
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
        };
        const auto settle = [&] {
            image->Refresh();
            image->Refresh();
        };
        const auto expectProved = [&](const char* what) {
            const auto before = proved();
            Require(image->Refresh(), what);
            Require(proved() == before + 1, what);
        };
        const auto expectFull = [&](const char* what) {
            const auto before = proved();
            image->Refresh();
            Require(proved() == before, what);
        };
        settle();
        expectProved("an unchanged surface was not proved current");
        expectProved("a proved surface was not proved again");
        draw({{1.0f, 0.0f, 0.0f, 1.0f}});
        settle();
        expectProved("a target holding its own pending results was not proved current");
        draw({{0.0f, 1.0f, 0.0f, 1.0f}});
        expectProved("a draw into an already pending target broke its proof");
        texels[0] = 0x11;
        expectFull("a CPU write of the surface was answered from the proof");
        settle();
        expectProved("the proof was not taken again after a CPU write");
        AgcDriver::GuestMemory::MarkWritten(address + 65536, 4096);
        expectFull("a driver store into the surface was answered from the proof");
        settle();
        AgcDriver::GuestMemory::MarkWritten(resource.dccAddress, keyCount);
        expectFull("a key store over the surface's keys was answered from the proof");
        settle();
        expectProved("the proof was not taken again after a key store");
        auto other = resource;
        other.dccAddress = 0;
        auto pending = StorageTexture::Create(context, detiler, other, 0);
        recorder.Keep(pending);
        pending->MarkDirty();
        expectFull("another image's pending results over the surface were answered from the proof");
        settle();
        expectProved("the proof was not taken again after another image's results were stored");
        pending->MarkDirty();
        expectFull("another image's pending results were answered from the proof");
        Require(!AgcDriver::Graphics::PendingStorageOverlaps(address, surfaceBytes, image.get()), "the full refresh left another image's results pending");
        auto fresh = StorageTexture::Create(context, detiler, resource, 0);
        const auto before = proved();
        fresh->Refresh();
        Require(proved() == before, "a new image of the surface was answered from another image's proof");
        std::memset(keys, 0x00, keyCount);
        expectFull("a fast clear of the keys was answered from the proof");
        recorder.Sync();
    }
    recorder.Sync();
}

void unchangedCpuStampTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::uint64_t offset = 0x2000;
    constexpr std::size_t bytes = 6 * 65536;
    constexpr std::array<std::uint8_t, 4> red{255, 0, 0, 255};
    const auto pattern = [](std::size_t i) { return static_cast<std::uint8_t>(i * 131u + 7u); };
    for (const bool imported : {false, true}) {
#ifdef _WIN32
        if (imported) {
            std::cout << "unchanged CPU stamps over a watched host import: Linux write watch only\n";
            continue;
        }
#endif
        if (imported && base.hostImportAlignment == 0) {
            std::cout << "host imports unavailable: unchanged CPU stamps over a watched host import not tested\n";
            continue;
        }
        const auto decided = PrepareImportWatch(base);
        if (imported) SetImportWatch(base, ImportWatch::Watch);
        struct Restore {
            const Context& context;
            ImportWatch decided;
            ~Restore() { SetImportWatch(context, decided); }
        } restore{base, decided};
        for (const bool changed : {false, true}) {
            const std::string what = std::string(changed ? "a CPU store that changed a byte" : "a CPU store that left the bytes as they were") + (imported ? " over a watched host import" : " over watched memory");
            void* block = AllocateWatched(bytes, 65536);
            if (block == nullptr) {
                std::cout << "no write watching: unchanged CPU stamps not tested\n";
                return;
            }
            auto* memory = static_cast<std::uint8_t*>(block);
            for (std::size_t i = 0; i < bytes; ++i) memory[i] = pattern(i);
            const auto blockAddress = reinterpret_cast<std::uint64_t>(block);
            const auto address = blockAddress + offset;
            {
                GuestAllocations::Mutation mutation;
                mutation.Add(block, bytes, true, true, true);
            }
            struct Unregister {
                const Context& context;
                void* block;
                std::uint64_t address;
                std::size_t bytes;
                ~Unregister() {
                    {
                        GuestAllocations::Mutation mutation;
                        mutation.Remove(block);
                    }
                    HostImportFor(context, address, bytes);
                    ReleaseWatched(block, bytes);
                }
            } unregister{base, block, blockAddress, bytes};
            AgcDriver::GuestMemory::CollectWritesUncached(blockAddress, bytes);
            TextureDetiler detiler(base);
            auto context = base;
            if (!imported) context.hostImportAlignment = 0;
            context.detiler = &detiler;
            GuestTextureResource resource{};
            resource.baseAddress = address;
            resource.width = side;
            resource.height = side;
            resource.mipCount = 1;
            resource.tileMode = TextureTileMode::kLinear;
            resource.dimension = TextureDimension::k2D;
            resource.format = 56;
            resource.dstSelX = 4;
            resource.dstSelY = 5;
            resource.dstSelZ = 6;
            resource.dstSelW = 7;
            Require(DescribeSurface(resource).guestBytes == surfaceBytes, "(c) unexpected surface size");
            {
                auto image = StorageTexture::Create(context, detiler, resource, 0);
                const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                const auto commands = recorder.Commands();
                recorder.Keep(image);
                const VkClearColorValue value{{1.0f, 0.0f, 0.0f, 1.0f}};
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
                context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
                image->MarkDirty();
                const std::array<std::pair<std::uint64_t, std::uint64_t>, 2> beside{{{address - 64, address - 16}, {address + surfaceBytes, address + surfaceBytes + 64}}};
                const std::array<std::pair<std::uint64_t, std::uint64_t>, 2> touching{{{address - 64, address - 16}, {address + surfaceBytes - 4, address + surfaceBytes}}};
                Require(!StorageTexture::AnyPendingOverlaps(beside) && StorageTexture::AnyPendingOverlaps(touching), "(c) " + what + ": a set of ranges beside the pending image or touching its last word was answered wrongly");
                for (std::size_t i = 0; i < 4096; ++i) memory[offset + i] = pattern(offset + i);
                if (changed) memory[offset + 16] = static_cast<std::uint8_t>(~pattern(offset + 16));
                AgcDriver::GuestMemory::CollectWritesUncached(blockAddress, bytes);
                Require(AgcDriver::GuestMemory::Watched(address, surfaceBytes) && (HostImportFor(context, address, surfaceBytes) != nullptr) == imported, "(c) " + what + ": the image is not on the path under test");
                image->WriteBack();
                recorder.Submit();
                device.WaitQueue();
                recorder.Sync();
                std::vector<std::byte> read(bytes);
                AgcDriver::GuestMemory::Read(blockAddress, read);
                const auto texel = [&](std::size_t at) {
                    for (std::size_t c = 0; c < 4; ++c) {
                        if (std::to_integer<std::uint8_t>(read[at + c]) != red[c]) return false;
                    }
                    return true;
                };
                const auto headEnd = static_cast<std::size_t>(65536);
                std::size_t stored = 0, total = 0;
                for (std::size_t at = offset; at < offset + surfaceBytes; at += 4) {
                    ++total;
                    if (texel(at)) ++stored;
                }
                if (!changed) {
                    if (stored != total) throw std::runtime_error(std::string("unchanged CPU stamps: ") + what + " dropped " + std::to_string(total - stored) + " of " + std::to_string(total) + " texels the GPU wrote");
                } else {
                    Require(std::to_integer<std::uint8_t>(read[offset + 16]) == static_cast<std::uint8_t>(~pattern(offset + 16)), "(c) " + what + ": the changed byte was overwritten by the GPU results");
                    for (std::size_t at = headEnd; at < offset + surfaceBytes; at += 4) Require(texel(at), "(c) " + what + ": GPU results outside the block the CPU changed were dropped");
                }
            }
            recorder.Sync();
        }
    }
    std::cout << "unchanged CPU stamps: ok\n";
}

void keptUnitSettleTests(const Device& device, Recorder& recorder) {
    Require(AgcDriver::GuestMemory::WriteWatched(), "(k) kept-unit settle tests require host write watching");
    const auto& base = device.GetContext();
    constexpr std::uint32_t width = 1024;
    constexpr std::uint32_t height = 512;
    constexpr std::size_t surfaceBytes = std::size_t{width} * height * 4;
    constexpr std::size_t unitBytes = 65536;
    constexpr std::array<std::uint8_t, 4> red{255, 0, 0, 255};
    constexpr std::array<std::uint8_t, 4> blue{0, 0, 255, 255};
    for (const bool direct : {false, true}) {
        const std::string what = direct ? "GPU-mapped direct memory" : "watched memory";
        if (direct && base.hostImportAlignment == 0) {
            std::cout << "host imports unavailable: units stored next to a unit kept for the CPU in " << what << " not tested\n";
            continue;
        }
        struct Backing {
            Recorder& recorder;
            bool direct;
            void* block = nullptr;
            std::int64_t physical = -1;
            ~Backing() {
                try {
                    recorder.Sync();
                } catch (const std::exception& error) {
                    std::cerr << "(k) the recorded work did not finish, the test memory stays mapped: " << error.what() << '\n';
                    return;
                }
                if (direct) {
                    if (block != nullptr) sceKernelMunmap(reinterpret_cast<std::uint64_t>(block), surfaceBytes);
                    if (physical >= 0) sceKernelReleaseDirectMemory(physical, surfaceBytes);
                    return;
                }
                if (block == nullptr) return;
                {
                    GuestAllocations::Mutation mutation;
                    mutation.Remove(block);
                }
                ReleaseWatched(block, surfaceBytes);
            }
        } backing{recorder, direct};
        if (direct) {
            Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, surfaceBytes, unitBytes, 0, &backing.physical) == 0, "(k) allocate the direct memory");
            Require(sceKernelMapDirectMemory(&backing.block, surfaceBytes, 0x33, 0, backing.physical, unitBytes) == 0, "(k) map the direct memory for the GPU");
        } else {
            backing.block = AllocateWatched(surfaceBytes, unitBytes);
            Require(backing.block != nullptr, "(k) no watched block although write watching is on");
            GuestAllocations::Mutation mutation;
            mutation.Add(backing.block, surfaceBytes, true, true, true);
        }
        auto* memory = static_cast<std::uint8_t*>(backing.block);
        std::memset(memory, 0x5a, surfaceBytes);
        const auto address = reinterpret_cast<std::uint64_t>(backing.block);
        AgcDriver::GuestMemory::CollectWritesUncached(address, surfaceBytes);
        TextureDetiler detiler(base);
        auto context = base;
        if (!direct) context.hostImportAlignment = 0;
        context.detiler = &detiler;
        GuestTextureResource resource{};
        resource.baseAddress = address;
        resource.width = width;
        resource.height = height;
        resource.mipCount = 1;
        resource.tileMode = TextureTileMode::kR64KBX;
        resource.dimension = TextureDimension::k2D;
        resource.format = 56;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        Require(DescribeSurface(resource).guestBytes == surfaceBytes, "(k) unexpected surface size");
        if (direct && !AgcDriver::GuestMemory::Watched(address, surfaceBytes)) {
            std::cout << "GPU-mapped direct memory is not write-watched here: units stored next to a unit kept for the CPU in " << what << " not tested\n";
            continue;
        }
        Require(AgcDriver::GuestMemory::Watched(address, surfaceBytes), "(k) " + what + " is not watched");
        if (direct && HostImportFor(context, address, surfaceBytes) == nullptr) {
            std::cout << "host import of the direct memory refused: units stored next to a unit kept for the CPU in " << what << " not tested\n";
            continue;
        }
        if (direct && !AgcDriver::GuestMemory::Watched(address, surfaceBytes)) {
            std::cout << "the host import of the direct memory took it out of the write watch (its imports are compared): units stored next to a unit kept for the CPU in " << what << " not tested\n";
            continue;
        }
        Require(direct || HostImportFor(context, address, surfaceBytes) == nullptr, "(k) watched memory was imported although host imports are off for it");
        {
            auto image = StorageTexture::Create(context, detiler, resource, 0);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            const auto clear = [&](VkClearColorValue value) {
                const auto commands = recorder.Commands();
                recorder.Keep(image);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
                context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
                image->MarkDirty();
            };
            const auto store = [&] {
                image->WriteBack();
                recorder.Submit();
                device.WaitQueue();
                recorder.Sync();
            };
            const auto unitHolds = [&](std::size_t unit, const std::array<std::uint8_t, 4>& texel) {
                std::vector<std::byte> read(unitBytes);
                AgcDriver::GuestMemory::Read(address + unit * unitBytes, read);
                for (std::size_t i = 0; i < unitBytes; ++i) {
                    if (std::to_integer<std::uint8_t>(read[i]) != texel[i % 4]) return false;
                }
                return true;
            };
            clear({{1.0f, 0.0f, 0.0f, 1.0f}});
            memory[16] = 0x77;
            AgcDriver::GuestMemory::CollectWritesUncached(address, surfaceBytes);
            store();
            std::vector<std::byte> head(unitBytes);
            AgcDriver::GuestMemory::Read(address, head);
            Require(std::to_integer<std::uint8_t>(head[16]) == 0x77, "(k) " + what + ": the CPU store into unit 0 was overwritten by results it was newer than");
            for (std::size_t unit = 1; unit < surfaceBytes / unitBytes; ++unit) Require(unitHolds(unit, red), "(k) " + what + ": the first write-back did not store unit " + std::to_string(unit));
            clear({{0.0f, 0.0f, 1.0f, 1.0f}});
            store();
            for (std::size_t unit = 1; unit < surfaceBytes / unitBytes; ++unit) {
                Require(unitHolds(unit, blue), "(k) " + what + ": unit " + std::to_string(unit) + " kept the results of the earlier write-back; that write-back's own store was taken for a CPU write because a neighbouring unit was kept for the CPU");
            }
            AgcDriver::GuestMemory::Read(address, head);
            Require(std::to_integer<std::uint8_t>(head[16]) == 0x77, "(k) " + what + ": the second write-back stored over unit 0, which the CPU wrote after that unit's generation");
        }
        std::cout << "units stored next to a unit kept for the CPU in " << what << ": ok\n";
    }
}

void settledBaselineTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
#ifdef _WIN32
    std::cout << "generation baseline over a pending store: Linux write watch only\n";
    return;
#endif
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: generation baseline over a pending store not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::uint64_t offset = 0x2000;
    constexpr std::size_t bytes = 6 * 65536;
    const auto decided = PrepareImportWatch(base);
    SetImportWatch(base, ImportWatch::Watch);
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{base, decided};
    for (const bool pending : {false, true}) {
        void* block = AllocateWatched(bytes, 65536);
        if (block == nullptr) {
            std::cout << "no write watching: generation baseline over a pending store not tested\n";
            return;
        }
        std::memset(block, 0x5a, bytes);
        const auto blockAddress = reinterpret_cast<std::uint64_t>(block);
        const auto address = blockAddress + offset;
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(block, bytes, true, true, true);
        }
        struct Unregister {
            const Context& context;
            void* block;
            std::uint64_t address;
            std::size_t bytes;
            ~Unregister() {
                {
                    GuestAllocations::Mutation mutation;
                    mutation.Remove(block);
                }
                HostImportFor(context, address, bytes);
                ReleaseWatched(block, bytes);
            }
        } unregister{base, block, blockAddress, bytes};
        AgcDriver::GuestMemory::CollectWritesUncached(blockAddress, bytes);
        TextureDetiler detiler(base);
        auto context = base;
        context.detiler = &detiler;
        GuestTextureResource resource{};
        resource.baseAddress = address;
        resource.width = side;
        resource.height = side;
        resource.mipCount = 1;
        resource.tileMode = TextureTileMode::kLinear;
        resource.dimension = TextureDimension::k2D;
        resource.format = 56;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto surfaceBytes = static_cast<std::size_t>(DescribeSurface(resource).guestBytes);
        Require(HostImportFor(context, address, surfaceBytes) != nullptr && AgcDriver::GuestMemory::Watched(address, surfaceBytes), "(b) the surface is not a watched host import");
        static_cast<void>(recorder.Commands());
        if (pending) recorder.NotePendingWrite(address + 4096, 4096);
        const auto before = recorder.Submissions();
        {
            auto image = StorageTexture::Create(context, detiler, resource, 0);
            recorder.Keep(image);
            if (pending) Require(recorder.Submissions() == before, "(b) an upload over a pending recorded store submitted the open batch to read its generation baseline");
        }
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    }
    std::cout << "generation baseline over a pending store: ok\n";
}

void importWindowTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    const auto& base = device.GetContext();
#ifdef _WIN32
    static_cast<void>(recorder);
    static_cast<void>(base);
    std::cout << "the import window: Linux write watch only\n";
#else
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: the import window not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
    void* block = AllocateWatched(bytes, 65536);
    if (block == nullptr) {
        std::cout << "no write watching: the import window not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    Require(ProbeImportWriteProtection(base).failure == nullptr, "(w) the import probe failed");
    const auto decided = PrepareImportWatch(base);
    if (decided == ImportWatch::Unwatch) {
        std::cout << "host imports are compared, not watched: the import window not tested\n";
        return;
    }
    SetImportWatch(base, ImportWatch::Watch);
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{base, decided};
    auto* texels = static_cast<std::uint8_t*>(block);
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(texels + surfaceBytes, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = address + surfaceBytes;
    {
        Require(!HostImportCovers(base, address, bytes), "(w) the block is imported before the cache takes its generation");
        const auto cached = CollectWritesUncached(address, surfaceBytes);
        Require(cached != 0 && UnchangedSince(address, surfaceBytes, cached), "(w) the cache's generation is not current before the import");
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto commands = recorder.Commands();
        recorder.Keep(image);
        const VkClearColorValue red{{1.0f, 0.0f, 0.0f, 1.0f}};
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &red, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        image->MarkDirty();
        const auto* import = HostImportFor(base, address, bytes);
        if (import == nullptr) {
            std::cout << "host import of the import window block refused: the import window not tested\n";
            recorder.Sync();
            return;
        }
        Require(!import->unwatched && Watched(address, bytes), "(w) an import under the watch decision left the watch");
        CollectWritesUncached(address, surfaceBytes);
        Require(UnchangedSince(address, surfaceBytes, cached), "(w) an import that stored nothing reads as a change to a cache taken before it");
        Require(image->Generation() != 0 && UnchangedSince(address, surfaceBytes, image->Generation()), "(w) an import that stored nothing reads as a change to the pending storage image");
        std::array<std::uint8_t, surfaceBytes / 65536> changed{};
        const std::array<std::uint64_t, surfaceBytes / 65536> generations{cached, cached, cached, cached};
        Require(ChangedBlocks(address, surfaceBytes, generations, changed) && std::all_of(changed.begin(), changed.end(), [](std::uint8_t value) { return value == BlockUnchanged; }), "(w) the pages the import resolved read as written");
        const auto after = CollectWritesUncached(address, surfaceBytes);
        Require(after != 0 && UnchangedSince(address, surfaceBytes, after), "(w) the imported range is not watched again after the import window");
        image->Refresh();
        Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto copyCommands = recorder.Commands();
        RecordMemoryBarrier(context, copyCommands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {side, side, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(copyCommands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, copyCommands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        const auto pixels = readback.Bytes();
        constexpr std::array<std::uint8_t, 4> expected{255, 0, 0, 255};
        for (std::size_t i = 0; i < pixels.size(); ++i) Require(std::to_integer<std::uint8_t>(pixels[i]) == expected[i % 4], "(w) results pending over the import window were dropped");
        std::vector<std::byte> memory(surfaceBytes);
        Read(address, memory);
        for (std::size_t i = 0; i < surfaceBytes; ++i) Require(std::to_integer<std::uint8_t>(memory[i]) == expected[i % 4], "(w) results pending over the import window did not reach guest memory");
        const auto stored = CollectWritesUncached(address, surfaceBytes);
        texels[65536 + 8] = 0x77;
        CollectWritesUncached(address, surfaceBytes);
        Require(WrittenSince(address + 65536, 65536, stored) && !WrittenSince(address, 65536, stored), "(w) a CPU store after the import window is not reported as one");
    }
    recorder.Sync();
#endif
}

void queuedWriteGroupTests() {
    using Write = Recorder::QueuedWrite;
    using Groups = std::vector<std::size_t>;
    const auto paint = [](const std::vector<Write>& writes, const Groups& groups, std::size_t bytes) {
        std::vector<std::uint8_t> memory(bytes, 0);
        std::size_t first = 0;
        for (const auto end : groups) {
            for (const bool computed : {false, true}) {
                for (auto i = first; i < end; ++i) {
                    if (writes[i].computed != computed) continue;
                    for (auto at = writes[i].begin; at < writes[i].end; ++at) memory[at] = static_cast<std::uint8_t>(i + 1);
                }
            }
            first = end;
        }
        return memory;
    };
    const auto programOrder = [](const std::vector<Write>& writes, std::size_t bytes) {
        std::vector<std::uint8_t> memory(bytes, 0);
        for (std::size_t i = 0; i < writes.size(); ++i) {
            for (auto at = writes[i].begin; at < writes[i].end; ++at) memory[at] = static_cast<std::uint8_t>(i + 1);
        }
        return memory;
    };
    const auto recorded = [](const std::vector<Write>& writes) {
        const auto order = Recorder::QueuedWriteOrder(writes);
        std::vector<Write> ordered;
        for (const auto index : order) ordered.push_back(writes[index]);
        const auto groups = Recorder::QueuedWriteGroups(ordered);
        std::vector<std::size_t> sequence;
        std::size_t first = 0;
        for (const auto end : groups) {
            for (const bool computed : {false, true}) {
                for (auto i = first; i < end; ++i) {
                    if (ordered[i].computed == computed) sequence.push_back(order[i]);
                }
            }
            first = end;
        }
        return std::pair{sequence, groups.size()};
    };
    Require(Recorder::QueuedWriteGroups({}).empty(), "no queued writes made a group");
    const std::vector<Write> flagged{{0x100, 0x200, false}, {0x100, 0x1f8, true}, {0x108, 0x200, true}, {0x200, 0x208, false}, {0x400, 0x500, false}, {0x400, 0x4f8, true}, {0x408, 0x500, true}, {0x500, 0x508, false}, {0x800, 0x804, false}};
    Require(recorded(flagged) == std::pair{std::vector<std::size_t>{0, 4, 1, 2, 5, 6, 3, 7, 8}, std::size_t{2}}, "the dumps of a run with a flag after each query slot were not folded once, with the clears before them and the flags after them");
    const std::vector<Write> open{{0x50, 0x58, false}, {0x100, 0x200, false}, {0x100, 0x1f8, true}, {0x108, 0x200, true}, {0x200, 0x208, false}, {0x400, 0x500, false}, {0x400, 0x4f8, true}};
    Require(recorded(open) == std::pair{std::vector<std::size_t>{0, 1, 5, 2, 3, 6, 4}, std::size_t{2}}, "the clear of a slot whose end dump the run does not hold was not moved ahead of the dumps");
    const std::vector<Write> overDump{{0x100, 0x200, false}, {0x100, 0x1f8, true}, {0x108, 0x200, true}, {0x1f0, 0x410, false}, {0x400, 0x4f8, true}};
    Require(Recorder::QueuedWriteOrder(overDump) == std::vector<std::size_t>{0, 1, 2, 3, 4}, "a store over an earlier dump that the later dumps cover only in part was moved ahead of that dump");
    const std::vector<Write> overFlag{{0x100, 0x200, false}, {0x100, 0x1f8, true}, {0x108, 0x200, true}, {0x500, 0x508, false}, {0x400, 0x508, false}, {0x400, 0x4f8, true}, {0x408, 0x500, true}};
    Require(Recorder::QueuedWriteOrder(overFlag) == std::vector<std::size_t>{0, 1, 2, 3, 4, 5, 6}, "a clear over an earlier store after a dump was moved ahead of that store");
    const std::vector<Write> query{{0x100, 0x200, false}, {0x100, 0x1f8, true}, {0x108, 0x200, true}, {0x200, 0x300, false}, {0x200, 0x2f8, true}, {0x208, 0x300, true}};
    Require(Recorder::QueuedWriteGroups(query) == Groups{3, 6}, "the clear of the next query slot did not wait for the dumps before it");
    const std::vector<Write> reused{{0x100, 0x1f8, true}, {0x100, 0x200, false}, {0x100, 0x1f8, true}};
    Require(Recorder::QueuedWriteGroups(reused) == Groups{1, 3}, "a store over a computed write of its group did not start a new group");
    const std::vector<Write> apart{{0x100, 0x1f8, true}, {0x1f8, 0x200, false}, {0x300, 0x310, false}};
    Require(Recorder::QueuedWriteGroups(apart) == Groups{1, 3}, "stores after the computed writes joined their group");
    const std::vector<Write> label{{0x100, 0x1f8, true}, {0x108, 0x200, true}, {0x400, 0x408, false}};
    Require(Recorder::QueuedWriteGroups(label) == Groups{2, 3}, "a label after the dumps landed before them");
    const std::vector<Write> stores{{0x100, 0x110, false}, {0x100, 0x110, false}};
    Require(Recorder::QueuedWriteGroups(stores) == Groups{2}, "stores alone were split into groups");
    std::uint64_t seed = 0x9e3779b97f4a7c15ull;
    const auto next = [&](std::uint64_t bound) {
        seed ^= seed << 13u;
        seed ^= seed >> 7u;
        seed ^= seed << 17u;
        return seed % bound;
    };
    constexpr std::size_t bytes = 64;
    for (int round = 0; round < 20000; ++round) {
        std::vector<Write> writes;
        const auto count = 1 + next(12);
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto begin = next(bytes - 1);
            const auto end = begin + 1 + next(std::min<std::uint64_t>(16, bytes - begin));
            writes.push_back({begin, std::min<std::uint64_t>(end, bytes), next(2) == 0});
        }
        const auto groups = Recorder::QueuedWriteGroups(writes);
        Require(!groups.empty() && groups.back() == writes.size() && std::is_sorted(groups.begin(), groups.end()), "queued write groups do not end at the last write in order");
        Require(paint(writes, groups, bytes) == programOrder(writes, bytes), "recording each group's stores before its computed writes changed the final bytes of the program order");
        const auto sequence = recorded(writes).first;
        std::vector<std::size_t> position(writes.size(), writes.size());
        for (std::size_t i = 0; i < sequence.size(); ++i) position[sequence[i]] = i;
        Require(sequence.size() == writes.size() && std::find(position.begin(), position.end(), writes.size()) == position.end(), "the recorded order is not a permutation of the queued writes");
        std::vector<std::uint8_t> memory(bytes, 0);
        for (const auto index : sequence) {
            for (auto at = writes[index].begin; at < writes[index].end; ++at) memory[at] = static_cast<std::uint8_t>(index + 1);
        }
        Require(memory == programOrder(writes, bytes), "the recorded order changed the final bytes of the program order");
        for (std::size_t j = 0; j < writes.size(); ++j) {
            if (writes[j].computed) continue;
            std::vector<bool> covered(bytes);
            for (auto k = j + 1; k < writes.size(); ++k) {
                if (writes[k].computed) std::fill(covered.begin() + writes[k].begin, covered.begin() + writes[k].end, true);
            }
            if (std::any_of(covered.begin() + writes[j].begin, covered.begin() + writes[j].end, [](bool value) { return value; })) continue;
            for (std::size_t i = 0; i < j; ++i) {
                Require(position[i] < position[j], writes[i].computed ? "a store no later dump overlaps was recorded before a dump ahead of it" : "a store no later dump overlaps was recorded before a store ahead of it");
            }
        }
    }
}

constexpr std::uint64_t SampleReady = 1ull << 63u;

void queuedSampleDumpTests(Recorder& recorder, const HostImport& import, std::uint64_t* words, std::uint64_t address, VkDeviceAddress target) {
    constexpr std::uint64_t untouched = 0xaaaaaaaaaaaaaaaaull;
    const auto storeZero = [&](std::uint64_t at, std::size_t bytes) {
        const std::vector<std::byte> zero(bytes);
        recorder.RecordStore(import.buffer, at - import.base, zero, at);
    };
    recorder.Sync();
    const auto base = recorder.SamplesTotal();
    std::fill(words, words + 256, untouched);
    Require(recorder.DumpSamples(target, address), "a dump was not queued");
    Require(recorder.HasQueuedStores() && recorder.QueuedStoreOverlaps(address + 240, 8) && !recorder.QueuedStoreOverlaps(address + 248, 8), "a queued dump does not cover its 16 counters");
    recorder.FlushStoresOverlapping(address + 256, 64);
    Require(recorder.HasQueuedStores(), "a reader of other bytes recorded the queued dump");
    Require(words[0] == untouched, "a queued dump landed before anything recorded it");
    recorder.FlushStoresOverlapping(address + 16, 8);
    Require(!recorder.HasQueuedStores(), "a reader of the dumped bytes left the dump queued");
    recorder.Sync();
    Require(words[0] == (base | SampleReady) && words[2] == SampleReady && words[1] == untouched, "the dump a reader recorded stored the wrong counters");
    std::fill(words, words + 256, untouched);
    storeZero(address, 256);
    Require(recorder.DumpSamples(target, address), "a dump after a clear was not queued");
    storeZero(address + 512, 16);
    Require(recorder.DumpSamples(target + 8, address + 8), "the end dump was not queued");
    Require(recorder.DumpSamples(target + 256, address + 256), "a dump of another slot was not queued");
    storeZero(address + 256, 16);
    Require(recorder.DumpSamples(target + 256 + 8, address + 256 + 8), "a dump after a store over an earlier dump was not queued");
    recorder.Sync();
    Require(words[0] == (base | SampleReady) && words[1] == (base | SampleReady) && words[2] == SampleReady && words[3] == SampleReady, "a clear and its dumps landed out of order");
    Require(words[32] == 0 && words[34] == SampleReady, "a store after a dump of its bytes did not land last");
    Require(words[33] == (base | SampleReady) && words[35] == SampleReady, "a dump after a store over an earlier dump landed before it");
    Require(words[64] == 0 && words[65] == 0 && words[66] == untouched, "a store between dumps was lost");
}

class CoverPass {
public:
    explicit CoverPass(const Context& context) : context(context), target(context, ColorTarget{0, {8, 8}, VK_FORMAT_R8G8B8A8_UNORM, 8 * 8 * 4, 0}, false) {
        try {
            if (!context.graphicsPipelineLibrary) {
                VkAttachmentDescription attachment{};
                attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
                attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
                const VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
                VkSubpassDescription subpass{};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &color;
                VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
                passInfo.attachmentCount = 1;
                passInfo.pAttachments = &attachment;
                passInfo.subpassCount = 1;
                passInfo.pSubpasses = &subpass;
                Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &renderPass), "vkCreateRenderPass cover");
                const auto view = target.View();
                VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                framebufferInfo.renderPass = renderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = &view;
                framebufferInfo.width = 8;
                framebufferInfo.height = 8;
                framebufferInfo.layers = 1;
                Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer cover");
            }
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout cover");
            const auto module = [&](const std::uint32_t* code, std::size_t bytes) {
                VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                info.codeSize = bytes;
                info.pCode = code;
                VkShaderModule result = VK_NULL_HANDLE;
                Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &info, nullptr, &result), "vkCreateShaderModule cover");
                return result;
            };
            vertex = module(COVER_VERT_SPV, sizeof(COVER_VERT_SPV));
            fragment = module(COVER_FRAG_SPV, sizeof(COVER_FRAG_SPV));
            std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
            stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertex, "main", nullptr};
            stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr};
            VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            const VkViewport viewport{0, 0, 8, 8, 0, 1};
            const VkRect2D scissor{{0, 0}, {8, 8}};
            VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.lineWidth = 1;
            VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState blend{};
            blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blendState.attachmentCount = 1;
            blendState.pAttachments = &blend;
            VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
            pipelineInfo.pStages = stages.data();
            pipelineInfo.pVertexInputState = &vertexInput;
            pipelineInfo.pInputAssemblyState = &assembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &raster;
            pipelineInfo.pMultisampleState = &multisample;
            pipelineInfo.pColorBlendState = &blendState;
            pipelineInfo.layout = layout;
            const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
            VkPipelineRenderingCreateInfoKHR renderingFormats{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
            renderingFormats.colorAttachmentCount = 1;
            renderingFormats.pColorAttachmentFormats = &colorFormat;
            if (context.graphicsPipelineLibrary) pipelineInfo.pNext = &renderingFormats;
            else pipelineInfo.renderPass = renderPass;
            Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateGraphicsPipelines cover");
        } catch (...) {
            release();
            throw;
        }
    }
    ~CoverPass() { release(); }
    CoverPass(const CoverPass&) = delete;
    CoverPass& operator=(const CoverPass&) = delete;

    bool Draw(Recorder& recorder) const {
        const auto start = recorder.StartDrawPass(PassKey, false, false, PassAccess{});
        const bool continued = start.continued;
        const auto commands = start.commands;
        if (!continued) {
            recorder.PrepareSampleSlot();
            begin(commands);
        }
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        recorder.NoteSampledDraw(commands);
        context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 3, 1, 0, 0);
        recorder.LeaveRenderPassOpen(PassKey, Recorder::NoTiming, false, PassAccess{});
        return continued;
    }

    void Record(VkCommandBuffer commands) const {
        begin(commands);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 3, 1, 0, 0);
        EndRenderPass(context, commands);
    }

    static constexpr std::uint64_t Samples = 64;

private:
    static constexpr std::uint64_t PassKey = 0xc0de;
    void begin(VkCommandBuffer commands) const {
        if (context.graphicsPipelineLibrary) {
            if (!transitioned) {
                transitioned = true;
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.image = target.Image();
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            }
            VkRenderingAttachmentInfoKHR attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
            attachment.imageView = target.View();
            attachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            VkRenderingInfoKHR rendering{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
            rendering.renderArea = {{0, 0}, {8, 8}};
            rendering.layerCount = 1;
            rendering.colorAttachmentCount = 1;
            rendering.pColorAttachments = &attachment;
            context.Resolved(&DeviceFunctions::cmdBeginRendering, "vkCmdBeginRenderingKHR")(commands, &rendering);
        } else {
            VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            info.renderPass = renderPass;
            info.framebuffer = framebuffer;
            info.renderArea = {{0, 0}, {8, 8}};
            context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &info, VK_SUBPASS_CONTENTS_INLINE);
        }
    }
    void release() noexcept {
        const auto device = context.device;
        if (pipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(device, pipeline, nullptr);
        if (vertex != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(device, vertex, nullptr);
        if (fragment != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(device, fragment, nullptr);
        if (layout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(device, layout, nullptr);
        if (framebuffer != VK_NULL_HANDLE) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(device, framebuffer, nullptr);
        if (renderPass != VK_NULL_HANDLE) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(device, renderPass, nullptr);
        pipeline = VK_NULL_HANDLE;
        vertex = fragment = VK_NULL_HANDLE;
        layout = VK_NULL_HANDLE;
        framebuffer = VK_NULL_HANDLE;
        renderPass = VK_NULL_HANDLE;
    }
    const Context& context;
    RenderTarget target;
    mutable bool transitioned = false;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

void inPassSampleDumpTests(Recorder& recorder, const Context& context, const CoverPass& cover, std::uint64_t* words, std::uint64_t address, VkDeviceAddress target) {
    const auto samples = [&](std::uint64_t word) { return word & ~SampleReady; };
    recorder.Sync();
    auto base = recorder.SamplesTotal();
    std::fill(words, words + 64, 0);
    Require(!cover.Draw(recorder), "the first draw of a batch continued a pass");
    Require(recorder.DumpSamples(target, address), "a dump inside a render pass was not made on the GPU");
    Require(cover.Draw(recorder), "an occlusion dump ended the render pass");
    Require(recorder.DumpSamples(target + 8, address + 8), "the second dump inside the pass was not made on the GPU");
    Require(cover.Draw(recorder), "the second occlusion dump ended the render pass");
    Require(words[0] == 0 && words[1] == 0, "a dump inside the pass landed before its batch ran");
    recorder.Sync();
    const auto first = samples(words[0]) - base, second = samples(words[1]) - base, total = recorder.SamplesTotal() - base;
    Require((words[0] & SampleReady) != 0 && (words[1] & SampleReady) != 0, "a dump inside a pass lost its ready bit");
    Require(first > 0 && second > first && total > second, "dumps inside one pass do not split its draws' samples in order");
    if (context.occlusionQueryPrecise) Require(first == CoverPass::Samples && second == 2 * CoverPass::Samples && total == 3 * CoverPass::Samples, "dumps inside one pass counted the wrong samples");
    base = recorder.SamplesTotal();
    Require(!cover.Draw(recorder), "the first draw of a new batch continued a pass");
    Require(recorder.DumpSamples(target, address), "a dump before a reader was not made on the GPU");
    recorder.FlushStoresOverlapping(address, 8);
    Require(!cover.Draw(recorder), "a reader of a queued dump recorded it inside the render pass");
    recorder.Sync();
    Require(samples(words[0]) > base && samples(words[0]) < recorder.SamplesTotal(), "the dump a reader recorded in the pass counted the wrong draws");
    base = recorder.SamplesTotal();
    constexpr std::size_t slots = 160;
    std::fill(words, words + slots * 32, 0);
    std::size_t continued = 0;
    for (std::size_t i = 0; i < slots; ++i) {
        if (cover.Draw(recorder)) ++continued;
        Require(recorder.DumpSamples(target + 256 * i, address + 256 * i), "a dump of many in one pass was not made on the GPU");
    }
    recorder.Sync();
    Require(continued + 2 >= slots, "draws between dumps did not keep their render pass");
    std::uint64_t previous = base;
    for (std::size_t i = 0; i < slots; ++i) {
        const auto value = samples(words[32 * i]);
        Require((words[32 * i] & SampleReady) != 0 && value > previous, "a dump past one query pool's slots counted the wrong draws");
        if (context.occlusionQueryPrecise) Require(value == base + (i + 1) * CoverPass::Samples, "a dump past one query pool's slots counted the wrong samples");
        previous = value;
    }
    base = recorder.SamplesTotal();
    constexpr std::size_t passes = 70;
    for (std::size_t i = 0; i < passes; ++i) {
        Require(!cover.Draw(recorder), "a draw after other work continued a pass");
        recorder.Commands();
    }
    recorder.Submit();
    Require(recorder.DumpSamples(target, address), "a dump after many passes was not made on the GPU");
    recorder.Sync();
    const auto folded = samples(words[0]);
    Require(folded > base && folded == recorder.SamplesTotal(), "segments folded at Submit without a dump were lost");
    if (context.occlusionQueryPrecise) Require(folded == base + passes * CoverPass::Samples, "segments folded at Submit without a dump counted the wrong samples");
}

bool pendingKeyStoreTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: pending key stores not tested\n";
        return false;
    }
    constexpr std::size_t surfaceBytes = 65536;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the pending key store block");
    auto* keys = static_cast<std::uint8_t*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{context, block, address};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the pending key store block refused: pending key stores not tested\n";
        return false;
    }
    recorder.Sync();
    std::memset(keys, 0x00, bytes);
    MarkDccUncompressed(context, address, surfaceBytes);
    Require(recorder.PendingWriteOverlaps(address, keyCount) && keys[0] == 0x00, "the uncompressed key store did not stay pending");
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Uncompressed, "the keys after a pending uncompressed store do not read as uncompressed");
    Require(recorder.PendingWriteOverlaps(address, keyCount), "reading keys the driver's own pending store wrote waited for the GPU");
    Require(CurrentDccKeys(address + 16, surfaceBytes / 2) == DccKeys::Uncompressed, "a key range inside the pending store does not read as uncompressed");
    recorder.NotePendingWrite(address + 16, 16);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Uncompressed && !recorder.PendingWriteOverlaps(address, keyCount), "a key read with a later writer over the pending store did not wait for it");
    Require(std::all_of(keys, keys + keyCount, [](std::uint8_t key) { return key == 0xff; }), "the uncompressed key store did not land");
    recorder.NotePendingWrite(address, keyCount);
    NoteKeysFillOnGpu(address, keyCount, DccKeys::Clear0001);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Clear0001 && recorder.PendingWriteOverlaps(address, keyCount), "the keys of a pending fill did not read as its keys without a wait");
    Require(KnownPendingDccKeys(address, surfaceBytes) == DccKeys::Clear0001 && recorder.PendingWriteOverlaps(address, keyCount), "the keys of a pending fill were not known without a wait");
    Require(KnownPendingDccKeys(address + 16, surfaceBytes / 2) == DccKeys::Clear0001, "a key range inside a pending fill was not known");
    Require(!KnownPendingDccKeys(address, 2 * surfaceBytes).has_value() && recorder.PendingWriteOverlaps(address, keyCount), "a range wider than the pending fill was known, or waited for");
    recorder.NotePendingWrite(address, 2 * keyCount);
    Require(!KnownPendingDccKeys(address, surfaceBytes).has_value() && recorder.PendingWriteOverlaps(address, keyCount), "the keys under a later writer over a pending fill were known");
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Uncompressed && !recorder.PendingWriteOverlaps(address, keyCount), "a later wider writer over a pending fill was not waited for");
    Require(!KnownPendingDccKeys(address, surfaceBytes).has_value(), "keys nothing writes were known as pending keys");
    recorder.NotePendingFill(address, keyCount / 2, 0x40);
    recorder.NotePendingFill(address + keyCount / 2, keyCount / 2, 0x40);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Clear0001 && recorder.PendingWriteOverlaps(address, keyCount), "the keys of pending 0001 fills over both halves did not read as 0001 without a wait");
    recorder.NotePendingWrite(address + keyCount / 2, 16);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Uncompressed && !recorder.PendingWriteOverlaps(address, keyCount), "a later writer of unknown bytes over pending fills was not waited for");
    recorder.NotePendingFill(address, keyCount / 2, 0x00);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Mixed && recorder.PendingWriteOverlaps(address, keyCount), "a pending 0000 fill over half of uncompressed keys did not read as mixed without a wait");
    recorder.Sync();
#ifdef _WIN32
    auto* edgeBlock = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * bytes, MEM_RESERVE, PAGE_NOACCESS));
    Require(edgeBlock != nullptr && VirtualAlloc(edgeBlock, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot reserve the key block past committed memory");
#else
    void* edgeMapping = mmap(nullptr, 2 * bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(edgeMapping != MAP_FAILED && mprotect(edgeMapping, bytes, PROT_READ | PROT_WRITE) == 0, "cannot reserve the key block past committed memory");
    auto* edgeBlock = static_cast<std::uint8_t*>(edgeMapping);
#endif
    const auto edge = reinterpret_cast<std::uint64_t>(edgeBlock) + bytes - keyCount / 2;
    Require(!AgcDriver::GuestMemory::Accessible(reinterpret_cast<const void*>(edge), keyCount), "keys past the end of committed memory read as accessible");
    recorder.NotePendingFill(edge, keyCount / 2, 0x40);
    Require(CurrentDccKeys(edge, surfaceBytes) == DccKeys::Unreadable && !recorder.PendingWriteOverlaps(edge, keyCount), "keys running past the end of committed memory, with a pending fill over their committed half, were not read as unreadable after a wait");
#ifdef _WIN32
    VirtualFree(edgeBlock, 0, MEM_RELEASE);
#else
    munmap(edgeBlock, 2 * bytes);
#endif
    constexpr std::uint64_t displayBytes = 15u * 9u * 65536u;
    constexpr std::size_t consoleKeys = 49152;
    const auto displayKeys = DccKeyCount(TextureTileMode::kR64KBX, 4, 1920, 1080, displayBytes);
    std::memset(keys, 0x00, consoleKeys);
    MarkDccUncompressed(context, address, displayBytes, displayKeys);
    Require(recorder.PendingWriteOverlaps(address, consoleKeys) && keys[consoleKeys - 1] == 0x00, "the uncompressed key store over a 1920x1080 surface did not stay pending");
    Require(CurrentDccKeys(address + displayBytes / 256, (consoleKeys - displayBytes / 256) * 256) == DccKeys::Uncompressed && recorder.PendingWriteOverlaps(address, consoleKeys), "the keys past one per 256 bytes of a pending 1920x1080 store did not read as uncompressed without a wait");
    Require(CurrentDccKeys(address, displayBytes, displayKeys) == DccKeys::Uncompressed && recorder.PendingWriteOverlaps(address, consoleKeys), "the keys of a pending 1920x1080 store did not read as uncompressed over the console's extent without a wait");
    recorder.NotePendingWrite(address + 40000, 1);
    Require(CurrentDccKeys(address, displayBytes, displayKeys) == DccKeys::Uncompressed && !recorder.PendingWriteOverlaps(address + 40000, 1), "a later writer of key 40000 over a pending 1920x1080 store was not waited for");
    Require(std::all_of(keys, keys + consoleKeys, [](std::uint8_t key) { return key == 0xff; }), "the uncompressed key store over a 1920x1080 surface did not land on all 49152 keys");
    recorder.Sync();
    return true;
}

bool shadowedKeyStoreTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    if (!UnitShadowEnabled() || context.hostImportAlignment == 0) {
        std::cout << "unit shadows or host imports unavailable: keys under a unit shadow not tested\n";
        return false;
    }
    constexpr std::uint64_t unit = 65536;
    constexpr std::size_t bytes = 2 * unit;
    constexpr std::size_t surfaceBytes = 65536;
    constexpr std::size_t keyCount = surfaceBytes / 256;
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, unit);
    Require(block != nullptr, "cannot reserve the shadowed key block");
    const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, static_cast<DWORD>(bytes), nullptr);
    Require(section != nullptr, "cannot create the shadowed key block");
    GuestArena::GuestArenaMap_nid_postfix(block, bytes, section, 0, PAGE_READWRITE);
    CloseHandle(section);
#else
    void* block = AllocateWatched(bytes, unit);
    Require(block != nullptr, "cannot allocate the shadowed key block");
#endif
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    auto* keys = static_cast<std::uint8_t*>(block);
    std::memset(block, 0xff, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{context, block, address};
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr || !Watched(address, bytes)) {
        std::cout << "the shadowed key block is not imported and watched: keys under a unit shadow not tested\n";
        return false;
    }
    recorder.Sync();
    CollectWritesUncached(address, bytes);
    const auto copyBuffer = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
    const auto shadow = [&](std::uint8_t value) {
        auto destination = ShadowDestinationFor(context, *import, address, address + unit);
        Require(destination.has_value() && destination->seedUnits.empty(), "the key unit has no unit shadow destination");
        auto pattern = std::make_shared<Buffer>(context, static_cast<std::size_t>(unit), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memset(pattern->Bytes().data(), value, pattern->Bytes().size());
        const auto commands = recorder.Commands();
        recorder.Keep(pattern);
        recorder.Keep(destination->slab);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkBufferCopy piece{0, destination->offset, unit};
        copyBuffer(commands, pattern->Handle(), destination->buffer, 1, &piece);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        recorder.MarkCovered(VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const ShadowedRange range{address, address + unit, destination->slab};
        MarkShadowed(*import, std::span(&range, 1), TrackerGeneration());
        Require(AnyShadowedOverlaps(address, keyCount), "the key unit is not shadowed");
    };
    recorder.NotePendingFill(address, unit, 0x00);
    MarkWritten(address, unit);
    shadow(0x40);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Clear0001 && !recorder.PendingWriteOverlaps(address, keyCount), "the keys under a fresh unit shadow were not read from its published bytes after a wait");
    Require(keys[0] == 0x40 && keys[unit - 1] == 0x40 && keys[unit] == 0xff && !AnyShadowedOverlaps(address, unit), "the fresh unit's publish did not land");
    recorder.Sync();
    shadow(0x22);
    MarkWritten(address, unit);
    recorder.NotePendingFill(address + unit, unit, 0x11);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Clear0001 && !AnyShadowedOverlaps(address, unit), "the keys under a stale unit shadow were not read from guest memory");
    Require(recorder.PendingWriteOverlaps(address + unit, unit), "a key read under a stale unit shadow waited for a write outside the keys");
    recorder.Sync();
    shadow(0x22);
    recorder.NotePendingFill(address, unit, 0xc0);
    MarkWritten(address, unit);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Clear1111 && recorder.PendingWriteOverlaps(address, keyCount), "the keys of a pending fill over a stale unit shadow were not read as the fill's keys without a wait");
    Require(!AnyShadowedOverlaps(address, unit), "the stale unit still counts as shadowed after the key read");
    recorder.Sync();
    GuestTextureResource resource{};
    resource.baseAddress = address + unit;
    resource.format = 56;
    resource.dccAddress = address;
    shadow(0x22);
    recorder.NotePendingFill(address, unit, 0xc0);
    MarkWritten(address, unit);
    Require(!WaitForKeyWriters(resource, surfaceBytes).has_value() && recorder.PendingWriteOverlaps(address, keyCount), "the key writers over a stale unit shadow were waited for");
    recorder.Sync();
    Require(keys[0] == 0x40 && keys[unit - 1] == 0x40, "a stale unit's slab bytes were published");
    return true;
}

void sampleDumpTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0 || !context.bufferDeviceAddress) {
        std::cout << "host imports or buffer device addresses unavailable: occlusion counter dumps on the GPU not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the occlusion counter block");
    auto* words = static_cast<std::uint64_t*>(block);
    constexpr std::uint64_t untouched = 0xaaaaaaaaaaaaaaaaull;
    std::fill(words, words + 64, untouched);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{context, block, address};
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr || import->address == 0) {
        std::cout << "host import of the occlusion counter block refused: occlusion counter dumps on the GPU not tested\n";
        return;
    }
    const auto target = import->address + (address - import->base);
    recorder.Sync();
    constexpr std::uint64_t ready = 1ull << 63u;
    Require(recorder.DumpSamples(target, address), "the occlusion counters were not dumped on the GPU");
    Require(words[0] == untouched && !recorder.Idle(), "the occlusion counter dump waited for the GPU or landed before its batch ran");
    recorder.Sync();
    const auto begin = recorder.SamplesTotal();
    for (std::size_t db = 0; db < 16; ++db) {
        Require(words[db * 2] == ((db == 0 ? begin : 0) | ready), "an occlusion counter dump stored the wrong value");
        Require(words[db * 2 + 1] == untouched, "an occlusion counter dump stored over the next counter");
    }
    Require(recorder.DumpSamples(target + 8, address + 8), "the second occlusion counter dump was not made on the GPU");
    recorder.Submit();
    recorder.Sync();
    for (std::size_t db = 0; db < 16; ++db) {
        Require(words[db * 2 + 1] == ((db == 0 ? begin : 0) | ready), "the counters changed with nothing drawn between two dumps");
        Require(words[db * 2] == ((db == 0 ? begin : 0) | ready), "the second dump stored over the first");
    }
    Require(recorder.SamplesTotal() == begin, "the sample total moved with nothing drawn");
    for (int i = 0; i < 40; ++i) Require(recorder.DumpSamples(target, address), "a dump past one batch's query slots was not made on the GPU");
    recorder.Sync();
    Require(words[0] == (begin | ready) && recorder.SamplesTotal() == begin, "dumps past one batch's query slots moved the total");
    Require(recorder.QueuesSampleDumps(), "occlusion dumps are recorded at once, not queued with the batch's stores");
    queuedSampleDumpTests(recorder, *import, words, address, target);
    if (!device.Graphics()) {
        std::cout << "the test queue has no graphics: occlusion dumps inside a render pass not tested\n";
        return;
    }
    CoverPass cover(context);
    inPassSampleDumpTests(recorder, context, cover, words, address, target);
}

void uncountedPassTests(const Device& device) {
    auto context = device.GetContext();
    context.bufferDeviceAddress = false;
    Recorder recorder(context);
    CoverPass cover(context);
    recorder.CountSamples();
    Require(!recorder.QueuesSampleDumps(), "a recorder without buffer device addresses counted samples in segments");
    const auto base = recorder.SamplesTotal();
    cover.Draw(recorder);
    recorder.RecordUncounted([&](VkCommandBuffer commands) { cover.Record(commands); });
    cover.Draw(recorder);
    recorder.Submit();
    recorder.Sync();
    const auto counted = recorder.SamplesTotal() - base;
    Require(counted != 0, "the draws around an uncounted pass were not counted");
    if (context.occlusionQueryPrecise) Require(counted == 2 * CoverPass::Samples, "the batch-wide occlusion query counted " + std::to_string(counted) + " samples, not the " + std::to_string(2 * CoverPass::Samples) + " of the two draws around an uncounted pass");
}

void cmaskPassTests(const Device& device, Recorder& recorder) {
    namespace GuestMemory = AgcDriver::GuestMemory;
    auto context = device.GetContext();
    context.hostImportAlignment = 0;
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    constexpr std::size_t surfaceBytes = 256 * 256 * 4;
    constexpr std::size_t bytes = surfaceBytes + 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the CMASK pass block");
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Release {
        const Context& context;
        void* block;
        ~Release() {
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
#ifdef _WIN32
            VirtualFree(block, 0, MEM_RELEASE);
#else
            std::free(block);
#endif
        }
    } release{context, block};
    Require(HostImportFor(context, address, bytes) == nullptr, "CMASK copy-path test imported its memory");
    std::memset(block, 0x55, surfaceBytes);
    std::memset(static_cast<std::byte*>(block) + surfaceBytes, 0, 4096);
    ColorTarget color{};
    color.address = address;
    color.extent = {256, 256};
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
    color.bytes = surfaceBytes;
    color.componentMapping = 0xe4u;
    color.tileMode = ColorTileMode::RenderTarget;
    color.cmaskAddress = address + surfaceBytes;
    color.cmaskBytes = CmaskBytes(color.extent.width, color.extent.height);
    color.clearWords = {0x80402010u, 0};
    const auto run = [&] { RunColorMetadataPass(context, {ColorMetadataPass::Mode::EliminateFastClear, {color}}); };
    auto broken = context;
    broken.deviceProc = nullptr;
    bool refused = false;
    try {
        RunColorMetadataPass(broken, {ColorMetadataPass::Mode::EliminateFastClear, {color}});
    } catch (const std::runtime_error& error) {
        refused = std::string_view(error.what()).find("missing Vulkan device function resolver") != std::string_view::npos;
        if (!refused) throw;
    }
    Require(refused, "CMASK storage creation failure was silently ignored");
    Require(*static_cast<std::uint8_t*>(block) == 0x55 && ReadDccKeys(color.cmaskAddress, color.cmaskBytes * 256u) == DccKeys::Clear0000, "failed CMASK storage creation changed guest memory");
    run();
    Require(StorageTexture::FindPending(address, surfaceBytes) != nullptr && *static_cast<std::uint8_t*>(block) == 0x55, "CMASK clear without host imports did not stay on the GPU");
    Require(ReadDccKeys(color.cmaskAddress, color.cmaskBytes * 256u) == DccKeys::Uncompressed, "resident CMASK clear left unresolved metadata");
    color.clearWords[0] = 0x12345678;
    run();
    std::vector<std::uint32_t> stored(surfaceBytes / 4);
    GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)));
    Require(std::ranges::all_of(stored, [](auto word) { return word == 0x80402010u; }), "expanded CMASK changed a pending clear or copy write-back lost it");
    const std::vector<std::byte> clearKeys(4096);
    GuestMemory::Write(color.cmaskAddress, clearKeys);
    run();
    GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)));
    Require(std::ranges::all_of(stored, [](auto word) { return word == 0x12345678u; }), "a second resident CMASK clear retained the first frame");
    recorder.Sync();
}

void metadataPassTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: resident metadata passes not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the metadata pass block");
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* keys = texels + surfaceBytes;
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the metadata pass block refused: resident metadata passes not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    ColorTarget color{};
    color.address = address;
    color.extent = {side, side};
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
    color.bytes = surfaceBytes;
    color.componentMapping = 0xe4u;
    color.tileMode = ColorTileMode::RenderTarget;
    color.elementBytes = 4;
    color.dccAddress = address + surfaceBytes;
    color.dccPipeAligned = true;
    color.clearWords = {0x80402010u, 0};
    const ColorMetadataPass pass{ColorMetadataPass::Mode::EliminateFastClear, {color}};
    const auto memoryHolds = [&](std::array<std::uint8_t, 4> texel) {
        StorageTexture::FlushPending(address, surfaceBytes, nullptr, "test");
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        std::vector<std::uint8_t> stored(surfaceBytes);
        AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)), 1);
        for (std::size_t i = 0; i < surfaceBytes; ++i) {
            if (stored[i] != texel[i % 4]) return false;
        }
        return true;
    };
    const auto keysUncompressed = [&] { return ReadDccKeys(color.dccAddress, surfaceBytes) == DccKeys::Uncompressed; };
    const auto extent = DccKeyCount(TextureTileMode::kR64KBX, 4, side, side, surfaceBytes);
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(keys, 0x20, extent);
    RunColorMetadataPass(context, pass);
    Require(StorageTexture::FindPending(address, surfaceBytes) != nullptr && texels[0] == 0x55, "the register fast clear eliminate did not stay in the resident image");
    Require(keysUncompressed(), "the register fast clear eliminate left the keys compressed");
    Require(CurrentDccKeys(color.dccAddress, surfaceBytes, extent) == DccKeys::Uncompressed, "the register fast clear eliminate did not store uncompressed keys over the target's console DCC extent");
    Require(memoryHolds({0x10, 0x20, 0x40, 0x80}), "the register fast clear eliminate did not store CB_COLOR_CLEAR_WORD");
    Require(std::all_of(keys, keys + extent, [](std::uint8_t key) { return key == 0xff; }), "the stored keys are not uncompressed");
    std::memset(keys, 0xc0, keyCount);
    RunColorMetadataPass(context, {ColorMetadataPass::Mode::DccDecompress, {color}});
    Require(StorageTexture::FindPending(address, surfaceBytes) != nullptr, "the DCC decompress did not stay in the resident image");
    Require(keysUncompressed(), "the DCC decompress left the keys compressed");
    Require(memoryHolds({0xff, 0xff, 0xff, 0xff}), "the DCC decompress did not store the 1111 value");
    {
        const auto* import = HostImportFor(base, address, bytes);
        Require(import != nullptr, "the metadata pass block lost its import");
        const auto commands = recorder.Commands();
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, import->buffer, color.dccAddress - import->base, keyCount, 0x20202020u);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT);
        recorder.NotePendingWrite(color.dccAddress, keyCount);
        Require(keys[0] == 0xff, "the recorded key fill landed before its batch ran");
        Require(CurrentDccKeys(color.dccAddress, surfaceBytes) == DccKeys::ClearRegister, "the keys read past a pending fast-clear fill");
    }
    std::memset(texels, 0x77, surfaceBytes);
    RunColorMetadataPass(context, pass);
    Require(StorageTexture::FindPending(address, surfaceBytes) != nullptr && texels[0] == 0x77, "a fast clear recorded before the pass fell back to CPU write-back");
    Require(keysUncompressed() && memoryHolds({0x10, 0x20, 0x40, 0x80}), "a fast clear recorded before the pass was not eliminated");
    std::memset(texels, 0x66, surfaceBytes);
    RunColorMetadataPass(context, pass);
    Require(memoryHolds({0x66, 0x66, 0x66, 0x66}), "a pass over uncompressed keys changed the texels");
    ClearCachedTextures(context.device);
    GuestTextureResource plain{};
    plain.baseAddress = address;
    plain.width = side;
    plain.height = side;
    plain.mipCount = 1;
    plain.tileMode = TextureTileMode::kR64KBX;
    plain.dimension = TextureDimension::k2D;
    plain.format = 56;
    plain.dstSelX = 4;
    plain.dstSelY = 5;
    plain.dstSelZ = 6;
    plain.dstSelW = 7;
    const auto unkeyed = CachedStorageSurface(context, plain);
    std::memset(keys, 0xc0, keyCount);
    RunColorMetadataPass(context, pass);
    const auto keyed = StorageTexture::FindPending(address, surfaceBytes);
    Require(keyed != nullptr && keyed != unkeyed && keyed->Descriptor().dccAddress == color.dccAddress, "a pass over a resident image under other keys did not remake it under the target's keys");
    Require(keysUncompressed() && memoryHolds({0xff, 0xff, 0xff, 0xff}), "a pass over a remade resident image did not store the 1111 value");
    {
        const auto* import = HostImportFor(base, address, bytes);
        Require(import != nullptr, "the metadata pass block lost its import");
        const auto commands = recorder.Commands();
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, import->buffer, color.dccAddress - import->base, keyCount, 0x20202020u);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT);
        recorder.NotePendingWrite(color.dccAddress, keyCount);
        NoteKeysFillOnGpu(color.dccAddress, keyCount, DccKeys::ClearRegister);
    }
    Require(KnownPendingDccKeys(color.dccAddress, surfaceBytes) == DccKeys::ClearRegister, "the keys of a pending register clear store were not known");
    const std::array<std::uint32_t, 4> clearPattern{0x80402010u, 0x80402010u, 0x80402010u, 0x80402010u};
    const char* refusal = nullptr;
    Require(!keyed->FillClear(clearPattern, StorageTexture::WholeImage, refusal) && refusal != nullptr && std::strcmp(refusal, "keys pending") == 0, "a clear over pending keys was not refused without the proof");
    refusal = nullptr;
    Require(keyed->FillClear(clearPattern, StorageTexture::WholeImage, refusal, true), "a clear over known pending keys was refused");
    Require(recorder.PendingWriteOverlaps(color.dccAddress, keyCount) && keys[0] == 0xff, "the clear over known pending keys waited for the key store");
    MarkDccUncompressed(context, color.dccAddress, surfaceBytes);
    Require(memoryHolds({0x10, 0x20, 0x40, 0x80}), "the clear recorded behind a pending key store did not store CB_COLOR_CLEAR_WORD");
    Require(std::all_of(keys, keys + keyCount, [](std::uint8_t key) { return key == 0xff; }), "the keys after a clear behind a pending key store are not uncompressed");
}

// The data word positions of a dispatch-cache variant (Driver.cpp's data-only hits): leaves
// located among the runs' words, aliased, unaligned, out-of-run and mismatched ones skipped.
void dataWordPositionsTests() {
    using AgcDriver::DataWordPositions;
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> runs{{0x1000, 0x1010}, {0x2000, 0x2008}};
    const std::vector<std::uint32_t> words{1, 2, 3, 4, 5, 6};
    const std::vector<std::uint32_t> flattened{2, 6, 3};
    std::vector<std::uint32_t> positions, slots;
    {
        const std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves{{1, 0x2004}, {0, 0x1004}, {2, 0x1008}};
        const std::vector<std::uint64_t> otherReads{0x1008};
        const auto counts = DataWordPositions(runs, leaves, otherReads, words, flattened, positions, slots);
        Require(positions == std::vector<std::uint32_t>{1, 5} && slots == std::vector<std::uint32_t>{0, 1}, "data word positions are wrong");
        Require(counts.aliased == 1 && counts.unmapped == 0 && counts.mismatched == 0, "an aliased leaf was not counted");
    }
    {
        const std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves{{0, 0x1006}, {1, 0x3000}, {1, 0x2008}, {2, 0x1000}};
        const auto counts = DataWordPositions(runs, leaves, {}, words, flattened, positions, slots);
        Require(positions.empty() && slots.empty(), "a skipped leaf produced a position");
        Require(counts.unmapped == 3 && counts.mismatched == 1 && counts.aliased == 0, "skipped leaves were counted wrongly");
    }
    {
        // Two pure slots over one dword: both positions kept, in position order.
        const std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves{{2, 0x1008}, {1, 0x2004}, {0, 0x1004}};
        static_cast<void>(DataWordPositions(runs, leaves, {}, words, flattened, positions, slots));
        Require(positions == std::vector<std::uint32_t>{1, 2, 5} && slots == std::vector<std::uint32_t>{0, 2, 1}, "positions are not sorted");
    }
}

std::array<std::uint32_t, 8> linearTextureWords(std::uint64_t address, std::uint32_t width, std::uint32_t height, std::uint32_t format) {
    const auto base40 = address >> 8u;
    std::array<std::uint32_t, 8> words{};
    words[0] = static_cast<std::uint32_t>(base40);
    words[1] = static_cast<std::uint32_t>((base40 >> 32u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u);
    words[2] = (((width - 1u) >> 2u) & 0xfffu) | ((height - 1u) << 14u);
    words[3] = 4u | (5u << 3u) | (6u << 6u) | (7u << 9u) | (9u << 28u);
    return words;
}

void imageMemoTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    TextureCache textureCache(context);
    context.textureCache = &textureCache;
    if (!device.NullDescriptors()) {
        std::cout << "nullDescriptor unavailable: image memo not tested\n";
        return;
    }
    context.nullDescriptors = true;
    constexpr std::uint32_t width = 64;
    constexpr std::uint32_t height = 4;
    constexpr std::size_t surfaceBytes = width * height * 4;
    constexpr std::size_t bytes = 2 * 65536;
    void* block = AllocateWatched(bytes, 65536);
    const bool watched = block != nullptr;
#ifdef _WIN32
    if (!watched) block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    if (!watched) block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the image memo block");
    auto* first = static_cast<std::uint8_t*>(block);
    auto* second = first + 65536;
    std::memset(first, 0x11, surfaceBytes);
    std::memset(second, 0x22, surfaceBytes);
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = 1;
    binding.count = 1;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageSamplers = {0u};
    program.bindings.push_back(binding);
    const CompiledShader compiled{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    std::vector<std::shared_ptr<Texture>> kept;
    const auto sample = [&](const std::uint8_t* surface) {
        const auto words = linearTextureWords(reinterpret_cast<std::uint64_t>(surface), width, height, 56);
        program.bindings[0].guestDescriptor.assign(words.begin(), words.end());
        ShaderResources resources(context, compiled);
        Require(resources.SampledTextures().size() == 1, "the build has no sampled texture");
        kept.push_back(resources.SampledTextures().front());
        return kept.back();
    };
    const auto made = sample(first);
    Require(sample(first) == made, "an unchanged sampled texture was made again");
    const auto other = sample(second);
    Require(other != made && sample(first) == made, "a texture of another descriptor answered for the first");
    ClearCachedTextures(context.device);
    const auto remade = sample(first);
    Require(remade != made && remade != other, "a texture the cache dropped was bound again");
    Require(sample(first) == remade, "the remade texture is not the cache's");
    const auto replaced = sample(second);
    Require(replaced != other && sample(second) == replaced && sample(first) == remade, "the cache does not answer with the replacements");
    const auto firstWords = linearTextureWords(reinterpret_cast<std::uint64_t>(first), width, height, 56);
    const auto secondWords = linearTextureWords(reinterpret_cast<std::uint64_t>(second), width, height, 56);
    const auto bindElements = [&](std::initializer_list<std::array<std::uint32_t, 8>> elements, std::vector<bool> depthCompare) {
        program.bindings[0].count = static_cast<std::uint32_t>(elements.size());
        program.bindings[0].guestDescriptor.clear();
        for (const auto& words : elements) program.bindings[0].guestDescriptor.insert(program.bindings[0].guestDescriptor.end(), words.begin(), words.end());
        program.bindings[0].imageDepthCompare = std::move(depthCompare);
        program.bindings[0].imageSamplers.assign(elements.size(), 0u);
        ShaderResources resources(context, compiled);
        Require(resources.SampledTextures().size() == elements.size(), "the build has the wrong number of sampled textures");
        kept.insert(kept.end(), resources.SampledTextures().begin(), resources.SampledTextures().end());
        return resources.SampledTextures();
    };
    const auto repeated = bindElements({firstWords, firstWords, secondWords, firstWords}, {});
    Require(repeated[0] == remade && repeated[1] == remade && repeated[2] == replaced && repeated[3] == remade, "repeated elements of one build do not bind the cache's textures");
    program.bindings[0].count = 1;
    program.bindings[0].imageDepthCompare.clear();
    program.bindings[0].imageSamplers.assign(1, 0u);
    recorder.Submit();
    device.WaitQueue();
    recorder.Sync();
    kept.clear();
    ClearCachedTextures(context.device);
    if (watched) ReleaseWatched(block, bytes);
#ifdef _WIN32
    else VirtualFree(block, 0, MEM_RELEASE);
#else
    else std::free(block);
#endif
}

// A template's data buffers refreshed by words from a patched compiled result (a data-only hit)
// and back: DataWordsHash() follows the buffers exactly, so a later recipe hit's hash compare
// (RecordedDispatch::DataRefresh::Hash) decides correctly in both directions.
void dataRefreshTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::FlattenedSrt;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {1, 2, 3, 4};
    program.bindings.push_back(binding);
    auto patched = program;
    patched.bindings[0].guestDescriptor[2] = 0x33;
    const CompiledShader original{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    const CompiledShader live{ShaderRecompiler::ShaderStage::Compute, &patched, 0};
    ShaderResources resources(context, original);
    Require(resources.DataWordsHash() == ShaderResources::DataWordsHash(original) && resources.DataWordsHash() != ShaderResources::DataWordsHash(live), "a fresh template's data hash is not its words'");
    Require(!resources.DataWordsDiffer(original) && resources.DataWordsDiffer(live), "the per-word compare disagrees with the words");
    Require(resources.RefreshData(recorder.Commands(), live, &recorder), "a refresh with different words recorded nothing");
    Require(resources.DataWordsHash() == ShaderResources::DataWordsHash(live) && !resources.DataWordsDiffer(live), "the refreshed template's hash is not the patched words'");
    Require(resources.RefreshData(recorder.Commands(), original, &recorder), "the refresh back recorded nothing");
    Require(resources.DataWordsHash() == ShaderResources::DataWordsHash(original), "the refreshed template's hash is not the original words'");
    Require(!resources.RefreshData(recorder.Commands(), original, &recorder), "a refresh with equal words recorded");
    auto replaced = patched;
    replaced.bindings[0].role = ShaderRecompiler::DescriptorRole::GuestSamplers;
    replaced.bindings[0].kind = ShaderRecompiler::DescriptorKind::Sampler;
    const CompiledShader changedResources{ShaderRecompiler::ShaderStage::Compute, &replaced, 0};
    bool rejected = false;
    try {
        static_cast<void>(resources.RefreshData(recorder.Commands(), changedResources, &recorder));
    } catch (const std::exception& error) {
        rejected = std::string_view(error.what()).find("cannot replace bound resources") != std::string_view::npos;
    }
    Require(rejected && resources.DataWordsHash() == ShaderResources::DataWordsHash(original), "a data refresh changed the bound resources");
    recorder.Submit();
    device.WaitQueue();
    recorder.Sync();
}

void multisampleGuestAccessTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if ((base.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0 || !device.NullDescriptors()) {
        std::cout << "4x color targets or nullDescriptor unavailable: multisampled target guest access not tested\n";
        return;
    }
    constexpr std::uint32_t side = 64;
    constexpr std::size_t targetBytes = 65536u;
    constexpr std::size_t bytes = 5u * 65536u;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the multisampled target block");
    std::memset(block, 0, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            ClearMultisampleTargets(context.device);
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
#ifdef _WIN32
            VirtualFree(block, 0, MEM_RELEASE);
#else
            std::free(block);
#endif
        }
    } unregister{base, block};
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    context.nullDescriptors = true;
    TextureCache cache(context);
    context.textureCache = &cache;
    const auto clearTargets = [&] {
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        ClearMultisampleTargets(context.device);
    };
    const auto refused = [](const std::function<void()>& access, const std::string& expected, const std::string& what) {
        std::string message;
        try {
            access();
        } catch (const std::exception& error) {
            message = error.what();
        }
        Require(message.find(expected) != std::string::npos && message.find("multisampled color target") != std::string::npos, what + " of a resolved-only multisampled target's memory did not throw: " + message);
    };
    ColorTarget target{};
    target.address = address;
    target.surfaceAddress = address;
    target.extent = {side, side};
    target.surfaceExtent = target.extent;
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    target.bytes = targetBytes;
    target.samples = 4;
    const bool ordered = MayReorderSamples(context, target.samples) && SampleOrderStorageFormat(context, target.format, target.elementBytes, target.samples) != VK_FORMAT_UNDEFINED;
    {
        std::lock_guard gpu(GpuMutex());
        Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "the multisampled target has no view");
        Require(MultisampleTargetStorable(context, target) == ordered, ordered ? "a multisampled target whose samples may be reordered was created without storage usage" : "a multisampled target nothing stored to was created with storage usage");
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    }
    std::array<std::byte, 64> texels{};
    refused([&] { AgcDriver::GuestMemory::Read(address + 256u, texels); }, "a CPU read", "a CPU read");
    refused([&] { AgcDriver::GuestMemory::ReadCommitted(address + 256u, texels); }, "a CPU read", "a committed CPU read");
    refused([&] { AgcDriver::GuestMemory::ReadCommittedUnsynced(address + 256u, texels); }, "a CPU read", "a committed CPU read without the flush hook");
    refused([&] { RequireNoMultisampleTarget(address, 64u, "a DMA copy"); }, "a DMA copy", "a DMA copy");
    const auto captureSrt = [](std::uint64_t srt, AgcDriver::ShaderMemory::PendingWriteQuery pendingWrite) {
        static constexpr std::array<std::uint32_t, 6> code{0xf4080100u, 0xfa000000u, 0xbf8cc07fu, 0xe0700000u, 0x80010000u, 0xbf810000u};
        const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srt), static_cast<std::uint32_t>(srt >> 32u)};
        ShaderRecompiler::RecompileRequest request{};
        request.shader = {ShaderRecompiler::ShaderStage::Compute, 0x24000u, code, 0, {}};
        request.context.waveSize = 32u;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderRecompiler::ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        request.layout.pushConstantSizeBytes = 128;
        AgcDriver::ShaderMemory memory({}, pendingWrite);
        static_cast<void>(memory.Capture(request));
    };
    refused([&] { captureSrt(address + 256u, nullptr); }, "a CPU read", "a shader capture");
    captureSrt(address + 4u * 65536u + 256u, nullptr);
    refused([&] { captureSrt(address + 256u, nullptr); }, "a CPU read", "a shader capture on a pooled page");
    const auto raw = [](std::uint64_t, std::size_t, std::span<std::byte>) { return AgcDriver::ShaderMemory::PendingWrite::Raw; };
    const auto synced = [](std::uint64_t, std::size_t, std::span<std::byte>) { return AgcDriver::ShaderMemory::PendingWrite::Sync; };
    refused([&] { captureSrt(address + 256u, raw); }, "a CPU read", "a shader capture's raw word read");
    refused([&] { captureSrt(address + 256u, synced); }, "a CPU read", "a shader capture's synced word read");
    captureSrt(address + 4u * 65536u + 256u, raw);
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = ShaderRecompiler::RuntimeAbi::FirstImageBinding;
    binding.count = 1;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageSamplers = {0};
    binding.guestDescriptor = {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        0xfacu | (9u << 28u),
        0u,
        0u,
        0u,
        0u,
    };
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    refused([&] {
        std::lock_guard gpu(GpuMutex());
        ShaderResources resources(context, compute);
    }, "a texture read", "a texture read");
    const auto multisampledWords = [&](std::uint32_t width, std::uint32_t sampleShift, std::uint64_t base) {
        return std::vector<std::uint32_t>{
            static_cast<std::uint32_t>(base >> 8u),
            static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (56u << 20u) | (((width - 1u) & 3u) << 30u),
            ((width - 1u) >> 2u) | ((side - 1u) << 14u),
            0xfacu | (sampleShift << 16u) | (14u << 28u),
            0u,
            sampleShift << 4u,
            0u,
            0u,
        };
    };
    const auto sampledProgram = [&](ShaderRecompiler::DescriptorKind kind, std::vector<std::uint32_t> words, ShaderRecompiler::DescriptorImageShape shape = ShaderRecompiler::DescriptorImageShape::Image2DMultisample) {
        ShaderRecompiler::RecompileResult result;
        auto element = binding;
        element.kind = kind;
        if (kind == ShaderRecompiler::DescriptorKind::StorageImage) element.binding = ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding;
        element.imageShape = shape;
        element.guestDescriptor = std::move(words);
        result.bindings.push_back(std::move(element));
        return result;
    };
    const auto refusedSample = [&](ShaderRecompiler::DescriptorKind kind, std::vector<std::uint32_t> words, const std::string& expected, const std::string& what) {
        const auto refusedProgram = sampledProgram(kind, std::move(words));
        const CompiledShader refusedCompute{ShaderRecompiler::ShaderStage::Compute, &refusedProgram, 0};
        std::string message;
        try {
            std::lock_guard gpu(GpuMutex());
            ShaderResources resources(context, refusedCompute);
        } catch (const std::exception& error) {
            message = error.what();
        }
        Require(message.find(expected) != std::string::npos, what + " did not throw: " + message);
    };
    {
        const auto sampled = sampledProgram(ShaderRecompiler::DescriptorKind::SampledImage, multisampledWords(side, 2u, address));
        const auto sampledArray = sampledProgram(ShaderRecompiler::DescriptorKind::SampledImage, multisampledWords(side, 2u, address), ShaderRecompiler::DescriptorImageShape::Image2DMultisampleArray);
        auto bothShapes = sampled;
        auto arrayBinding = sampledArray.bindings.front();
        arrayBinding.binding = ShaderRecompiler::RuntimeAbi::FirstImageBinding + 1u;
        bothShapes.bindings.push_back(std::move(arrayBinding));
        const CompiledShader sampledCompute{ShaderRecompiler::ShaderStage::Compute, &sampled, 0};
        const CompiledShader sampledArrayCompute{ShaderRecompiler::ShaderStage::Compute, &sampledArray, 0};
        const CompiledShader bothShapesCompute{ShaderRecompiler::ShaderStage::Compute, &bothShapes, 0};
        std::lock_guard gpu(GpuMutex());
        ShaderResources first(context, sampledCompute);
        ShaderResources second(context, sampledCompute);
        ShaderResources arrayFirst(context, sampledArrayCompute);
        ShaderResources arraySecond(context, sampledArrayCompute);
        ShaderResources combined(context, bothShapesCompute);
        Require(first.SampledTextures().size() == 1 && first.SampledTextures().front() != nullptr, "the multisampled target was not bound as a sampled texture");
        Require(first.SampledTextures().front()->Layout() == VK_IMAGE_LAYOUT_GENERAL && first.SampledTextures().front()->ViewFormat() == target.format, "the sampled multisampled target is not a view of the target's image in its layout");
        Require(first.SampledTextures().front()->SampledViewRange(false).type == VK_IMAGE_VIEW_TYPE_2D, "Image2DMultisample did not bind a 2D image view");
        Require(second.SampledTextures().front() == first.SampledTextures().front(), "a second lookup of the multisampled target made another texture");
        Require(arrayFirst.SampledTextures().size() == 1 && arrayFirst.SampledTextures().front() != nullptr, "the multisampled array target was not bound as a sampled texture");
        Require(arrayFirst.SampledTextures().front()->SampledViewRange(false).type == VK_IMAGE_VIEW_TYPE_2D_ARRAY, "Image2DMultisampleArray did not bind a 2D array image view");
        Require(arrayFirst.SampledTextures().front()->View() != first.SampledTextures().front()->View(), "Image2DMultisampleArray reused the non-arrayed multisampled image view");
        Require(arraySecond.SampledTextures().front() == arrayFirst.SampledTextures().front(), "a second lookup of the multisampled array target made another texture");
        Require(combined.SampledTextures().size() == 2 && combined.SampledTextures()[0] == first.SampledTextures().front() && combined.SampledTextures()[1] == arrayFirst.SampledTextures().front(), "consecutive multisampled bindings of different shapes did not preserve their distinct views");
        Require(first.MultisampleSources() == std::vector<std::uint64_t>{address} && !first.Reusable(), "resources sampling a multisampled target are kept for reuse without the barrier their lookup records");
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    }
    refusedSample(ShaderRecompiler::DescriptorKind::SampledImage, multisampledWords(side, 3u, address), "as a 64x64 texture of 8 samples", "sampling a 4x target as an 8x texture");
    refusedSample(ShaderRecompiler::DescriptorKind::SampledImage, multisampledWords(side / 2u, 2u, address), "as a 32x64 texture of 4 samples", "sampling a 4x target as a narrower texture");
    refusedSample(ShaderRecompiler::DescriptorKind::SampledImage, multisampledWords(side, 2u, address + 4u * 65536u), "which no draw rendered to and whose tile mode 0 has no modeled multisampled color footprint", "sampling a linear multisampled texture no draw rendered to");
    if (context.storageImageMultisample) {
        const auto stored = sampledProgram(ShaderRecompiler::DescriptorKind::StorageImage, multisampledWords(side, 2u, address));
        const CompiledShader storedCompute{ShaderRecompiler::ShaderStage::Compute, &stored, 0};
        std::lock_guard gpu(GpuMutex());
        const auto rendered = MultisampleTargetView(context, target);
        Require(MultisampleTargetStorable(context, target) == ordered, "sampling the multisampled target changed its storage usage");
        ShaderResources resources(context, storedCompute);
        Require(MultisampleTargetStorable(context, target) && (MultisampleTargetView(context, target) != rendered) != ordered, "a storage binding did not move the multisampled target to an image with storage usage, or moved one that had it");
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    } else {
        refusedSample(ShaderRecompiler::DescriptorKind::StorageImage, multisampledWords(side, 2u, address), "as a storage image at its sample count", "storage access to a multisampled target without shaderStorageImageMultisample");
    }
    refused([&] { AgcDriver::GuestMemory::Read(address + 3u * 65536u + 256u, texels); }, "a CPU read", "a CPU read in the padding of a later sample plane");
    AgcDriver::GuestMemory::Read(address + 4u * 65536u, texels);
    clearTargets();
    target.tileMode = ColorTileMode::RenderTarget;
    {
        std::lock_guard gpu(GpuMutex());
        Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "the tiled multisampled target has no view");
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    }
    refused([&] { AgcDriver::GuestMemory::Read(address + targetBytes - texels.size(), texels); }, "a CPU read", "a CPU read inside the tiled multisampled target");
    AgcDriver::GuestMemory::Read(address + targetBytes, texels);
    clearTargets();
    if ((base.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_2_BIT) != 0) {
        target.extent = {128, 64};
        target.surfaceExtent = target.extent;
        target.samples = 2;
        {
            std::lock_guard gpu(GpuMutex());
            Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "the 2x tiled multisampled target has no view");
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
        }
        refused([&] { AgcDriver::GuestMemory::Read(address + 2u * targetBytes - texels.size(), texels); }, "a CPU read", "a CPU read inside the 2x tile-height padding");
        AgcDriver::GuestMemory::Read(address + 2u * targetBytes, texels);
        clearTargets();
    }
    if ((base.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_8_BIT) != 0) {
        target.extent = {64, 32};
        target.surfaceExtent = target.extent;
        target.samples = 8;
        {
            std::lock_guard gpu(GpuMutex());
            Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "the 8x tiled multisampled target has no view");
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
        }
        refused([&] { AgcDriver::GuestMemory::Read(address + 2u * targetBytes - texels.size(), texels); }, "a CPU read", "a CPU read inside the 8x tile-height padding");
        AgcDriver::GuestMemory::Read(address + 2u * targetBytes, texels);
        clearTargets();
    }
    AgcDriver::GuestMemory::Read(address + 256u, texels);
    target.extent = {side, side};
    target.surfaceExtent = target.extent;
    target.samples = 4;
    VkImageView previous = VK_NULL_HANDLE;
    std::shared_ptr<Texture> previousTexture;
    for (const bool removeAllocation : {false, true}) {
        VkImageView view = VK_NULL_HANDLE;
        std::shared_ptr<Texture> texture;
        {
            const auto sampled = sampledProgram(ShaderRecompiler::DescriptorKind::SampledImage, multisampledWords(side, 2u, address));
            const CompiledShader sampledCompute{ShaderRecompiler::ShaderStage::Compute, &sampled, 0};
            std::lock_guard gpu(GpuMutex());
            view = MultisampleTargetView(context, target);
            Require(view != VK_NULL_HANDLE && view != previous, "a remapped allocation reused its retired multisampled image");
            ShaderResources resources(context, sampledCompute);
            texture = resources.SampledTextures().front();
            Require(texture != nullptr && texture != previousTexture, "a remapped allocation sampled its retired multisampled image");
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
        }
        {
            GuestAllocations::Mutation mutation;
            mutation.Protect(block, bytes, true, true, true, [] {});
        }
        refused([&] { RequireNoMultisampleTarget(address, 64u, "a CPU read"); }, "a CPU read", "a read after protection changed");
        bool failed = false;
        try {
            GuestAllocations::Mutation mutation;
            mutation.Unmap(block, bytes, [](const void*, std::size_t, const void*, bool) { throw std::runtime_error("unmap failed"); });
        } catch (const std::runtime_error&) {
            failed = true;
        }
        Require(failed, "the failed host unmap did not throw");
        refused([&] { RequireNoMultisampleTarget(address, 64u, "a CPU read"); }, "a CPU read", "a read after a failed unmap");
        {
            GuestAllocations::Mutation mutation;
            if (removeAllocation) mutation.Remove(block);
            else mutation.Unmap(block, bytes, [](const void*, std::size_t, const void*, bool) {});
            mutation.Add(block, bytes, true, true, true);
        }
        RequireNoMultisampleTarget(address, bytes, "a texture read");
        AgcDriver::GuestMemory::Read(address + 256u, texels);
        refusedSample(ShaderRecompiler::DescriptorKind::SampledImage, multisampledWords(side, 2u, address), "which no draw rendered to", "sampling a multisampled target whose allocation was unmapped");
        previous = view;
        previousTexture = texture;
    }
    clearTargets();
    std::cout << "Multisampled target guest access tests passed\n";
}

void oneFragmentFmaskGuardTests(const Device& device) {
    const auto& base = device.GetContext();
    if (!device.NullDescriptors()) {
        std::cout << "nullDescriptor unavailable: one-fragment FMASK guard not tested\n";
        return;
    }
    constexpr std::uint32_t side = 64;
    constexpr std::size_t bytes = 3u * 65536u;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the one-fragment FMASK block");
    std::memset(block, 0, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            ClearMultisampleTargets(context.device);
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
#ifdef _WIN32
            VirtualFree(block, 0, MEM_RELEASE);
#else
            std::free(block);
#endif
        }
    } unregister{base, block};
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    context.nullDescriptors = true;
    TextureCache cache(context);
    context.textureCache = &cache;
    const auto refused = [](const std::function<void()>& access, const std::string& what) {
        std::string message;
        try {
            access();
        } catch (const std::exception& error) {
            message = error.what();
        }
        Require(message.find("FMASK") != std::string::npos && message.find("one-fragment color target") != std::string::npos, what + " of a host-drawn one-fragment target's FMASK did not throw: " + message);
    };
    ColorTarget target{};
    target.extent = {128, 128};
    target.samples = 1;
    target.coverageSamples = 4;
    target.fmaskAddress = address;
    GuardOneFragmentFmask(context, target);
    GuardOneFragmentFmask(context, target);
    std::array<std::byte, 64> words{};
    refused([&] { AgcDriver::GuestMemory::Read(address + 256u, words); }, "a CPU read");
    refused([&] { RequireNoMultisampleTarget(address + 65535u, 1u, "a DMA copy"); }, "a DMA copy of the last guarded byte");
    RequireNoMultisampleTarget(address + 65536u, 64u, "a DMA copy");
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = ShaderRecompiler::RuntimeAbi::FirstImageBinding;
    binding.count = 1;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageSamplers = {0};
    binding.guestDescriptor = {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        0xfacu | (9u << 28u),
        0u,
        0u,
        0u,
        0u,
    };
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    for (const char* what : {"a texture read", "a repeated texture read"}) {
        refused([&] {
            std::lock_guard gpu(GpuMutex());
            ShaderResources resources(context, compute);
        }, what);
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(block, bytes, [](const void*, std::size_t, const void*, bool) {});
        mutation.Add(block, bytes, true, true, true);
    }
    AgcDriver::GuestMemory::Read(address + 256u, words);
    RequireNoMultisampleTarget(address, 65536u, "a DMA copy");
    std::cout << "One-fragment FMASK guard tests passed\n";
}

void fmaskDecompressMsaaTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if ((base.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0 || base.hostImportAlignment == 0) {
        std::cout << "4x color targets or host imports unavailable: FMASK decompress not tested\n";
        return;
    }
    constexpr std::uint32_t side = 128;
    constexpr std::size_t targetBytes = 65536u;
    constexpr std::size_t cmaskBytes = 4096u;
    constexpr std::size_t destBytes = targetBytes;
    constexpr std::size_t bytes = 5u * 65536u + cmaskBytes + 2u * destBytes;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, (bytes + 65535u) & ~std::size_t{65535u});
#endif
    Require(block != nullptr, "cannot allocate the FMASK decompress block");
    std::memset(block, 0, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    const auto cmaskAddress = address + 5u * 65536u;
    const auto destAddress = cmaskAddress + cmaskBytes;
    const auto destAddress2 = destAddress + destBytes;
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            ClearMultisampleTargets(context.device);
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
#ifdef _WIN32
            VirtualFree(block, 0, MEM_RELEASE);
#else
            std::free(block);
#endif
        }
    } unregister{base, block};
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    ColorTarget source{};
    source.address = address;
    source.surfaceAddress = address;
    source.extent = {side, side};
    source.surfaceExtent = source.extent;
    source.format = VK_FORMAT_R8G8B8A8_UNORM;
    source.elementBytes = 4;
    source.bytes = targetBytes;
    source.samples = 4;
    source.cmaskAddress = cmaskAddress;
    source.cmaskBytes = cmaskBytes;
    source.cmaskFastClear = true;
    source.clearWords = {0x11223344u, 0};
    ColorTarget destination{};
    destination.address = destAddress;
    destination.surfaceAddress = destAddress;
    destination.extent = {side, side};
    destination.surfaceExtent = destination.extent;
    destination.format = VK_FORMAT_R8G8B8A8_UNORM;
    destination.elementBytes = 4;
    destination.bytes = destBytes;
    destination.samples = 1;
    destination.tileMode = ColorTileMode::RenderTarget;
    {
        std::lock_guard gpu(GpuMutex());
        RunColorMetadataPass(context, {ColorMetadataPass::Mode::FmaskDecompress, {source}});
        auto recolored = source;
        recolored.clearWords = {0x55667788u, 0};
        RunColorMetadataPass(context, {ColorMetadataPass::Mode::Resolve, {destination}, recolored});
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    }
    std::vector<std::uint32_t> resolved(destBytes / 4);
    AgcDriver::GuestMemory::Read(destAddress, std::as_writable_bytes(std::span(resolved)));
    Require(std::ranges::all_of(resolved, [](auto word) { return word == 0x11223344u; }), "FMASK decompress did not materialize the seeded clear to the host multisampled image");
    source.clearWords = {0x55667788u, 0};
    ColorTarget destination2 = destination;
    destination2.address = destAddress2;
    destination2.surfaceAddress = destAddress2;
    {
        std::lock_guard gpu(GpuMutex());
        RunColorMetadataPass(context, {ColorMetadataPass::Mode::FmaskDecompress, {source}});
        RunColorMetadataPass(context, {ColorMetadataPass::Mode::Resolve, {destination2}, source});
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    }
    std::vector<std::uint32_t> resolved2(destBytes / 4);
    AgcDriver::GuestMemory::Read(destAddress2, std::as_writable_bytes(std::span(resolved2)));
    Require(std::ranges::all_of(resolved2, [](auto word) { return word == 0x11223344u; }), "subsequent FMASK decompress re-cleared the multisampled target");
    std::cout << "FMASK decompress MSAA clear and resolve tests passed\n";
}

void tiledResolveTests(const Device& device, Recorder& recorder) {
    namespace GuestMemory = AgcDriver::GuestMemory;
    auto context = device.GetContext();
    if ((context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0 || !device.Graphics()) {
        std::cout << "4x color targets or graphics unavailable: CB resolves into tiled targets not tested\n";
        return;
    }
    context.hostImportAlignment = 0;
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    constexpr std::uint32_t side = 128;
    constexpr std::size_t targetBytes = side * side * 4u;
    constexpr std::size_t sourceBytes = targetBytes * 4u;
    constexpr std::size_t bytes = targetBytes + sourceBytes + 65536u;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the tiled resolve block");
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Release {
        const Context& context;
        void* block;
        ~Release() {
            ClearMultisampleTargets(context.device);
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
#ifdef _WIN32
            VirtualFree(block, 0, MEM_RELEASE);
#else
            std::free(block);
#endif
        }
    } release{context, block};
    ColorTarget source{};
    source.address = address + targetBytes;
    source.surfaceAddress = source.address;
    source.extent = {side, side};
    source.surfaceExtent = source.extent;
    source.format = VK_FORMAT_R8G8B8A8_UNORM;
    source.bytes = targetBytes;
    source.componentMapping = 0xe4u;
    source.tileMode = ColorTileMode::RenderTarget;
    source.samples = 4;
    source.cmaskAddress = address + targetBytes + sourceBytes;
    source.cmaskBytes = 4096;
    source.cmaskFastClear = true;
    const std::array<std::pair<ColorTileMode, std::uint32_t>, 3> modes{{{ColorTileMode::Standard4KB, 0x80402010u}, {ColorTileMode::Standard64KB, 0x10204080u}, {ColorTileMode::RenderTarget, 0x40801020u}}};
    for (const auto& [mode, clear] : modes) {
        const auto name = std::to_string(static_cast<std::uint32_t>(mode));
        std::memset(block, 0x55, targetBytes);
        std::memset(static_cast<std::byte*>(block) + targetBytes + sourceBytes, 0, 4096);
        ColorTarget destination{};
        destination.address = address;
        destination.surfaceAddress = address;
        destination.extent = {side, side};
        destination.surfaceExtent = destination.extent;
        destination.format = VK_FORMAT_R8G8B8A8_UNORM;
        destination.bytes = targetBytes;
        destination.componentMapping = 0xe4u;
        destination.tileMode = mode;
        source.clearWords = {clear, 0};
        RunColorMetadataPass(context, {ColorMetadataPass::Mode::Resolve, {destination}, source});
        std::vector<std::uint32_t> resolved(targetBytes / 4);
        GuestMemory::Read(address, std::as_writable_bytes(std::span(resolved)));
        const auto texels = std::ranges::count(resolved, clear);
        Require(texels == static_cast<std::ptrdiff_t>(resolved.size()), "a CB resolve into a tile mode " + name + " target left " + std::to_string(resolved.size() - static_cast<std::size_t>(texels)) + " texels without the resolved color");
        recorder.Sync();
        ClearMultisampleTargets(context.device);
        ClearCachedTextures(context.device);
    }
    {
        ColorTarget destination{};
        destination.address = address;
        destination.extent = {side, side};
        destination.format = VK_FORMAT_R8G8B8A8_UNORM;
        destination.bytes = targetBytes;
        destination.tileMode = ColorTileMode::Linear;
        std::string message;
        try {
            RunColorMetadataPass(context, {ColorMetadataPass::Mode::Resolve, {destination}, source});
        } catch (const std::exception& error) {
            message = error.what();
        }
        Require(message.find("without a resident image is not implemented") != std::string::npos, "a CB resolve into a linear target did not throw: " + message);
    }
    recorder.Sync();
}

void depthSurfaceProofTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (!device.NullDescriptors()) {
        std::cout << "nullDescriptor unavailable: depth surface fast proof not tested\n";
        return;
    }
    constexpr std::uint32_t side = 64;
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the depth surface block");
    std::memset(block, 0, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            ClearDepthSurfaces(context.device);
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
#ifdef _WIN32
            VirtualFree(block, 0, MEM_RELEASE);
#else
            std::free(block);
#endif
        }
    } unregister{base, block};
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    context.nullDescriptors = true;
    TextureCache cache(context);
    context.textureCache = &cache;
    const DepthTarget target{address, 0, {side, side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    Require(DepthSurfaceView(context, target) != VK_NULL_HANDLE, "the depth surface has no view");
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = ShaderRecompiler::RuntimeAbi::FirstImageBinding;
    binding.count = 1;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageSamplers = {0};
    binding.guestDescriptor = {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (22u << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        0xfacu | (9u << 28u),
        0u,
        0u,
        0u,
        0u,
    };
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    bool reusable = false;
    std::array<bool, 2> proved{};
    std::array<ShaderResources::ProofReport, 2> reports{};
    {
        ShaderResources resources(context, compute);
        reusable = resources.Reusable();
        for (std::size_t use = 0; reusable && use < reports.size(); ++use) proved[use] = resources.Revalidate(compute, &reports[use]);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
    }
    Require(reusable, "a set sampling only a depth surface is not reusable");
    for (std::size_t use = 0; use < reports.size(); ++use) {
        const auto name = std::to_string(use + 2);
        Require(proved[use], "a set sampling an unchanged depth surface failed its proof on use " + name);
        Require(reports[use].path == ShaderResources::ProofPath::Fast, "a set sampling an unchanged depth surface left the fast proof for the full walk on use " + name);
    }
    const auto resource = DecodeTextureResource(binding.guestDescriptor);
    const VkComponentMapping identity{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    const auto texture = DepthSurfaceTexture(context, binding.guestDescriptor, resource, identity);
    Require(texture != nullptr && DepthSurfaceHolds(context, resource, texture.get()), "a 2D depth lookup is not fast-provable");
    auto arrayed = resource;
    arrayed.dimension = TextureDimension::k2DArray;
    Require(!DepthSurfaceHolds(context, arrayed, texture.get()), "a 2D-array depth lookup took the fast proof instead of the full walk");
    auto layered = resource;
    layered.baseArray = 1;
    Require(!DepthSurfaceHolds(context, layered, texture.get()), "a layered depth lookup took the fast proof instead of the full walk");
}

}

class SampleProgram {
public:
    SampleProgram(const Context& context, Recorder& recorder, std::span<const std::uint32_t> code = SAMPLE_LOD_SPV) : context(context), recorder(recorder), result(context, sizeof(float) * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
        const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float) * 2};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &setLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &push;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = code.size_bytes();
        moduleInfo.pCode = code.data();
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
        pipelineInfo.layout = pipelineLayout;
        Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &samplerInfo, nullptr, &sampler), "vkCreateSampler");
        const VkDescriptorPoolSize sizes[2]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = sizes;
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
    }
    ~SampleProgram() {
        context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, sampler, nullptr);
        context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
        context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }
    SampleProgram(const SampleProgram&) = delete;
    SampleProgram& operator=(const SampleProgram&) = delete;

    float Red(VkImageView view, VkImageLayout layout, float lod, float layer = 0) {
        VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocateInfo.descriptorPool = pool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &setLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocateInfo, &set), "vkAllocateDescriptorSets");
        const VkDescriptorImageInfo image{sampler, view, layout};
        const VkDescriptorBufferInfo buffer{result.Handle(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet writes[2]{{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        writes[0].dstSet = set;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &image;
        writes[1].dstSet = set;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &buffer;
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 2, writes, 0, nullptr);
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
        const float parameters[]{lod, layer};
        context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(parameters), parameters);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, 1, 1, 1);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        recorder.Sync();
        Check(context.Function<PFN_vkFreeDescriptorSets>("vkFreeDescriptorSets")(context.device, pool, 1, &set), "vkFreeDescriptorSets");
        float texel[4];
        std::memcpy(texel, result.Bytes().data(), sizeof(texel));
        return texel[0];
    }

private:
    const Context& context;
    Recorder& recorder;
    Buffer result;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};

void expectRed(float got, float want, const char* what) {
    if (std::abs(got - want) > 1.5f / 255.0f) throw std::runtime_error(std::string(what) + ": read " + std::to_string(got) + ", expected " + std::to_string(want));
}

void minLodTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (!context.imageViewMinLod) {
        std::cout << "VK_EXT_image_view_min_lod unavailable: minimum LOD clamp not tested\n";
        return;
    }
    TextureDetiler detiler(context);
    GuestTextureResource resource{};
    resource.baseAddress = 0x100000;
    resource.width = 8;
    resource.height = 8;
    resource.mipCount = 4;
    resource.baseLevel = 0;
    resource.lastLevel = 3;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    Require(geometry.mips.size() == 4, "the min LOD test surface has an unexpected mip count");
    const std::array<std::uint8_t, 4> levels{0x00, 0x40, 0x80, 0xff};
    std::vector<std::byte> snapshot(static_cast<std::size_t>(geometry.guestBytes));
    for (std::size_t level = 0; level < levels.size(); ++level) {
        const auto& mip = geometry.mips[level];
        std::fill_n(snapshot.begin() + static_cast<std::ptrdiff_t>(mip.tiledOffset), static_cast<std::size_t>(mip.tiledSize), std::byte{levels[level]});
    }
    SampleProgram program(context, recorder);
    const VkComponentMapping identity{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    const auto sample = [&](std::uint32_t minLod, float lod, std::uint32_t baseLevel = 0) {
        auto described = resource;
        described.minLod = minLod;
        described.baseLevel = baseLevel;
        Texture texture(context, detiler, described, identity, snapshot);
        return program.Red(texture.View(), texture.Layout(), lod);
    };
    const auto unorm = [&](std::size_t level) { return levels[level] / 255.0f; };
    expectRed(sample(0, 0.0f), unorm(0), "minimum LOD clamp: no clamp reads level 0");
    expectRed(sample(0x100, 0.0f), unorm(1), "minimum LOD clamp: MIN_LOD 1 reads level 1 at LOD 0");
    const auto fractional = sample(0x180, 0.0f);
    if (std::abs(fractional - unorm(1)) <= 1.5f / 255.0f) {
        std::cout << "minimum LOD clamp: the device takes the integer part of a fractional view minimum LOD\n";
    } else {
        expectRed(fractional, (unorm(1) + unorm(2)) / 2.0f, "minimum LOD clamp: MIN_LOD 1.5 blends levels 1 and 2");
    }
    expectRed(sample(0x100, 2.0f), unorm(2), "minimum LOD clamp: MIN_LOD 1 lowered LOD 2");
    expectRed(sample(0xfff, 0.0f), unorm(3), "minimum LOD clamp: MIN_LOD past the last level reads the last level");
    expectRed(sample(0x200, 0.0f, 1), unorm(2), "minimum LOD clamp: MIN_LOD 2 over a view from level 1 reads level 2");
    expectRed(sample(0x100, 0.0f, 1), unorm(1), "minimum LOD clamp: MIN_LOD at the view's base level reads its base level");
}

void singleCubeTests(const Device& device, Recorder& recorder) {
    auto context = device.GetContext();
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    SampleProgram program(context, recorder, SAMPLE_ARRAY_SPV);
    const VkComponentMapping identity{};
    for (const auto first : {0u, 5u, 6u}) {
        std::array<std::uint32_t, 8> words{0x1000u, (56u << 20u) | (3u << 30u), 15u | (63u << 14u), 0xb0000facu, first | (first << 16u), 0, 0, 0};
        auto resource = DecodeTextureResource(words);
        const auto geometry = DescribeSurface(resource);
        std::vector<std::byte> memory(static_cast<std::size_t>(geometry.guestBytes) + 256);
        auto* surface = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(memory.data()) + 255) & ~std::uintptr_t{255});
        for (std::uint32_t layer = 0; layer < geometry.imageLayers; ++layer) {
            std::memset(surface + geometry.GuestLayerOffset(layer), 16 * (layer + 1), static_cast<std::size_t>(geometry.layerBytes));
        }
        resource.baseAddress = reinterpret_cast<std::uint64_t>(surface);
        const std::span<const std::byte> snapshot(surface, static_cast<std::size_t>(geometry.guestBytes));
        Texture texture(context, detiler, resource, identity, snapshot);
        auto storage = StorageTexture::Create(context, detiler, resource, 0);
        recorder.Keep(storage);
        Texture storageView(context, storage, resource, identity);
        for (std::uint32_t face = 0; face < 6; ++face) {
            const auto expected = 16.0f * (first + face + 1) / 255.0f;
            expectRed(program.Red(texture.View(), texture.Layout(), 0, face), expected, "single cube snapshot sampled the wrong face");
            expectRed(program.Red(storageView.View(), storageView.Layout(), 0, face), expected, "single cube storage view sampled the wrong face");
        }
    }
}

void cubeExtraSliceTests(const Device& device, Recorder& recorder) {
    auto context = device.GetContext();
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    SampleProgram program(context, recorder, SAMPLE_ARRAY_SPV);
    const VkComponentMapping identity{};
    std::array<std::uint32_t, 8> words{0x1000u, (56u << 20u) | (3u << 30u), 15u | (63u << 14u), 0xb0000facu, 6u, 0, 0, 0};
    auto resource = DecodeTextureResource(words);
    const auto geometry = DescribeSurface(resource);
    Require(geometry.imageLayers == 7u, "a cube with a slice past its last whole cube does not allocate it");
    std::vector<std::byte> memory(static_cast<std::size_t>(geometry.guestBytes) + 256);
    auto* surface = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(memory.data()) + 255) & ~std::uintptr_t{255});
    for (std::uint32_t layer = 0; layer < geometry.imageLayers; ++layer) {
        std::memset(surface + geometry.GuestLayerOffset(layer), 16 * (layer + 1), static_cast<std::size_t>(geometry.layerBytes));
    }
    resource.baseAddress = reinterpret_cast<std::uint64_t>(surface);
    const std::span<const std::byte> snapshot(surface, static_cast<std::size_t>(geometry.guestBytes));
    Texture texture(context, detiler, resource, identity, snapshot);
    auto storage = StorageTexture::Create(context, detiler, resource, 0);
    recorder.Keep(storage);
    Texture storageView(context, storage, resource, identity);
    const auto wordCube = [&words](std::vector<std::byte>& backing, std::uint32_t format, const std::function<std::uint32_t(std::uint32_t)>& word) {
        auto cubeWords = words;
        cubeWords[1] = (format << 20u) | (3u << 30u);
        auto described = DecodeTextureResource(cubeWords);
        const auto layout = DescribeSurface(described);
        backing.assign(static_cast<std::size_t>(layout.guestBytes) + 256, std::byte{});
        auto* base = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(backing.data()) + 255) & ~std::uintptr_t{255});
        for (std::uint32_t layer = 0; layer < layout.imageLayers; ++layer) {
            const auto value = word(layer);
            for (std::uint64_t offset = 0; offset < layout.layerBytes; offset += sizeof(value)) std::memcpy(base + layout.GuestLayerOffset(layer) + offset, &value, sizeof(value));
        }
        described.baseAddress = reinterpret_cast<std::uint64_t>(base);
        return described;
    };
    std::vector<std::byte> depthMemory;
    const auto depth = wordCube(depthMemory, 22u, [](std::uint32_t layer) {
        const float value = static_cast<float>(layer + 1) / 8.0f;
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    });
    auto depthStorage = StorageTexture::Create(context, detiler, depth, 0);
    recorder.Keep(depthStorage);
    Require(Texture::CanDepthCopyFrom(*depthStorage, depth), "a cube with a slice past its last whole cube cannot take a depth copy");
    Texture depthCopy(context, depthStorage, depth, identity, Texture::DepthCopyTag{});
    depthCopy.RefreshDepthCopy();
    constexpr std::array<std::uint32_t, 7> unsignedFloats{960u, 1024u, 1056u, 1088u, 1104u, 1120u, 1136u};
    std::vector<std::byte> repackMemory;
    const auto repacked = wordCube(repackMemory, 43u, [&unsignedFloats](std::uint32_t layer) { return unsignedFloats[layer] << 21u; });
    auto repackStorage = StorageTexture::Create(context, detiler, repacked, 0);
    recorder.Keep(repackStorage);
    Require(Texture::CanRepackCopyFrom(*repackStorage, repacked), "a cube with a slice past its last whole cube cannot take a repacked copy");
    Texture repackCopy(context, repackStorage, repacked, identity, Texture::RepackCopyTag{});
    repackCopy.RefreshDepthCopy();
    for (std::uint32_t layer = 0; layer < geometry.imageLayers; ++layer) {
        const auto expected = 16.0f * (layer + 1) / 255.0f;
        expectRed(program.Red(texture.View(), texture.Layout(), 0, static_cast<float>(layer)), expected, "a cube snapshot with a slice past its last whole cube sampled the wrong slice");
        expectRed(program.Red(storageView.View(), storageView.Layout(), 0, static_cast<float>(layer)), expected, "a cube storage view with a slice past its last whole cube sampled the wrong slice");
        expectRed(program.Red(depthCopy.View(), depthCopy.Layout(), 0, static_cast<float>(layer)), static_cast<float>(layer + 1) / 8.0f, "a cube depth copy with a slice past its last whole cube sampled the wrong slice");
        expectRed(program.Red(repackCopy.View(), repackCopy.Layout(), 0, static_cast<float>(layer)), static_cast<float>(layer + 1), "a cube repacked copy with a slice past its last whole cube sampled the wrong slice");
    }
}

struct SampledViewProbe {
    Context& context;
    PFN_vkGetDeviceProcAddr resolver;
    std::uint32_t views = 0;
    bool checking = false;
    static inline SampledViewProbe* active = nullptr;
    explicit SampledViewProbe(Context& context) : context(context), resolver(context.deviceProc) {
        Require(active == nullptr, "sampled view probe is already active");
        active = this;
        context.deviceProc = resolve;
    }
    ~SampledViewProbe() {
        context.deviceProc = resolver;
        active = nullptr;
    }
    static VKAPI_ATTR VkResult VKAPI_CALL createView(VkDevice device, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* callbacks, VkImageView* created) {
        if (active->checking && info->format == VK_FORMAT_R8G8B8A8_SRGB) {
            const auto* next = static_cast<const VkBaseInStructure*>(info->pNext);
            while (next != nullptr && next->sType != VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO) next = next->pNext;
            if (next == nullptr || reinterpret_cast<const VkImageViewUsageCreateInfo*>(next)->usage != VK_IMAGE_USAGE_SAMPLED_BIT) return VK_ERROR_VALIDATION_FAILED_EXT;
            ++active->views;
        }
        return reinterpret_cast<PFN_vkCreateImageView>(active->resolver(device, "vkCreateImageView"))(device, info, callbacks, created);
    }
    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL resolve(VkDevice device, const char* name) {
        if (std::strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(createView);
        return active->resolver(device, name);
    }
};

void sampledStorageBaseLevelUsageTests(const Device& device, Recorder& recorder) {
    auto context = device.GetContext();
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.width = resource.height = 8;
    resource.depthOrLastArray = 1;
    resource.mipCount = 2;
    resource.lastLevel = 1;
    resource.minLod = context.imageViewMinLod ? 0x100 : 0;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2DArray;
    resource.format = 130;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    std::vector<std::byte> memory(static_cast<std::size_t>(geometry.guestBytes) + 256, std::byte{0x80});
    resource.baseAddress = (reinterpret_cast<std::uintptr_t>(memory.data()) + 255) & ~std::uintptr_t{255};
    auto storage = StorageTexture::Create(context, detiler, resource, 0);
    recorder.Keep(storage);
    recorder.Sync();
    SampleProgram program(context, recorder);
    auto sampledContext = context;
    SampledViewProbe probe(sampledContext);
    Texture sampled(sampledContext, storage, resource, VkComponentMapping{});
    probe.checking = true;
    const auto base = sampled.BaseLevelView();
    probe.checking = false;
    expectRed(program.Red(base, sampled.Layout(), 0), 0.21586f, "sampled sRGB base level");
    Require(sampled.BaseLevelView() == base && probe.views == 1, "the lazy base-level view of a sampled storage image must restrict usage to SAMPLED and be reused");
}

void firstLayerViewTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    TextureDetiler detiler(context);
    auto withDetiler = context;
    withDetiler.detiler = &detiler;
    GuestTextureResource resource{};
    resource.width = 64;
    resource.height = 4;
    resource.depthOrLastArray = 2;
    resource.baseArray = 1;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2DArray;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    std::vector<std::uint8_t> memory(static_cast<std::size_t>(geometry.guestBytes) + 256);
    auto* surface = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(memory.data()) + 255) & ~std::uintptr_t{255});
    for (std::uint32_t layer = 0; layer < 3; ++layer) std::memset(surface + geometry.GuestLayerOffset(layer), 0x20 * (layer + 1), static_cast<std::size_t>(geometry.layerBytes));
    resource.baseAddress = reinterpret_cast<std::uint64_t>(surface);
    auto image = StorageTexture::Create(withDetiler, detiler, resource, 0);
    recorder.Keep(image);
    SampleProgram program(context, recorder);
    expectRed(program.Red(image->FirstLayerView(0), VK_IMAGE_LAYOUT_GENERAL, 0.0f), 0x40 / 255.0f, "a first-layer view does not read the BASE_ARRAY layer");
    Require(image->FirstLayerView(0) == image->FirstLayerView(0), "first-layer views are not reused");
    const VkComponentMapping identity{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    Texture snapshotTexture(context, detiler, resource, identity, std::span<const std::byte>(reinterpret_cast<const std::byte*>(surface), static_cast<std::size_t>(geometry.guestBytes)));
    Require(snapshotTexture.FirstLayerView() != VK_NULL_HANDLE, "a sampled 2D array texture has no first-layer view");
    expectRed(program.Red(snapshotTexture.FirstLayerView(), snapshotTexture.Layout(), 0.0f), 0x40 / 255.0f, "a sampled first-layer view does not read the BASE_ARRAY layer");
    Texture storageView(context, image, resource, identity);
    Require(storageView.FirstLayerView() != VK_NULL_HANDLE, "a sampled view of a 2D array storage image has no first-layer view");
    expectRed(program.Red(storageView.FirstLayerView(), storageView.Layout(), 0.0f), 0x40 / 255.0f, "a sampled first-layer view of a storage image does not read the BASE_ARRAY layer");
    auto flat = resource;
    flat.dimension = TextureDimension::k2D;
    flat.depthOrLastArray = 0;
    flat.baseArray = 0;
    Texture flatTexture(context, detiler, flat, identity, std::span<const std::byte>(reinterpret_cast<const std::byte*>(surface), static_cast<std::size_t>(DescribeSurface(flat).guestBytes)));
    Require(flatTexture.FirstLayerView() == VK_NULL_HANDLE, "a 2D texture has a first-layer view");
    expectRed(program.Red(flatTexture.View(), flatTexture.Layout(), 0.0f), 0x20 / 255.0f, "a 2D texture over the surface does not read its first layer");
}

void sliceDirtyTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    constexpr std::uint32_t side = 128;
    constexpr std::size_t sliceBytes = 65536;
    constexpr std::size_t bytes = 4 * sliceBytes;
    constexpr std::uint8_t untouched = 0x11;
    for (const auto tileMode : {TextureTileMode::kLinear, TextureTileMode::kR64KBX}) {
        const std::string what = tileMode == TextureTileMode::kLinear ? "a linear two-slice image" : "a tiled two-slice image";
        void* block = AllocateWatched(bytes, 65536);
        if (block == nullptr) {
            std::cout << "no write watching: slice dirty marking not tested\n";
            return;
        }
        auto* memory = static_cast<std::uint8_t*>(block);
        std::memset(memory, untouched, bytes);
        const auto address = reinterpret_cast<std::uint64_t>(block);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(block, bytes, true, true, true);
        }
        struct Unregister {
            const Context& context;
            void* block;
            std::uint64_t address;
            std::size_t bytes;
            ~Unregister() {
                {
                    GuestAllocations::Mutation mutation;
                    mutation.Remove(block);
                }
                HostImportFor(context, address, bytes);
                ReleaseWatched(block, bytes);
            }
        } unregister{base, block, address, bytes};
        AgcDriver::GuestMemory::CollectWritesUncached(address, bytes);
        TextureDetiler detiler(base);
        auto context = base;
        context.hostImportAlignment = 0;
        context.detiler = &detiler;
        GuestTextureResource resource{};
        resource.baseAddress = address;
        resource.width = side;
        resource.height = side;
        resource.depthOrLastArray = 1;
        resource.mipCount = 1;
        resource.tileMode = tileMode;
        resource.dimension = TextureDimension::k2DArray;
        resource.format = 56;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto geometry = DescribeSurface(resource);
        Require(geometry.imageLayers == 2 && geometry.layerBytes == sliceBytes && geometry.guestBytes == 2 * sliceBytes, what + " does not hold two 64 KiB slices");
        const auto stored = [&](std::uint32_t slice, std::array<std::uint8_t, 4> color) {
            std::vector<std::byte> read(sliceBytes);
            AgcDriver::GuestMemory::Read(address + geometry.GuestLayerOffset(slice), read);
            for (std::size_t at = 0; at < read.size(); ++at) {
                if (std::to_integer<std::uint8_t>(read[at]) != color[at % 4]) return false;
            }
            return true;
        };
        {
            auto image = StorageTexture::Create(context, detiler, resource, 0);
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            const VkClearColorValue red{{1.0f, 0.0f, 0.0f, 1.0f}};
            const VkClearColorValue green{{0.0f, 1.0f, 0.0f, 1.0f}};
            const VkImageSubresourceRange first{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            const VkImageSubresourceRange second{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1, 1};
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &red, 1, &first);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &green, 1, &second);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkSliceDirty(0);
            for (std::uint32_t slice = 0; slice < 2; ++slice) {
                const auto display = address + geometry.GuestLayerOffset(slice);
                const auto found = StorageTexture::FindPendingSlice(display, sliceBytes);
                Require(found == image, what + ": a display buffer that is slice " + std::to_string(slice) + " does not find the pending image");
                Require(AgcDriver::ResidentPresentSlice(found->Descriptor().baseAddress, found->GuestBytes(), found->ImageLayers(), display, sliceBytes) == slice, what + ": a display buffer that is slice " + std::to_string(slice) + " is not presented from that layer");
            }
            Require(StorageTexture::FindPending(address + geometry.GuestLayerOffset(1), sliceBytes) == nullptr, what + ": the lookup by base address finds a display buffer in slice 1");
            Require(StorageTexture::FindPendingSlice(address + 2 * sliceBytes, sliceBytes) == nullptr && StorageTexture::FindPendingSlice(address + geometry.GuestLayerOffset(1), sliceBytes + 1) == nullptr, what + ": a range past the image's last slice finds the pending image");
            image->WriteBack();
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            Require(stored(0, {255, 0, 0, 255}), what + ": the slice a draw wrote was not stored");
            Require(stored(1, {untouched, untouched, untouched, untouched}), what + ": a slice no draw wrote was stored over guest memory");
            static_cast<void>(recorder.Commands());
            recorder.Keep(image);
            image->MarkSliceDirty(1);
            image->WriteBack();
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            Require(stored(0, {255, 0, 0, 255}) && stored(1, {0, 255, 0, 255}), what + ": the second slice was not stored once a draw wrote it");
            auto other = StorageTexture::Create(context, detiler, resource, 0);
            static_cast<void>(recorder.Commands());
            recorder.Keep(image);
            recorder.Keep(other);
            image->MarkSliceDirty(1);
            other->MarkSliceDirty(1);
            std::string ambiguous;
            try {
                static_cast<void>(StorageTexture::FindPendingSlice(address + geometry.GuestLayerOffset(1), sliceBytes));
            } catch (const std::runtime_error& error) {
                ambiguous = error.what();
            }
            Require(ambiguous.find("is a slice of the pending array images") != std::string::npos, what + ": a slice of two pending array images did not throw: " + ambiguous);
            image->WriteBack();
            other->WriteBack();
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
        }
        recorder.Sync();
    }
    std::cout << "slice dirty marking: ok\n";
}

void atomicViewTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    TextureDetiler detiler(context);
    auto withDetiler = context;
    withDetiler.detiler = &detiler;
    for (const std::uint32_t format : {20u, 21u, 22u, 56u}) {
        GuestTextureResource resource{};
        resource.width = 64;
        resource.height = 4;
        resource.mipCount = 1;
        resource.tileMode = TextureTileMode::kLinear;
        resource.dimension = TextureDimension::k2D;
        resource.format = format;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto geometry = DescribeSurface(resource);
        std::vector<std::uint8_t> memory(static_cast<std::size_t>(geometry.guestBytes) + 256);
        resource.baseAddress = (reinterpret_cast<std::uintptr_t>(memory.data()) + 255) & ~std::uintptr_t{255};
        auto image = StorageTexture::Create(withDetiler, detiler, resource, 0);
        recorder.Keep(image);
        if (format == 56u) {
            bool refused = false;
            try {
                static_cast<void>(image->AtomicView(0, false));
            } catch (const std::exception&) {
                refused = true;
            }
            Require(refused, "an 8_8_8_8 storage image has an atomic view");
            continue;
        }
        const auto atomic = image->AtomicView(0, false);
        Require(atomic != VK_NULL_HANDLE && atomic == image->AtomicView(0, false), "atomic storage views are not reused");
        Require((atomic == image->View(0)) == (format == 20u), "only a 32_UINT storage image serves atomics through its own view");
    }
}

bool vertexTargetRangeTests(const Device& device, Recorder& recorder, bool imported) {
    if (!device.Graphics() || (imported && device.GetContext().hostImportAlignment == 0)) return false;
    constexpr std::size_t unit = 65536;
    constexpr std::size_t bytes = unit * 3;
    void* watched = AllocateWatched(bytes, unit);
    std::vector<std::byte> fallback(watched == nullptr ? bytes + unit : 0);
    auto* block = watched != nullptr ? static_cast<std::byte*>(watched) : reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(fallback.data()) + unit - 1) & ~(unit - 1));
    struct Release {
        void* watched;
        ~Release() { if (watched != nullptr) ReleaseWatched(watched, bytes); }
    } release{watched};
    std::fill_n(block, bytes, std::byte{0});
    const auto address = reinterpret_cast<std::uint64_t>(block);
    auto context = device.GetContext();
    context.recorder = &recorder;
    if (!imported) {
        context.hostImportAlignment = 0;
        context.dmaBufImport = false;
    }
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Cleanup {
        const Context& context;
        Recorder& recorder;
        void* block;
        ~Cleanup() {
            recorder.Sync();
            WaitForOptimizedPipelines(context.device);
            SharedResourceCache().Clear();
            ClearCachedTextures(context.device);
            recorder.Sync();
            ClearCachedPipelines(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } cleanup{context, recorder, block};
    if (imported && HostImportFor(context, address, bytes) == nullptr) return false;
    const auto shader = [](auto& words, std::uint64_t variant) {
        ShaderRecompiler::RecompileResult program;
        program.spirv = std::vector<std::uint32_t>(std::begin(words), std::end(words));
        program.variantId = variant;
        return program;
    };
    auto producerVertex = shader(COVER_VERT_SPV, 101);
    auto producerPixel = shader(VERTEXRANGEPRODUCER_FRAG_SPV, 102);
    auto vertex = shader(VERTEXRANGE_VERT_SPV, 103);
    auto pixel = shader(COVER_FRAG_SPV, 104);
    const std::array producerShaders{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &producerVertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &producerPixel, 0}};
    const std::array shaders{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &pixel, 0}};
    const auto stateFor = [](std::uint64_t base, VkFormat format, std::uint32_t elementBytes) {
        State state{};
        state.stages.path = ShaderPath::Vertex;
        state.color = {base, {64, 64}, format, unit, 0xe4u, ColorTileMode::RenderTarget, elementBytes};
        state.colors = {state.color};
        state.hasColorTarget = true;
        state.renderExtent = {64, 64};
        state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        state.viewport = {0, 64, 64, -64, 0, 1};
        state.scissor = {{0, 0}, {64, 64}};
        state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        state.blend.colorWriteMask = 15;
        state.blends = {state.blend};
        return state;
    };
    const auto producer = stateFor(address + unit, VK_FORMAT_R32G32B32A32_SFLOAT, 16);
    const auto consumer = stateFor(address, VK_FORMAT_R8G8B8A8_UNORM, 4);
    const AgcDriver::Pm4::DrawParameters triangle{0, 3, 0, 1, 0, false};
    const auto produce = [&] {
        Draw(context, producer, triangle, producerShaders);
        WaitForOptimizedPipelines(context.device);
        const std::array range{std::pair{address + unit, address + 2 * unit}};
        Require(StorageTexture::AnyPendingOverlaps(range), "the color producer did not leave GPU results pending");
    };
    const auto attributeFor = [](std::uint64_t base, std::uint32_t stride, std::uint32_t fetchIndex, std::uint32_t outOfBoundsSelect = 0) {
        return ShaderRecompiler::VertexAttribute{0, 4, {{static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u) | (stride << 16u), static_cast<std::uint32_t>(bytes / 16), (77u << 12u) | (outOfBoundsSelect << 28u)}}, fetchIndex};
    };
    const auto check = [&](std::uint32_t edge) {
        std::vector<std::byte> texels(64 * 64 * 4);
        ReadColorTarget(consumer.color, texels);
        for (std::size_t y = 0; y < 64; ++y) {
            for (std::size_t x = 0; x < 64; ++x) {
                const auto expected = x < edge ? std::byte{0} : std::byte{255};
                for (std::size_t channel = 0; channel < 4; ++channel) Require(texels[(y * 64 + x) * 4 + channel] == expected, "the draw did not consume the GPU-produced vertex offset");
            }
        }
    };
    const auto reject = [&](auto action, const char* expected) {
        try { action(); }
        catch (const std::runtime_error& error) {
            Require(std::string(error.what()).find(expected) != std::string::npos, error.what());
            return;
        }
        throw std::runtime_error("an actual render-target alias was accepted");
    };
    for (const std::uint32_t fetchIndex : {0u, 1u}) {
        producerPixel = shader(VERTEXRANGEPRODUCER_FRAG_SPV, 102);
        vertex.vertexAttributes = {attributeFor(address, 16, fetchIndex)};
        auto draw = triangle;
        if (fetchIndex == 0) draw.firstVertex = unit / 16;
        else draw.firstInstance = unit / 16;
        produce();
        std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
        Draw(context, consumer, draw, shaders, {}, &recipe);
        WaitForOptimizedPipelines(context.device);
        const auto read = recorder.DescribePendingRead(address + unit, 16);
        Require(read.has_value() == imported && (!read || read->kind == Recorder::ReadKind::DrawInput || read->kind == Recorder::ReadKind::StorageUpload), "the vertex regression used the wrong copied/imported path");
        check(16);
        Require(recipe != nullptr, "the range regression did not create a draw recipe");
        std::vector<std::byte> cleared(unit);
        AgcDriver::GuestMemory::Write(address, cleared);
        for (std::size_t offset = 5; offset < producerPixel.spirv.size();) {
            const auto count = producerPixel.spirv[offset] >> 16u;
            Require(count != 0, "invalid producer SPIR-V");
            if ((producerPixel.spirv[offset] & 0xffffu) == 43u && count == 4 && producerPixel.spirv[offset + 3] == 0x3f000000u) producerPixel.spirv[offset + 3] = 0x3f400000u;
            offset += count;
        }
        producerPixel.variantId = 105;
        produce();
        const auto hit = DrawWithRecipe(context, consumer, draw, shaders, {}, *recipe);
        WaitForOptimizedPipelines(context.device);
        Require(hit.recorded, "the vertex range regression did not reuse its draw recipe");
        check(24);
        vertex.vertexAttributes = {attributeFor(address + unit, 16, fetchIndex)};
        Draw(context, consumer, triangle, shaders);
        WaitForOptimizedPipelines(context.device);
        check(24);
        vertex.vertexAttributes = {attributeFor(address, 16, fetchIndex)};
        auto indirect = draw;
        indirect.indirect.emplace();
        reject([&] { Draw(context, consumer, indirect, shaders); }, "vertex buffer aliases the render target");
        if (fetchIndex == 0) {
            const std::array<std::uint16_t, 3> indices{0, 1, 2};
            AgcDriver::GuestMemory::Write(address + 2 * unit, std::as_bytes(std::span(indices)));
            auto indexed = draw;
            indexed.indexed = true;
            indexed.indexSize = 2;
            indexed.indexAddress = address + 2 * unit;
            produce();
            Draw(context, consumer, indexed, shaders);
            WaitForOptimizedPipelines(context.device);
            check(24);
            indexed.indexAddress = address;
            reject([&] { Draw(context, consumer, indexed, shaders); }, "index buffer aliases the render target");
        }
        reject([&] { Draw(context, consumer, triangle, shaders); }, "vertex buffer aliases the render target");
        vertex.vertexAttributes = {attributeFor(address, 0, fetchIndex, 3)};
        reject([&] { Draw(context, consumer, draw, shaders); }, "vertex buffer aliases the render target");
    }
    vertex.vertexAttributes = {attributeFor(address, 0, 0)};
    Draw(context, consumer, triangle, shaders);
    WaitForOptimizedPipelines(context.device);
    std::cout << "Vertex range and pending color producer tests passed (" << (imported ? "imported" : "copied") << ")\n";
    return true;
}

std::uint64_t processPrivateBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) return 0;
    return counters.PrivateUsage;
#else
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("RssAnon:", 0) == 0) return std::stoull(line.substr(8)) * 1024u;
    }
    return 0;
#endif
}

// What one graphics pipeline the store keeps costs: `count` pipelines, each with its own fragment
// module (so its own fragment shader library), built through CachedPipeline and held, after their
// optimized relinks; process private bytes and VK_EXT_memory_budget heap usage before and after,
// and after the store is cleared.
int pipelineStoreMemoryBench(const Device& device, std::uint32_t count) {
    if (!device.Graphics() || count == 0) {
        std::cout << "skipped, the pipeline store memory bench requires a graphics queue and at least one pipeline\n";
        return 77;
    }
    std::lock_guard gpu(GpuMutex());
    const auto& context = device.GetContext();
    struct Cleanup {
        VkDevice device;
        ~Cleanup() { ClearCachedPipelines(device); }
    } cleanup{context.device};
    ClearCachedPipelines(context.device);
    ShaderRecompiler::RecompileResult vertex;
    vertex.variantId = 21;
    vertex.spirv = std::vector<std::uint32_t>(std::begin(TRIANGLE_vert_SPV), std::end(TRIANGLE_vert_SPV));
    std::vector<ShaderRecompiler::RecompileResult> fragments(count + 1u);
    for (std::uint32_t index = 0; index <= count; ++index) {
        fragments[index].variantId = 1000u + index;
        fragments[index].spirv = std::vector<std::uint32_t>(std::begin(TRIANGLE_frag_SPV), std::end(TRIANGLE_frag_SPV));
        fragments[index].spirv[2] = 0x50535430u + index;
    }
    State state{};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.hasColorTarget = true;
    state.color.format = VK_FORMAT_R8G8B8A8_UNORM;
    state.colors.push_back(state.color);
    state.blend.colorWriteMask = 15;
    state.blends.push_back(state.blend);
    const std::array first{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &fragments[count], 0}};
    ShaderResources resources(context, first, state.color, static_cast<std::uint32_t>(state.colors.size()), 0, 0);
    const VertexInputLayout input;
    std::vector<std::shared_ptr<Pipeline>> held;
    held.reserve(count + 1u);
    const auto build = [&](std::uint32_t index) {
        const std::array shaders{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &fragments[index], 0}};
        held.push_back(CachedPipeline(context, state, input, resources, shaders, VK_IMAGE_LAYOUT_GENERAL));
    };
    struct Sample {
        std::uint64_t privateBytes;
        std::optional<Device::HeapUsage> heaps;
    };
    const auto sample = [&] {
        WaitForOptimizedPipelines(context.device);
        return Sample{processPrivateBytes(), device.MemoryUsage()};
    };
    build(count);
    const auto before = sample();
    const auto started = std::chrono::steady_clock::now();
    for (std::uint32_t index = 0; index < count; ++index) build(index);
    const auto built = std::chrono::steady_clock::now();
    const auto after = sample();
    const auto linked = std::chrono::steady_clock::now();
    const auto totals = PipelineStoreTotals();
    held.clear();
    ClearCachedPipelines(context.device);
    const auto cleared = sample();
    const auto mib = [](std::uint64_t later, std::uint64_t earlier) { return (static_cast<double>(later) - static_cast<double>(earlier)) / (1024.0 * 1024.0); };
    const auto perPipeline = [&](std::uint64_t later, std::uint64_t earlier) { return (static_cast<double>(later) - static_cast<double>(earlier)) / 1024.0 / count; };
    const char* linking = !context.graphicsPipelineLibrary ? "whole pipelines" : context.pipelineLinkTimeOptimization ? "libraries and optimized relinks" : "libraries, fast links only";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Pipeline store memory: " << count << " graphics pipelines (one fragment module each, " << linking << "), " << totals.cached << " in the store, built in "
              << std::chrono::duration<double>(built - started).count() << " s, relinked in " << std::chrono::duration<double>(linked - built).count() << " s\n";
    std::cout << "  process private bytes: +" << mib(after.privateBytes, before.privateBytes) << " MiB = " << perPipeline(after.privateBytes, before.privateBytes) << " KiB per pipeline; after clearing the store " << mib(cleared.privateBytes, before.privateBytes) << " MiB above the start\n";
    if (before.heaps && after.heaps && cleared.heaps) {
        std::cout << "  VK_EXT_memory_budget heap usage: device-local +" << mib(after.heaps->deviceLocal, before.heaps->deviceLocal) << " MiB = " << perPipeline(after.heaps->deviceLocal, before.heaps->deviceLocal) << " KiB per pipeline, host +" << mib(after.heaps->host, before.heaps->host) << " MiB = " << perPipeline(after.heaps->host, before.heaps->host)
                  << " KiB per pipeline; after clearing the store device-local " << mib(cleared.heaps->deviceLocal, before.heaps->deviceLocal) << " MiB, host " << mib(cleared.heaps->host, before.heaps->host) << " MiB above the start\n";
    } else {
        std::cout << "  VK_EXT_memory_budget unavailable: no device heap usage\n";
    }
    return 0;
}

void pipelineCacheTests(const Device& device, bool benchmark) {
    std::thread establishThreadedProcess([] {});
    establishThreadedProcess.join();
    std::lock_guard gpu(GpuMutex());
    const auto& context = device.GetContext();
    struct Cleanup {
        VkDevice device;
        ~Cleanup() { ClearCachedPipelines(device); }
    } cleanup{context.device};
    ShaderRecompiler::RecompileResult vertex, fragment;
    vertex.variantId = 1;
    fragment.variantId = 2;
    vertex.spirv = std::vector<std::uint32_t>(std::begin(TRIANGLE_vert_SPV), std::end(TRIANGLE_vert_SPV));
    fragment.spirv = std::vector<std::uint32_t>(std::begin(TRIANGLE_frag_SPV), std::end(TRIANGLE_frag_SPV));
    const std::array shaders{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}};
    State state{};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.hasColorTarget = true;
    state.color.format = VK_FORMAT_R8G8B8A8_UNORM;
    state.colors.push_back(state.color);
    state.blend.colorWriteMask = 15;
    state.blends.push_back(state.blend);
    ShaderResources resources(context, shaders, state.color, static_cast<std::uint32_t>(state.colors.size()), 0, 0);
    const auto lookup = [&](const VertexInputLayout& input) {
        return CachedPipeline(context, state, input, resources, shaders, VK_IMAGE_LAYOUT_GENERAL);
    };
    VertexInputLayout input;
    const auto first = lookup(input);
    Require(lookup(input) == first, "pipeline cache did not retain an identical key");
    state.cullMode = VK_CULL_MODE_BACK_BIT;
    Require(lookup(input) != first, "pipeline cache ignored changed raster state");
    state.cullMode = 0;
    const auto largest = std::min({32u, context.limits.maxVertexInputBindings, context.limits.maxVertexInputAttributes});
    for (const auto attributes : {0u, 8u, largest}) {
        input.bindings.clear();
        input.attributes.clear();
        for (std::uint32_t i = 0; i < attributes; ++i) {
            input.bindings.push_back({i, 16, VK_VERTEX_INPUT_RATE_VERTEX});
            input.attributes.push_back({i, i, VK_FORMAT_R32G32B32A32_SFLOAT, 0});
        }
        const auto expected = lookup(input);
        if (attributes != 0) Require(expected != first, "pipeline cache ignored changed vertex input");
        Require(lookup(input) == expected, "pipeline cache lost the larger key");
        if (!benchmark) continue;
        std::array<double, 9> times;
        for (unsigned pass = 0; pass <= times.size(); ++pass) {
            const auto started = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < 65536; ++i) Require(lookup(input) == expected, "pipeline cache benchmark missed a warmed key");
            if (pass != 0) times[pass - 1] = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count() / 65536;
        }
        std::ranges::sort(times);
        std::cout << "Pipeline cache " << attributes << " attributes: median " << times[4] << " us/lookup, p95 pass " << times.back() << " us\n";
    }
    input = {};
    Require(lookup(input) == first, "pipeline cache retained bytes from a larger key");
    vertex.variantId = 0;
    Require(lookup(input) == first, "pipeline cache keyed on the variant id instead of the module");
    vertex.spirv[2] ^= 1u;
    const auto other = lookup(input);
    Require(other != first && lookup(input) == other, "pipeline cache ignored another module");
    vertex.spirv[2] ^= 1u;
    vertex.variantId = 1;
    Require(lookup(input) == first, "another module disturbed the retained pipeline");
    ClearCachedPipelines(context.device);
    Require(lookup(input) != first, "clearing the pipeline cache retained its old entry");
    std::cout << "Pipeline cache key and retained lifetime tests passed\n";
}

struct PipelineCacheFileProbe {
    Context& context;
    PFN_vkGetDeviceProcAddr resolver;
    std::atomic<std::uint32_t> creations{0};
    static inline PipelineCacheFileProbe* active = nullptr;
    explicit PipelineCacheFileProbe(Context& context) : context(context), resolver(context.deviceProc) {
        Require(active == nullptr, "pipeline cache file probe is already active");
        active = this;
        context.deviceProc = resolve;
    }
    ~PipelineCacheFileProbe() {
        context.deviceProc = resolver;
        active = nullptr;
    }
    static VKAPI_ATTR VkResult VKAPI_CALL createCache(VkDevice device, const VkPipelineCacheCreateInfo* info, const VkAllocationCallbacks* callbacks, VkPipelineCache* created) {
        ++active->creations;
        return reinterpret_cast<PFN_vkCreatePipelineCache>(active->resolver(device, "vkCreatePipelineCache"))(device, info, callbacks, created);
    }
    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL resolve(VkDevice device, const char* name) {
        if (std::strcmp(name, "vkCreatePipelineCache") == 0) return reinterpret_cast<PFN_vkVoidFunction>(createCache);
        return active->resolver(device, name);
    }
};

void pipelineCacheHeaderTests(const Device& device) {
    const auto root = std::filesystem::temp_directory_path() / ("aps5-pipeline-header-test-" + std::to_string(std::random_device{}()));
    Require(root.native().size() < 225, "the pipeline cache header test requires a temporary root shorter than 225 characters");
#ifdef _WIN32
    std::error_code cleanupError;
    const auto cleanupRoot = ShaderRecompiler::NativeCachePath_nid_no_patch(root, cleanupError);
    Require(!cleanupError, "the pipeline cache header test could not resolve its cleanup directory");
#else
    const auto& cleanupRoot = root;
#endif
    struct RemoveDirectory {
        std::filesystem::path directory;
        ~RemoveDirectory() {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
    } removeDirectory{cleanupRoot};
    auto directory = root;
    char version[17];
    std::snprintf(version, sizeof(version), "%016llx", static_cast<unsigned long long>(ShaderRecompiler::ShaderDiskCache::Generated::SourceVersion));
    while ((directory / version).native().size() < 220) {
        const auto length = (directory / version).native().size();
        const auto remaining = 245 > length ? 245 - length : 1;
        const auto chunk = std::min<std::size_t>(remaining, 16);
        directory /= std::string(chunk, 'd');
    }
    const auto versionDirectory = directory / version;
    Require(versionDirectory.native().size() >= 220 && versionDirectory.native().size() <= 249, "the pipeline cache directory is not between 220 and 249 characters");
#ifdef _WIN32
    _putenv_s("ANYPS5_NO_SHADER_CACHE", "0");
    _putenv_s("ANYPS5_SHADER_CACHE_DIR", directory.string().c_str());
#else
    setenv("ANYPS5_NO_SHADER_CACHE", "0", 1);
    setenv("ANYPS5_SHADER_CACHE_DIR", directory.string().c_str(), 1);
#endif
    auto context = device.GetContext();
    PipelineCacheFileProbe probe(context);
    PipelineCache cache(context, device.Properties());
    cache.SaveNow();
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(versionDirectory)) files.push_back(entry.path());
    Require(files.size() == 1 && files.front().filename().string().starts_with("vk-pipelines-"), "the long pipeline cache directory does not hold exactly one cache file");
    Require(files.front().native().size() > 260, "the pipeline cache header test did not exceed MAX_PATH");
    std::vector<std::byte> stored;
    Require(ShaderRecompiler::ReadWholeFile(files.front(), stored) && !stored.empty(), "the long pipeline cache file was not persisted");
    const auto creations = probe.creations.load();
    for (int repeat = 0; repeat < 3; ++repeat) cache.SaveNow();
    Require(probe.creations.load() == creations, "an unchanged long-path pipeline cache was reloaded for an idle save");
    std::vector<std::byte> after;
    Require(ShaderRecompiler::ReadWholeFile(files.front(), after) && after == stored, "an idle save changed the long pipeline cache file");
#ifdef _WIN32
    std::error_code lockError;
    const auto lockNative = ShaderRecompiler::NativeCachePath_nid_no_patch(files.front(), lockError);
    Require(!lockError, "could not resolve long cache path");
    struct FileLockGuard {
        HANDLE handle = INVALID_HANDLE_VALUE;
        ~FileLockGuard() {
            if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        }
    } lockGuard{CreateFileW(lockNative.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    Require(lockGuard.handle != INVALID_HANDLE_VALUE, "could not lock pipeline cache file");
    std::string capturedStderr;
    {
        StderrCapture capture;
        PipelineCache lockedCache(context, device.Properties());
        lockedCache.SaveNow();
        capturedStderr = capture.Finish();
    }
    Require(capturedStderr.find("cannot read") != std::string::npos, "an unreadable long pipeline cache file did not report the cannot read diagnostic");
    if (lockGuard.handle != INVALID_HANDLE_VALUE) {
        CloseHandle(lockGuard.handle);
        lockGuard.handle = INVALID_HANDLE_VALUE;
    }
    std::vector<std::byte> retained;
    Require(ShaderRecompiler::ReadWholeFile(files.front(), retained) && retained == stored, "an unreadable long pipeline cache file was overwritten");
#endif
    std::cout << "long-path pipeline cache header reused without reloading\n";
}

int pipelineCacheFileTests(const Device& device) {
    const auto& context = device.GetContext();
    const auto directory = std::filesystem::temp_directory_path() / ("aps5-pipeline-cache-file-test-" + std::to_string(std::random_device{}()));
    struct RemoveDirectory {
        std::filesystem::path directory;
        ~RemoveDirectory() {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
    } removeDirectory{directory};
#ifdef _WIN32
    _putenv_s("ANYPS5_NO_SHADER_CACHE", "0");
    _wputenv_s(L"ANYPS5_SHADER_CACHE_DIR", directory.wstring().c_str());
#else
    setenv("ANYPS5_NO_SHADER_CACHE", "0", 1);
    setenv("ANYPS5_SHADER_CACHE_DIR", directory.string().c_str(), 1);
#endif
    const auto properties = device.Properties();
    struct Objects {
        const Context& context;
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        std::array<VkShaderModule, 2> modules{};
        ~Objects() {
            for (const auto module : modules) {
                if (module != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            }
            if (layout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
            if (setLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
        }
    } objects{context};
    const std::array bindings{VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
    VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    setLayoutInfo.pBindings = bindings.data();
    Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setLayoutInfo, nullptr, &objects.setLayout), "vkCreateDescriptorSetLayout");
    const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float) * 2};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &objects.setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &objects.layout), "vkCreatePipelineLayout");
    const std::array<std::span<const std::uint32_t>, 2> codes{std::span<const std::uint32_t>(SAMPLE_LOD_SPV), std::span<const std::uint32_t>(SAMPLE_ARRAY_SPV)};
    for (std::size_t index = 0; index < codes.size(); ++index) {
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = codes[index].size_bytes();
        moduleInfo.pCode = codes[index].data();
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &objects.modules[index]), "vkCreateShaderModule");
    }
    const auto build = [&](VkPipelineCache cache, std::size_t index) {
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, objects.modules[index], "main", nullptr};
        info.layout = objects.layout;
        VkPipeline pipeline = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, cache, 1, &info, nullptr, &pipeline), "vkCreateComputePipelines");
        context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
    };
    const auto bytes = [&](VkPipelineCache cache) {
        std::size_t size = 0;
        Check(context.Function<PFN_vkGetPipelineCacheData>("vkGetPipelineCacheData")(context.device, cache, &size, nullptr), "vkGetPipelineCacheData");
        return size;
    };
    const auto footprint = [&](std::size_t index) {
        VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        VkPipelineCache fresh = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreatePipelineCache>("vkCreatePipelineCache")(context.device, &info, nullptr, &fresh), "vkCreatePipelineCache");
        const auto before = bytes(fresh);
        build(fresh, index);
        const auto after = bytes(fresh);
        context.Function<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache")(context.device, fresh, nullptr);
        return after > before ? after - before : 0;
    };
    const std::array<std::size_t, 2> footprints{footprint(0), footprint(1)};
    const auto holds = [&](VkPipelineCache cache, std::size_t index) {
        const auto before = bytes(cache);
        build(cache, index);
        const auto after = bytes(cache);
        return after < before + footprints[index] / 2;
    };
    const auto cacheFile = [&] {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) Require(entry.is_directory(), "a pipeline cache file lives in the shader cache root");
        char version[17];
        std::snprintf(version, sizeof(version), "%016llx", static_cast<unsigned long long>(ShaderRecompiler::ShaderDiskCache::Generated::SourceVersion));
        const auto versionDirectory = directory / version;
        Require(std::filesystem::is_directory(versionDirectory), "the current shader cache version has no directory");
        std::vector<std::filesystem::path> found;
        for (const auto& entry : std::filesystem::directory_iterator(versionDirectory)) found.push_back(entry.path());
        Require(found.size() == 1 && found.front().filename().string().starts_with("vk-pipelines-"), "the current shader cache version's directory does not hold exactly one pipeline cache file");
        return found.front();
    };
    const auto contents = [](const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<char>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    };
    if (footprints[0] == 0 || footprints[1] == 0) {
        std::cout << "skipped, the device does not keep pipelines in a pipeline cache\n";
        return 77;
    }
    for (const bool firstSavesFirst : {true, false}) {
        std::filesystem::remove_all(directory);
        auto first = std::make_unique<PipelineCache>(context, properties);
        auto second = std::make_unique<PipelineCache>(context, properties);
        build(first->Handle(), 0);
        build(second->Handle(), 1);
        if (firstSavesFirst) {
            first.reset();
            second.reset();
        } else {
            second.reset();
            first.reset();
        }
        PipelineCache third(context, properties);
        Require(holds(third.Handle(), 0), firstSavesFirst ? "the cache file lost the pipeline of the object that saved first" : "the cache file lost the pipeline of the object that saved last");
        Require(holds(third.Handle(), 1), firstSavesFirst ? "the cache file lost the pipeline of the object that saved last" : "the cache file lost the pipeline of the object that saved first");
    }
    const auto path = cacheFile();
    const auto stored = contents(path);
    {
        PipelineCache idle(context, properties);
    }
    Require(contents(path) == stored, "an object that added no pipelines rewrote the cache file");
    {
        PipelineCache unseeded(context, properties, false);
        Require(!holds(unseeded.Handle(), 0), "a cache created without the file already held a pipeline of the file");
    }
    {
        PipelineCache reloaded(context, properties);
        Require(holds(reloaded.Handle(), 0) && holds(reloaded.Handle(), 1), "an unseeded cache dropped pipelines from the cache file when it saved");
    }
    std::filesystem::remove_all(directory);
    {
        PipelineCache writer(context, properties);
        build(writer.Handle(), 0);
    }
    {
        PipelineCache unseeded(context, properties, false);
        unseeded.SaveNow();
        build(unseeded.Handle(), 1);
        unseeded.SaveNow();
    }
    {
        PipelineCache reloaded(context, properties);
        Require(holds(reloaded.Handle(), 0) && holds(reloaded.Handle(), 1), "an unseeded cache that saved twice over an unchanged file lost pipelines of the file");
    }
    std::filesystem::remove_all(directory);
    {
        PipelineCache writer(context, properties);
        build(writer.Handle(), 0);
    }
    {
        PipelineCache extender(context, properties);
        Require(holds(extender.Handle(), 0), "the cache file lost a pipeline");
        build(extender.Handle(), 1);
    }
    {
        PipelineCache reloaded(context, properties);
        Require(holds(reloaded.Handle(), 0) && holds(reloaded.Handle(), 1), "a cache that saved over an unchanged file lost pipelines");
    }
    {
        std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
        corrupt << "not a pipeline cache";
    }
    {
        PipelineCache replacing(context, properties);
        build(replacing.Handle(), 1);
    }
    {
        PipelineCache reloaded(context, properties);
        Require(holds(reloaded.Handle(), 1), "an invalid cache file was not replaced by the saving object's pipelines");
    }
    {
        PipelineCache busy(context, properties);
        build(busy.Handle(), 0);
        std::promise<void> held;
        std::promise<void> release;
        std::thread creation([&] {
            auto use = busy.Synchronization().LockForPipelineUse();
            held.set_value();
            release.get_future().wait();
        });
        held.get_future().wait();
        auto saving = std::async(std::launch::async, [&] { busy.SaveUnlessBusy(); });
        const bool saveReturned = saving.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        bool otherCreationProceeded = false;
        if (saveReturned) {
            auto other = std::async(std::launch::async, [&] { auto use = busy.Synchronization().LockForPipelineUse(); });
            otherCreationProceeded = other.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
            release.set_value();
            other.wait();
        } else {
            release.set_value();
        }
        creation.join();
        saving.wait();
        Require(saveReturned, "a periodic pipeline cache save waited for a pipeline creation to finish");
        Require(otherCreationProceeded, "a pipeline creation waited behind a periodic pipeline cache save");
    }
    std::cout << "Pipeline cache file merge tests passed\n";
    return 0;
}

void descriptorPoolTests(const Device& device) {
    DescriptorCache cache(device.GetContext());
    const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    const std::array<std::uint32_t, 4> key{binding.binding, binding.descriptorType, binding.descriptorCount, binding.stageFlags};
    const auto layout = cache.Layout(key, std::span(&binding, 1));
    const VkDescriptorPoolSize size{binding.descriptorType, binding.descriptorCount};
    const auto held = cache.Allocate(layout, std::span(&size, 1));
    std::uint64_t initialPools = 0;
    for (unsigned round = 0; round < 3; ++round) {
        std::vector<DescriptorCache::SetAllocation> sets;
        for (unsigned i = 0; i < 1100; ++i) {
            const auto allocation = cache.Allocate(layout, std::span(&size, 1));
            Require(allocation.set != VK_NULL_HANDLE, "descriptor pool rollover refused a valid layout");
            sets.push_back(allocation);
        }
        const auto pools = cache.Counters().pools;
        if (round == 0) {
            initialPools = pools;
        } else {
            Require(pools <= initialPools + 1, "descriptor pool recycling grows behind one retained set");
        }
        for (const auto& allocation : sets) cache.Free(allocation);
    }
    cache.Free(held);
    std::cout << "Descriptor pool recycling passed across 3,301 sets (" << initialPools << " initial pools, " << cache.Counters().pools << " final pools)\n";
}

void keysFillTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: DCC key fills not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the key fill block");
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* keys = texels + surfaceBytes;
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(keys, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    const auto keysAddress = address + surfaceBytes;
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the key fill block refused: DCC key fills not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = keysAddress;
    Require(DescribeSurface(resource).guestBytes == surfaceBytes, "the key fill surface has an unexpected size");
    {
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto draw = [&](VkClearColorValue value) {
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
        };
        const auto holds = [&](std::array<std::uint8_t, 4> texel) {
            Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {side, side, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            const auto pixels = readback.Bytes();
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                if (std::to_integer<std::uint8_t>(pixels[i]) != texel[i % 4]) return false;
            }
            return true;
        };
        const auto fill = [&](std::uint8_t key) {
            recorder.Sync();
            std::memset(keys, key, keyCount);
            return StorageTexture::NoteKeysFill(keysAddress, keyCount, key);
        };
        Require(holds({0, 0, 0, 0}), "a surface under 0000 keys was not cleared");
        draw({{1.0f, 0.0f, 0.0f, 1.0f}});
        Require(fill(0x00) == 1, "a 0000 key fill did not cover the surface");
        Require(image->FilledKeys() == DccKeys::Clear0000, "a key fill over pending results was not recorded");
        image->Refresh();
        Require(holds({0, 0, 0, 0}), "a key fill did not clear the results made before it at the next refresh");
        Require(image->FilledKeys() == DccKeys::Uncompressed, "the image cleared by a refresh still holds the fill");
        draw({{0.0f, 0.0f, 1.0f, 1.0f}});
        Require(fill(0x00) == 1, "a second 0000 key fill did not cover the surface");
#ifdef _WIN32
        _putenv_s("APS5_KEYS_FILL_CLEAR", "1");
#else
        setenv("APS5_KEYS_FILL_CLEAR", "1", 1);
#endif
        Require(StorageTexture::ClearByKeysFill(keysAddress, keyCount, 0x00) == 1, "a 0000 key fill did not clear the surface at once");
        Require(holds({0, 0, 0, 0}), "a key fill cleared at once left results made before it");
        draw({{0.0f, 1.0f, 0.0f, 1.0f}});
        Require(image->FilledKeys() == DccKeys::Uncompressed, "results drawn after a key fill cleared at once are held under the fill's code");
        Require(holds({0, 255, 0, 255}), "results drawn after a key fill cleared at once were lost");
    }
    recorder.Sync();
}

void integerClearTests(const Device& device) {
    const auto& context = device.GetContext();
    std::size_t tested = 0;
    for (const std::uint32_t format : {5u, 11u, 20u}) {
        if (!StorageFormatAvailable(context, format)) continue;
        ++tested;
        Require(StorageClearAvailable(context, format, DccKeys::Clear0000), "an integer storage format refused the 0000 clear");
        for (const auto keys : {DccKeys::Clear0001, DccKeys::Clear1111, DccKeys::ClearRegister, DccKeys::Uncompressed}) {
            Require(!StorageClearAvailable(context, format, keys), "an integer storage format took a clear code other than 0000");
        }
    }
    Require(tested == 3, "an integer storage format has no storage form");
    std::cout << "integer storage clears: " << tested << " formats ok\n";
}

bool clearHandOverTests(const Device& device, Recorder& recorder, bool watched) {
    const auto& base = device.GetContext();
    const std::string what = watched ? "clear-key hand-over (watched)" : "clear-key hand-over";
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: " << what << " not tested\n";
        return false;
    }
    constexpr std::size_t surfaceA = 256 * 256 * 4;
    constexpr std::size_t surfaceB = 128 * 128 * 8;
    constexpr std::size_t keyCount = surfaceB / 256;
    constexpr std::size_t bytes = surfaceA + 65536;
    static_assert(surfaceB <= surfaceA && keyCount <= bytes - surfaceA, "B lies over the first units of A, and B's DCC keys follow A inside the block");
    const auto decided = PrepareImportWatch(base);
#ifndef _WIN32
    if (watched && decided == ImportWatch::Unwatch) {
        std::cout << "host imports are compared on this device: " << what << " not tested\n";
        return false;
    }
#endif
    void* block = nullptr;
    if (watched) {
        block = AllocateWatched(bytes, 65536);
        if (block == nullptr) {
            std::cout << "no write watching: " << what << " not tested\n";
            return false;
        }
    } else {
#ifdef _WIN32
        block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        block = std::aligned_alloc(65536, bytes);
#endif
    }
    Require(block != nullptr && reinterpret_cast<std::uint64_t>(block) % 65536 == 0, "cannot allocate the clear-key hand-over block");
    if (watched) SetImportWatch(base, ImportWatch::Watch);
    struct RestoreWatch {
        const Context& context;
        ImportWatch decided;
        ~RestoreWatch() { SetImportWatch(context, decided); }
    } restoreWatch{base, decided};
    auto* memory = static_cast<std::uint8_t*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        bool watched;
        ~Unregister() {
            StorageTexture::SetClearHandOver(true);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
            if (watched) ReleaseWatched(block, bytes);
        }
    } unregister{base, block, address, watched};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the clear-key hand-over block refused: " << what << " not tested\n";
        return false;
    }
    if (watched && !AgcDriver::GuestMemory::Watched(address, bytes)) {
        std::cout << "the imported block left the write watch: " << what << " not tested\n";
        return false;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    const auto surface = [&](std::uint32_t width, std::uint32_t height, std::uint32_t format, std::uint64_t dcc) {
        GuestTextureResource resource{};
        resource.baseAddress = address;
        resource.width = width;
        resource.height = height;
        resource.mipCount = 1;
        resource.tileMode = TextureTileMode::kR64KBX;
        resource.dimension = TextureDimension::k2D;
        resource.format = format;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        resource.dccAddress = dcc;
        return resource;
    };
    const auto resourceA = surface(256, 256, 56, 0);
    const auto resourceB = surface(128, 128, 71, address + surfaceA);
    Require(DescribeSurface(resourceA).guestBytes == surfaceA && DescribeSurface(resourceB).guestBytes == surfaceB, "the clear-key hand-over surfaces have unexpected sizes");
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    const auto draw = [&](const std::shared_ptr<StorageTexture>& image, VkClearColorValue value) {
        const auto commands = recorder.Commands();
        recorder.Keep(image);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        image->MarkDirty();
    };
    const auto readImage = [&](const std::shared_ptr<StorageTexture>& image, std::uint32_t width, std::uint32_t height, std::size_t texelBytes) {
        Buffer readback(context, static_cast<std::size_t>(width) * height * texelBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {width, height, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        const auto pixels = readback.Bytes();
        return std::vector<std::byte>(pixels.begin(), pixels.end());
    };
    const auto readMemory = [&](std::uint64_t from, std::size_t length) {
        std::vector<std::byte> read(length);
        AgcDriver::GuestMemory::Read(from, read);
        recorder.Sync();
        if (!watched) std::memcpy(read.data(), memory + (from - address), length);
        return read;
    };
    const auto filled = [](std::span<const std::byte> read, std::span<const std::uint8_t> texel) {
        for (std::size_t i = 0; i < read.size(); ++i) {
            if (std::to_integer<std::uint8_t>(read[i]) != texel[i % texel.size()]) return false;
        }
        return true;
    };
    constexpr std::array<std::uint8_t, 4> red{255, 0, 0, 255};
    constexpr std::array<std::uint8_t, 4> blue{0, 0, 255, 255};
    constexpr std::array<std::uint8_t, 8> green{0, 0, 0x00, 0x3c, 0, 0, 0x00, 0x3c};
    constexpr std::array<std::uint8_t, 8> cleared{};
    enum class Order { Superseded, ReadFirst, Rewritten, CpuStore };
    struct Outcome {
        std::vector<std::byte> bytes;
        std::vector<std::byte> imageB;
        std::uint64_t handOvers = 0;
        std::uint64_t superseded = 0;
    };
    const auto run = [&](bool on, Order order) {
        const std::string where = what + (order == Order::Superseded ? ", A never read again" : order == Order::ReadFirst ? ", A read before B wrote" : order == Order::Rewritten ? ", A written again after the hand-over" : ", a CPU store before B wrote") + (on ? ", on" : ", off");
        StorageTexture::SetClearHandOver(on);
        recorder.Sync();
        std::memset(memory, 0x55, surfaceA);
        std::memset(memory + surfaceA, 0x00, keyCount);
        Outcome outcome{};
        const auto handOvers = StorageTexture::ClearHandOvers();
        const auto superseded = StorageTexture::HandedOverSuperseded();
        {
            auto imageA = StorageTexture::Create(context, detiler, resourceA, 0);
            auto imageB = StorageTexture::Create(context, detiler, resourceB, 0);
            draw(imageA, {{1.0f, 0.0f, 0.0f, 1.0f}});
            imageB->Refresh();
            if (on && watched) {
                Require(StorageTexture::ClearHandOvers() == handOvers + 1, where + ": B's refresh under clear keys did not hand A's results over");
                Require(StorageTexture::FindPending(address, surfaceA) == imageA, where + ": A's results did not stay pending");
                Require(std::all_of(memory, memory + 2 * 65536, [](std::uint8_t value) { return value == 0x55; }), where + ": B's refresh stored A's results over B");
            } else {
                Require(StorageTexture::ClearHandOvers() == handOvers, where + ": the hand-over ran while off");
            }
            outcome.imageB = readImage(imageB, 128, 128, 8);
            Require(filled(outcome.imageB, cleared), where + ": B does not hold its keys' clear");
            if (order == Order::ReadFirst) {
                Require(filled(readMemory(address, 65536), red), where + ": a read of A's memory before B wrote it did not see A's results");
                imageA->Refresh();
                Require(filled(readImage(imageA, 256, 256, 4), red), where + ": A read again as A lost its results");
            }
            if (order == Order::Rewritten) {
                draw(imageA, {{0.0f, 0.0f, 1.0f, 1.0f}});
                Require(filled(readMemory(address, 65536), blue), where + ": a read of A's memory did not see A's newer results");
                imageB->Refresh();
                Require(filled(readImage(imageB, 128, 128, 8), cleared), where + ": B refreshed again does not hold its keys' clear");
            }
            if (order == Order::CpuStore) {
                std::memset(memory + 65536, 0x77, 65536);
            }
            draw(imageB, {{0.0f, 1.0f, 0.0f, 1.0f}});
            Require(StorageTexture::FlushPending(address, bytes, nullptr, "clear-key hand-over test", PublishScope::Whole), where + ": nothing was pending at the end");
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            outcome.bytes = readMemory(address, surfaceA);
            Require(filled(readImage(imageB, 128, 128, 8), green), where + ": B lost its results");
        }
        outcome.handOvers = StorageTexture::ClearHandOvers() - handOvers;
        outcome.superseded = StorageTexture::HandedOverSuperseded() - superseded;
        recorder.Sync();
        return outcome;
    };
    for (const auto order : {Order::Superseded, Order::ReadFirst, Order::Rewritten, Order::CpuStore}) {
        const std::string name = what + (order == Order::Superseded ? ", A never read again" : order == Order::ReadFirst ? ", A read before B wrote" : order == Order::Rewritten ? ", A written again after the hand-over" : ", a CPU store before B wrote");
        const auto off = run(false, order);
        const auto on = run(true, order);
        const auto& lastA = order == Order::Rewritten ? blue : red;
        if (order != Order::CpuStore) Require(filled(std::span(off.bytes).first(2 * 65536), green), name + ": B's results did not reach guest memory with the hand-over off");
        Require(filled(std::span(off.bytes).subspan(2 * 65536), lastA), name + ": A's results did not reach guest memory with the hand-over off");
        Require(on.imageB == off.imageB, name + ": B held other texels with the hand-over on than off");
        Require(on.bytes == off.bytes, name + ": the hand-over left other guest bytes than the old order");
        Require(off.handOvers == 0 && off.superseded == 0, name + ": the hand-over ran while off");
        if (!watched) {
            Require(on.handOvers == 0, name + ": results were handed over in memory the write tracker does not watch");
            continue;
        }
        if (order == Order::Superseded) Require(on.superseded == 2, name + ": B's results did not drop A's two units under B (dropped " + std::to_string(on.superseded) + ")");
        if (order == Order::ReadFirst) Require(on.superseded <= 1, name + ": a unit stored by the read was dropped again");
    }
    std::cout << what << ": ok\n";
    return true;
}

void denormalClearTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: denormal fill clears not tested\n";
        return;
    }
    constexpr std::uint32_t side = 64;
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the denormal clear block");
    std::memset(block, 0x55, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the denormal clear block refused: denormal fill clears not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 29;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    Require(ResolveTextureFormat(resource.format) == VK_FORMAT_R16G16_SFLOAT, "guest format 29 is not R16G16_SFLOAT");
    const bool keeps = ClearKeepsDenormals(context, VK_FORMAT_R16G16_SFLOAT);
    std::cout << "denormal half clears are " << (keeps ? "kept" : "flushed") << " by this device\n";
    Require(ClearKeepsDenormals(context, VK_FORMAT_R16G16_SFLOAT) == keeps, "the denormal clear probe changed its answer");
    Require(!ClearKeepsDenormals(context, VK_FORMAT_R32G32B32A32_SFLOAT), "a format without small floats probed as keeping denormals");
    {
        auto image = StorageTexture::Create(context, detiler, resource, 0);
        const auto holds = [&](std::uint32_t texel) {
            Buffer readback(context, side * side * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {side, side, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            const auto pixels = readback.Bytes();
            for (std::size_t at = 0; at + 4 <= pixels.size(); at += 4) {
                std::uint32_t value = 0;
                std::memcpy(&value, pixels.data() + at, 4);
                if (value != texel) return false;
            }
            return true;
        };
        const char* refusal = nullptr;
        const std::array<std::uint32_t, 4> normal{0x3c00bc00u, 0x3c00bc00u, 0x3c00bc00u, 0x3c00bc00u};
        Require(image->FillClear(normal, StorageTexture::WholeImage, refusal), "a normal half pattern was refused");
        Require(holds(0x3c00bc00u), "a normal half fill clear stored other bits");
        const std::array<std::uint32_t, 4> denormal{0x83ff0001u, 0x83ff0001u, 0x83ff0001u, 0x83ff0001u};
        refusal = nullptr;
        const bool cleared = image->FillClear(denormal, StorageTexture::WholeImage, refusal);
        Require(cleared == keeps, keeps ? "a denormal half pattern was refused on a device whose clears keep denormals" : "a denormal half pattern was cleared on a device whose clears flush them");
        if (cleared) Require(holds(0x83ff0001u), "a denormal half fill clear stored other bits");
        const std::array<std::uint32_t, 4> nan{0x7e007e00u, 0x7e007e00u, 0x7e007e00u, 0x7e007e00u};
        refusal = nullptr;
        Require(!image->FillClear(nan, StorageTexture::WholeImage, refusal), "a NaN half pattern was cleared");
    }
    recorder.Sync();
}

void pipelineLibraryTests(const Device& device) {
    std::lock_guard gpu(GpuMutex());
    const auto& context = device.GetContext();
    struct Cleanup {
        VkDevice device;
        ~Cleanup() { ClearCachedPipelines(device); }
    } cleanup{context.device};
    ClearCachedPipelines(context.device);
    ShaderRecompiler::RecompileResult vertex, fragment;
    vertex.variantId = 11;
    fragment.variantId = 12;
    vertex.spirv = std::vector<std::uint32_t>(std::begin(TRIANGLE_vert_SPV), std::end(TRIANGLE_vert_SPV));
    fragment.spirv = std::vector<std::uint32_t>(std::begin(TRIANGLE_frag_SPV), std::end(TRIANGLE_frag_SPV));
    const std::array shaders{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}};
    State state{};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.hasColorTarget = true;
    state.color.format = VK_FORMAT_R8G8B8A8_UNORM;
    state.colors.push_back(state.color);
    state.blend.colorWriteMask = 15;
    state.blends.push_back(state.blend);
    ShaderResources resources(context, shaders, state.color, static_cast<std::uint32_t>(state.colors.size()), 0, 0);
    VertexInputLayout input;
    const auto lookup = [&] { return CachedPipeline(context, state, input, resources, shaders, VK_IMAGE_LAYOUT_GENERAL); };
    const auto counters = [&] { return PipelineLibraryCountersOf(context.device); };
    const auto first = lookup();
    if (!context.graphicsPipelineLibrary) {
        Require(counters().linked == 0, "a pipeline was linked from libraries without VK_EXT_graphics_pipeline_library");
        std::cout << "graphics pipeline libraries unavailable or disabled: pipelines are created whole\n";
        return;
    }
    const auto built = counters();
    Require(built.linked == 1 && built.built == std::array<std::uint64_t, 4>{1, 1, 1, 1}, "the first pipeline was not linked from four new libraries");
    state.blends.front().blendEnable = VK_TRUE;
    state.blends.front().srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    state.blends.front().dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    const auto blended = lookup();
    Require(blended != first && counters().built == std::array<std::uint64_t, 4>{1, 1, 1, 2}, "a blend change rebuilt more than the fragment output library");
    state.cullMode = VK_CULL_MODE_BACK_BIT;
    state.frontFace = VK_FRONT_FACE_CLOCKWISE;
    const auto culled = lookup();
    Require(culled != blended && counters().built == std::array<std::uint64_t, 4>{1, 1, 1, 2}, "a cull mode change rebuilt a library instead of setting dynamic state");
    input.bindings.push_back({0, 16, VK_VERTEX_INPUT_RATE_VERTEX});
    input.attributes.push_back({0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0});
    static_cast<void>(lookup());
    Require(counters().built == std::array<std::uint64_t, 4>{2, 1, 1, 2} && counters().linked == 4, "a vertex input change rebuilt more than the vertex input library");
    Require(StagePushOffset(88, ShaderRecompiler::ShaderStage::Fragment, true) == PipelinePushSlotBytes && StagePushOffset(88, ShaderRecompiler::ShaderStage::Fragment, false) == 88 && StagePushOffset(40, ShaderRecompiler::ShaderStage::TessellationEvaluation, true) == 40, "a stage got the wrong push offset");
    const auto beforeSlots = counters().built;
    ShaderRecompiler::RecompileResult otherVertex = vertex;
    otherVertex.spirv[2] ^= 1u;
    std::array slotted{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &fragment, PipelinePushSlotBytes}};
    static_cast<void>(CachedPipeline(context, state, input, resources, slotted, VK_IMAGE_LAYOUT_GENERAL));
    slotted.front().program = &otherVertex;
    static_cast<void>(CachedPipeline(context, state, input, resources, slotted, VK_IMAGE_LAYOUT_GENERAL));
    Require(counters().built[2] == beforeSlots[2] + 1u && counters().built[1] == beforeSlots[1] + 1u, "a pixel shader in its fixed slot built its library again for another vertex shader");
    fragment.spirv[2] ^= 1u;
    static_cast<void>(lookup());
    Require(counters().built == std::array<std::uint64_t, 4>{2, 2, 3, 2}, "a new pixel shader rebuilt more than the fragment shader library");
    state.colors.front().format = VK_FORMAT_R16G16B16A16_SFLOAT;
    static_cast<void>(lookup());
    Require(counters().built == std::array<std::uint64_t, 4>{2, 2, 3, 3}, "another target format rebuilt more than the fragment output library");
    state.samples = 4;
    state.colors.front().samples = 4;
    const auto multisampled = lookup();
    Require(counters().built == std::array<std::uint64_t, 4>{2, 2, 4, 4}, "a 4x pipeline shared the 1x fragment shader or fragment output library");
    state.samples = 1;
    state.colors.front().samples = 1;
    Require(lookup() != multisampled && counters().built == std::array<std::uint64_t, 4>{2, 2, 4, 4}, "the 1x pipeline after a 4x one built its libraries again");
    state.depth = DepthTarget{0, 0, {64, 64}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
    state.depthTest = true;
    state.depthCompare = VK_COMPARE_OP_LESS;
    static_cast<void>(lookup());
    Require(counters().built == std::array<std::uint64_t, 4>{2, 3, 5, 5}, "a depth target shared the shader libraries built with no depth format");
    state.depth.reset();
    state.depthTest = false;
    if ((context.sampleLocationSampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0) {
        state.samples = 4;
        state.colors.front().samples = 4;
        state.customSampleLocations = true;
        state.sampleLocations = {0x10, 0x21, 0x32, 0x43};
        const auto pattern = lookup();
        const auto beforePatternChange = counters().built;
        state.sampleLocations[0] = 0x20;
        const auto otherPattern = lookup();
        Require(otherPattern != pattern && counters().built[2] == beforePatternChange[2] + 1u && counters().built[3] == beforePatternChange[3] + 1u && counters().built[0] == beforePatternChange[0] && counters().built[1] == beforePatternChange[1], "a custom sample pattern did not rebuild exactly the fragment shader and fragment output libraries");
        state.sampleLocations[0] = 0x10;
        Require(lookup() == pattern && counters().built[2] == beforePatternChange[2] + 1u && counters().built[3] == beforePatternChange[3] + 1u, "a cached custom sample pattern was not reused");
        if ((context.sampleLocationGridSampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0) {
            state.sampleLocationGrid = true;
            state.sampleLocations[3u * 8u] = 0x20;
            const auto gridPattern = lookup();
            Require(gridPattern != pattern && counters().built[2] == beforePatternChange[2] + 2u && counters().built[3] == beforePatternChange[3] + 2u, "a pattern varying between pixels shared a uniform pattern's fragment libraries");
            state.sampleLocationGrid = false;
            state.sampleLocations[3u * 8u] = 0;
            Require(lookup() == pattern, "the uniform pattern was not reused after a grid pattern");
        }

        std::optional<VkFormat> compatibleDepth, incompatibleDepth;
        state.customSampleLocations = false;
        for (const auto format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
            state.depth = DepthTarget{};
            state.depth->format = format;
            state.depth->samples = state.samples;
            (ValidateSampleLocations(context, state) ? compatibleDepth : incompatibleDepth) = format;
        }
        if (compatibleDepth && incompatibleDepth) {
            state.depth->format = *compatibleDepth;
            const auto compatible = lookup();
            const auto beforeDepthChange = counters().built;
            state.depth->format = *incompatibleDepth;
            const auto incompatible = lookup();
            Require(incompatible != compatible && counters().built[2] == beforeDepthChange[2] + 1u && counters().built[3] == beforeDepthChange[3] + 1u, "depth-format sample-location enablement did not rebuild both fragment libraries");
        }
        state.customSampleLocations = true;
    }
    WaitForOptimizedPipelines(context.device);
    const auto lto = std::getenv("APS5_NO_PIPELINE_LTO") == nullptr && context.pipelineLinkTimeOptimization;
    Require(counters().optimized == (lto ? counters().linked : 0u), "a linked pipeline was not optimized in the background");
    Require(first->Optimized() == lto && culled->Optimized() == lto, "a pipeline did not switch to its optimized link");
    ClearCachedPipelines(context.device);
    Require(counters().linked == 0, "clearing the pipelines kept the device's libraries");
    std::cout << "Pipeline library reuse tests passed\n";
}

#ifdef _WIN32
namespace StorageCpuSnapshotTest {
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = std::size_t{Side} * Side * 4u;
constexpr std::size_t BlockBytes = 12 * SurfaceBytes;

class Block {
public:
    explicit Block(const Context& context, bool watched = false) : context(context), watched(watched) {
        if (watched) {
            memory = GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536);
            const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, BlockBytes, nullptr);
            Require(section != nullptr, "cannot allocate the shared texture section");
            try { GuestArena::GuestArenaMap_nid_postfix(memory, BlockBytes, section, 0, PAGE_READWRITE); }
            catch (...) { CloseHandle(section); throw; }
            CloseHandle(section);
        } else memory = VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(memory != nullptr, "cannot allocate the texture block");
        std::memset(memory, 0x5a, BlockBytes);
        GuestAllocations::Mutation().Add(memory, BlockBytes, true, true, true);
    }

    ~Block() {
        ClearCachedTextures(context.device);
        GuestAllocations::Mutation().Remove(memory);
        HostImportFor(context, Surface(0), BlockBytes);
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(memory, BlockBytes);
            GuestArena::GuestArenaRelease_nid_postfix(memory, BlockBytes);
        } else VirtualFree(memory, 0, MEM_RELEASE);
    }

    std::uint64_t Surface(std::size_t surface) const { return reinterpret_cast<std::uint64_t>(memory) + surface * SurfaceBytes; }

private:
    const Context& context;
    bool watched;
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

GuestTextureResource Resource(std::uint64_t address, std::uint32_t side = Side) {
    const auto words = Descriptor(address, side);
    return DecodeTextureResource(words);
}

void Run(Context context) {
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    for (const bool watched : {true, false}) {
        Block block(context, watched);
        const auto address = block.Surface(0);
        Require(AgcDriver::GuestMemory::Watched(address, SurfaceBytes) == watched, "snapshot fixture has the wrong write tracking");
        Require(HostImportFor(context, address, SurfaceBytes) != nullptr, "snapshot fixture requires a host import");
        Require(AgcDriver::GuestMemory::Watched(address, SurfaceBytes) == watched, "host import changed the fixture's write tracking");
        auto image = StorageTexture::Create(context, *context.detiler, Resource(address), 0);
        const auto expectedBytes = watched ? 0u : SurfaceBytes;
        Require(image->OriginalSnapshotBytes() == expectedBytes, "watched GPU-direct upload retained an unnecessary CPU surface snapshot");
        const auto readPixel = [&] {
            Buffer result(context, 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            CommandBatch batch(context);
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {1, 1, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(batch.Handle(), image->Image(), VK_IMAGE_LAYOUT_GENERAL, result.Handle(), 1, &copy);
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            batch.SubmitAndWait();
            result.Invalidate();
            std::uint32_t pixel = 0;
            std::memcpy(&pixel, result.Bytes().data(), sizeof(pixel));
            return pixel;
        };
        Require(readPixel() == 0x5a5a5a5au, "snapshot upload returned incorrect GPU texels");
        std::memset(reinterpret_cast<void*>(address), 0x6b, SurfaceBytes);
        image->Refresh();
        Require(readPixel() == 0x6b6b6b6bu, "snapshot refresh missed a CPU write");
        Require(image->OriginalSnapshotBytes() == expectedBytes, "watched refresh allocated a CPU surface snapshot");
        {
            CommandBatch batch(context);
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            VkClearColorValue clear{};
            clear.uint32[0] = 0x7c7c7c7cu;
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(batch.Handle(), image->Image(), VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
            batch.SubmitAndWait();
        }
        image->MarkDirty();
        image->WriteBack();
        std::uint32_t pixel = 0;
        AgcDriver::GuestMemory::ReadCommitted(address, std::as_writable_bytes(std::span(&pixel, 1)));
        Require(pixel == 0x7c7c7c7cu, "snapshot write-back lost GPU texels");
        Require(image->OriginalSnapshotBytes() == expectedBytes, "GPU-direct write-back allocated a CPU surface snapshot");
        if (watched) {
            AgcDriver::GuestMemory::Unwatch(address, SurfaceBytes);
            Require(!AgcDriver::GuestMemory::Watched(address, SurfaceBytes), "snapshot fixture is still write-watched");
            std::memset(reinterpret_cast<void*>(address), 0x8d, SurfaceBytes);
            image->Refresh();
            Require(image->OriginalSnapshotBytes() == SurfaceBytes, "unwatched refresh did not allocate its comparison snapshot");
            Require(readPixel() == 0x8d8d8d8du, "unwatched refresh returned incorrect GPU texels");
        }
        image.reset();
        Context cpu = context;
        cpu.hostImportAlignment = 0;
        image = StorageTexture::Create(cpu, *context.detiler, Resource(address), 0);
        Require(image->OriginalSnapshotBytes() == SurfaceBytes, "CPU upload has no guest-byte snapshot");
        const auto expectedPixel = watched ? 0x8d8d8d8du : 0x7c7c7c7cu;
        Require(readPixel() == expectedPixel, "CPU upload returned incorrect GPU texels");
        {
            CommandBatch batch(context);
            VkClearColorValue clear{};
            clear.uint32[0] = 0x9e9e9e9eu;
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(batch.Handle(), image->Image(), VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
            batch.SubmitAndWait();
        }
        image->MarkDirty();
        image->WriteBack();
        AgcDriver::GuestMemory::ReadCommitted(address, std::as_writable_bytes(std::span(&pixel, 1)));
        Require(pixel == 0x9e9e9e9eu, "CPU write-back lost guest texels");
        std::printf("%s snapshot fixture: direct %zu bytes, CPU fallback %zu bytes\n", watched ? "watched" : "unwatched", expectedBytes, image->OriginalSnapshotBytes());
    }
}
}
#endif

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--collect-mapping-history-only") {
            if (!collectMappingHistoryTests()) return 77;
            std::cout << "collect mapping history tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--hint-mapping-history-only") {
            if (!hintMappingHistoryTests()) return 77;
            std::cout << "hint mapping history tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--partial-watch-restoration-only") {
            if (!partialWatchRestorationTests()) return 77;
            std::cout << "partial private write-watch restoration tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--device-selection-only") {
            deviceSelectionTests();
            std::cout << "recorder device selection tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--queued-write-groups-only") {
            queuedWriteGroupTests();
            std::cout << "queued write group tests passed\n";
            return 0;
        }
        std::unique_ptr<Device> created;
        try {
            created = std::make_unique<Device>(argc == 2 && (std::string_view(argv[1]) == "--import-watch-other-family-only" || std::string_view(argv[1]) == "--deferred-settle-other-family-only" || std::string_view(argv[1]) == "--host-import-lifetime-other-family-only" || std::string_view(argv[1]) == "--settle-write-order-other-family-only"));
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::cout << "skipped, no usable Vulkan device: " << error.what() << '\n';
            return 77;
        }
        Device& device = *created;
        if (argc == 2 && std::string_view(argv[1]) == "--host-import-lifetime-only") return RunHostImportLifetimeTests(device.GetContext());
        if (argc == 2 && std::string_view(argv[1]) == "--host-import-mutation-race-only") return RunHostImportMutationRaceTests(device.GetContext());
        if (argc == 2 && (std::string_view(argv[1]) == "--gpu-timing-complete-only" || std::string_view(argv[1]) == "--gpu-timing-incomplete-only")) {
            return gpuTimingCompletionTests(device, std::string_view(argv[1]) == "--gpu-timing-incomplete-only");
        }
        if (argc == 2 && std::string_view(argv[1]) == "--import-watch-only") {
            importWatchTests(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--import-watch-other-family-only") {
            if (device.GetContext().settleQueue == VK_NULL_HANDLE) {
                std::cout << "skipped, no other queue family to settle imports on\n";
                return 77;
            }
            importWatchTests(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--settle-initial-dcc-only") {
#ifdef _WIN32
            return settleInitialDccTests(device) ? 0 : 77;
#else
            std::cout << "skipped, the device has no deferred settle of host imports: imports under 1 MiB wait for one only on Windows\n";
            return 77;
#endif
        }
#ifdef _WIN32
        if (argc == 2 && std::string_view(argv[1]) == "--host-import-lifetime-other-family-only") {
            if (device.GetContext().settleFamily == device.GetContext().queueFamily) {
                std::cout << "skipped, no other queue family to settle imports on\n";
                return 77;
            }
            return RunHostImportLifetimeTests(device.GetContext());
        }
        if (argc == 2 && (std::string_view(argv[1]) == "--deferred-settle-only" || std::string_view(argv[1]) == "--deferred-settle-other-family-only")) {
            if (std::string_view(argv[1]) == "--deferred-settle-other-family-only" && device.GetContext().settleFamily == device.GetContext().queueFamily) {
                std::cout << "skipped, no other queue family to settle imports on\n";
                return 77;
            }
            return deferredSettleTests(device) ? 0 : 77;
        }
        if (argc == 2 && (std::string_view(argv[1]) == "--settle-write-order-only" || std::string_view(argv[1]) == "--settle-write-order-other-family-only")) {
            if (std::string_view(argv[1]) == "--settle-write-order-other-family-only" && device.GetContext().settleFamily == device.GetContext().queueFamily) {
                std::cout << "skipped, no other queue family to settle imports on\n";
                return 77;
            }
            return settleWriteOrderTests(device) ? 0 : 77;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--settle-wait-failure-only") return settleWaitFailureTests(device) ? 0 : 77;
        if (argc == 2 && std::string_view(argv[1]) == "--settle-first-use-only") return settleFirstUseTests(device) ? 0 : 77;
        if (argc == 2 && std::string_view(argv[1]) == "--settle-earlier-import-only") return settleEarlierImportTests(device) ? 0 : 77;
        if (argc == 2 && std::string_view(argv[1]) == "--settle-retire-only") return settleRetireTests(device) ? 0 : 77;
        if (argc == 2 && std::string_view(argv[1]) == "--settle-storage-image-only") return settleStorageImageTests(device) ? 0 : 77;
        if (argc == 2 && std::string_view(argv[1]) == "--reimport-bench") return reimportBench(device);
        if (argc == 2 && std::string_view(argv[1]) == "--host-import-cut-only") return hostImportCutTests(device);
#endif
        if (argc == 2 && std::string_view(argv[1]) == "--host-import-retirement-only") {
            const auto alignment = device.GetContext().hostImportAlignment;
            if (alignment == 0 || alignment > 65536) {
                std::cout << "skipped, host import retirement requires host imports aligned within 64 KiB; alignment=" << alignment << '\n';
                return 77;
            }
            hostImportRetirementTests(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sparse-run-import-only") {
            if (device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, sparse run imports require host imports\n";
                return 77;
            }
            return sparseRunImportTests(device, false) ? 0 : 77;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sparse-run-copied-only") {
            if (device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, sparse run imports require host imports\n";
                return 77;
            }
            return sparseRunImportTests(device, true) ? 0 : 77;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--pipeline-library-only") {
            pipelineLibraryTests(device);
            return 0;
        }
        if ((argc == 2 || argc == 3) && std::string_view(argv[1]) == "--benchmark-pipeline-store-memory") return pipelineStoreMemoryBench(device, argc == 3 ? static_cast<std::uint32_t>(std::stoul(argv[2])) : 4096u);
        if (argc == 2 && (std::string_view(argv[1]) == "--benchmark-pipeline-cache" || std::string_view(argv[1]) == "--pipeline-cache-only")) {
            pipelineCacheTests(device, std::string_view(argv[1]) == "--benchmark-pipeline-cache");
            return 0;
        }

        if (argc == 2 && std::string_view(argv[1]) == "--pipeline-cache-file-only") {
            return pipelineCacheFileTests(device);
        }

        if (argc == 2 && std::string_view(argv[1]) == "--pipeline-cache-header-only") {
            pipelineCacheHeaderTests(device);
            return 0;
        }

        if (argc == 2 && std::string_view(argv[1]) == "--descriptor-pool-only") {
            descriptorPoolTests(device);
            return 0;
        }
        std::unique_lock gpu(GpuMutex());
#ifdef _WIN32
        if (argc == 2 && std::string_view(argv[1]) == "--storage-cpu-snapshot-only") {
            if (device.GetContext().hostImportAlignment == 0) {
                Require(std::getenv("ANYPS5_REQUIRE_VULKAN") == nullptr, "storage snapshot test requires host imports");
                std::cout << "skipped, host imports unavailable\n";
                return 77;
            }
            StorageCpuSnapshotTest::Run(device.GetContext());
            std::cout << "Storage CPU snapshot tests passed\n";
            return 0;
        }
#endif
        std::cout << "host imports " << (PrepareImportWatch(device.GetContext()) == ImportWatch::Unwatch ? "are compared" : "stay watched") << '\n';
        Recorder recorder(device.GetContext());
        recorder.Activate();
        if (argc == 2 && std::string_view(argv[1]) == "--command-reuse-only") {
            if (device.GetContext().hostImportAlignment == 0) return 77;
            const auto before = Recorder::StoreCounts().stores;
            storeRunTests(device, recorder);
            return Recorder::StoreCounts().stores != before ? 0 : 77;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--copied-direct-only") {
            copiedDirectOrderingTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--pending-write-snapshot-only") {
            pendingWriteSnapshotTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--benchmark-pending-write-snapshot") {
            pendingWriteSnapshotBenchmark(recorder);
            return 0;
        }
        if (argc == 2 && (std::string_view(argv[1]) == "--storage-upload-lifetime-only" || std::string_view(argv[1]) == "--storage-upload-failure-only")) {
            storageUploadLifetimeTests(device, recorder, std::string_view(argv[1]) == "--storage-upload-failure-only");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--tiled-resolve-only") {
            if ((device.GetContext().limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0 || !device.Graphics()) {
                std::cout << "skipped, the device lacks 4x color targets or a graphics queue\n";
                return 77;
            }
            tiledResolveTests(device, recorder);
            std::cout << "CB resolves into tiled targets tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--cube-only") {
            singleCubeTests(device, recorder);
            cubeExtraSliceTests(device, recorder);
            std::cout << "Single cube snapshot and storage sampling tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sampled-storage-base-level-usage-only") {
            sampledStorageBaseLevelUsageTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--unchanged-cpu-stamps-only") {
            unchangedCpuStampTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--kept-unit-settle-only") {
            keptUnitSettleTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--clear-hand-over-only") {
            clearHandOverTests(device, recorder, false);
            if (!clearHandOverTests(device, recorder, true)) {
                std::cout << "skipped, the device imports no watched host memory for the clear-key hand-over\n";
                return 77;
            }
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--integer-clear-only") {
            integerClearTests(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--settled-baseline-only") {
            settledBaselineTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--completion-labels-only") {
            completionCountTests(device, recorder);
            afterRecordedWorkTests(device, recorder);
            directCompletionLabelTests(recorder);
            labelTests(recorder);
            lateLabelTests(recorder);
            largeLabelTests(recorder);
            std::cout << "Completion label storage and ordering tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--bda-staging-only") {
            if (!bdaStagingTests(device, recorder)) return 77;
            std::cout << "address-based staging tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--read-written-staging-only") {
            if (!readWrittenStagingTests(device, recorder)) return 77;
            std::cout << "read and written staging tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--written-buffer-order-only") {
            if (!writtenBufferOrderTests(device, recorder)) return 77;
            std::cout << "written buffer order tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--read-list-only") {
            readListTests(device, recorder);
            std::cout << "Read list tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--deferred-failure-report-only") {
            deferredFailureReportTests(recorder);
            std::cout << "Deferred failure report tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--host-import-unmap-only") {
            gpu.unlock();
            hostImportUnmapTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--slice-dirty-only") {
            sliceDirtyTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--resident-present-only") {
            RunResidentPresentTests(device.GetContext());
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--parked-import-unmap-only") {
            gpu.unlock();
            parkedImportUnmapTests(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--idle-publish-wait-only") {
            if (!AgcDriver::Graphics::UnitShadowEnabled() || device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, unit shadows or host imports are unavailable\n";
                return 77;
            }
            idlePublishWaitTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--unit-shadow-only") {
            if (!AgcDriver::Graphics::UnitShadowEnabled() || device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, unit shadows or host imports are unavailable\n";
                return 77;
            }
            if (!unitShadowTests(device, recorder)) {
                std::cout << "skipped, the device does not import watched host memory for unit shadows\n";
                return 77;
            }
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--bda-shadowed-results-only") {
            return addressShadowTests(device, recorder) ? 0 : 77;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--completion-store-shadow-only") {
            return completionStoreShadowTests(device, recorder) ? 0 : 77;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--split-writeback-only") {
            if (device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, host imports are unavailable\n";
                return 77;
            }
            splitWriteBackTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--storage-write-back-only") {
            if (device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, host imports are unavailable\n";
                return 77;
            }
            storageRefreshTests(device, recorder, false);
            storageRefreshTests(device, recorder, true);
            writeBackPaddingTests(device, recorder, false);
            writeBackPaddingTests(device, recorder, true);
            singlePassTests(device, recorder, false);
            singlePassTests(device, recorder, true);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--cmask-only") {
            cmaskPassTests(device, recorder);
            std::cout << "CMASK copied clear, write-back and failure tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--writeback-blocks-only") {
            if (device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, host imports are unavailable\n";
                return 77;
            }
            writeBackBlockTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--metadata-pass-only") {
            metadataPassTests(device, recorder);
            std::cout << "Color metadata pass tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--pending-key-stores-only") {
            if (device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, host imports are unavailable\n";
                return 77;
            }
            if (!pendingKeyStoreTests(device, recorder)) {
                std::cout << "skipped, the device does not import the pending key store block\n";
                return 77;
            }
            std::cout << "Pending DCC key store tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--sample-dumps-only") {
            if (device.GetContext().hostImportAlignment == 0) {
                std::cout << "skipped, host imports are unavailable\n";
                return 77;
            }
            if (!device.GetContext().bufferDeviceAddress) {
                std::cout << "skipped, the device lacks buffer device addresses\n";
                return 77;
            }
            sampleDumpTests(device, recorder);
            std::cout << "Occlusion counter dump tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--uncounted-passes-only") {
            if (!device.Graphics()) {
                std::cout << "skipped, the test queue has no graphics\n";
                return 77;
            }
            uncountedPassTests(device);
            std::cout << "Uncounted pass tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--shadowed-key-stores-only") {
            if (!shadowedKeyStoreTests(device, recorder)) {
                std::cout << "skipped, the device imports no watched host memory under a unit shadow\n";
                return 77;
            }
            std::cout << "DCC keys under unit shadows tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--draw-inputs-in-place-only") {
            drawInputInPlaceTests(device, recorder);
            std::cout << "In-place draw input tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--draw-inputs-copied-only") {
            drawInputInPlaceTests(device, recorder, true);
            std::cout << "Copied draw input tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--multisample-only") {
            multisampleGuestAccessTests(device, recorder);
            oneFragmentFmaskGuardTests(device);
            fmaskDecompressMsaaTests(device, recorder);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--draw-descriptors-only") {
            drawDescriptorWriteTests(device, recorder);
            drawSnapshotTableSetTests(device, recorder);
            imageTableFirstLayerRevalidateTests(device, recorder);
            std::cout << "Draw descriptor writes and snapshot overrides passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--space-pending-writes-only") {
            overlappedRangeTests(recorder);
            spaceWriteSyncTests(device, recorder);
            std::cout << "Address space pending write tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--draw-snapshot-cap-only") {
            drawSnapshotCapTests(device, recorder);
            containingSnapshotTests(device);
            unwrittenSnapshotTests(device);
            drawSnapshotUploadTests(device, recorder);
            std::cout << "Draw snapshot cap tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--draw-snapshot-epoch-only") {
            drawSnapshotEpochTests(device);
            return 0;
        }

        if (argc == 2 && (std::string_view(argv[1]) == "--vertex-target-range-copied" || std::string_view(argv[1]) == "--vertex-target-range-imported")) {
            return vertexTargetRangeTests(device, recorder, std::string_view(argv[1]) == "--vertex-target-range-imported") ? 0 : 77;
        }

#ifdef _WIN32
        if (argc == 2 && std::string_view(argv[1]) == "--host-import-second-mapping-only") {
            secondMappingImportTests(device);
            return 0;
        }
#endif
        readTrackingTests(device, recorder);
        readListTests(device, recorder);
        overlappedRangeTests(recorder);
        spaceWriteSyncTests(device, recorder);
        writeSettledTests(device, recorder);
        completionCountTests(device, recorder);
        afterRecordedWorkTests(device, recorder);
        directCompletionLabelTests(recorder);
        batchStampTests(recorder);
        labelTests(recorder);
        lateLabelTests(recorder);
        largeLabelTests(recorder);
        unchangedSinceTests();
        closeRaceTests(device, recorder);
        keyProofTests(device, recorder);
        targetKeyProofTests(device, recorder);
        refreshProofTests(device, recorder);
        resourceReadTests(device, recorder);
        readWrittenStagingTests(device, recorder);
        bdaStagingTests(device, recorder);
        depthSurfaceProofTests(device, recorder);
        multisampleGuestAccessTests(device, recorder);
        oneFragmentFmaskGuardTests(device);
        tiledResolveTests(device, recorder);
        fmaskDecompressMsaaTests(device, recorder);
        misalignedSnapshotTests(device, recorder);
        misalignedRegionTests(device, recorder);
        drawSnapshotReuseTests(device, recorder);
        drawDescriptorWriteTests(device, recorder);
        drawSnapshotTableSetTests(device, recorder);
        imageTableFirstLayerRevalidateTests(device, recorder);
        drawSnapshotCapTests(device, recorder);
        drawSnapshotEvictionTests(device);
        containingSnapshotTests(device);
        unwrittenSnapshotTests(device);
        drawSnapshotUploadTests(device, recorder);
        drawInputReuseTests(device, recorder);
        drawInputInPlaceTests(device, recorder);
        RunResidentPresentTests(device.GetContext());
        sliceDirtyTests(device, recorder);
        storeRunTests(device, recorder);
        queuedWriteGroupTests();
        remappedImportTests(device);
#ifdef _WIN32
        secondMappingImportTests(device);
#endif
        movedMetadataTests(device, recorder);
        viewPastLastMipTests(device, recorder);
        keysFillTests(device, recorder);
        clearHandOverTests(device, recorder, false);
        clearHandOverTests(device, recorder, true);
        integerClearTests(device);
        denormalClearTests(device, recorder);
        unitShadowTests(device, recorder);
        storageRefreshTests(device, recorder, false);
        storageRefreshTests(device, recorder, true);
        static_cast<void>(writtenBufferOrderTests(device, recorder));
        writeBackPaddingTests(device, recorder, false);
        writeBackPaddingTests(device, recorder, true);
        splitWriteBackTests(device, recorder);
        writeBackBlockTests(device, recorder);
        singlePassTests(device, recorder, false);
        singlePassTests(device, recorder, true);
        unchangedCpuStampTests(device, recorder);
        keptUnitSettleTests(device, recorder);
        settledBaselineTests(device, recorder);
        importWatchTests(device);
        staleGenerationTests(device, recorder);
        importWindowTests(device, recorder);
        dataWordPositionsTests();
        dataRefreshTests(device, recorder);
        imageMemoTests(device, recorder);
        minLodTests(device, recorder);
        firstLayerViewTests(device, recorder);
        sampledStorageBaseLevelUsageTests(device, recorder);
        singleCubeTests(device, recorder);
        cubeExtraSliceTests(device, recorder);
        atomicViewTests(device, recorder);
        metadataPassTests(device, recorder);
        cmaskPassTests(device, recorder);
        pendingKeyStoreTests(device, recorder);
        shadowedKeyStoreTests(device, recorder);
        sampleDumpTests(device, recorder);
        copiedDirectOrderingTests(device, recorder);
        gpu.unlock();
        hostImportUnmapTests(device, recorder);
        drawSnapshotPatchTests(device);
        drawSnapshotEpochTests(device);
        gpu.lock();
        recorder.Activate();
        std::cout << "Recorder read tracking and label tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
