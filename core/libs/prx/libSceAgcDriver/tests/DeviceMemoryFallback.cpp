#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>

using namespace AgcDriver::Graphics;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

constexpr std::uint32_t HostType = 0;
constexpr std::uint32_t DeviceType = 1;
constexpr std::uint64_t MiB = 1ull << 20u;

std::uint64_t reportedBudget = 0;
std::uint64_t reportedUsage = 0;
std::uint64_t reportedHostBudget = 0;
std::uint64_t reportedHostUsage = 0;

VKAPI_ATTR void VKAPI_CALL mockMemoryProperties2(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
    auto* reported = static_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(properties->pNext);
    reported->heapBudget[0] = reportedBudget;
    reported->heapUsage[0] = reportedUsage;
    reported->heapBudget[1] = reportedHostBudget;
    reported->heapUsage[1] = reportedHostUsage;
}

Context mockContext(bool budgetExtension) {
    Context context{};
    context.physical = reinterpret_cast<VkPhysicalDevice>(static_cast<std::uintptr_t>(1));
    context.memoryProperties2 = budgetExtension ? mockMemoryProperties2 : nullptr;
    context.memory.memoryHeapCount = 2;
    context.memory.memoryHeaps[0] = {4096 * MiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    context.memory.memoryHeaps[1] = {16384 * MiB, 0};
    context.memory.memoryTypeCount = 2;
    context.memory.memoryTypes[HostType] = {0, 1};
    context.memory.memoryTypes[DeviceType] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0};
    return context;
}

void Choose(const char* what, bool budgetExtension, std::uint64_t budget, std::uint64_t usage, std::uint32_t mask, std::uint32_t expected) {
    reportedBudget = budget;
    reportedUsage = usage;
    const auto type = mockContext(budgetExtension).DeviceMemoryType(mask, 64 * MiB, "test");
    Expect(type == expected, std::string(what) + ": chose memory type " + std::to_string(type) + ", expected " + std::to_string(expected));
}

VkDeviceSize reclaimAsked = 0;
VkDeviceSize reclaimReturns = 0;
VkDeviceSize reclaimUsageAfter = 0;
bool reclaimNewImage = false;

VkDeviceSize mockReclaim(VkDeviceSize bytes, bool newImage) {
    reclaimAsked = bytes;
    reclaimNewImage = newImage;
    reportedUsage = reclaimUsageAfter;
    return reclaimReturns;
}

void ReclaimTests() {
    const std::uint32_t both = (1u << HostType) | (1u << DeviceType);
    Context context = mockContext(true);
    context.cacheReclaim = mockReclaim;
    const auto choose = [&](const char* what, std::uint64_t budget, std::uint64_t usage, VkDeviceSize returns, std::uint64_t usageAfter, std::uint32_t expected, VkDeviceSize asked) {
        reportedBudget = budget;
        reportedUsage = usage;
        reclaimAsked = 0;
        reclaimReturns = returns;
        reclaimUsageAfter = usageAfter;
        const auto type = context.DeviceMemoryType(both, 64 * MiB, "test");
        Expect(type == expected, std::string(what) + ": chose memory type " + std::to_string(type) + ", expected " + std::to_string(expected));
        Expect(reclaimAsked == asked, std::string(what) + ": asked the caches for " + std::to_string(reclaimAsked) + " bytes, expected " + std::to_string(asked));
    };
    choose("room in the budget leaves the caches alone", 3000 * MiB, 2000 * MiB, 0, 2000 * MiB, DeviceType, 0);
    choose("caches release the whole allocation", 3000 * MiB, 2937 * MiB, 64 * MiB, 2937 * MiB, DeviceType, 64 * MiB);
    Expect(reclaimNewImage, "the budget reclaim did not ask for a new image's room");
    choose("caches release the allocation and the overage", 2952 * MiB, 3633 * MiB, 745 * MiB, 3633 * MiB, DeviceType, 745 * MiB);
    choose("a partial release that the budget then shows", 3000 * MiB, 2990 * MiB, 32 * MiB, 2900 * MiB, DeviceType, 64 * MiB);
    choose("a partial release the budget does not show", 3000 * MiB, 2990 * MiB, 32 * MiB, 2990 * MiB, HostType, 64 * MiB);
    choose("caches release nothing", 3000 * MiB, 2990 * MiB, 0, 2990 * MiB, HostType, 64 * MiB);
}

