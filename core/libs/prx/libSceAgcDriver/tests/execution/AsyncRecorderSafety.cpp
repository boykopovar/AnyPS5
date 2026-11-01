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
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <atomic>
#include <condition_variable>
#include <stdexcept>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

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
    std::vector<VkCommandBuffer> submittedCommands;
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
    mock.submittedCommands.push_back(info->pCommandBuffers[0]);
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

struct StderrCapture {
    std::FILE* file = nullptr;
    int saved = -1;

    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        if (file == nullptr) throw std::runtime_error("cannot create diagnostic capture");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        const int redirected = saved < 0 ? -1 : _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        const int redirected = saved < 0 ? -1 : dup2(fileno(file), fileno(stderr));
#endif
        if (redirected < 0) {
            Restore();
            std::fclose(file);
            file = nullptr;
            throw std::runtime_error("cannot redirect diagnostic capture");
        }
    }

    void Restore() {
        if (saved < 0) return;
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        saved = -1;
    }

    std::string Read() {
        Restore();
        std::rewind(file);
        std::string result;
        std::array<char, 1024> buffer{};
        while (const auto count = std::fread(buffer.data(), 1, buffer.size(), file)) result.append(buffer.data(), count);
        for (std::size_t offset = 0; (offset = result.find("\r\n", offset)) != std::string::npos;) result.erase(offset, 1);
        return result;
    }

    ~StderrCapture() {
        Restore();
        if (file != nullptr) std::fclose(file);
    }
};

struct DiagnosticDeadline {
    std::mutex mutex;
    std::condition_variable wake;
    bool done = false;
    std::thread thread;

    DiagnosticDeadline() : thread([this] {
        std::unique_lock lock(mutex);
        if (!wake.wait_for(lock, std::chrono::seconds(2), [&] { return done; })) {
            std::fprintf(stderr, "FAIL: timeout diagnostic fixture exceeded its two-second deadline\n");
            std::_Exit(1);
        }
    }) {}

    ~DiagnosticDeadline() {
        {
            std::lock_guard lock(mutex);
            done = true;
        }
        wake.notify_one();
        thread.join();
    }
};

struct DiagnosticWaitCall {
    std::uint64_t timeout = 0;
    std::uint32_t count = 0;
    VkSemaphoreWaitFlags flags = 0;
    std::array<VkSemaphore, 2> semaphores{};
    std::array<std::uint64_t, 2> values{};
};

DiagnosticWaitCall DiagnosticCall(const VkSemaphoreWaitInfo* info, std::uint64_t timeout) {
    DiagnosticWaitCall call{timeout, info->semaphoreCount, info->flags};
    const auto count = std::min<std::uint32_t>(info->semaphoreCount, 2);
    std::copy_n(info->pSemaphores, count, call.semaphores.begin());
    std::copy_n(info->pValues, count, call.values.begin());
    return call;
}

struct ForegroundWaitScript {
    VkResult finalResult = VK_SUCCESS;
    std::vector<DiagnosticWaitCall> calls;
};

thread_local ForegroundWaitScript* foregroundWaitScript = nullptr;

struct WatcherWaitScript {
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<std::pair<std::uint32_t, VkResult>> stages;
    std::vector<DiagnosticWaitCall> calls;
    std::size_t stage = 0;
    std::uint32_t timeouts = 0;
    std::uint64_t counter = 0;
    std::uint32_t unexpectedCalls = 0;
    bool ended = false;
    bool keepTimingOut = false;
};

std::atomic<WatcherWaitScript*> watcherWaitScript{nullptr};

VKAPI_ATTR VkResult VKAPI_CALL diagnosticWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* info, std::uint64_t timeout) {
    if (auto* script = foregroundWaitScript; script != nullptr) {
        script->calls.push_back(DiagnosticCall(info, timeout));
        return script->calls.size() <= 2 ? VK_TIMEOUT : script->finalResult;
    }
    if (auto* script = watcherWaitScript.load(std::memory_order_acquire); script != nullptr) {
        std::unique_lock lock(script->mutex);
        script->calls.push_back(DiagnosticCall(info, timeout));
        if (script->keepTimingOut) {
            const bool delay = script->calls.size() >= 101;
            script->wake.notify_all();
            lock.unlock();
            if (delay) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return VK_TIMEOUT;
        }
        if (script->ended || script->stage >= script->stages.size()) {
            ++script->unexpectedCalls;
            script->wake.notify_all();
            return VK_ERROR_DEVICE_LOST;
        }
        const auto [limit, result] = script->stages[script->stage];
        if (script->timeouts++ < limit) return VK_TIMEOUT;
        script->timeouts = 0;
        ++script->stage;
        if (result == VK_SUCCESS) script->counter = info->pValues[0];
        else script->ended = true;
        script->wake.notify_all();
        return result;
    }
    return mockWaitSemaphores(device, info, timeout);
}

