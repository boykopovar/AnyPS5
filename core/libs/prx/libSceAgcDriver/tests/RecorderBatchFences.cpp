#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace AgcDriver::Graphics;

namespace {

enum class Mode { Fenceless, Fenced, NoTimeline };

constexpr std::array<Mode, 3> Modes{Mode::Fenceless, Mode::Fenced, Mode::NoTimeline};

const char* Name(Mode mode) {
    return mode == Mode::Fenceless ? "fenceless" : mode == Mode::Fenced ? "fenced timeline" : "no timeline";
}

int failures = 0;

void Expect(Mode mode, bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL (%s): %s\n", Name(mode), what.c_str());
    ++failures;
}

struct Entry {
    VkFence fence = VK_NULL_HANDLE;
    std::uint64_t signal = 0;
    bool held = false;
    bool done = false;
};

struct MockDevice {
    std::uintptr_t next = 1;
    std::thread::id owner;
    std::map<VkFence, bool> signaled;
    std::vector<Entry> queue;
    std::uint64_t timelineValue = 0;
    std::uint64_t fenceCreates = 0;
    std::uint64_t fenceStatusCalls = 0;
    std::uint64_t fenceWaits = 0;
    std::uint64_t counterReads = 0;
    std::uint64_t semaphoreWaits = 0;
    std::uint64_t submits = 0;
    std::uint64_t submitsWithFence = 0;
    std::uint64_t semaphoresCreated = 0;
    std::uint64_t semaphoresDestroyed = 0;
    std::uint64_t allocatedCommands = 0;
    std::uint64_t freedCommands = 0;
    std::uint64_t idleWaits = 0;
    VkResult submitResult = VK_SUCCESS;
    VkResult createSemaphoreResult = VK_SUCCESS;
    VkResult waitResult = VK_SUCCESS;
    VkResult pollResult = VK_SUCCESS;
    VkResult idleResult = VK_SUCCESS;
};

MockDevice mock;
std::mutex mockLock;

bool Owner() {
    return std::this_thread::get_id() == mock.owner;
}

void Finish(Entry& entry) {
    entry.done = true;
    if (entry.fence != VK_NULL_HANDLE) mock.signaled.at(entry.fence) = true;
    if (entry.signal != 0) mock.timelineValue = std::max(mock.timelineValue, entry.signal);
}

void CompleteUntil(const std::function<bool(const Entry&)>& reached) {
    for (auto& entry : mock.queue) {
        if (entry.done) continue;
        if (entry.held) return;
        Finish(entry);
        if (reached(entry)) return;
    }
}

void Complete(std::size_t count) {
    std::lock_guard lock(mockLock);
    std::size_t remaining = count;
    CompleteUntil([&](const Entry&) { return --remaining == 0; });
}

void CompleteAll() {
    Complete(std::numeric_limits<std::size_t>::max());
}

void ReleaseHeld() {
    std::lock_guard lock(mockLock);
    for (auto& entry : mock.queue) entry.held = false;
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* commands) {
    *commands = reinterpret_cast<VkCommandBuffer>(mock.next++);
    ++mock.allocatedCommands;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeCommandBuffers(VkDevice, VkCommandPool, std::uint32_t count, const VkCommandBuffer*) {
    mock.freedCommands += count;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    std::lock_guard lock(mockLock);
    *fence = reinterpret_cast<VkFence>(mock.next++);
    mock.signaled[*fence] = false;
    ++mock.fenceCreates;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) {
    std::lock_guard lock(mockLock);
    mock.signaled.erase(fence);
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetFenceStatus(VkDevice, VkFence fence) {
    std::lock_guard lock(mockLock);
    ++mock.fenceStatusCalls;
    if (mock.pollResult != VK_SUCCESS) return mock.pollResult;
    return mock.signaled.at(fence) ? VK_SUCCESS : VK_NOT_READY;
}

VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t, const VkFence* fences, VkBool32, std::uint64_t) {
    std::lock_guard lock(mockLock);
    ++mock.fenceWaits;
    if (mock.waitResult != VK_SUCCESS) return mock.waitResult;
    const auto fence = fences[0];
    if (!mock.signaled.at(fence)) CompleteUntil([&](const Entry& entry) { return entry.fence == fence; });
    return mock.signaled.at(fence) ? VK_SUCCESS : VK_TIMEOUT;
}

VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t count, const VkFence* fences) {
    std::lock_guard lock(mockLock);
    for (std::uint32_t index = 0; index < count; ++index) mock.signaled.at(fences[index]) = false;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateSemaphore(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore* semaphore) {
    std::lock_guard lock(mockLock);
    if (mock.createSemaphoreResult != VK_SUCCESS) return mock.createSemaphoreResult;
    *semaphore = reinterpret_cast<VkSemaphore>(mock.next++);
    ++mock.semaphoresCreated;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroySemaphore(VkDevice, VkSemaphore, const VkAllocationCallbacks*) {
    std::lock_guard lock(mockLock);
    ++mock.semaphoresDestroyed;
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetSemaphoreCounterValue(VkDevice, VkSemaphore, std::uint64_t* value) {
    std::lock_guard lock(mockLock);
    if (!Owner()) {
        *value = mock.timelineValue;
        return VK_SUCCESS;
    }
    ++mock.counterReads;
    if (mock.pollResult != VK_SUCCESS) return mock.pollResult;
    *value = mock.timelineValue;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockWaitSemaphores(VkDevice, const VkSemaphoreWaitInfo* info, std::uint64_t) {
    if (!Owner()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return VK_TIMEOUT;
    }
    std::lock_guard lock(mockLock);
    ++mock.semaphoreWaits;
    if (mock.waitResult != VK_SUCCESS) return mock.waitResult;
    const auto target = info->pValues[0];
    if (mock.timelineValue < target) CompleteUntil([&](const Entry& entry) { return entry.signal >= target; });
    return mock.timelineValue >= target ? VK_SUCCESS : VK_TIMEOUT;
}

VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo* submissions, VkFence fence) {
    std::lock_guard lock(mockLock);
    if (mock.submitResult != VK_SUCCESS) return mock.submitResult;
    Entry entry;
    entry.fence = fence;
    entry.held = submissions[0].waitSemaphoreCount != 0;
    if (submissions[0].signalSemaphoreCount != 0) {
        const auto* timeline = static_cast<const VkTimelineSemaphoreSubmitInfoKHR*>(submissions[0].pNext);
        entry.signal = timeline->pSignalSemaphoreValues[0];
    }
    ++mock.submits;
    if (fence != VK_NULL_HANDLE) ++mock.submitsWithFence;
    mock.queue.push_back(entry);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockDeviceWaitIdle(VkDevice) {
    ++mock.idleWaits;
    return mock.idleResult;
}

VKAPI_ATTR void VKAPI_CALL mockCmdUpdateBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBeginQuery(VkCommandBuffer, VkQueryPool, std::uint32_t, VkQueryControlFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdEndQuery(VkCommandBuffer, VkQueryPool, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdResetQueryPool(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdCopyQueryPoolResults(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdDispatch(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t) {}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateCommandBuffers)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockFreeCommandBuffers)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(mockCreateFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyFence)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(mockGetFenceStatus)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(mockWaitForFences)},
        {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(mockResetFences)},
        {"vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(mockCreateSemaphore)},
        {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(mockDestroySemaphore)},
        {"vkGetSemaphoreCounterValueKHR", reinterpret_cast<PFN_vkVoidFunction>(mockGetSemaphoreCounterValue)},
        {"vkWaitSemaphoresKHR", reinterpret_cast<PFN_vkVoidFunction>(mockWaitSemaphores)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockBeginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockEndCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(mockQueueSubmit)},
        {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(mockDeviceWaitIdle)},
        {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdUpdateBuffer)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPipelineBarrier)},
        {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBeginQuery)},
        {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdEndQuery)},
        {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(mockCmdResetQueryPool)},
        {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyQueryPoolResults)},
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBindPipeline)},
        {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPushConstants)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(mockCmdDispatch)},
    };
    const auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}

