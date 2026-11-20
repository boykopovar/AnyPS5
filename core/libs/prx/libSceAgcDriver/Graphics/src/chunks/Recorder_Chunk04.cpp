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
std::uint32_t Recorder::TakeDispatchOrder(bool& unordered) {
    const auto order = std::exchange(nextDispatchOrder, DispatchOrder{});
    unordered = order.unordered;
    return order.queueKey;
}

Recorder::OverlapStatistics Recorder::OverlapCounts() {
    return OverlapStatistics{overlapDeferred.load(std::memory_order_relaxed), overlapJoined.load(std::memory_order_relaxed), overlapFlushed.load(std::memory_order_relaxed)};
}

void Recorder::flushDeferredDispatch() {
    if (open == nullptr || open->deferredDispatchQueue == 0) return;
    open->deferredDispatchQueue = 0;
    open->deferredRun.clear();
    open->runAccess.clear();
    auto after = std::move(open->afterRun);
    open->afterRun.clear();
    constexpr VkAccessFlags dispatchedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
    recordBarrier(open->commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, dispatchedAccess);
    CountBarriers(CommandClass::DispatchTrailing);
    open->coveredAccess = dispatchedAccess;
    overlapFlushed.fetch_add(1, std::memory_order_relaxed);
    std::exception_ptr failure;
    for (auto& work : after) {
        try {
            work(*this);
        } catch (...) {
            if (failure == nullptr) failure = std::current_exception();
        }
    }
    if (failure != nullptr) std::rethrow_exception(failure);
}

bool Recorder::runConflict(std::span<const RunAccess> accesses) const {
    for (const auto& access : accesses) {
        for (const auto& member : open->runAccess) {
            if (((access[2] ^ member[2]) & RunAccessImage) != 0) continue;
            if (!(access[0] < member[1] && member[0] < access[1])) continue;
            if (((access[2] | member[2]) & RunAccessWritten) != 0) return true;
        }
    }
    return false;
}

bool Recorder::joinsRun(std::uint32_t queueKey, std::span<const void* const> owners, std::span<const RunAccess> accesses) const {
    if (queueKey == 0 || open == nullptr || open->deferredDispatchQueue != queueKey || open->renderPass.open || open->run.open) return false;
    const auto taken = [&](const void* owner) { return std::find(open->deferredRun.begin(), open->deferredRun.end(), owner) != open->deferredRun.end(); };
    return std::none_of(owners.begin(), owners.end(), taken) && !runConflict(accesses);
}

VkCommandBuffer Recorder::RunCommands(std::uint32_t queueKey, const void* owner, std::span<const RunAccess> accesses, bool& inRun, VkAccessFlags* coveredAccess) {
    GuestMemory::AssertGpuLockHeld("Recorder::RunCommands");
    inRun = joinsRun(queueKey, std::span(&owner, 1), accesses);
    if (!inRun) return Commands(coveredAccess);
    if (coveredAccess != nullptr) *coveredAccess = 0;
    return open->commands;
}

void Recorder::AfterRun(std::function<void(Recorder&)> work) {
    Require(RunOpen(), "work after a dispatch run without an open run");
    open->afterRun.push_back(std::move(work));
}

bool Recorder::CloseRunOwing(const void* owner) {
    GuestMemory::AssertGpuLockHeld("Recorder::CloseRunOwing");
    if (!RunOpen() || std::find(open->deferredRun.begin(), open->deferredRun.end(), owner) == open->deferredRun.end()) return false;
    flushDeferredDispatch();
    return true;
}

VkCommandBuffer Recorder::OverlapCommands(std::uint32_t queueKey, std::span<const void* const> owners, std::span<const RunAccess> accesses, VkAccessFlags* coveredAccess, bool& overlapped) {
    GuestMemory::AssertGpuLockHeld("Recorder::OverlapCommands");
    overlapped = joinsRun(queueKey, owners, accesses);
    if (!overlapped) return Commands(coveredAccess);
    if (coveredAccess != nullptr) *coveredAccess = 0;
    return open->commands;
}

void Recorder::DeferDispatchBarrier(std::uint32_t queueKey, std::span<const void* const> owners, std::span<const RunAccess> accesses) {
    Require(open != nullptr && queueKey != 0, "a deferred dispatch barrier has no batch");
    if (open->deferredDispatchQueue == 0) {
        open->deferredRun.clear();
        open->runAccess.clear();
    } else {
        overlapJoined.fetch_add(1, std::memory_order_relaxed);
    }
    open->deferredRun.insert(open->deferredRun.end(), owners.begin(), owners.end());
    open->runAccess.insert(open->runAccess.end(), accesses.begin(), accesses.end());
    open->deferredDispatchQueue = queueKey;
    open->coveredAccess = 0;
    overlapDeferred.fetch_add(1, std::memory_order_relaxed);
}