VKAPI_ATTR VkResult VKAPI_CALL diagnosticCounterValue(VkDevice device, VkSemaphore semaphore, std::uint64_t* value) {
    if (auto* script = watcherWaitScript.load(std::memory_order_acquire); script != nullptr) {
        std::lock_guard lock(script->mutex);
        *value = script->counter;
        return VK_SUCCESS;
    }
    return mockCounterValue(device, semaphore, value);
}

PFN_vkVoidFunction VKAPI_CALL diagnosticProc(VkDevice device, const char* name) {
    if (std::string_view(name) == "vkWaitSemaphoresKHR") return reinterpret_cast<PFN_vkVoidFunction>(diagnosticWaitSemaphores);
    if (std::string_view(name) == "vkGetSemaphoreCounterValueKHR") return reinterpret_cast<PFN_vkVoidFunction>(diagnosticCounterValue);
    return mockProc(device, name);
}

std::size_t Occurrences(const std::string& text, std::string_view needle) {
    std::size_t result = 0;
    for (std::size_t offset = 0; (offset = text.find(needle, offset)) != std::string::npos; offset += needle.size()) ++result;
    return result;
}

std::string DiagnosticLine(std::uint64_t serial, std::uint64_t seconds, const std::string& suffix) {
    return "[gpu] recorded batch " + std::to_string(serial) + " still running on the GPU after " + std::to_string(seconds) + " s (" + suffix + ")\n";
}

void Refuses(const std::function<void()>& work, const char* operation) {
    const auto before = std::array{mock.submits, mock.ends, mock.fenceWaits, mock.freedCommands};
    bool refused = false;
    try { work(); } catch (const std::runtime_error&) { refused = true; }
    Expect(refused, std::string(operation) + " accepted a live borrow");
    Expect(before == std::array{mock.submits, mock.ends, mock.fenceWaits, mock.freedCommands}, std::string(operation) + " mutated the batch before refusing");
}

void WaitSerialDiagnostics(bool async, VkResult finalResult) {
    DiagnosticDeadline deadline;
    StderrCapture capture;
    ForegroundWaitScript script{finalResult, {}};
    std::uint64_t main = 0;
    std::uint64_t target = 0;
    bool threw = false;
    std::string error;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        auto context = mockContext();
        context.deviceProc = diagnosticProc;
        if (!async) context.asyncQueue = VK_NULL_HANDLE;
        Recorder recorder(context, true);
        recorder.Activate();
        Expect(recorder.AsyncLaneActive() == async, "timeout fixture selected the wrong lane mode");
        recorder.Commands();
        recorder.Submit();
        main = recorder.NewestSubmitted();
        if (async) {
            const Recorder::LaneScope lane(1);
            recorder.Commands();
            recorder.Submit();
        }
        target = recorder.NewestSubmitted();
        recorder.PrepareWait(target);
        foregroundWaitScript = &script;
        try { recorder.WaitSerial(target); }
        catch (const std::runtime_error& failure) { threw = true; error = failure.what(); }
        foregroundWaitScript = nullptr;
        recorder.Sync();
    }
    const auto output = capture.Read();
    Expect(threw == (finalResult != VK_SUCCESS), "WaitSerial swallowed an error or rejected success");
    if (threw) Expect(error.find("vkWaitSemaphoresKHR recorder") != std::string::npos, "WaitSerial propagated an unrelated error");
    Expect(script.calls.size() == 3, "WaitSerial did not retry exactly twice");
    const auto expectedValues = async ? std::array{main, target} : std::array{target, std::uint64_t{0}};
    for (const auto& call : script.calls) {
        Expect(call.timeout == 5'000'000'000ull && call.flags == 0, "WaitSerial changed its timeout or WAIT_ALL mode");
        Expect(call.count == (async ? 2u : 1u) && call.values == expectedValues, "WaitSerial split or changed its complete lane target set");
        Expect(call.semaphores == script.calls.front().semaphores && call.semaphores[0] != VK_NULL_HANDLE, "WaitSerial changed timeline handles between retries");
        if (async) Expect(call.semaphores[1] != VK_NULL_HANDLE && call.semaphores[0] != call.semaphores[1], "WaitSerial aliased the two timelines");
    }
    const std::string suffix = async ? "timeline wait: main " + std::to_string(main) + ", async " + std::to_string(target) : "timeline wait";
    Expect(Occurrences(output, DiagnosticLine(target, 5, suffix)) == 1, "WaitSerial omitted or duplicated its five-second target report");
    Expect(Occurrences(output, DiagnosticLine(target, 10, suffix)) == 1, "WaitSerial omitted or duplicated its ten-second target report");
    Expect(Occurrences(output, "(timeline wait") == 2, "WaitSerial emitted an unexpected timeout report");
}

