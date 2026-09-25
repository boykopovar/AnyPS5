#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>

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
std::mutex threadSyncsMutex;

// This thread's fence and timeline waits in total (Recorder::ThreadWaitedMs): a caller times a span
// of its own work and reads the difference to learn how much of it was waiting for the GPU.
thread_local double threadWaitedMs = 0;

void CountThreadSync(int source, double ms) {
    threadWaitedMs += ms;
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
std::mutex liveRecordersMutex;
std::vector<std::uint64_t> liveRecorders;
std::atomic<std::uint64_t> nextRecorderId{1};

bool RecorderAlive(std::uint64_t id) {
    std::lock_guard lock(liveRecordersMutex);
    return std::find(liveRecorders.begin(), liveRecorders.end(), id) != liveRecorders.end();
}

// Completion actions in progress on this thread (finish()): a pending-write sync the flush hook
// makes from inside one (a copied buffer's write-back store) is skipped, see SyncThrough.
thread_local int completionDepth = 0;

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
thread_local std::vector<DeferredBatch> deferredBatches;
std::atomic<std::uint64_t> deferredPending{0};
// APS5_PROFILE_DRAW, for the [recorder] line: batches and objects released after an unlock and the
// time that took, on the release thread and inline (on the unlocking thread: the kill switch, a
// full queue, a stopping thread or a failed hand-off), the batches that went inline because the
// queue was full, and the longest queue seen (in batches).
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
    std::mutex joinMutex;
};

ReleaseQueue& ReleaseThreadQueue() {
    static ReleaseQueue* const queue = new ReleaseQueue;
    return *queue;
}

void ReleaseThreadMain() {
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
    if (deferredBatches.empty()) return;
    // Taken off the thread's list first: a destructor that took and released the mutex (none is
    // known to) would re-enter here and must find nothing.
    auto releasing = std::move(deferredBatches);
    deferredBatches.clear();
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
                queue.thread = std::thread(&ReleaseThreadMain);
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
std::atomic<std::int64_t> pendingLabelSince{NoPendingLabel};
std::atomic<std::uint64_t> writeGeneration{0};
std::atomic<std::uint64_t> completionLabels{0};
std::atomic<std::uint64_t> completionStoresSkipped{0};
std::atomic<std::uint64_t> completionStoresRun{0};
std::atomic<std::uint64_t> workSinceSubmit{0};
// The pending-label table's own mutex: every mutation of a recorder's `labels` holds it (under the
// GPU mutex), so a WAIT_REG_MEM looks a label up with this small lock alone (Recorder::LookupLabel)
// instead of queueing behind a dispatch. `labelTableOwner` is the active recorder while it lives,
// set and cleared under this mutex, so a lookup never touches a table being destroyed. Nothing
// under this mutex takes the GPU mutex (no lock-order cycle).
std::mutex labelTableMutex;
Recorder* labelTableOwner = nullptr;
// Flush hook statistics: calls, calls whose snapshot check overlapped (the GpuMutex was taken),
// targeted syncs and the batches they left in flight.
std::atomic<std::uint64_t> hookCalls{0}, hookLocks{0}, targetedSyncs{0}, batchesLeftInFlight{0};
// Snapshot maintenance (under the GpuMutex): notes whose range the snapshot already covered (no
// rebuild), rebuilds, and the time the rebuilds took (APS5_PROFILE_DRAW), so their cost is visible.
std::uint64_t snapshotCovered = 0, snapshotRebuilds = 0;
double snapshotRebuildMs = 0;

using WriteRanges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
// The sorted, merged union of the guest ranges every unfinished batch will write. Rebuilt under
// GuestMemory::GpuMutex (the only writer) and read by the flush hook without it: the lock is the
// graphics worker's main stall, so a no-overlap access must not wait for another queue's device
// work. A range leaves the snapshot only after its batch's completions (CPU write-backs) ran, so a
// reader that sees no overlap either precedes the note (the queues are unordered then, as on the
// GPU) or follows the write-back.
std::atomic<std::shared_ptr<const WriteRanges>> pendingWrites;

bool HookSnapshotEnabled() {
    // Debug aid: APS5_NO_HOOK_SNAPSHOT=1 takes the GpuMutex on every access as before.
    static const bool enabled = std::getenv("APS5_NO_HOOK_SNAPSHOT") == nullptr;
    return enabled;
}

bool SnapshotOverlaps(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return false;
    const auto snapshot = pendingWrites.load(std::memory_order_acquire);
    if (snapshot == nullptr || snapshot->empty()) return false;
    // Merged ranges are ordered by both bounds: the first one ending past the access decides.
    const auto it = std::partition_point(snapshot->begin(), snapshot->end(), [&](const auto& range) { return range.second <= address; });
    return it != snapshot->end() && it->first < address + bytes;
}

// Whether one merged range of the snapshot contains [address, end) entirely.
bool SnapshotCovers(std::uint64_t address, std::uint64_t end) {
    const auto snapshot = pendingWrites.load(std::memory_order_acquire);
    if (snapshot == nullptr || snapshot->empty()) return false;
    const auto it = std::partition_point(snapshot->begin(), snapshot->end(), [&](const auto& range) { return range.second <= address; });
    return it != snapshot->end() && it->first <= address && end <= it->second;
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
    std::fprintf(stderr, "%s\n", report.c_str());
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
        if (key.site != GuestMemory::ReadSite::Store && bytes <= CompareLimit && GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) {
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
            if (key.site == GuestMemory::ReadSite::Store) return &HookSyncOutcomes::stores;
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

// GuestMemory flush hook: a CPU access to memory that recorded GPU work will write waits for that
// work first; then storage image results pending for the range are stored. The recorder is only
// dereferenced under the GpuMutex (it is destroyed with its device under that lock).
void FlushForAccess(std::uint64_t address, std::size_t bytes) {
    hookCalls.fetch_add(1, std::memory_order_relaxed);
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
    if (StorageTexture::FlushPending(address, bytes)) {
        // Stores into imported memory were only recorded; the CPU is about to read them.
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Hook);
        std::lock_guard gpu(GuestMemory::GpuMutex());
        if (auto* recorder = Recorder::Active(); recorder != nullptr) {
            Recorder::CountSync(2);
            recorder->Sync();
        }
    }
}

}

Recorder::Recorder(const Context& context, bool timelineSemaphores) : context(context), id(nextRecorderId.fetch_add(1)) {
    {
        std::lock_guard lock(liveRecordersMutex);
        liveRecorders.push_back(id);
    }
    // Idempotent: the same hook for every recorder (the deferred lists are per thread, not per recorder).
    GuestMemory::SetGpuUnlockHook(&ReleaseDeferredKeeps);
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
    if (activeRecorder == this) {
        activeRecorder = nullptr;
        pendingWrites.store(nullptr, std::memory_order_release);
        pendingLabelSince.store(NoPendingLabel, std::memory_order_release);
        completionLabels.store(0, std::memory_order_release);
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
    if (!deferredBatches.empty()) {
        auto own = std::move(deferredBatches);
        deferredBatches.clear();
        DestroyDeferred(std::move(own), false);
    }
    JoinReleaseThread();
    while (deferredPending.load(std::memory_order_acquire) != 0) std::this_thread::yield();
    {
        // Unlocked lookups stop before `labels` is destroyed (after this body).
        std::lock_guard tableLock(labelTableMutex);
        if (labelTableOwner == this) labelTableOwner = nullptr;
    }
    for (auto& [commands, fence] : spare) {
        context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 1, &commands);
        context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    }
    spare.clear();
    // Sync() above waited for every batch, so no submission still signals the timeline.
    if (timeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroySemaphore>("vkDestroySemaphore")(context.device, timeline, nullptr);
    timeline = VK_NULL_HANDLE;
}

std::optional<std::chrono::steady_clock::time_point> Recorder::PendingLabelSince() {
    const auto since = pendingLabelSince.load(std::memory_order_acquire);
    if (since == NoPendingLabel) return std::nullopt;
    return std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(since));
}

std::uint64_t Recorder::WriteGeneration() {
    return writeGeneration.load(std::memory_order_acquire);
}

std::uint64_t Recorder::PendingCompletionLabels() {
    return completionLabels.load(std::memory_order_acquire);
}

std::uint64_t Recorder::RecordedWorkSinceSubmit() {
    return workSinceSubmit.load(std::memory_order_relaxed);
}

void Recorder::CountRecordedWork() {
    workSinceSubmit.fetch_add(1, std::memory_order_relaxed);
}

Recorder* Recorder::Active() {
    return activeRecorder;
}

void Recorder::Activate() {
    activeRecorder = this;
    {
        std::lock_guard tableLock(labelTableMutex);
        labelTableOwner = this;
    }
    GuestMemory::SetFlushHook(&FlushForAccess);
}

std::optional<std::uint64_t> Recorder::LookupLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, std::uint32_t& queue) {
    std::lock_guard tableLock(labelTableMutex);
    if (labelTableOwner == nullptr) return std::nullopt;
    return labelTableOwner->lookupLabel(address, bytes, afterStamp, queue);
}