bool Recorder::SnapshotWriteOverlaps(std::uint64_t address, std::size_t bytes) {
    return AgcDriver::Graphics::SnapshotOverlaps(address, bytes);
}

std::shared_ptr<const Recorder::WriteRanges> Recorder::PendingWriteSnapshot() {
    return pendingWrites.load(std::memory_order_acquire);
}

bool Recorder::SnapshotOverlaps(const WriteRanges* snapshot, std::uint64_t address, std::size_t bytes) {
    return AgcDriver::Graphics::SnapshotOverlaps(snapshot, address, bytes);
}

void Recorder::CountSync(int source, const void* site) {
    if (source < 0 || source >= 5) return;
    ++syncCounts[source];
    announcedSource = source;
    announcedSite = site;
}

void Recorder::AnnounceSyncSite(const void* site) {
    announcedSite = site;
}

double Recorder::ThreadWaitedMs() {
    return HostThreadSlot<double, ThreadWaitedSlot>::Get();
}

std::uint64_t Recorder::ReapsWithWork() {
    return holdCounters.reapsWithWork;
}

VkCommandBuffer Recorder::Commands(VkAccessFlags* coveredAccess) {
    ensureOpen();
    flushDeferredDispatch();
    // Work recorded after a draw's render pass or an inline store run must see their writes: the
    // pass's end and the run's trailing barrier go in first (a per-batch run waits for Submit, or
    // for a caller whose ranges overlap a queued store: FlushStores).
    if (open->renderPass.open) endOpenRenderPass(PassBreak::Command);
    if (open->run.open && !LabelRunsPerBatch()) closeStoreRun();
    if (coveredAccess != nullptr) *coveredAccess = open->coveredAccess;
    open->coveredAccess = 0;
    return open->commands;
}

void Recorder::MarkCovered(VkAccessFlags access) {
    if (open != nullptr) open->coveredAccess = access;
}

Recorder::DrawPassStart Recorder::StartDrawPass(std::uint64_t key, bool forced, bool fenced, const PassAccess& access) {
    ensureOpen();
    flushDeferredDispatch();
    auto& pass = open->renderPass;
    if (!pass.open || forced || (countingSamples && segmentMode() && !segmentSlotReady(*open))) {
        const auto reason = !pass.open ? pass.closedBy : forced ? PassBreak::Forced : PassBreak::Segment;
        const auto commands = Commands();
        open->passHazards.Clear();
        open->passHazards.BeginPass();
        return {commands, false, true, reason};
    }
    const auto hazard = open->passHazards.Check(access, pass.key == key);
    if (hazard == PassHazard::None && pass.key == key) {
        PerformanceTrace::Amount("Graphics.RenderPass.Continued", 1);
        return {open->commands, true, false, PassBreak::Continued};
    }
    const auto reason = PassBreakFor(hazard);
    const bool merge = MergeBarriers() && !fenced && !pass.fenced && !pass.afterPass;
    const bool elide = merge && hazard == PassHazard::None;
    endOpenRenderPass(reason, !merge);
    const auto commands = Commands();
    if (!elide) open->passHazards.Clear();
    open->passHazards.BeginPass();
    return {commands, false, !elide, reason};
}

void Recorder::LeaveRenderPassOpen(std::uint64_t key, std::uint32_t timing, bool fenced, const PassAccess& access, std::function<void(CommandState&)> afterPass) {
    Require(open != nullptr, "no batch is open for the render pass");
    auto& pass = open->renderPass;
    if (!pass.open) {
        pass.timing = timing;
        pass.afterPass = std::move(afterPass);
    }
    pass.open = true;
    pass.key = key;
    pass.fenced = pass.fenced || fenced;
    open->passHazards.Add(access);
}

void Recorder::SealPassWriteOnly() {
    if (open != nullptr) open->passHazards.SealWriteOnly();
}

bool Recorder::RenderPassOpen() const {
    return open != nullptr && open->renderPass.open;
}

void Recorder::endOpenRenderPass(PassBreak by, bool barrier) {
    ++passGeneration;
    auto& pass = open->renderPass;
    if (open->segmentActive) endSegment(*open);
    EndRenderPass(context, open->commands);
    // The pass's attachment and shader writes are visible to everything recorded after it (the
    // host sees them at the batch's fence).
    if (barrier) {
        recordBarrier(open->commands, PassEndBarrier.srcStage, PassEndBarrier.dstStage, PassEndBarrier.srcAccess, PassEndBarrier.dstAccess);
        CountBarriers(CommandClass::Draw);
    }
    if (pass.afterPass) pass.afterPass(open->graphicsState);
    EndGpuTiming(pass.timing);
    open->coveredAccess = barrier ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT : 0;
    open->hostReadOwed = true;
    pass = {};
    pass.closedBy = by;
    PerformanceTrace::Amount(PassBreakScopeNames[static_cast<std::size_t>(by)], 1);
}