void ProgressWatcherStopDuringTimeout(bool async) {
    DiagnosticDeadline deadline;
    StderrCapture capture;
    WatcherWaitScript script;
    script.keepTimingOut = true;
    std::chrono::steady_clock::duration teardown{};
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        auto context = mockContext();
        context.deviceProc = diagnosticProc;
        if (!async) context.asyncQueue = VK_NULL_HANDLE;
        auto recorder = std::make_unique<Recorder>(context, true);
        recorder->Activate();
        Expect(recorder->AsyncLaneActive() == async, "watcher stop fixture selected the wrong lane mode");
        const Recorder::LaneScope lane(async ? 1 : 0);
        watcherWaitScript.store(&script, std::memory_order_release);
        recorder->Commands();
        recorder->Submit();
        {
            std::unique_lock scriptLock(script.mutex);
            Expect(script.wake.wait_for(scriptLock, std::chrono::seconds(1), [&] { return script.calls.size() >= 101; }), "progress watcher did not reach its active timeout retry");
        }
        const auto before = std::chrono::steady_clock::now();
        recorder.reset();
        teardown = std::chrono::steady_clock::now() - before;
        watcherWaitScript.store(nullptr, std::memory_order_release);
    }
    std::size_t callsAfterJoin = 0;
    {
        std::lock_guard lock(script.mutex);
        callsAfterJoin = script.calls.size();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    {
        std::lock_guard lock(script.mutex);
        Expect(script.calls.size() == callsAfterJoin, "progress watcher continued polling after teardown joined");
    }
    const auto output = capture.Read();
    Expect(teardown < std::chrono::seconds(1), "progress watcher did not stop promptly during timeout retries");
    Expect(callsAfterJoin >= 101 && !script.ended, "watcher stop fixture terminated through an error instead of teardown");
    for (const auto& call : script.calls) {
        Expect(call.timeout == 50'000'000ull && call.count == 1 && call.flags == 0 && call.values[0] == 1, "active watcher retry changed its short polling contract");
    }
    Expect(Occurrences(output, DiagnosticLine(1, 5, "progress watcher")) == 1, "active watcher retry omitted or duplicated its five-second report");
    Expect(output.find("watcher stops") == std::string::npos, "active watcher stop fixture returned an unexpected wait error");
}

