#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeapObjects.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

std::atomic<PFN_vkCreateImageView> realCreateImageView{nullptr};
std::atomic<PFN_vkDestroyImageView> realDestroyImageView{nullptr};
std::atomic<PFN_vkCreateSampler> realCreateSampler{nullptr};
std::atomic<PFN_vkDestroySampler> realDestroySampler{nullptr};

std::mutex& recordsMutex() {
    static auto& mutex = *new std::mutex();
    return mutex;
}

std::unordered_map<VkImageView, ImageViewRecord>& views() {
    static auto& map = *new std::unordered_map<VkImageView, ImageViewRecord>();
    return map;
}

struct EncodedImageView {
    std::uint32_t key = 0;
    std::vector<std::byte> bytes;
};

std::unordered_map<VkImageView, std::vector<EncodedImageView>>& encodedViews() {
    static auto& map = *new std::unordered_map<VkImageView, std::vector<EncodedImageView>>();
    return map;
}

std::unordered_map<VkSampler, SamplerRecord>& samplers() {
    static auto& map = *new std::unordered_map<VkSampler, SamplerRecord>();
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
    const auto result = realCreateImageView.load(std::memory_order_acquire)(device, info, allocator, view);
    if (result == VK_SUCCESS) {
        auto record = recordOf(*info);
        static std::atomic<std::uint64_t> generations{0};
        record.generation = generations.fetch_add(1, std::memory_order_relaxed) + 1u;
        std::lock_guard lock(recordsMutex());
        views()[*view] = std::move(record);
        encodedViews().erase(*view);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL destroyImageView(VkDevice device, VkImageView view, const VkAllocationCallbacks* allocator) {
    {
        std::lock_guard lock(recordsMutex());
        views().erase(view);
        encodedViews().erase(view);
    }
    realDestroyImageView.load(std::memory_order_acquire)(device, view, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL createSampler(VkDevice device, const VkSamplerCreateInfo* info, const VkAllocationCallbacks* allocator,
                                             VkSampler* sampler) {
    const auto result = realCreateSampler.load(std::memory_order_acquire)(device, info, allocator, sampler);
    if (result == VK_SUCCESS) {
        auto record = recordOf(*info);
        std::lock_guard lock(recordsMutex());
        samplers()[*sampler] = std::move(record);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL destroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* allocator) {
    {
        std::lock_guard lock(recordsMutex());
        samplers().erase(sampler);
    }
    realDestroySampler.load(std::memory_order_acquire)(device, sampler, allocator);
}

template <typename TFunction>
bool resolve(std::atomic<TFunction>& real, VkDevice device, PFN_vkGetDeviceProcAddr deviceProc, const char* name) {
    const auto function = reinterpret_cast<TFunction>(deviceProc(device, name));
    if (function == nullptr) return false;
    real.store(function, std::memory_order_release);
    return true;
}

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
    if (std::strcmp(name, "vkCreateImageView") == 0) {
        if (!resolve(realCreateImageView, device, deviceProc, name)) return nullptr;
        return reinterpret_cast<PFN_vkVoidFunction>(&createImageView);
    }
    if (std::strcmp(name, "vkDestroyImageView") == 0) {
        if (!resolve(realDestroyImageView, device, deviceProc, name)) return nullptr;
        return reinterpret_cast<PFN_vkVoidFunction>(&destroyImageView);
    }
    if (std::strcmp(name, "vkCreateSampler") == 0) {
        if (!resolve(realCreateSampler, device, deviceProc, name)) return nullptr;
        return reinterpret_cast<PFN_vkVoidFunction>(&createSampler);
    }
    if (std::strcmp(name, "vkDestroySampler") == 0) {
        if (!resolve(realDestroySampler, device, deviceProc, name)) return nullptr;
        return reinterpret_cast<PFN_vkVoidFunction>(&destroySampler);
    }
    return nullptr;
}

bool CopyEncodedImageView(VkImageView view, std::uint32_t key, std::span<std::byte> target) {
    std::lock_guard lock(recordsMutex());
    const auto found = encodedViews().find(view);
    if (found == encodedViews().end()) return false;
    for (const auto& encoded : found->second) {
        if (encoded.key != key || encoded.bytes.size() != target.size()) continue;
        std::memcpy(target.data(), encoded.bytes.data(), target.size());
        return true;
    }
    return false;
}

void KeepEncodedImageView(VkImageView view, std::uint64_t generation, std::uint32_t key, std::span<const std::byte> bytes) {
    std::lock_guard lock(recordsMutex());
    const auto record = views().find(view);
    if (record == views().end() || record->second.generation != generation) return;
    auto& entries = encodedViews()[view];
    for (auto& encoded : entries) {
        if (encoded.key == key) {
            encoded.bytes.assign(bytes.begin(), bytes.end());
            return;
        }
    }
    entries.push_back({key, std::vector<std::byte>(bytes.begin(), bytes.end())});
}

std::optional<ImageViewRecord> FindImageView(VkImageView view) {
    std::lock_guard lock(recordsMutex());
    const auto found = views().find(view);
    if (found == views().end()) return std::nullopt;
    return found->second;
}

std::optional<SamplerRecord> FindSampler(VkSampler sampler) {
    std::lock_guard lock(recordsMutex());
    const auto found = samplers().find(sampler);
    if (found == samplers().end()) return std::nullopt;
    return found->second;
}

}
