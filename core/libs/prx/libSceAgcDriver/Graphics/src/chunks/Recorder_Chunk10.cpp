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
namespace {
std::vector<std::pair<std::uint64_t, std::uint64_t>> Recorder::notedReads(const Batch& batch) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    std::size_t next = 0;
    const auto expand = [&](std::size_t position) {
        for (; next < batch.readLists.size() && batch.readLists[next].position == position; ++next) result.insert(result.end(), batch.readLists[next].ranges->begin(), batch.readLists[next].ranges->end());
    };
    for (std::size_t index = 0; index < batch.reads.size(); ++index) {
        expand(index);
        result.emplace_back(batch.reads[index].begin, batch.reads[index].end);
    }
    expand(batch.reads.size());
    return result;
}

bool Recorder::PendingReadOverlaps(std::uint64_t address, std::size_t bytes, bool ignoreSignaled) const {
    if (bytes == 0) return false;
    readQueries.Add(1);
    const auto end = address + bytes;
    if (open != nullptr) {
        if (const auto kind = readOverlap(*open, address, end)) {
            readHits[static_cast<std::size_t>(*kind)].Add(1);
            return true;
        }
    }
    // Newest first: the batch most likely still running decides without a status query per batch.
    for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
        const auto kind = readOverlap(**it, address, end);
        if (!kind.has_value()) continue;
        if (ignoreSignaled && signaled(**it)) {
            readStaleIgnored.Add(1);
            continue;
        }
        readHits[static_cast<std::size_t>(*kind)].Add(1);
        return true;
    }
    return false;
}

std::optional<Recorder::PendingReadInfo> Recorder::DescribePendingRead(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return std::nullopt;
    const auto end = address + bytes;
    if (open != nullptr) {
        if (const auto kind = readOverlap(*open, address, end)) return PendingReadInfo{submissions + 1, open->queue, *kind, true, false};
    }
    for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
        if (const auto kind = readOverlap(**it, address, end)) return PendingReadInfo{(*it)->serial, (*it)->queue, *kind, false, signaled(**it)};
    }
    return std::nullopt;
}

Recorder::ReadStatistics Recorder::ReadCounts() {
    ReadStatistics counts{readsNoted.Sum(), readQueries.Sum(), readStaleIgnored.Sum(), {}};
    for (std::size_t kind = 0; kind < ReadKinds; ++kind) counts.hits[kind] = readHits[kind].Sum();
    return counts;
}

std::optional<Recorder::PendingWriteInfo> Recorder::DescribePendingWrite(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return std::nullopt;
    const auto end = address + bytes;
    // The same choice SyncThrough makes: the open batch forces a full Sync, otherwise the newest
    // overlapping in-flight batch is the target. Of that batch's ranges the first overlapping one
    // is reported (a dispatch notes each written V# once, so it is the range the access hit).
    const auto firstOverlap = [&](const Batch& batch) -> const std::pair<std::uint64_t, std::uint64_t>* {
        for (const auto& range : batch.writes) {
            if (address < range.second && range.first < end) return &range;
        }
        return nullptr;
    };
    // With SyncThrough disabled every hit is a full Sync: the open batch (if any) is submitted and
    // everything in flight finishes, whichever batch noted the range.
    const bool syncAll = !SyncThroughEnabled();
    const auto allBatches = inFlight.size() + (open != nullptr ? 1 : 0);
    if (open != nullptr) {
        if (const auto* range = firstOverlap(*open)) return PendingWriteInfo{submissions + 1, true, false, range->first, range->second, inFlight.size() + 1};
    }
    std::size_t finished = inFlight.size();
    for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it, --finished) {
        const auto* range = firstOverlap(**it);
        if (range == nullptr) continue;
        // In flight means submitted, so the fence is live; signaled = the GPU already ran it.
        const bool signaled = fenceStatus((*it)->fence) == VK_SUCCESS;
        if (syncAll) return PendingWriteInfo{submissions + (open != nullptr ? 1 : 0), open != nullptr, signaled, range->first, range->second, allBatches};
        return PendingWriteInfo{(*it)->serial, false, signaled, range->first, range->second, finished};
    }
    return std::nullopt;
}

