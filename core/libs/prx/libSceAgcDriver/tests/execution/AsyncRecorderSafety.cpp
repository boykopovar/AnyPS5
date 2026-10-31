#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <utility>

using namespace AgcDriver::Graphics;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

constexpr std::size_t Budget = Recorder::KeptBytesBudget;

struct MockDevice {
    std::uintptr_t next = 1;
    std::map<VkFence, bool> signaled;
    std::uint64_t submits = 0;
    std::vector<std::uint64_t> waitedValues;
    std::uint64_t ends = 0;
    bool poolFails = false;
    std::uint32_t destroyedSemaphores = 0;
    std::uint32_t mainQueries = 0;
    std::uint32_t asyncQueries = 0;
    std::map<VkCommandPool, std::uint32_t> poolFamilies;
    std::map<VkCommandBuffer, VkCommandPool> commandPools;
    VkResult timedResult = VK_TIMEOUT;
    std::array<std::uint64_t, 2> timedTargets{};
    std::uint32_t timedCount = 0;
    std::uint32_t timedCalls = 0;
    std::uint64_t fenceWaits = 0;
    std::uint64_t allocatedCommands = 0;
    std::uint64_t freedCommands = 0;
    VkResult endResult = VK_SUCCESS;
    VkResult submitResult = VK_SUCCESS;
    bool lostSubmissionPending = false;
    std::uint64_t idleWaits = 0;
    VkResult idleResult = VK_ERROR_DEVICE_LOST;
};

MockDevice mock;
std::function<void()> beforeFenceWait;

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* commands) {
    *commands = reinterpret_cast<VkCommandBuffer>(mock.next++);
    mock.commandPools[*commands] = info->commandPool;
    ++mock.allocatedCommands;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeCommandBuffers(VkDevice, VkCommandPool, std::uint32_t count, const VkCommandBuffer*) {
    Expect(!mock.lostSubmissionPending, "command buffers were freed before the lost submission finished");
    mock.freedCommands += count;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    *fence = reinterpret_cast<VkFence>(mock.next++);
    mock.signaled[*fence] = false;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) {
    Expect(!mock.lostSubmissionPending, "a fence was destroyed before the lost submission finished");
    mock.signaled.erase(fence);
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetFenceStatus(VkDevice, VkFence fence) {
    return mock.signaled.at(fence) ? VK_SUCCESS : VK_NOT_READY;
}

VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t count, const VkFence* fences, VkBool32, std::uint64_t) {
    if (auto before = std::exchange(beforeFenceWait, {})) before();
    for (std::uint32_t i = 0; i < count; ++i) mock.signaled.at(fences[i]) = true;
    ++mock.fenceWaits;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t count, const VkFence* fences) {
    Expect(!mock.lostSubmissionPending, "a fence was reset before the lost submission finished");
    for (std::uint32_t i = 0; i < count; ++i) mock.signaled.at(fences[i]) = false;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer) {
    ++mock.ends;
    return mock.endResult;
}

VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo* info, VkFence) {
    ++mock.submits;
    const auto* timeline = static_cast<const VkTimelineSemaphoreSubmitInfo*>(info->pNext);
    std::uint64_t waited = 0;
    if (timeline != nullptr) for (std::uint32_t i = 0; i < timeline->waitSemaphoreValueCount; ++i) waited = std::max(waited, timeline->pWaitSemaphoreValues[i]);
    mock.waitedValues.push_back(waited);
    if (mock.submitResult == VK_ERROR_DEVICE_LOST) mock.lostSubmissionPending = true;
    return mock.submitResult;
}

VKAPI_ATTR VkResult VKAPI_CALL mockDeviceWaitIdle(VkDevice) {
    ++mock.idleWaits;
    mock.lostSubmissionPending = false;
    return mock.idleResult;
}

