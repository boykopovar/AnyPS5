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
void ReportHookSyncs(HookSyncStats& stats) {
    std::vector<std::pair<const HookSyncKey*, const HookSyncTotals*>> hot;
    hot.reserve(stats.byKey.size());
    for (const auto& [key, totals] : stats.byKey) hot.emplace_back(&key, &totals);
    std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second->waitedMs > b.second->waitedMs; });
    std::string report;
    char text[512];
    const auto count = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
    const auto& o = stats.outcomes;
    std::snprintf(text, sizeof(text), "[hooksync] %llu pending-write syncs waited %.0f ms (10 s); read bytes after the wait: unchanged %llu (target open %llu, in flight unsignaled %llu, signaled %llu), changed %llu; stores %llu, unchecked %llu; targets: %llu open, %llu already signaled, %llu batches finished; noted range:", count(stats.count), stats.waitedMs, count(o.unchangedOpen + o.unchangedPending + o.unchangedSignaled), count(o.unchangedOpen), count(o.unchangedPending), count(o.unchangedSignaled), count(o.changed), count(o.stores), count(o.unchecked), count(stats.openTargets), count(stats.signaledTargets), count(stats.batchesFinished));
    report += text;
    for (std::size_t i = 0; i < SizeBuckets; ++i) {
        if (stats.rangeBuckets[i] == 0) continue;
        std::snprintf(text, sizeof(text), " %s %llu", SizeBucketNames[i], static_cast<unsigned long long>(stats.rangeBuckets[i]));
        report += text;
    }
    report += "; access:";
    for (std::size_t i = 0; i < SizeBuckets; ++i) {
        if (stats.accessBuckets[i] == 0) continue;
        std::snprintf(text, sizeof(text), " %s %llu", SizeBucketNames[i], static_cast<unsigned long long>(stats.accessBuckets[i]));
        report += text;
    }
    report += "; top by wait (queue packet site frames: count/ms, unchanged open/unsignaled/signaled, changed, stores, avg range/access):";
    for (std::size_t i = 0; i < hot.size() && i < 10; ++i) {
        const auto& key = *hot[i].first;
        const auto& totals = *hot[i].second;
        const auto& k = totals.outcomes;
        std::snprintf(text, sizeof(text), " [0x%x %s %s +0x%llx/+0x%llx/+0x%llx/+0x%llx/+0x%llx/+0x%llx: %llu/%.0fms u%llu/%llu/%llu c%llu s%llu %.0fK/%.0fK]", key.queue, PacketName(key.opcode).c_str(), GuestMemory::ReadSiteName(key.site), key.frames[0], key.frames[1], key.frames[2], key.frames[3], key.frames[4], key.frames[5], count(totals.count), totals.waitedMs, count(k.unchangedOpen), count(k.unchangedPending), count(k.unchangedSignaled), count(k.changed), count(k.stores), totals.rangeBytes / 1024.0 / totals.count, totals.accessBytes / 1024.0 / totals.count);
        report += text;
    }
    AgcDriver::ProfilePrint_nid_no_patch("%s\n", report.c_str());
    stats = HookSyncStats{};
}

