#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include "prx/libSceAgcDriver/Graphics/include/QueueSubmitter.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace AgcDriver::Graphics;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

enum class BufferState { Initial, Recording, Executable };

struct Submission {
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    std::vector<std::string> recorded;
    std::set<std::thread::id> recordingThreads;
    std::thread::id thread;
};

struct MockDevice {
    std::mutex mutex;
    std::condition_variable changed;
    std::uintptr_t next = 0x10000;
    VkCommandPool contextPool = VK_NULL_HANDLE;
    std::set<VkCommandPool> pools;
    std::uint64_t poolsCreated = 0;
    std::uint64_t poolsDestroyed = 0;
    std::map<VkCommandBuffer, VkCommandPool> owners;
    std::map<VkCommandBuffer, BufferState> states;
    std::map<VkCommandBuffer, std::vector<std::string>> recorded;
    std::map<VkCommandBuffer, std::set<std::thread::id>> recordingThreads;
    std::uint64_t allocated = 0;
    std::uint64_t freed = 0;
    std::map<VkFence, bool> signaled;
    std::vector<Submission> submissions;
    std::vector<std::string> violations;
    bool gate = false;
    bool gateOpen = false;
    VkResult encoderEndResult = VK_SUCCESS;

    std::uintptr_t Handle() {
        next += 0x10;
        return next;
    }

    void Violation(const std::string& what) { violations.push_back(what); }
};

MockDevice* mock = nullptr;

