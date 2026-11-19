#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
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

std::vector<std::string> events;
VkResult idleResult = VK_SUCCESS;
bool inspectFree = false;
const VkAllocationCallbacks* lastAllocator = nullptr;

template<typename THandle>
THandle Handle(std::uintptr_t value) {
    return reinterpret_cast<THandle>(value);
}

std::string Name(const char* kind, const void* handle) {
    return std::string(kind) + std::to_string(reinterpret_cast<std::uintptr_t>(handle));
}

VKAPI_ATTR void VKAPI_CALL mockDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks*) {
    events.push_back(Name("buffer ", reinterpret_cast<const void*>(buffer)) + Name(" on ", device));
}

VKAPI_ATTR void VKAPI_CALL mockFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    lastAllocator = allocator;
    if (inspectFree) {
        const auto snapshot = MemoryAccounting::Read(device);
        Expect(snapshot.complete && std::none_of(snapshot.allocations.begin(), snapshot.allocations.end(), [&](const auto& allocation) { return allocation.memory == memory; }), "the ledger still held memory when its actual free ran");
    }
    events.push_back(Name("memory ", reinterpret_cast<const void*>(memory)) + Name(" on ", device));
}

VKAPI_ATTR VkResult VKAPI_CALL mockQueueWaitIdle(VkQueue queue) {
    events.push_back(Name("idle ", queue));
    return idleResult;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    if (std::strcmp(name, "vkDestroyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&mockDestroyBuffer);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&mockFreeMemory);
    if (std::strcmp(name, "vkQueueWaitIdle") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&mockQueueWaitIdle);
    return nullptr;
}

VKAPI_ATTR VkResult VKAPI_CALL reloadedQueueWaitIdle(VkQueue queue) {
    events.push_back(Name("reloaded idle ", queue));
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL reloadedFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    mockFreeMemory(device, memory, allocator);
    events.push_back("reloaded free");
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL reloadedProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkQueueWaitIdle") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&reloadedQueueWaitIdle);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&reloadedFreeMemory);
    return mockProc(device, name);
}

Context ContextFor(VkDevice device, PFN_vkGetDeviceProcAddr proc) {
    Context context{};
    context.device = device;
    context.queue = Handle<VkQueue>(0x900);
    context.deviceProc = proc;
    return context;
}

void DeferredUntilTheQueueIsIdle() {
    events.clear();
    const auto device = Handle<VkDevice>(0x100);
    const auto proc = DeferDestructionToIdle(device, mockProc);
    const auto context = ContextFor(device, proc);
    Expect(context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle") == &mockQueueWaitIdle, "a function other than vkDestroyBuffer and vkFreeMemory was not passed through");
    context.Function<PFN_vkFreeMemory>("vkFreeMemory")(device, Handle<VkDeviceMemory>(0x21), nullptr);
    context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device, Handle<VkBuffer>(0x11), nullptr);
    context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device, VK_NULL_HANDLE, nullptr);
    Expect(events.empty() && DeferredObjects(device) == 2, "a destroy or free was not deferred, or a null handle was queued");
    const auto start = std::chrono::steady_clock::now();
    DestroyDeferredAtIdle(context, start);
    Expect(events.empty() && DeferredObjects(device) == 2, "deferred objects younger than the drain age were destroyed, or the queue was waited for");
    DestroyDeferredAtIdle(context, start + std::chrono::seconds(1));
    const std::vector<std::string> drained{"idle 2304", "buffer 17 on 256", "memory 33 on 256"};
    Expect(events == drained && DeferredObjects(device) == 0, "old deferred objects were not destroyed after a queue wait, buffers before memory");
    events.clear();
    for (std::uintptr_t index = 0; index < 512; ++index) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device, Handle<VkBuffer>(0x1000 + index), nullptr);
    DestroyDeferredAtIdle(context, std::chrono::steady_clock::now());
    Expect(events.size() == 513 && events.front() == "idle 2304" && DeferredObjects(device) == 0, "512 young deferred objects were not destroyed after a queue wait");
    events.clear();
    idleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    context.Function<PFN_vkFreeMemory>("vkFreeMemory")(device, Handle<VkDeviceMemory>(0x22), nullptr);
    bool threw = false;
    try {
        DestroyDeferredAtIdle(context, std::chrono::steady_clock::now() + std::chrono::seconds(1));
    } catch (const std::exception&) {
        threw = true;
    }
    Expect(threw && DeferredObjects(device) == 1 && events.size() == 1, "a failed queue wait did not throw, or destroyed the deferred objects");
    idleResult = VK_SUCCESS;
    events.clear();
    DestroyDeferredObjects(device);
    Expect(events == std::vector<std::string>{"memory 34 on 256"} && DeferredObjects(device) == 0, "the deferred objects were not destroyed when the device went, or the queue was waited for");
    events.clear();
    context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device, Handle<VkBuffer>(0x12), nullptr);
    Expect(events == std::vector<std::string>{"buffer 18 on 256"}, "a destroy after the device went was deferred");
}

