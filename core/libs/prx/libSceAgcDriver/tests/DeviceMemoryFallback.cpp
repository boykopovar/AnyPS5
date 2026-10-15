#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

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
constexpr std::uint32_t SecondHostType = 2;
constexpr std::uint64_t MiB = 1ull << 20u;

std::uint64_t reportedBudget = 0;
std::uint64_t reportedUsage = 0;
std::uint64_t reportedHostBudget = 0;
std::uint64_t reportedHostUsage = 0;
std::uint64_t reportedSecondHostBudget = 0;
std::uint64_t reportedSecondHostUsage = 0;
std::vector<std::uint32_t> attemptedTypes;

VKAPI_ATTR void VKAPI_CALL mockMemoryProperties2(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
    auto* reported = static_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(properties->pNext);
    reported->heapBudget[0] = reportedBudget;
    reported->heapUsage[0] = reportedUsage;
    reported->heapBudget[1] = reportedHostBudget;
    reported->heapUsage[1] = reportedHostUsage;
    reported->heapBudget[2] = reportedSecondHostBudget;
    reported->heapUsage[2] = reportedSecondHostUsage;
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    attemptedTypes.push_back(info->memoryTypeIndex);
    if (info->memoryTypeIndex == DeviceType) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *memory = reinterpret_cast<VkDeviceMemory>(static_cast<std::uintptr_t>(attemptedTypes.size()));
    return VK_SUCCESS;
}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    return std::string_view(name) == "vkAllocateMemory" ? reinterpret_cast<PFN_vkVoidFunction>(mockAllocateMemory) : nullptr;
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

void HostHeapTests() {
    const std::uint32_t both = (1u << HostType) | (1u << DeviceType);
    reportedHostBudget = 8000 * MiB;
    reportedHostUsage = 2000 * MiB;
    Choose("host heap has room", true, 4096 * MiB, 4096 * MiB, both, HostType);
    reportedHostUsage = 8000 * MiB - 64 * MiB;
    Choose("allocation fills the host heap exactly", true, 4096 * MiB, 4096 * MiB, both, HostType);
    reportedHostUsage = 8000 * MiB - 32 * MiB;
    Choose("host heap nearly full keeps the device-local type", true, 4096 * MiB, 4096 * MiB, both, DeviceType);
    reportedHostUsage = 9000 * MiB;
    Choose("host heap over its budget keeps the device-local type", true, 4096 * MiB, 4096 * MiB, both, DeviceType);
    reportedHostBudget = 0;
    reportedHostUsage = 0;
    Choose("unreported host budget is not assumed free", true, 4096 * MiB, 4096 * MiB, both, DeviceType);
}

void SecondHostHeapTests() {
    auto context = mockContext(true);
    context.memory.memoryHeapCount = 3;
    context.memory.memoryHeaps[2] = {16384 * MiB, 0};
    context.memory.memoryTypeCount = 3;
    context.memory.memoryTypes[SecondHostType] = {0, 2};
    reportedBudget = 4096 * MiB;
    reportedUsage = 4096 * MiB;
    reportedHostBudget = 8000 * MiB;
    reportedHostUsage = 8000 * MiB - 32 * MiB;
    reportedSecondHostBudget = 8000 * MiB;
    reportedSecondHostUsage = 2000 * MiB;
    const auto all = (1u << HostType) | (1u << DeviceType) | (1u << SecondHostType);
    const auto type = context.DeviceMemoryType(all, 64 * MiB, "test");
    Expect(type == SecondHostType, "a full first host heap stopped the search for a host heap with room: chose memory type " + std::to_string(type));
    const auto masked = context.DeviceMemoryType((1u << HostType) | (1u << DeviceType), 64 * MiB, "test");
    Expect(masked == DeviceType, "with the only allowed host heap full, memory type " + std::to_string(masked) + " was chosen instead of the device-local type");
    reportedSecondHostUsage = 9000 * MiB;
    const auto bothFull = context.DeviceMemoryType(all, 64 * MiB, "test");
    Expect(bothFull == DeviceType, "with every host heap full, a host type was chosen: memory type " + std::to_string(bothFull));
    reportedSecondHostBudget = 0;
    reportedSecondHostUsage = 0;
}

void RefusedWithBothHeapsFullTests() {
    const std::uint32_t both = (1u << HostType) | (1u << DeviceType);
    auto context = mockContext(true);
    context.deviceProc = mockProc;
    reportedBudget = 4096 * MiB;
    reportedUsage = 4096 * MiB;
    reportedHostBudget = 8000 * MiB;
    reportedHostUsage = 9000 * MiB;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = 64 * MiB;
    allocation.memoryTypeIndex = context.DeviceMemoryType(both, allocation.allocationSize, "test");
    VkDeviceMemory memory = VK_NULL_HANDLE;
    attemptedTypes.clear();
    const auto result = context.AllocateDeviceMemory(allocation, both, memory, "test");
    Expect(result == VK_SUCCESS && memory != VK_NULL_HANDLE && allocation.memoryTypeIndex == HostType, "with both heaps over their budgets, a device-local allocation the driver refused did not land in the host memory type");
    Expect(attemptedTypes == std::vector<std::uint32_t>{DeviceType, HostType}, "with both heaps over their budgets, the device-local type was not tried before the host type");
}

void RefusedFallbackTests() {
    const auto context = mockContext(true);
    const std::uint32_t both = (1u << HostType) | (1u << DeviceType);
    const auto refused = context.RefusedDeviceMemoryFallback(both, DeviceType, 9 * MiB, "test");
    Expect(refused.has_value() && *refused == HostType, "a device-local type out of device memory did not fall back to the host type");
    Expect(!context.RefusedDeviceMemoryFallback(both, HostType, 9 * MiB, "test").has_value(), "a host type out of memory was given a fallback");
    Expect(!context.RefusedDeviceMemoryFallback(1u << DeviceType, DeviceType, 9 * MiB, "test").has_value(), "a fallback was chosen outside the allowed type mask");
    Expect(!context.RefusedDeviceMemoryFallback(both, 7, 9 * MiB, "test").has_value(), "an unknown memory type was given a fallback");
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
        reportedHostBudget = 8000 * MiB;
        reportedHostUsage = 2000 * MiB;
        Choose("no VK_EXT_memory_budget", false, 0, 0, both, DeviceType);
        Choose("budget not reported", true, 0, 4000 * MiB, both, DeviceType);
        Choose("room in the device-local budget", true, 3000 * MiB, 2000 * MiB, both, DeviceType);
        Choose("allocation fills the budget exactly", true, 3000 * MiB, 2936 * MiB, both, DeviceType);
        Choose("allocation exceeds the budget", true, 3000 * MiB, 2937 * MiB, both, HostType);
        Choose("usage already over the budget", true, 2952 * MiB, 3633 * MiB, both, HostType);
        Choose("no host type allowed", true, 3000 * MiB, 3000 * MiB, 1u << DeviceType, DeviceType);
        HostHeapTests();
        SecondHostHeapTests();
        RefusedWithBothHeapsFullTests();
        BufferPlacementTests();
        RefusedFallbackTests();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("device memory placement tests passed");
    return 0;
}
