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
// Fence wait time by sync source (APS5_PROFILE_DRAW), and the source CountSync announced for the
// Sync/SyncThrough that follows it on this thread.
double syncWaitedMs[5] = {};
thread_local int announcedSource = 4;
// The call site CountSync named for that sync (nullptr: none; the sync's own return address is
// taken then), and the fence-wait table per (source, site) it feeds (APS5_PROFILE_DRAW, under the
// GpuMutex like finish()): which caller's syncs wait, when the source alone does not say (source
// 0 is every WaitIdle and drain). `activeSyncSite` is the entry of the sync in progress on this
// thread (an index: a nested sync from a completion replaces and restores it), so finish() charges
// each batch's wait to it.
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
    // APS5_NO_SYNC_SITES=1 leaves only the per-source and per-thread counts.
    static const bool profiled = std::getenv("APS5_PROFILE_DRAW") != nullptr && std::getenv("APS5_NO_SYNC_SITES") == nullptr;
    return profiled;
}

// Begins a sync's attribution: returns the previous active entry for the caller to restore, after
// counting the sync under (source, site). Beyond the table's limit every further site shares the
// last entry (a nullptr site), so the table stays small.
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

// The sites by wait, longest first, as "source@+offset syncs/batches/wait" (offsets symbolize with
// nm against the driver's image like the [guestmem] callers; +0x0 is the overflow entry).
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
// Fence waits by thread and source (APS5_PROFILE_DRAW), keyed by the thread's GpuMutex queue tag:
// a sync waits for the GPU under the mutex, so which worker's syncs (and which kind) hold it against
// queue 0 has to be visible. Its own mutex: the table is read while the report line is printed.
struct ThreadSyncs {
    std::uint32_t tag;
    std::array<std::uint64_t, 5> counts;
    std::array<double, 5> waitedMs;
};
std::vector<ThreadSyncs> threadSyncs;
AgcDriver::Mutex threadSyncsMutex;

// This thread's fence and timeline waits in total (Recorder::ThreadWaitedMs): a caller times a span
// of its own work and reads the difference to learn how much of it was waiting for the GPU.
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
// Unlocked timeline waits (WaitSerial): count and time, for the [recorder] line.
std::atomic<std::uint64_t> unlockedWaits{0}, unlockedWaitedUs{0};
// vkQueueSubmit calls and their time (APS5_PROFILE_DRAW; Submit runs under the GpuMutex).
std::uint64_t submitCount = 0;
double submitUs = 0, submitMaxUs = 0;
// Threads inside a timeline wait with the GpuMutex released (WaitSerial, syncThroughUnlocked): a
// recorder's teardown, after its Sync() made every wait satisfiable, waits until they left the
// semaphore before destroying it. Counted per recorder id (a small fixed array; the waiter cannot
// touch a recorder that may be dying, so it is keyed by the id it copied out), so a replaced
// device's old recorder does not spin on the new device's in-flight waits.
constexpr std::size_t WaiterSlots = 16;
std::atomic<int> unlockedWaiters[WaiterSlots]{};
std::atomic<int>& WaitersOf(std::uint64_t id) { return unlockedWaiters[id % WaiterSlots]; }
// Recorders alive, by id (own mutex, taken under the GpuMutex or with nothing held): a thread that
// released the GpuMutex around a wait learns whether its recorder still exists before touching it.
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

// Completion actions in progress on this thread (finish()): a pending-write sync the flush hook
// makes from inside one (a copied buffer's write-back store) is skipped, see SyncThrough.
struct CompletionDepthSlot {};

int completionDepth() {
    return HostThreadSlot<int, CompletionDepthSlot>::Get();
}
// This thread's hook syncs that waited for an unsignaled target (Recorder::ThreadHookWaits).
thread_local std::uint64_t hookRealWaits = 0;

}

bool Recorder::Alive(std::uint64_t id) {
    return RecorderAlive(id);
}

bool Recorder::InCompletion() {
    return completionDepth() != 0;
}

