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
bool Recorder::DumpSamples(VkDeviceAddress target, std::uint64_t address) {
    GuestMemory::AssertGpuLockHeld("Recorder::DumpSamples");
    if (target == 0 || !gpuSampleCounter()) return false;
    countingSamples = true;
    flushDeferredDispatch();
    if (segmentMode()) {
        ensureOpen();
        if (address != 0 && LabelRunsPerBatch() && pendingSamples.size() < SegmentFoldCapacity && open->queuedDumps < DumpFoldCapacity) {
            if (open->segmentActive) endSegment(*open);
            if (open->renderPass.open) dumpsQueuedInPass.fetch_add(1, std::memory_order_relaxed);
            if (open->run.queued.empty()) storeRuns.fetch_add(1, std::memory_order_relaxed);
            auto& queued = open->run.queued.emplace_back();
            queued.address = address;
            queued.dumpTarget = target;
            queued.dumpSegments = static_cast<std::uint32_t>(pendingSamples.size());
            ++open->queuedDumps;
            dumpsQueued.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        FlushStores();
        Commands();
        foldSamples(*open, target);
        dumpsAtOnce.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    Commands();
    endSamples(*open);
    foldSamples(*open, target);
    if (open->samplePool != nullptr) open->kept.push_back(std::move(open->samplePool));
    open->samplePool.reset();
    open->samples = VK_NULL_HANDLE;
    beginSamples(*open);
    return true;
}

bool Recorder::RecordMeshArguments(VkCommandBuffer commands, VkDeviceAddress record, VkDeviceAddress arguments, std::span<const std::uint32_t, 7> rules) {
    constexpr std::uint32_t PushBytes = 48;
    if (meshArgumentState == 0) {
        meshArgumentState = -1;
        VkShaderModule module = VK_NULL_HANDLE;
        try {
            const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, PushBytes};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &push;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &meshArgumentLayout), "vkCreatePipelineLayout mesh arguments");
            module = CreateShaderModule(context, MESH_ARGUMENTS_SPV, sizeof(MESH_ARGUMENTS_SPV), "vkCreateShaderModule mesh arguments");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            pipelineInfo.stage.module = module;
            pipelineInfo.stage.pName = "main";
            pipelineInfo.layout = meshArgumentLayout;
            {
                auto pipelineCacheLock = context.LockPipelineCacheForUse();
                Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &meshArgumentPipeline), "vkCreateComputePipelines mesh arguments");
            }
            context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            meshArgumentState = 1;
        } catch (const std::exception& error) {
            if (module != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            std::fprintf(stderr, "[gpu] mesh indirect draws read their records on the CPU: %s\n", error.what());
        }
    }
    if (meshArgumentState < 0) return false;
    recordBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    context.BindComputePipeline(commands, meshArgumentPipeline);
    struct {
        VkDeviceAddress record;
        VkDeviceAddress arguments;
        std::array<std::uint32_t, 8> rules;
    } parameters{record, arguments, {rules[0], rules[1], rules[2], rules[3], rules[4], rules[5], rules[6], 0u}};
    static_assert(sizeof(parameters) == PushBytes);
    function(cmdPushConstants, "vkCmdPushConstants")(commands, meshArgumentLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, PushBytes, &parameters);
    function(cmdDispatch, "vkCmdDispatch")(commands, 1, 1, 1);
    recordBarrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    CountBarriers(CommandClass::Draw, 2);
    return true;
}

