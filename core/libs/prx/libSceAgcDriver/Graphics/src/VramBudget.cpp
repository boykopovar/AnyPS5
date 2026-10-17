#include "prx/libSceAgcDriver/Graphics/include/VramBudget.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace AgcDriver::Graphics {

namespace {

constexpr VkDeviceSize MiB = VkDeviceSize{1} << 20u;
constexpr auto refreshInterval = std::chrono::milliseconds(100);

VkDeviceSize mibSetting(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? static_cast<VkDeviceSize>(std::strtoull(value, nullptr, 10)) * MiB : 0;
}

bool disabled() {
    static const bool off = std::getenv("APS5_NO_VRAM_BUDGET") != nullptr;
    return off;
}

std::int64_t ticks() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

}

VramBudget::VramBudget(const Context& context) : query(context.memoryProperties2), physical(context.physical) {
    if (disabled() || context.integratedGpu || query == nullptr || physical == VK_NULL_HANDLE) return;
    for (std::uint32_t index = 0; index < context.memory.memoryHeapCount && index < VK_MAX_MEMORY_HEAPS; ++index) {
        const auto& candidate = context.memory.memoryHeaps[index];
        if ((candidate.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0 && (!heap || candidate.size > heapBytes)) {
            heap = index;
            heapBytes = candidate.size;
        }
    }
    if (!heap || heapBytes == 0) {
        heap.reset();
        return;
    }
    static const VkDeviceSize forcedLimit = mibSetting("APS5_VRAM_BUDGET_MIB");
    limit = forcedLimit != 0 ? std::min(forcedLimit, heapBytes) : heapBytes;
}

VkDeviceSize VramBudget::TargetFor(VkDeviceSize size, VkDeviceSize available) {
    return std::max<VkDeviceSize>(available, std::min<VkDeviceSize>(size / 4u, 1024 * MiB));
}

VkDeviceSize VramBudget::target() const {
    const auto reported = budget.load(std::memory_order_relaxed);
    return TargetFor(heapBytes, reported != 0 ? std::min(reported, limit) : limit);
}

void VramBudget::Refresh() {
    if (!heap) return;
    std::unique_lock lock(refreshMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    released.store(0, std::memory_order_relaxed);
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    properties.pNext = &reported;
    query(physical, &properties);
    budget.store(reported.heapBudget[*heap], std::memory_order_relaxed);
    usage.store(reported.heapUsage[*heap], std::memory_order_relaxed);
    readAt.store(ticks(), std::memory_order_relaxed);
    if (announced) return;
    announced = true;
    std::fprintf(stderr, "[vram] device-local heap %u of %llu MiB: VK_EXT_memory_budget budget %llu MiB, usage %llu MiB; over %llu MiB the device pool keeps only what was taken from it in the last second\n", *heap, static_cast<unsigned long long>(heapBytes / MiB), static_cast<unsigned long long>(reported.heapBudget[*heap] / MiB), static_cast<unsigned long long>(reported.heapUsage[*heap] / MiB), static_cast<unsigned long long>(target() / MiB));
}

VkDeviceSize VramBudget::Excess() {
    if (!heap) return 0;
    const auto elapsed = std::chrono::steady_clock::duration(ticks() - readAt.load(std::memory_order_relaxed));
    if (readAt.load(std::memory_order_relaxed) == 0 || elapsed >= refreshInterval) Refresh();
    const auto reported = usage.load(std::memory_order_relaxed);
    const auto used = reported - std::min(reported, released.load(std::memory_order_relaxed));
    const auto bound = target();
    return used > bound ? used - bound : 0;
}

void VramBudget::Released(VkDeviceSize bytes) {
    if (heap) released.fetch_add(bytes, std::memory_order_relaxed);
}

VramReport VramBudget::Report() const {
    return {heap.value_or(0), heapBytes, budget.load(std::memory_order_relaxed), usage.load(std::memory_order_relaxed), target()};
}

}
