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
void Recorder::OnComplete(std::function<void()> action) {
    ensureOpen();
    open->completions.push_back(std::move(action));
    ++open->writeBackCompletionCount;
    writeBackCompletions.fetch_add(1, std::memory_order_acq_rel);
}

void Recorder::OnWriteComplete(std::uint64_t address, std::size_t bytes, std::function<void()> action) {
    const auto end = address + bytes;
    Batch* holder = open != nullptr && overlaps(*open, address, end) ? open.get() : nullptr;
    for (auto batch = inFlight.rbegin(); holder == nullptr && batch != inFlight.rend(); ++batch) {
        if (overlaps(**batch, address, end)) holder = batch->get();
    }
    if (holder == nullptr) {
        action();
        return;
    }
    holder->completions.push_back(std::move(action));
    ++holder->writeBackCompletionCount;
    writeBackCompletions.fetch_add(1, std::memory_order_acq_rel);
}

bool Recorder::noteWrite(std::uint64_t address, std::size_t bytes, bool ownLabel, int value, std::int64_t word) {
    CaptureTrace::Log("buffer-write batch=%llu address=%llx bytes=%zu label=%d", static_cast<unsigned long long>(submissions + 1), static_cast<unsigned long long>(address), bytes, ownLabel);
    if (bytes == 0) return false;
    GuestMemory::ForgetKnownZero(address, bytes);
    ensureOpen();
    const auto end = address + bytes;
    open->writes.emplace_back(address, end);
    open->writeNotes.push_back(++writeNoteCount);
    if (published != nullptr && !SnapshotCovers(published.get(), address, end)) published = nullptr;
    open->writeValues.push_back(static_cast<std::int16_t>(value));
    open->writeWords.push_back(word);
    if (!ownLabel) markOverwritten(address, end);
    // A poller waiting on this range learns that the open batch may now hold its producer.
    if (activeRecorder == this) writeGeneration.fetch_add(1, std::memory_order_release);
    // Consecutive dispatches write the same output buffers: a range the snapshot already contains
    // leaves the published union unchanged, so the O(N log N) rebuild is skipped.
    if (SnapshotCovers(address, end)) {
        ++snapshotCovered;
        return false;
    }
    return true;
}

