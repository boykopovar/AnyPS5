#include "HostImportFailure.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
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

constexpr std::size_t Alignment = 65536;
constexpr std::size_t RangeBytes = std::size_t{1} << 20u;

struct MockDevice {
    std::uintptr_t next = 0x1000;
    std::map<VkFence, bool> signaled;
    std::vector<VkFence> submitted;
    std::set<VkBuffer> buffers;
    std::map<VkBuffer, VkDeviceSize> sizes;
    std::set<VkDeviceMemory> memories;
    std::map<VkDeviceMemory, VkDeviceSize> importedMemories;
    std::map<VkDeviceMemory, std::vector<std::byte>> contents;
    std::uint64_t nextAddress = 0x500000000ull;
};

MockDevice mock;

bool Live(VkBuffer buffer) {
    return mock.buffers.contains(buffer);
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* commands) {
    *commands = reinterpret_cast<VkCommandBuffer>(mock.next++);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeCommandBuffers(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*) {}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    *fence = reinterpret_cast<VkFence>(mock.next++);
    mock.signaled[*fence] = false;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) {
    mock.signaled.erase(fence);
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetFenceStatus(VkDevice, VkFence fence) {
    return mock.signaled.at(fence) ? VK_SUCCESS : VK_NOT_READY;
}

VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t count, const VkFence* fences, VkBool32, std::uint64_t) {
    for (std::uint32_t i = 0; i < count; ++i) mock.signaled.at(fences[i]) = true;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t count, const VkFence* fences) {
    for (std::uint32_t i = 0; i < count; ++i) mock.signaled.at(fences[i]) = false;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence fence) {
    mock.submitted.push_back(fence);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdUpdateBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBeginQuery(VkCommandBuffer, VkQueryPool, std::uint32_t, VkQueryControlFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdEndQuery(VkCommandBuffer, VkQueryPool, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdResetQueryPool(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdCopyQueryPoolResults(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdDispatch(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t) {}

VKAPI_ATTR VkResult VKAPI_CALL mockGetMemoryHostPointerProperties(VkDevice, VkExternalMemoryHandleTypeFlagBits, const void*, VkMemoryHostPointerPropertiesEXT* properties) {
    properties->memoryTypeBits = 1;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateBuffer(VkDevice, const VkBufferCreateInfo* info, const VkAllocationCallbacks*, VkBuffer* buffer) {
    *buffer = reinterpret_cast<VkBuffer>(mock.next++);
    mock.buffers.insert(*buffer);
    mock.sizes[*buffer] = info->size;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*) {
    mock.buffers.erase(buffer);
}

VKAPI_ATTR void VKAPI_CALL mockGetBufferMemoryRequirements(VkDevice, VkBuffer buffer, VkMemoryRequirements* requirements) {
    requirements->size = mock.sizes.at(buffer);
    requirements->alignment = Alignment;
    requirements->memoryTypeBits = 1;
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    *memory = reinterpret_cast<VkDeviceMemory>(mock.next++);
    mock.memories.insert(*memory);
    mock.contents[*memory].resize(static_cast<std::size_t>(info->allocationSize));
    for (const auto* link = static_cast<const VkBaseInStructure*>(info->pNext); link != nullptr; link = link->pNext) {
        if (link->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT) mock.importedMemories[*memory] = info->allocationSize;
    }
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    mock.memories.erase(memory);
    mock.importedMemories.erase(memory);
    mock.contents.erase(memory);
}

VKAPI_ATTR VkResult VKAPI_CALL mockMapMemory(VkDevice, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize, VkMemoryMapFlags, void** data) {
    *data = mock.contents.at(memory).data() + offset;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockUnmapMemory(VkDevice, VkDeviceMemory) {}

VKAPI_ATTR VkResult VKAPI_CALL mockBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL mockGetBufferDeviceAddress(VkDevice, const VkBufferDeviceAddressInfo* info) {
    const auto address = mock.nextAddress;
    mock.nextAddress += (mock.sizes.at(info->buffer) + Alignment - 1) / Alignment * Alignment;
    return address;
}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateCommandBuffers)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockFreeCommandBuffers)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(mockCreateFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyFence)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(mockGetFenceStatus)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(mockWaitForFences)},
        {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(mockResetFences)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockBeginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockEndCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(mockQueueSubmit)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPipelineBarrier)},
        {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdUpdateBuffer)},
        {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBeginQuery)},
        {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdEndQuery)},
        {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(mockCmdResetQueryPool)},
        {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyQueryPoolResults)},
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBindPipeline)},
        {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPushConstants)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(mockCmdDispatch)},
        {"vkGetMemoryHostPointerPropertiesEXT", reinterpret_cast<PFN_vkVoidFunction>(mockGetMemoryHostPointerProperties)},
        {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCreateBuffer)},
        {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyBuffer)},
        {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferMemoryRequirements)},
        {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateMemory)},
        {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(mockFreeMemory)},
        {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(mockMapMemory)},
        {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(mockUnmapMemory)},
        {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(mockBindBufferMemory)},
        {"vkGetBufferDeviceAddressKHR", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferDeviceAddress)},
    };
    const auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}

