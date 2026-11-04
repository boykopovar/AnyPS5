#include "HostImportFailure.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
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

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    *memory = reinterpret_cast<VkDeviceMemory>(mock.next++);
    mock.memories.insert(*memory);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    mock.memories.erase(memory);
}

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
        base = static_cast<std::byte*>(VirtualAlloc(nullptr, 2 * RangeBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        base = static_cast<std::byte*>(std::aligned_alloc(Alignment, 2 * RangeBytes));
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
        const auto* grown = HostImportFor(context, heap.Address(RangeBytes), 4096);
        Expect(grown != nullptr && grown->buffer != retired && grown->base == heap.Address() && grown->bytes == 2 * RangeBytes, "the grown run was not imported as one import replacing the first");
        Expect(Live(retired), "an import dropped with the recorder idle was destroyed at once, while the hold that took it can still record with it");
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
        Expect(HostImportFor(context, heap.Address(RangeBytes), 4096) != nullptr, "the grown run was not imported");
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
    const auto* grown = HostImportFor(context, heap.Address(RangeBytes), 4096);
    Expect(grown != nullptr && grown->buffer != retired, "the grown run was not imported as a new import");
    if (grown == nullptr) return;
    Expect(HostImportFor(context, heap.Address(), 4096) == grown, "a lookup after the replacement served the retired import instead of the one that replaced it");
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
        AgcDriver::Tests::RunHostImportFailureTests(mockContext(), false);
        RecentImportFollowsReplacement();
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