bool Recorder::SnapshotWriteOverlaps(std::uint64_t address, std::size_t bytes) {
    return SnapshotOverlaps(address, bytes);
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
    return threadWaitedMs;
}

VkCommandBuffer Recorder::Commands() {
    GuestMemory::AssertGpuLockHeld("Recorder::Commands");
    if (open == nullptr) {
        auto batch = std::make_unique<Batch>();
        try {
            if (!spare.empty()) {
                // The pool resets command buffers on begin; the fence was reset when the batch completed.
                std::tie(batch->commands, batch->fence) = spare.back();
                spare.pop_back();
            } else {
                VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocation.commandPool = context.pool;
                allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocation.commandBufferCount = 1;
                Check(context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocation, &batch->commands), "vkAllocateCommandBuffers recorder");
                VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                Check(context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &info, nullptr, &batch->fence), "vkCreateFence recorder");
            }
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Check(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(batch->commands, &begin), "vkBeginCommandBuffer recorder");
        } catch (...) {
            release(*batch);
            throw;
        }
        open = std::move(batch);
    }
    return open->commands;
}

namespace {

constexpr std::uint32_t MaxTimedRanges = 512;

bool GpuTimingEnabled() {
    static const bool enabled = std::getenv("APS5_PROFILE_GPU") != nullptr;
    return enabled;
}

}