void ProgressWatcherDiagnostics(bool async, bool advance, std::uint32_t timeouts) {
    DiagnosticDeadline deadline;
    StderrCapture capture;
    WatcherWaitScript script;
    if (advance) script.stages.emplace_back(100, VK_SUCCESS);
    script.stages.emplace_back(timeouts, VK_ERROR_DEVICE_LOST);
    std::chrono::steady_clock::duration teardown{};
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        auto context = mockContext();
        context.deviceProc = diagnosticProc;
        if (!async) context.asyncQueue = VK_NULL_HANDLE;
        auto recorder = std::make_unique<Recorder>(context, true);
        recorder->Activate();
        Expect(recorder->AsyncLaneActive() == async, "watcher fixture selected the wrong lane mode");
        const Recorder::LaneScope lane(async ? 1 : 0);
        watcherWaitScript.store(&script, std::memory_order_release);
        for (std::size_t i = 0; i < script.stages.size(); ++i) {
            recorder->Commands();
            recorder->Submit();
        }
        {
            std::unique_lock scriptLock(script.mutex);
            Expect(script.wake.wait_for(scriptLock, std::chrono::seconds(1), [&] { return script.ended; }), "progress watcher did not consume the bounded timeout script");
        }
        recorder->Commands();
        recorder->Submit();
        {
            std::unique_lock scriptLock(script.mutex);
            Expect(!script.wake.wait_for(scriptLock, std::chrono::milliseconds(20), [&] { return script.unexpectedCalls != 0; }), "progress watcher retried after its error");
        }
        const auto before = std::chrono::steady_clock::now();
        recorder.reset();
        teardown = std::chrono::steady_clock::now() - before;
        watcherWaitScript.store(nullptr, std::memory_order_release);
    }
    const auto output = capture.Read();
    Expect(teardown < std::chrono::seconds(1), "progress watcher teardown did not join promptly");
    const std::size_t expectedCalls = (advance ? 101 : 0) + timeouts + 1;
    Expect(script.calls.size() == expectedCalls && script.unexpectedCalls == 0, "progress watcher made calls after failure or skipped timeout polls");
    for (std::size_t i = 0; i < script.calls.size(); ++i) {
        const auto& call = script.calls[i];
        Expect(call.timeout == 50'000'000ull && call.count == 1 && call.flags == 0, "progress watcher changed its short WAIT_ALL polling contract");
        Expect(call.values[0] == (advance && i >= 101 ? 2u : 1u), "progress watcher failed to advance its completed target");
        Expect(call.semaphores[0] != VK_NULL_HANDLE && call.semaphores == script.calls.front().semaphores, "progress watcher changed its lane timeline");
    }
    const auto finalTarget = advance ? 2u : 1u;
    Expect(Occurrences(output, DiagnosticLine(finalTarget, 5, "progress watcher")) == (timeouts == 100 ? 1u : 0u), "progress watcher reported before one hundred polls or missed its five-second report");
    if (advance) Expect(Occurrences(output, DiagnosticLine(1, 5, "progress watcher")) == 1, "progress watcher missed its first target report");
    Expect(Occurrences(output, "(progress watcher)") == (advance ? 1u : 0u) + (timeouts == 100 ? 1u : 0u), "progress watcher did not reset the report budget after successful advancement");
    Expect(output.find("after 10 s (progress watcher)") == std::string::npos, "progress watcher carried elapsed time across successful advancement");
    const auto failure = "[gpu] progress watcher: vkWaitSemaphoresKHR failed (" + std::to_string(static_cast<int>(VK_ERROR_DEVICE_LOST)) + "), watcher stops\n";
    Expect(Occurrences(output, failure) == 1, "progress watcher omitted or duplicated its terminal error diagnostic");
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

void UnwoundCompletionReturnEdge(bool label) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    bool wrote = false;
    bool completed = false;
    alignas(4) std::uint32_t destination = 0;
    const std::uint32_t value = 0x12345678;
    const auto submitted = mock.waitedValues.size();
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    {
        const Recorder::LaneScope lane(1);
        recorder.Commands();
    }
    recorder.Commands();
    {
        const Recorder::LaneScope lane(1);
        bool caught = false;
        std::uint64_t beforeUnwind = 0;
        try {
            const Recorder::LaneSwitch mainLane(&recorder, 0);
            recorder.Commands();
            recorder.OnComplete([&] { wrote = true; });
            beforeUnwind = mock.submits;
            throw std::runtime_error("injected switched body failure");
        } catch (const std::runtime_error& error) {
            caught = std::string_view(error.what()) == "injected switched body failure";
        }
        Expect(caught && Recorder::ThreadLane() == 1, "body unwind lost its exception or restored lane");
        Expect(mock.submits == beforeUnwind, "body unwind submitted its return edge");
        if (label) recorder.AfterCompletions(reinterpret_cast<std::uintptr_t>(&destination), std::as_bytes(std::span(&value, 1)), 1, 0x51, false);
        else Expect(recorder.AfterRecordedWork([&] {
            Expect(wrote, "unwound completion action overtook its source writeback");
            completed = true;
        }), "unwound return edge had no completion carrier");
    }
    Expect(mock.waitedValues.size() == submitted + 3 && mock.waitedValues[submitted + 1] == 0, "unwound return edge changed an earlier main prefix");
    recorder.FinishUpTo(2);
    Expect(!wrote && !completed && destination == 0, "unwound completion escaped before its source writer");
    recorder.FinishUpTo(3);
    Expect(wrote && !completed && destination == 0, "unwound completion lost its separate return carrier");
    recorder.Sync();
    Expect(label ? destination == value : completed, "unwound completion was lost");
}

