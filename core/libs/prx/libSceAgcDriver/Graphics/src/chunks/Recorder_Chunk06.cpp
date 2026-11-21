#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionSnapshots.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/MeshArguments_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBack_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBackBound_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleCounter_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleDumps_spv.h"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DispatchArguments_spv.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include "prx/libc/include/general/AtomicSharedPtr.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
void Recorder::CountSamples() {
    GuestMemory::AssertGpuLockHeld("Recorder::CountSamples");
    gpuSampleCounter();
    countingSamples = true;
    if (segmentMode() || open == nullptr || open->samples != VK_NULL_HANDLE) return;
    if (open->renderPass.open) endOpenRenderPass(PassBreak::Samples);
    beginSamples(*open);
}

std::uint64_t Recorder::SamplesTotal() {
    if (sampleCounterState <= 0) return samplesPassed.load(std::memory_order_acquire);
    if (segmentMode() && !Idle()) Sync();
    std::uint64_t total = 0;
    std::memcpy(&total, sampleCounter->Bytes().data(), sizeof(total));
    for (const auto& segment : pendingSamples) {
        std::uint64_t samples = 0;
        Check(context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(context.device, segment.handle, segment.slot, 1, sizeof(samples), &samples, sizeof(samples), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "vkGetQueryPoolResults occlusion");
        total += samples;
    }
    pendingSamples.clear();
    std::memcpy(sampleCounter->Bytes().data(), &total, sizeof(total));
    return total;
}

void Recorder::NoteSampledDraw(VkCommandBuffer commands) {
    if (open == nullptr) return;
    if (segmentMode()) {
        if (countingSamples && commands == open->commands && !open->segmentActive) beginSegment(*open);
        return;
    }
    if (open->sampleActive) open->samplesDrawn = true;
}

void Recorder::NoteDrawTraceToken(std::uint64_t token) noexcept {
    if (token == 0 || open == nullptr) return;
    try {
        auto inserted = open->drawTraceTokenSet.insert(token);
        if (!inserted.second) return;
        try {
            open->drawTraceTokens.push_back(token);
        } catch (...) {
            open->drawTraceTokenSet.erase(inserted.first);
            throw;
        }
    } catch (...) {
        AgcDriver::CaptureTrace::Log("draw-token-note-failed token=%llu", static_cast<unsigned long long>(token));
    }
}

void Recorder::NoteFaultToken(AgcDriver::FaultToken token) noexcept {
    if (token == 0 || open == nullptr) return;
    if (open->firstToken == 0) open->firstToken = token;
    open->lastToken = token;
}

bool Recorder::gpuSampleCounter() {
    if (sampleCounterState != 0) return sampleCounterState > 0;
    sampleCounterState = -1;
    if (!context.bufferDeviceAddress) return false;
    VkShaderModule module = VK_NULL_HANDLE;
    try {
        sampleCounter = std::make_unique<Buffer>(context, DumpListOffset + 16 * DumpFoldCapacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        std::memset(sampleCounter->Bytes().data(), 0, sampleCounter->Bytes().size());
        const auto initial = samplesPassed.load(std::memory_order_acquire);
        std::memcpy(sampleCounter->Bytes().data(), &initial, sizeof(initial));
        const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, SampleCounterPushBytes};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &sampleLayout), "vkCreatePipelineLayout sample counter");
        module = CreateShaderModule(context, SAMPLE_COUNTER_SPV, sizeof(SAMPLE_COUNTER_SPV), "vkCreateShaderModule sample counter");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = module;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = sampleLayout;
        {
            auto pipelineCacheLock = context.LockPipelineCacheForUse();
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &samplePipeline), "vkCreateComputePipelines sample counter");
        }
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        module = VK_NULL_HANDLE;
        const VkPushConstantRange dumpPush{VK_SHADER_STAGE_COMPUTE_BIT, 0, SampleDumpPushBytes};
        layoutInfo.pPushConstantRanges = &dumpPush;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &dumpLayout), "vkCreatePipelineLayout sample dumps");
        module = CreateShaderModule(context, SAMPLE_DUMPS_SPV, sizeof(SAMPLE_DUMPS_SPV), "vkCreateShaderModule sample dumps");
        pipelineInfo.stage.module = module;
        pipelineInfo.layout = dumpLayout;
        {
            auto pipelineCacheLock = context.LockPipelineCacheForUse();
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &dumpPipeline), "vkCreateComputePipelines sample dumps");
        }
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        module = VK_NULL_HANDLE;
    } catch (const std::exception& error) {
        if (module != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (samplePipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, samplePipeline, nullptr);
        if (dumpLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, dumpLayout, nullptr);
        if (sampleLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, sampleLayout, nullptr);
        samplePipeline = VK_NULL_HANDLE;
        dumpLayout = VK_NULL_HANDLE;
        sampleLayout = VK_NULL_HANDLE;
        sampleCounter.reset();
        std::fprintf(stderr, "[gpu] occlusion counter dumps drain the device: %s\n", error.what());
        return false;
    }
    sampleCounterState = 1;
    return true;
}