Context mockContext() {
    Context context{};
    context.device = reinterpret_cast<VkDevice>(mock.next++);
    context.queue = reinterpret_cast<VkQueue>(mock.next++);
    context.deviceProc = mockProc;
    return context;
}

void SetBatchFences(bool enabled) {
#ifdef _WIN32
    _putenv_s("APS5_BATCH_FENCES", enabled ? "1" : "");
#else
    if (enabled) setenv("APS5_BATCH_FENCES", "1", 1);
    else unsetenv("APS5_BATCH_FENCES");
#endif
}

std::array<std::byte, 0x20000> memory{};

std::uint64_t Address(int index) {
    return reinterpret_cast<std::uint64_t>(memory.data()) + 0x100u * static_cast<std::uint64_t>(index);
}

constexpr std::size_t Length = 0x100;

void Batch(Recorder& recorder, int index, bool hold = false) {
    recorder.NotePendingWrite(Address(index), Length);
    if (hold) recorder.HoldNextSubmit(reinterpret_cast<VkSemaphore>(std::uintptr_t{0xfeed}), 1);
    recorder.Submit();
}

bool Settled(const Recorder& recorder, int index) {
    return recorder.PendingWriteSettled(Address(index), Length);
}

void Begin(Mode mode) {
    mock = MockDevice{};
    mock.owner = std::this_thread::get_id();
    SetBatchFences(mode == Mode::Fenced);
}