void FailedReturnDestructorRetry(bool syncFirst) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    int tails = 0;
    int completions = 0;
    VkCommandBuffer source = VK_NULL_HANDLE;
    const auto submitted = mock.submittedCommands.size();
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    bool caught = false;
    try {
        const Recorder::LaneSwitch asyncLane(&recorder, 1);
        source = recorder.Commands();
        recorder.OnComplete([&] { ++completions; });
        recorder.DeferDispatchBarrier(7, {}, {});
        recorder.AfterRun([&](Recorder& active) {
            ++tails;
            Expect(active.Commands() == source, "return callback changed its source command");
            throw std::runtime_error("injected return submission failure");
        });
    } catch (const std::runtime_error& error) {
        caught = std::string_view(error.what()) == "injected return submission failure";
    }
    Expect(caught && Recorder::ThreadLane() == 0, "direct return destructor did not propagate its callback failure");
    Expect(tails == 1 && mock.submittedCommands.size() == submitted, "failed return destructor submitted or repeated source work");
    if (syncFirst) recorder.Sync();
    const auto successor = recorder.Commands();
    recorder.Submit();
    Expect(mock.submittedCommands.size() == submitted + 2, "return retry lost or duplicated a batch");
    if (mock.submittedCommands.size() == submitted + 2) {
        Expect(mock.submittedCommands[submitted] == source && mock.submittedCommands[submitted + 1] == successor, "return retry submitted its successor before its source");
        Expect(mock.waitedValues[submitted + 1] == 1, "return retry lost the source timeline dependency");
    }
    recorder.Sync();
    Expect(tails == 1 && completions == 1, "return retry duplicated a callback or lost completion");
}

void NestedUnwoundReturnEdges(int depth) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    int completed = 0;
    VkCommandBuffer leaf = VK_NULL_HANDLE;
    const auto submitted = mock.submittedCommands.size();
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    const std::function<void(int)> enter = [&](int left) {
        const Recorder::LaneSwitch next(&recorder, 1 - Recorder::ThreadLane());
        if (left != 1) {
            enter(left - 1);
            return;
        }
        leaf = recorder.Commands();
        recorder.OnComplete([&] { ++completed; });
        throw std::runtime_error("injected nested body failure");
    };
    bool caught = false;
    try { enter(depth); }
    catch (const std::runtime_error& error) { caught = std::string_view(error.what()) == "injected nested body failure"; }
    Expect(caught && Recorder::ThreadLane() == 0, "nested unwind lost its original exception or lane");
    Expect(mock.submittedCommands.size() == submitted, "nested unwind submitted between destructors");
    const auto successor = recorder.Commands();
    const bool changedLane = (depth & 1) != 0;
    Expect(mock.submittedCommands.size() == submitted + (changedLane ? 1 : 0), "nested return edges did not coalesce before acquisition");
    Expect(changedLane ? successor != leaf : successor == leaf, "nested return changed the wrong command batch");
    recorder.Submit();
    const auto count = changedLane ? 2u : 1u;
    Expect(mock.submittedCommands.size() == submitted + count, "nested return duplicated its submission");
    if (mock.submittedCommands.size() == submitted + count) {
        Expect(mock.submittedCommands[submitted] == leaf, "nested return lost its leaf batch");
        Expect(mock.waitedValues.back() == (changedLane ? 1u : 0u), "nested return lost its dependency or introduced a cycle");
    }
    recorder.Sync();
    Expect(completed == 1, "nested return duplicated or lost its completion");
}