// Wraps one pending-write sync (constructed before it, under the GpuMutex): gathers what the sync
// waits for and, in the destructor, the time it took and whether the bytes changed.
class HookSyncScope {
public:
    HookSyncScope(const Recorder& recorder, std::uint64_t address, std::size_t bytes) : enabled(HookSyncProfiled()), address(address), bytes(bytes) {
        if (!enabled) return;
        info = recorder.DescribePendingWrite(address, bytes);
        const auto packet = GuestMemory::CurrentPacket();
        key = HookSyncKey{packet.queue, packet.opcode, GuestMemory::CurrentReadSite(), {}};
        // Frame 0 is the hook's caller (the GuestMemory entry, or the driver code when the entry
        // was inlined); the driver code that made the access is one of the next frames.
        GuestMemory::CaptureCallerOffsets(key.frames, 1);
        // The access's bytes are copied raw: a Read here would re-enter the hook. The same access
        // check the accessing code makes guards the copy; a range that fails it is left unchecked.
        // A store's bytes are not compared (counted as a store instead).
        if (!GuestMemory::WritesGuestMemory(key.site) && bytes <= CompareLimit && GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) {
            before.resize(bytes);
            std::memcpy(before.data(), reinterpret_cast<const void*>(address), bytes);
        }
        start = std::chrono::steady_clock::now();
    }
    ~HookSyncScope() {
        if (!enabled) return;
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration<double, std::milli>(now - start).count();
        auto& stats = HookSyncs();
        auto& totals = stats.byKey[key];
        ++totals.count;
        totals.waitedMs += ms;
        totals.accessBytes += bytes;
        ++stats.count;
        stats.waitedMs += ms;
        stats.accessBuckets[SizeBucket(bytes)] += 1;
        if (info.has_value()) {
            const auto rangeBytes = info->rangeEnd - info->rangeBegin;
            totals.rangeBytes += rangeBytes;
            stats.rangeBuckets[SizeBucket(rangeBytes)] += 1;
            if (info->open) ++stats.openTargets;
            if (info->signaled) ++stats.signaledTargets;
            stats.batchesFinished += info->batchesToFinish;
        }
        // One outcome per sync, on both the key's and the interval's counters.
        const auto outcome = [&]() -> std::uint64_t HookSyncOutcomes::* {
            if (GuestMemory::WritesGuestMemory(key.site)) return &HookSyncOutcomes::stores;
            if (before.empty() || !GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) return &HookSyncOutcomes::unchecked;
            if (std::memcmp(before.data(), reinterpret_cast<const void*>(address), bytes) != 0) return &HookSyncOutcomes::changed;
            // A target already signaled (or no target found: the batch is finishing) had run
            // before the copy, so its unchanged bytes are not evidence of a false overlap.
            if (!info.has_value() || info->signaled) return &HookSyncOutcomes::unchangedSignaled;
            return info->open ? &HookSyncOutcomes::unchangedOpen : &HookSyncOutcomes::unchangedPending;
        }();
        ++(totals.outcomes.*outcome);
        ++(stats.outcomes.*outcome);
        if (now - stats.lastReport > std::chrono::seconds(10)) ReportHookSyncs(stats);
    }
    HookSyncScope(const HookSyncScope&) = delete;
    HookSyncScope& operator=(const HookSyncScope&) = delete;

private:
    static constexpr std::size_t CompareLimit = 65536;
    bool enabled;
    std::uint64_t address;
    std::size_t bytes;
    std::optional<Recorder::PendingWriteInfo> info;
    HookSyncKey key{};
    std::vector<std::byte> before;
    std::chrono::steady_clock::time_point start;
};

// Attribution of the hook's recorded-store syncs (source 2: a pending image's write-back or a unit
// publish was recorded for the access and SyncThrough waits for it), the [hooksync] recorded-store
// line every 10 s under the same switch as HookSyncScope: per (queue, packet, read site), the syncs,
// their wait, the batches in flight at the sync, the C4 classification of the access (every 64 KiB
// block CPU-written since the overlapping images' generations: the store would not change a byte
// the CPU reads), whether the bytes changed over the wait, whether the images overlapped had a
// consumer proof within StorageTexture::DeadImagePresents presents, and the accesses the C4 skip
// took instead of a flush (with the images that left the registry there as an empty write-back
// would have left them). The skips are counted without the GpuMutex (their own mutex); the syncs
// under it.
struct RecordedStoreTotals {
    std::uint64_t count = 0;
    double waitedMs = 0;
    std::uint64_t batchesFinished = 0;
    std::uint64_t cpuWrittenYes = 0, cpuWrittenNo = 0, unclassified = 0, publishOnly = 0;
    std::uint64_t changed = 0, unchanged = 0, unchecked = 0;
    std::uint64_t imagesDead = 0, imagesLive = 0;
    std::uint64_t skipped = 0, flushed = 0, evicted = 0;
};