Context mockContext() {
    static const auto device = reinterpret_cast<VkDevice>(mock.next++);
    static const auto queue = reinterpret_cast<VkQueue>(mock.next++);
    Context context{};
    context.bufferDeviceAddress = true;
    context.memory.memoryTypeCount = 1;
    context.memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    context.memory.memoryHeapCount = 1;
    context.limits.nonCoherentAtomSize = 64;
    context.limits.minStorageBufferOffsetAlignment = 16;
    context.limits.minMemoryMapAlignment = 64;
    context.limits.maxStorageBufferRange = 0xffffffffu;
    context.limits.maxMemoryAllocationCount = 4096;
    context.limits.bufferImageGranularity = 1;
    context.limits.minUniformBufferOffsetAlignment = 16;
    context.memory.memoryHeaps[0].size = std::uint64_t{1} << 34u;
    context.memory.memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    context.device = device;
    context.queue = queue;
    context.deviceProc = mockProc;
    context.hostImportAlignment = Alignment;
    return context;
}

struct Heap {
    std::byte* base = nullptr;
    bool first = false;
    bool second = false;

    Heap() {
#ifdef _WIN32
        base = static_cast<std::byte*>(VirtualAlloc(nullptr, 4 * RangeBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        base = static_cast<std::byte*>(std::aligned_alloc(Alignment, 4 * RangeBytes));
#endif
        if (base == nullptr) throw std::runtime_error("cannot allocate the test heap");
        GuestAllocations::Mutation mutation;
        mutation.Add(base, RangeBytes, true, true, true);
        first = true;
    }

    void Grow() {
        GuestAllocations::Mutation mutation;
        mutation.Add(base + RangeBytes, RangeBytes, true, true, true);
        second = true;
    }

    std::uint64_t Address(std::size_t offset = 0) const {
        return reinterpret_cast<std::uint64_t>(base) + offset;
    }

    ~Heap() {
        try {
            GuestAllocations::Mutation mutation;
            if (second) mutation.Remove(base + RangeBytes);
            if (first) mutation.Remove(base);
        } catch (...) {
        }
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        std::free(base);
#endif
    }
};

struct ActiveRecorder {
    std::unique_ptr<Recorder> recorder;

    ActiveRecorder() {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        recorder = std::make_unique<Recorder>(mockContext());
        recorder->Activate();
    }

    ~ActiveRecorder() {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        recorder.reset();
    }
};

void PromoteThroughBuild(const Context& context) {
    GuestBufferMemory promoting(context);
    promoting.AcquireRegistered();
    promoting.Upload(true);
    promoting.WriteBack();
}

void WindowPromotionOutlivesTheHold() {
    struct RestoreWindow {
        ~RestoreWindow() { SetHostImportRangeWindow(std::uint64_t{64} << 20u); }
    } restore;
    SetHostImportRangeWindow(Alignment);
    Heap heap;
    ActiveRecorder active;
    auto& recorder = *active.recorder;
    const auto context = mockContext();
    VkBuffer retired = VK_NULL_HANDLE;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto* taken = HostImportFor(context, heap.Address(), 4096);
        Expect(taken != nullptr && taken->bytes == Alignment, "the first access did not import one window");
        if (taken == nullptr) return;
        retired = taken->buffer;
        const auto* whole = HostImportFor(context, heap.Address(), RangeBytes);
        Expect(whole != nullptr && whole->base == heap.Address() && whole->bytes == RangeBytes && whole->buffer != retired, "a held import window could not be promoted to its whole registered range");
        if (whole == nullptr) return;
        Expect(Live(retired) && taken->buffer == retired && taken->bytes == Alignment, "promotion invalidated the held window or its entry");
        recorder.Commands();
        recorder.Submit();
        recorder.Sync();
        Expect(Live(retired), "a sync inside the promoting hold destroyed its old window");
        recorder.Commands();
        recorder.Submit();
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Expect(Live(retired), "the old window died while its hold's later batch was in flight");
        recorder.Sync();
    }
    Expect(!Live(retired), "the old window survived every batch of the promoting hold");
}

void IdleRetireOutlivesTheHold() {
    Heap heap;
    ActiveRecorder active;
    auto& recorder = *active.recorder;
    const auto context = mockContext();
    VkBuffer retired = VK_NULL_HANDLE;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto* taken = HostImportFor(context, heap.Address(), 4096);
        Expect(taken != nullptr, "the first range was not imported");
        if (taken == nullptr) return;
        retired = taken->buffer;
        Expect(recorder.Idle(), "the recorder is not idle before the drop");
        heap.Grow();
        PromoteThroughBuild(context);
        const auto* grown = HostImportFor(context, heap.Address(RangeBytes), 4096);
        Expect(grown != nullptr && grown->buffer != retired && grown->base == heap.Address() && grown->bytes == 2 * RangeBytes, "the next build did not import the grown run as one import replacing the first");
        Expect(Live(retired), "an import dropped with the recorder idle was destroyed at once, while the hold that took it can still record with it");
        Expect(HostImportCounters().retiredPinnedBytes == RangeBytes, "an import dropped with the recorder active but idle was not counted as pinned, although its hold keeps it");
        if (!Live(retired)) return;
        Expect(taken->buffer == retired && taken->base == heap.Address(), "the dropped import's entry changed under the hold that took it");
        recorder.Commands();
        recorder.Submit();
        recorder.Sync();
        Expect(Live(retired), "an import dropped in this hold was destroyed by a Sync inside the same hold");
        recorder.Commands();
        recorder.Submit();
        Expect(Live(retired), "an import dropped in this hold was destroyed while a batch the hold recorded afterwards is in flight");
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Expect(Live(retired), "an import dropped in a hold was destroyed at the hold's unlock while a batch of that hold is in flight");
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        recorder.Sync();
    }
    Expect(!Live(retired), "the dropped import outlived every batch of its hold");
}

