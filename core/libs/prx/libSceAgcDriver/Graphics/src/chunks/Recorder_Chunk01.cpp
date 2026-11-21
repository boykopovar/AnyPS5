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

Recorder* activeRecorder = nullptr;

bool DrawProfiled() {
    static const bool profiled = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profiled;
}

std::uint64_t syncCounts[5] = {};
double syncWaitedMs[5] = {};
thread_local int announcedSource = 4;
thread_local const void* announcedSite = nullptr;
struct SyncSiteWaits {
    int source;
    const void* site;
    std::uint64_t syncs;
    std::uint64_t batches;
    double waitedMs;
};
std::vector<SyncSiteWaits> syncSites;
constexpr std::size_t NoSyncSite = static_cast<std::size_t>(-1);
constexpr std::size_t SyncSiteLimit = 48;
thread_local std::size_t activeSyncSite = NoSyncSite;

bool SyncSitesProfiled() {
    static const bool profiled = std::getenv("APS5_PROFILE_DRAW") != nullptr && std::getenv("APS5_NO_SYNC_SITES") == nullptr;
    return profiled;
}

std::size_t BeginSyncSite(int source, const void* site) {
    const auto previous = activeSyncSite;
    if (!SyncSitesProfiled()) return previous;
    auto it = std::find_if(syncSites.begin(), syncSites.end(), [&](const SyncSiteWaits& entry) { return entry.source == source && entry.site == site; });
    if (it == syncSites.end()) {
        if (syncSites.size() >= SyncSiteLimit) {
            it = std::find_if(syncSites.begin(), syncSites.end(), [&](const SyncSiteWaits& entry) { return entry.source == source && entry.site == nullptr; });
            if (it == syncSites.end()) it = syncSites.insert(syncSites.end(), SyncSiteWaits{source, nullptr, 0, 0, 0});
        } else {
            it = syncSites.insert(syncSites.end(), SyncSiteWaits{source, site, 0, 0, 0});
        }
    }
    ++it->syncs;
    activeSyncSite = static_cast<std::size_t>(it - syncSites.begin());
    return previous;
}

void CountSiteWait(double ms) {
    if (activeSyncSite == NoSyncSite || activeSyncSite >= syncSites.size()) return;
    auto& entry = syncSites[activeSyncSite];
    ++entry.batches;
    entry.waitedMs += ms;
}

std::string SyncSiteReport() {
    static const char* const names[5] = {"idle", "pending-write", "recorded-store", "address-based", "other"};
    std::vector<const SyncSiteWaits*> order;
    for (const auto& entry : syncSites) order.push_back(&entry);
    std::sort(order.begin(), order.end(), [](const SyncSiteWaits* a, const SyncSiteWaits* b) { return a->waitedMs > b->waitedMs; });
    std::string report;
    for (std::size_t i = 0; i < order.size() && i < 8; ++i) {
        char text[96];
        std::snprintf(text, sizeof(text), " %s@+0x%llx %llu/%llu/%.1fs", names[order[i]->source], order[i]->site != nullptr ? GuestMemory::CodeOffset(order[i]->site) : 0ull, static_cast<unsigned long long>(order[i]->syncs), static_cast<unsigned long long>(order[i]->batches), order[i]->waitedMs / 1000);
        report += text;
    }
    return report;
}
struct ThreadSyncs {
    std::uint32_t tag;
    std::array<std::uint64_t, 5> counts;
    std::array<double, 5> waitedMs;
};
std::vector<ThreadSyncs> threadSyncs;
AgcDriver::Mutex threadSyncsMutex;

struct ThreadWaitedSlot {};

void CountThreadSync(int source, double ms) {
    HostThreadSlot<double, ThreadWaitedSlot>::Set(HostThreadSlot<double, ThreadWaitedSlot>::Get() + ms);
    const auto tag = GuestMemory::GpuLockThreadTag();
    std::lock_guard lock(threadSyncsMutex);
    auto it = std::find_if(threadSyncs.begin(), threadSyncs.end(), [&](const ThreadSyncs& thread) { return thread.tag == tag; });
    if (it == threadSyncs.end()) it = threadSyncs.insert(threadSyncs.end(), ThreadSyncs{tag, {}, {}});
    ++it->counts[source];
    it->waitedMs[source] += ms;
}