void RefusedFallbackTests() {
    const auto context = mockContext(true);
    const std::uint32_t both = (1u << HostType) | (1u << DeviceType);
    const auto refused = context.RefusedDeviceMemoryFallback(both, DeviceType);
    Expect(refused.has_value() && *refused == HostType, "a device-local type out of device memory did not fall back to the host type");
    Expect(!context.RefusedDeviceMemoryFallback(both, HostType).has_value(), "a host type out of memory was given a fallback");
    Expect(!context.RefusedDeviceMemoryFallback(1u << DeviceType, DeviceType).has_value(), "a fallback was chosen outside the allowed type mask");
    Expect(!context.RefusedDeviceMemoryFallback(both, 7).has_value(), "an unknown memory type was given a fallback");
}

void BufferPlacementTests() {
    constexpr auto coherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    Context context = mockContext(true);
    context.memory.memoryTypeCount = 4;
    context.memory.memoryTypes[0] = {coherent | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0};
    context.memory.memoryTypes[1] = {coherent, 1};
    context.memory.memoryTypes[2] = {coherent | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 0};
    context.memory.memoryTypes[3] = {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 1};
    reportedBudget = 3000 * MiB;
    reportedUsage = 2936 * MiB;
    reportedHostBudget = 8000 * MiB;
    reportedHostUsage = 2000 * MiB;
    const auto choose = [&](const char* what, std::uint32_t mask, VkMemoryPropertyFlags flags, std::uint32_t expected) {
        const auto type = context.BufferMemoryType(mask, flags, 64 * MiB);
        Expect(type == expected, std::string(what) + ": chose " + std::to_string(type) + ", expected " + std::to_string(expected));
    };
    choose("coherent buffer fits exactly in preferred cached heap", 15, coherent, 0);
    ++reportedUsage;
    choose("coherent buffer moves before allocating from a full heap", 15, coherent, 1);
    choose("buffer respects its allowed type mask", 5, coherent, 0);
    choose("another type on the same full heap is not an alternative", 7, coherent | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 0);
    choose("noncoherent alternative is not eligible", 9, coherent, 0);
    choose("explicit device-local requirement is preserved", 15, coherent | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
    choose("uncached-only coherent buffer preserves its type", 2, coherent, 1);
    reportedHostUsage = reportedHostBudget - 64 * MiB + 1;
    choose("alternative must have enough reported room", 15, coherent, 0);
    reportedHostBudget = 0;
    choose("unreported alternative budget is not assumed free", 15, coherent, 0);
    reportedHostBudget = 8000 * MiB;
    reportedHostUsage = 2000 * MiB;
    reportedBudget = 0;
    choose("unreported preferred budget retains cached preference", 15, coherent, 0);
    reportedBudget = 3000 * MiB;
    context.memoryProperties2 = nullptr;
    choose("no budget query retains cached preference", 15, coherent, 0);
    context.memoryProperties2 = mockMemoryProperties2;
    context.physical = VK_NULL_HANDLE;
    choose("no physical device retains cached preference", 15, coherent, 0);
}

}

int main() {
    try {
        const std::uint32_t both = (1u << HostType) | (1u << DeviceType);
        Choose("no VK_EXT_memory_budget", false, 0, 0, both, DeviceType);
        Choose("budget not reported", true, 0, 4000 * MiB, both, DeviceType);
        Choose("room in the device-local budget", true, 3000 * MiB, 2000 * MiB, both, DeviceType);
        Choose("allocation fills the budget exactly", true, 3000 * MiB, 2936 * MiB, both, DeviceType);
        Choose("allocation exceeds the budget", true, 3000 * MiB, 2937 * MiB, both, HostType);
        Choose("usage already over the budget", true, 2952 * MiB, 3633 * MiB, both, HostType);
        Choose("no host type allowed", true, 3000 * MiB, 3000 * MiB, 1u << DeviceType, DeviceType);
        BufferPlacementTests();
        RefusedFallbackTests();
        ReclaimTests();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("device memory placement tests passed");
    return 0;
}