bool Recorder::RecordChangedCopy(VkCommandBuffer commands, VkDeviceAddress result, VkDeviceAddress snapshot, VkDeviceAddress target, std::uint32_t dwords, VkDeviceAddress counter) {
    constexpr std::uint32_t PushBytes = 40;
    constexpr std::uint32_t GroupSize = 64;
    if (changedCopyState == 0) {
        changedCopyState = -1;
        VkShaderModule module = VK_NULL_HANDLE;
        try {
            const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, PushBytes};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &push;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &changedCopyLayout), "vkCreatePipelineLayout changed copy");
            module = CreateShaderModule(context, STAGING_COPY_BACK_SPV, sizeof(STAGING_COPY_BACK_SPV), "vkCreateShaderModule changed copy");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            pipelineInfo.stage.module = module;
            pipelineInfo.stage.pName = "main";
            pipelineInfo.layout = changedCopyLayout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &changedCopyPipeline), "vkCreateComputePipelines changed copy");
            context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            changedCopyState = 1;
        } catch (const std::exception& error) {
            if (module != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            std::fprintf(stderr, "[gpu] staged write-backs copy their whole ranges: %s\n", error.what());
        }
    }
    if (changedCopyState < 0) return false;
    static const bool alwaysCopy = std::getenv("APS5_CHANGED_COPY_ALWAYS") != nullptr;
    context.BindComputePipeline(commands, changedCopyPipeline);
    const auto dispatchLimit = static_cast<std::uint32_t>(std::min<std::uint64_t>(std::uint64_t{context.limits.maxComputeWorkGroupCount[0]} * GroupSize, std::numeric_limits<std::uint32_t>::max() - GroupSize));
    const auto perDispatch = dispatchLimit >= GroupSize ? dispatchLimit - dispatchLimit % GroupSize : dispatchLimit;
    for (std::uint32_t done = 0; done < dwords;) {
        const auto count = std::min(dwords - done, perDispatch);
        struct {
            VkDeviceAddress result;
            VkDeviceAddress snapshot;
            VkDeviceAddress target;
            VkDeviceAddress counter;
            std::uint32_t count;
            std::uint32_t always;
        } parameters{result + VkDeviceAddress{done} * 4u, snapshot + VkDeviceAddress{done} * 4u, target + VkDeviceAddress{done} * 4u, counter, count, alwaysCopy ? 1u : 0u};
        static_assert(sizeof(parameters) == PushBytes);
        function(cmdPushConstants, "vkCmdPushConstants")(commands, changedCopyLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, PushBytes, &parameters);
        function(cmdDispatch, "vkCmdDispatch")(commands, (count + GroupSize - 1) / GroupSize, 1, 1);
        done += count;
    }
    return true;
}