std::uint64_t Recorder::Batch::StoreRun::Queued::End() const {
    return address + (dumpTarget != 0 ? SampleDumpBytes : bytes.size());
}

std::vector<std::size_t> Recorder::QueuedWriteOrder(std::span<const QueuedWrite> writes) {
    std::vector<std::size_t> order(writes.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::vector<bool> coveredLater(writes.size()), overLater(writes.size());
    std::map<std::uint64_t, std::uint64_t> later;
    for (auto i = writes.size(); i-- > 0;) {
        const auto& write = writes[i];
        if (!write.computed) {
            const auto after = later.upper_bound(write.begin);
            coveredLater[i] = after != later.begin() && std::prev(after)->second >= write.end;
            const auto last = later.lower_bound(write.end);
            overLater[i] = last != later.begin() && std::prev(last)->second > write.begin;
            continue;
        }
        auto begin = write.begin, end = write.end;
        auto at = later.lower_bound(begin);
        if (at != later.begin() && std::prev(at)->second >= begin) --at;
        while (at != later.end() && at->first <= end) {
            begin = std::min(begin, at->first);
            end = std::max(end, at->second);
            at = later.erase(at);
        }
        later.emplace(begin, end);
    }
    std::vector<std::size_t> before, dumps, after;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> earlier, delayed;
    for (std::size_t i = 0; i < writes.size(); ++i) {
        const auto& write = writes[i];
        const auto over = [&](const std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) {
            return std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return write.begin < range.second && range.first < write.end; });
        };
        if (write.computed) {
            dumps.push_back(i);
            earlier.emplace_back(write.begin, write.end);
        } else if (dumps.empty()) {
            before.push_back(i);
        } else if (overLater[i]) {
            if (over(delayed) || (!coveredLater[i] && over(earlier))) return order;
            before.push_back(i);
        } else {
            after.push_back(i);
            delayed.emplace_back(write.begin, write.end);
        }
    }
    order = std::move(before);
    order.insert(order.end(), dumps.begin(), dumps.end());
    order.insert(order.end(), after.begin(), after.end());
    return order;
}

std::vector<std::size_t> Recorder::QueuedWriteGroups(std::span<const QueuedWrite> writes) {
    std::vector<std::size_t> ends;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> computed;
    for (std::size_t i = 0; i < writes.size(); ++i) {
        const auto& write = writes[i];
        if (!write.computed && !computed.empty()) {
            ends.push_back(i);
            computed.clear();
        }
        if (write.computed) computed.emplace_back(write.begin, write.end);
    }
    if (!writes.empty()) ends.push_back(writes.size());
    return ends;
}

bool Recorder::segmentMode() const {
    return sampleCounterState > 0;
}

bool Recorder::QueuesSampleDumps() const {
    return segmentMode() && LabelRunsPerBatch();
}

Recorder::SampleDumpStatistics Recorder::SampleDumpCounts() {
    return SampleDumpStatistics{dumpsQueued.load(std::memory_order_relaxed), dumpsQueuedInPass.load(std::memory_order_relaxed), dumpsAtOnce.load(std::memory_order_relaxed), segmentsClosed.load(std::memory_order_relaxed), dumpFolds.load(std::memory_order_relaxed)};
}

bool Recorder::segmentSlotReady(const Batch& batch) const {
    return batch.segmentActive || (batch.segments != VK_NULL_HANDLE && batch.segmentNext < SegmentSlots);
}

void Recorder::PrepareSampleSlot() {
    GuestMemory::AssertGpuLockHeld("Recorder::PrepareSampleSlot");
    if (!countingSamples || !segmentMode()) return;
    ensureOpen();
    flushDeferredDispatch();
    Require(!open->renderPass.open, "a sample slot is prepared inside a render pass");
    if (!segmentSlotReady(*open)) takeSegmentPool(*open);
}

void Recorder::EndPassSamples() {
    if (open != nullptr && open->segmentActive) endSegment(*open);
}