std::string ThreadSyncReport() {
    static const char* const names[5] = {"idle", "pending-write", "recorded-store", "address-based", "other"};
    std::string report;
    std::lock_guard lock(threadSyncsMutex);
    for (const auto& thread : threadSyncs) {
        char text[64];
        if (thread.tag == 0xffffffffu) std::snprintf(text, sizeof(text), " untagged:");
        else std::snprintf(text, sizeof(text), " queue 0x%x:", thread.tag);
        report += text;
        for (int source = 0; source < 5; ++source) {
            if (thread.counts[source] == 0) continue;
            std::snprintf(text, sizeof(text), " %s %llu/%.1fs", names[source], static_cast<unsigned long long>(thread.counts[source]), thread.waitedMs[source] / 1000);
            report += text;
        }
    }
    return report;
}
std::atomic<std::uint64_t> unlockedWaits{0}, unlockedWaitedUs{0};
std::uint64_t submitCount = 0;
double submitUs = 0, submitMaxUs = 0;
constexpr std::size_t WaiterSlots = 16;
std::atomic<int> unlockedWaiters[WaiterSlots]{};
std::atomic<int>& WaitersOf(std::uint64_t id) { return unlockedWaiters[id % WaiterSlots]; }
AgcDriver::Mutex liveRecordersMutex;
std::vector<std::uint64_t> liveRecorders;
std::atomic<std::uint64_t> nextRecorderId{1};
std::atomic<std::uint64_t> samplesPassed{0};
constexpr std::uint32_t SampleFoldCapacity = 64;
constexpr std::uint32_t SampleCounterPushBytes = 24;
constexpr std::uint32_t SegmentSlots = 128;
constexpr std::uint32_t SegmentFoldCapacity = 1024;
constexpr std::uint32_t DumpFoldCapacity = 1024;
constexpr std::uint32_t SampleDumpBytes = 15 * 16 + 8;
constexpr VkDeviceSize DumpListOffset = 16 + 8 * SegmentFoldCapacity;
constexpr std::uint32_t SampleDumpPushBytes = 40;
std::atomic<std::uint64_t> dumpsQueued{0}, dumpsQueuedInPass{0}, dumpsAtOnce{0}, segmentsClosed{0}, dumpFolds{0};

struct SamplePoolCache {
    VkDevice device;
    PFN_vkDestroyQueryPool destroy;
    AgcDriver::Mutex mutex;
    std::vector<VkQueryPool> free;
    bool closed = false;
    void put(VkQueryPool pool) {
        {
            std::lock_guard lock(mutex);
            if (!closed && free.size() < 256) {
                free.push_back(pool);
                return;
            }
        }
        destroy(device, pool, nullptr);
    }
    VkQueryPool take() {
        std::lock_guard lock(mutex);
        if (free.empty()) return VK_NULL_HANDLE;
        const auto pool = free.back();
        free.pop_back();
        return pool;
    }
    void close() {
        std::lock_guard lock(mutex);
        closed = true;
        for (const auto pool : free) destroy(device, pool, nullptr);
        free.clear();
    }
};

struct SamplePool {
    SamplePool(std::shared_ptr<SamplePoolCache> cache, VkQueryPool pool) : cache(std::move(cache)), pool(pool) {}
    SamplePool(const SamplePool&) = delete;
    SamplePool& operator=(const SamplePool&) = delete;
    ~SamplePool() { cache->put(pool); }
    std::shared_ptr<SamplePoolCache> cache;
    VkQueryPool pool;
};

bool RecorderAlive(std::uint64_t id) {
    std::lock_guard lock(liveRecordersMutex);
    return std::find(liveRecorders.begin(), liveRecorders.end(), id) != liveRecorders.end();
}

struct CompletionDepthSlot {};

int completionDepth() {
    return HostThreadSlot<int, CompletionDepthSlot>::Get();
}
thread_local std::uint64_t hookRealWaits = 0;

}

bool Recorder::Alive(std::uint64_t id) {
    return RecorderAlive(id);
}

bool Recorder::InCompletion() {
    return completionDepth() != 0;
}