bool Recorder::RecordBoundChangedCopy(VkCommandBuffer commands, BoundWords result, BoundWords snapshot, BoundWords target, std::uint32_t dwords, VkBuffer counter) {
    constexpr std::uint32_t GroupSize = 64;
    struct Parameters {
        std::uint32_t resultBase;
        std::uint32_t snapshotBase;
        std::uint32_t targetBase;
        std::uint32_t count;
        std::uint32_t always;
        std::uint32_t counting;
    };
    if (!context.pushDescriptors) return false;
    if (boundChangedCopyState == 0) {
        boundChangedCopyState = -1;
        VkShaderModule module = VK_NULL_HANDLE;
        try {
            cmdPushChangedCopySet = context.Function<PFN_vkCmdPushDescriptorSetKHR>("vkCmdPushDescriptorSetKHR");
            std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
            for (std::uint32_t binding = 0; binding < bindings.size(); ++binding) bindings[binding] = {binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
            setInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
            setInfo.pBindings = bindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &boundChangedCopySetLayout), "vkCreateDescriptorSetLayout bound changed copy");
            const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Parameters)};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &boundChangedCopySetLayout;
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &push;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &boundChangedCopyLayout), "vkCreatePipelineLayout bound changed copy");
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = sizeof(STAGING_COPY_BACK_BOUND_SPV);
            moduleInfo.pCode = STAGING_COPY_BACK_BOUND_SPV;
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule bound changed copy");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            pipelineInfo.stage.module = module;
            pipelineInfo.stage.pName = "main";
            pipelineInfo.layout = boundChangedCopyLayout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &boundChangedCopyPipeline), "vkCreateComputePipelines bound changed copy");
            context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            boundChangedCopyState = 1;
        } catch (const std::exception& error) {
            if (module != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            std::fprintf(stderr, "[gpu] staged write-backs do not bind their buffers: %s\n", error.what());
        }
    }
    if (boundChangedCopyState < 0) return false;
    const auto alignment = std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, sizeof(std::uint32_t));
    if (context.limits.maxStorageBufferRange <= alignment) return false;
    static const bool alwaysCopy = std::getenv("APS5_CHANGED_COPY_ALWAYS") != nullptr;
    const auto dispatchLimit = static_cast<std::uint32_t>(std::min<std::uint64_t>({std::uint64_t{context.limits.maxComputeWorkGroupCount[0]} * GroupSize, (context.limits.maxStorageBufferRange - alignment) / sizeof(std::uint32_t), std::numeric_limits<std::uint32_t>::max() - GroupSize}));
    const auto perDispatch = dispatchLimit >= GroupSize ? dispatchLimit - dispatchLimit % GroupSize : dispatchLimit;
    context.BindComputePipeline(commands, boundChangedCopyPipeline);
    for (std::uint32_t done = 0; done < dwords;) {
        const auto count = std::min(dwords - done, perDispatch);
        const auto span = VkDeviceSize{count} * sizeof(std::uint32_t);
        std::array<VkDescriptorBufferInfo, 4> infos{};
        std::array<std::uint32_t, 3> bases{};
        const std::array<BoundWords, 3> words{result, snapshot, target};
        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto offset = words[index].offset + VkDeviceSize{done} * sizeof(std::uint32_t);
            const auto bound = offset / alignment * alignment;
            infos[index] = {words[index].buffer, bound, offset - bound + span};
            bases[index] = static_cast<std::uint32_t>((offset - bound) / sizeof(std::uint32_t));
        }
        infos[3] = counter != VK_NULL_HANDLE ? VkDescriptorBufferInfo{counter, 0, sizeof(std::uint32_t)} : infos[0];
        std::array<VkWriteDescriptorSet, 4> writes{};
        for (std::uint32_t binding = 0; binding < writes.size(); ++binding) {
            writes[binding] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[binding].dstBinding = binding;
            writes[binding].descriptorCount = 1;
            writes[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[binding].pBufferInfo = &infos[binding];
        }
        cmdPushChangedCopySet(commands, VK_PIPELINE_BIND_POINT_COMPUTE, boundChangedCopyLayout, 0, static_cast<std::uint32_t>(writes.size()), writes.data());
        const Parameters parameters{bases[0], bases[1], bases[2], count, alwaysCopy ? 1u : 0u, counter != VK_NULL_HANDLE ? 1u : 0u};
        function(cmdPushConstants, "vkCmdPushConstants")(commands, boundChangedCopyLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(parameters), &parameters);
        function(cmdDispatch, "vkCmdDispatch")(commands, (count + GroupSize - 1) / GroupSize, 1, 1);
        done += count;
    }
    return true;
}

void Recorder::beginSamples(Batch& batch) {
    auto cache = std::static_pointer_cast<SamplePoolCache>(samplePools);
    if (sampleCounterState > 0 && cache == nullptr) {
        cache = std::make_shared<SamplePoolCache>();
        cache->device = context.device;
        cache->destroy = context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool");
        samplePools = cache;
    }
    batch.samples = sampleCounterState > 0 ? cache->take() : VK_NULL_HANDLE;
    if (batch.samples == VK_NULL_HANDLE) {
        VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        info.queryType = VK_QUERY_TYPE_OCCLUSION;
        info.queryCount = 1;
        Check(context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(context.device, &info, nullptr, &batch.samples), "vkCreateQueryPool occlusion");
    }
    if (sampleCounterState > 0) {
        try {
            batch.samplePool = std::make_shared<SamplePool>(cache, batch.samples);
        } catch (...) {
            cache->put(batch.samples);
            batch.samples = VK_NULL_HANDLE;
            throw;
        }
    }
    function(cmdResetQueryPool, "vkCmdResetQueryPool")(batch.commands, batch.samples, 0, 1);
    function(cmdBeginQuery, "vkCmdBeginQuery")(batch.commands, batch.samples, 0, context.occlusionQueryPrecise ? VK_QUERY_CONTROL_PRECISE_BIT : 0u);
    batch.sampleActive = true;
    batch.samplesDrawn = false;
}