struct RecordedStoreStats {
    AgcDriver::Mutex mutex;
    std::map<HookSyncKey, RecordedStoreTotals> byKey;
    RecordedStoreTotals totals;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

RecordedStoreStats& RecordedStoreSyncs() {
    static RecordedStoreStats stats;
    return stats;
}

void ReportRecordedStoreSyncs(RecordedStoreStats& stats) {
    std::vector<std::pair<const HookSyncKey*, const RecordedStoreTotals*>> hot;
    for (const auto& [key, totals] : stats.byKey) hot.emplace_back(&key, &totals);
    std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return std::tie(a.second->waitedMs, a.second->skipped) > std::tie(b.second->waitedMs, b.second->skipped); });
    const auto count = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
    const auto& t = stats.totals;
    const auto skips = StorageTexture::TakeHookSkipCounts();
    char text[640];
    std::snprintf(text, sizeof(text), "[hooksync] recorded-store syncs (10 s): %llu waited %.0f ms, %llu batches in flight at them; skipped (cpu-written blocks) %llu / flushed %llu / evicted stale %llu; flushed later after a skip %llu (by the same site %llu); C4 every block CPU-written: yes %llu / no %llu / unclassified %llu (publish only %llu); bytes after the wait: changed %llu, unchanged %llu, unchecked %llu; images overlapped: dead %llu / live %llu (no consumer proof for %llu presents); by queue packet site (count/ms, skipped, cpu-written yes/no, changed/unchanged, dead/live):", count(t.count), t.waitedMs, count(t.batchesFinished), count(t.skipped), count(t.flushed), count(t.evicted), count(skips.flushedAfterSkip), count(skips.flushedAfterSkipSameSite), count(t.cpuWrittenYes), count(t.cpuWrittenNo), count(t.unclassified), count(t.publishOnly), count(t.changed), count(t.unchanged), count(t.unchecked), count(t.imagesDead), count(t.imagesLive), count(StorageTexture::DeadImagePresents));
    std::string report = text;
    for (std::size_t i = 0; i < hot.size() && i < 10; ++i) {
        const auto& key = *hot[i].first;
        const auto& k = *hot[i].second;
        std::snprintf(text, sizeof(text), " [0x%x %s %s +0x%llx/+0x%llx/+0x%llx: %llu/%.0fms k%llu y%llu/n%llu c%llu/u%llu d%llu/l%llu]", key.queue, PacketName(key.opcode).c_str(), GuestMemory::ReadSiteName(key.site), key.frames[0], key.frames[1], key.frames[2], count(k.count), k.waitedMs, count(k.skipped), count(k.cpuWrittenYes), count(k.cpuWrittenNo), count(k.changed), count(k.unchanged), count(k.imagesDead), count(k.imagesLive));
        report += text;
    }
    AgcDriver::ProfilePrint_nid_no_patch("%s\n", report.c_str());
    stats.byKey.clear();
    stats.totals = {};
}

// The key of the access the hook handles now: always inlined into FlushForAccess so that frame 0
// is the hook's caller for the skip counters and the sync scope alike, whatever the inliner does
// with the scope's constructor.
[[gnu::always_inline]] inline HookSyncKey RecordedStoreKey() {
    const auto packet = GuestMemory::CurrentPacket();
    HookSyncKey key{packet.queue, packet.opcode, GuestMemory::CurrentReadSite(), {}};
    GuestMemory::CaptureCallerOffsets(key.frames, 1);
    return key;
}

void CountRecordedStoreSkip(const HookSyncKey& key, std::size_t evicted) {
    auto& stats = RecordedStoreSyncs();
    std::lock_guard lock(stats.mutex);
    for (auto* totals : {&stats.byKey[key], &stats.totals}) {
        ++totals->skipped;
        totals->evicted += evicted;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - stats.lastReport > std::chrono::seconds(10)) {
        stats.lastReport = now;
        ReportRecordedStoreSyncs(stats);
    }
}

class RecordedStoreSyncScope {
public:
    RecordedStoreSyncScope(const Recorder& recorder, std::uint64_t address, std::size_t bytes, const StorageTexture::AccessClassification& classification, bool stored, const HookSyncKey& key) : enabled(HookSyncProfiled()), address(address), bytes(bytes), classification(classification), stored(stored), key(key) {
        if (!enabled) return;
        batchesInFlight = recorder.InFlightBatches();
        if (!GuestMemory::WritesGuestMemory(key.site) && bytes <= CompareLimit && GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) {
            before.resize(bytes);
            std::memcpy(before.data(), reinterpret_cast<const void*>(address), bytes);
        }
        start = std::chrono::steady_clock::now();
    }
    ~RecordedStoreSyncScope() {
        if (!enabled) return;
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration<double, std::milli>(now - start).count();
        auto& stats = RecordedStoreSyncs();
        std::lock_guard lock(stats.mutex);
        const auto add = [&](RecordedStoreTotals& totals) {
            ++totals.count;
            totals.waitedMs += ms;
            totals.batchesFinished += batchesInFlight;
            if (stored) ++totals.flushed;
            if (classification.images == 0) ++totals.publishOnly;
            else if (!classification.checked) ++totals.unclassified;
            else ++(classification.allCpuWritten ? totals.cpuWrittenYes : totals.cpuWrittenNo);
            totals.imagesDead += classification.dead;
            totals.imagesLive += classification.live;
            if (before.empty() || !GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) ++totals.unchecked;
            else if (std::memcmp(before.data(), reinterpret_cast<const void*>(address), bytes) != 0) ++totals.changed;
            else ++totals.unchanged;
        };
        add(stats.byKey[key]);
        add(stats.totals);
        if (now - stats.lastReport > std::chrono::seconds(10)) {
            stats.lastReport = now;
            ReportRecordedStoreSyncs(stats);
        }
    }
    RecordedStoreSyncScope(const RecordedStoreSyncScope&) = delete;
    RecordedStoreSyncScope& operator=(const RecordedStoreSyncScope&) = delete;

private:
    static constexpr std::size_t CompareLimit = 65536;
    bool enabled;
    std::uint64_t address;
    std::size_t bytes;
    StorageTexture::AccessClassification classification;
    bool stored;
    HookSyncKey key;
    std::size_t batchesInFlight = 0;
    std::vector<std::byte> before;
    std::chrono::steady_clock::time_point start;
};