VKAPI_ATTR void VKAPI_CALL mockCmdUpdateBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBeginQuery(VkCommandBuffer commands, VkQueryPool, std::uint32_t, VkQueryControlFlags) {
    if (mock.poolFamilies[mock.commandPools[commands]] == 0) ++mock.mainQueries;
    else ++mock.asyncQueries;
}
VKAPI_ATTR void VKAPI_CALL mockCmdEndQuery(VkCommandBuffer, VkQueryPool, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdResetQueryPool(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdCopyQueryPoolResults(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdDispatch(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t) {}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateSemaphore(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore* semaphore) {
    *semaphore = reinterpret_cast<VkSemaphore>(mock.next++);
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL mockDestroySemaphore(VkDevice, VkSemaphore, const VkAllocationCallbacks*) { ++mock.destroyedSemaphores; }
VKAPI_ATTR VkResult VKAPI_CALL mockCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo* info, const VkAllocationCallbacks*, VkCommandPool* pool) {
    if (mock.poolFails) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *pool = reinterpret_cast<VkCommandPool>(mock.next++);
    mock.poolFamilies[*pool] = info->queueFamilyIndex;
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL mockDestroyCommandPool(VkDevice, VkCommandPool pool, const VkAllocationCallbacks*) { mock.poolFamilies.erase(pool); }
VKAPI_ATTR VkResult VKAPI_CALL mockResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockWaitSemaphores(VkDevice, const VkSemaphoreWaitInfo* info, std::uint64_t timeout) {
    if (timeout != 1234) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); return VK_TIMEOUT; }
    ++mock.timedCalls;
    mock.timedCount = info->semaphoreCount;
    Expect(info->flags == 0, "timed wait used WAIT_ANY instead of WAIT_ALL");
    std::copy_n(info->pValues, info->semaphoreCount, mock.timedTargets.begin());
    return mock.timedResult;
}
VKAPI_ATTR VkResult VKAPI_CALL mockCounterValue(VkDevice, VkSemaphore, std::uint64_t* value) { *value = 0; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockCreateQueryPool(VkDevice, const VkQueryPoolCreateInfo*, const VkAllocationCallbacks*, VkQueryPool* pool) { *pool = reinterpret_cast<VkQueryPool>(mock.next++); return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockDestroyQueryPool(VkDevice, VkQueryPool, const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL mockQueryResults(VkDevice, VkQueryPool, std::uint32_t, std::uint32_t, std::size_t size, void* data, VkDeviceSize, VkQueryResultFlags) { std::memset(data, 0, size); return VK_SUCCESS; }

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(mockCreateSemaphore)},
        {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(mockDestroySemaphore)},
        {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(mockCreateCommandPool)},
        {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyCommandPool)},
        {"vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockResetCommandBuffer)},
        {"vkCreateQueryPool", reinterpret_cast<PFN_vkVoidFunction>(mockCreateQueryPool)},
        {"vkDestroyQueryPool", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyQueryPool)},
        {"vkWaitSemaphoresKHR", reinterpret_cast<PFN_vkVoidFunction>(mockWaitSemaphores)},
        {"vkGetSemaphoreCounterValueKHR", reinterpret_cast<PFN_vkVoidFunction>(mockCounterValue)},
        {"vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(mockQueryResults)},
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
    context.pool = reinterpret_cast<VkCommandPool>(mock.next++);
    mock.poolFamilies[context.pool] = 0;
    context.asyncQueue = reinterpret_cast<VkQueue>(mock.next++);
    context.asyncFamily = 1;
    context.asyncTimestampValidBits = 64;
    context.asyncCommands = std::make_shared<AsyncCommandBuffers>();
    return context;
}

void Refuses(const std::function<void()>& work, const char* operation) {
    const auto before = std::array{mock.submits, mock.ends, mock.fenceWaits, mock.freedCommands};
    bool refused = false;
    try { work(); } catch (const std::runtime_error&) { refused = true; }
    Expect(refused, std::string(operation) + " accepted a live borrow");
    Expect(before == std::array{mock.submits, mock.ends, mock.fenceWaits, mock.freedCommands}, std::string(operation) + " mutated the batch before refusing");
}

void BorrowScopes() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    Expect(recorder.AsyncLaneActive(), "mock async lane did not activate");
    int completed = 0;
    {
        Recorder::LaneScope lane(1);
        recorder.Commands();
        recorder.OnComplete([&] { ++completed; });
    }
    const auto commands = recorder.Commands();
    recorder.OnComplete([&] { ++completed; });
    {
        Recorder::CommandBorrow outer(&recorder, commands);
        Recorder::CommandBorrow inner(&recorder, commands);
        Expect(recorder.Commands() == commands, "nested Commands replaced the borrowed buffer");
        recorder.EndGpuTiming(recorder.BeginGpuTiming(Recorder::CommandClass::Copy));
        Refuses([&] { recorder.Submit(); }, "Submit");
        Refuses([&] { recorder.SubmitAllLanes(); }, "SubmitAllLanes");
        Refuses([&] { recorder.Sync(); }, "Sync");
        Refuses([&] { recorder.Reap(); }, "Reap");
        Refuses([&] { recorder.FinishUpTo(recorder.OpenSerial()); }, "FinishUpTo");
        Refuses([&] { recorder.PrepareWait(recorder.OpenSerial()); }, "PrepareWait");
        Refuses([&] { recorder.SplitForRun(); }, "SplitForRun");
        Expect(completed == 0, "refusal ran a completion");
        Recorder other(mockContext(), true);
        other.Commands();
        other.Sync();
    }
    recorder.Sync();
    Expect(completed == 2, "released borrow did not preserve both completions");
    recorder.RecordUncounted([&](VkCommandBuffer) { Refuses([&] { recorder.Sync(); }, "record callback Sync"); });
    recorder.Sync();
    const auto main = recorder.Commands();
    static_cast<void>(main);
    const auto split = recorder.SplitForRun();
    recorder.DeferDispatchBarrier(0x51, {}, {});
    const auto prelude = recorder.PreludeCommands();
    Expect(prelude != VK_NULL_HANDLE, "fixture has no prelude");
    {
        Recorder::CommandBorrow held(&recorder, prelude);
        Refuses([&] { recorder.Commands(); }, "seal borrowed prelude");
    }
    {
        Recorder::CommandBorrow held(&recorder, split);
        Expect(recorder.Commands() == split, "sealing an unborrowed prelude replaced the main handle");
    }
    recorder.Sync();
}

void TimedWaitsAndSamples() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    recorder.CountSamples();
    recorder.Commands();
    recorder.Submit();
    const auto main = recorder.NewestSubmitted();
    {
        Recorder::LaneScope lane(1);
        recorder.Commands();
        recorder.Submit();
    }
    const auto async = recorder.NewestSubmitted();
    Expect(async > main, "fixture did not finish on the async timeline");
    Expect(mock.mainQueries != 0 && mock.asyncQueries == 0, "occlusion queries escaped onto the compute-only lane");
    recorder.PrepareWait(async);
    const auto before = mock.fenceWaits;
    const auto calls = mock.timedCalls;
    mock.timedResult = VK_TIMEOUT;
    Expect(!recorder.WaitSerialFor(async, 1234), "timed wait ignored an unfinished async target");
    Expect(mock.timedCalls == calls + 1 && mock.timedCount == 2 && mock.timedTargets == std::array{main, async}, "timed wait did not use one complete lane target set");
    Expect(mock.fenceWaits == before, "timed timeout retired work");
    mock.timedResult = VK_SUCCESS;
    Expect(recorder.WaitSerialFor(async, 1234), "timed wait rejected completed lane targets");
    recorder.Sync();
}

void OptionalPoolFallback() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto destroyed = mock.destroyedSemaphores;
    mock.poolFails = true;
    {
        Recorder recorder(mockContext(), true);
        recorder.Activate();
        Expect(!recorder.AsyncLaneActive() && recorder.HasTimeline(), "optional pool failure disabled the main recorder");
        Expect(mock.destroyedSemaphores == destroyed + 1, "optional pool failure leaked the async semaphore");
        Recorder::LaneScope lane(1);
        for (int i = 0; i < 1000; ++i) Recorder::NoteConsumedLabel(0x1000, 4);
        recorder.Commands();
        recorder.Sync();
    }
    mock.poolFails = false;
}