void Recorder::QueueKeyStore(VkBuffer buffer, VkDeviceSize first, VkDeviceSize last, std::shared_ptr<void> seed, std::uint64_t begin, std::uint64_t end) {
    ensureOpen();
    // A queued label store over the keys lands first (program order: the key store is later).
    FlushStoresOverlapping(begin, static_cast<std::size_t>(end - begin));
    keyStoreCount.fetch_add(1, std::memory_order_relaxed);
    if (seed != nullptr) open->kept.push_back(seed);
    // The same range queued twice lands once: nothing wrote over it in between, or the writer
    // would have recorded the first store first.
    for (const auto& store : open->keyStores) {
        if (store.buffer == buffer && store.first == first && store.last == last) {
            keyStoresJoined.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
    open->keyStores.push_back({buffer, first, last, std::move(seed), begin, end});
    if (KeyStoresEach()) recordKeyStores(false);
}

bool Recorder::QueuedKeyStoreOverlaps(std::uint64_t address, std::size_t bytes) const {
    if (open == nullptr || bytes == 0) return false;
    const auto end = address + bytes;
    return std::any_of(open->keyStores.begin(), open->keyStores.end(), [&](const Batch::KeyStore& store) { return address < store.end && store.begin < end; });
}

bool Recorder::AnyQueuedKeyStore(const std::function<bool(std::uint64_t, std::uint64_t)>& overlaps) const {
    if (open == nullptr) return false;
    return std::any_of(open->keyStores.begin(), open->keyStores.end(), [&](const Batch::KeyStore& store) { return overlaps(store.begin, store.end); });
}

void Recorder::FlushKeyStores() {
    if (open == nullptr || open->keyStores.empty()) return;
    recordKeyStores(true);
}

void Recorder::recordKeyStores(bool forWriter) {
    auto stores = std::move(open->keyStores);
    open->keyStores.clear();
    if (stores.empty()) return;
    const auto commands = Commands();
    const auto timing = beginTiming(ClassKey(CommandClass::DccKeyStore));
    // Ordered behind every earlier recorded read or write of the ranges (the title's DCC clear or
    // decompress kernel storing keys through a V#), and visible to the work after and to the host.
    recordBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    const auto fill = context.Resolved(&DeviceFunctions::cmdFillBuffer, "vkCmdFillBuffer");
    const auto copy = context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
    std::uint64_t bytes = 0;
    for (const auto& store : stores) {
        const VkDeviceSize fillBegin = std::min((store.first + 3) & ~VkDeviceSize{3}, store.last);
        const VkDeviceSize fillEnd = std::max(store.last & ~VkDeviceSize{3}, fillBegin);
        if (fillEnd > fillBegin) fill(commands, store.buffer, fillBegin, fillEnd - fillBegin, 0xffffffffu);
        if (store.seed != nullptr) {
            VkBufferCopy copies[2];
            std::uint32_t copyCount = 0;
            if (fillBegin > store.first) copies[copyCount++] = {0, store.first, fillBegin - store.first};
            if (store.last > fillEnd) copies[copyCount++] = {0, fillEnd, store.last - fillEnd};
            if (copyCount != 0) copy(commands, static_cast<Buffer*>(store.seed.get())->Handle(), store.buffer, copyCount, copies);
        }
        bytes += store.last - store.first;
    }
    recordBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
    CountBarriers(CommandClass::DccKeyStore, 2);
    EndGpuTiming(timing, bytes);
    open->coveredAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
    keyStoreRuns.fetch_add(1, std::memory_order_relaxed);
    if (forWriter) keyStoreRunsForWriter.fetch_add(1, std::memory_order_relaxed);
}

void Recorder::ensureOpen() {
    GuestMemory::AssertGpuLockHeld("Recorder::Commands");
    if (open == nullptr) {
        ++passGeneration;
        auto batch = std::make_unique<Batch>();
        try {
            VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocation.commandPool = context.pool;
            allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocation.commandBufferCount = 1;
            Check(context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocation, &batch->commands), "vkAllocateCommandBuffers recorder");
            if (!spare.empty()) {
                batch->fence = spare.back();
                spare.pop_back();
            } else {
                VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                Check(context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &info, nullptr, &batch->fence), "vkCreateFence recorder");
            }
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Check(function(beginCommandBuffer, "vkBeginCommandBuffer")(batch->commands, &begin), "vkBeginCommandBuffer recorder");
            static const bool suppress = std::getenv("APS5_NO_STATE_SUPPRESSION") == nullptr;
            batch->graphicsState.Begin(batch->commands, suppress);
        } catch (...) {
            release(*batch);
            throw;
        }
        batch->queue = GuestMemory::GpuLockThreadTag();
        if (countingSamples && !segmentMode()) beginSamples(*batch);
        dropStaleHoldKept();
        batch->kept.insert(batch->kept.end(), holdKept.begin(), holdKept.end());
        open = std::move(batch);
        open->predictedSerial = submissions + 1;
        try {
            if (open->queue == 0) WriteCheckpointSerial(context, open->commands, open->predictedSerial, open->queue);
            else WriteCheckpointSentinel(context, open->commands);
        } catch (...) {
        }
        if (PerformanceTrace::Recording()) {
            static std::atomic<std::uint64_t> traceIds{0};
            open->traceId = traceIds.fetch_add(1, std::memory_order_relaxed) + 1;
        }
        TraceBatchTag = open->traceId;
        // The whole-batch range: its start stamp waits for the previous batches like any
        // bottom-of-pipe stamp, so it marks when this batch's execution began.
        if (BatchStampsEnabled()) open->batchTiming = beginTiming(BatchTimingKey);
    }
}