// GuestMemory flush hook: a CPU access to memory that recorded GPU work will write waits for that
// work first; then storage image results pending for the range are stored. The recorder is only
// dereferenced under the GpuMutex (it is destroyed with its device under that lock).
void FlushForAccess(std::uint64_t address, std::size_t bytes) {
    if (DrawProfiled()) hookCalls.fetch_add(1, std::memory_order_relaxed);
    // A label this worker queued but has not recorded writes the range: in queue order the label
    // precedes this access, so the group is recorded first (it then notes the range as a pending
    // write of the open batch, and the check below syncs on it like any recorded store). Not from
    // inside a completion action (a write-back store made while a batch is reaped): the record
    // would reap nested in the reap in progress, landing later batches' write-backs before the
    // rest of this one's, and this queue's queued label follows every batch of its own in flight
    // anyway, so the write-back landing first is the order the hardware gives.
    if (QueuedLabelsOnThread::Get() && QueuedLabelOverlaps(address, bytes)) {
        if (completionDepth() != 0 && HookCompletionGuard()) {
            queuedLabelHookInCompletion.fetch_add(1, std::memory_order_relaxed);
        } else if (auto* record = queuedLabelRecorder.load(std::memory_order_acquire); record != nullptr) {
            queuedLabelHookRecords.fetch_add(1, std::memory_order_relaxed);
            record();
        }
    }
    if (HookClearAccess() && HookSnapshotEnabled() && !HookFlushCpuBlocks() && ClearAccess(address, bytes)) {
        ClearAccesses::Set(ClearAccesses::Get() + 1);
        return;
    }
    if (!HookSnapshotEnabled() || SnapshotOverlaps(address, bytes)) {
        hookLocks.fetch_add(1, std::memory_order_relaxed);
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Hook);
        std::lock_guard gpu(GuestMemory::GpuMutex());
        if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(address, bytes)) {
            const HookSyncScope attribution(*recorder, address, bytes);
            Recorder::CountSync(1);
            // The wait runs without the mutex when this acquisition is the outermost one (a stage-A
            // read, a game thread); the recorder is not dereferenced after the call.
            recorder->SyncThrough(address, bytes, true);
        }
    }
    const auto site = GuestMemory::CurrentReadSite();
    const auto scope = site != GuestMemory::ReadSite::Store ? PublishScope::Whole : PublishScope::PartialUnits;
    const bool waited = completionDepth() != 0;
    bool published = false;
    // The classification is made before the store (the store changes what it measures); profile only.
    StorageTexture::AccessClassification classification{};
    if (HookSyncProfiled()) StorageTexture::ClassifyAccess(address, bytes, classification);
    // C4: an access whose every 64 KiB block the CPU wrote since the overlapping images' generations
    // reads the same bytes with or without their store (the store keeps those blocks), so the
    // images stay pending (short of the layers the store would have left empty-handed anyway) and
    // only the unit shadows over the range are published; with no image over the range that
    // publish is all FlushPending would do (one registry scan, not two).
    bool stored = false;
    std::size_t images = 0, evicted = 0;
    const bool kept = !HookFlushCpuBlocks() && StorageTexture::AccessKeptByCpu(address, bytes, &images, &evicted);
    if (kept || (images == 0 && !HookFlushCpuBlocks())) {
        stored = StorageTexture::WaitForFlushing(address, bytes);
        published = StorageTexture::PublishShadowsOnly(address, bytes, scope, "memory access", waited);
        if (kept && HookSyncProfiled()) CountRecordedStoreSkip(RecordedStoreKey(), evicted);
    } else {
        stored = StorageTexture::FlushPending(address, bytes, nullptr, "memory access", scope, &published, waited);
    }
    const bool publishInProgress = site == GuestMemory::ReadSite::HostWrite && AnyPublishInProgress();
    if (stored || published || publishInProgress || (completionDepth() == 0 && SnapshotOverlaps(address, bytes))) {
        // Stores into imported memory were only recorded; the CPU is about to read them. The wait
        // targets the batch holding them (the open one: SyncThrough submits it) and runs with the
        // mutex released when this acquisition is the outermost; a nested hook waits locked through
        // that batch ([lock] 'locked GPU waits'). Inside a completion action the store must land
        // before the completing batch's CPU write-back (an older result under a newer one), which
        // only the full Sync() gives there.
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Hook);
        std::lock_guard gpu(GuestMemory::GpuMutex());
        if (auto* recorder = Recorder::Active(); recorder != nullptr) {
            const RecordedStoreSyncScope attribution(*recorder, address, bytes, classification, stored, HookSyncProfiled() ? RecordedStoreKey() : HookSyncKey{});
            Recorder::CountSync(2);
            if (HookFullSync() || completionDepth() != 0) recorder->Sync();
            else recorder->SyncThrough(address, bytes, true);
        }
    }
}

}