template<typename T>
T handleOf(std::uintptr_t value) {
    return reinterpret_cast<T>(value);
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo* info, const VkAllocationCallbacks*, VkCommandPool* pool) {
    std::lock_guard lock(mock->mutex);
    if ((info->flags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) == 0) mock->Violation("an encoder pool cannot reset its command buffers one by one");
    *pool = handleOf<VkCommandPool>(mock->Handle());
    mock->pools.insert(*pool);
    ++mock->poolsCreated;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyCommandPool(VkDevice, VkCommandPool pool, const VkAllocationCallbacks*) {
    std::lock_guard lock(mock->mutex);
    if (pool == mock->contextPool) mock->Violation("the device's command pool was destroyed by the recorder");
    if (mock->pools.erase(pool) == 0) mock->Violation("an unknown command pool was destroyed");
    ++mock->poolsDestroyed;
    for (auto it = mock->owners.begin(); it != mock->owners.end();) {
        if (it->second != pool) {
            ++it;
            continue;
        }
        ++mock->freed;
        mock->states.erase(it->first);
        it = mock->owners.erase(it);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* commands) {
    std::lock_guard lock(mock->mutex);
    if (mock->pools.count(info->commandPool) == 0) mock->Violation("a command buffer was allocated from an unknown pool");
    *commands = handleOf<VkCommandBuffer>(mock->Handle());
    mock->owners[*commands] = info->commandPool;
    mock->states[*commands] = BufferState::Initial;
    ++mock->allocated;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeCommandBuffers(VkDevice, VkCommandPool pool, std::uint32_t count, const VkCommandBuffer* commands) {
    std::lock_guard lock(mock->mutex);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (CommandStream::IsHandle(commands[index])) {
            mock->Violation("a stream handle was freed as a command buffer");
            continue;
        }
        const auto found = mock->owners.find(commands[index]);
        if (found == mock->owners.end()) {
            mock->Violation("an unknown command buffer was freed");
            continue;
        }
        if (found->second != pool) mock->Violation("a command buffer was freed into a pool that did not allocate it");
        mock->owners.erase(found);
        mock->states.erase(commands[index]);
        ++mock->freed;
    }
}

VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer commands, const VkCommandBufferBeginInfo*) {
    std::unique_lock lock(mock->mutex);
    if (CommandStream::IsHandle(commands)) {
        mock->Violation("vkBeginCommandBuffer was given a stream handle");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const auto owner = mock->owners.find(commands);
    if (owner == mock->owners.end()) {
        mock->Violation("an unknown command buffer was begun");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (mock->gate && owner->second != mock->contextPool) mock->changed.wait(lock, [] { return mock->gateOpen; });
    mock->states[commands] = BufferState::Recording;
    mock->recorded[commands].clear();
    mock->recordingThreads[commands].clear();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer commands) {
    std::lock_guard lock(mock->mutex);
    if (CommandStream::IsHandle(commands)) {
        mock->Violation("vkEndCommandBuffer was given a stream handle");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (mock->states[commands] != BufferState::Recording) mock->Violation("a command buffer was ended outside recording");
    const auto owner = mock->owners.find(commands);
    if (owner != mock->owners.end() && owner->second != mock->contextPool && mock->encoderEndResult != VK_SUCCESS) return mock->encoderEndResult;
    mock->states[commands] = BufferState::Executable;
    return VK_SUCCESS;
}

void record(VkCommandBuffer commands, const std::string& op) {
    std::lock_guard lock(mock->mutex);
    if (CommandStream::IsHandle(commands)) {
        mock->Violation(op + " reached the driver with a stream handle");
        return;
    }
    if (mock->states[commands] != BufferState::Recording) mock->Violation(op + " was recorded into a command buffer that was not recording");
    mock->recorded[commands].push_back(op);
    mock->recordingThreads[commands].insert(std::this_thread::get_id());
}

VKAPI_ATTR void VKAPI_CALL mockCmdFillBuffer(VkCommandBuffer commands, VkBuffer, VkDeviceSize, VkDeviceSize, std::uint32_t value) {
    record(commands, "fill " + std::to_string(value));
}

VKAPI_ATTR void VKAPI_CALL mockCmdUpdateBuffer(VkCommandBuffer commands, VkBuffer, VkDeviceSize, VkDeviceSize size, const void* data) {
    std::uint32_t value = 0;
    if (size >= sizeof(value)) std::memcpy(&value, data, sizeof(value));
    record(commands, "update " + std::to_string(value));
}

VKAPI_ATTR void VKAPI_CALL mockCmdDispatch(VkCommandBuffer commands, std::uint32_t x, std::uint32_t, std::uint32_t) {
    record(commands, "dispatch " + std::to_string(x));
}

VKAPI_ATTR void VKAPI_CALL mockCmdPipelineBarrier(VkCommandBuffer commands, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {
    record(commands, "barrier");
}

VKAPI_ATTR void VKAPI_CALL mockCmdBeginQuery(VkCommandBuffer commands, VkQueryPool, std::uint32_t, VkQueryControlFlags) {
    record(commands, "begin query");
}

VKAPI_ATTR void VKAPI_CALL mockCmdEndQuery(VkCommandBuffer commands, VkQueryPool, std::uint32_t) {
    record(commands, "end query");
}

VKAPI_ATTR void VKAPI_CALL mockCmdResetQueryPool(VkCommandBuffer commands, VkQueryPool, std::uint32_t, std::uint32_t) {
    record(commands, "reset queries");
}

VKAPI_ATTR void VKAPI_CALL mockCmdCopyQueryPoolResults(VkCommandBuffer commands, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) {
    record(commands, "copy queries");
}

VKAPI_ATTR void VKAPI_CALL mockCmdBindPipeline(VkCommandBuffer commands, VkPipelineBindPoint, VkPipeline) {
    record(commands, "bind pipeline");
}

VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t count, const VkSubmitInfo* submits, VkFence fence) {
    std::lock_guard lock(mock->mutex);
    for (std::uint32_t submit = 0; submit < count; ++submit) {
        Submission submission;
        submission.fence = fence;
        submission.thread = std::this_thread::get_id();
        if (submits[submit].commandBufferCount > 1) mock->Violation("a batch submitted more than one command buffer");
        if (submits[submit].commandBufferCount == 1) {
            const auto commands = submits[submit].pCommandBuffers[0];
            submission.commands = commands;
            if (CommandStream::IsHandle(commands)) mock->Violation("a stream handle was submitted");
            const auto owner = mock->owners.find(commands);
            submission.pool = owner != mock->owners.end() ? owner->second : VK_NULL_HANDLE;
            if (mock->states[commands] != BufferState::Executable) mock->Violation("a command buffer was submitted before it was ended");
            submission.recorded = mock->recorded[commands];
            submission.recordingThreads = mock->recordingThreads[commands];
        }
        mock->submissions.push_back(std::move(submission));
    }
    if (fence != VK_NULL_HANDLE) mock->signaled[fence] = true;
    mock->changed.notify_all();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    std::lock_guard lock(mock->mutex);
    *fence = handleOf<VkFence>(mock->Handle());
    mock->signaled[*fence] = false;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) {
    std::lock_guard lock(mock->mutex);
    mock->signaled.erase(fence);
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetFenceStatus(VkDevice, VkFence fence) {
    std::lock_guard lock(mock->mutex);
    return mock->signaled[fence] ? VK_SUCCESS : VK_NOT_READY;
}

VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t count, const VkFence* fences, VkBool32, std::uint64_t) {
    std::unique_lock lock(mock->mutex);
    const bool signaled = mock->changed.wait_for(lock, std::chrono::seconds(10), [&] {
        return std::all_of(fences, fences + count, [](VkFence fence) { return mock->signaled[fence]; });
    });
    if (signaled) return VK_SUCCESS;
    mock->Violation("a fence wait timed out: its batch was never submitted");
    return VK_ERROR_DEVICE_LOST;
}

VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t count, const VkFence* fences) {
    std::lock_guard lock(mock->mutex);
    for (std::uint32_t index = 0; index < count; ++index) mock->signaled[fences[index]] = false;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockDeviceWaitIdle(VkDevice) {
    return VK_SUCCESS;
}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(mockCreateCommandPool)},
        {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyCommandPool)},
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateCommandBuffers)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockFreeCommandBuffers)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockBeginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockEndCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(mockQueueSubmit)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(mockCreateFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyFence)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(mockGetFenceStatus)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(mockWaitForFences)},
        {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(mockResetFences)},
        {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(mockDeviceWaitIdle)},
        {"vkCmdFillBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdFillBuffer)},
        {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdUpdateBuffer)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(mockCmdDispatch)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPipelineBarrier)},
        {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBeginQuery)},
        {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdEndQuery)},
        {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(mockCmdResetQueryPool)},
        {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyQueryPoolResults)},
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBindPipeline)},
    };
    const auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}