void Recorder::RecordStore(VkBuffer buffer, VkDeviceSize offset, std::span<const std::byte> bytes, std::uint64_t address) {
    if (bytes.empty()) return;
    ensureOpen();
    flushDeferredDispatch();
    auto& run = open->run;
    const VkDeviceSize end = offset + bytes.size();
    storeCount.fetch_add(1, std::memory_order_relaxed);
    if (LabelRunsPerBatch()) {
        // Queued for the batch's run: a store inside the last queued one replaces its bytes, a
        // contiguous one joins it (a joined range overlapping an earlier queued store takes the
        // WAW barrier when the run is recorded, so no join is refused).
        if (run.queued.empty()) storeRuns.fetch_add(1, std::memory_order_relaxed);
        else if (auto& last = run.queued.back(); last.buffer == buffer) {
            const VkDeviceSize lastEnd = last.offset + last.bytes.size();
            if (offset >= last.offset && end <= lastEnd) {
                std::memcpy(last.bytes.data() + (offset - last.offset), bytes.data(), bytes.size());
                storesReplaced.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            if (offset == lastEnd && last.bytes.size() + bytes.size() <= 65536) {
                last.bytes.insert(last.bytes.end(), bytes.begin(), bytes.end());
                storesJoined.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
        run.queued.push_back({buffer, offset, std::vector<std::byte>(bytes.begin(), bytes.end()), address});
        return;
    }
    // A label says the work before it is done: the queued key stores (results of that work) land
    // ahead of every store of the run.
    if (!open->keyStores.empty()) recordKeyStores(true);
    if (open->renderPass.open) endOpenRenderPass(PassBreak::Label);
    open->coveredAccess = 0;
    // Two transfers to the same bytes have no order of their own within the run.
    const auto overlapsRecorded = [&] { return std::any_of(run.recorded.begin(), run.recorded.end(), [&](const auto& store) { return std::get<0>(store) == buffer && offset < std::get<2>(store) && std::get<1>(store) < end; }); };
    if (!run.open) {
        run.timing = beginTiming(ClassKey(CommandClass::LabelRun));
        run.storedBytes = 0;
        // The run's leading barrier: every earlier write of the range (a shader's, a transfer's, a
        // color attachment's) lands before the stores of the run.
        recordBarrier(open->commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        CountBarriers(CommandClass::LabelRun);
        run.open = true;
        storeRuns.fetch_add(1, std::memory_order_relaxed);
    } else if (run.buffer == buffer && !run.bytes.empty()) {
        const VkDeviceSize pendingEnd = run.offset + run.bytes.size();
        if (offset >= run.offset && end <= pendingEnd) {
            std::memcpy(run.bytes.data() + (offset - run.offset), bytes.data(), bytes.size());
            storesReplaced.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (offset == pendingEnd && run.bytes.size() + bytes.size() <= 65536) {
            // A joined store shares the pending vkCmdUpdateBuffer, which no barrier precedes: one
            // that overlaps a store the run already recorded takes the WAW barrier below instead.
            if (!JoinWawCheck() || !overlapsRecorded()) {
                run.bytes.insert(run.bytes.end(), bytes.begin(), bytes.end());
                storesJoined.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            storeJoinsRefused.fetch_add(1, std::memory_order_relaxed);
        }
    }
    flushPendingStore();
    const bool waw = overlapsRecorded();
    if (waw) {
        recordBarrier(open->commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        CountBarriers(CommandClass::LabelRun);
        storeWawBarriers.fetch_add(1, std::memory_order_relaxed);
    }
    run.buffer = buffer;
    run.offset = offset;
    run.bytes.assign(bytes.begin(), bytes.end());
    if (!LabelRunsEnabled()) closeStoreRun();
}

}
}
}
}
}