static bool CheckpointMarkersEnabled() {
    static const bool enabled = std::getenv("APS5_GPU_CHECKPOINTS") != nullptr;
    return enabled;
}

struct CheckpointMarkerDevice {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkCmdWriteBufferMarkerAMD write = nullptr;
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    PFN_vkUnmapMemory unmapMemory = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    const void* mapping = nullptr;
};

static CheckpointMarkerDevice& CheckpointMarkerState() {
    static CheckpointMarkerDevice state;
    return state;
}

static bool PrepareCheckpointMarker(const Context& context) {
    if (!CheckpointMarkersEnabled()) return false;
    auto& state = CheckpointMarkerState();
    if (state.device == context.device && state.buffer != VK_NULL_HANDLE) return state.write != nullptr;
    if (state.device != VK_NULL_HANDLE && state.device != context.device) return false;
    if (context.device == VK_NULL_HANDLE || context.deviceProc == nullptr) return false;
    auto write = reinterpret_cast<PFN_vkCmdWriteBufferMarkerAMD>(context.deviceProc(context.device, "vkCmdWriteBufferMarkerAMD"));
    if (write == nullptr) return false;
    auto destroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(context.deviceProc(context.device, "vkDestroyBuffer"));
    auto freeMemory = reinterpret_cast<PFN_vkFreeMemory>(context.deviceProc(context.device, "vkFreeMemory"));
    auto unmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(context.deviceProc(context.device, "vkUnmapMemory"));
    if (destroyBuffer == nullptr || freeMemory == nullptr || unmapMemory == nullptr) return false;
    if (state.buffer == VK_NULL_HANDLE) {
        auto createBuffer = reinterpret_cast<PFN_vkCreateBuffer>(context.deviceProc(context.device, "vkCreateBuffer"));
        auto getRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(context.deviceProc(context.device, "vkGetBufferMemoryRequirements"));
        auto allocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(context.deviceProc(context.device, "vkAllocateMemory"));
        auto bindMemory = reinterpret_cast<PFN_vkBindBufferMemory>(context.deviceProc(context.device, "vkBindBufferMemory"));
        auto mapMemory = reinterpret_cast<PFN_vkMapMemory>(context.deviceProc(context.device, "vkMapMemory"));
        if (createBuffer == nullptr || getRequirements == nullptr || allocateMemory == nullptr || bindMemory == nullptr || mapMemory == nullptr) return false;
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = 2 * sizeof(std::uint32_t);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer buffer = VK_NULL_HANDLE;
        if (createBuffer(context.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS || buffer == VK_NULL_HANDLE) return false;
        VkMemoryRequirements requirements{};
        getRequirements(context.device, buffer, &requirements);
        std::uint32_t memoryType = context.memory.memoryTypeCount;
        const VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (std::uint32_t i = 0; i < context.memory.memoryTypeCount; ++i) {
            if ((requirements.memoryTypeBits & (1u << i)) != 0 && (context.memory.memoryTypes[i].propertyFlags & flags) == flags) {
                memoryType = i;
                break;
            }
        }
        if (memoryType >= context.memory.memoryTypeCount) {
            auto destroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(context.deviceProc(context.device, "vkDestroyBuffer"));
            if (destroyBuffer != nullptr) destroyBuffer(context.device, buffer, nullptr);
            return false;
        }
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memoryType;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (allocateMemory(context.device, &allocation, nullptr, &memory) != VK_SUCCESS || memory == VK_NULL_HANDLE) {
            auto destroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(context.deviceProc(context.device, "vkDestroyBuffer"));
            if (destroyBuffer != nullptr) destroyBuffer(context.device, buffer, nullptr);
            return false;
        }
        if (bindMemory(context.device, buffer, memory, 0) != VK_SUCCESS) {
            auto destroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(context.deviceProc(context.device, "vkDestroyBuffer"));
            auto freeMemory = reinterpret_cast<PFN_vkFreeMemory>(context.deviceProc(context.device, "vkFreeMemory"));
            if (destroyBuffer != nullptr) destroyBuffer(context.device, buffer, nullptr);
            if (freeMemory != nullptr) freeMemory(context.device, memory, nullptr);
            return false;
        }
        void* mapping = nullptr;
        if (mapMemory(context.device, memory, 0, bufferInfo.size, 0, &mapping) != VK_SUCCESS || mapping == nullptr) {
            auto destroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(context.deviceProc(context.device, "vkDestroyBuffer"));
            auto freeMemory = reinterpret_cast<PFN_vkFreeMemory>(context.deviceProc(context.device, "vkFreeMemory"));
            if (destroyBuffer != nullptr) destroyBuffer(context.device, buffer, nullptr);
            if (freeMemory != nullptr) freeMemory(context.device, memory, nullptr);
            return false;
        }
        std::memset(mapping, 0, bufferInfo.size);
        state.buffer = buffer;
        state.memory = memory;
        state.mapping = mapping;
    }
    state.device = context.device;
    state.write = write;
    state.destroyBuffer = destroyBuffer;
    state.freeMemory = freeMemory;
    state.unmapMemory = unmapMemory;
    return state.write != nullptr && state.buffer != VK_NULL_HANDLE;
}

constexpr VkDeviceSize CheckpointMarkerSerialOffset = 0;
constexpr VkDeviceSize CheckpointMarkerTokenOffset = sizeof(std::uint32_t);

void WriteCheckpointMarker(const Context& context, VkCommandBuffer commands, std::uint64_t token, std::uint32_t queue) {
    if (!CheckpointMarkersEnabled()) return;
    if (queue != 0) return;
    if (commands == VK_NULL_HANDLE || token == 0) return;
    try {
        if (!PrepareCheckpointMarker(context)) return;
        auto& state = CheckpointMarkerState();
        state.write(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, state.buffer, CheckpointMarkerTokenOffset, static_cast<std::uint32_t>(token));
    } catch (...) {
    }
}

void WriteNvCheckpointToken(const Context& context, VkCommandBuffer commands, std::uint64_t token, std::uint32_t queue) {
    if (!CheckpointMarkersEnabled()) return;
    if (queue != 0) return;
    if (commands == VK_NULL_HANDLE || token == 0) return;
    try {
        if (context.device == VK_NULL_HANDLE || context.deviceProc == nullptr) return;
        auto setCheckpoint = reinterpret_cast<PFN_vkCmdSetCheckpointNV>(context.deviceProc(context.device, "vkCmdSetCheckpointNV"));
        if (setCheckpoint == nullptr) return;
        EmitNvCheckpointToken(commands, static_cast<AgcDriver::FaultToken>(token), setCheckpoint);
    } catch (...) {
    }
}

void WriteCheckpointSerial(const Context& context, VkCommandBuffer commands, std::uint64_t serial, std::uint32_t queue) {
    if (!CheckpointMarkersEnabled()) return;
    if (queue != 0) return;
    if (commands == VK_NULL_HANDLE || serial == 0) return;
    try {
        if (!PrepareCheckpointMarker(context)) return;
        auto& state = CheckpointMarkerState();
        state.write(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, state.buffer, CheckpointMarkerSerialOffset, static_cast<std::uint32_t>(serial));
    } catch (...) {
    }
}

std::uint32_t LastCheckpointMarkerSerial() noexcept {
    const void* mapping = CheckpointMarkerState().mapping;
    if (mapping == nullptr) return 0;
    std::uint32_t serial = 0;
    std::memcpy(&serial, static_cast<const char*>(mapping) + CheckpointMarkerSerialOffset, sizeof(serial));
    return serial;
}

std::uint32_t LastCheckpointMarkerToken() noexcept {
    const void* mapping = CheckpointMarkerState().mapping;
    if (mapping == nullptr) return 0;
    std::uint32_t token = 0;
    std::memcpy(&token, static_cast<const char*>(mapping) + CheckpointMarkerTokenOffset, sizeof(token));
    return token;
}

void WriteCheckpointSentinel(const Context& context, VkCommandBuffer commands) {
    if (!CheckpointMarkersEnabled()) return;
    if (commands == VK_NULL_HANDLE) return;
    try {
        if (!PrepareCheckpointMarker(context)) return;
        auto& state = CheckpointMarkerState();
        state.write(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, state.buffer, CheckpointMarkerSerialOffset, 0);
        state.write(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, state.buffer, CheckpointMarkerTokenOffset, 0);
    } catch (...) {
    }
}

void ResetCheckpointSnapshots() noexcept {
    AgcDriver::SubmissionSnapshots::DeviceSnapshots().Reset();
}

void TeardownCheckpointMarkers() noexcept {
    ResetCheckpointSnapshots();
    auto& state = CheckpointMarkerState();
    if (state.device == VK_NULL_HANDLE) return;
    try {
        if (state.mapping != nullptr && state.unmapMemory != nullptr) state.unmapMemory(state.device, state.memory);
        if (state.buffer != VK_NULL_HANDLE && state.destroyBuffer != nullptr) state.destroyBuffer(state.device, state.buffer, nullptr);
        if (state.memory != VK_NULL_HANDLE && state.freeMemory != nullptr) state.freeMemory(state.device, state.memory, nullptr);
    } catch (...) {
    }
    state = CheckpointMarkerDevice{};
}

struct Recorder::ProgressWatcher {
    std::atomic<std::uint64_t> submitted{0};
    std::atomic<bool> stop{false};
    std::atomic<std::uint32_t> wakes{0};
    std::thread thread;

    ProgressWatcher(VkDevice device, VkSemaphore timeline, PFN_vkWaitSemaphoresKHR waitSemaphores, PFN_vkGetSemaphoreCounterValueKHR counterValue) {
        thread = std::thread([this, device, timeline, waitSemaphores, counterValue] {
            CpuTopology::PinHelperThread("GPU progress watcher");
            std::uint64_t seen = 0;
            for (;;) {
                const auto token = wakes.load(std::memory_order_acquire);
                if (stop.load(std::memory_order_acquire)) return;
                if (submitted.load(std::memory_order_acquire) <= seen) {
                    wakes.wait(token, std::memory_order_acquire);
                    continue;
                }
                const std::uint64_t target = seen + 1;
                VkSemaphoreWaitInfoKHR info{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR};
                info.semaphoreCount = 1;
                info.pSemaphores = &timeline;
                info.pValues = &target;
                const auto result = waitSemaphores(device, &info, 50'000'000ull);
                if (result == VK_TIMEOUT) continue;
                if (result != VK_SUCCESS) {
                    std::fprintf(stderr, "[gpu] progress watcher: vkWaitSemaphoresKHR failed (%d), watcher stops\n", static_cast<int>(result));
                    return;
                }
                std::uint64_t value = target;
                if (counterValue(device, timeline, &value) != VK_SUCCESS) value = target;
                seen = std::max(target, value);
                KernelWakeSleepers_nid_no_patch();
            }
        });
    }

    void Submitted(std::uint64_t serial) {
        submitted.store(serial, std::memory_order_release);
        wakes.fetch_add(1, std::memory_order_release);
        wakes.notify_one();
    }

    ~ProgressWatcher() {
        stop.store(true, std::memory_order_release);
        wakes.fetch_add(1, std::memory_order_release);
        wakes.notify_one();
        thread.join();
    }
};

Recorder::Recorder(const Context& context, bool timelineSemaphores) : context(context), id(nextRecorderId.fetch_add(1)) {
    {
        std::lock_guard lock(liveRecordersMutex);
        liveRecorders.push_back(id);
    }
    // Idempotent: the same hook for every recorder (the deferred lists are per thread, not per recorder).
    GuestMemory::SetGpuUnlockHook(&ReleaseDeferredKeeps);
    if (ProcTable() && context.deviceProc != nullptr) {
        getFenceStatus = context.Function<PFN_vkGetFenceStatus>("vkGetFenceStatus");
        resetFences = context.Function<PFN_vkResetFences>("vkResetFences");
        waitForFences = context.Function<PFN_vkWaitForFences>("vkWaitForFences");
        cmdUpdateBuffer = context.Function<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer");
        beginCommandBuffer = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer");
        endCommandBuffer = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer");
        queueSubmit = context.Function<PFN_vkQueueSubmit>("vkQueueSubmit");
        cmdPipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        cmdBeginQuery = context.Function<PFN_vkCmdBeginQuery>("vkCmdBeginQuery");
        cmdEndQuery = context.Function<PFN_vkCmdEndQuery>("vkCmdEndQuery");
        cmdResetQueryPool = context.Function<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool");
        cmdCopyQueryPoolResults = context.Function<PFN_vkCmdCopyQueryPoolResults>("vkCmdCopyQueryPoolResults");
        cmdPushConstants = context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants");
        cmdDispatch = context.Function<PFN_vkCmdDispatch>("vkCmdDispatch");
    }
    if (!timelineSemaphores) return;
    // The timeline starts at 0 and every Submit signals its serial (1, 2, ...): a value that only
    // grows, so a thread waiting for it outside the mutex can never observe a reset or a reused
    // handle (the batch fences are pooled and reset in release()). A failure here only disables the
    // unlocked waits; the locked Sync path still works.
    VkSemaphoreTypeCreateInfoKHR type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
    type.initialValue = 0;
    VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
    const auto result = context.Function<PFN_vkCreateSemaphore>("vkCreateSemaphore")(context.device, &info, nullptr, &timeline);
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "[gpu] timeline semaphore creation failed (Vulkan result %d); drains wait under the GPU mutex\n", static_cast<int>(result));
        timeline = VK_NULL_HANDLE;
    }
}