void Recorder::readSamples(Batch& batch) {
    if (sampleCounterState > 0) return;
    if (batch.samples == VK_NULL_HANDLE || !batch.submitted) return;
    std::uint64_t total = 0;
    for (const auto pool : batch.endedSamples) {
        std::uint64_t samples = 0;
        Check(context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(context.device, pool, 0, 1, sizeof(samples), &samples, sizeof(samples), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "vkGetQueryPoolResults occlusion");
        total += samples;
    }
    std::uint64_t samples = 0;
    Check(context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(context.device, batch.samples, 0, 1, sizeof(samples), &samples, sizeof(samples), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "vkGetQueryPoolResults occlusion");
    samplesPassed.fetch_add(total + samples, std::memory_order_acq_rel);
}

void Recorder::readGpuTiming(Batch& batch) {
    if (batch.queries == VK_NULL_HANDLE || batch.timedKeys.empty()) return;
    std::vector<std::uint64_t> stamps(batch.timedKeys.size() * 2);
    const auto result = context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(context.device, batch.queries, 0, static_cast<std::uint32_t>(stamps.size()), stamps.size() * sizeof(std::uint64_t), stamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT);
    if (result != VK_SUCCESS) return;
    const auto period = context.limits.timestampPeriod;
    if (!GpuTimingEnabled()) {
        if (batch.batchTiming == 0 && stamps[1] >= stamps[0]) {
            batch.gpuStartNs = static_cast<double>(stamps[0]) * period;
            batch.gpuEndNs = static_cast<double>(stamps[1]) * period;
            PerformanceTrace::GpuRange("batch", batch.gpuStartNs, batch.gpuEndNs, batch.traceId);
        }
        return;
    }
    static auto lastReport = std::chrono::steady_clock::now();
    // The union of the timed ranges (class ranges nest program ranges: a copy's transfer inside
    // its class range) against the batch span gives what no range covers.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> intervals;
    intervals.reserve(batch.timedKeys.size());
    std::unique_lock lock(timingMutex);
    for (std::size_t i = 0; i < batch.timedKeys.size(); ++i) {
        const auto ns = static_cast<double>(stamps[i * 2 + 1] - stamps[i * 2]) * period;
        if (i == batch.batchTiming) {
            // The whole batch: apart from the per-program totals it contains.
            batch.gpuStartNs = static_cast<double>(stamps[i * 2]) * period;
            batch.gpuEndNs = static_cast<double>(stamps[i * 2 + 1]) * period;
            PerformanceTrace::GpuRange("batch", batch.gpuStartNs, batch.gpuEndNs, batch.traceId);
            timingBatchMs += ns / 1e6;
            AgcDriver::PerformanceCounters::AddGpuNanoseconds(static_cast<std::uint64_t>(ns));
            continue;
        }
        const auto key = batch.timedKeys[i];
        if (key >= ClassKey(CommandClass::DispatchLeading) && key < ClassKey(CommandClass::Count)) PerformanceTrace::GpuRange(CommandClassNames[key - ClassKey(CommandClass::DispatchLeading)], static_cast<double>(stamps[i * 2]) * period, static_cast<double>(stamps[i * 2 + 1]) * period, batch.traceId);
        auto& totals = timingByKey[key];
        ++totals.count;
        totals.ms += ns / 1e6;
        totals.bytes += batch.timedBytes[i];
        (key >= ClassKey(CommandClass::DispatchLeading) && key < ClassKey(CommandClass::Count) ? timingClassMs : timingProgramMs) += ns / 1e6;
        intervals.emplace_back(stamps[i * 2], stamps[i * 2 + 1]);
    }
    std::sort(intervals.begin(), intervals.end());
    std::uint64_t coveredTicks = 0, unionEnd = 0;
    for (const auto& [begin, end] : intervals) {
        const auto from = std::max(begin, unionEnd);
        if (end > from) coveredTicks += end - from;
        unionEnd = std::max(unionEnd, end);
    }
    timingUnionMs += static_cast<double>(coveredTicks) * period / 1e6;
    ++timingBatches;
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport < std::chrono::seconds(10)) return;
    lastReport = now;
    const auto presents = timingPresents.exchange(0, std::memory_order_relaxed);
    const double perPresent = presents != 0 ? 1.0 / static_cast<double>(presents) : 0.0;
    std::vector<std::pair<std::uint64_t, TimingTotals>> hot;
    std::string classes;
    for (const auto& [key, totals] : timingByKey) {
        if (key >= ClassKey(CommandClass::DispatchLeading) && key < ClassKey(CommandClass::Count)) {
            char text[160];
            std::snprintf(text, sizeof(text), " %s x%llu %.1fms %.1fMiB (per present x%.1f %.2fms %.2fMiB)", CommandClassNames[key - ClassKey(CommandClass::DispatchLeading)], static_cast<unsigned long long>(totals.count), totals.ms, totals.bytes / 1048576.0, static_cast<double>(totals.count) * perPresent, totals.ms * perPresent, totals.bytes / 1048576.0 * perPresent);
            classes += text;
        } else {
            hot.emplace_back(key, totals);
        }
    }
    std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.ms > b.second.ms; });
    // The first field stays the program sum: the measure scripts match on it.
    AgcDriver::ProfilePrint_nid_no_patch("[gputime] %.0f ms of GPU time in %llu batches over 10 s (batch %.0f ms first to last command; classes %.0f ms, all ranges %.0f ms, untimed %.0f ms outside every range; %llu presents; %llu ranges dropped at the %u cap); by program:", timingProgramMs, static_cast<unsigned long long>(timingBatches), timingBatchMs, timingClassMs, timingUnionMs, timingBatchMs - timingUnionMs, static_cast<unsigned long long>(presents), static_cast<unsigned long long>(timingDropped.exchange(0, std::memory_order_relaxed)), MaxTimedRanges);
    for (std::size_t i = 0; i < hot.size() && i < 12; ++i) AgcDriver::ProfilePrint_nid_no_patch(" %s x%llu %.0fms", Recorder::TimingKeyLabel(hot[i].first).c_str(), static_cast<unsigned long long>(hot[i].second.count), hot[i].second.ms);
    AgcDriver::ProfilePrint_nid_no_patch("; by class:%s\n", classes.c_str());
    timingByKey.clear();
    timingProgramMs = timingClassMs = timingUnionMs = timingBatchMs = 0;
    timingBatches = 0;
    lock.unlock();
    reportBarriers();
}

bool Recorder::FlipReadCheck() {
    static const bool enabled = std::getenv("APS5_FLIP_READ_CHECK") != nullptr;
    return enabled;
}

std::vector<Recorder::Completed> Recorder::CompletedBatches(std::uint64_t afterSerial, std::uint64_t throughSerial, std::size_t& missing) const {
    std::vector<Completed> found;
    missing = 0;
    if (throughSerial <= afterSerial) return found;
    std::lock_guard lock(completedMutex);
    for (auto serial = afterSerial + 1; serial <= throughSerial; ++serial) {
        const auto& entry = completed[serial % completed.size()];
        if (entry.serial == serial) found.push_back(entry);
        else ++missing;
    }
    return found;
}

std::uint64_t Recorder::NewestSubmitted(std::chrono::steady_clock::time_point* submittedAt) const {
    std::lock_guard lock(completedMutex);
    if (submittedAt != nullptr) *submittedAt = newestSubmittedAt;
    return newestSubmitted;
}

}
}
}
}
}
}