void BusyRetireOutlivesALaterBatch() {
    Heap heap;
    ActiveRecorder active;
    auto& recorder = *active.recorder;
    const auto context = mockContext();
    VkBuffer retired = VK_NULL_HANDLE;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        recorder.Keep(std::make_shared<int>(0));
        const auto* taken = HostImportFor(context, heap.Address(), 4096);
        Expect(taken != nullptr, "the first range was not imported");
        if (taken == nullptr) return;
        retired = taken->buffer;
        heap.Grow();
        PromoteThroughBuild(context);
        Expect(HostImportFor(context, heap.Address(RangeBytes), 4096) != nullptr, "the next build did not import the grown run");
        mock.submitted.clear();
        recorder.Submit();
        recorder.Commands();
        recorder.Submit();
        Expect(mock.submitted.size() == 2, "the hold did not submit the keeping batch and the later one apart");
        if (mock.submitted.size() != 2) return;
        mock.signaled.at(mock.submitted[0]) = true;
        recorder.Reap();
        Expect(Live(retired), "the dropped import was destroyed when the batch open at the drop completed, while a later batch of the hold that may use it is in flight");
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Expect(Live(retired), "the dropped import was destroyed at the hold's unlock, while a later batch of the hold that may use it is in flight");
        mock.signaled.at(mock.submitted[1]) = true;
        recorder.Reap();
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        recorder.Commands();
        recorder.Sync();
    }
    Expect(!Live(retired), "the dropped import outlived every batch of its hold");
}