namespace {

// APS5_NO_HOOK_COMPLETION_GUARD=1: the flush hook records the accessing worker's queued labels
// even from inside a completion action, as before (see FlushForAccess).
bool HookCompletionGuard() {
    static const bool guard = std::getenv("APS5_NO_HOOK_COMPLETION_GUARD") == nullptr;
    return guard;
}

// APS5_COMPLETION_STORE_SYNC=1: a store made by a completion action waits for later batches that
// note its range, as before (the wait runs under whichever hold reaped the batch).
bool CompletionStoreSyncs() {
    static const bool enabled = std::getenv("APS5_COMPLETION_STORE_SYNC") != nullptr;
    return enabled;
}

// APS5_HOOK_LOCKED_WAIT=1: the flush hook's pending-write wait stays under the GpuMutex as before.
bool HookLockedWait() {
    static const bool locked = std::getenv("APS5_HOOK_LOCKED_WAIT") != nullptr;
    return locked;
}

// APS5_HOOK_FULL_SYNC=1: the flush hook's recorded-store wait is a full Sync() under the GpuMutex
// (every batch in flight) as before, instead of SyncThrough's targeted, unlocked wait (H1).
bool HookFullSync() {
    static const bool hookFullSync = std::getenv("APS5_HOOK_FULL_SYNC") != nullptr;
    return hookFullSync;
}

// APS5_HOOK_FLUSH_CPU_BLOCKS=0: an access whose every 64 KiB block the CPU wrote since the
// overlapping pending images' generations flushes them as before, instead of the C4 skip (H2:
// StorageTexture::AccessKeptByCpu).
bool HookFlushCpuBlocks() {
    static const bool hookFlushCpuBlocks = [] {
        const char* text = std::getenv("APS5_HOOK_FLUSH_CPU_BLOCKS");
        return text != nullptr && text[0] == '0';
    }();
    return hookFlushCpuBlocks;
}

// What the holds spend on the recorder (APS5_PROFILE_DRAW, all under the GpuMutex, printed at the
// end of the [recorder] line): completions and their time (of which releasing the kept objects),
// pending-write syncs from inside completions (skipped, or waited with the kill switch), reaps and
// the batches they retired, and the flush hook's unlocked waits (with those that found the
// recorder torn down when they retook the mutex, and those that had to wait locked).
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
    // The GPU wait alone (timeline wait with the mutex released) and, separately, the relock's
    // own wait for the GpuMutex (also counted by the [lock] line under 'hook'), so the two
    // remaining costs, GPU latency and hold contention, stay apart.
    double hookUnlockedWaitMs = 0;
    double hookRelockMs = 0;
    // The same by sync source (1 pending write, 2 recorded store).
    std::array<std::uint64_t, 5> hookUnlockedWaitsBySource{};
    std::array<double, 5> hookUnlockedWaitMsBySource{};
    std::uint64_t hookUnlockedTornDown = 0;
    // Genuine nested hooks (depth >= 2 outside a completion action) that had to wait locked.
    std::uint64_t hookLockedWaits = 0;
};
HoldCounters holdCounters;

// Deferred release of a finished batch's kept objects (and its completion actions, whose captures
// hold some of the same objects): finish() moves them to this list of the finishing thread instead
// of destroying them under the mutex, and the GpuMutex unlock hook (ReleaseDeferredKeeps, set by
// the constructor) hands the list to the release thread once the thread gave up its outermost
// hold. Destroying a ShaderResources with its buffers, descriptor set, textures and pipelines cost
// ~1.5 ms per reap with work, inside whichever hold reaped ('kept objects released', 15.9 s per
// 200 s run), and most reaps are queue 0's own (ReapRecorded before its dispatches), so a release
// on the unlocking thread would still be the frame's time. The destructors touch only their own
// caches' mutexes and the device, never the recorder or GpuMutex, so they need no hold and no
// particular thread (every Vulkan object is externally synchronized per object, and its cache's
// mutex covers its pool); they need the device alive, which ~Recorder guarantees by draining its
// own thread's list, stopping and joining the release thread (it drains the queue before it
// leaves) and waiting for every release still in progress on another thread (`deferredPending`:
// counted up under the mutex, down after each batch's objects are gone) before the device goes.
// The queue is bounded (APS5_RELEASE_QUEUE_MAX batches, default 256): a thread whose hand-off
// would exceed it destroys its batches itself, as it did before the release thread, so memory
// cannot grow behind a release thread that falls behind.
// APS5_RELEASE_UNDER_LOCK=1 destroys them in finish() as before; APS5_RELEASE_ON_UNLOCK=1 destroys
// them on the unlocking thread (no release thread).
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

