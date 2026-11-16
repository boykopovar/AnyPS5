#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeapObjects.hpp"

#include <array>
#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

enum class Hook : std::size_t { CreateImageView, DestroyImageView, CreateSampler, DestroySampler, Count };

using RealFunctions = std::array<PFN_vkVoidFunction, static_cast<std::size_t>(Hook::Count)>;

std::mutex& recordsMutex() {
    static auto& mutex = *new std::mutex();
    return mutex;
}

std::unordered_map<VkDevice, RealFunctions>& realFunctions() {
    static auto& map = *new std::unordered_map<VkDevice, RealFunctions>();
    return map;
}

template <typename TFunction>
TFunction realFunction(VkDevice device, Hook hook) {
    std::lock_guard lock(recordsMutex());
    const auto found = realFunctions().find(device);
    if (found == realFunctions().end()) return nullptr;
    return reinterpret_cast<TFunction>(found->second[static_cast<std::size_t>(hook)]);
}

template <typename THandle>
struct DeviceHandle {
    VkDevice device;
    THandle handle;
    bool operator==(const DeviceHandle& other) const { return device == other.device && handle == other.handle; }
};

template <typename THandle>
struct DeviceHandleHash {
    std::size_t operator()(const DeviceHandle<THandle>& key) const {
        const auto device = std::hash<VkDevice>{}(key.device);
        const auto handle = std::hash<THandle>{}(key.handle);
        return device ^ (handle + 0x9e3779b97f4a7c15ull + (device << 6u) + (device >> 2u));
    }
};

struct EncodedImageView {
    std::uint32_t key = 0;
    std::vector<std::byte> bytes;
};

struct ViewEntry {
    ImageViewRecord record;
    std::vector<EncodedImageView> encoded;
};

using ViewKey = DeviceHandle<VkImageView>;
using SamplerKey = DeviceHandle<VkSampler>;

std::unordered_map<ViewKey, ViewEntry, DeviceHandleHash<VkImageView>>& views() {
    static auto& map = *new std::unordered_map<ViewKey, ViewEntry, DeviceHandleHash<VkImageView>>();
    return map;
}

std::unordered_map<SamplerKey, SamplerRecord, DeviceHandleHash<VkSampler>>& samplers() {
    static auto& map = *new std::unordered_map<SamplerKey, SamplerRecord, DeviceHandleHash<VkSampler>>();
    return map;
}

ImageViewRecord recordOf(const VkImageViewCreateInfo& info) {
    ImageViewRecord record;
    record.info = info;
    record.info.pNext = nullptr;
    for (auto* next = static_cast<const VkBaseInStructure*>(info.pNext); next != nullptr; next = next->pNext) {
        if (next->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO) {
            record.usage = *reinterpret_cast<const VkImageViewUsageCreateInfo*>(next);
            record.hasUsage = true;
        } else if (next->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT) {
            record.minLod = *reinterpret_cast<const VkImageViewMinLodCreateInfoEXT*>(next);
            record.hasMinLod = true;
        }
    }
    return record;
}

SamplerRecord recordOf(const VkSamplerCreateInfo& info) {
    SamplerRecord record;
    record.info = info;
    record.info.pNext = nullptr;
    for (auto* next = static_cast<const VkBaseInStructure*>(info.pNext); next != nullptr; next = next->pNext) {
        if (next->sType == VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO) {
            record.reduction = *reinterpret_cast<const VkSamplerReductionModeCreateInfo*>(next);
            record.hasReduction = true;
        }
    }
    return record;
}