void ReentrantWaitPreparation() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    int completed = 0;
    recorder.Commands();
    recorder.OnComplete([&] {
        const Recorder::LaneScope lane(1);
        recorder.Commands();
        recorder.OnComplete([&] { ++completed; });
        const auto submitted = mock.submits;
        beforeFenceWait = [submitted] { Expect(mock.submits > submitted, "a reentrant opposite-lane producer stayed open before the next fence wait"); };
    });
    recorder.Submit();
    recorder.Commands();
    recorder.Submit();
    recorder.Sync();
    Expect(completed == 1 && !beforeFenceWait, "reentrant wait preparation lost the producer completion");
}

void CompletionReturnEdge(bool label) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    bool wrote = false;
    bool completed = false;
    alignas(4) std::uint32_t destination = 0;
    const std::uint32_t value = 0x12345678;
    const auto submitted = mock.waitedValues.size();
    {
        const Recorder::LaneScope lane(1);
        recorder.Commands();
    }
    recorder.Commands();
    {
        const Recorder::LaneScope lane(1);
        {
            const Recorder::LaneSwitch mainLane(&recorder, 0);
            recorder.Commands();
            recorder.OnComplete([&] { wrote = true; });
        }
        if (label) recorder.AfterCompletions(reinterpret_cast<std::uintptr_t>(&destination), std::as_bytes(std::span(&value, 1)), 1, 0x51, false);
        else Expect(recorder.AfterRecordedWork([&] { Expect(wrote, "completion action overtook the main-lane writeback"); completed = true; }), "returned work had no completion carrier");
    }
    Expect(mock.waitedValues.size() >= submitted + 3 && mock.waitedValues[submitted + 1] == 0, "the earlier main batch acquired a retroactive async wait");
    recorder.FinishUpTo(2);
    Expect(!wrote && !completed && destination == 0, "completion escaped before the main-lane writer finished");
    recorder.FinishUpTo(3);
    Expect(wrote && !completed && destination == 0, "completion was attached to the returned main writer instead of its carrier");
    recorder.Sync();
    Expect(label ? destination == value : completed, "return-edge completion was lost");
}

