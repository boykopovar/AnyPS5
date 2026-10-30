#include "prx/libSceAgcDriver/Graphics/include/LiveAllocations.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace AgcDriver::Graphics;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

std::uintptr_t next = 0x1000;
std::vector<VkDeviceMemory> freed;
VkResult allocateResult = VK_SUCCESS;

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    if (allocateResult != VK_SUCCESS) return allocateResult;
    *memory = reinterpret_cast<VkDeviceMemory>(next);
    next += 0x10;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    freed.push_back(memory);
}

VKAPI_ATTR void VKAPI_CALL mockDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*) {
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&mockAllocateMemory);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&mockFreeMemory);
    if (std::strcmp(name, "vkDestroyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&mockDestroyBuffer);
    return nullptr;
}

VkDeviceMemory allocate(PFN_vkGetDeviceProcAddr proc, VkDevice device, std::uint32_t type, VkDeviceSize bytes) {
    VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    info.allocationSize = bytes;
    info.memoryTypeIndex = type;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    const auto result = reinterpret_cast<PFN_vkAllocateMemory>(proc(device, "vkAllocateMemory"))(device, &info, nullptr, &memory);
    return result == VK_SUCCESS ? memory : VK_NULL_HANDLE;
}

}

int main() {
    const auto device = reinterpret_cast<VkDevice>(std::uintptr_t{0x77});
    const auto proc = CountAllocations(device, &mockProc, 2);
    Expect(proc(device, "vkDestroyBuffer") == reinterpret_cast<PFN_vkVoidFunction>(&mockDestroyBuffer), "an uncounted function was not forwarded to the resolver");
    const auto first = allocate(proc, device, 1, 3u << 20u);
    VkDeviceMemory second = VK_NULL_HANDLE;
    {
        const AllocationLabel outer("staging");
        {
            const AllocationLabel inner("texture");
            second = allocate(proc, device, 2, 1u << 20u);
        }
        static_cast<void>(allocate(proc, device, 2, 2u << 20u));
    }
    const auto third = allocate(proc, device, 1, 5u << 20u);
    auto live = CountedAllocations(device);
    Expect(live.count == 4 && live.peak == 4, "four allocations were not counted live");
    Expect(live.countByType[1] == 2 && live.bytesByType[1] == (8u << 20u), "memory type 1 did not hold two allocations of 8 MiB");
    Expect(live.countByType[2] == 2 && live.bytesByType[2] == (3u << 20u), "memory type 2 did not hold two allocations of 3 MiB");
    allocateResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    Expect(allocate(proc, device, 1, 1u << 20u) == VK_NULL_HANDLE && CountedAllocations(device).count == 4, "a failed allocation was counted");
    allocateResult = VK_SUCCESS;
    reinterpret_cast<PFN_vkFreeMemory>(proc(device, "vkFreeMemory"))(device, first, nullptr);
    reinterpret_cast<PFN_vkFreeMemory>(proc(device, "vkFreeMemory"))(device, VK_NULL_HANDLE, nullptr);
    live = CountedAllocations(device);
    Expect(freed.size() == 2 && freed[0] == first, "a free was not forwarded to the resolver");
    Expect(live.count == 3 && live.peak == 4, "a freed allocation stayed live or lowered the peak");
    Expect(live.countByType[1] == 1 && live.bytesByType[1] == (5u << 20u), "the freed allocation was not removed from its memory type");
    VkPhysicalDeviceMemoryProperties memory{};
    memory.memoryTypeCount = 3;
    memory.memoryTypes[1] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0};
    memory.memoryTypes[2] = {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1};
    const auto lines = LiveAllocationReport(live, memory, 2);
    const std::vector<std::string> expected{
        "[device-fault] live device memory: 3 allocations (peak 4, maxMemoryAllocationCount 2)",
        "[device-fault] memory type 1 (heap 0, flags 0x1): 1 allocations, 5 MiB",
        "[device-fault] memory type 1 by size: <16MiB 1/5 MiB",
        "[device-fault] memory type 1 by caller: unlabelled 1/5 MiB",
        "[device-fault] memory type 2 (heap 1, flags 0x6): 2 allocations, 3 MiB",
        "[device-fault] memory type 2 by size: <16MiB 2/3 MiB",
        "[device-fault] memory type 2 by caller: staging 1/2 MiB; texture 1/1 MiB",
    };
    Expect(lines.size() == expected.size(), "the report did not list the total and each used memory type with its sizes and callers");
    for (std::size_t index = 0; index < lines.size() && index < expected.size(); ++index) Expect(lines[index] == expected[index], "report line " + std::to_string(index) + " changed: " + lines[index]);
    reinterpret_cast<PFN_vkFreeMemory>(proc(device, "vkFreeMemory"))(device, second, nullptr);
    const auto relabelled = CountedAllocations(device);
    Expect(relabelled.labels.size() == 2 && relabelled.labels[0].label == "unlabelled" && relabelled.labels[1].label == "staging", "a freed caller kept its tally");
    Expect(SizeClass(0) == 0 && SizeClass((1u << 20u) - 1u) == 0 && SizeClass(1u << 20u) == 1 && SizeClass(512ull << 20u) == 4 && SizeClass(~0ull) == 4, "the size classes changed");
    Expect(CountedAllocations(reinterpret_cast<VkDevice>(std::uintptr_t{0x78})).count == 0, "an uncounted device reported allocations");
    static_cast<void>(third);
    if (failures != 0) return 1;
    std::puts("live allocation tests passed");
    return 0;
}