void Recorder::RecordUncounted(const std::function<void(CommandState&)>& record) {
    Commands();
    if (open->segmentActive) endSegment(*open);
    const bool counted = open->sampleActive;
    if (counted) {
        open->endedSamples.reserve(open->endedSamples.size() + 1);
        endSamples(*open);
        open->endedSamples.push_back(open->samples);
        open->samples = VK_NULL_HANDLE;
    }
    record(open->graphicsState);
    if (counted) beginSamples(*open);
}

void Recorder::takeSegmentPool(Batch& batch) {
    auto cache = std::static_pointer_cast<SamplePoolCache>(segmentPools);
    if (cache == nullptr) {
        cache = std::make_shared<SamplePoolCache>();
        cache->device = context.device;
        cache->destroy = context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool");
        segmentPools = cache;
    }
    if (batch.segmentPool != nullptr) batch.kept.push_back(std::move(batch.segmentPool));
    batch.segmentPool.reset();
    batch.segments = cache->take();
    if (batch.segments == VK_NULL_HANDLE) {
        VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        info.queryType = VK_QUERY_TYPE_OCCLUSION;
        info.queryCount = SegmentSlots;
        Check(context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(context.device, &info, nullptr, &batch.segments), "vkCreateQueryPool occlusion segments");
    }
    try {
        batch.segmentPool = std::make_shared<SamplePool>(cache, batch.segments);
    } catch (...) {
        cache->put(batch.segments);
        batch.segments = VK_NULL_HANDLE;
        throw;
    }
    batch.segmentNext = 0;
    function(cmdResetQueryPool, "vkCmdResetQueryPool")(batch.commands, batch.segments, 0, SegmentSlots);
}

void Recorder::beginSegment(Batch& batch) {
    Require(batch.segments != VK_NULL_HANDLE && batch.segmentNext < SegmentSlots, "an occlusion segment begins without a reset query slot");
    batch.segmentSlot = batch.segmentNext++;
    function(cmdBeginQuery, "vkCmdBeginQuery")(batch.commands, batch.segments, batch.segmentSlot, context.occlusionQueryPrecise ? VK_QUERY_CONTROL_PRECISE_BIT : 0u);
    batch.segmentActive = true;
}

void Recorder::endSegment(Batch& batch) {
    function(cmdEndQuery, "vkCmdEndQuery")(batch.commands, batch.segments, batch.segmentSlot);
    batch.segmentActive = false;
    pendingSamples.push_back({batch.segmentPool, batch.segments, batch.segmentSlot});
    segmentsClosed.fetch_add(1, std::memory_order_relaxed);
}