std::uint32_t Recorder::BeginGpuTiming(std::uint64_t key) {
    if (!GpuTimingEnabled()) return NoTiming;
    const auto commands = Commands();
    if (open->queries == VK_NULL_HANDLE) {
        VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        info.queryCount = MaxTimedRanges * 2;
        if (context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(context.device, &info, nullptr, &open->queries) != VK_SUCCESS) {
            open->queries = VK_NULL_HANDLE;
            return NoTiming;
        }
        context.Function<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(commands, open->queries, 0, info.queryCount);
    }
    if (open->timedKeys.size() >= MaxTimedRanges) return NoTiming;
    const auto index = static_cast<std::uint32_t>(open->timedKeys.size());
    open->timedKeys.push_back(key);
    // Both stamps wait for everything before them to complete, so the range is the timed work alone
    // (a top-of-pipe stamp is not held back by the barrier that precedes the work).
    context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, open->queries, index * 2);
    return index;
}

void Recorder::EndGpuTiming(std::uint32_t index) {
    if (index == NoTiming || open == nullptr || open->queries == VK_NULL_HANDLE) return;
    context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(open->commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, open->queries, index * 2 + 1);
}

void Recorder::readGpuTiming(Batch& batch) {
    if (batch.queries == VK_NULL_HANDLE || batch.timedKeys.empty()) return;
    std::vector<std::uint64_t> stamps(batch.timedKeys.size() * 2);
    const auto result = context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(context.device, batch.queries, 0, static_cast<std::uint32_t>(stamps.size()), stamps.size() * sizeof(std::uint64_t), stamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    if (result != VK_SUCCESS) return;
    struct Totals { std::uint64_t count = 0; double ms = 0; };
    static std::map<std::uint64_t, Totals> byKey;
    static double totalMs = 0;
    static std::uint64_t batches = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < batch.timedKeys.size(); ++i) {
        const auto ns = static_cast<double>(stamps[i * 2 + 1] - stamps[i * 2]) * context.limits.timestampPeriod;
        auto& totals = byKey[batch.timedKeys[i]];
        ++totals.count;
        totals.ms += ns / 1e6;
        totalMs += ns / 1e6;
    }
    ++batches;
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport < std::chrono::seconds(10)) return;
    lastReport = now;
    std::vector<std::pair<std::uint64_t, Totals>> hot(byKey.begin(), byKey.end());
    std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.ms > b.second.ms; });
    std::fprintf(stderr, "[gputime] %.0f ms of GPU time in %llu batches over 10 s; by program:", totalMs, static_cast<unsigned long long>(batches));
    for (std::size_t i = 0; i < hot.size() && i < 12; ++i) std::fprintf(stderr, " 0x%llx x%llu %.0fms", static_cast<unsigned long long>(hot[i].first), static_cast<unsigned long long>(hot[i].second.count), hot[i].second.ms);
    std::fprintf(stderr, "\n");
    byKey.clear();
    totalMs = 0;
    batches = 0;
}

void Recorder::Keep(std::shared_ptr<void> object) {
    Commands();
    open->kept.push_back(std::move(object));
}

void Recorder::OnComplete(std::function<void()> action) {
    Commands();
    open->completions.push_back(std::move(action));
}