bool Recorder::HasCompletions() const {
    if (open != nullptr && !open->completions.empty()) return true;
    for (const auto& batch : inFlight) {
        if (!batch->completions.empty()) return true;
    }
    return false;
}

void Recorder::noteLabelOn(Batch& batch, std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue, bool behindCompletion) {
    // One entry per dword; a label overlapping older entries (a 4-byte store inside an 8-byte one or
    // the reverse) replaces exactly the dwords it stores, so a lookup composes what memory will hold.
    std::lock_guard tableLock(labelTableMutex);
    for (std::size_t offset = 0; offset + 4 <= bytes.size(); offset += 4) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data() + offset, 4);
        const auto dword = address + offset;
        labels.insert_or_assign(dword, LabelEntry{value, queue, stamp, &batch, 0, false, behindCompletion});
        batch.labelDwords.push_back(dword);
        LabelGroupDwords().emplace_back(dword, stamp);
    }
    recordedLabels.store(labels.size(), std::memory_order_relaxed);
}

void Recorder::markBehindCompletion(const Batch& batch, std::uint64_t begin, std::uint64_t end) {
    std::lock_guard tableLock(labelTableMutex);
    for (auto it = labels.lower_bound(begin); it != labels.end() && it->first < end; ++it) {
        if (it->second.batch == &batch) it->second.behindCompletion = true;
    }
}

void Recorder::NoteLabel(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue) {
    ensureOpen();
    // The table entry before the write note: the note bumps the write generation a poller
    // watches, and a poller that sees the bump then finds the label without the GPU mutex.
    const bool tabled = bytes.size() <= LabelTableBytes;
    if (tabled) {
        noteLabelOn(*open, address, bytes, stamp, queue);
    } else {
        std::lock_guard tableLock(labelTableMutex);
        labels.erase(labels.lower_bound(address), labels.lower_bound(address + bytes.size()));
        recordedLabels.store(labels.size(), std::memory_order_relaxed);
        open->wideLabels.push_back(wideLabels.emplace(address, WideLabel{address + bytes.size(), {bytes.begin(), bytes.end()}, ++wideLabelSequence, false, open.get()}));
        wideLabelBytes = std::max<std::uint64_t>(wideLabelBytes, bytes.size());
        recordedWideLabels.store(wideLabels.size(), std::memory_order_relaxed);
    }
    if (activeRecorder == this && pendingLabelSince.load(std::memory_order_relaxed) == NoPendingLabel) {
        pendingLabelSince.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }
    noteWriteOn(*open, address, bytes.size(), true);
}

std::optional<Recorder::LabelHit> Recorder::PendingLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, LabelRefusal* refusal) const {
    // Under the GPU mutex; the table mutex is taken too so this one path serves both lock regimes.
    std::lock_guard tableLock(labelTableMutex);
    return lookupLabel(address, bytes, afterStamp, refusal);
}

std::size_t Recorder::PendingLabels() const {
    std::lock_guard tableLock(labelTableMutex);
    return labels.size();
}

bool Recorder::PendingLabelIn(std::uint64_t address, std::size_t bytes) const {
    std::lock_guard tableLock(labelTableMutex);
    if (bytes == 0) return false;
    const auto end = address + bytes;
    if (const auto first = labels.lower_bound(address); first != labels.end() && first->first < end) return true;
    if (wideLabelInLocked(address, bytes)) return true;
    // A queued label of another queue is unordered against the caller on hardware (nothing that
    // queue recorded could have satisfied a wait yet); it is still reported, since the table mutex
    // is held anyway and a caller deciding a CPU store wants the conservative answer.
    if (const auto first = queuedLabels.lower_bound(address); first != queuedLabels.end() && first->first < end) return true;
    return false;
}