std::unique_ptr<Recorder> Make(Mode mode) {
    auto recorder = std::make_unique<Recorder>(mockContext(), mode != Mode::NoTimeline);
    SetBatchFences(false);
    Expect(mode, recorder->HasTimeline() == (mode != Mode::NoTimeline), "the recorder's timeline does not match its configuration");
    return recorder;
}

void ExpectNoLeaks(Mode mode) {
    Expect(mode, mock.signaled.empty(), "a batch fence outlived the recorder");
    Expect(mode, mock.allocatedCommands == mock.freedCommands, "a command buffer outlived the recorder");
    Expect(mode, mock.semaphoresCreated == mock.semaphoresDestroyed, "the timeline outlived the recorder");
}

std::string Count(std::uint64_t value) {
    return std::to_string(value);
}

void Configuration(Mode mode) {
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        for (int index = 0; index < 20; ++index) {
            Batch(*recorder, index);
            if (index % 3 == 2) CompleteAll();
            recorder->Reap();
        }
        recorder->Sync();
        Expect(mode, recorder->Idle() && recorder->Submissions() == 20 && mock.submits == 20, "twenty batches were not all submitted and completed");
        if (mode == Mode::Fenceless) {
            Expect(mode, mock.fenceCreates == 0 && mock.submitsWithFence == 0, "fenceless batches made " + Count(mock.fenceCreates) + " fences and submitted " + Count(mock.submitsWithFence) + " with one");
            Expect(mode, mock.fenceStatusCalls == 0 && mock.fenceWaits == 0, "fenceless batches polled or waited on a fence");
        } else {
            Expect(mode, mock.fenceCreates != 0 && mock.submitsWithFence == mock.submits, "fenced batches were submitted without a fence of their own");
        }
    }
    ExpectNoLeaks(mode);
}

void ReapCost(Mode mode) {
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        for (int index = 0; index < 8; ++index) Batch(*recorder, index);
        Complete(4);
        mock.counterReads = mock.fenceStatusCalls = 0;
        Expect(mode, !recorder->Reap() && recorder->InFlightBatches() == 4, "a reap with four of eight batches complete did not retire exactly four");
        if (mode == Mode::Fenceless) Expect(mode, mock.counterReads == 1 && mock.fenceStatusCalls == 0, "a fenceless reap made " + Count(mock.counterReads) + " timeline reads and " + Count(mock.fenceStatusCalls) + " fence queries, not one read");
        else Expect(mode, mock.counterReads == 0 && mock.fenceStatusCalls == 5, "a fenced reap made " + Count(mock.counterReads) + " timeline reads and " + Count(mock.fenceStatusCalls) + " fence queries, not five");
        CompleteAll();
        mock.counterReads = mock.fenceStatusCalls = 0;
        Expect(mode, Settled(*recorder, 7) && Settled(*recorder, 4) && Settled(*recorder, 5) && Settled(*recorder, 7), "completed batches are not settled");
        if (mode == Mode::Fenceless) Expect(mode, mock.counterReads == 1, "the settled queries made " + Count(mock.counterReads) + " timeline reads, not one for all four");
        else Expect(mode, mock.fenceStatusCalls == 4, "the settled queries made " + Count(mock.fenceStatusCalls) + " fence queries, not four");
        mock.counterReads = mock.fenceStatusCalls = 0;
        Expect(mode, recorder->Reap() && recorder->Idle(), "a reap with every batch complete did not retire them all");
        if (mode == Mode::Fenceless) Expect(mode, mock.counterReads == 0 && mock.fenceStatusCalls == 0, "a reap after a read that covered every batch read the timeline again");
        else Expect(mode, mock.fenceStatusCalls == 4, "a fenced reap made " + Count(mock.fenceStatusCalls) + " fence queries, not four");
        mock.counterReads = mock.fenceStatusCalls = 0;
        Expect(mode, recorder->Reap() && mock.counterReads == 0 && mock.fenceStatusCalls == 0, "a reap with nothing in flight queried the GPU");
    }
    ExpectNoLeaks(mode);
}