Recorder::~Recorder() {
    try {
        Sync();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[gpu] recorder teardown: %s\n", error.what());
    }
    ForgetHtileMasks_nid_no_patch(id);
    progressWatcher.reset();
    holdKept.clear();
    if (activeRecorder == this) {
        activeRecorder = nullptr;
        pendingWrites.store(nullptr, std::memory_order_release);
        publishGeneration.fetch_add(1, std::memory_order_release);
        pendingLabelSince.store(NoPendingLabel, std::memory_order_release);
        completionLabels.store(0, std::memory_order_release);
        completionStores.store(0, std::memory_order_release);
        writeBackCompletions.store(0, std::memory_order_release);
        workSinceSubmit.store(0, std::memory_order_release);
    }
    {
        // A thread that released the mutex around a wait on this recorder re-checks the id when it
        // retakes the mutex and touches nothing then. That check is only sound while the device
        // (and so this destructor) is torn down under the GpuMutex, as the driver's drain and
        // failure paths do; a worker dropping the last device reference after releasing its lock
        // would run this without the mutex, concurrently with a relocked waiter.
        std::lock_guard lock(liveRecordersMutex);
        liveRecorders.erase(std::remove(liveRecorders.begin(), liveRecorders.end(), id), liveRecorders.end());
    }
    // Sync() above completed every batch, so every timeline wait in progress on this recorder
    // returns now; the semaphore is destroyed only once they all left it.
    while (WaitersOf(id).load(std::memory_order_acquire) != 0) std::this_thread::yield();
    // The kept objects of every batch this thread finished (Sync() above included) belong to the
    // device that is going away: destroyed now, here (no hand-off: nothing should stay in flight
    // behind this thread); the release thread is then stopped and joined (it drains its queue
    // first, holding no mutex this thread holds: the destructors take only their caches' own
    // mutexes, which order after the GpuMutex), and the releases still in progress inline on
    // other threads (an unlock hook that found the queue stopping or full) are waited for before
    // the caller destroys the device. The count is global: a newer recorder's batches (device
    // replacement) are waited for too, which only prolongs the spin.
    if (!DeferredBatches().empty()) DestroyDeferred(TakeDeferred(), false);
    JoinReleaseThread();
    while (deferredPending.load(std::memory_order_acquire) != 0) std::this_thread::yield();
    {
        // Unlocked lookups stop before `labels` is destroyed (after this body).
        std::lock_guard tableLock(labelTableMutex);
        if (labelTableOwner == this) labelTableOwner = nullptr;
    }
    if (meshArgumentPipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, meshArgumentPipeline, nullptr);
    if (meshArgumentLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, meshArgumentLayout, nullptr);
    if (changedCopyPipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, changedCopyPipeline, nullptr);
    if (changedCopyLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, changedCopyLayout, nullptr);
    if (boundChangedCopyPipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, boundChangedCopyPipeline, nullptr);
    if (boundChangedCopyLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, boundChangedCopyLayout, nullptr);
    if (boundChangedCopySetLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, boundChangedCopySetLayout, nullptr);
    if (sampleCounterState > 0) {
        samplesPassed.store(SamplesTotal(), std::memory_order_release);
        if (samplePools != nullptr) std::static_pointer_cast<SamplePoolCache>(samplePools)->close();
        if (segmentPools != nullptr) std::static_pointer_cast<SamplePoolCache>(segmentPools)->close();
        context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, samplePipeline, nullptr);
        context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, sampleLayout, nullptr);
        context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, dumpPipeline, nullptr);
        context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, dumpLayout, nullptr);
        sampleCounter.reset();
    }
    if (dispatchArgumentPipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, dispatchArgumentPipeline, nullptr);
    if (dispatchArgumentLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, dispatchArgumentLayout, nullptr);
    for (const auto fence : spare) context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    spare.clear();
    for (const auto pool : sparePools) context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, pool, nullptr);
    sparePools.clear();
    // Sync() above waited for every batch, so no submission still signals the timeline.
    if (timeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroySemaphore>("vkDestroySemaphore")(context.device, timeline, nullptr);
    timeline = VK_NULL_HANDLE;
}

}
}
