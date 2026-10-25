#include "prx/libSceAgcDriver/Graphics/include/VramBudget.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace AgcDriver::Graphics {

namespace {

constexpr VkDeviceSize MiB = VkDeviceSize{1} << 20u;

VkDeviceSize mibSetting(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? static_cast<VkDeviceSize>(std::strtoull(value, nullptr, 10)) * MiB : 0;
}

bool disabled() {
    static const bool off = std::getenv("APS5_NO_VRAM_BUDGET") != nullptr;
    return off;
}

}

VramBudget::VramBudget(const Context& context) {
    if (disabled() || context.integratedGpu || context.memoryProperties2 == nullptr || context.physical == VK_NULL_HANDLE) return;
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
    return TargetFor(heapBytes, budget != 0 ? std::min(budget, limit) : limit);
}

void VramBudget::Read(const VkPhysicalDeviceMemoryBudgetPropertiesEXT& reported) {
    if (!heap) return;
    released = 0;
    budget = reported.heapBudget[*heap];
    usage = reported.heapUsage[*heap];
    if (announced) return;
    announced = true;
    std::fprintf(stderr, "[vram] device-local heap %u of %llu MiB: VK_EXT_memory_budget budget %llu MiB, usage %llu MiB; over %llu MiB the device pool keeps only what was taken from it in the last second\n", *heap, static_cast<unsigned long long>(heapBytes / MiB), static_cast<unsigned long long>(budget / MiB), static_cast<unsigned long long>(usage / MiB), static_cast<unsigned long long>(target() / MiB));
}

VkDeviceSize VramBudget::Excess() const {
    if (!heap) return 0;
    const auto used = usage - std::min(usage, released);
    const auto bound = target();
    return used > bound ? used - bound : 0;
}

void VramBudget::Released(VkDeviceSize bytes) {
    if (heap) released += bytes;
}

VramReport VramBudget::Report() const {
    return {heap.value_or(0), heapBytes, budget, usage, target()};
}

}