namespace {

bool HookCompletionGuard() {
    static const bool guard = std::getenv("APS5_NO_HOOK_COMPLETION_GUARD") == nullptr;
    return guard;
}

bool CompletionStoreSyncs() {
    static const bool enabled = std::getenv("APS5_COMPLETION_STORE_SYNC") != nullptr;
    return enabled;
}

bool HookLockedWait() {
    static const bool locked = std::getenv("APS5_HOOK_LOCKED_WAIT") != nullptr;
    return locked;
}

bool HookFullSync() {
    static const bool hookFullSync = std::getenv("APS5_HOOK_FULL_SYNC") != nullptr;
    return hookFullSync;
}

bool HookFlushCpuBlocks() {
    static const bool hookFlushCpuBlocks = [] {
        const char* text = std::getenv("APS5_HOOK_FLUSH_CPU_BLOCKS");
        return text != nullptr && text[0] == '0';
    }();
    return hookFlushCpuBlocks;
}

struct HoldCounters {
    std::uint64_t completions = 0;
    double completionMs = 0;
    double keptReleaseMs = 0;
    std::uint64_t completionSyncsSkipped = 0;
    std::uint64_t completionSyncsWaited = 0;
    double completionSyncWaitMs = 0;
    std::uint64_t reaps = 0;
    std::uint64_t reapsWithWork = 0;
    std::uint64_t reapBatches = 0;
    double reapMs = 0;
    std::uint64_t hookUnlockedWaits = 0;
    double hookUnlockedWaitMs = 0;
    double hookRelockMs = 0;
    std::array<std::uint64_t, 5> hookUnlockedWaitsBySource{};
    std::array<double, 5> hookUnlockedWaitMsBySource{};
    std::uint64_t hookUnlockedTornDown = 0;
    std::uint64_t hookLockedWaits = 0;
};
HoldCounters holdCounters;

struct DeferredBatch {
    std::vector<std::shared_ptr<void>> kept;
    std::vector<std::function<void()>> completions;
};
struct DeferredBatchesTag {};
auto& DeferredBatches() { return HostThreadLocal<std::vector<DeferredBatch>, DeferredBatchesTag>(); }
struct DeferredBytesTag {};
auto& DeferredBytes() { return HostThreadLocal<std::size_t, DeferredBytesTag>(); }
std::vector<DeferredBatch> TakeDeferred() {
    auto taken = std::move(DeferredBatches());
    DeferredBatches().clear();
    DeferredBytes() = 0;
    return taken;
}
std::atomic<std::uint64_t> deferredPending{0};
std::atomic<std::uint64_t> threadReleases{0}, threadObjects{0}, threadReleaseUs{0};
std::atomic<std::uint64_t> inlineReleases{0}, inlineObjects{0}, inlineReleaseUs{0}, inlineOverBound{0};
std::atomic<std::uint64_t> releaseQueueMax{0};

bool ReleaseUnderLock() {
    static const bool locked = std::getenv("APS5_RELEASE_UNDER_LOCK") != nullptr;
    return locked;
}

bool ReleaseOnUnlock() {
    static const bool onUnlock = std::getenv("APS5_RELEASE_ON_UNLOCK") != nullptr;
    return onUnlock;
}

std::size_t ReleaseQueueBound() {
    static const std::size_t bound = [] {
        const char* value = std::getenv("APS5_RELEASE_QUEUE_MAX");
        const auto parsed = value != nullptr ? std::strtoull(value, nullptr, 10) : 0ull;
        return parsed != 0 ? static_cast<std::size_t>(parsed) : std::size_t{256};
    }();
    return bound;
}

void DestroyDeferred(std::vector<DeferredBatch> releasing, bool onThread) {
    if (releasing.empty()) return;
    const bool profile = DrawProfiled();
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto count = releasing.size();
    std::uint64_t objects = 0;
    for (auto& batch : releasing) {
        objects += batch.kept.size();
        batch.completions.clear();
        batch.kept.clear();
        deferredPending.fetch_sub(1, std::memory_order_acq_rel);
    }
    releasing.clear();
    if (profile) {
        const auto us = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
        (onThread ? threadReleases : inlineReleases).fetch_add(count, std::memory_order_relaxed);
        (onThread ? threadObjects : inlineObjects).fetch_add(objects, std::memory_order_relaxed);
        (onThread ? threadReleaseUs : inlineReleaseUs).fetch_add(us, std::memory_order_relaxed);
    }
}

struct ReleaseQueue {
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<DeferredBatch> items;
    std::thread thread;
    bool started = false;
    bool stop = false;
    AgcDriver::Mutex joinMutex;
};

ReleaseQueue& ReleaseThreadQueue() {
    static ReleaseQueue* const queue = new ReleaseQueue;
    return *queue;
}

void ReleaseThreadMain() {
    CpuTopology::PinHelperThread("release thread");
    auto& queue = ReleaseThreadQueue();
    std::unique_lock lock(queue.mutex);
    for (;;) {
        queue.wake.wait(lock, [&] { return !queue.items.empty() || queue.stop; });
        if (queue.items.empty()) return;
        auto items = std::move(queue.items);
        queue.items.clear();
        lock.unlock();
        DestroyDeferred(std::move(items), true);
        lock.lock();
    }
}

void JoinReleaseThread() {
    auto& queue = ReleaseThreadQueue();
    std::lock_guard joining(queue.joinMutex);
    std::thread worker;
    {
        std::lock_guard lock(queue.mutex);
        if (!queue.started) return;
        queue.stop = true;
        worker = std::move(queue.thread);
    }
    queue.wake.notify_all();
    worker.join();
    std::lock_guard lock(queue.mutex);
    queue.started = false;
    queue.stop = false;
}

void ReleaseDeferredKeeps() {
    if (DeferredBatches().empty()) return;
    auto releasing = TakeDeferred();
    if (ReleaseOnUnlock()) {
        DestroyDeferred(std::move(releasing), false);
        return;
    }
    auto& queue = ReleaseThreadQueue();
    const bool profile = DrawProfiled();
    bool handedOff = false;
    bool overBound = false;
    try {
        std::lock_guard lock(queue.mutex);
        if (queue.items.size() + releasing.size() > ReleaseQueueBound()) {
            overBound = true;
        } else if (!queue.stop) {
            if (!queue.started) {
                queue.thread = Libc::StartHostThread(&ReleaseThreadMain);
                queue.started = true;
            }
            queue.items.reserve(queue.items.size() + releasing.size());
            for (auto& batch : releasing) queue.items.push_back(std::move(batch));
            handedOff = true;
            if (profile) {
                const auto queued = static_cast<std::uint64_t>(queue.items.size());
                auto seen = releaseQueueMax.load(std::memory_order_relaxed);
                while (queued > seen && !releaseQueueMax.compare_exchange_weak(seen, queued, std::memory_order_relaxed)) {
                }
            }
        }
    } catch (...) {
    }
    if (!handedOff) {
        if (profile && overBound) inlineOverBound.fetch_add(releasing.size(), std::memory_order_relaxed);
        DestroyDeferred(std::move(releasing), false);
        return;
    }
    queue.wake.notify_one();
}
constexpr std::int64_t NoPendingLabel = std::numeric_limits<std::int64_t>::min();
constexpr BarrierMask PassEndBarrier{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT};
constexpr BarrierMask LabelLeadingBarrier{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
std::atomic<std::int64_t> pendingLabelSince{NoPendingLabel};
std::atomic<std::uint64_t> writeGeneration{0};
std::atomic<std::uint64_t> publishGeneration{0};
std::atomic<std::uint64_t> completionLabels{0};
std::atomic<std::uint64_t> completionStores{0};
std::atomic<std::uint64_t> writeBackCompletions{0};
std::atomic<std::uint64_t> completionStoresSkipped{0};
std::atomic<std::uint64_t> completionStoresRun{0};
std::atomic<std::uint64_t> commandBuffersFreed{0};
std::atomic<std::uint64_t> fencesKept{0};
std::atomic<std::uint64_t> writeBacksOverLabels{0};
std::atomic<std::uint64_t> completionLabelsCountedLate{0};
std::atomic<std::uint64_t> workSinceSubmit{0};

bool CountAllCompletionLabels() {
    static const bool all = std::getenv("APS5_COUNT_ALL_COMPLETION_LABELS") != nullptr;
    return all;
}

bool ProcTable() {
    static const bool table = std::getenv("APS5_NO_PROC_TABLE") == nullptr;
    return table;
}
AgcDriver::Mutex labelTableMutex;
std::atomic<unsigned> injectedSubmitFailures{0};
Recorder* labelTableOwner = nullptr;
struct QueuedLabelRangesTag {};
auto& QueuedLabelRanges() { return ThreadScratch<std::vector<std::pair<std::uint64_t, std::uint64_t>>, QueuedLabelRangesTag>(); }
struct QueuedLabelsOnThreadTag {};
using QueuedLabelsOnThread = HostThreadSlot<bool, QueuedLabelsOnThreadTag>;
std::atomic<void (*)()> queuedLabelRecorder{nullptr};
struct LabelGroupDwordsTag {};
auto& LabelGroupDwords() { return HostThreadLocal<std::vector<std::pair<std::uint64_t, std::uint64_t>>, LabelGroupDwordsTag>(); }
thread_local Recorder::LateStatistics lateCounts{};

bool LateTrustEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("APS5_LABEL_TRUST_LATE");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}
std::atomic<std::uint64_t> storeCount{0}, storeRuns{0}, storesJoined{0}, storesReplaced{0}, storeWawBarriers{0}, storeJoinsRefused{0};
std::atomic<std::uint64_t> keyStoreCount{0}, keyStoreRuns{0}, keyStoreRunsForWriter{0}, keyStoresJoined{0};

bool KeyStoresEach() {
    static const bool each = std::getenv("APS5_DCC_KEYS_EACH") != nullptr;
    return each;
}
std::atomic<std::uint64_t> queuedLabelsNoted{0}, queuedLabelsOverRecorded{0}, queuedLabelHits{0}, queuedLabelHookRecords{0}, queuedLabelHookInCompletion{0};
constexpr std::size_t ReadKinds = static_cast<std::size_t>(Recorder::ReadKind::Count);
ProfileCounter readsNoted, readQueries, readStaleIgnored;
ProfileCounter readHits[ReadKinds];

bool ReadTrackingEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("APS5_COPY_READ_TRACKING");
        return value == nullptr || std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

bool LabelRunsEnabled() {
    static const bool enabled = std::getenv("APS5_NO_LABEL_RUNS") == nullptr && std::getenv("APS5_NO_LABEL_BATCHING") == nullptr;
    return enabled;
}

bool LabelRunsPerBatch() {
    static const bool perBatch = LabelRunsEnabled() && std::getenv("APS5_LABEL_RUNS_INLINE") == nullptr;
    return perBatch;
}
std::atomic<std::uint64_t> storeRunsAtSubmit{0}, storeRunsForced{0};

bool JoinWawCheck() {
    static const bool check = std::getenv("APS5_NO_JOIN_WAW_CHECK") == nullptr;
    return check;
}

bool SeparateQueuedLabels() {
    static const bool separate = std::getenv("APS5_NO_SEPARATE_QUEUED_LABELS") == nullptr;
    return separate;
}

bool QueuedLabelOverlaps(std::uint64_t address, std::size_t bytes) {
    const auto end = address + bytes;
    for (const auto& [begin, finish] : QueuedLabelRanges()) {
        if (address < finish && begin < end) return true;
    }
    return false;
}
std::atomic<std::uint64_t> hookCalls{0}, hookLocks{0}, targetedSyncs{0}, batchesLeftInFlight{0};
std::uint64_t snapshotCovered = 0, snapshotRebuilds = 0;
double snapshotRebuildMs = 0;

using WriteRanges = Recorder::WriteRanges;
AtomicSharedPtr<const WriteRanges> pendingWrites;

bool HookSnapshotEnabled() {
    static const bool enabled = std::getenv("APS5_NO_HOOK_SNAPSHOT") == nullptr;
    return enabled;
}

bool SnapshotOverlaps(const WriteRanges* snapshot, std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || snapshot == nullptr || snapshot->empty()) return false;
    const auto it = std::partition_point(snapshot->begin(), snapshot->end(), [&](const auto& range) { return range.second <= address; });
    return it != snapshot->end() && it->first < address + bytes;
}