VKAPI_ATTR VkResult VKAPI_CALL createImageView(VkDevice device, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* allocator,
                                               VkImageView* view) {
    const auto real = realFunction<PFN_vkCreateImageView>(device, Hook::CreateImageView);
    if (real == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    const auto result = real(device, info, allocator, view);
    if (result == VK_SUCCESS) {
        auto record = recordOf(*info);
        static std::atomic<std::uint64_t> generations{0};
        record.generation = generations.fetch_add(1, std::memory_order_relaxed) + 1u;
        std::lock_guard lock(recordsMutex());
        views()[{device, *view}] = ViewEntry{std::move(record), {}};
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL destroyImageView(VkDevice device, VkImageView view, const VkAllocationCallbacks* allocator) {
    {
        std::lock_guard lock(recordsMutex());
        views().erase({device, view});
    }
    if (const auto real = realFunction<PFN_vkDestroyImageView>(device, Hook::DestroyImageView)) real(device, view, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL createSampler(VkDevice device, const VkSamplerCreateInfo* info, const VkAllocationCallbacks* allocator,
                                             VkSampler* sampler) {
    const auto real = realFunction<PFN_vkCreateSampler>(device, Hook::CreateSampler);
    if (real == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    const auto result = real(device, info, allocator, sampler);
    if (result == VK_SUCCESS) {
        auto record = recordOf(*info);
        std::lock_guard lock(recordsMutex());
        samplers()[{device, *sampler}] = std::move(record);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL destroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* allocator) {
    {
        std::lock_guard lock(recordsMutex());
        samplers().erase({device, sampler});
    }
    if (const auto real = realFunction<PFN_vkDestroySampler>(device, Hook::DestroySampler)) real(device, sampler, allocator);
}

struct ObjectHook {
    const char* name;
    Hook slot;
    PFN_vkVoidFunction hook;
};

const std::array<ObjectHook, 4> ObjectHooks{{
    {"vkCreateImageView", Hook::CreateImageView, reinterpret_cast<PFN_vkVoidFunction>(&createImageView)},
    {"vkDestroyImageView", Hook::DestroyImageView, reinterpret_cast<PFN_vkVoidFunction>(&destroyImageView)},
    {"vkCreateSampler", Hook::CreateSampler, reinterpret_cast<PFN_vkVoidFunction>(&createSampler)},
    {"vkDestroySampler", Hook::DestroySampler, reinterpret_cast<PFN_vkVoidFunction>(&destroySampler)},
}};

}

const VkImageViewCreateInfo* ImageViewRecord::Chain() {
    info.pNext = nullptr;
    usage.pNext = nullptr;
    minLod.pNext = nullptr;
    if (hasMinLod) {
        minLod.pNext = info.pNext;
        info.pNext = &minLod;
    }
    if (hasUsage) {
        usage.pNext = info.pNext;
        info.pNext = &usage;
    }
    return &info;
}

const VkSamplerCreateInfo* SamplerRecord::Chain() {
    info.pNext = nullptr;
    reduction.pNext = nullptr;
    if (hasReduction) info.pNext = &reduction;
    return &info;
}

PFN_vkVoidFunction DescriptorHeapObjectFunction(VkDevice device, PFN_vkGetDeviceProcAddr deviceProc, const char* name) {
    if (device == VK_NULL_HANDLE) return nullptr;
    for (const auto& entry : ObjectHooks) {
        if (std::strcmp(name, entry.name) != 0) continue;
        const auto function = deviceProc(device, name);
        if (function == nullptr) return nullptr;
        std::lock_guard lock(recordsMutex());
        realFunctions()[device][static_cast<std::size_t>(entry.slot)] = function;
        return entry.hook;
    }
    return nullptr;
}

bool CopyEncodedImageView(VkDevice device, VkImageView view, std::uint32_t key, std::span<std::byte> target) {
    std::lock_guard lock(recordsMutex());
    const auto found = views().find({device, view});
    if (found == views().end()) return false;
    for (const auto& encoded : found->second.encoded) {
        if (encoded.key != key || encoded.bytes.size() != target.size()) continue;
        std::memcpy(target.data(), encoded.bytes.data(), target.size());
        return true;
    }
    return false;
}

void KeepEncodedImageView(VkDevice device, VkImageView view, std::uint64_t generation, std::uint32_t key, std::span<const std::byte> bytes) {
    std::lock_guard lock(recordsMutex());
    const auto record = views().find({device, view});
    if (record == views().end() || record->second.record.generation != generation) return;
    auto& entries = record->second.encoded;
    for (auto& encoded : entries) {
        if (encoded.key == key) {
            encoded.bytes.assign(bytes.begin(), bytes.end());
            return;
        }
    }
    entries.push_back({key, std::vector<std::byte>(bytes.begin(), bytes.end())});
}

std::optional<ImageViewRecord> FindImageView(VkDevice device, VkImageView view) {
    std::lock_guard lock(recordsMutex());
    const auto found = views().find({device, view});
    if (found == views().end()) return std::nullopt;
    return found->second.record;
}

std::optional<SamplerRecord> FindSampler(VkDevice device, VkSampler sampler) {
    std::lock_guard lock(recordsMutex());
    const auto found = samplers().find({device, sampler});
    if (found == samplers().end()) return std::nullopt;
    return found->second;
}

}