void IdleCompletionEdge() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    const Recorder::LaneScope lane(1);
    {
        const Recorder::LaneSwitch mainLane(&recorder, 0);
        recorder.Commands();
    }
    recorder.Sync();
    Expect(recorder.Idle(), "idle fixture still has recorded work");
    const auto allocated = mock.allocatedCommands;
    Expect(!recorder.AfterRecordedWork([] {}), "a retired return edge created new completion work");
    Expect(recorder.Idle() && mock.allocatedCommands == allocated, "idle completion lookup allocated a batch");
}

void ConsumedCompletionEdge(bool label) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    bool wrote = false;
    bool completed = false;
    alignas(4) std::uint32_t source = 0;
    alignas(4) std::uint32_t destination = 0;
    const std::uint32_t value = 0xabcdef;
    const auto bytes = std::as_bytes(std::span(&value, 1));
    {
        const Recorder::LaneScope lane(1);
        recorder.Commands();
        recorder.Submit();
    }
    recorder.Commands();
    recorder.NoteLabel(reinterpret_cast<std::uintptr_t>(&source), bytes, 1, 0);
    recorder.OnComplete([&] { wrote = true; });
    {
        const Recorder::LaneScope lane(1);
        Recorder::NoteConsumedLabel(reinterpret_cast<std::uintptr_t>(&source), sizeof(source));
        if (label) recorder.AfterCompletions(reinterpret_cast<std::uintptr_t>(&destination), bytes, 2, 0x51, false);
        else Expect(recorder.AfterRecordedWork([&] { Expect(wrote, "consumed-label action overtook its producer"); completed = true; }), "consumed label had no completion carrier");
    }
    recorder.FinishUpTo(1);
    Expect(!wrote && !completed && destination == 0, "consumed-label completion escaped onto older async work");
    recorder.FinishUpTo(2);
    Expect(wrote && !completed && destination == 0, "consumed-label completion did not retain its carrier");
    recorder.Sync();
    Expect(label ? destination == value : completed, "consumed-label completion was lost");
}