bool Recorder::noteWrite(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return false;
    Commands();
    const auto end = address + bytes;
    open->writes.emplace_back(address, end);
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

void Recorder::noteWriteOn(Batch& batch, std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return;
    if (&batch == open.get()) {
        NotePendingWrite(address, bytes);
        return;
    }
    // An in-flight batch: its range joins the snapshot at once (the completion that stores it runs
    // when the batch finishes, and the hook must sync for a CPU read until then). The generation
    // moves as well, so a poller re-consults the label table for a completion label.
    batch.writes.emplace_back(address, address + bytes);
    if (activeRecorder == this) writeGeneration.fetch_add(1, std::memory_order_release);
    if (!SnapshotCovers(address, address + bytes)) publishPendingWrites();
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recorder::NotePendingWrite(std::uint64_t address, std::size_t bytes) {
    if (!noteWrite(address, bytes)) return;
    publishPendingWrites();
    // The note precedes this thread's vkQueueSubmit and the label another queue polls for; the
    // fence makes that order hold without relying on x86 store ordering.
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recorder::NotePendingWrites(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    bool publish = false;
    for (const auto& [begin, end] : ranges) {
        if (end > begin && noteWrite(begin, static_cast<std::size_t>(end - begin))) publish = true;
    }
    if (!publish) return;
    publishPendingWrites();
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recorder::publishPendingWrites() const {
    // Only the active recorder owns the snapshot: one torn down after its successor was activated,
    // or one a worker still dispatches into after device.reset(), must not publish its ranges.
    if (activeRecorder != this) return;
    const auto started = DrawProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto merged = std::make_shared<WriteRanges>();
    if (open != nullptr) merged->insert(merged->end(), open->writes.begin(), open->writes.end());
    for (const auto& batch : inFlight) merged->insert(merged->end(), batch->writes.begin(), batch->writes.end());
    for (const auto* batch : finishing) merged->insert(merged->end(), batch->writes.begin(), batch->writes.end());
    std::sort(merged->begin(), merged->end());
    std::size_t out = 0;
    for (const auto& [begin, end] : *merged) {
        if (out != 0 && begin <= (*merged)[out - 1].second) (*merged)[out - 1].second = std::max((*merged)[out - 1].second, end);
        else (*merged)[out++] = {begin, end};
    }
    merged->resize(out);
    pendingWrites.store(std::move(merged), std::memory_order_release);
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
    const auto end = address + bytes;
    if (open != nullptr && overlaps(*open, address, end)) return true;
    for (const auto& batch : inFlight) {
        if (overlaps(*batch, address, end)) return true;
    }
    return false;
}

bool Recorder::OpenWriteOverlaps(std::uint64_t address, std::size_t bytes) const {
    return bytes != 0 && open != nullptr && overlaps(*open, address, address + bytes);
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
        const bool signaled = context.Function<PFN_vkGetFenceStatus>("vkGetFenceStatus")(context.device, (*it)->fence) == VK_SUCCESS;
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

void Recorder::noteLabelOn(Batch& batch, std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue) {
    // One entry per dword; a label overlapping older entries (a 4-byte store inside an 8-byte one or
    // the reverse) replaces exactly the dwords it stores, so a lookup composes what memory will hold.
    std::lock_guard tableLock(labelTableMutex);
    for (std::size_t offset = 0; offset + 4 <= bytes.size(); offset += 4) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data() + offset, 4);
        const auto dword = address + offset;
        labels.insert_or_assign(dword, LabelEntry{value, queue, stamp, &batch});
        batch.labelDwords.push_back(dword);
    }
}

void Recorder::NoteLabel(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue) {
    Commands();
    noteLabelOn(*open, address, bytes, stamp, queue);
    if (activeRecorder == this && pendingLabelSince.load(std::memory_order_relaxed) == NoPendingLabel) {
        pendingLabelSince.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }
}

std::optional<std::uint64_t> Recorder::PendingLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, std::uint32_t& queue) const {
    // Under the GPU mutex; the table mutex is taken too so this one path serves both lock regimes.
    std::lock_guard tableLock(labelTableMutex);
    return lookupLabel(address, bytes, afterStamp, queue);
}

bool Recorder::PendingLabelIn(std::uint64_t address, std::size_t bytes) const {
    std::lock_guard tableLock(labelTableMutex);
    if (labels.empty() || bytes == 0) return false;
    const auto end = address + bytes;
    for (const auto& [dword, entry] : labels) {
        if (dword >= address && dword < end) return true;
    }
    return false;
}

std::optional<std::uint64_t> Recorder::lookupLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, std::uint32_t& queue) const {
    if (labels.empty() || (bytes != 4 && bytes != 8) || address % 4 != 0) return std::nullopt;
    std::uint64_t value = 0;
    for (std::size_t offset = 0; offset < bytes; offset += 4) {
        const auto found = labels.find(address + offset);
        if (found == labels.end() || found->second.stamp <= afterStamp) return std::nullopt;
        value |= static_cast<std::uint64_t>(found->second.value) << (offset * 8u);
        if (offset == 0) queue = found->second.queue;
    }
    return value;
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
        std::memcpy(reinterpret_cast<void*>(address), copy.data(), copy.size());
        GuestMemory::MarkWritten(address, copy.size());
    });
    // Counted per batch and counted down when the batch finishes, on every exit path of finish()
    // (a label whose range became unmapped is reported there, and a lost device throws before the
    // completions run): a label that will never land must not leave the workers reaping forever.
    ++batch.completionLabelCount;
    completionLabels.fetch_add(1, std::memory_order_acq_rel);
    // The table entry before the write note: the note bumps the generation a poller watches, and a
    // poller that sees the bump then finds the label without the GPU mutex.
    noteLabelOn(batch, address, bytes, stamp, queue);
    noteWriteOn(batch, address, bytes.size());
    if (&batch == open.get() && activeRecorder == this && pendingLabelSince.load(std::memory_order_relaxed) == NoPendingLabel) {
        pendingLabelSince.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }
}