void HeldFirstBatch(Mode mode) {
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        Batch(*recorder, 0, true);
        Batch(*recorder, 1);
        Batch(*recorder, 2);
        CompleteAll();
        Expect(mode, !recorder->Reap() && recorder->InFlightBatches() == 3, "a reap retired a batch behind a submission still waiting on its semaphore");
        Expect(mode, !Settled(*recorder, 0) && !Settled(*recorder, 1) && !Settled(*recorder, 2), "a batch behind a waiting submission counted as settled");
        Expect(mode, recorder->UnsignaledBatches() == 3, "batches behind a waiting submission were not all unsignaled");
        const auto info = recorder->DescribePendingWrite(Address(2), Length);
        Expect(mode, info.has_value() && !info->signaled && info->serial == 3, "the newest batch behind a waiting submission described as signaled");
        ReleaseHeld();
        CompleteAll();
        Expect(mode, Settled(*recorder, 0) && Settled(*recorder, 2) && recorder->UnsignaledBatches() == 0, "released batches are not settled");
        const auto done = recorder->DescribePendingWrite(Address(1), Length);
        Expect(mode, done.has_value() && done->signaled, "a completed batch described as unsignaled");
        Expect(mode, recorder->Reap() && recorder->Idle(), "a reap did not retire the released batches");
        Expect(mode, !recorder->DescribePendingWrite(Address(1), Length).has_value(), "a retired batch still describes a pending write");
    }
    ExpectNoLeaks(mode);
}

void PartialPrefix(Mode mode) {
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        Batch(*recorder, 0);
        Batch(*recorder, 1);
        Batch(*recorder, 2, true);
        Batch(*recorder, 3);
        CompleteAll();
        Expect(mode, Settled(*recorder, 0) && Settled(*recorder, 1), "the batches ahead of a waiting submission are not settled");
        Expect(mode, !Settled(*recorder, 2) && !Settled(*recorder, 3), "the waiting submission or the batch behind it counted as settled");
        const auto ahead = recorder->DescribePendingWrite(Address(1), Length);
        Expect(mode, ahead.has_value() && ahead->signaled && ahead->serial == 2, "the newest batch ahead of a waiting submission did not describe as signaled");
        const auto held = recorder->DescribePendingWrite(Address(2), Length);
        Expect(mode, held.has_value() && !held->signaled && held->serial == 3, "the waiting submission described as signaled");
        Expect(mode, !recorder->Reap() && recorder->InFlightBatches() == 2, "a reap did not stop at the waiting submission");
        Expect(mode, !Settled(*recorder, 2) && !Settled(*recorder, 3) && recorder->UnsignaledBatches() == 2, "the batches from the waiting submission on are not unsignaled");
        ReleaseHeld();
        CompleteAll();
        Expect(mode, recorder->Reap() && recorder->Idle(), "a reap did not retire the rest once the submission was released");
    }
    ExpectNoLeaks(mode);
}

void TimelineWaits(Mode mode) {
    if (mode == Mode::NoTimeline) return;
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        for (int index = 0; index < 5; ++index) Batch(*recorder, index, index == 3);
        recorder->WaitSerial(2);
        recorder->FinishUpTo(2);
        Expect(mode, recorder->InFlightBatches() == 3 && mock.semaphoreWaits == 1, "a wait for serial 2 and the completions up to it left " + Count(recorder->InFlightBatches()) + " batches in flight after " + Count(mock.semaphoreWaits) + " timeline waits");
        Expect(mode, mock.fenceWaits == (mode == Mode::Fenceless ? 0u : 2u), "the completions up to serial 2 made " + Count(mock.fenceWaits) + " fence waits");
        Expect(mode, Settled(*recorder, 0) && Settled(*recorder, 1) && !Settled(*recorder, 3), "the completions up to serial 2 changed what is settled beyond it");
        mock.semaphoreWaits = mock.fenceWaits = 0;
        recorder->SyncThrough(Address(2), Length, true);
        Expect(mode, recorder->InFlightBatches() == 2 && mock.semaphoreWaits == 1, "a sync through serial 3 left " + Count(recorder->InFlightBatches()) + " batches in flight after " + Count(mock.semaphoreWaits) + " timeline waits");
        Expect(mode, mock.fenceWaits == (mode == Mode::Fenceless ? 0u : 1u), "the sync through serial 3 made " + Count(mock.fenceWaits) + " fence waits");
        ReleaseHeld();
        recorder->Sync();
        Expect(mode, recorder->Idle(), "a sync left batches in flight");
    }
    ExpectNoLeaks(mode);
}

