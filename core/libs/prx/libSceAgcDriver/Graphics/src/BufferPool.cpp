#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace AgcDriver::Graphics {

namespace {

// APS5_BUFFER_POOL_SHARED=1: one tier for every size, as before the split.
bool sharedTiers() {
    static const bool shared = std::getenv("APS5_BUFFER_POOL_SHARED") != nullptr;
    return shared;
}

}

BufferPool::BufferPool(const Context& context) : device(context.device), unmap(context.Function<PFN_vkUnmapMemory>("vkUnmapMemory")), destroyBuffer(context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")), freeMemory(context.Function<PFN_vkFreeMemory>("vkFreeMemory")) {
    smallTier.budget = smallBudget;
    largeTier.budget = budget;
    deviceTier.budget = DeviceBudget();
}

BufferPool::~BufferPool() {
    for (const auto& slot : smallTier.free) destroy(slot.allocation);
    for (const auto& slot : largeTier.free) destroy(slot.allocation);
    for (const auto& slot : deviceTier.free) destroy(slot.allocation);
}

VkDeviceSize BufferPool::DeviceBudget() {
    static const VkDeviceSize deviceBudget = [] {
        const char* value = std::getenv("APS5_STAGING_POOL_MIB");
        return (value != nullptr ? std::strtoull(value, nullptr, 10) : 512ull) << 20u;
    }();
    return deviceBudget;
}

void BufferPool::destroy(const BufferAllocation& allocation) noexcept {
    // Device-local allocations (see DeviceBuffer) are never mapped.
    if (allocation.mapping != nullptr) unmap(device, allocation.memory);
    destroyBuffer(device, allocation.buffer, nullptr);
    freeMemory(device, allocation.memory, nullptr);
}

std::size_t BufferPool::Capacity(std::size_t bytes) {
    static const bool exact = std::getenv("APS5_NO_BUFFER_CLASSES") != nullptr;
    constexpr std::size_t smallest = 256;
    if (exact || bytes >= classLimit) return bytes;
    return std::max(smallest, std::bit_ceil(bytes));
}

BufferPool::Tier& BufferPool::tierFor(std::size_t capacity, VkMemoryPropertyFlags properties) {
    if ((properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 && (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 && DeviceBudget() != 0) return deviceTier;
    return !sharedTiers() && capacity < classLimit ? smallTier : largeTier;
}

std::optional<BufferAllocation> BufferPool::Take(std::size_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto capacity = Capacity(bytes);
    std::lock_guard lock(mutex);
    ++clock;
    if (profile) {
        static auto lastReport = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport > std::chrono::seconds(10)) {
            lastReport = now;
            std::fprintf(stderr, "[bufferpool] small: %llu hits, %llu misses, %llu evictions, %zu retained (%.0f MiB); large: %llu hits, %llu misses, %llu evictions, %zu retained (%.0f MiB)%s; device: %llu hits, %llu misses, %llu evictions, %zu retained (%.0f MiB of %llu)\n", static_cast<unsigned long long>(smallTier.hits), static_cast<unsigned long long>(smallTier.misses), static_cast<unsigned long long>(smallTier.evictions), smallTier.free.size(), smallTier.retainedBytes / 1048576.0, static_cast<unsigned long long>(largeTier.hits), static_cast<unsigned long long>(largeTier.misses), static_cast<unsigned long long>(largeTier.evictions), largeTier.free.size(), largeTier.retainedBytes / 1048576.0, sharedTiers() ? " (shared)" : "", static_cast<unsigned long long>(deviceTier.hits), static_cast<unsigned long long>(deviceTier.misses), static_cast<unsigned long long>(deviceTier.evictions), deviceTier.free.size(), deviceTier.retainedBytes / 1048576.0, static_cast<unsigned long long>(deviceTier.budget >> 20u));
        }
    }
    auto& tier = tierFor(capacity, properties);
    for (auto it = tier.free.begin(); it != tier.free.end(); ++it) {
        const auto& allocation = it->allocation;
        if (allocation.bytes != capacity || allocation.usage != usage || allocation.properties != properties) continue;
        auto result = allocation;
        tier.retainedBytes -= allocation.allocationBytes;
        *it = std::move(tier.free.back());
        tier.free.pop_back();
        ++tier.hits;
        return result;
    }
    ++tier.misses;
    return std::nullopt;
}

void BufferPool::evictOldest(Tier& tier, std::vector<BufferAllocation>& evicted) {
    const auto oldest = std::min_element(tier.free.begin(), tier.free.end(), [](const Slot& left, const Slot& right) { return left.lastUse < right.lastUse; });
    // First: a throw here leaves the slot retained and counted.
    evicted.push_back(oldest->allocation);
    tier.retainedBytes -= oldest->allocation.allocationBytes;
    *oldest = std::move(tier.free.back());
    tier.free.pop_back();
    ++tier.evictions;
}

std::size_t BufferPool::MaxSlots() {
    // APS5_BUFFER_POOL_SLOTS=<n> bounds the retained allocations; 64 is the capacity the pool had
    // before least-recently-used retention, for A/B runs.
    static const std::size_t slots = [] {
        const char* value = std::getenv("APS5_BUFFER_POOL_SLOTS");
        const auto parsed = value != nullptr ? std::strtoull(value, nullptr, 10) : 0;
        return parsed != 0 ? static_cast<std::size_t>(parsed) : defaultSlots;
    }();
    return slots;
}

void BufferPool::Put(const BufferAllocation& allocation) noexcept {
    // Evicted allocations are destroyed after the mutex is released (see evictOldest). The vector
    // may throw on growth; a Put that cannot retain simply destroys, as noexcept requires.
    std::vector<BufferAllocation> evicted;
    try {
        std::lock_guard lock(mutex);
        auto& tier = tierFor(allocation.bytes, allocation.properties);
        if (allocation.allocationBytes > tier.budget) {
            evicted.push_back(allocation);
        } else {
            const auto maxSlots = MaxSlots();
            while (!tier.free.empty() && (tier.retainedBytes + allocation.allocationBytes > tier.budget || tier.free.size() >= maxSlots)) evictOldest(tier, evicted);
            tier.free.push_back({allocation, ++clock});
            tier.retainedBytes += allocation.allocationBytes;
        }
    } catch (...) {
        destroy(allocation);
    }
    for (const auto& gone : evicted) destroy(gone);
}

std::shared_ptr<BufferPool> GetBufferPool(const Context& context) {
    if (!context.bufferPool) context.bufferPool = std::make_shared<BufferPool>(context);
    return context.bufferPool;
}

}