void UnrelatedConsumedTail() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    std::uint32_t label = 0;
    const std::uint32_t value = 1;
    const auto commands = recorder.Commands();
    recorder.NoteLabel(reinterpret_cast<std::uintptr_t>(&label), std::as_bytes(std::span(&value, 1)), 1, 0);
    recorder.DeferDispatchBarrier(7, {}, {});
    int completed = 0;
    recorder.AfterRun([&](Recorder& active) {
        Expect(active.Commands() == commands, "old-run callback changed its command buffer");
        active.Keep(std::make_shared<int>(1));
        active.OnComplete([&] { ++completed; });
    });
    {
        const Recorder::LaneScope lane(1);
        Recorder::NoteConsumedLabel(reinterpret_cast<std::uintptr_t>(&label), sizeof(label));
        recorder.Commands();
    }
    recorder.Sync();
    Expect(completed == 1, "unrelated-lane note interrupted the old-run callback");
}

void ConsumedFastJoin(bool overlap, bool throwTail) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto liveCommands = mock.allocatedCommands - mock.freedCommands;
    const auto liveFences = mock.signaled.size();
    {
        Recorder recorder(mockContext(), true);
        recorder.Activate();
        std::uint32_t label = 0;
        const std::uint32_t value = 1;
        VkCommandBuffer old;
        int tails = 0;
        {
            const Recorder::LaneScope lane(1);
            old = recorder.Commands();
            recorder.DeferDispatchBarrier(7, {}, {});
            recorder.AfterRun([&](Recorder& active) {
                ++tails;
                Expect(active.Commands() == old, "future edge split an old-run callback");
                active.Keep(std::make_shared<int>(1));
                if (throwTail) throw std::runtime_error("injected old-run failure");
            });
        }
        recorder.Commands();
        recorder.NoteLabel(reinterpret_cast<std::uintptr_t>(&label), std::as_bytes(std::span(&value, 1)), 1, 0);
        {
            const Recorder::LaneScope lane(1);
            Recorder::NoteConsumedLabel(reinterpret_cast<std::uintptr_t>(&label), sizeof(label));
            const auto acquire = [&] {
                bool joined = true;
                VkCommandBuffer command = overlap ? recorder.OverlapCommands(7, {}, {}, nullptr, joined) : recorder.RunCommands(7, nullptr, {}, joined, nullptr);
                Expect(!joined && command != old, "a consumed edge survived a fast join acquisition");
                const Recorder::CommandBorrow borrow(&recorder, command);
                recorder.Keep(std::make_shared<int>(2));
            };
            if (throwTail) {
                bool failed = false;
                try { acquire(); } catch (const std::runtime_error&) { failed = true; }
                Expect(failed, "injected old-run failure did not escape acquisition");
            }
            acquire();
            recorder.Submit();
            Expect(mock.waitedValues.back() == 1, "retry lost the consumed producer dependency");
        }
        recorder.Sync();
        Expect(tails == 1, "old-run callback was duplicated");
    }
    Expect(mock.allocatedCommands - mock.freedCommands == liveCommands && mock.signaled.size() == liveFences, "failed edge acquisition leaked command or fence ownership");
}

void MutualConsumedPrefixes() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    std::array<std::uint32_t, 2> labels{};
    const std::uint32_t value = 1;
    const auto bytes = std::as_bytes(std::span(&value, 1));
    const auto before = mock.submits;
    int completed = 0;
    for (int lane : {1, 0}) {
        const Recorder::LaneScope scope(lane);
        recorder.Commands();
        recorder.NoteLabel(reinterpret_cast<std::uintptr_t>(&labels[lane]), bytes, 1, lane);
        recorder.OnComplete([&] { ++completed; });
    }
    for (int lane : {1, 0}) {
        const Recorder::LaneScope scope(lane);
        Recorder::NoteConsumedLabel(reinterpret_cast<std::uintptr_t>(&labels[1 - lane]), sizeof(value));
    }
    Expect(recorder.Commands() != VK_NULL_HANDLE, "mutual prefix consumption lost the open successor");
    Expect(mock.submits == before + 2, "mutual prefix consumption duplicated or lost a submission");
    recorder.Submit();
    Expect(mock.waitedValues.back() == 2, "mutual prefix successor did not wait for its producer");
    recorder.Sync();
    Expect(completed == 2, "mutual prefix callbacks were duplicated or lost");
}