std::uintptr_t nextQueue = 0x7000000;

struct Fixture {
    MockDevice device;
    Context context{};
    std::thread::id main = std::this_thread::get_id();

    Fixture() {
        mock = &device;
        device.contextPool = handleOf<VkCommandPool>(device.Handle());
        device.pools.insert(device.contextPool);
        context.device = handleOf<VkDevice>(device.Handle());
        nextQueue += 0x10;
        context.queue = handleOf<VkQueue>(nextQueue);
        context.pool = device.contextPool;
        context.deviceProc = mockProc;
    }

    ~Fixture() {
        std::lock_guard lock(device.mutex);
        for (const auto& violation : device.violations) Expect(false, violation);
        mock = nullptr;
    }

    void Fill(Recorder& recorder, std::uint32_t value) const {
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(recorder.Commands(), handleOf<VkBuffer>(0x100), 0, 4, value);
    }

    void Update(Recorder& recorder, std::uint32_t value) const {
        std::array<std::uint32_t, 4> data{value, 0, 0, 0};
        context.Function<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(recorder.Commands(), handleOf<VkBuffer>(0x100), 0, sizeof(data), data.data());
        data.fill(0xdeadbeefu);
    }

    std::vector<Submission> Submissions() {
        std::lock_guard lock(device.mutex);
        return device.submissions;
    }

    void Drain() const {
        if (auto* submitter = QueueSubmitter::Existing(context.queue); submitter != nullptr) submitter->Drain();
    }
};

std::string joined(const std::vector<std::string>& ops) {
    std::string text;
    for (const auto& op : ops) text += (text.empty() ? "" : ", ") + op;
    return "[" + text + "]";
}