void Recorder::NoteWrittenBack(std::uint64_t address, std::size_t bytes) {
    Recorder* recorder = activeRecorder;
    if (recorder == nullptr || bytes == 0) return;
    std::lock_guard ringLock(recorder->writtenBackMutex);
    recorder->writtenBack.push_back({++recorder->writtenBackSequence, address, address + bytes});
    while (recorder->writtenBack.size() > 16384) recorder->writtenBack.pop_front();
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

void Recorder::Submit() {
    // The work count is cleared even when nothing is open: the driver counts a dispatch after its
    // call returns (outside the mutex), so a submit by another thread in between leaves a stale
    // count behind, and the callers that act on it would otherwise take the mutex for nothing at
    // every packet or submission end until a real batch is next submitted.
    if (activeRecorder == this) workSinceSubmit.store(0, std::memory_order_relaxed);
    if (open == nullptr) return;
    GuestMemory::AssertGpuLockHeld("Recorder::Submit");
    auto batch = std::move(open);
    Check(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(batch->commands), "vkEndCommandBuffer recorder");
    VkSubmitInfo submission{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submission.commandBufferCount = 1;
    submission.pCommandBuffers = &batch->commands;
    // The timeline reaches this batch's serial when it completes (see WaitSerial).
    const std::uint64_t serial = submissions + 1;
    VkTimelineSemaphoreSubmitInfoKHR timelineInfo{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR};
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &serial;
    if (timeline != VK_NULL_HANDLE) {
        submission.pNext = &timelineInfo;
        submission.signalSemaphoreCount = 1;
        submission.pSignalSemaphores = &timeline;
    }
    Check(context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submission, batch->fence), "vkQueueSubmit recorder");
    batch->submitted = true;
    batch->serial = ++submissions;
    inFlight.push_back(std::move(batch));
    if (activeRecorder == this) pendingLabelSince.store(NoPendingLabel, std::memory_order_release);
}

std::uint64_t Recorder::SubmitAndEpoch() {
    Submit();
    return inFlight.empty() ? 0 : submissions;
}

namespace {

// One thread's registration as a timeline waiter (see `unlockedWaiters`): constructed while the
// caller still holds the mutex (or the device), so a teardown that starts after the mutex is
// released sees it and keeps the semaphore until the wait returned.
struct UnlockedWaiter {
    std::atomic<int>& count;
    explicit UnlockedWaiter(std::uint64_t recorderId) : count(WaitersOf(recorderId)) { count.fetch_add(1, std::memory_order_acq_rel); }
    ~UnlockedWaiter() { count.fetch_sub(1, std::memory_order_acq_rel); }
    UnlockedWaiter(const UnlockedWaiter&) = delete;
    UnlockedWaiter& operator=(const UnlockedWaiter&) = delete;
};

// Restores the thread's active sync-site entry on every exit path (a Check throw on a lost device
// included), so the next sync on the thread is not attributed to this one's site.
struct RestoreSyncSite {
    std::size_t site;
    ~RestoreSyncSite() { activeSyncSite = site; }
};

// The timeline wait itself, on handles copied out of the recorder: the caller holds no mutex, and
// the recorder may be torn down meanwhile (its destructor waits for the registered waiters to
// leave after completing every batch, so the handles stay valid until this returns).
VkResult WaitTimeline(VkDevice device, VkSemaphore timeline, PFN_vkWaitSemaphoresKHR waitSemaphores, std::uint64_t serial) {
    VkSemaphoreWaitInfoKHR wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR};
    wait.semaphoreCount = 1;
    wait.pSemaphores = &timeline;
    wait.pValues = &serial;
    auto result = waitSemaphores(device, &wait, 5'000'000'000ull);
    for (int waited = 5; result == VK_TIMEOUT; waited += 5) {
        // As for the fence wait in finish(): a hung batch is reported every 5 s.
        std::fprintf(stderr, "[gpu] recorded batch %llu still running on the GPU after %d s (timeline wait)\n", static_cast<unsigned long long>(serial), waited);
        result = waitSemaphores(device, &wait, 5'000'000'000ull);
    }
    return result;
}

}

void Recorder::WaitSerial(std::uint64_t serial) {
    if (timeline == VK_NULL_HANDLE || serial == 0) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const UnlockedWaiter waiter{id};
    const auto result = WaitTimeline(context.device, timeline, context.Function<PFN_vkWaitSemaphoresKHR>("vkWaitSemaphoresKHR"), serial);
    if (profile) {
        unlockedWaits.fetch_add(1, std::memory_order_relaxed);
        unlockedWaitedUs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()), std::memory_order_relaxed);
    }
    Check(result, "vkWaitSemaphoresKHR recorder");
}

void Recorder::FinishUpTo(std::uint64_t serial) {
    // A drain: the wait already happened (unlocked), the fences are signaled, so the source-0 wait
    // recorded here is the bookkeeping only (a caller that did not wait first, WaitForLeases, shows
    // up in the site table by its own address).
    ++syncCounts[0];
    announcedSource = 4;
    const void* site = std::exchange(announcedSite, nullptr);
    const auto previousSite = inFlight.empty() || inFlight.front()->serial > serial ? activeSyncSite : BeginSyncSite(0, site != nullptr ? site : __builtin_return_address(0));
    while (!inFlight.empty() && inFlight.front()->serial <= serial) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, 0);
    }
    activeSyncSite = previousSite;
}

void Recorder::Sync() {
    const auto source = std::exchange(announcedSource, 4);
    const void* site = std::exchange(announcedSite, nullptr);
    Submit();
    // Counted only when something is waited for: an idle recorder's Sync is free and would only
    // swamp the site table with the callers that check nothing first.
    const auto previousSite = inFlight.empty() ? activeSyncSite : BeginSyncSite(source, site != nullptr ? site : __builtin_return_address(0));
    while (!inFlight.empty()) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, source);
    }
    activeSyncSite = previousSite;
}

void Recorder::SyncThrough(std::uint64_t address, std::size_t bytes, bool waitUnlocked) {
    if (bytes == 0) return;
    const auto end = address + bytes;
    if (completionDepth != 0 && !CompletionStoreSyncs()) {
        // A store made by a completion action (a copied buffer's write-back, GuestBufferMemory::
        // WriteBack) reached the flush hook, and a batch still in flight notes the range. Every
        // batch in flight now was recorded after the completing one (finish runs front to back),
        // so in program order this store precedes their writes: landing it before them is the
        // order the hardware gives; waiting for them first (what the hook would do) lands the
        // completing batch's bytes over the later batch's, which is wrong, and does so under the
        // hold of whoever reaped (the [hooksync] 'store' entries, up to tens of ms per reap). The
        // later batch's own ordering against this write-back is its recorder's business at record
        // time (FillBuffer and DispatchIndirect consult copiedWriters; a later copied writer's
        // write-back runs after this one in finish order; a label behind completions lands after).
        // Known gap, not introduced here: a later batch writing the same range GPU-direct in place
        // (a writable region bound in place, or a GPU copy-back) is not ordered against this
        // write-back by the resource build; it went from deterministically stale to racy.
        announcedSource = 4;
        announcedSite = nullptr;
        ++holdCounters.completionSyncsSkipped;
        return;
    }
    if (completionDepth != 0) ++holdCounters.completionSyncsWaited;
    const auto completionWaitStart = completionDepth != 0 && DrawProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    struct CompletionWait {
        std::chrono::steady_clock::time_point start;
        ~CompletionWait() {
            if (start != std::chrono::steady_clock::time_point{}) holdCounters.completionSyncWaitMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    } completionWait{completionWaitStart};
    if (waitUnlocked) {
        // The hook's wait without the mutex: its own acquisition is the outermost on this thread.
        const auto source = announcedSource;
        const void* site = announcedSite != nullptr ? announcedSite : __builtin_return_address(0);
        if (syncThroughUnlocked(address, end, source, site)) return;
        // A completion-store wait (kill switch above) is already counted as 'waited'.
        if (completionDepth == 0) ++holdCounters.hookLockedWaits;
    }
    if (!SyncThroughEnabled() || (open != nullptr && overlaps(*open, address, end))) {
        // The announced site (or this call's caller) names the sync, not this function.
        if (announcedSite == nullptr) announcedSite = __builtin_return_address(0);
        Sync();
        return;
    }
    const auto source = std::exchange(announcedSource, 4);
    const void* site = std::exchange(announcedSite, nullptr);
    std::uint64_t targetSerial = 0;
    for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
        if (overlaps(**it, address, end)) {
            targetSerial = (*it)->serial;
            break;
        }
    }
    if (targetSerial == 0) return;
    targetedSyncs.fetch_add(1, std::memory_order_relaxed);
    const RestoreSyncSite restoreSite{BeginSyncSite(source, site != nullptr ? site : __builtin_return_address(0))};
    // Batches are in flight in serial order. A completion's own guest access can sync re-entrantly
    // and finish the target first (and a new batch may reuse its allocation, so the serial, not the
    // pointer, identifies it); the loop stops then, and later batches stay in flight either way.
    while (!inFlight.empty() && inFlight.front()->serial <= targetSerial) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, source);
    }
    batchesLeftInFlight.fetch_add(inFlight.size(), std::memory_order_relaxed);
}