void ReturnResolverReentry() {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    int tails = 0;
    int completed = 0;
    bool refused = false;
    VkCommandBuffer source = VK_NULL_HANDLE;
    const auto submitted = mock.submittedCommands.size();
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    {
        const Recorder::LaneSwitch asyncLane(&recorder, 1);
        source = recorder.Commands();
        recorder.DeferDispatchBarrier(7, {}, {});
        recorder.AfterRun([&](Recorder& active) {
            ++tails;
            Expect(active.Commands() == source, "return resolver refused its internal source callback");
            active.Keep(std::make_shared<int>(1));
            active.OnComplete([&] { ++completed; });
            const auto before = std::array{mock.submits, mock.ends, mock.allocatedCommands, mock.fenceWaits};
            {
                const Recorder::LaneScope destination(0);
                try { active.Commands(); } catch (const std::runtime_error&) { refused = true; }
            }
            Expect(refused, "return resolver admitted destination reentry");
            Expect(before == std::array{mock.submits, mock.ends, mock.allocatedCommands, mock.fenceWaits}, "refused return reentry mutated command ownership");
            Expect(active.Commands() == source, "return reentry refusal damaged its source callback");
        });
    }
    recorder.Commands();
    recorder.Submit();
    Expect(mock.submittedCommands.size() == submitted + 2 && mock.waitedValues.back() == 1, "resolver callback lost the return dependency");
    recorder.Sync();
    Expect(tails == 1 && completed == 1, "resolver callback was duplicated or lost");
}

void ConsumedReturnPrefix(bool throwPrefix) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    alignas(4) std::uint32_t label = 0;
    const std::uint32_t value = 1;
    int prefixTails = 0;
    int sourceTails = 0;
    int completed = 0;
    bool freshDestinationRefused = false;
    VkCommandBuffer source = VK_NULL_HANDLE;
    VkCommandBuffer prefix = VK_NULL_HANDLE;
    const auto submitted = mock.submittedCommands.size();
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    {
        const Recorder::LaneScope lane(1);
        source = recorder.Commands();
    }
    bool caughtBody = false;
    try {
        const Recorder::LaneSwitch asyncLane(&recorder, 1);
        {
            const Recorder::LaneScope destination(0);
            prefix = recorder.Commands();
            recorder.NoteLabel(reinterpret_cast<std::uintptr_t>(&label), std::as_bytes(std::span(&value, 1)), 1, 0);
            recorder.OnComplete([&] { ++completed; });
            recorder.DeferDispatchBarrier(7, {}, {});
            recorder.AfterRun([&](Recorder& active) {
                ++prefixTails;
                Expect(active.Commands() == prefix, "return resolver refused its existing destination prefix");
                active.Keep(std::make_shared<int>(2));
                if (throwPrefix) throw std::runtime_error("injected destination prefix failure");
            });
        }
        recorder.OnComplete([&] { ++completed; });
        recorder.DeferDispatchBarrier(9, {}, {});
        recorder.AfterRun([&](Recorder& active) {
            ++sourceTails;
            Expect(active.Commands() == source, "consumed return resolver changed its source callback");
            Expect(mock.submittedCommands.size() == submitted + 1 && mock.submittedCommands.back() == prefix, "source callback ran before its consumed destination prefix was submitted");
            const auto before = std::array{mock.submits, mock.ends, mock.allocatedCommands, mock.fenceWaits};
            {
                const Recorder::LaneScope destination(0);
                try { active.Commands(); } catch (const std::runtime_error&) { freshDestinationRefused = true; }
            }
            Expect(freshDestinationRefused, "detached prefix permission admitted fresh destination work");
            Expect(before == std::array{mock.submits, mock.ends, mock.allocatedCommands, mock.fenceWaits}, "detached prefix reentry mutated command ownership");
            Expect(active.Commands() == source, "detached prefix refusal damaged the source callback");
        });
        Recorder::NoteConsumedLabel(reinterpret_cast<std::uintptr_t>(&label), sizeof(label));
        throw std::runtime_error("injected consumed return body failure");
    } catch (const std::runtime_error& error) {
        caughtBody = std::string_view(error.what()) == "injected consumed return body failure";
    }
    Expect(caughtBody && Recorder::ThreadLane() == 0, "consumed return body lost its exception or restored lane");
    Expect(mock.submittedCommands.size() == submitted, "consumed return submitted during body unwind");
    if (throwPrefix) {
        bool caughtPrefix = false;
        try { recorder.Commands(); }
        catch (const std::runtime_error& error) { caughtPrefix = std::string_view(error.what()) == "injected destination prefix failure"; }
        Expect(caughtPrefix && prefixTails == 1 && sourceTails == 0, "destination prefix failure did not leave the source retryable");
        Expect(mock.submittedCommands.size() == submitted, "failed destination prefix was submitted");
    }
    const auto successor = recorder.Commands();
    recorder.Submit();
    Expect(mock.submittedCommands.size() == submitted + 3, "consumed return lost or duplicated a prefix, source, or successor");
    if (mock.submittedCommands.size() == submitted + 3) {
        Expect(mock.submittedCommands[submitted] == prefix && mock.submittedCommands[submitted + 1] == source && mock.submittedCommands[submitted + 2] == successor, "consumed return reordered command identities");
        Expect(mock.waitedValues[submitted] == 0 && mock.waitedValues[submitted + 2] == 2, "consumed return retroactively changed its prefix or lost its source dependency");
    }
    recorder.Sync();
    Expect(prefixTails == 1 && sourceTails == 1 && completed == 2 && freshDestinationRefused, "consumed return retry duplicated callbacks or lost reentry protection");
}