void DeferredBatchesEncodeOffTheRecordingThread() {
    RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, true);
    RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, true);
    const auto before = CommandEncoders::Totals();
    Fixture fixture;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(fixture.context);
        fixture.Fill(recorder, 1);
        fixture.Fill(recorder, 2);
        fixture.Update(recorder, 3);
        Expect(CommandStream::IsHandle(recorder.Commands()), "a deferred batch handed out a command buffer instead of its stream handle");
        recorder.Submit();
        fixture.Fill(recorder, 4);
        recorder.Sync();
        Expect(recorder.Idle(), "a synced deferred batch is still in flight");
        auto submissions = fixture.Submissions();
        Expect(submissions.size() == 2, "two deferred batches made " + std::to_string(submissions.size()) + " submissions");
        if (submissions.size() == 2) {
            Expect(joined(submissions[0].recorded) == "[fill 1, fill 2, update 3]", "the first deferred batch encoded " + joined(submissions[0].recorded));
            Expect(joined(submissions[1].recorded) == "[fill 4]", "the second deferred batch encoded " + joined(submissions[1].recorded));
            for (const auto& submission : submissions) {
                Expect(submission.pool != fixture.context.pool && submission.pool != VK_NULL_HANDLE, "a deferred batch was encoded into the device's command pool");
                Expect(submission.thread != fixture.main, "a deferred batch was submitted on the recording thread");
                Expect(!submission.recordingThreads.empty() && submission.recordingThreads.count(fixture.main) == 0, "a deferred batch's commands reached the driver on the recording thread");
            }
        }
        for (std::uint32_t batch = 0; batch < 12; ++batch) {
            fixture.Fill(recorder, 10 + batch);
            recorder.Sync();
        }
        submissions = fixture.Submissions();
        Expect(submissions.size() == 14, "fourteen deferred batches made " + std::to_string(submissions.size()) + " submissions");
        for (std::size_t index = 2; index < submissions.size(); ++index) Expect(joined(submissions[index].recorded) == "[fill " + std::to_string(8 + index) + "]", "deferred batch " + std::to_string(index) + " encoded " + joined(submissions[index].recorded));
        std::lock_guard lock(fixture.device.mutex);
        Expect(fixture.device.poolsCreated == CommandEncoders::Count(), std::to_string(CommandEncoders::Count()) + " encoders created " + std::to_string(fixture.device.poolsCreated) + " command pools");
        Expect(fixture.device.allocated <= 2 * CommandEncoders::Count(), "fourteen deferred batches, at most two in flight, allocated " + std::to_string(fixture.device.allocated) + " command buffers instead of reusing their encoders' spares");
    }
    const auto after = CommandEncoders::Totals();
    Expect(after.streams - before.streams == 14 && after.ops - before.ops == 16, "the encode counters saw " + std::to_string(after.streams - before.streams) + " streams and " + std::to_string(after.ops - before.ops) + " ops, not 14 and 16");
    std::lock_guard lock(fixture.device.mutex);
    Expect(fixture.device.poolsDestroyed == fixture.device.poolsCreated, "the recorder's teardown left encoder command pools alive");
    Expect(fixture.device.allocated == fixture.device.freed, "the recorder's teardown leaked " + std::to_string(fixture.device.allocated - fixture.device.freed) + " command buffers");
}

void SubmitWaitsForTheEncodeAndDrainWaitsForBoth() {
    RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, true);
    RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, true);
    const auto before = CommandEncoders::Totals();
    Fixture fixture;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(fixture.context);
        {
            std::lock_guard lock(fixture.device.mutex);
            fixture.device.gate = true;
        }
        fixture.Fill(recorder, 7);
        recorder.Submit();
        std::atomic<std::size_t> submittedBeforeGate{99};
        std::atomic<bool> gateOpened{false};
        std::thread opener([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            {
                std::lock_guard lock(fixture.device.mutex);
                submittedBeforeGate.store(fixture.device.submissions.size());
                fixture.device.gateOpen = true;
                gateOpened.store(true);
            }
            fixture.device.changed.notify_all();
        });
        fixture.Drain();
        const bool openedBeforeDrain = gateOpened.load();
        const auto submissions = fixture.Submissions();
        opener.join();
        Expect(submittedBeforeGate.load() == 0, "the submitter submitted a deferred batch before its encode finished");
        Expect(openedBeforeDrain, "Drain returned while a queued batch was still being encoded");
        Expect(submissions.size() == 1 && joined(submissions[0].recorded) == "[fill 7]", "the drained deferred batch was not submitted with its commands");
        {
            std::lock_guard lock(fixture.device.mutex);
            fixture.device.gate = false;
        }
        fixture.Fill(recorder, 8);
        recorder.Submit();
        {
            const QueueAccess access(fixture.context.queue);
            const auto accessed = fixture.Submissions();
            Expect(accessed.size() == 2 && joined(accessed[1].recorded) == "[fill 8]", "queue access did not wait for the queued deferred batch");
        }
        recorder.Sync();
    }
    const auto after = CommandEncoders::Totals();
    Expect(after.waits > before.waits && after.waitNs > before.waitNs, "the submitter's wait for a held encode was not counted");
}