void CachedLookupMatchesTheFullLookup() {
    Heap heap;
    ActiveRecorder active;
    const auto context = mockContext();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    HostImportLookupCache cache;
    const auto* first = HostImportForCached(context, heap.Address(), 4096, cache);
    Expect(first != nullptr && cache.import == first, "the first cached lookup did not import and remember the range");
    if (first == nullptr) return;
    Expect(HostImportFor(context, heap.Address(), 4096) == first, "the full lookup answered another import than the cached one");
    const auto before = first->lastUse;
    Expect(HostImportForCached(context, heap.Address(), 4096, cache) == first, "a repeat lookup did not answer from the cache");
    const auto cachedUse = first->lastUse;
    static_cast<void>(HostImportFor(context, heap.Address(), 4096));
    Expect(cachedUse - before == first->lastUse - cachedUse, "a cached lookup and a full lookup stamp the last use differently");
    Expect(HostImportForCached(context, heap.Address(), 8192, cache) == HostImportFor(context, heap.Address(), 8192) && cache.bytes == 8192, "another range was answered from the cache");
    Expect(HostImportForCached(context, 0, 0, cache) == nullptr && HostImportForCached(context, heap.Address(), 0, cache) == nullptr, "an empty range was answered");
}

void CachedLookupFollowsTheImportTable() {
    Heap heap;
    ActiveRecorder active;
    const auto context = mockContext();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    HostImportLookupCache cache;
    const auto* first = HostImportForCached(context, heap.Address(), 4096, cache);
    Expect(first != nullptr, "the first range was not imported");
    if (first == nullptr) return;
    const auto firstBuffer = first->buffer;
    {
        Heap other;
        const auto* added = HostImportFor(context, other.Address(), 4096);
        Expect(added != nullptr && added != first, "an import of another range was not made");
        Expect(HostImportForCached(context, heap.Address(), 4096, cache) == HostImportFor(context, heap.Address(), 4096), "an import added between two lookups changed the cached answer");
        Expect(HostImportForCached(context, other.Address(), 4096, cache) == added, "the cache answered the range of another import");
        Expect(HostImportForCached(context, heap.Address(), 4096, cache) == first, "the cache lost the import that stayed");
    }
    HostImportLookupCache removed;
    static_cast<void>(HostImportForCached(context, heap.Address(), 4096, removed));
    heap.Grow();
    Expect(HostImportForCached(context, heap.Address(), 4096, removed) == HostImportFor(context, heap.Address(), 4096), "the cache and the full lookup disagreed while the growth was owed");
    PromoteThroughBuild(context);
    const auto* grown = HostImportForCached(context, heap.Address(), 4096, removed);
    Expect(grown != nullptr && grown == HostImportFor(context, heap.Address(), 4096) && grown->buffer != firstBuffer && grown->bytes == 2 * RangeBytes, "a dropped import was still answered from the cache");
    Expect(removed.import == grown && HostImportForCached(context, heap.Address(), 4096, removed) == grown, "the cache did not refill after the import was dropped");
    HostImportLookupCache gone;
    Expect(HostImportForCached(context, heap.Address(), 4096, gone) == grown, "a cache filled after the drop answered another import");
    Expect(HostImportForCached(context, heap.Address(2 * RangeBytes), 4096, gone) == nullptr && gone.import == nullptr, "a range outside every registered range was answered or remembered");
}

void RecentImportFollowsReplacement() {
    Heap heap;
    ActiveRecorder active;
    const auto context = mockContext();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto* first = HostImportFor(context, heap.Address(), 4096);
    Expect(first != nullptr, "the first range was not imported");
    if (first == nullptr) return;
    Expect(HostImportFor(context, heap.Address(), 4096) == first, "a repeated lookup did not find the same import");
    const auto retired = first->buffer;
    heap.Grow();
    PromoteThroughBuild(context);
    const auto* grown = HostImportFor(context, heap.Address(RangeBytes), 4096);
    Expect(grown != nullptr && grown->buffer != retired, "the next build did not import the grown run as a new import");
    if (grown == nullptr) return;
    Expect(HostImportFor(context, heap.Address(), 4096) == grown, "a lookup after the replacement served the retired import instead of the one that replaced it");
}