void CompletionUnwoundReturn(bool destructorOnly, bool followingAction) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    int failedActions = 0;
    int followingActions = 0;
    int producerCompleted = 0;
    VkCommandBuffer producer = VK_NULL_HANDLE;
    const auto submitted = mock.submittedCommands.size();
    const auto liveCommands = mock.allocatedCommands - mock.freedCommands;
    const auto liveFences = mock.signaled.size();
    {
        Recorder recorder(mockContext(), true);
        recorder.Activate();
        recorder.Commands();
        recorder.OnComplete([&] {
            ++failedActions;
            const Recorder::LaneSwitch asyncLane(&recorder, 1);
            producer = recorder.Commands();
            recorder.OnComplete([&] { ++producerCompleted; });
            throw std::runtime_error("injected completion switched body failure");
        });
        if (followingAction) recorder.OnComplete([&] {
            ++followingActions;
            Expect(mock.submittedCommands.size() == submitted + 2 && mock.submittedCommands.back() == producer, "following completion ran before resolving the unwound producer");
        });
        if (!destructorOnly) {
            recorder.Sync();
            Expect(failedActions == 1 && producerCompleted == 1, "Sync stopped after its last popped completion left an unwound producer");
            Expect(recorder.Idle(), "Sync left work after its final completion unwound");
        }
    }
    Expect(failedActions == 1 && producerCompleted == 1, "teardown failed to drain the final completion's unwound producer");
    Expect(followingActions == (followingAction ? 1 : 0), "completion failure skipped or repeated its following action");
    Expect(mock.submittedCommands.size() == submitted + 2 && mock.submittedCommands.back() == producer, "completion unwind lost or duplicated the producer submission");
    Expect(Recorder::ThreadLane() == 0, "completion unwind left the thread on its switched lane");
    Expect(mock.allocatedCommands - mock.freedCommands == liveCommands && mock.signaled.size() == liveFences, "completion unwind leaked command or fence ownership");
}