bool SnapshotOverlaps(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return false;
    const auto snapshot = pendingWrites.load(std::memory_order_acquire);
    return SnapshotOverlaps(snapshot.get(), address, bytes);
}

bool HookClearAccess() {
    static const bool enabled = std::getenv("APS5_NO_HOOK_CLEAR_ACCESS") == nullptr;
    return enabled;
}

struct ThreadWriteRanges {
    std::uint64_t generation = ~std::uint64_t{0};
    std::shared_ptr<const WriteRanges> ranges;
};
struct ThreadWriteRangesTag {};

bool ThreadSnapshotOverlaps(std::uint64_t address, std::size_t bytes) {
    auto& copy = ThreadScratch<ThreadWriteRanges, ThreadWriteRangesTag>();
    const auto generation = publishGeneration.load(std::memory_order_acquire);
    if (copy.generation != generation) {
        copy.ranges = pendingWrites.load(std::memory_order_acquire);
        copy.generation = generation;
    }
    return SnapshotOverlaps(copy.ranges.get(), address, bytes);
}

struct ClearAccessesTag {};
using ClearAccesses = HostThreadSlot<std::uint64_t, ClearAccessesTag>;

bool ClearAccess(std::uint64_t address, std::size_t bytes) {
    return StorageTexture::PendingClearOnThread(address, bytes) && LiveUnitsClear(address, bytes) && !ThreadSnapshotOverlaps(address, bytes);
}