bool Recorder::CompletionLabelIn(std::uint64_t address, std::size_t bytes) const {
    std::lock_guard tableLock(labelTableMutex);
    if (bytes == 0) return false;
    const auto end = address + bytes;
    for (auto it = labels.lower_bound(address); it != labels.end() && it->first < end; ++it) {
        if (it->second.behindCompletion) return true;
    }
    if (const auto first = queuedLabels.lower_bound(address); first != queuedLabels.end() && first->first < end) return true;
    return false;
}

std::optional<Recorder::LabelHit> Recorder::lookupLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, LabelRefusal* refusal) const {
    if (refusal != nullptr) *refusal = LabelRefusal::None;
    if ((labels.empty() && queuedLabels.empty()) || (bytes != 4 && bytes != 8) || address % 4 != 0) return std::nullopt;
    LabelHit hit{0, 0, 0, false, std::numeric_limits<std::uint64_t>::max()};
    bool queued = false;
    bool candidate = false;
    const auto tag = GuestMemory::GpuLockThreadTag();
    const auto refuse = [&](LabelRefusal reason, std::uint64_t LateStatistics::*counter, const LabelEntry& entry) {
        if (refusal != nullptr) *refusal = entry.behindCompletion ? LabelRefusal::BehindCompletion : reason;
        if (counter != nullptr) ++(lateCounts.*counter);
        return std::nullopt;
    };
    for (std::size_t offset = 0; offset < bytes; offset += 4) {
        const auto dword = address + offset;
        const LabelEntry* entry = nullptr;
        // A queued label is only good for its own queue's wait (see NoteQueuedLabel), where it is
        // that queue's newest store of the dword; everyone else sees the recorded entry.
        if (const auto own = queuedLabels.find(dword); own != queuedLabels.end() && own->second.queue == tag) {
            entry = &own->second;
            queued = true;
        } else if (const auto found = labels.find(dword); found != labels.end()) {
            entry = &found->second;
            if (entry->batch == nullptr) {
                if (entry->queue != tag) return std::nullopt;
                queued = true;
            }
        }
        if (entry == nullptr) return std::nullopt;
        if (entry->stamp <= afterStamp) {
            // The late rule (see NoteLabel): counted in both modes, so a control run shows the population.
            if (!candidate) ++lateCounts.candidates;
            candidate = true;
            if (!LateTrustEnabled()) return refuse(LabelRefusal::TrustOff, nullptr, *entry);
            if (entry->batch == nullptr) return refuse(LabelRefusal::Queued, &LateStatistics::queued, *entry);
            if (entry->overwritten) return refuse(LabelRefusal::Overwritten, &LateStatistics::overwritten, *entry);
            if (entry->generation == 0) return refuse(LabelRefusal::Unclosed, &LateStatistics::unclosed, *entry);
            hit.late = true;
            hit.generation = std::min(hit.generation, entry->generation);
        }
        hit.value |= static_cast<std::uint64_t>(entry->value) << (offset * 8u);
        if (offset == 0) {
            hit.queue = entry->queue;
            hit.stamp = entry->stamp;
        }
    }
    if (!hit.late) hit.generation = 0;
    if (queued) queuedLabelHits.fetch_add(1, std::memory_order_relaxed);
    return hit;
}

bool Recorder::unsignaled(std::uint64_t serial) const {
    for (const auto& batch : inFlight) {
        if (batch->serial == serial) return fenceStatus(batch->fence) != VK_SUCCESS;
    }
    return false;
}