void OtherDevicesDestroyAtOnce() {
    events.clear();
    const auto device = Handle<VkDevice>(0x101);
    const auto proc = DeferDestructionToIdle(device, mockProc);
    const auto other = Handle<VkDevice>(0x102);
    reinterpret_cast<PFN_vkFreeMemory>(proc(other, "vkFreeMemory"))(other, Handle<VkDeviceMemory>(0x23), nullptr);
    Expect(events == std::vector<std::string>{"memory 35 on 258"} && DeferredObjects(other) == 0, "a free on a device without deferred destruction was deferred");
    bool threw = false;
    try {
        DeferDestructionToIdle(device, mockProc);
    } catch (const std::exception&) {
        threw = true;
    }
    Expect(threw, "deferred destruction was set up twice for one device");
    DestroyDeferredObjects(device);
}

PFN_vkGetDeviceProcAddr TrySetUp(VkDevice device, PFN_vkGetDeviceProcAddr resolver) {
    try {
        return DeferDestructionToIdle(device, resolver);
    } catch (const std::exception&) {
        return nullptr;
    }
}

void ALaterDeviceUsesItsOwnResolver() {
    events.clear();
    const auto first = Handle<VkDevice>(0x103);
    const auto firstProc = DeferDestructionToIdle(first, mockProc);
    DestroyDeferredObjects(first);
    const auto second = Handle<VkDevice>(0x104);
    const auto proc = TrySetUp(second, reloadedProc);
    Expect(proc != nullptr, "a device set up after the last one went could not use a resolver from a reloaded Vulkan loader");
    if (proc == nullptr) return;
    Expect(ContextFor(second, proc).Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle") == &reloadedQueueWaitIdle, "a later device did not pass through to its own resolver");
    Expect(reinterpret_cast<PFN_vkQueueWaitIdle>(firstProc(first, "vkQueueWaitIdle")) == &mockQueueWaitIdle, "an earlier device's wrapper passed through to a later device's resolver");
    Expect(TrySetUp(Handle<VkDevice>(0x105), mockProc) == nullptr, "two live devices with different resolvers were accepted");
    DestroyDeferredObjects(second);
}

void AccountingUntilFree() {
    Expect(MemoryAccounting::Enabled(), "memory accounting is not enabled");
    events.clear();
    inspectFree = true;
    const auto device = Handle<VkDevice>(0x110);
    const auto proc = DeferDestructionToIdle(device, mockProc);
    const auto context = ContextFor(device, proc);
    VkPhysicalDeviceMemoryProperties properties{};
    properties.memoryHeapCount = 2;
    properties.memoryTypeCount = 2;
    properties.memoryTypes[0].heapIndex = 0;
    properties.memoryTypes[1].heapIndex = 1;
    const auto record = [&](std::uintptr_t value, std::uint32_t type) {
        const auto memory = Handle<VkDeviceMemory>(value);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = (type + 1u) << 20u;
        allocation.memoryTypeIndex = type;
        MemoryAccounting::Record(device, memory, allocation, properties, "deferred memory");
        return memory;
    };
    const auto count = [&] {
        const auto snapshot = MemoryAccounting::Read(device);
        Expect(snapshot.complete, "deferred memory accounting became incomplete");
        return snapshot.allocations.size();
    };
    const auto first = record(0x31, 0);
    const auto second = record(0x32, 1);
    const auto third = record(0x33, 0);
    MemoryAccounting::Free(context, first);
    const auto freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
    freeMemory(device, second, nullptr);
    Expect(events.empty() && DeferredObjects(device) == 2 && count() == 3, "queued memory left the ledger before an actual free");
    idleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    bool threw = false;
    try {
        DestroyDeferredAtIdle(context, std::chrono::steady_clock::now() + std::chrono::seconds(1));
    } catch (const std::exception&) {
        threw = true;
    }
    Expect(threw && DeferredObjects(device) == 2 && count() == 3, "a failed idle wait removed queued memory from the ledger");
    idleResult = VK_ERROR_DEVICE_LOST;
    DestroyDeferredAtIdle(context, std::chrono::steady_clock::now() + std::chrono::seconds(1));
    Expect(DeferredObjects(device) == 0 && count() == 1 && MemoryAccounting::Read(device).allocations.front().memory == third, "draining deferred memory did not remove just the actual frees");
    idleResult = VK_SUCCESS;
    MemoryAccounting::Free(freeMemory, device, third);
    Expect(DeferredObjects(device) == 1 && count() == 1, "device cleanup memory was removed before its queued free");
    DestroyDeferredObjects(device);
    Expect(count() == 0 && DeferredObjects(device) == 0, "device cleanup left deferred memory counted");
    const auto fourth = record(0x34, 1);
    const VkAllocationCallbacks allocator{};
    MemoryAccounting::Free(freeMemory, device, fourth, &allocator);
    Expect(count() == 0 && DeferredObjects(device) == 0 && lastAllocator == &allocator, "a wrapper without a deferred device failed to free or forward its allocator");
    const auto fifth = record(0x35, 0);
    MemoryAccounting::Free(&mockFreeMemory, device, fifth);
    Expect(count() == 0, "a direct free left memory counted");
    const auto later = Handle<VkDevice>(0x111);
    const auto laterProc = DeferDestructionToIdle(later, reloadedProc);
    events.clear();
    const auto sixth = record(0x36, 0);
    MemoryAccounting::Free(freeMemory, device, sixth, &allocator);
    Expect(count() == 0 && lastAllocator == &allocator && events == std::vector<std::string>{"memory 54 on 272"}, "an earlier device's immediate free used a later resolver or left memory counted");
    const auto laterMemory = Handle<VkDeviceMemory>(0x37);
    VkMemoryAllocateInfo laterAllocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    laterAllocation.allocationSize = 3u << 20u;
    laterAllocation.memoryTypeIndex = 1;
    MemoryAccounting::Record(later, laterMemory, laterAllocation, properties, "later deferred memory");
    MemoryAccounting::Free(ContextFor(later, laterProc), laterMemory);
    const auto queued = MemoryAccounting::Read(later);
    Expect(DeferredObjects(later) == 1 && queued.complete && queued.allocations.size() == 1 && queued.allocations.front().bytes == laterAllocation.allocationSize && queued.allocations.front().heap == 1, "a later device's queued free changed its allocation charge");
    DestroyDeferredObjects(later);
    const auto drained = MemoryAccounting::Read(later);
    Expect(drained.complete && drained.allocations.empty() && events == std::vector<std::string>{"memory 54 on 272", "memory 55 on 273", "reloaded free"}, "a later device did not clear accounting through its own free callback");
    MemoryAccounting::ForgetDevice(later);
    MemoryAccounting::ForgetDevice(device);
    inspectFree = false;
    if (failures == 0) std::puts("deferred memory accounting preserves per-device resolvers across loader changes");
    if (failures == 0) std::puts("deferred memory accounting survives queuing and failed waits, and clears at actual frees");
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::strcmp(argv[1], "--memory-accounting") == 0) {
            AccountingUntilFree();
            return failures == 0 ? 0 : 1;
        }
        DeferredUntilTheQueueIsIdle();
        OtherDevicesDestroyAtOnce();
        ALaterDeviceUsesItsOwnResolver();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("deferred destruction tests passed");
    return 0;
}