void EncodeFailureIsRethrown() {
    RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, true);
    RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, true);
    const auto before = CommandEncoders::Totals();
    Fixture fixture;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(fixture.context);
        {
            std::lock_guard lock(fixture.device.mutex);
            fixture.device.encoderEndResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        fixture.Fill(recorder, 5);
        recorder.Submit();
        recorder.Sync();
        const auto submissions = fixture.Submissions();
        Expect(submissions.size() == 1 && submissions[0].commands == VK_NULL_HANDLE && submissions[0].fence != VK_NULL_HANDLE, "a batch whose encode failed did not signal its fence with an empty submission");
        {
            std::lock_guard lock(fixture.device.mutex);
            fixture.device.encoderEndResult = VK_SUCCESS;
        }
        fixture.Fill(recorder, 6);
        std::string reported;
        try {
            recorder.Submit();
        } catch (const std::exception& error) {
            reported = error.what();
        }
        Expect(reported.find("vkEndCommandBuffer encoder") != std::string::npos, "the next submit did not rethrow the encoder's failure: '" + reported + "'");
        Expect(!recorder.Recording() && fixture.Submissions().size() == 1, "the batch refused after the encode failure was submitted or left open");
        recorder.Sync();
    }
    const auto after = CommandEncoders::Totals();
    Expect(after.failures - before.failures == 1, "the failed encode was not counted");
    std::lock_guard lock(fixture.device.mutex);
    Expect(fixture.device.allocated == fixture.device.freed, "a failed encode leaked " + std::to_string(fixture.device.allocated - fixture.device.freed) + " command buffers");
}

void DeferredEncodeIsDecidedPerBatch() {
    RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, true);
    Fixture fixture;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(fixture.context);
        RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, false);
        fixture.Fill(recorder, 1);
        Expect(!CommandStream::IsHandle(recorder.Commands()), "a direct batch handed out a stream handle");
        RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, true);
        fixture.Fill(recorder, 2);
        recorder.Submit();
        fixture.Fill(recorder, 3);
        Expect(CommandStream::IsHandle(recorder.Commands()), "a batch opened with the switch set was not deferred");
        RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, false);
        fixture.Fill(recorder, 4);
        recorder.Submit();
        fixture.Fill(recorder, 5);
        recorder.Sync();
        const auto submissions = fixture.Submissions();
        Expect(submissions.size() == 3, "three batches made " + std::to_string(submissions.size()) + " submissions");
        if (submissions.size() == 3) {
            Expect(joined(submissions[0].recorded) == "[fill 1, fill 2]" && submissions[0].pool == fixture.context.pool && submissions[0].recordingThreads.count(fixture.main) == 1, "a batch opened with the switch unset was not recorded directly");
            Expect(joined(submissions[1].recorded) == "[fill 3, fill 4]" && submissions[1].pool != fixture.context.pool && submissions[1].recordingThreads.count(fixture.main) == 0, "a batch opened with the switch set was not encoded on an encoder");
            Expect(joined(submissions[2].recorded) == "[fill 5]" && submissions[2].pool == fixture.context.pool, "the batch after the switch went off was not recorded directly");
        }
    }
    std::lock_guard lock(fixture.device.mutex);
    Expect(fixture.device.allocated == fixture.device.freed, "mixed batches leaked " + std::to_string(fixture.device.allocated - fixture.device.freed) + " command buffers");
}

void AsyncSubmitOffEncodesInline() {
    RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, true);
    RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, true);
    Fixture fixture;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(fixture.context);
        fixture.Fill(recorder, 1);
        RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, false);
        fixture.Fill(recorder, 2);
        recorder.Submit();
        const auto submissions = fixture.Submissions();
        Expect(submissions.size() == 1, "a deferred batch submitted after the submitter went off made " + std::to_string(submissions.size()) + " submissions");
        if (submissions.size() == 1) {
            Expect(joined(submissions[0].recorded) == "[fill 1, fill 2]", "a deferred batch encoded inline recorded " + joined(submissions[0].recorded));
            Expect(submissions[0].pool == fixture.context.pool && submissions[0].thread == fixture.main && submissions[0].recordingThreads.count(fixture.main) == 1, "a deferred batch submitted without the submitter was not encoded and submitted on the calling thread");
        }
        fixture.Fill(recorder, 3);
        Expect(!CommandStream::IsHandle(recorder.Commands()), "a batch opened without the submitter was deferred");
        recorder.Sync();
    }
    std::lock_guard lock(fixture.device.mutex);
    Expect(fixture.device.poolsCreated == 0, "inline encodes started encoder threads");
    Expect(fixture.device.allocated == fixture.device.freed, "inline encodes leaked " + std::to_string(fixture.device.allocated - fixture.device.freed) + " command buffers");
}

