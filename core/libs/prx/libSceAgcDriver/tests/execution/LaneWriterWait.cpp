#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::Recorder;
using Range = std::pair<std::uint64_t, std::uint64_t>;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("lane writer wait: " + message);
}

struct Fence {
    bool signaled = false;
    std::uint64_t serial = 0;
};

struct Mock {
    std::uintptr_t next = 1;
    std::map<VkFence, Fence> fences;
    std::uint64_t submits = 0;
    std::uint64_t ends = 0;
    std::uint64_t waits = 0;
    std::vector<std::uint64_t> waited;
    std::vector<std::pair<VkCommandBuffer, std::uint64_t>> submittedCommands;
    std::function<void()> beforeWait;
};

Mock mock;
std::array<std::byte, 1024> storage{};

Range range(std::size_t index) {
    const auto begin = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(storage.data())) + index * 64;
    return {begin, begin + 16};
}

std::array<Range, 1> ranges(std::size_t index) { return {range(index)}; }

VKAPI_ATTR VkResult VKAPI_CALL allocateCommands(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* output) {
    *output = reinterpret_cast<VkCommandBuffer>(mock.next++);
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL freeCommands(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*) {}
VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* output) {
    *output = reinterpret_cast<VkFence>(mock.next++);
    mock.fences.emplace(*output, Fence{});
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) { mock.fences.erase(fence); }
VKAPI_ATTR VkResult VKAPI_CALL fenceStatus(VkDevice, VkFence fence) { return mock.fences.at(fence).signaled ? VK_SUCCESS : VK_NOT_READY; }
VKAPI_ATTR VkResult VKAPI_CALL waitFences(VkDevice, std::uint32_t count, const VkFence* fences, VkBool32, std::uint64_t) {
    if (auto before = std::exchange(mock.beforeWait, {})) before();
    ++mock.waits;
    for (std::uint32_t i = 0; i < count; ++i) {
        auto& fence = mock.fences.at(fences[i]);
        mock.waited.push_back(fence.serial);
        fence.signaled = true;
    }
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL resetFences(VkDevice, std::uint32_t count, const VkFence* fences) {
    for (std::uint32_t i = 0; i < count; ++i) mock.fences.at(fences[i]).signaled = false;
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL beginCommands(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL endCommands(VkCommandBuffer) { ++mock.ends; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL resetCommands(VkCommandBuffer, VkCommandBufferResetFlags) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue, std::uint32_t count, const VkSubmitInfo* info, VkFence fence) {
    Require(count == 1 && info->pNext != nullptr, "unexpected mock submission shape");
    const auto* timeline = static_cast<const VkTimelineSemaphoreSubmitInfo*>(info->pNext);
    Require(timeline->signalSemaphoreValueCount == 1, "submission did not signal one timeline");
    mock.fences.at(fence).serial = timeline->pSignalSemaphoreValues[0];
    for (std::uint32_t i = 0; i < info->commandBufferCount; ++i)
        mock.submittedCommands.emplace_back(info->pCommandBuffers[i], timeline->pSignalSemaphoreValues[0]);
    ++mock.submits;
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL createSemaphore(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore* output) { *output = reinterpret_cast<VkSemaphore>(mock.next++); return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL destroySemaphore(VkDevice, VkSemaphore, const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL createPool(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool* output) { *output = reinterpret_cast<VkCommandPool>(mock.next++); return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL destroyPool(VkDevice, VkCommandPool, const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL waitSemaphores(VkDevice, const VkSemaphoreWaitInfo*, std::uint64_t) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); return VK_TIMEOUT; }
VKAPI_ATTR VkResult VKAPI_CALL counterValue(VkDevice, VkSemaphore, std::uint64_t* value) { *value = 0; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL idle(VkDevice) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL updateBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, const void*) {}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL beginQuery(VkCommandBuffer, VkQueryPool, std::uint32_t, VkQueryControlFlags) {}
VKAPI_ATTR void VKAPI_CALL endQuery(VkCommandBuffer, VkQueryPool, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL resetQuery(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL copyQuery(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) {}
VKAPI_ATTR void VKAPI_CALL bindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL pushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) {}
VKAPI_ATTR void VKAPI_CALL dispatch(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t) {}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL resolve(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> functions{
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(allocateCommands)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(freeCommands)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(createFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(destroyFence)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(fenceStatus)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(waitFences)},
        {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(resetFences)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(beginCommands)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(endCommands)},
        {"vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(resetCommands)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(submit)},
        {"vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(createSemaphore)},
        {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(destroySemaphore)},
        {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(createPool)},
        {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(destroyPool)},
        {"vkWaitSemaphoresKHR", reinterpret_cast<PFN_vkVoidFunction>(waitSemaphores)},
        {"vkGetSemaphoreCounterValueKHR", reinterpret_cast<PFN_vkVoidFunction>(counterValue)},
        {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(idle)},
        {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(updateBuffer)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(barrier)},
        {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(beginQuery)},
        {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(endQuery)},
        {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(resetQuery)},
        {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(copyQuery)},
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(bindPipeline)},
        {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(pushConstants)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(dispatch)},
    };
    const auto found = functions.find(name);
    return found == functions.end() ? nullptr : found->second;
}

Context freshContext() {
    mock = {};
    Context context{};
    context.device = reinterpret_cast<VkDevice>(mock.next++);
    context.queue = reinterpret_cast<VkQueue>(mock.next++);
    context.pool = reinterpret_cast<VkCommandPool>(mock.next++);
    context.deviceProc = resolve;
    context.asyncQueue = reinterpret_cast<VkQueue>(mock.next++);
    context.asyncFamily = 1;
    context.asyncTimestampValidBits = 64;
    context.asyncCommands = std::make_shared<AgcDriver::Graphics::AsyncCommandBuffers>();
    return context;
}

struct Session {
    std::unique_lock<AgcDriver::GuestMemory::GpuMutexType> lock;
    Recorder recorder;
    Session() : lock(AgcDriver::GuestMemory::GpuMutex()), recorder(freshContext(), true) {
        recorder.Activate();
        Require(recorder.AsyncLaneActive(), "mock async lane did not activate");
    }
};

void write(Recorder& recorder, std::size_t index) {
    const auto [begin, end] = range(index);
    recorder.NotePendingWrite(begin, static_cast<std::size_t>(end - begin));
}

bool waited(std::uint64_t serial) { return std::find(mock.waited.begin(), mock.waited.end(), serial) != mock.waited.end(); }

void bothRoots(unsigned submitted) {
    std::array<int, 2> completed{};
    Session session;
    auto& recorder = session.recorder;
    for (const int lane : {1, 0}) {
        const Recorder::LaneScope scope(lane);
        recorder.Commands();
        write(recorder, static_cast<std::size_t>(lane));
        recorder.OnComplete([&, lane] { ++completed[static_cast<std::size_t>(lane)]; });
        if ((submitted & (1u << lane)) != 0) recorder.Submit();
    }
    const std::array selected{range(0), range(1)};
    Require(recorder.WritersPendingOver(selected, 0, 0), "two-lane roots were not selected");
    const auto result = recorder.FinishWritersByLane(selected, 0, 0);
    Require(result.targets[0] != 0 && result.targets[1] != 0, "one selected lane was omitted");
    Require(completed == std::array{1, 1} && recorder.InFlightBatches() == 0, "selected lane callbacks were lost or repeated");
    Require(!recorder.WritersPendingOver(selected, 0, 0), "writer verifier retained completed roots");
}

void exclusions(bool submitted) {
    int completed = 0;
    Session session;
    auto& recorder = session.recorder;
    std::uint64_t own = 0;
    {
        const Recorder::LaneScope scope(1);
        recorder.Commands();
        write(recorder, 0);
        own = recorder.LastWriteNote(range(0).first, 16);
        recorder.OnComplete([&] { ++completed; });
        if (submitted) recorder.Submit();
    }
    Require(own != 0, "own note fixture has no stamp");
    const std::array excluded{own};
    const std::array zero{std::uint64_t{0}};
    Require(recorder.WritersPendingOver(ranges(0), 0, 0, zero), "zero exclusion hid an ordinary write");
    Require(!recorder.WritersPendingOver(ranges(0), 0, 0, excluded), "own note was not excluded on the other open lane");
    const auto before = std::array{mock.submits, mock.ends, mock.waits};
    const auto noHit = recorder.FinishWritersByLane(ranges(0), 0, 0, excluded);
    Require(noHit.targets == std::array<std::uint64_t, 2>{} && before == std::array{mock.submits, mock.ends, mock.waits} && completed == 0,
        "excluded-only query submitted or retired work");
    {
        const Recorder::LaneScope scope(1);
        write(recorder, 0);
    }
    Require(recorder.WritersPendingOver(ranges(0), 0, 0, excluded), "own exclusion also hid a foreign note");
    recorder.FinishWritersByLane(ranges(0), 0, 0, excluded);
    Require(completed == 1 && !recorder.WritersPendingOver(ranges(0), 0, 0, excluded), "foreign write was not completed");
}

void consumedOpenRoots() {
    std::array<int, 3> completed{};
    std::array<std::uint32_t, 2> labels{};
    const std::uint32_t value = 1;
    const auto bytes = std::as_bytes(std::span(&value, 1));
    std::array<VkCommandBuffer, 2> originals{};
    VkCommandBuffer replacement = VK_NULL_HANDLE;
    std::exception_ptr callbackError;
    Session session;
    auto& recorder = session.recorder;
    for (const int lane : {1, 0}) {
        const Recorder::LaneScope scope(lane);
        originals[static_cast<std::size_t>(lane)] = recorder.Commands();
        write(recorder, static_cast<std::size_t>(lane));
        recorder.NoteLabel(reinterpret_cast<std::uintptr_t>(&labels[static_cast<std::size_t>(lane)]), bytes, 1, static_cast<std::uint32_t>(lane));
        recorder.OnComplete([&, lane] {
            ++completed[static_cast<std::size_t>(lane)];
            if (lane != 0) return;
            try {
                const Recorder::LaneScope replacementLane(1);
                replacement = recorder.Commands();
                write(recorder, 2);
                recorder.OnComplete([&] { ++completed[2]; });
            } catch (...) { callbackError = std::current_exception(); }
        });
    }
    for (const int lane : {1, 0}) {
        const Recorder::LaneScope scope(lane);
        Recorder::NoteConsumedLabel(reinterpret_cast<std::uintptr_t>(&labels[static_cast<std::size_t>(1 - lane)]), sizeof(value));
    }
    Require(mock.submits == 0, "consumed open roots were submitted before selection");
    const std::array selected{range(0), range(1)};
    const auto result = recorder.FinishWritersByLane(selected, 0, 0);
    if (callbackError) std::rethrow_exception(callbackError);
    Require(mock.submits == 2 && mock.ends == 2 && mock.submittedCommands.size() == 2, "recursive label consumption lost or duplicated an original submission");
    for (std::size_t lane = 0; lane < originals.size(); ++lane) {
        const auto match = [&](const auto& submitted) { return submitted.first == originals[lane]; };
        Require(std::count_if(mock.submittedCommands.begin(), mock.submittedCommands.end(), match) == 1, "original open root was not submitted exactly once");
        const auto submitted = std::find_if(mock.submittedCommands.begin(), mock.submittedCommands.end(), match);
        Require(result.targets[lane] == submitted->second && waited(submitted->second), "recursive submission mapped the wrong original root serial");
    }
    Require(replacement != VK_NULL_HANDLE && replacement != originals[0] && replacement != originals[1], "completion did not open a distinct replacement");
    Require(completed == std::array{1, 1, 0} && result.leftOpen == 1 && result.leftInFlight == 0, "original-root wait retired or submitted the replacement");
    Require(recorder.WritersPendingOver(ranges(2), 0, 0), "replacement write was lost");
    const auto before = std::array{mock.submits, mock.ends, mock.waits};
    const auto repeated = recorder.FinishWritersByLane(selected, 0, 0);
    Require(repeated.targets == std::array<std::uint64_t, 2>{} && repeated.leftOpen == 1 &&
        before == std::array{mock.submits, mock.ends, mock.waits} && completed == std::array{1, 1, 0}, "completed-root query selected the replacement");
    recorder.Sync();
    Require(completed == std::array{1, 1, 1}, "replacement cleanup lost or duplicated a callback");
}

void lease(bool list, bool publication, bool submitted) {
    int completed = 0;
    Session session;
    auto& recorder = session.recorder;
    std::uint64_t own = 0;
    {
        const Recorder::LaneScope scope(1);
        recorder.Commands();
        write(recorder, 0);
        own = recorder.LastWriteNote(range(0).first, 16);
        if (list) recorder.NotePendingReadList(std::make_shared<const Recorder::ReadList>(Recorder::ReadList{range(0)}), Recorder::ReadKind::AddressBased);
        else recorder.NotePendingRead(range(0).first, 16, Recorder::ReadKind::AddressBased);
        recorder.OnComplete([&] { ++completed; });
        if (submitted) recorder.Submit();
    }
    const auto selected = ranges(publication ? 1 : 0);
    const auto leaseBegin = publication ? range(0).first : 0;
    const auto leaseEnd = publication ? range(0).second : 0;
    const std::array excluded{own};
    Require(recorder.WritersPendingOver(selected, leaseBegin, leaseEnd, excluded), "address-based lease was not selected");
    recorder.FinishWritersByLane(selected, leaseBegin, leaseEnd, excluded);
    Require(completed == 1 && !recorder.WritersPendingOver(selected, leaseBegin, leaseEnd, excluded), "lease target was not finished");
}

void ordinaryRead() {
    int completed = 0;
    Session session;
    auto& recorder = session.recorder;
    {
        const Recorder::LaneScope scope(1);
        recorder.Commands();
        recorder.NotePendingRead(range(0).first, 16, Recorder::ReadKind::DispatchElement);
        recorder.OnComplete([&] { ++completed; });
    }
    Require(!recorder.WritersPendingOver(ranges(0), range(0).first, range(0).second), "ordinary read was classified as a writer or lease");
    recorder.FinishWritersByLane(ranges(0), range(0).first, range(0).second);
    Require(mock.submits == 0 && mock.waits == 0 && completed == 0, "ordinary read caused a wait");
    recorder.Sync();
}

void excludedInflight() {
    std::array<int, 3> completed{};
    Session session;
    auto& recorder = session.recorder;
    {
        const Recorder::LaneScope scope(1);
        recorder.Commands();
        write(recorder, 1);
        recorder.OnComplete([&] { ++completed[0]; });
        recorder.Submit();
    }
    const auto older = recorder.NewestSubmitted();
    recorder.Commands();
    write(recorder, 0);
    recorder.OnComplete([&] { ++completed[1]; });
    recorder.Submit();
    const auto selected = recorder.NewestSubmitted();
    recorder.Commands();
    write(recorder, 2);
    recorder.OnComplete([&] { ++completed[2]; });
    recorder.Submit();
    const auto later = recorder.NewestSubmitted();
    const auto result = recorder.FinishWritersByLane(ranges(0), 0, 0);
    Require(result.targets[0] == selected && result.targets[1] == 0, "global serial widened the selected roots");
    Require(completed == std::array{0, 1, 0} && recorder.InFlightBatches() == 2 && result.leftInFlight == 2,
        "an unrelated older or later batch was retired");
    Require(waited(selected) && !waited(older) && !waited(later), "an unrelated fence was waited");
    Require(recorder.WritersPendingOver(ranges(1), 0, 0) && recorder.WritersPendingOver(ranges(2), 0, 0), "excluded pending notes were removed");
    recorder.Sync();
}

void progressOpen(int selectedLane) {
    std::array<int, 2> completed{};
    bool progressBeforeWait = false;
    Session session;
    auto& recorder = session.recorder;
    {
        const Recorder::LaneScope scope(selectedLane);
        recorder.Commands();
        write(recorder, 0);
        recorder.OnComplete([&] { ++completed[0]; });
        recorder.Submit();
    }
    const auto selected = recorder.NewestSubmitted();
    {
        const Recorder::LaneScope scope(1 - selectedLane);
        recorder.Commands();
        write(recorder, 1);
        recorder.OnComplete([&] { ++completed[1]; });
    }
    const auto before = mock.submits;
    mock.beforeWait = [&] { progressBeforeWait = mock.submits == before + 1; };
    const auto result = recorder.FinishWritersByLane(ranges(0), 0, 0);
    const auto progress = recorder.NewestSubmitted();
    Require(progressBeforeWait && progress > selected, "other open lane did not submit before the blocking wait");
    Require(result.targets[static_cast<std::size_t>(selectedLane)] == selected && result.targets[static_cast<std::size_t>(1 - selectedLane)] == 0,
        "progress submission became a writer root");
    Require(completed == std::array{1, 0} && recorder.InFlightBatches() == 1 && result.leftInFlight == 1,
        "progress-only work was retired");
    Require(waited(selected) && !waited(progress), "progress-only fence was waited");
    recorder.Sync();
}

void prefixDependency() {
    std::array<int, 4> completed{};
    Session session;
    auto& recorder = session.recorder;
    {
        const Recorder::LaneScope scope(1);
        recorder.Commands();
        write(recorder, 1);
        recorder.OnComplete([&] { ++completed[0]; });
        recorder.Submit();
    }
    const auto dependency = recorder.NewestSubmitted();
    recorder.OrderAfterLane(1);
    recorder.Commands();
    write(recorder, 2);
    recorder.OnComplete([&] { ++completed[1]; });
    recorder.Submit();
    const auto prefix = recorder.NewestSubmitted();
    recorder.Commands();
    write(recorder, 0);
    recorder.OnComplete([&] { ++completed[2]; });
    recorder.Submit();
    const auto selected = recorder.NewestSubmitted();
    {
        const Recorder::LaneScope scope(1);
        recorder.Commands();
        write(recorder, 3);
        recorder.OnComplete([&] { ++completed[3]; });
        recorder.Submit();
    }
    const auto excluded = recorder.NewestSubmitted();
    recorder.FinishWritersByLane(ranges(0), 0, 0);
    Require(completed == std::array{1, 1, 1, 0}, "selected FIFO prefix did not carry its other-lane dependency");
    Require(waited(dependency) && waited(prefix) && waited(selected) && !waited(excluded), "dependency closure waited the wrong fences");
    Require(recorder.InFlightBatches() == 1, "dependency finishing retired the other lane's later work");
    recorder.Sync();
}

void callbackReentry() {
    std::array<int, 4> completed{};
    std::exception_ptr callbackError;
    Session session;
    auto& recorder = session.recorder;
    {
        const Recorder::LaneScope scope(1);
        recorder.Commands();
        write(recorder, 2);
        recorder.OnComplete([&] { ++completed[0]; });
        recorder.Submit();
    }
    const auto excludedOlder = recorder.NewestSubmitted();
    recorder.Commands();
    write(recorder, 0);
    recorder.OnComplete([&] {
        ++completed[1];
        try { recorder.FinishWritersByLane(ranges(1), 0, 0); } catch (...) { callbackError = std::current_exception(); }
    });
    recorder.Submit();
    recorder.Commands();
    write(recorder, 1);
    recorder.OnComplete([&] { ++completed[2]; });
    recorder.Submit();
    recorder.Commands();
    write(recorder, 3);
    recorder.OnComplete([&] { ++completed[3]; });
    recorder.Submit();
    const auto excludedLater = recorder.NewestSubmitted();
    const std::array selected{range(0), range(1)};
    recorder.FinishWritersByLane(selected, 0, 0);
    if (callbackError) std::rethrow_exception(callbackError);
    Require(completed == std::array{0, 1, 1, 0} && recorder.InFlightBatches() == 2, "callback reentry duplicated, lost or widened retirement");
    Require(!waited(excludedOlder) && !waited(excludedLater), "callback reentry waited excluded work");
    recorder.Sync();
}

void borrowed(bool selectedBorrow) {
    std::array<int, 2> completed{};
    Session session;
    auto& recorder = session.recorder;
    {
        const Recorder::LaneScope scope(1);
        recorder.Commands();
        write(recorder, 0);
        recorder.OnComplete([&] { ++completed[0]; });
        if (!selectedBorrow) recorder.Submit();
    }
    const auto commands = recorder.Commands();
    write(recorder, 1);
    recorder.OnComplete([&] { ++completed[1]; });
    const std::vector<Range> selected = selectedBorrow ? std::vector<Range>{range(0), range(1)} : std::vector<Range>{range(0)};
    const auto before = std::array{mock.submits, mock.ends, mock.waits};
    const auto beforeFlight = recorder.InFlightBatches();
    {
        const Recorder::CommandBorrow borrow(&recorder, commands);
        const auto empty = recorder.FinishWritersByLane(std::span<const Range>{}, 0, 0);
        Require(empty.targets == std::array<std::uint64_t, 2>{} && before == std::array{mock.submits, mock.ends, mock.waits} &&
            recorder.InFlightBatches() == beforeFlight && completed == std::array{0, 0}, "empty query mutated or refused borrowed work");
        Require(!recorder.WritersPendingOver(std::span<const Range>{}, 0, 0), "empty query matched a writer");
        const auto noHit = recorder.FinishWritersByLane(ranges(9), 0, 0);
        Require(noHit.targets == std::array<std::uint64_t, 2>{} && before == std::array{mock.submits, mock.ends, mock.waits} &&
            recorder.InFlightBatches() == beforeFlight && completed == std::array{0, 0}, "no-hit query mutated or refused borrowed work");
        bool refused = false;
        try { recorder.FinishWritersByLane(selected, 0, 0); }
        catch (const std::runtime_error& error) { refused = std::strstr(error.what(), "borrow") != nullptr; }
        Require(refused, "writer wait accepted a live command borrow");
        Require(before == std::array{mock.submits, mock.ends, mock.waits} && recorder.InFlightBatches() == beforeFlight && completed == std::array{0, 0},
            "borrow refusal happened after end, submit, wait or retirement");
        Require(recorder.Commands() == commands && recorder.WritersPendingOver(selected, 0, 0), "borrow refusal destroyed the open handle or notes");
    }
    recorder.FinishWritersByLane(selected, 0, 0);
    Require(completed[0] == 1 && completed[1] == (selectedBorrow ? 1 : 0), "released borrow did not restore selected completion");
    recorder.Sync();
}

void environment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    if (*value == '\0') unsetenv(name);
    else setenv(name, value, 1);
#endif
}

}

int main() {
    environment("APS5_ASYNC_LANE", "51");
    environment("APS5_COPY_READ_TRACKING", "1");
    environment("APS5_PROFILE_GPU", "");
    environment("APS5_PROFILE_DRAW", "");
    environment("APS5_TRACE_FRAMES", "");
    environment("APS5_TRACE_SECONDS", "");
    environment("APS5_TRACE_SUMMARY_EVERY", "");
    environment("APS5_TRACE_BENCH", "");
    try {
        for (unsigned submitted = 0; submitted < 4; ++submitted) bothRoots(submitted);
        consumedOpenRoots();
        exclusions(false);
        exclusions(true);
        for (const bool list : {false, true})
            for (const bool publication : {false, true})
                for (const bool submitted : {false, true}) lease(list, publication, submitted);
        ordinaryRead();
        excludedInflight();
        progressOpen(0);
        progressOpen(1);
        prefixDependency();
        callbackReentry();
        borrowed(true);
        borrowed(false);
        std::puts("lane writer wait host tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
