#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>

namespace AgcDriver::Graphics {
namespace {

struct DeviceAllocations {
    std::map<VkDeviceMemory, MemoryAccounting::Allocation> allocations;
    std::chrono::steady_clock::time_point reported{};
};

struct Accounting {
    std::mutex mutex;
    std::map<VkDevice, DeviceAllocations> devices;
    std::atomic<bool> complete{true};
};

Accounting& accounting() {
    static Accounting value;
    return value;
}

}

bool MemoryAccounting::Enabled() {
    static const bool enabled = std::getenv("APS5_TRACE_MEMORY_BUDGET") != nullptr;
    return enabled;
}

void MemoryAccounting::Record(VkDevice device, VkDeviceMemory memory, const VkMemoryAllocateInfo& allocation, const VkPhysicalDeviceMemoryProperties& properties, const char* category) noexcept {
    if (!Enabled() || memory == VK_NULL_HANDLE) return;
    auto& state = accounting();
    try {
        const auto heap = allocation.memoryTypeIndex < properties.memoryTypeCount ? properties.memoryTypes[allocation.memoryTypeIndex].heapIndex : VK_MAX_MEMORY_HEAPS;
        std::lock_guard lock(state.mutex);
        auto& allocations = state.devices[device].allocations;
        if (allocations.contains(memory)) state.complete = false;
        allocations.insert_or_assign(memory, Allocation{memory, allocation.allocationSize, 0, heap, category});
    } catch (...) {
        state.complete = false;
    }
}

VkResult MemoryAccounting::Allocate(const Context& context, const VkMemoryAllocateInfo& allocation, const char* category, VkDeviceMemory* memory) {
    const auto result = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, memory);
    if (result == VK_SUCCESS) Record(context.device, *memory, allocation, context.memory, category);
    return result;
}

void MemoryAccounting::Free(PFN_vkFreeMemory freeMemory, VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    if (Enabled() && memory != VK_NULL_HANDLE && !IsDeferredMemoryFree(freeMemory)) {
        auto& state = accounting();
        try {
            std::lock_guard lock(state.mutex);
            const auto found = state.devices.find(device);
            if (found != state.devices.end()) found->second.allocations.erase(memory);
        } catch (...) {
            state.complete = false;
        }
    }
    freeMemory(device, memory, allocator);
}

void MemoryAccounting::Free(const Context& context, VkDeviceMemory memory) {
    Free(context.Function<PFN_vkFreeMemory>("vkFreeMemory"), context.device, memory);
}

void MemoryAccounting::Use(VkDevice device, VkDeviceMemory memory, const char* category) noexcept {
    if (!Enabled()) return;
    auto& state = accounting();
    try {
        std::lock_guard lock(state.mutex);
        const auto found = state.devices.find(device);
        if (found == state.devices.end()) return;
        const auto allocation = found->second.allocations.find(memory);
        if (allocation == found->second.allocations.end()) return;
        allocation->second.retained = 0;
        if (category != nullptr) allocation->second.category = category;
    } catch (...) {
        state.complete = false;
    }
}

void MemoryAccounting::Retain(VkDevice device, VkDeviceMemory memory, VkDeviceSize bytes) noexcept {
    if (!Enabled()) return;
    auto& state = accounting();
    try {
        std::lock_guard lock(state.mutex);
        const auto found = state.devices.find(device);
        if (found == state.devices.end()) return;
        const auto allocation = found->second.allocations.find(memory);
        if (allocation == found->second.allocations.end()) return;
        if (bytes > allocation->second.bytes) state.complete = false;
        allocation->second.retained = std::min(bytes, allocation->second.bytes);
    } catch (...) {
        state.complete = false;
    }
}

void MemoryAccounting::ForgetDevice(VkDevice device) noexcept {
    if (!Enabled()) return;
    auto& state = accounting();
    try {
        std::lock_guard lock(state.mutex);
        state.devices.erase(device);
    } catch (...) {
        state.complete = false;
    }
}

void MemoryAccounting::RetainPart(VkDevice device, VkDeviceMemory memory, VkDeviceSize bytes, bool retained) noexcept {
    if (!Enabled()) return;
    auto& state = accounting();
    try {
        std::lock_guard lock(state.mutex);
        const auto found = state.devices.find(device);
        if (found == state.devices.end()) return;
        const auto allocation = found->second.allocations.find(memory);
        if (allocation == found->second.allocations.end()) return;
        auto& value = allocation->second;
        const auto available = retained ? value.bytes - value.retained : value.retained;
        if (bytes > available) state.complete = false;
        const auto change = std::min(bytes, available);
        if (retained) value.retained += change;
        else value.retained -= change;
    } catch (...) {
        state.complete = false;
    }
}