// Destroys deferred batches, on the release thread (`onThread`) or on the thread that deferred
// them, and counts each batch off `deferredPending` only once its objects are gone: ~Recorder
// waits for that count.
void DestroyDeferred(std::vector<DeferredBatch> releasing, bool onThread) {
    if (releasing.empty()) return;
    const bool profile = DrawProfiled();
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto count = releasing.size();
    std::uint64_t objects = 0;
    for (auto& batch : releasing) {
        objects += batch.kept.size();
        // The completions first (their captures hold some of the objects), then the objects.
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

// The release thread's queue: one for the process. The thread is started at the first hand-off and
// stopped and joined by ~Recorder (JoinReleaseThread), after which the next hand-off starts it
// again (a device replacement makes a new recorder). The queue object is leaked so that a static
// destructor never races the thread: at process exit the thread is idle on its condition variable
// (every ~Recorder joined it) or, when no ~Recorder ran, mid-release like any worker thread. Nothing
// is done under `mutex` but the hand-off and the take; `joinMutex` serializes joiners and orders
// before `mutex`; neither is ever taken by a kept object's destructor.
struct ReleaseQueue {
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<DeferredBatch> items;
    std::thread thread;
    bool started = false;
    // Set by a joiner under `mutex` until the thread was joined: the thread leaves once the queue
    // is empty, and hand-offs meanwhile destroy inline (nothing may be queued without a taker).
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

// Stops the release thread once it emptied the queue and joins it (~Recorder, before its device
// goes). Hand-offs made while the thread stops destroy inline, so nothing can sit in the queue
// without a taker; a later hand-off starts the thread again.
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

// The unlock hook: hands what this thread deferred to the release thread, or destroys it here
// (APS5_RELEASE_ON_UNLOCK=1, the queue is full or stopping, or the hand-off cannot be made).
void ReleaseDeferredKeeps() {
    if (DeferredBatches().empty()) return;
    // Taken off the thread's list first: a destructor that took and released the mutex (none is
    // known to) would re-enter here and must find nothing.
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
            // The thread before the items: items queued with no thread to take them would keep
            // `deferredPending` up and ~Recorder waiting forever.
            if (!queue.started) {
                queue.thread = Libc::StartHostThread(&ReleaseThreadMain);
                queue.started = true;
            }
            // The allocation before any move: a failure here leaves `releasing` intact for the
            // fallback below, and the moves cannot throw.
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
// Lock-free state of the active recorder's open batch, read by the queue workers between packets
// and inside WAIT_REG_MEM polls (see the static readers in Recorder.hpp). Written under the mutex.
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
// Write-backs noted over a tracked label dword (NoteWrittenBack): a CPU store over a GPU label.
std::atomic<std::uint64_t> writeBacksOverLabels{0};
// Completion labels that entered the pending count at a write-back (see NoteWrittenBack).
std::atomic<std::uint64_t> completionLabelsCountedLate{0};
std::atomic<std::uint64_t> workSinceSubmit{0};

// APS5_COUNT_ALL_COMPLETION_LABELS=1: a completion label also stored on the GPU counts as pending
// from its registration, as before (see Recorder::AfterCompletions).
bool CountAllCompletionLabels() {
    static const bool all = std::getenv("APS5_COUNT_ALL_COMPLETION_LABELS") != nullptr;
    return all;
}

// APS5_NO_PROC_TABLE=1: the recorder resolves its Vulkan entry points per call, as before.
bool ProcTable() {
    static const bool table = std::getenv("APS5_NO_PROC_TABLE") == nullptr;
    return table;
}
// The pending-label table's own mutex: every mutation of a recorder's `labels` holds it (under the
// GPU mutex), so a WAIT_REG_MEM looks a label up with this small lock alone (Recorder::LookupLabel)
// instead of queueing behind a dispatch. `labelTableOwner` is the active recorder while it lives,
// set and cleared under this mutex, so a lookup never touches a table being destroyed. Nothing
// under this mutex takes the GPU mutex (no lock-order cycle).
AgcDriver::Mutex labelTableMutex;
std::atomic<unsigned> injectedSubmitFailures{0};
Recorder* labelTableOwner = nullptr;
// The ranges of the calling worker's queued labels (NoteQueuedLabel, cleared by
// ForgetQueuedLabels): the flush hook, on the same thread, has them recorded before an access that
// overlaps one, through the function the driver installed.
struct QueuedLabelRangesTag {};
auto& QueuedLabelRanges() { return ThreadScratch<std::vector<std::pair<std::uint64_t, std::uint64_t>>, QueuedLabelRangesTag>(); }
struct QueuedLabelsOnThreadTag {};
using QueuedLabelsOnThread = HostThreadSlot<bool, QueuedLabelsOnThreadTag>;
std::atomic<void (*)()> queuedLabelRecorder{nullptr};
// The dwords (with their stamps) this thread noted since its last CloseLabelGroup (see Recorder::CloseLabelGroup).
struct LabelGroupDwordsTag {};
auto& LabelGroupDwords() { return HostThreadLocal<std::vector<std::pair<std::uint64_t, std::uint64_t>>, LabelGroupDwordsTag>(); }
// Late-rule counters (Recorder::LateCounts), per thread: lookups run on the waiting worker.
thread_local Recorder::LateStatistics lateCounts{};

// APS5_LABEL_TRUST_LATE=1 trusts entries with stamp <= afterStamp under the late rule; off by
// default until the frame-rate collapse it produced at the video stage (lt_on: 119 -> 25 presents
// per 10 s with 0 late-trusted waits) is understood.
bool LateTrustEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("APS5_LABEL_TRUST_LATE");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}
// Store-run and queued-label counters (Recorder::StoreCounts), relaxed: they are only reported.
std::atomic<std::uint64_t> storeCount{0}, storeRuns{0}, storesJoined{0}, storesReplaced{0}, storeWawBarriers{0}, storeJoinsRefused{0};
std::atomic<std::uint64_t> keyStoreCount{0}, keyStoreRuns{0}, keyStoreRunsForWriter{0}, keyStoresJoined{0};

// Debug aid: APS5_DCC_KEYS_EACH=1 records every DCC key store at once (see Recorder::QueueKeyStore).
bool KeyStoresEach() {
    static const bool each = std::getenv("APS5_DCC_KEYS_EACH") != nullptr;
    return each;
}
std::atomic<std::uint64_t> queuedLabelsNoted{0}, queuedLabelsOverRecorded{0}, queuedLabelHits{0}, queuedLabelHookRecords{0}, queuedLabelHookInCompletion{0};
// Read-tracking counters (Recorder::ReadCounts), relaxed: they are only reported.
constexpr std::size_t ReadKinds = static_cast<std::size_t>(Recorder::ReadKind::Count);
ProfileCounter readsNoted, readQueries, readStaleIgnored;
ProfileCounter readHits[ReadKinds];

bool ReadTrackingEnabled() {
    // APS5_COPY_READ_TRACKING=0: no in-place reads are noted; the copy HLE's CPU path then requires
    // an idle recorder (VulkanDevice::CopyBuffer), never less.
    static const bool enabled = [] {
        const char* value = std::getenv("APS5_COPY_READ_TRACKING");
        return value == nullptr || std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

bool LabelRunsEnabled() {
    // Debug aid: APS5_NO_LABEL_RUNS=1 records every store between a barrier pair of its own
    // (APS5_NO_LABEL_BATCHING=1 turns off every label-batching change of Driver.cpp too).
    static const bool enabled = std::getenv("APS5_NO_LABEL_RUNS") == nullptr && std::getenv("APS5_NO_LABEL_BATCHING") == nullptr;
    return enabled;
}

// One store run per batch, recorded at Submit (see Recorder::RecordStore). Debug aid:
// APS5_LABEL_RUNS_INLINE=1 records the stores at once and closes the run at the next command.
bool LabelRunsPerBatch() {
    static const bool perBatch = LabelRunsEnabled() && std::getenv("APS5_LABEL_RUNS_INLINE") == nullptr;
    return perBatch;
}
std::atomic<std::uint64_t> storeRunsAtSubmit{0}, storeRunsForced{0};

// APS5_NO_JOIN_WAW_CHECK=1: a contiguous store joins the pending one without the check against
// the run's recorded stores, as before (see RecordStore).
bool JoinWawCheck() {
    static const bool check = std::getenv("APS5_NO_JOIN_WAW_CHECK") == nullptr;
    return check;
}

// APS5_NO_SEPARATE_QUEUED_LABELS=1: a queued label enters the recorded table and replaces a
// recorded entry of its dword, as before (see NoteQueuedLabel).
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
// Flush hook statistics: calls, calls whose snapshot check overlapped (the GpuMutex was taken),
// targeted syncs and the batches they left in flight.
std::atomic<std::uint64_t> hookCalls{0}, hookLocks{0}, targetedSyncs{0}, batchesLeftInFlight{0};
// Snapshot maintenance (under the GpuMutex): notes whose range the snapshot already covered (no
// rebuild), rebuilds, and the time the rebuilds took (APS5_PROFILE_DRAW), so their cost is visible.
std::uint64_t snapshotCovered = 0, snapshotRebuilds = 0;
double snapshotRebuildMs = 0;

using WriteRanges = Recorder::WriteRanges;
// The sorted, merged union of the guest ranges every unfinished batch will write. Rebuilt under
// GuestMemory::GpuMutex (the only writer) and read by the flush hook without it: the lock is the
// graphics worker's main stall, so a no-overlap access must not wait for another queue's device
// work. A range leaves the snapshot only after its batch's completions (CPU write-backs) ran, so a
// reader that sees no overlap either precedes the note (the queues are unordered then, as on the
// GPU) or follows the write-back.
AtomicSharedPtr<const WriteRanges> pendingWrites;

bool HookSnapshotEnabled() {
    // Debug aid: APS5_NO_HOOK_SNAPSHOT=1 takes the GpuMutex on every access as before.
    static const bool enabled = std::getenv("APS5_NO_HOOK_SNAPSHOT") == nullptr;
    return enabled;
}

bool SnapshotOverlaps(const WriteRanges* snapshot, std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || snapshot == nullptr || snapshot->empty()) return false;
    // Merged ranges are ordered by both bounds: the first one ending past the access decides.
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

// Whether one merged range of the snapshot contains [address, end) entirely.
bool SnapshotCovers(const WriteRanges* snapshot, std::uint64_t address, std::uint64_t end) {
    if (snapshot == nullptr || snapshot->empty()) return false;
    const auto it = std::partition_point(snapshot->begin(), snapshot->end(), [&](const auto& range) { return range.second <= address; });
    return it != snapshot->end() && it->first <= address && end <= it->second;
}

bool SnapshotCovers(std::uint64_t address, std::uint64_t end) {
    const auto snapshot = pendingWrites.load(std::memory_order_acquire);
    return SnapshotCovers(snapshot.get(), address, end);
}

// Attribution of the pending-write syncs the hook makes (the [hooksync] line every 10 s, under
// APS5_PROFILE_DRAW; APS5_NO_HOOKSYNC_PROFILE=1 leaves only the plain counts). Every sync is charged
// to the packet the accessing thread executes and the read site it named (or, when it named none,
// its return addresses), together with the noted range it hit, the batch that noted it and the
// time waited. A small read (up to 64 KiB) has its bytes compared before and after the wait: a
// sync whose bytes come out unchanged although its target batch had not run yet was not needed
// for that read (the noted range was larger than what the GPU wrote, or the GPU wrote the same
// values), which separates false overlaps from real producer/consumer dependencies; see
// HookSyncOutcomes for the cases that prove nothing. All state lives under the GpuMutex, which
// the hook holds.
// Debug aid: APS5_NO_SYNC_THROUGH=1 makes every pending-write sync a full Sync (SyncThrough and
// the attribution's DescribePendingWrite read the same switch).
bool SyncThroughEnabled() {
    static const bool enabled = std::getenv("APS5_NO_SYNC_THROUGH") == nullptr;
    return enabled;
}

// Six frames: whether the hook's constructor and FlushGpuWrites are inlined into the GuestMemory
// entry decides at which frame the driver caller sits, so enough are kept to see its own caller.
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

// Whether the bytes of a small access changed over the wait, split by the state of the target
// batch: only an unchanged access whose target had not run yet (open, or in flight with its fence
// unsignaled) shows the sync was not needed; against a signaled fence the copy made before the
// wait already held the GPU's values, so unchanged proves nothing. A store (ReadSite::Store) is
// counted apart: its sync orders a CPU write after the GPU's, whatever the bytes do.
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

// Size buckets for the noted ranges and the accesses: <=4K, <=64K, <=1M, <=16M, <=256M, larger.
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
    // 0xfffd is reserved for the deferred-label stores (recordDeferredLabels), once its owner
    // brackets them with SetCurrentPacket; until then they are charged to the following packet.
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