void Recorder::noteWriteOn(Batch& batch, std::uint64_t address, std::size_t bytes, bool ownLabel) {
    if (bytes == 0) return;
    if (&batch != open.get()) GuestMemory::ForgetKnownZero(address, bytes);
    if (&batch == open.get()) {
        if (!noteWrite(address, bytes, ownLabel)) return;
        const std::array ranges{std::pair{address, address + bytes}};
        publishPendingWrites(ranges);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return;
    }
    // An in-flight batch: its range joins the snapshot at once (the completion that stores it runs
    // when the batch finishes, and the hook must sync for a CPU read until then). The generation
    // moves as well, so a poller re-consults the label table for a completion label.
    batch.writes.emplace_back(address, address + bytes);
    batch.writeNotes.push_back(++writeNoteCount);
    if (published != nullptr && !SnapshotCovers(published.get(), address, address + bytes)) published = nullptr;
    batch.writeValues.push_back(-1);
    batch.writeWords.push_back(-1);
    if (!ownLabel) markOverwritten(address, address + bytes);
    if (activeRecorder == this) writeGeneration.fetch_add(1, std::memory_order_release);
    if (!SnapshotCovers(address, address + bytes)) {
        const std::array ranges{std::pair{address, address + bytes}};
        publishPendingWrites(ranges);
    }
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recorder::markOverwritten(std::uint64_t address, std::uint64_t end) {
    if (recordedLabels.load(std::memory_order_relaxed) == 0 && recordedWideLabels.load(std::memory_order_relaxed) == 0) return;
    std::lock_guard tableLock(labelTableMutex);
    // A dword overlaps the range when it starts before the end and ends after the start.
    for (auto it = labels.lower_bound(address >= 3 ? address - 3 : 0); it != labels.end() && it->first < end; ++it) it->second.overwritten = true;
    for (auto it = wideLabels.lower_bound(address >= wideLabelBytes ? address - wideLabelBytes + 1 : 0); it != wideLabels.end() && it->first < end; ++it) {
        if (it->second.end > address) it->second.overwritten = true;
    }
}

void Recorder::NotePendingWrite(std::uint64_t address, std::size_t bytes) {
    if (!noteWrite(address, bytes)) return;
    const std::array ranges{std::pair{address, address + bytes}};
    publishPendingWrites(ranges);
    // The note precedes this thread's vkQueueSubmit and the label another queue polls for; the
    // fence makes that order hold without relying on x86 store ordering.
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recorder::NotePendingFill(std::uint64_t address, std::size_t bytes, std::uint8_t value) {
    if (!noteWrite(address, bytes, false, value, value * std::int64_t{0x01010101})) return;
    const std::array ranges{std::pair{address, address + bytes}};
    publishPendingWrites(ranges);
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recorder::NotePendingWordFill(std::uint64_t address, std::size_t bytes, std::uint32_t word) {
    const int value = word == (word & 0xffu) * 0x01010101u ? static_cast<int>(word & 0xffu) : -1;
    if (!noteWrite(address, bytes, false, value, word)) return;
    const std::array ranges{std::pair{address, address + bytes}};
    publishPendingWrites(ranges);
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recorder::NotePendingWrites(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    notePendingRanges(ranges, -1);
}

void Recorder::NotePendingReach(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    notePendingRanges(ranges, ReachValue);
}

void Recorder::notePendingRanges(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges, int value) {
    bool publish = false;
    for (const auto& [begin, end] : ranges) {
        if (end > begin && noteWrite(begin, static_cast<std::size_t>(end - begin), false, value)) publish = true;
    }
    if (!publish) return;
    publishPendingWrites(ranges);
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recorder::NoteStampedWrites(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    const auto first = writeNoteCount + 1;
    NotePendingWrites(ranges);
    if (open == nullptr) return;
    for (auto note = first; note <= writeNoteCount; ++note) open->stampedNotes.push_back(note);
}

void Recorder::publishPendingWrites(std::span<const std::pair<std::uint64_t, std::uint64_t>> additions) const {
    if (activeRecorder != this) {
        publishedWriteRangesValid = false;
        return;
    }
    const auto started = DrawProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto previous = pendingWrites.load(std::memory_order_acquire);
    const auto owned = publishedWriteRanges.lock();
    const bool extend = !additions.empty() && previous != nullptr && previous == owned && publishedWriteRangesValid &&
        std::all_of(additions.begin(), additions.end(), [](const auto& range) { return range.second > range.first; });
    publishedWriteRangesValid = false;
    auto merged = std::make_shared<WriteRanges>();
    if (extend && additions.size() == 1) {
        auto range = additions.front();
        const auto first = std::partition_point(previous->begin(), previous->end(), [&](const auto& entry) { return entry.second < range.first; });
        auto last = first;
        while (last != previous->end() && last->first <= range.second) {
            range.first = std::min(range.first, last->first);
            range.second = std::max(range.second, last->second);
            ++last;
        }
        merged->reserve(previous->size() - static_cast<std::size_t>(last - first) + 1);
        merged->insert(merged->end(), previous->begin(), first);
        merged->push_back(range);
        merged->insert(merged->end(), last, previous->end());
    } else {
        if (extend) {
            merged->reserve(previous->size() + additions.size());
            merged->insert(merged->end(), previous->begin(), previous->end());
            merged->insert(merged->end(), additions.begin(), additions.end());
        } else {
            if (open != nullptr) merged->insert(merged->end(), open->writes.begin(), open->writes.end());
            for (const auto& batch : inFlight) merged->insert(merged->end(), batch->writes.begin(), batch->writes.end());
            for (const auto* batch : finishing) merged->insert(merged->end(), batch->writes.begin(), batch->writes.end());
        }
        std::sort(merged->begin(), merged->end());
        std::size_t out = 0;
        for (const auto& [begin, end] : *merged) {
            if (out != 0 && begin <= (*merged)[out - 1].second) (*merged)[out - 1].second = std::max((*merged)[out - 1].second, end);
            else (*merged)[out++] = {begin, end};
        }
        merged->resize(out);
    }
    publishedWriteRangesValid = std::all_of(merged->begin(), merged->end(), [](const auto& range) { return range.second > range.first; });
    publishedWriteRanges = merged;
    published = merged;
    pendingWrites.store(std::move(merged), std::memory_order_release);
    publishGeneration.fetch_add(1, std::memory_order_release);
    ++snapshotRebuilds;
    if (DrawProfiled()) snapshotRebuildMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

bool Recorder::overlaps(const Batch& batch, std::uint64_t address, std::uint64_t end) {
    for (const auto& [begin, finish] : batch.writes) {
        if (address < finish && begin < end) return true;
    }
    return false;
}

bool Recorder::PendingWriteOverlaps(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return false;
    if (published != nullptr && !SnapshotOverlaps(published.get(), address, bytes)) return false;
    const auto end = address + bytes;
    if (open != nullptr && overlaps(*open, address, end)) return true;
    for (const auto& batch : inFlight) {
        if (overlaps(*batch, address, end)) return true;
    }
    return false;
}

bool Recorder::PendingStoreOverlaps(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return false;
    if (published != nullptr && !SnapshotOverlaps(published.get(), address, bytes)) return false;
    const auto end = address + bytes;
    const auto stores = [&](const Batch& batch) {
        for (std::size_t index = 0; index < batch.writes.size(); ++index) {
            if (batch.writeValues[index] != ReachValue && address < batch.writes[index].second && batch.writes[index].first < end) return true;
        }
        return false;
    };
    if (open != nullptr && stores(*open)) return true;
    return std::any_of(inFlight.begin(), inFlight.end(), [&](const auto& batch) { return stores(*batch); });
}

bool Recorder::OpenWriteOverlaps(std::uint64_t address, std::size_t bytes) const {
    return bytes != 0 && open != nullptr && overlaps(*open, address, address + bytes);
}

std::uint64_t Recorder::PendingWriterSerial(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return 0;
    const auto end = address + bytes;
    const auto writes = [&](const Batch& batch) { return overlaps(batch, address, end) || readOverlap(batch, address, end, ReadKind::AddressBased).has_value(); };
    if (open != nullptr && writes(*open)) return OpenSerial();
    for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
        if (writes(**it)) return (*it)->serial;
    }
    return 0;
}

std::uint64_t Recorder::PendingWriterSerial(std::uint64_t address, std::size_t bytes, std::span<const std::uint64_t> exceptNotes) const {
    if (exceptNotes.empty()) return PendingWriterSerial(address, bytes);
    if (bytes == 0) return 0;
    const auto end = address + bytes;
    const auto writes = [&](const Batch& batch) {
        for (std::size_t index = 0; index < batch.writes.size(); ++index) {
            const auto& [begin, finish] = batch.writes[index];
            if (address >= finish || begin >= end) continue;
            if (index < batch.writeNotes.size() && std::find(exceptNotes.begin(), exceptNotes.end(), batch.writeNotes[index]) != exceptNotes.end()) continue;
            return true;
        }
        return readOverlap(batch, address, end, ReadKind::AddressBased).has_value();
    };
    if (open != nullptr && writes(*open)) return OpenSerial();
    for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
        if (writes(**it)) return (*it)->serial;
    }
    return 0;
}

std::uint64_t Recorder::PendingLeaseSerial(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return 0;
    const auto end = address + bytes;
    const auto leases = [&](const Batch& batch) { return readOverlap(batch, address, end, ReadKind::AddressBased).has_value(); };
    if (open != nullptr && leases(*open)) return OpenSerial();
    for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
        if (leases(**it)) return (*it)->serial;
    }
    return 0;
}

bool Recorder::signaled(const Batch& batch) const {
    return batch.submitted && fenceStatus(batch.fence) == VK_SUCCESS;
}

bool Recorder::PendingWriteSettled(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return true;
    const auto end = address + bytes;
    if (open != nullptr && overlaps(*open, address, end)) return false;
    for (const auto& batch : inFlight) {
        if (overlaps(*batch, address, end) && !signaled(*batch)) return false;
    }
    return true;
}

std::vector<std::uint64_t> Recorder::StampedNotesSince(std::uint64_t note) const {
    std::vector<std::uint64_t> notes;
    const auto collect = [&](const Batch& batch) {
        for (const auto stamped : batch.stampedNotes) {
            if (stamped > note) notes.push_back(stamped);
        }
    };
    for (const auto& batch : inFlight) collect(*batch);
    if (open != nullptr) collect(*open);
    return notes;
}

std::uint64_t Recorder::LastWriteNote(std::uint64_t address, std::size_t bytes) const {
    if (open == nullptr || open->writes.empty() || open->writeNotes.size() != open->writes.size()) return 0;
    if (open->writes.back() != std::pair<std::uint64_t, std::uint64_t>{address, address + bytes}) return 0;
    return open->writeNotes.back() == writeNoteCount ? writeNoteCount : 0;
}

std::uint64_t Recorder::NewestWriteNote(std::uint64_t address, std::size_t bytes) const {
    return newestNote(address, bytes, true);
}

std::uint64_t Recorder::NewestStoreNote(std::uint64_t address, std::size_t bytes) const {
    return newestNote(address, bytes, false);
}

std::uint64_t Recorder::newestNote(std::uint64_t address, std::size_t bytes, bool reach) const {
    if (bytes == 0) return 0;
    const auto end = address + bytes;
    std::uint64_t newest = 0;
    const auto scan = [&](const Batch& batch) {
        for (std::size_t i = 0; i < batch.writes.size(); ++i) {
            if ((reach || batch.writeValues[i] != ReachValue) && address < batch.writes[i].second && batch.writes[i].first < end) newest = std::max(newest, batch.writeNotes[i]);
        }
    };
    if (open != nullptr) scan(*open);
    for (const auto& batch : inFlight) scan(*batch);
    return newest;
}

std::vector<Recorder::PendingWrite> Recorder::PendingWritesOver(std::uint64_t address, std::size_t bytes) const {
    std::vector<PendingWrite> found;
    if (bytes == 0) return found;
    const auto end = address + bytes;
    const auto scan = [&](const Batch& batch) {
        for (std::size_t i = 0; i < batch.writes.size(); ++i) {
            if (address < batch.writes[i].second && batch.writes[i].first < end) found.push_back({batch.writes[i].first, batch.writes[i].second, batch.writeNotes[i], batch.writeValues[i], batch.writeWords[i]});
        }
    };
    if (open != nullptr) scan(*open);
    for (const auto& batch : inFlight) scan(*batch);
    return found;
}

void Recorder::PendingWriteRanges(std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) const {
    ranges.clear();
    if (open != nullptr) ranges.insert(ranges.end(), open->writes.begin(), open->writes.end());
    for (const auto& batch : inFlight) ranges.insert(ranges.end(), batch->writes.begin(), batch->writes.end());
}

bool Recorder::ReadTracking() {
    return ReadTrackingEnabled();
}

void Recorder::NotePendingRead(std::uint64_t address, std::size_t bytes, ReadKind kind) {
    CaptureTrace::Log("buffer-read batch=%llu address=%llx bytes=%zu kind=%d", static_cast<unsigned long long>(submissions + 1), static_cast<unsigned long long>(address), bytes, static_cast<int>(kind));
    if (bytes == 0 || !ReadTrackingEnabled()) return;
    ensureOpen();
    open->reads.push_back({address, address + bytes, kind});
    readsNoted.Add(1);
}

void Recorder::NotePendingReads(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges, ReadKind kind) {
    if (ranges.empty() || !ReadTrackingEnabled()) return;
    ensureOpen();
    for (const auto& [begin, end] : ranges) {
        CaptureTrace::Log("buffer-read batch=%llu address=%llx bytes=%llu kind=%d", static_cast<unsigned long long>(submissions + 1), static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), static_cast<int>(kind));
        if (end > begin) open->reads.push_back({begin, end, kind});
    }
    readsNoted.Add(ranges.size());
}

bool Recorder::ValidReadList(const ReadList& ranges) {
    for (std::size_t index = 0; index < ranges.size(); ++index) {
        if (ranges[index].second <= ranges[index].first) return false;
        if (index != 0 && ranges[index].first < ranges[index - 1].second) return false;
    }
    return true;
}

void Recorder::NotePendingReadList(std::shared_ptr<const ReadList> ranges, ReadKind kind) {
    if (ranges == nullptr || ranges->empty() || !ReadTrackingEnabled()) return;
    if (CaptureTrace::Enabled() || FlipReadCheck()) {
        NotePendingReads(*ranges, kind);
        return;
    }
    ensureOpen();
    readsNoted.Add(ranges->size());
    for (const auto& note : open->readLists) {
        if (note.ranges == ranges && note.kind == kind) return;
    }
    open->readLists.push_back({std::move(ranges), open->reads.size(), kind});
}

std::optional<Recorder::ReadKind> Recorder::readOverlap(const Batch& batch, std::uint64_t address, std::uint64_t end, std::optional<ReadKind> only) {
    const auto listOverlaps = [&](const Batch::ReadListNote& note) {
        if (only.has_value() && note.kind != *only) return false;
        const auto& list = *note.ranges;
        const auto found = std::partition_point(list.begin(), list.end(), [&](const auto& range) { return range.second <= address; });
        return found != list.end() && found->first < end;
    };
    std::size_t next = 0;
    for (std::size_t index = 0; index < batch.reads.size(); ++index) {
        for (; next < batch.readLists.size() && batch.readLists[next].position == index; ++next) {
            if (listOverlaps(batch.readLists[next])) return batch.readLists[next].kind;
        }
        const auto& read = batch.reads[index];
        if ((!only.has_value() || read.kind == *only) && address < read.end && read.begin < end) return read.kind;
    }
    for (; next < batch.readLists.size(); ++next) {
        if (listOverlaps(batch.readLists[next])) return batch.readLists[next].kind;
    }
    return std::nullopt;
}

}
}
}
}
}
}
}