MemoryAccounting::Snapshot MemoryAccounting::Read(VkDevice device) {
    Snapshot result;
    if (!Enabled()) return result;
    auto& state = accounting();
    std::lock_guard lock(state.mutex);
    result.complete = state.complete;
    const auto found = state.devices.find(device);
    if (found == state.devices.end()) return result;
    result.allocations.reserve(found->second.allocations.size());
    for (const auto& [memory, allocation] : found->second.allocations) result.allocations.push_back(allocation);
    return result;
}

void MemoryAccounting::Report(const Context& context, const VkPhysicalDeviceMemoryBudgetPropertiesEXT& reported, std::uint64_t sampledBytes, std::uint64_t storageBytes, std::uint64_t storageBudget) noexcept {
    if (!Enabled()) return;
    try {
        auto& state = accounting();
        {
            std::lock_guard lock(state.mutex);
            auto& last = state.devices[context.device].reported;
            const auto now = std::chrono::steady_clock::now();
            if (last != std::chrono::steady_clock::time_point{} && now - last < std::chrono::seconds(10)) return;
            last = now;
        }
        const auto snapshot = Read(context.device);
        std::map<std::pair<std::uint32_t, std::string>, std::array<VkDeviceSize, 2>> categories;
        std::array<std::array<VkDeviceSize, 2>, VK_MAX_MEMORY_HEAPS> totals{};
        for (const auto& allocation : snapshot.allocations) {
            auto& category = categories[{allocation.heap, allocation.category}];
            category[0] += allocation.bytes - allocation.retained;
            category[1] += allocation.retained;
            if (allocation.heap >= VK_MAX_MEMORY_HEAPS) continue;
            totals[allocation.heap][0] += allocation.bytes;
            totals[allocation.heap][1] += allocation.retained;
        }
        std::optional<std::uint32_t> storageHeap;
        for (std::uint32_t heap = 0; heap < context.memory.memoryHeapCount && heap < VK_MAX_MEMORY_HEAPS; ++heap) {
            const auto allocated = totals[heap][0];
            const auto retained = totals[heap][1];
            const auto usage = reported.heapUsage[heap];
            std::fprintf(stderr, "[gpu-memory] heap=%u budget=%llu usage=%llu allocated=%llu active=%llu retained=%llu unattributed_usage=%llu tracked_above_usage=%llu complete=%u\n", heap,
                static_cast<unsigned long long>(reported.heapBudget[heap]), static_cast<unsigned long long>(usage), static_cast<unsigned long long>(allocated),
                static_cast<unsigned long long>(allocated - retained), static_cast<unsigned long long>(retained), static_cast<unsigned long long>(usage - std::min(usage, allocated)),
                static_cast<unsigned long long>(allocated - std::min(usage, allocated)), snapshot.complete ? 1u : 0u);
            if ((context.memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0 && (!storageHeap || context.memory.memoryHeaps[heap].size > context.memory.memoryHeaps[*storageHeap].size)) storageHeap = heap;
        }
        for (const auto& [key, bytes] : categories) {
            std::fprintf(stderr, "[gpu-memory] heap=%u category=\"%s\" active=%llu retained=%llu\n", key.first, key.second.c_str(), static_cast<unsigned long long>(bytes[0]), static_cast<unsigned long long>(bytes[1]));
        }
        if (storageHeap && reported.heapBudget[*storageHeap] != 0) {
            const auto budget = reported.heapBudget[*storageHeap];
            const auto usage = reported.heapUsage[*storageHeap];
            const auto reserved = usage - std::min<std::uint64_t>(usage, sampledBytes + storageBytes) + sampledBytes + budget / 8u;
            const auto available = budget > reserved ? budget - reserved : 0u;
            const auto cap = StorageTextureCeiling(context.memory, &reported);
            std::fprintf(stderr, "[gpu-memory] storage heap=%u sampled_live=%llu storage_cached=%llu reserved=%llu available=%llu cap=%llu selected=%llu\n", *storageHeap,
                static_cast<unsigned long long>(sampledBytes), static_cast<unsigned long long>(storageBytes), static_cast<unsigned long long>(reserved),
                static_cast<unsigned long long>(available), static_cast<unsigned long long>(cap), static_cast<unsigned long long>(storageBudget));
        }
    } catch (...) {
        accounting().complete = false;
        std::fprintf(stderr, "[gpu-memory] allocation report unavailable\n");
    }
}

}
