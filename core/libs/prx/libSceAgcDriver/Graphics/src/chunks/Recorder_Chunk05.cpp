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
void Recorder::recordBarrier(VkCommandBuffer commands, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) const {
    if (cmdPipelineBarrier == nullptr) {
        RecordMemoryBarrier(context, commands, sourceStage, destinationStage, sourceAccess, destinationAccess);
        return;
    }
    const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, sourceAccess, destinationAccess};
    cmdPipelineBarrier(commands, sourceStage, destinationStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void Recorder::flushPendingStore() {
    auto& run = open->run;
    if (run.bytes.empty()) return;
    function(cmdUpdateBuffer, "vkCmdUpdateBuffer")(open->commands, run.buffer, run.offset, run.bytes.size(), run.bytes.data());
    run.recorded.emplace_back(run.buffer, run.offset, run.offset + run.bytes.size());
    run.storedBytes += run.bytes.size();
    run.bytes.clear();
}

bool Recorder::closeStoreRun(bool atSubmit) {
    flushDeferredDispatch();
    auto& run = open->run;
    if (LabelRunsPerBatch()) {
        auto stores = std::move(run.queued);
        run.queued.clear();
        open->queuedDumps = 0;
        if (stores.empty()) return false;
        if (!open->keyStores.empty()) recordKeyStores(true);
        const bool passEndLast = open->renderPass.open && !open->renderPass.afterPass;
        if (open->renderPass.open) endOpenRenderPass(PassBreak::Label);
        const auto commands = open->commands;
        run.timing = beginTiming(ClassKey(CommandClass::LabelRun));
        if (BarrierValidate()) {
            std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
            for (const auto& store : stores) ranges.emplace_back(store.address, store.End());
            NoteAccess(CommandClass::LabelRun, Access{{}, ranges, {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
        }
        std::uint32_t barriers = 2;
        if (atSubmit) {
            recordBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
        } else if (passEndLast && BarrierBatch() && Covers(PassEndBarrier, LabelLeadingBarrier)) {
            CountMerged(CommandClass::LabelRun);
            barriers = 1;
        } else {
            recordBarrier(commands, LabelLeadingBarrier.srcStage, LabelLeadingBarrier.dstStage, LabelLeadingBarrier.srcAccess, LabelLeadingBarrier.dstAccess);
        }
        std::uint64_t storedBytes = 0;
        const auto update = function(cmdUpdateBuffer, "vkCmdUpdateBuffer");
        std::vector<QueuedWrite> writes;
        writes.reserve(stores.size());
        for (const auto& store : stores) writes.push_back({store.address, store.End(), store.dumpTarget != 0});
        if (const auto order = QueuedWriteOrder(writes); !std::is_sorted(order.begin(), order.end())) {
            std::vector<Batch::StoreRun::Queued> ordered;
            std::vector<QueuedWrite> orderedWrites;
            ordered.reserve(stores.size());
            orderedWrites.reserve(writes.size());
            for (const auto index : order) {
                ordered.push_back(std::move(stores[index]));
                orderedWrites.push_back(writes[index]);
            }
            stores = std::move(ordered);
            writes = std::move(orderedWrites);
        }
        const bool dumps = std::any_of(writes.begin(), writes.end(), [](const QueuedWrite& write) { return write.computed; });
        if (dumps) recordQueuedDumps(commands, stores);
        std::size_t first = 0;
        std::uint32_t dumpIndex = 0, consumed = 0;
        for (const auto end : QueuedWriteGroups(writes)) {
            if (first != 0) {
                recordBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                ++barriers;
                run.recorded.clear();
            }
            for (std::size_t i = first; i < end; ++i) {
                const auto& store = stores[i];
                if (store.dumpTarget != 0) continue;
                const VkDeviceSize storeEnd = store.offset + store.bytes.size();
                if (std::any_of(run.recorded.begin(), run.recorded.end(), [&](const auto& earlier) { return std::get<0>(earlier) == store.buffer && store.offset < std::get<2>(earlier) && std::get<1>(earlier) < storeEnd; })) {
                    recordBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
                    ++barriers;
                    storeWawBarriers.fetch_add(1, std::memory_order_relaxed);
                } else if (i != first) {
                    recordBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
                    ++barriers;
                }
                update(commands, store.buffer, store.offset, store.bytes.size(), store.bytes.data());
                run.recorded.emplace_back(store.buffer, store.offset, storeEnd);
                storedBytes += store.bytes.size();
            }
            const auto before = dumpIndex;
            foldQueuedDumps(commands, stores, first, end, dumpIndex, consumed);
            if (dumpIndex != before) barriers += 3;
            first = end;
        }
        if (dumps) {
            retireSegments(consumed);
            dumpFolds.fetch_add(1, std::memory_order_relaxed);
        }
        const VkPipelineStageFlags written = VK_PIPELINE_STAGE_TRANSFER_BIT | (dumps ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : 0u);
        const VkAccessFlags writtenAccess = VK_ACCESS_TRANSFER_WRITE_BIT | (dumps ? VK_ACCESS_SHADER_WRITE_BIT : 0u);
        if (atSubmit) {
            recordBarrier(commands, written, VK_PIPELINE_STAGE_HOST_BIT, writtenAccess, VK_ACCESS_HOST_READ_BIT);
            open->coveredAccess = 0;
            storeRunsAtSubmit.fetch_add(1, std::memory_order_relaxed);
        } else {
            recordBarrier(commands, written, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, writtenAccess, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            open->coveredAccess = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            storeRunsForced.fetch_add(1, std::memory_order_relaxed);
        }
        CountBarriers(CommandClass::LabelRun, barriers);
        EndGpuTiming(run.timing, storedBytes);
        run.timing = NoTiming;
        run.recorded.clear();
        return atSubmit;
    }
    flushPendingStore();
    recordBarrier(open->commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    CountBarriers(CommandClass::LabelRun);
    EndGpuTiming(run.timing, run.storedBytes);
    run.timing = NoTiming;
    open->coveredAccess = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    run.open = false;
    run.buffer = VK_NULL_HANDLE;
    run.recorded.clear();
    return false;
}

bool Recorder::QueuedStoreOverlaps(std::uint64_t address, std::size_t bytes) const {
    if (open == nullptr || bytes == 0) return false;
    const auto end = address + bytes;
    return std::any_of(open->run.queued.begin(), open->run.queued.end(), [&](const Batch::StoreRun::Queued& store) { return address < store.End() && store.address < end; });
}

bool Recorder::AnyQueuedStore(const std::function<bool(std::uint64_t, std::uint64_t)>& overlaps) const {
    if (open == nullptr) return false;
    return std::any_of(open->run.queued.begin(), open->run.queued.end(), [&](const Batch::StoreRun::Queued& store) { return overlaps(store.address, store.End()); });
}

void Recorder::FlushStores() {
    if (open == nullptr || open->run.queued.empty()) return;
    closeStoreRun(false);
}

namespace {

constexpr std::uint32_t MaxTimedRanges = 512;
constexpr std::size_t CommandClasses = static_cast<std::size_t>(Recorder::CommandClass::Count);
constexpr const char* CommandClassNames[CommandClasses] = {"dispatch-lead", "dispatch-trail", "indirect-args", "label-run", "fill", "fill-clear", "copy", "staging-in", "staging-out", "draw", "storage-upload", "storage-writeback", "dcc-clear", "dcc-keys", "present-blit", "shadow-publish", "template-refresh", "input-mirror"};
constexpr const char* BarrierScopes[CommandClasses] = {"Graphics.Barrier.dispatch-lead", "Graphics.Barrier.dispatch-trail", "Graphics.Barrier.indirect-args", "Graphics.Barrier.label-run", "Graphics.Barrier.fill", "Graphics.Barrier.fill-clear", "Graphics.Barrier.copy", "Graphics.Barrier.staging-in", "Graphics.Barrier.staging-out", "Graphics.Barrier.draw", "Graphics.Barrier.storage-upload", "Graphics.Barrier.storage-writeback", "Graphics.Barrier.dcc-clear", "Graphics.Barrier.dcc-keys", "Graphics.Barrier.present-blit", "Graphics.Barrier.shadow-publish", "Graphics.Barrier.template-refresh", "Graphics.Barrier.input-mirror"};
constexpr const char* BarrierMergedScopes[CommandClasses] = {"Graphics.BarrierMerged.dispatch-lead", "Graphics.BarrierMerged.dispatch-trail", "Graphics.BarrierMerged.indirect-args", "Graphics.BarrierMerged.label-run", "Graphics.BarrierMerged.fill", "Graphics.BarrierMerged.fill-clear", "Graphics.BarrierMerged.copy", "Graphics.BarrierMerged.staging-in", "Graphics.BarrierMerged.staging-out", "Graphics.BarrierMerged.draw", "Graphics.BarrierMerged.storage-upload", "Graphics.BarrierMerged.storage-writeback", "Graphics.BarrierMerged.dcc-clear", "Graphics.BarrierMerged.dcc-keys", "Graphics.BarrierMerged.present-blit", "Graphics.BarrierMerged.shadow-publish", "Graphics.BarrierMerged.template-refresh", "Graphics.BarrierMerged.input-mirror"};
std::atomic<std::uint64_t> classBarriers[CommandClasses]{};
std::atomic<std::uint64_t> classMerged[CommandClasses]{};
constexpr std::size_t HazardKinds = 5;
constexpr const char* HazardNames[HazardKinds] = {"raw", "waw", "war", "image", "conservative"};
std::atomic<std::uint64_t> validateEmitted[CommandClasses]{}, validateSkipped[CommandClasses]{}, validateKinds[HazardKinds]{};
std::atomic<std::uint64_t> validateBatchEnds{0};
std::atomic<std::int64_t> traceBarriersLeft{[] {
    const char* text = std::getenv("APS5_TRACE_BARRIERS");
    return text != nullptr ? static_cast<std::int64_t>(std::strtoll(text, nullptr, 10)) : std::int64_t{0};
}()};
std::atomic<std::uint64_t> timingPresents{0};
std::atomic<std::uint64_t> presentSerial{0};
std::atomic<std::uint64_t> settledFrames{0};
std::atomic<std::uint64_t> settledFrameBusyNs{0};
std::atomic<bool> batchStampsRequested{false};
std::atomic<std::uint64_t> timingDropped{0};
struct TimingTotals { std::uint64_t count = 0; double ms = 0; std::uint64_t bytes = 0; };
AgcDriver::Mutex timingMutex;
std::map<std::uint64_t, TimingTotals> timingByKey;
double timingProgramMs = 0, timingClassMs = 0, timingUnionMs = 0, timingBatchMs = 0;
std::uint64_t timingBatches = 0;

bool DrawOrGpuProfiled() {
    static const bool profiled = std::getenv("APS5_PROFILE_DRAW") != nullptr || Recorder::GpuTimingEnabled();
    return profiled;
}

void reportBarriers() {
    if (!DrawOrGpuProfiled()) return;
    static auto lastReport = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport < std::chrono::seconds(10)) return;
    lastReport = now;
    std::uint64_t total = 0, merged = 0;
    std::string line, mergedLine;
    for (std::size_t i = 0; i < CommandClasses; ++i) {
        const auto count = classBarriers[i].exchange(0, std::memory_order_relaxed);
        total += count;
        char text[64];
        std::snprintf(text, sizeof(text), " %s %llu", CommandClassNames[i], static_cast<unsigned long long>(count));
        line += text;
        const auto mergedCount = classMerged[i].exchange(0, std::memory_order_relaxed);
        if (mergedCount == 0) continue;
        merged += mergedCount;
        std::snprintf(text, sizeof(text), " %s %llu", CommandClassNames[i], static_cast<unsigned long long>(mergedCount));
        mergedLine += text;
    }
    AgcDriver::ProfilePrint_nid_no_patch("[barriers] %llu recorded (10 s) by class:%s; merged %llu:%s", static_cast<unsigned long long>(total), line.c_str(), static_cast<unsigned long long>(merged), mergedLine.c_str());
    if (Recorder::BarrierValidate()) {
        std::uint64_t emitted = 0, skipped = 0;
        std::string classes, kinds;
        for (std::size_t i = 0; i < CommandClasses; ++i) {
            const auto emittedCount = validateEmitted[i].exchange(0, std::memory_order_relaxed);
            const auto skippedCount = validateSkipped[i].exchange(0, std::memory_order_relaxed);
            emitted += emittedCount;
            skipped += skippedCount;
            if (emittedCount == 0 && skippedCount == 0) continue;
            char text[96];
            std::snprintf(text, sizeof(text), " %s %llu/%llu", CommandClassNames[i], static_cast<unsigned long long>(emittedCount), static_cast<unsigned long long>(skippedCount));
            classes += text;
        }
        for (std::size_t i = 0; i < HazardKinds; ++i) {
            char text[64];
            std::snprintf(text, sizeof(text), " %s %llu", HazardNames[i], static_cast<unsigned long long>(validateKinds[i].exchange(0, std::memory_order_relaxed)));
            kinds += text;
        }
        AgcDriver::ProfilePrint_nid_no_patch("; validate: would emit %llu (%s) + %llu batch ends, would skip %llu; emitted/skipped by class:%s", static_cast<unsigned long long>(emitted), kinds.c_str() + 1, static_cast<unsigned long long>(validateBatchEnds.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(skipped), classes.c_str());
    }
    AgcDriver::ProfilePrint_nid_no_patch("\n");
    AgcDriver::ProfilePrint_nid_no_patch("[samples] occlusion dumps (cumulative): %llu queued (%llu inside an open pass), %llu recorded at once; %llu segments closed; %llu folds of queued dumps\n", static_cast<unsigned long long>(dumpsQueued.load(std::memory_order_relaxed)), static_cast<unsigned long long>(dumpsQueuedInPass.load(std::memory_order_relaxed)), static_cast<unsigned long long>(dumpsAtOnce.load(std::memory_order_relaxed)), static_cast<unsigned long long>(segmentsClosed.load(std::memory_order_relaxed)), static_cast<unsigned long long>(dumpFolds.load(std::memory_order_relaxed)));
}

}

bool Recorder::GpuTimingEnabled() {
    static const bool enabled = std::getenv("APS5_PROFILE_GPU") != nullptr;
    return enabled;
}

bool Recorder::BatchStampsEnabled() {
    return GpuTimingEnabled() || DrawProfiled() || PerformanceTrace::Configured() || batchStampsRequested.load(std::memory_order_relaxed);
}

void Recorder::CountBarriers(CommandClass which, std::uint32_t count) {
    const auto index = static_cast<std::size_t>(which);
    PerformanceTrace::Amount(BarrierScopes[index], count);
    if (!DrawOrGpuProfiled()) return;
    classBarriers[index].fetch_add(count, std::memory_order_relaxed);
}

bool Recorder::MergeBarriers() {
    static const bool merge = std::getenv("APS5_FULL_BARRIERS") == nullptr && std::getenv("APS5_NO_BARRIER_ELISION") == nullptr;
    return merge;
}

bool Recorder::BarrierBatch() {
    return MergeBarriers() && !RuntimeSwitches::Set(RuntimeSwitch::NoBarrierBatch);
}

bool Recorder::ElideCovered(CommandClass which, const BarrierMask& wanted, VkAccessFlags covered) {
    if (!BarrierBatch() || !OrderedByTrailing(covered, wanted)) return false;
    CountMerged(which);
    return true;
}

void Recorder::CountMerged(CommandClass which) {
    PerformanceTrace::Amount(BarrierMergedScopes[static_cast<std::size_t>(which)], 1);
    if (!DrawOrGpuProfiled()) return;
    classMerged[static_cast<std::size_t>(which)].fetch_add(1, std::memory_order_relaxed);
}

bool Recorder::BarrierValidate() {
    static const bool validate = std::getenv("APS5_BARRIER_VALIDATE") != nullptr;
    return validate;
}

void Recorder::NoteAccess(CommandClass which, const Access& access) {
    if (!BarrierValidate()) return;
    ensureOpen();
    auto& tracker = open->tracker;
    const auto overlapsAny = [](const auto& ranges, const std::pair<std::uint64_t, std::uint64_t>& range) {
        return std::any_of(ranges.begin(), ranges.end(), [&](const auto& other) { return range.first < other.end && other.begin < range.second; });
    };
    std::size_t kind = HazardKinds;
    if (access.conservative) kind = 4;
    for (const auto& range : access.reads) {
        if (kind != HazardKinds) break;
        if (overlapsAny(tracker.writes, range)) kind = 0;
    }
    for (const auto& range : access.writes) {
        if (kind != HazardKinds) break;
        if (overlapsAny(tracker.writes, range)) kind = 1;
        else if (overlapsAny(tracker.reads, range)) kind = 2;
    }
    for (const auto& [image, written] : access.images) {
        if (kind != HazardKinds) break;
        if (std::any_of(tracker.images.begin(), tracker.images.end(), [&](const auto& seen) { return seen.image == image && (seen.written || written); })) kind = 3;
    }
    const auto index = static_cast<std::size_t>(which);
    if (kind != HazardKinds) {
        validateEmitted[index].fetch_add(1, std::memory_order_relaxed);
        validateKinds[kind].fetch_add(1, std::memory_order_relaxed);
        tracker.reads.clear();
        tracker.writes.clear();
        tracker.images.clear();
    } else {
        validateSkipped[index].fetch_add(1, std::memory_order_relaxed);
        if (traceBarriersLeft.load(std::memory_order_relaxed) > 0 && traceBarriersLeft.fetch_sub(1, std::memory_order_relaxed) > 0) {
            const auto first = [](const auto& ranges) { return ranges.empty() ? std::pair<std::uint64_t, std::uint64_t>{0, 0} : std::pair<std::uint64_t, std::uint64_t>{ranges.front().first, ranges.front().second}; };
            const auto reads = first(access.reads);
            const auto writes = first(access.writes);
            std::fprintf(stderr, "[barriers] would skip %s: reads %zu (first 0x%llx+0x%llx), writes %zu (first 0x%llx+0x%llx), images %zu; since the last barrier: %zu reads, %zu writes, %zu images\n", CommandClassNames[index], access.reads.size(), static_cast<unsigned long long>(reads.first), static_cast<unsigned long long>(reads.second - reads.first), access.writes.size(), static_cast<unsigned long long>(writes.first), static_cast<unsigned long long>(writes.second - writes.first), access.images.size(), tracker.reads.size(), tracker.writes.size(), tracker.images.size());
        }
    }
    for (const auto& [begin, end] : access.reads) tracker.reads.push_back({begin, end, access.stages});
    for (const auto& [begin, end] : access.writes) tracker.writes.push_back({begin, end, access.stages});
    for (const auto& [image, written] : access.images) tracker.images.push_back({image, written, access.stages});
}

void Recorder::CountPresent() {
    timingPresents.fetch_add(1, std::memory_order_relaxed);
    presentSerial.fetch_add(1, std::memory_order_relaxed);
    ReferenceStorage::Shared().ReleaseIdle();
}

std::uint64_t Recorder::Presents() {
    return presentSerial.load(std::memory_order_relaxed);
}

void Recorder::NoteFrameGpuBusy(double milliseconds) {
    settledFrameBusyNs.fetch_add(static_cast<std::uint64_t>(milliseconds * 1e6), std::memory_order_relaxed);
    settledFrames.fetch_add(1, std::memory_order_release);
}

std::pair<std::uint64_t, double> Recorder::FrameGpuBusy() {
    const auto frames = settledFrames.load(std::memory_order_acquire);
    return {frames, static_cast<double>(settledFrameBusyNs.load(std::memory_order_relaxed)) / 1e6};
}

void Recorder::RequestBatchStamps() {
    batchStampsRequested.store(true, std::memory_order_relaxed);
}

void Recorder::AddGpuTiming(CommandClass which, double nanoseconds, std::uint64_t bytes) {
    std::lock_guard lock(timingMutex);
    auto& totals = timingByKey[ClassKey(which)];
    ++totals.count;
    totals.ms += nanoseconds / 1e6;
    totals.bytes += bytes;
    timingClassMs += nanoseconds / 1e6;
}

std::uint32_t Recorder::BeginGpuTiming(std::uint64_t key) {
    if (!GpuTimingEnabled()) return NoTiming;
    ensureOpen();
    if (open->queries == VK_NULL_HANDLE && open->renderPass.open) return NoTiming;
    return beginTiming(key);
}

std::string Recorder::TimingKeyLabel(std::uint64_t key) {
    char text[32];
    std::snprintf(text, sizeof(text), (key & DrawTimingFlag) != 0 ? "draw 0x%llx" : "0x%llx", static_cast<unsigned long long>(key & ~DrawTimingFlag));
    return text;
}

std::uint32_t Recorder::BeginInPassGpuTiming(std::uint64_t key) {
    if (!GpuTimingEnabled() || open == nullptr || open->queries == VK_NULL_HANDLE) return NoTiming;
    return beginTiming(key);
}

std::uint32_t Recorder::beginTiming(std::uint64_t key) {
    const bool full = GpuTimingEnabled();
    if (!full && !(key == BatchTimingKey && BatchStampsEnabled())) return NoTiming;
    const auto commands = open->commands;
    const std::uint32_t queryCount = full ? MaxTimedRanges * 2 : 2;
    if (open->queries == VK_NULL_HANDLE) {
        if (!full && !sparePools.empty()) {
            open->queries = sparePools.back();
            sparePools.pop_back();
        } else {
            VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            info.queryCount = queryCount;
            if (context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(context.device, &info, nullptr, &open->queries) != VK_SUCCESS) {
                open->queries = VK_NULL_HANDLE;
                return NoTiming;
            }
        }
        function(cmdResetQueryPool, "vkCmdResetQueryPool")(commands, open->queries, 0, queryCount);
    }
    if (open->timedKeys.size() >= queryCount / 2) {
        timingDropped.fetch_add(1, std::memory_order_relaxed);
        return NoTiming;
    }
    const auto index = static_cast<std::uint32_t>(open->timedKeys.size());
    open->timedKeys.push_back(key);
    open->timedBytes.push_back(0);
    context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, open->queries, index * 2);
    return index;
}

void Recorder::EndGpuTiming(std::uint32_t index, std::uint64_t bytes) {
    if (index == NoTiming || open == nullptr || open->queries == VK_NULL_HANDLE) return;
    if (index < open->timedBytes.size()) open->timedBytes[index] += bytes;
    context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(open->commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, open->queries, index * 2 + 1);
}

bool Recorder::RecordDispatchArguments(VkCommandBuffer commands, VkDeviceAddress source, VkDeviceAddress arguments, const std::array<std::uint32_t, 3>* threadGroup) {
    constexpr std::uint32_t PushBytes = 48;
    if (dispatchArgumentState == 0) {
        dispatchArgumentState = -1;
        VkShaderModule module = VK_NULL_HANDLE;
        try {
            const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, PushBytes};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &push;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &dispatchArgumentLayout), "vkCreatePipelineLayout dispatch arguments");
            module = CreateShaderModule(context, DISPATCH_ARGUMENTS_SPV, sizeof(DISPATCH_ARGUMENTS_SPV), "vkCreateShaderModule dispatch arguments");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            pipelineInfo.stage.module = module;
            pipelineInfo.stage.pName = "main";
            pipelineInfo.layout = dispatchArgumentLayout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &dispatchArgumentPipeline), "vkCreateComputePipelines dispatch arguments");
            context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            dispatchArgumentState = 1;
        } catch (const std::exception& error) {
            if (module != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            std::fprintf(stderr, "[gpu] indirect dispatches read their group counts unchecked: %s\n", error.what());
        }
    }
    if (dispatchArgumentState < 0) return false;
    recordBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    context.BindComputePipeline(commands, dispatchArgumentPipeline);
    const auto* limit = context.limits.maxComputeWorkGroupCount;
    struct {
        VkDeviceAddress source;
        VkDeviceAddress arguments;
        std::array<std::uint32_t, 4> limits;
        std::array<std::uint32_t, 4> group;
    } parameters{source, arguments, {limit[0], limit[1], limit[2], threadGroup != nullptr ? 1u : 0u}, {1u, 1u, 1u, 0u}};
    if (threadGroup != nullptr) std::copy(threadGroup->begin(), threadGroup->end(), parameters.group.begin());
    static_assert(sizeof(parameters) == PushBytes);
    function(cmdPushConstants, "vkCmdPushConstants")(commands, dispatchArgumentLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, PushBytes, &parameters);
    function(cmdDispatch, "vkCmdDispatch")(commands, 1, 1, 1);
    const VkPipelineStageFlags readers = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | (threadGroup != nullptr ? VK_PIPELINE_STAGE_TRANSFER_BIT : 0u);
    const VkAccessFlags reads = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | (threadGroup != nullptr ? VK_ACCESS_TRANSFER_READ_BIT : 0u);
    recordBarrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, readers, VK_ACCESS_SHADER_WRITE_BIT, reads);
    CountBarriers(CommandClass::DispatchLeading, 2);
    return true;
}

}
}
}
}
}