void LockedSyncThrough(Mode mode) {
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        for (int index = 0; index < 4; ++index) Batch(*recorder, index, index == 3);
        recorder->SyncThrough(Address(1), Length, false);
        Expect(mode, recorder->InFlightBatches() == 2, "a sync through serial 2 left " + Count(recorder->InFlightBatches()) + " batches in flight");
        Expect(mode, !Settled(*recorder, 2) && !Settled(*recorder, 3), "a sync through serial 2 completed the batches beyond it");
        CompleteAll();
        Expect(mode, Settled(*recorder, 2) && !Settled(*recorder, 3), "the batch ahead of a waiting submission is not settled after the sync");
        ReleaseHeld();
        recorder->Sync();
        Expect(mode, recorder->Idle(), "a sync left batches in flight");
    }
    ExpectNoLeaks(mode);
}

void SubmitFailure(Mode mode) {
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        Batch(*recorder, 0);
        mock.submitResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        bool failed = false;
        try {
            Batch(*recorder, 1);
        } catch (const std::exception& error) {
            failed = std::string_view(error.what()).find("vkQueueSubmit recorder") != std::string_view::npos;
        }
        Expect(mode, failed, "a failed submission did not report its Vulkan failure");
        Expect(mode, !recorder->Recording() && recorder->Submissions() == 1 && recorder->InFlightBatches() == 1 && mock.submits == 1, "a failed submission changed the batches in flight");
        mock.submitResult = VK_SUCCESS;
        Batch(*recorder, 2);
        if (mode != Mode::NoTimeline) Expect(mode, mock.queue.size() == 2 && mock.queue.back().signal == 2, "the submission after a failed one did not take the next serial");
        CompleteAll();
        Expect(mode, Settled(*recorder, 0) && Settled(*recorder, 2) && recorder->Reap() && recorder->Idle(), "the batches around a failed submission did not complete");
        if (mode == Mode::Fenceless) Expect(mode, mock.fenceCreates == 0, "a failed submission made a fence");
    }
    ExpectNoLeaks(mode);
}

void FailedWait(Mode mode, VkResult result) {
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        Batch(*recorder, 0);
        Batch(*recorder, 1);
        mock.waitResult = result;
        bool failed = false;
        try {
            recorder->Sync();
        } catch (const std::exception& error) {
            const auto operation = mode == Mode::Fenceless ? "vkWaitSemaphoresKHR recorder" : "vkWaitForFences recorder";
            failed = std::string_view(error.what()).find(operation) != std::string_view::npos;
        }
        Expect(mode, failed, "a failed wait did not report its Vulkan failure");
        Expect(mode, recorder->InFlightBatches() == 1 && !recorder->Recording(), "a failed wait left " + Count(recorder->InFlightBatches()) + " batches in flight, not the one it did not reach");
        Expect(mode, mock.idleWaits == (result == VK_ERROR_DEVICE_LOST ? 0u : 1u), "a failed wait settled the device " + Count(mock.idleWaits) + " times");
        Expect(mode, !Settled(*recorder, 1), "a batch the GPU never completed counted as settled after a failed wait");
        mock.waitResult = VK_SUCCESS;
        recorder->Sync();
        Expect(mode, recorder->Idle(), "a sync after a failed wait left batches in flight");
    }
    ExpectNoLeaks(mode);
}