bool Recorder::syncThroughUnlocked(std::uint64_t address, std::uint64_t end, int source, const void* site) {
    // Only for the outermost acquisition: a nested hook (inside a dispatch's hold) must not give
    // up the caller's mutex, and a thread that waits here holds nothing the relock could invert
    // (the hook took the mutex from a depth of 0 with the same locks held).
    if (HookLockedWait() || timeline == VK_NULL_HANDLE || GuestMemory::GpuMutex().DepthOnThisThread() != 1) return false;
    // The target as SyncThrough chooses it: the open batch (submitted here, so the GPU can reach
    // it) makes everything the target, else the newest overlapping in-flight batch; with
    // SyncThrough disabled everything is the target as well.
    std::uint64_t targetSerial = 0;
    if (!SyncThroughEnabled() || (open != nullptr && overlaps(*open, address, end))) {
        Submit();
        targetSerial = submissions;
    } else {
        for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
            if (overlaps(**it, address, end)) {
                targetSerial = (*it)->serial;
                break;
            }
        }
    }
    announcedSource = 4;
    announcedSite = nullptr;
    if (targetSerial == 0) return true;
    // Everything the wait needs is copied out: the recorder may be torn down while the mutex is
    // released (its destructor completes every batch and waits for the semaphore to be left, see
    // WaitTimeline), after which `this` must not be touched. The id re-check below relies on the
    // teardown running under the GpuMutex (the driver's drain and failure paths reset the device
    // under it; a worker dropping its last device reference outside its lock would not).
    const auto myId = id;
    const auto device = context.device;
    const auto semaphore = timeline;
    const auto waitSemaphores = context.Function<PFN_vkWaitSemaphoresKHR>("vkWaitSemaphoresKHR");
    // The site entry is taken under the mutex (the table is only touched under it; entries are
    // appended, never removed, so the index stays valid across the release).
    const RestoreSyncSite restoreSite{BeginSyncSite(source, site)};
    const bool profile = DrawProfiled();
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto waited = start;
    auto& mutex = GuestMemory::GpuMutex();
    VkResult result = VK_SUCCESS;
    {
        // Registered before the release: a teardown can only start once the mutex is free.
        const UnlockedWaiter waiter{myId};
        mutex.unlock();
        result = WaitTimeline(device, semaphore, waitSemaphores, targetSerial);
        // Read before the relock: the GPU wait, not the wait for the mutex behind it.
        if (profile) waited = std::chrono::steady_clock::now();
    }
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Hook);
    mutex.lock();
    if (profile) {
        const auto ms = std::chrono::duration<double, std::milli>(waited - start).count();
        // The GPU wait is charged like a locked wait, so the per-source, per-thread and per-site
        // totals of the [recorder] line keep their meaning; the unlocked count says how many were
        // not held. The relock's wait for the mutex is kept apart (the [lock] line also counts it
        // under 'hook' waits, for a cross-check).
        syncWaitedMs[source] += ms;
        CountThreadSync(source, ms);
        CountSiteWait(ms);
        ++holdCounters.hookUnlockedWaits;
        holdCounters.hookUnlockedWaitMs += ms;
        holdCounters.hookRelockMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waited).count();
    }
    if (!RecorderAlive(myId)) {
        // Torn down meanwhile: its destructor's Sync() ran every completion, nothing is pending.
        ++holdCounters.hookUnlockedTornDown;
        Check(result, "vkWaitSemaphoresKHR recorder");
        return true;
    }
    Check(result, "vkWaitSemaphoresKHR recorder");
    // The completions up to the target, under the mutex again; the fences are signaled, so the
    // waits inside finish() return at once (charged to the same site entry). Another thread may
    // have finished some meanwhile.
    targetedSyncs.fetch_add(1, std::memory_order_relaxed);
    while (!inFlight.empty() && inFlight.front()->serial <= targetSerial) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, source);
    }
    batchesLeftInFlight.fetch_add(inFlight.size(), std::memory_order_relaxed);
    return true;
}