void IndependentSubmitHolds() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    {
        const Recorder::LaneScope lane(1);
        recorder.Commands();
        recorder.HoldNextSubmit(reinterpret_cast<VkSemaphore>(std::uintptr_t{101}), 11);
    }
    recorder.Commands();
    recorder.HoldNextSubmit(reinterpret_cast<VkSemaphore>(std::uintptr_t{102}), 22);
    recorder.Submit();
    Expect(mock.waitedValues.back() == 22, "main submission took the async hold");
    {
        const Recorder::LaneScope lane(1);
        recorder.Submit();
        Expect(mock.waitedValues.back() == 11, "async submission lost its own hold");
    }
    recorder.Sync();
}

void FailedAsyncOpenCleanup() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto liveCommands = mock.allocatedCommands - mock.freedCommands;
    const auto liveFences = mock.signaled.size();
    {
        Recorder recorder(mockContext(), true);
        recorder.Activate();
        recorder.Commands();
        recorder.DeferDispatchBarrier(7, {}, {});
        recorder.AfterRun([](Recorder&) { throw std::runtime_error("injected main prefix failure"); });
        {
            const Recorder::LaneScope lane(1);
            bool failed = false;
            try { recorder.Commands(); } catch (const std::runtime_error&) { failed = true; }
            Expect(failed, "main prefix failure did not escape async opening");
            Expect(mock.allocatedCommands - mock.freedCommands == liveCommands + 1, "failed async opening lost its unowned command buffer");
            recorder.Commands();
        }
        recorder.Sync();
    }
    Expect(mock.allocatedCommands - mock.freedCommands == liveCommands && mock.signaled.size() == liveFences, "failed async opening leaked command or fence ownership");
}

void FailedConsumedProducer() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    std::uint32_t label = 0;
    const std::uint32_t value = 1;
    {
        const Recorder::LaneScope lane(1);
        recorder.Commands();
        recorder.NoteLabel(reinterpret_cast<std::uintptr_t>(&label), std::as_bytes(std::span(&value, 1)), 1, 1);
        recorder.DeferDispatchBarrier(7, {}, {});
        recorder.AfterRun([](Recorder&) { throw std::runtime_error("injected consumed producer failure"); });
    }
    recorder.Commands();
    Recorder::NoteConsumedLabel(reinterpret_cast<std::uintptr_t>(&label), sizeof(label));
    bool failed = false;
    try { recorder.Commands(); } catch (const std::runtime_error&) { failed = true; }
    Expect(failed, "consumed producer failure did not escape acquisition");
    recorder.Commands();
    recorder.Submit();
    Expect(mock.waitedValues.back() == 1, "producer failure lost the consumed note before retry");
    recorder.Sync();
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_ASYNC_LANE", "51");
#else
    setenv("APS5_ASYNC_LANE", "51", 1);
#endif
    try {
        BorrowScopes();
        TimedWaitsAndSamples();
        OptionalPoolFallback();
        ReentrantWaitPreparation();
        CompletionReturnEdge(false);
        CompletionReturnEdge(true);
        IdleCompletionEdge();
        ConsumedCompletionEdge(false);
        ConsumedCompletionEdge(true);
        UnrelatedConsumedTail();
        ConsumedFastJoin(false, false);
        ConsumedFastJoin(true, false);
        ConsumedFastJoin(false, true);
        MutualConsumedPrefixes();
        IndependentSubmitHolds();
        FailedAsyncOpenCleanup();
        FailedConsumedProducer();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return failures == 0 ? 0 : 1;
}