bool SnapshotCovers(const WriteRanges* snapshot, std::uint64_t address, std::uint64_t end) {
    if (snapshot == nullptr || snapshot->empty()) return false;
    const auto it = std::partition_point(snapshot->begin(), snapshot->end(), [&](const auto& range) { return range.second <= address; });
    return it != snapshot->end() && it->first <= address && end <= it->second;
}

bool SnapshotCovers(std::uint64_t address, std::uint64_t end) {
    const auto snapshot = pendingWrites.load(std::memory_order_acquire);
    return SnapshotCovers(snapshot.get(), address, end);
}

bool SyncThroughEnabled() {
    static const bool enabled = std::getenv("APS5_NO_SYNC_THROUGH") == nullptr;
    return enabled;
}

constexpr std::size_t HookSyncFrames = 6;

struct HookSyncKey {
    std::uint32_t queue;
    std::uint32_t opcode;
    GuestMemory::ReadSite site;
    std::array<unsigned long long, HookSyncFrames> frames;
    bool operator<(const HookSyncKey& other) const {
        return std::tie(queue, opcode, site, frames) < std::tie(other.queue, other.opcode, other.site, other.frames);
    }
};

struct HookSyncOutcomes {
    std::uint64_t unchangedOpen = 0;
    std::uint64_t unchangedPending = 0;
    std::uint64_t unchangedSignaled = 0;
    std::uint64_t changed = 0;
    std::uint64_t stores = 0;
    std::uint64_t unchecked = 0;
};