bool Recorder::Reap() {
    const auto status = context.Function<PFN_vkGetFenceStatus>("vkGetFenceStatus");
    const bool profile = DrawProfiled();
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    std::uint64_t retired = 0;
    while (!inFlight.empty()) {
        if (status(context.device, inFlight.front()->fence) != VK_SUCCESS) break;
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), false, 4);
        ++retired;
    }
    if (profile) {
        ++holdCounters.reaps;
        if (retired != 0) ++holdCounters.reapsWithWork;
        holdCounters.reapBatches += retired;
        holdCounters.reapMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    return inFlight.empty();
}

void Recorder::finish(std::unique_ptr<Batch> batch, bool wait, int source) {
    // The batch's writes stay published until its completions ran, on every exit path.
    struct Finishing {
        Recorder& recorder;
        const Batch* batch;
        ~Finishing() {
            auto& list = recorder.finishing;
            list.erase(std::remove(list.begin(), list.end(), batch), list.end());
            // The batch's completion labels have landed, or never will (fence failure below).
            if (batch->completionLabelCount != 0) completionLabels.fetch_sub(batch->completionLabelCount, std::memory_order_acq_rel);
            // This also runs while the fence-failure throw unwinds: an allocation failure in the
            // rebuild keeps the previous snapshot, which is a superset (a stale entry only causes a
            // false hit, never a miss) rather than terminating.
            try {
                recorder.publishPendingWrites();
            } catch (...) {
            }
        }
    } finishingScope{*this, batch.get()};
    finishing.push_back(batch.get());
    if (wait) {
        // APS5_PROFILE_DRAW: how long the CPU waits for recorded GPU work, reported every 10 s.
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        static double waitedMs = 0;
        static std::uint64_t waits = 0;
        static auto lastReport = std::chrono::steady_clock::now();
        const auto waitStart = std::chrono::steady_clock::now();
        struct Report {
            bool enabled;
            int source;
            std::chrono::steady_clock::time_point start;
            const Recorder& recorder;
            ~Report() {
                if (!enabled) return;
                const auto now = std::chrono::steady_clock::now();
                const auto ms = std::chrono::duration<double, std::milli>(now - start).count();
                waitedMs += ms;
                syncWaitedMs[source] += ms;
                ++waits;
                CountThreadSync(source, ms);
                CountSiteWait(ms);
                if (now - lastReport > std::chrono::seconds(10)) {
                    lastReport = now;
                    const auto& h = holdCounters;
                    std::fprintf(stderr, "[recorder] %llu syncs waited %.1f s for the GPU in total (sources, count/wait: idle %llu/%.1fs, pending write %llu/%.1fs, recorded store %llu/%.1fs, address-based %llu/%.1fs, other %llu/%.1fs); hook %llu calls, %llu locked; %llu targeted syncs left %llu batches in flight; snapshot %llu rebuilds %.0f ms, %llu notes covered; %llu unlocked timeline waits %.1f s; %llu submissions, %zu label entries, %llu completion labels pending; fence waits by thread (count/wait):%s; top sync sites (source@caller syncs/batches/wait):%s; under holds (cumulative): %llu completions ran %.0f ms (kept objects released %.0f ms), pending-write syncs from completions: %llu skipped, %llu waited %.0f ms; %llu reaps (%llu with work) retired %llu batches in %.0f ms; hook waits unlocked %llu / %.0f ms GPU + %.0f ms relock (%llu found the recorder torn down), locked %llu; deferred releases: on the release thread %llu batches (%llu objects) in %.0f ms, inline %llu batches (%llu objects) in %.0f ms (%llu batches over the queue bound of %zu), queue max %llu batches, %llu pending\n",static_cast<unsigned long long>(waits), waitedMs / 1000, static_cast<unsigned long long>(syncCounts[0]), syncWaitedMs[0] / 1000, static_cast<unsigned long long>(syncCounts[1]), syncWaitedMs[1] / 1000, static_cast<unsigned long long>(syncCounts[2]), syncWaitedMs[2] / 1000, static_cast<unsigned long long>(syncCounts[3]), syncWaitedMs[3] / 1000, static_cast<unsigned long long>(syncCounts[4]), syncWaitedMs[4] / 1000, static_cast<unsigned long long>(hookCalls.load()), static_cast<unsigned long long>(hookLocks.load()), static_cast<unsigned long long>(targetedSyncs.load()), static_cast<unsigned long long>(batchesLeftInFlight.load()), static_cast<unsigned long long>(snapshotRebuilds), snapshotRebuildMs, static_cast<unsigned long long>(snapshotCovered), static_cast<unsigned long long>(unlockedWaits.load()), unlockedWaitedUs.load() / 1e6, static_cast<unsigned long long>(recorder.submissions), recorder.labels.size(), static_cast<unsigned long long>(completionLabels.load()), ThreadSyncReport().c_str(), SyncSiteReport().c_str(), static_cast<unsigned long long>(h.completions), h.completionMs, h.keptReleaseMs, static_cast<unsigned long long>(h.completionSyncsSkipped), static_cast<unsigned long long>(h.completionSyncsWaited), h.completionSyncWaitMs, static_cast<unsigned long long>(h.reaps), static_cast<unsigned long long>(h.reapsWithWork), static_cast<unsigned long long>(h.reapBatches), h.reapMs, static_cast<unsigned long long>(h.hookUnlockedWaits), h.hookUnlockedWaitMs, h.hookRelockMs, static_cast<unsigned long long>(h.hookUnlockedTornDown), static_cast<unsigned long long>(h.hookLockedWaits), static_cast<unsigned long long>(threadReleases.load()), static_cast<unsigned long long>(threadObjects.load()), threadReleaseUs.load() / 1000.0, static_cast<unsigned long long>(inlineReleases.load()), static_cast<unsigned long long>(inlineObjects.load()), inlineReleaseUs.load() / 1000.0, static_cast<unsigned long long>(inlineOverBound.load()), ReleaseQueueBound(), static_cast<unsigned long long>(releaseQueueMax.load()), static_cast<unsigned long long>(deferredPending.load()));
                    std::fprintf(stderr, "[recorder] completion label stores: %llu run, %llu skipped (no CPU write-back overlapped them)\n", static_cast<unsigned long long>(completionStoresRun.load()), static_cast<unsigned long long>(completionStoresSkipped.load()));
                }
            }
        } report{profile, source, waitStart, *this};
        const auto waitFences = context.Function<PFN_vkWaitForFences>("vkWaitForFences");
        auto result = waitFences(context.device, 1, &batch->fence, VK_TRUE, 5'000'000'000ull);
        for (int waited = 5; result == VK_TIMEOUT; waited += 5) {
            // A batch still running after 5 s is reported (every 5 s) so a GPU-side hang is visible.
            std::fprintf(stderr, "[gpu] recorded batch still running on the GPU after %d s\n", waited);
            result = waitFences(context.device, 1, &batch->fence, VK_TRUE, 5'000'000'000ull);
        }
        if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) {
            const auto idle = context.Function<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(context.device);
            Check(idle, "vkDeviceWaitIdle after recorder fence failure");
        }
        if (result != VK_SUCCESS) {
            release(*batch);
            Check(result, "vkWaitForFences recorder");
        }
    }
    readGpuTiming(*batch);
    // Completions store GPU results to guest memory; a failing one is reported, the rest still run.
    // Timed (APS5_PROFILE_DRAW) with the release of the kept objects: both run under whichever hold
    // reaped or synced the batch. Inside them the flush hook makes no pending-write wait (SyncThrough).
    const bool profile = DrawProfiled();
    const auto completionStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    {
        struct InCompletion {
            InCompletion() { ++completionDepth; }
            ~InCompletion() { --completionDepth; }
        } inCompletion;
        for (auto& action : batch->completions) {
            try {
                action();
            } catch (const std::exception& error) {
                std::fprintf(stderr, "[gpu] deferred write-back failed: %s\n", error.what());
            }
        }
    }
    if (profile) holdCounters.completions += batch->completions.size();
    const auto keptStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    // Deferred only from inside a hold: the unlock that releases the list is this thread's own, and
    // a caller finishing batches without the mutex (a test) might never make one.
    if (ReleaseUnderLock() || !GuestMemory::GpuMutex().HeldByThisThread()) {
        batch->completions.clear();
        batch->kept.clear();
    } else {
        // To this thread's unlock (see ReleaseDeferredKeeps); the completions go with the objects,
        // their captures hold the same ones. The push comes first: a failed one (the list's growth)
        // destroys the batch's objects here, under the mutex, counts nothing the destructor would
        // wait for and must not escape (the rest of finish() releases the fence and command buffer).
        try {
            deferredBatches.push_back({std::move(batch->kept), std::move(batch->completions)});
            deferredPending.fetch_add(1, std::memory_order_acq_rel);
        } catch (...) {
        }
        batch->completions.clear();
        batch->kept.clear();
    }
    if (profile) {
        const auto now = std::chrono::steady_clock::now();
        holdCounters.keptReleaseMs += std::chrono::duration<double, std::milli>(now - keptStart).count();
        holdCounters.completionMs += std::chrono::duration<double, std::milli>(now - completionStart).count();
    }
    // The batch's label entries leave the table (a later label to the same dword already replaced
    // its entry and belongs to another batch). Correctness never depended on this removal.
    if (!batch->labelDwords.empty()) {
        std::lock_guard tableLock(labelTableMutex);
        for (const auto dword : batch->labelDwords) {
            const auto found = labels.find(dword);
            if (found != labels.end() && found->second.batch == batch.get()) labels.erase(found);
        }
    }
    batch->labelDwords.clear();
    release(*batch);
}

void Recorder::release(Batch& batch) noexcept {
    if (batch.queries != VK_NULL_HANDLE) context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, batch.queries, nullptr);
    batch.queries = VK_NULL_HANDLE;
    // A completed (or never submitted) batch's objects are kept for reuse: the fence is signaled or
    // untouched, so resetting it cannot block, and the command buffer is no longer pending.
    if (batch.commands != VK_NULL_HANDLE && batch.fence != VK_NULL_HANDLE && spare.size() < 64 && context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &batch.fence) == VK_SUCCESS) {
        spare.emplace_back(batch.commands, batch.fence);
        batch.commands = VK_NULL_HANDLE;
        batch.fence = VK_NULL_HANDLE;
        return;
    }
    if (batch.commands != VK_NULL_HANDLE) context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 1, &batch.commands);
    if (batch.fence != VK_NULL_HANDLE) context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, batch.fence, nullptr);
    batch.commands = VK_NULL_HANDLE;
    batch.fence = VK_NULL_HANDLE;
}

}