void Recorder::AfterCompletions(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue, bool storedOnGpu) {
    Require(open != nullptr || !inFlight.empty(), "no batch to append a completion label to");
    Batch& batch = open != nullptr ? *open : *inFlight.back();
    std::vector<std::byte> copy(bytes.begin(), bytes.end());
    // The store bypasses the flush hook: it runs inside finish() under the mutex, and the hook would
    // find this very range noted (or a later label to it) and sync re-entrantly. Stamped like every
    // driver store so a collect memoized for the packet sees it.
    static const bool always = std::getenv("APS5_LABEL_STORE_ALWAYS") != nullptr;
    const auto sequence = [&] {
        std::lock_guard ringLock(writtenBackMutex);
        return writtenBackSequence;
    }();
    batch.completions.push_back([this, address, copy = std::move(copy), sequence, storedOnGpu] {
        if (storedOnGpu && !always && !writtenBackSince(sequence, address, address + copy.size())) {
            completionStoresSkipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        completionStoresRun.fetch_add(1, std::memory_order_relaxed);
        GuestMemory::CheckRange(reinterpret_cast<const void*>(address), copy.size(), 4, true);
        GuestMemory::StoreOwnBytes(address, copy);
    });
    // Counted per batch and counted down when the batch finishes, on every exit path of finish()
    // (a label whose range became unmapped is reported there, and a lost device throws before the
    // completions run): a label that will never land must not leave the workers reaping forever.
    // One stored on the GPU already needs no reap unless a write-back overlaps it (the store above
    // is skipped otherwise), so it enters the count at that write-back (NoteWrittenBack).
    const bool behindCompletion = !(storedOnGpu && !always && !CountAllCompletionLabels());
    if (!behindCompletion) {
        batch.completionLabelRanges.push_back({address, address + bytes.size(), false});
    } else {
        ++batch.completionLabelCount;
        completionLabels.fetch_add(1, std::memory_order_acq_rel);
        ++batch.completionStoreCount;
        completionStores.fetch_add(1, std::memory_order_acq_rel);
        GuestMemory::BreakProofs();
    }
    // The table entry before the write note: the note bumps the generation a poller watches, and a
    // poller that sees the bump then finds the label without the GPU mutex.
    noteLabelOn(batch, address, bytes, stamp, queue, behindCompletion);
    noteWriteOn(batch, address, bytes.size(), true);
    if (&batch == open.get() && activeRecorder == this && pendingLabelSince.load(std::memory_order_relaxed) == NoPendingLabel) {
        pendingLabelSince.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }
}

bool Recorder::AfterRecordedWork(std::function<void()> action) {
    if (Idle()) return false;
    Batch& batch = open != nullptr ? *open : *inFlight.back();
    batch.completions.push_back(std::move(action));
    ++batch.completionLabelCount;
    completionLabels.fetch_add(1, std::memory_order_acq_rel);
        GuestMemory::BreakProofs();
    if (&batch == open.get() && activeRecorder == this && pendingLabelSince.load(std::memory_order_relaxed) == NoPendingLabel) {
        pendingLabelSince.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }
    return true;
}

void Recorder::NoteWrittenBack(std::uint64_t address, std::size_t bytes) {
    Recorder* recorder = activeRecorder;
    if (recorder == nullptr || bytes == 0) return;
    GuestMemory::AssertGpuLockHeld("Recorder::NoteWrittenBack");
    const auto end = address + bytes;
    {
        std::lock_guard ringLock(recorder->writtenBackMutex);
        recorder->writtenBack.push_back({++recorder->writtenBackSequence, address, end});
        while (recorder->writtenBack.size() > 16384) recorder->writtenBack.pop_front();
    }
    if (recorder->recordedLabels.load(std::memory_order_relaxed) != 0 && recorder->PendingLabelIn(address, bytes)) writeBacksOverLabels.fetch_add(1, std::memory_order_relaxed);
    // The GPU-stored completion labels this write-back overlaps now need their completion store,
    // so they enter the pending count (once each; finish() subtracts the batch's count). Batches
    // in `finishing` run their own completions in the same finish(), so only open and in-flight
    // ones are scanned.
    const auto count = [&](Batch& batch) {
        for (auto& label : batch.completionLabelRanges) {
            if (label.counted || label.begin >= end || address >= label.end) continue;
            label.counted = true;
            ++batch.completionLabelCount;
            completionLabels.fetch_add(1, std::memory_order_acq_rel);
        GuestMemory::BreakProofs();
            completionLabelsCountedLate.fetch_add(1, std::memory_order_relaxed);
            ++batch.completionStoreCount;
            completionStores.fetch_add(1, std::memory_order_acq_rel);
            recorder->markBehindCompletion(batch, label.begin, label.end);
        }
    };
    if (recorder->open != nullptr) count(*recorder->open);
    for (auto& batch : recorder->inFlight) count(*batch);
}

bool Recorder::writtenBackSince(std::uint64_t sequence, std::uint64_t begin, std::uint64_t end) {
    std::lock_guard ringLock(writtenBackMutex);
    if (writtenBack.empty() || writtenBack.back()[0] <= sequence) return false;
    if (writtenBack.front()[0] > sequence + 1) return true;
    for (auto it = writtenBack.rbegin(); it != writtenBack.rend() && (*it)[0] > sequence; ++it) {
        if ((*it)[1] < end && begin < (*it)[2]) return true;
    }
    return false;
}

void Recorder::HoldNextSubmit(VkSemaphore semaphore, std::uint64_t value) {
    submitHold = semaphore;
    submitHoldValue = value;
}

void Recorder::FailNextSubmitsForTest(unsigned count) {
    injectedSubmitFailures.store(count, std::memory_order_relaxed);
}

void Recorder::Submit() {
    PerformanceTimer frameTiming("Recorder.Submit");
    // The work count is cleared even when nothing is open: the driver counts a dispatch after its
    // call returns (outside the mutex), so a submit by another thread in between leaves a stale
    // count behind, and the callers that act on it would otherwise take the mutex for nothing at
    // every packet or submission end until a real batch is next submitted.
    if (activeRecorder == this) workSinceSubmit.store(0, std::memory_order_relaxed);
    dropStaleHoldKept();
    if (open == nullptr) return;
    GuestMemory::AssertGpuLockHeld("Recorder::Submit");
    flushDeferredDispatch();
    if (!open->keyStores.empty()) recordKeyStores(false);
    if (open->renderPass.open) endOpenRenderPass(PassBreak::NewBatch);
    const bool segments = segmentMode();
    if (!segments) {
        endSamples(*open);
        if (pendingSamples.size() >= SampleFoldCapacity) foldSamples(*open, 0);
    }
    const bool hostReadCovered = (open->run.open || !open->run.queued.empty()) && closeStoreRun(true);
    if (segments && pendingSamples.size() >= SampleFoldCapacity) foldSegments();
    if (BarrierValidate()) validateBatchEnds.fetch_add(1, std::memory_order_relaxed);
    if (open->hostReadOwed && !hostReadCovered) {
        // A fence makes nothing visible to the host: a lean draw's writes (no download barrier of
        // its own) get the batch's one here.
        recordBarrier(open->commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        CountBarriers(CommandClass::Draw);
    }
    EndGpuTiming(open->batchTiming);
    if (!GpuTimingEnabled()) reportBarriers();
    if (FlipReadCheck()) {
        // A CPU write into a noted read after this collect (and so possibly before the GPU read
        // it) fails UnchangedSince at the presenter's check (VulkanDevice RetirePresents).
        for (const auto& [begin, end] : notedReads(*open)) open->readGeneration = std::max(open->readGeneration, GuestMemory::CollectWrites(begin, end - begin));
    }
    auto batch = std::move(open);
    struct DiscardUnsubmitted {
        Recorder& recorder;
        std::unique_ptr<Batch>& batch;
        ~DiscardUnsubmitted() {
            if (batch != nullptr && !batch->submitted) recorder.discard(std::move(batch));
        }
    } discardUnsubmitted{*this, batch};
    Check(function(endCommandBuffer, "vkEndCommandBuffer")(batch->commands), "vkEndCommandBuffer recorder");
    if (injectedSubmitFailures.load(std::memory_order_relaxed) != 0 && injectedSubmitFailures.fetch_sub(1, std::memory_order_relaxed) != 0) throw std::runtime_error("vkQueueSubmit recorder: injected failure");
    VkSubmitInfo submission{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submission.commandBufferCount = 1;
    submission.pCommandBuffers = &batch->commands;
    // The timeline reaches this batch's serial when it completes (see WaitSerial).
    const std::uint64_t serial = submissions + 1;
    if (CheckpointMarkersEnabled() && batch->queue == 0 && batch->predictedSerial != 0) {
        Require(batch->predictedSerial == serial, "Recorder::Submit submit serial mismatch");
        try {
            if (const auto snapshot = GuestBufferMemory::TrySnapshotAddressSpace(); snapshot.has_value()) AgcDriver::SubmissionSnapshots::DeviceSnapshots().NoteSubmit(serial, snapshot->first, snapshot->second, AgcDriver::SubmitBracket{serial, CompletedThrough(), batch->firstToken, batch->lastToken});
        } catch (...) {
        }
    }
    std::string tokensStr = "[";
    for (std::size_t i = 0; i < batch->drawTraceTokens.size(); ++i) {
        if (i > 0) tokensStr += ",";
        tokensStr += std::to_string(batch->drawTraceTokens[i]);
    }
    tokensStr += "]";
    CaptureTrace::Log("submit batch=%llu tokens=%s", static_cast<unsigned long long>(serial), tokensStr.c_str());
    VkTimelineSemaphoreSubmitInfoKHR timelineInfo{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR};
    const VkSemaphore hold = std::exchange(submitHold, VK_NULL_HANDLE);
    const std::uint64_t holdValue = submitHoldValue;
    VkPipelineStageFlags holdStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (hold != VK_NULL_HANDLE) {
        submission.waitSemaphoreCount = 1;
        submission.pWaitSemaphores = &hold;
        submission.pWaitDstStageMask = &holdStage;
        timelineInfo.waitSemaphoreValueCount = 1;
        timelineInfo.pWaitSemaphoreValues = &holdValue;
    }
    if (timeline != VK_NULL_HANDLE) {
        timelineInfo.signalSemaphoreValueCount = 1;
        timelineInfo.pSignalSemaphoreValues = &serial;
        submission.signalSemaphoreCount = 1;
        submission.pSignalSemaphores = &timeline;
    }
    if (hold != VK_NULL_HANDLE || timeline != VK_NULL_HANDLE) submission.pNext = &timelineInfo;
    DestroyDeferredAtIdle(context);
    const auto submitStart = std::chrono::steady_clock::now();
    batch->submitStarted.store(true, std::memory_order_release);
    const auto result = function(queueSubmit, "vkQueueSubmit")(context.queue, 1, &submission, batch->fence);
    if (result == VK_ERROR_DEVICE_LOST) {
        const auto idle = context.Function<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(context.device);
        if (idle != VK_SUCCESS && idle != VK_ERROR_DEVICE_LOST) std::terminate();
    }
    Check(result, "vkQueueSubmit recorder");
    if (batch->guestWork) LibcRunReportMilestone_nid_no_patch(2);
    batch->submitted = true;
    batch->serial = ++submissions;
    batch->submittedAt = std::chrono::steady_clock::now();
    CaptureTrace::Log("submit-confirmed batch=%llu tokens=%s", static_cast<unsigned long long>(batch->serial), tokensStr.c_str());
    if (DrawProfiled()) {
        const auto us = std::chrono::duration<double, std::micro>(batch->submittedAt - submitStart).count();
        ++submitCount;
        submitUs += us;
        submitMaxUs = std::max(submitMaxUs, us);
        if (auto* frame = PerformanceContext::Current()) frame->Add(frame->Get("Recorder", "submit"), batch->submittedAt - submitStart);
    }
    {
        std::lock_guard lock(completedMutex);
        newestSubmitted = batch->serial;
        newestSubmittedAt = batch->submittedAt;
    }
    if (timeline != VK_NULL_HANDLE) {
        if (!progressWatcher) {
            progressWatcher = std::make_unique<ProgressWatcher>(context.device, timeline, context.Function<PFN_vkWaitSemaphoresKHR>("vkWaitSemaphoresKHR"),
                                                                context.Function<PFN_vkGetSemaphoreCounterValueKHR>("vkGetSemaphoreCounterValueKHR"));
        }
        progressWatcher->Submitted(batch->serial);
    }
    inFlight.push_back(std::move(batch));
    inFlightKeptBytes += inFlight.back()->keptBytes;
    if (activeRecorder == this) pendingLabelSince.store(NoPendingLabel, std::memory_order_release);
}

}
}
}
}
}
}
}