std::uint64_t PhysicalImportBytes() {
    std::uint64_t bytes = 0;
    for (const auto& [memory, size] : mock.importedMemories) bytes += size;
    return bytes;
}

struct RestoreBudget {
    ~RestoreBudget() { SetHostImportBudget(0); }
};

void PinnedBytesBoundTheBudget() {
    constexpr std::uint64_t budget = 8 * RangeBytes;
    const RestoreBudget restore;
    const auto context = mockContext();
    ClearHostImports(context.device);
    const auto baseline = PhysicalImportBytes();
    Heap heaps[3];
    ActiveRecorder active;
    auto& recorder = *active.recorder;
    SetHostImportBudget(budget);
    const auto promoted = [&](Heap& heap) { return ExistingHostImportFor(context, heap.Address(), 2 * RangeBytes) != nullptr; };
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Expect(HostImportCounters().retiredPinnedBytes == 0, "bytes were counted as retired and pinned before any import retired");
        for (auto& heap : heaps) Expect(HostImportFor(context, heap.Address(), 4096) != nullptr, "a first range was not imported");
        for (std::size_t index = 0; index < 2; ++index) {
            heaps[index].Grow();
            PromoteThroughBuild(context);
            Expect(HostImportFor(context, heaps[index].Address(RangeBytes), 4096) != nullptr && promoted(heaps[index]), "a grown run that fits the budget was not imported as one import");
        }
        auto counts = HostImportCounters();
        Expect(counts.retiredPinnedBytes == 2 * RangeBytes, "the replaced imports held by the recorder were not counted as retired and pinned: " + std::to_string(counts.retiredPinnedBytes));
        Expect(PhysicalImportBytes() - baseline == counts.liveBytes + counts.retiredPinnedBytes, "the live and retired bytes differ from the memory the driver holds");
        heaps[2].Grow();
        PromoteThroughBuild(context);
        static_cast<void>(HostImportFor(context, heaps[2].Address(RangeBytes), 4096));
        counts = HostImportCounters();
        Expect(!promoted(heaps[2]), "a grown run that fits the budget only when the imports it replaces and the retired imports are not counted was imported");
        Expect(counts.retiredPinnedBytes == 2 * RangeBytes && PhysicalImportBytes() - baseline == counts.liveBytes + counts.retiredPinnedBytes, "the counts differ from the memory the driver holds after the refused promotion");
        Expect(PhysicalImportBytes() - baseline <= budget, "the imports pin more than the budget");
        recorder.Commands();
        recorder.Submit();
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Expect(HostImportCounters().retiredPinnedBytes == 2 * RangeBytes, "a hold ending released the retired imports while their batch is in flight");
        PromoteThroughBuild(context);
        Expect(!promoted(heaps[2]), "a build promoted the run while the retired imports are still pinned");
        recorder.Sync();
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Expect(HostImportCounters().retiredPinnedBytes == 0, "the retired bytes were not released when the recorder completed");
        Expect(!promoted(heaps[2]), "the run was promoted before a build");
        const auto before = AddressSpaceCounters().rebuiltGrowth;
        PromoteThroughBuild(context);
        Expect(promoted(heaps[2]), "the run refused for pinned bytes was not promoted by the next build after the recorder released them, with no registry or budget change");
        Expect(AddressSpaceCounters().rebuiltGrowth == before + 1, "the release did not wake the cached address space as a growth rebuild");
        Expect(PhysicalImportBytes() - baseline <= budget, "the imports pin more than the budget after the release");
    }
    ClearHostImports(context.device);
}