void FailedPoll(Mode mode) {
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        Batch(*recorder, 0);
        Batch(*recorder, 1);
        CompleteAll();
        mock.pollResult = VK_ERROR_DEVICE_LOST;
        Expect(mode, !recorder->Reap() && recorder->InFlightBatches() == 2, "a reap retired batches whose completion could not be read");
        Expect(mode, !Settled(*recorder, 0) && !Settled(*recorder, 1) && recorder->UnsignaledBatches() == 2, "batches whose completion could not be read counted as settled");
        mock.pollResult = VK_SUCCESS;
        Expect(mode, Settled(*recorder, 1) && recorder->Reap() && recorder->Idle(), "the batches were not retired once their completion could be read");
    }
    ExpectNoLeaks(mode);
}

void TimelineCreationFails() {
    const auto mode = Mode::NoTimeline;
    Begin(mode);
    mock.createSemaphoreResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        Recorder recorder(mockContext(), true);
        Expect(mode, !recorder.HasTimeline(), "a recorder whose timeline failed to create still reports one");
        for (int index = 0; index < 3; ++index) Batch(recorder, index);
        Expect(mode, mock.fenceCreates != 0 && mock.submitsWithFence == mock.submits, "batches without a timeline were submitted without a fence");
        Complete(2);
        Expect(mode, !recorder.Reap() && recorder.InFlightBatches() == 1, "a reap without a timeline did not retire the two completed batches");
        recorder.Sync();
        Expect(mode, recorder.Idle(), "a sync without a timeline left batches in flight");
    }
    ExpectNoLeaks(mode);
}

std::vector<std::string> Walk(Mode mode) {
    std::vector<std::string> trace;
    Begin(mode);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    {
        const auto recorder = Make(mode);
        std::uint32_t state = 20260101;
        const auto next = [&](std::uint32_t bound) {
            state = state * 1664525u + 1013904223u;
            return (state >> 16) % bound;
        };
        int submitted = 0;
        for (int step = 0; step < 400; ++step) {
            std::string line;
            switch (next(8)) {
            case 0:
            case 1:
            case 2:
                if (submitted < 120) Batch(*recorder, submitted++, next(5) == 0);
                line = "submit";
                break;
            case 3:
            case 4:
                Complete(1 + next(3));
                line = "complete";
                break;
            case 5:
                ReleaseHeld();
                line = "release";
                break;
            case 6:
                line = std::string("reap ") + (recorder->Reap() ? "idle" : "busy");
                break;
            default:
                if (next(4) == 0) {
                    ReleaseHeld();
                    recorder->Sync();
                    line = "sync";
                } else {
                    line = "skip";
                }
                break;
            }
            line += " flight=" + std::to_string(recorder->InFlightBatches()) + " unsignaled=" + std::to_string(recorder->UnsignaledBatches()) + " settled=";
            for (int index = std::max(0, submitted - 12); index < submitted; ++index) line += Settled(*recorder, index) ? '1' : '0';
            trace.push_back(line);
        }
        ReleaseHeld();
        recorder->Sync();
        Expect(mode, recorder->Idle(), "the walk left batches in flight");
    }
    ExpectNoLeaks(mode);
    return trace;
}

void WalksAgree() {
    const auto reference = Walk(Mode::NoTimeline);
    for (const auto mode : {Mode::Fenceless, Mode::Fenced}) {
        const auto trace = Walk(mode);
        std::size_t step = 0;
        while (step < trace.size() && step < reference.size() && trace[step] == reference[step]) ++step;
        Expect(mode, step == reference.size() && trace.size() == reference.size(), "the walk of random submissions and completions diverged from the fence-only recorder at step " + std::to_string(step) + (step < trace.size() ? ": " + trace[step] + " against " + (step < reference.size() ? reference[step] : std::string("nothing")) : std::string()));
    }
}

}

int main() {
    try {
        for (const auto mode : Modes) {
            Configuration(mode);
            ReapCost(mode);
            HeldFirstBatch(mode);
            PartialPrefix(mode);
            TimelineWaits(mode);
            LockedSyncThrough(mode);
            SubmitFailure(mode);
            FailedWait(mode, VK_ERROR_DEVICE_LOST);
            FailedWait(mode, VK_ERROR_OUT_OF_HOST_MEMORY);
            FailedPoll(mode);
        }
        TimelineCreationFails();
        WalksAgree();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures != 0) {
        std::fprintf(stderr, "%d recorder batch fence checks failed\n", failures);
        return 1;
    }
    std::printf("recorder batch fence tests passed\n");
    return 0;
}