void DefaultLeaseSubmit() {
    namespace Guest = AgcDriver::GuestMemory;
    namespace Allocations = GuestAllocations;
    using namespace AgcDriver::Graphics;
    std::vector<std::unique_ptr<Recorder>> recorders;
    std::unique_lock gpu(Guest::GpuMutex());
    for (const bool poolFallback : {false, true}) {
        auto context = mockContext();
        context.hostImportAlignment = 1;
        if (!poolFallback) context.asyncQueue = VK_NULL_HANDLE;
        mock.poolFails = poolFallback;
        recorders.push_back(std::make_unique<Recorder>(context, true));
        mock.poolFails = false;
        auto& recorder = *recorders.back();
        recorder.Activate();
        Expect(!recorder.AsyncLaneActive(), "default lease fixture unexpectedly enabled the async queue");
        static_cast<void>(ImportMappedRanges(context, {}, Allocations::GuestAllocationsGeneration_nid_postfix(), false));
        alignas(64) std::array<std::byte, 64> memory{};
        gpu.unlock();
        {
            Allocations::Mutation mutation;
            mutation.Add(memory.data(), memory.size(), true, true, true);
        }
        gpu.lock();
        const auto completed = std::make_shared<std::array<bool, 2>>();
        recorder.Commands();
        const auto lease = std::make_shared<Allocations::Lease>(Allocations::GuestAllocationsAcquire_nid_postfix());
        recorder.Keep(lease);
        recorder.OnComplete([completed, lease] { lease->clear(); (*completed)[0] = true; });
        CountLeaseOutcome(false, recorder.OpenSerial());
        recorder.Submit();
        const auto leaseSerial = recorder.Submissions();
        recorder.Commands();
        recorder.OnComplete([completed] { (*completed)[1] = true; });
        gpu.unlock();
        {
            Allocations::Mutation mutation;
            mutation.Protect(memory.data(), memory.size(), true, false, true, [] {});
            mutation.Remove(memory.data());
        }
        gpu.lock();
        Expect(recorder.Submissions() == leaseSerial + 1, "one-lane pin waiter did not submit the newer unrelated open batch");
        Expect((*completed)[0] && !(*completed)[1], "one-lane pin waiter failed to retire only the older lease prefix");
        recorder.Sync();
    }
    recorders.clear();
}

void ReturnedOnCompleteCarrier(bool unwind) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    bool sourceCompleted = false, actionCompleted = false;
    Recorder recorder(mockContext(), true);
    recorder.Activate();
    const auto submitted = mock.submittedCommands.size();
    VkCommandBuffer prefix = VK_NULL_HANDLE;
    try {
        const Recorder::LaneSwitch async(&recorder, 1);
        recorder.Commands();
        recorder.OnComplete([&] { sourceCompleted = true; });
        {
            const Recorder::LaneScope destination(0);
            prefix = recorder.Commands();
        }
        if (unwind) throw std::runtime_error("injected completion carrier unwind");
    } catch (const std::runtime_error& error) {
        Expect(unwind && std::string_view(error.what()) == "injected completion carrier unwind", "completion carrier lost its body exception");
    }
    recorder.OnComplete([&] {
        Expect(sourceCompleted, "returned OnComplete ran before its source completion");
        actionCompleted = true;
    });
    recorder.Submit();
    Expect(mock.submittedCommands.size() == submitted + 3, "returned OnComplete did not separate the old destination prefix from its carrier");
    if (mock.submittedCommands.size() == submitted + 3) {
        Expect(mock.submittedCommands[submitted + 1] == prefix && mock.submittedCommands.back() != prefix, "returned OnComplete reused the destination prefix as its carrier");
        Expect(mock.waitedValues[submitted + 1] == 0 && mock.waitedValues.back() == 1, "returned OnComplete lost the source dependency or changed prior destination work");
    }
    recorder.Sync();
    Expect(sourceCompleted && actionCompleted, "returned OnComplete lost completion work");
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
        for (bool async : {false, true}) {
            WaitSerialDiagnostics(async, VK_SUCCESS);
            WaitSerialDiagnostics(async, VK_ERROR_DEVICE_LOST);
            ProgressWatcherDiagnostics(async, false, 99);
            ProgressWatcherDiagnostics(async, false, 100);
            ProgressWatcherDiagnostics(async, true, 100);
            ProgressWatcherStopDuringTimeout(async);
        }
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
        UnwoundCompletionReturnEdge(false);
        UnwoundCompletionReturnEdge(true);
        FailedReturnDestructorRetry(false);
        FailedReturnDestructorRetry(true);
        NestedUnwoundReturnEdges(2);
        NestedUnwoundReturnEdges(3);
        NestedUnwoundReturnEdges(4);
        NestedUnwoundReturnEdges(5);
        ReturnResolverReentry();
        ConsumedReturnPrefix(false);
        ConsumedReturnPrefix(true);
        CompletionUnwoundReturn(false, false);
        CompletionUnwoundReturn(false, true);
        CompletionUnwoundReturn(true, false);
        ReturnedOnCompleteCarrier(false);
        ReturnedOnCompleteCarrier(true);
        DefaultLeaseSubmit();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return failures == 0 ? 0 : 1;
}