struct HookSyncTotals {
    std::uint64_t count = 0;
    HookSyncOutcomes outcomes;
    std::uint64_t rangeBytes = 0;
    std::uint64_t accessBytes = 0;
    double waitedMs = 0;
};

constexpr std::size_t SizeBuckets = 6;
constexpr const char* SizeBucketNames[SizeBuckets] = {"<=4K", "<=64K", "<=1M", "<=16M", "<=256M", ">256M"};

std::size_t SizeBucket(std::uint64_t bytes) {
    if (bytes <= 4096) return 0;
    if (bytes <= 65536) return 1;
    if (bytes <= (1u << 20u)) return 2;
    if (bytes <= (16u << 20u)) return 3;
    if (bytes <= (256u << 20u)) return 4;
    return 5;
}

struct HookSyncStats {
    std::map<HookSyncKey, HookSyncTotals> byKey;
    std::uint64_t count = 0, openTargets = 0, signaledTargets = 0, batchesFinished = 0;
    HookSyncOutcomes outcomes;
    double waitedMs = 0;
    std::array<std::uint64_t, SizeBuckets> rangeBuckets{};
    std::array<std::uint64_t, SizeBuckets> accessBuckets{};
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

HookSyncStats& HookSyncs() {
    static HookSyncStats stats;
    return stats;
}

bool HookSyncProfiled() {
    static const bool profiled = std::getenv("APS5_PROFILE_DRAW") != nullptr && std::getenv("APS5_NO_HOOKSYNC_PROFILE") == nullptr;
    return profiled;
}

std::string PacketName(std::uint32_t opcode) {
    if (opcode == GuestMemory::NoPacket) return "no-packet";
    if (opcode == 0xffffu) return "flip";
    if (opcode == 0xfffdu) return "deferred-labels";
    for (const auto& entry : Pm4::Opcodes) {
        if (entry.value == opcode) return std::string(entry.name);
    }
    char text[24];
    std::snprintf(text, sizeof(text), "op 0x%x", opcode);
    return text;
}

}
}