void DiscardedBatchIsNeverEncoded() {
    RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, true);
    RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, true);
    const auto before = CommandEncoders::Totals();
    Fixture fixture;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(fixture.context);
        fixture.Fill(recorder, 1);
        Recorder::FailNextSubmitsForTest(1);
        bool failed = false;
        try {
            recorder.Submit();
        } catch (const std::exception&) {
            failed = true;
        }
        Expect(failed && !recorder.Recording(), "an injected submit failure did not discard the deferred batch");
        fixture.Fill(recorder, 2);
        recorder.Sync();
        const auto submissions = fixture.Submissions();
        Expect(submissions.size() == 1 && joined(submissions[0].recorded) == "[fill 2]", "the batch after a discarded deferred batch encoded " + (submissions.empty() ? std::string("nothing") : joined(submissions[0].recorded)));
    }
    const auto after = CommandEncoders::Totals();
    Expect(after.streams - before.streams == 1, "a discarded deferred batch was encoded");
}

void VerifyEncodesInline() {
    RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, true);
    RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, true);
    const auto before = CommandEncoders::Totals();
    Fixture fixture;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(fixture.context);
        fixture.Fill(recorder, 1);
        fixture.Update(recorder, 2);
        Expect(CommandStream::IsHandle(recorder.Commands()), "a verified deferred batch handed out a command buffer");
        recorder.Submit();
        recorder.Sync();
        const auto submissions = fixture.Submissions();
        Expect(submissions.size() == 1, "a verified deferred batch made " + std::to_string(submissions.size()) + " submissions");
        if (submissions.size() == 1) {
            Expect(joined(submissions[0].recorded) == "[fill 1, update 2]", "a verified deferred batch encoded " + joined(submissions[0].recorded));
            Expect(submissions[0].pool == fixture.context.pool && submissions[0].recordingThreads.count(fixture.main) == 1, "a verified deferred batch was not encoded on the recording thread");
            Expect(submissions[0].thread != fixture.main, "a verified deferred batch was not handed to the submitter");
        }
    }
    const auto after = CommandEncoders::Totals();
    Expect(after.inlineStreams - before.inlineStreams == 1 && after.streams == before.streams, "a verified deferred batch was not counted as an inline encode");
    std::lock_guard lock(fixture.device.mutex);
    Expect(fixture.device.poolsCreated == 0, "a verified deferred batch started encoder threads");
    Expect(fixture.device.allocated == fixture.device.freed, "verified batches leaked " + std::to_string(fixture.device.allocated - fixture.device.freed) + " command buffers");
}

}

int main(int argc, char** argv) {
    const bool verify = argc > 1 && std::string_view(argv[1]) == "verify";
    try {
        Expect(CommandEncoders::Inline() == verify, verify ? "APS5_VERIFY_DEFERRED_ENCODE is not seen" : "APS5_VERIFY_DEFERRED_ENCODE is set outside the verify run");
        if (verify) {
            VerifyEncodesInline();
        } else {
            DeferredBatchesEncodeOffTheRecordingThread();
            SubmitWaitsForTheEncodeAndDrainWaitsForBoth();
            EncodeFailureIsRethrown();
            DeferredEncodeIsDecidedPerBatch();
            AsyncSubmitOffEncodesInline();
            DiscardedBatchIsNeverEncoded();
        }
        RuntimeSwitches::Assign(RuntimeSwitch::AsyncSubmit, false);
        RuntimeSwitches::Assign(RuntimeSwitch::DeferredEncode, false);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures != 0) {
        std::fprintf(stderr, "%d deferred encode checks failed\n", failures);
        return 1;
    }
    std::printf("recorder deferred encode tests passed\n");
    return 0;
}