void RepeatedPromotionsStayInTheBudget() {
    constexpr std::uint64_t budget = 4 * RangeBytes;
    const RestoreBudget restore;
    const auto context = mockContext();
    ClearHostImports(context.device);
    const auto baseline = PhysicalImportBytes();
    Heap heaps[3];
    ActiveRecorder active;
    auto& recorder = *active.recorder;
    SetHostImportBudget(budget);
    std::uint64_t peak = 0;
    std::uint64_t promoted = 0;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        for (auto& heap : heaps) {
            const auto* taken = HostImportFor(context, heap.Address(), 4096);
            peak = std::max(peak, PhysicalImportBytes() - baseline);
            if (taken == nullptr) continue;
            heap.Grow();
            PromoteThroughBuild(context);
            const auto* grown = HostImportFor(context, heap.Address(RangeBytes), 4096);
            if (grown != nullptr && grown->bytes == 2 * RangeBytes) ++promoted;
            peak = std::max(peak, PhysicalImportBytes() - baseline);
        }
        recorder.Commands();
        recorder.Submit();
    }
    Expect(peak <= budget, "repeated promotions with their old imports held pinned " + std::to_string(peak) + " bytes of a " + std::to_string(budget) + " byte budget");
    Expect(promoted >= 1 && promoted < 3, "the promotions did not fill the budget and then stay queued: " + std::to_string(promoted) + " of 3 promoted");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        recorder.Sync();
    }
    Expect(HostImportCounters().retiredPinnedBytes == 0, "the retired bytes were not all released");
    ClearHostImports(context.device);
}

void RetiredBytesReleaseOnce() {
    const auto context = mockContext();
    ClearHostImports(context.device);
    Heap heap;
    ActiveRecorder active;
    auto& recorder = *active.recorder;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Expect(HostImportFor(context, heap.Address(), 4096) != nullptr, "the first range was not imported");
        heap.Grow();
        PromoteThroughBuild(context);
        Expect(HostImportFor(context, heap.Address(RangeBytes), 4096) != nullptr, "the grown run was not imported");
        Expect(HostImportCounters().retiredPinnedBytes == RangeBytes, "the replaced import was not counted as retired and pinned");
        recorder.Commands();
        recorder.Submit();
    }
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        ClearHostImports(context.device);
        Expect(HostImportCounters().retiredPinnedBytes == RangeBytes && HostImportCounters().liveBytes == 0, "dropping the live imports released a retired import a batch still holds");
        recorder.Sync();
        recorder.Sync();
        recorder.Reap();
    }
    Expect(HostImportCounters().retiredPinnedBytes == 0, "a retired import was released more or fewer than once: " + std::to_string(HostImportCounters().retiredPinnedBytes) + " bytes remain");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        ClearHostImports(context.device);
        recorder.Sync();
    }
    Expect(HostImportCounters().retiredPinnedBytes == 0, "an import dropped after its holder released changed the retired bytes");
}

void ParkedImportsStayPinned() {
    const auto context = mockContext();
    ClearHostImports(context.device);
    Heap heap;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Expect(HostImportFor(context, heap.Address(), 4096) != nullptr, "the first range was not imported");
        auto elsewhere = context;
        elsewhere.device = reinterpret_cast<VkDevice>(mock.next++);
        static_cast<void>(HostImportFor(elsewhere, heap.Address(), 4096));
        Expect(HostImportCounters().retiredPinnedBytes >= RangeBytes, "an import parked by a device change was not counted as pinned");
        ClearHostImports(context.device);
    }
    Expect(HostImportCounters().retiredPinnedBytes == 0, "a parked import destroyed by ClearHostImports left its bytes counted");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_RELEASE_ON_UNLOCK", "1");
#else
    setenv("APS5_RELEASE_ON_UNLOCK", "1", 1);
    setenv("APS5_WRITE_WATCH_IMPORTS", "unwatch", 1);
#endif
    try {
        WindowPromotionOutlivesTheHold();
        IdleRetireOutlivesTheHold();
        BusyRetireOutlivesALaterBatch();
        CachedLookupMatchesTheFullLookup();
        CachedLookupFollowsTheImportTable();
        AgcDriver::Tests::RunHostImportFailureTests(mockContext(), false);
        RecentImportFollowsReplacement();
        PinnedBytesBoundTheBudget();
        RepeatedPromotionsStayInTheBudget();
        RetiredBytesReleaseOnce();
        ParkedImportsStayPinned();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures != 0) {
        std::fprintf(stderr, "%d host import retire checks failed\n", failures);
        return 1;
    }
    std::printf("host import retire tests passed\n");
    return 0;
}