void Recorder::copySegments(VkCommandBuffer commands, std::uint32_t count) {
    Require(count <= pendingSamples.size() && count <= SegmentFoldCapacity, "occlusion segments exceed the fold capacity");
    const auto copy = function(cmdCopyQueryPoolResults, "vkCmdCopyQueryPoolResults");
    for (std::uint32_t i = 0; i < count;) {
        const auto& first = pendingSamples[i];
        std::uint32_t run = 1;
        while (i + run < count && pendingSamples[i + run].handle == first.handle && pendingSamples[i + run].slot == first.slot + run) ++run;
        copy(commands, first.handle, first.slot, run, sampleCounter->Handle(), 16 + 8 * static_cast<VkDeviceSize>(i), 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        i += run;
    }
}

void Recorder::retireSegments(std::uint32_t count) {
    for (std::uint32_t i = 0; i < count; ++i) {
        if (pendingSamples[i].pool != nullptr) open->kept.push_back(std::move(pendingSamples[i].pool));
    }
    pendingSamples.erase(pendingSamples.begin(), pendingSamples.begin() + count);
}

void Recorder::recordQueuedDumps(VkCommandBuffer commands, std::span<const Batch::StoreRun::Queued> writes) {
    std::uint32_t segments = 0;
    std::vector<std::uint32_t> list;
    for (const auto& write : writes) {
        if (write.dumpTarget == 0) continue;
        segments = write.dumpSegments;
        list.insert(list.end(), {static_cast<std::uint32_t>(write.dumpTarget), static_cast<std::uint32_t>(write.dumpTarget >> 32u), write.dumpSegments, 0u});
    }
    Require(list.size() <= 4u * DumpFoldCapacity, "queued occlusion dumps exceed the fold capacity");
    copySegments(commands, segments);
    function(cmdUpdateBuffer, "vkCmdUpdateBuffer")(commands, sampleCounter->Handle(), DumpListOffset, list.size() * sizeof(std::uint32_t), list.data());
}

void Recorder::dispatchDumps(VkCommandBuffer commands, std::uint32_t firstDump, std::uint32_t dumps, std::uint32_t firstSegment, std::uint32_t lastSegment) {
    recordBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    context.BindComputePipeline(commands, dumpPipeline);
    struct {
        VkDeviceAddress counter;
        VkDeviceAddress list;
        std::uint32_t firstDump;
        std::uint32_t dumps;
        std::uint32_t firstSegment;
        std::uint32_t lastSegment;
        std::uint32_t phase;
    } parameters{sampleCounter->DeviceAddress(), sampleCounter->DeviceAddress() + DumpListOffset, firstDump, dumps, firstSegment, lastSegment, 0u};
    static_assert(sizeof(parameters) == SampleDumpPushBytes);
    const auto push = function(cmdPushConstants, "vkCmdPushConstants");
    const auto dispatch = function(cmdDispatch, "vkCmdDispatch");
    push(commands, dumpLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, SampleDumpPushBytes, &parameters);
    dispatch(commands, 1, 1, 1);
    if (dumps == 0) return;
    const auto publish = [&](std::uint32_t phase) {
        recordBarrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
        parameters.phase = phase;
        push(commands, dumpLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, SampleDumpPushBytes, &parameters);
        dispatch(commands, 1, 1, 1);
    };
    publish(1u);
    publish(2u);
}

void Recorder::foldQueuedDumps(VkCommandBuffer commands, std::span<const Batch::StoreRun::Queued> writes, std::size_t first, std::size_t end, std::uint32_t& dumpIndex, std::uint32_t& consumed) {
    std::uint32_t count = 0, segments = consumed;
    for (std::size_t i = first; i < end; ++i) {
        if (writes[i].dumpTarget == 0) continue;
        ++count;
        segments = writes[i].dumpSegments;
    }
    if (count == 0) return;
    dispatchDumps(commands, dumpIndex, count, consumed, segments);
    dumpIndex += count;
    consumed = segments;
}

void Recorder::foldSegments() {
    const auto commands = open->commands;
    const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(pendingSamples.size(), SegmentFoldCapacity));
    recordBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    copySegments(commands, count);
    dispatchDumps(commands, 0, 0, 0, count);
    recordBarrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
    CountBarriers(CommandClass::Copy, 3);
    retireSegments(count);
}

void Recorder::endSamples(Batch& batch) {
    if (!batch.sampleActive) return;
    function(cmdEndQuery, "vkCmdEndQuery")(batch.commands, batch.samples, 0);
    batch.sampleActive = false;
    if (sampleCounterState > 0 && batch.samplesDrawn) pendingSamples.push_back({batch.samplePool, batch.samples});
    batch.samplesDrawn = false;
}

void Recorder::foldSamples(Batch& batch, VkDeviceAddress target) {
    const auto commands = batch.commands;
    const auto copy = function(cmdCopyQueryPoolResults, "vkCmdCopyQueryPoolResults");
    const auto push = function(cmdPushConstants, "vkCmdPushConstants");
    const auto dispatch = function(cmdDispatch, "vkCmdDispatch");
    const auto bindCompute = context.ComputeBinding();
    std::size_t done = 0;
    do {
        const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(SampleFoldCapacity, pendingSamples.size() - done));
        if (done != 0) recordBarrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& segment = pendingSamples[done + i];
            copy(commands, segment.handle, segment.slot, 1, sampleCounter->Handle(), 16 + 8 * i, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            if (segment.pool != nullptr) batch.kept.push_back(segment.pool);
        }
        done += count;
        const bool last = done == pendingSamples.size();
        recordBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        bindCompute(commands, samplePipeline);
        struct {
            VkDeviceAddress counter;
            VkDeviceAddress target;
            std::uint32_t count;
            std::uint32_t store;
        } parameters{sampleCounter->DeviceAddress(), target, count, last && target != 0 ? 1u : 0u};
        static_assert(sizeof(parameters) == SampleCounterPushBytes);
        push(commands, sampleLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, SampleCounterPushBytes, &parameters);
        dispatch(commands, 1, 1, 1);
        if (parameters.store == 1u) {
            const auto publish = [&](std::uint32_t store) {
                recordBarrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
                parameters.store = store;
                push(commands, sampleLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, SampleCounterPushBytes, &parameters);
                dispatch(commands, 1, 1, 1);
            };
            publish(3u);
            publish(2u);
        }
    } while (done < pendingSamples.size());
    recordBarrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_HOST_READ_BIT);
    CountBarriers(CommandClass::Copy, target != 0 ? 4u : 2u);
    pendingSamples.clear();
}

}
}
}
}
}
}
