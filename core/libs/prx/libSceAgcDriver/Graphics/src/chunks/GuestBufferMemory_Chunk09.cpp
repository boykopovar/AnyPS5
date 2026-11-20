#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/AdaptiveStaging.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SnapshotCopies.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#ifdef __APPLE__
struct udmabuf_create {
    std::uint32_t memfd;
    std::uint32_t flags;
    std::uint64_t offset;
    std::uint64_t size;
};
struct udmabuf_create_item {
    std::uint32_t memfd;
    std::uint32_t pad;
    std::uint64_t offset;
    std::uint64_t size;
};
struct udmabuf_create_list {
    std::uint32_t flags;
    std::uint32_t count;
    udmabuf_create_item list[];
};
constexpr std::uint32_t UDMABUF_FLAGS_CLOEXEC = 0x01;
constexpr unsigned long UDMABUF_CREATE = 0;
constexpr unsigned long UDMABUF_CREATE_LIST = 0;
#else
#include <linux/fs.h>
#include <linux/udmabuf.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <array>
#include <vector>
#include <span>
#include <atomic>
#include <tuple>
#include <utility>
#include <functional>
#include <bit>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_set>
#include <thread>
#include <stop_token>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace AgcDriver::Graphics {
namespace {
void reportMirrors() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& state = Mirrors();
    const auto now = std::chrono::steady_clock::now();
    if (now - state.lastReport < std::chrono::seconds(10)) return;
    state.lastReport = now;
    std::size_t count = 0;
    std::size_t writable = 0;
    std::size_t heaps = 0;
    std::uint64_t bytes = 0;
    std::uint64_t heapBytes = 0;
    {
        std::lock_guard lock(state.mutex);
        for (const auto& [base, mirror] : state.entries) {
            ++count;
            bytes += mirror->bytes;
            if (mirror->writable) ++writable;
            if (mirror->heap) ++heaps;
        }
        heapBytes = state.heapBytes;
    }
    AgcDriver::ProfilePrint_nid_no_patch("[buffers] image mirrors: %zu ranges (%.1f MiB, %zu writable, %zu heap %.1f MiB), %llu address-based builds served, %llu descriptor sub-ranges bound, %llu sparse heaps bound, %llu rebuilds, %llu refreshes: %llu blocks compared, %llu copied, %llu refresh syncs; heap refills %llu, unwatched heaps %llu, %llu heap mirrors evicted for budget, %llu heap mirrors released for imports (%.1f MiB)\n", count, bytes / 1048576.0, writable, heaps, heapBytes / 1048576.0, static_cast<unsigned long long>(state.builds), static_cast<unsigned long long>(state.subranges), static_cast<unsigned long long>(state.sparseBinds), static_cast<unsigned long long>(state.rebuilds), static_cast<unsigned long long>(state.refreshes), static_cast<unsigned long long>(state.blocksCompared), static_cast<unsigned long long>(state.blocksCopied), static_cast<unsigned long long>(state.syncs), static_cast<unsigned long long>(state.heapRefills), static_cast<unsigned long long>(state.heapUnwatched), static_cast<unsigned long long>(state.evictions), static_cast<unsigned long long>(state.heapImported), state.heapImportedBytes / 1048576.0);
#ifdef _WIN32
    auto& imports = Imports();
    std::lock_guard importsLock(imports.mutex);
    if (imports.settlesSubmitted != 0 || imports.settlesWaited != 0 || !imports.settles.empty()) AgcDriver::ProfilePrint_nid_no_patch("[buffers] host imports under %llu KiB: %llu settle reads submitted, %llu back in the write watch, %llu kept compared under a storage image, %llu retired during their read (%llu unmaps waited for it), %zu pending; %llu larger imports settled inside their window\n", static_cast<unsigned long long>(settleFloorBytes >> 10u), static_cast<unsigned long long>(imports.settlesSubmitted), static_cast<unsigned long long>(imports.settlesRewatched), static_cast<unsigned long long>(imports.settlesKeptCompared), static_cast<unsigned long long>(imports.settlesParked), static_cast<unsigned long long>(imports.settleUnmapWaits), imports.settles.size(), static_cast<unsigned long long>(imports.settlesWaited));
#endif
}

// Deferred lease release. The lease an address-based build takes (AcquireRegistered) is dropped by its
// write-back, which runs as a completion action when the batch that recorded the work finished; the
// work is no longer synced as soon as it is recorded (that sync was ~30000 waits per 200 s). A guest
// thread that unmaps, mprotects or frees a leased allocation finds it pinned in the registry and calls
// this waiter (with the registry lock released, see GuestAllocationsRequireUnpinned): under the device
// lock it submits the recorder and finishes its batches up to the newest one that was recorded with a
// lease (CountLeaseOutcome notes that serial); the completions release the leases, later batches stay
// in flight, and the registry rescans. A lease held by a build in progress belongs to a worker holding
// the device lock, so the waiter waits for that worker first; once it holds the lock every lease of
// recorded work is in a batch. A lease whose batch was not noted (the record threw between the keep
// and the note) is covered by draining the recorder when the noted serial is already finished. The
// waiter yields instead when its own thread holds the device lock (a driver thread mutating the
// registry mid-packet, as the plain spin did) and reports whether it finished any work.
struct LeaseState {
    AgcDriver::Mutex mutex;
    LeaseStats stats;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    // Under GuestMemory::GpuMutex: the recorder whose batches hold leases (serials restart with a
    // replaced device's recorder), the serial of its newest batch recorded with a lease, and the
    // newest one a waiter finished.
    const Recorder* leaseRecorder = nullptr;
    std::uint64_t newestLeaseSerial = 0;
    std::uint64_t finishedLeaseSerial = 0;
    std::uint64_t newestWritingLeaseSerial = 0;
    std::atomic<std::uint64_t> notedWritingLeases{0};
    std::atomic<std::uint64_t> finishedWritingLeases{0};
};

LeaseState& Leases() {
    static LeaseState state;
    return state;
}

// APS5_PROFILE_DRAW: AddSnapshot's consistency compares of a captured region against the region
// serving it, made and skipped (see AddSnapshot); printed in the [address-sync] line. Atomic: the
// unlocked stage A of descriptor builds adds snapshots too.
struct SnapshotStats {
    std::atomic<std::uint64_t> checked{0};
    std::atomic<std::uint64_t> skipped{0};
    std::atomic<std::uint64_t> mismatched{0};
};

SnapshotStats& Snapshots() {
    static SnapshotStats stats;
    return stats;
}

bool WaitForLeasesAndImports(std::uintptr_t address, std::size_t bytes) {
    const auto start = std::chrono::steady_clock::now();
    bool synced = false;
    bool drained = false;
    bool retiredImport = false;
    bool droppedAddressSpace = false;
    // A range pinned only by the cached address space is released by dropping the cache's
    // reference: no GPU wait, no device lock (the mutating thread may be a driver thread holding
    // it). The registry rescans; a range still pinned by a build or a batch holding the space comes
    // back here and finishes lease batches below.
    if (auto dropped = Spaces().current.exchange(nullptr); dropped != nullptr) {
        Spaces().droppedByWaiter.store(true, std::memory_order_relaxed);
        Spaces().waiterDrops.fetch_add(1, std::memory_order_relaxed);
        dropped.reset();
        droppedAddressSpace = true;
        auto& imports = Imports();
        const auto end = static_cast<std::uint64_t>(address) + bytes;
        bool overlapsImport = false;
        {
            std::lock_guard importsLock(imports.mutex);
            const auto next = imports.imports.lower_bound(address);
            const auto previous = next == imports.imports.begin() ? imports.imports.end() : std::prev(next);
            overlapsImport = (next != imports.imports.end() && next->first < end) || (previous != imports.imports.end() && previous->first + previous->second.bytes > address);
        }
        if (!overlapsImport) {
            auto& state = Leases();
            std::lock_guard lock(state.mutex);
            ++state.stats.contentionWaits;
            ++state.stats.cacheDrops;
            state.stats.contentionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            return true;
        }
    }
    if (Recorder::InCompletion()) throw std::runtime_error("guest allocation mutation cannot wait for an active GPU completion");
    if (GuestMemory::GpuMutex().HeldByThisThread()) {
        auto* recorder = Recorder::Active();
        if (recorder != nullptr && !recorder->Idle()) {
            Recorder::CountSync(4);
            recorder->Sync();
            synced = true;
            drained = true;
        }
        if (recorder != nullptr) recorder->FlushDeferredReleases();
        if (recorder != nullptr) GuestBufferMemory::ReleaseRetainedSpaces(recorder->Id(), recorder->CompletedThrough());
        const auto end = static_cast<std::uint64_t>(address) + bytes;
        auto& imports = Imports();
        if (releaseParkedImports(imports, address, end)) retiredImport = true;
        const auto lease = GuestAllocations::GuestAllocationsAcquireRegistered_nid_postfix();
        {
            std::lock_guard importsLock(imports.mutex);
            for (auto it = imports.imports.begin(); it != imports.imports.end();) {
                if (!holdsRangeOver(it->second, address, end)) {
                    ++it;
                    continue;
                }
                const auto next = std::next(it);
                retireImport(imports.device, imports, it, lease, false);
                it = next;
                retiredImport = true;
            }
        }
#ifdef _WIN32
        if (releaseSettleRetired(imports, address, end)) retiredImport = true;
#endif
        if (retiredImport && recorder != nullptr && !recorder->Idle()) {
            Recorder::CountSync(4);
            recorder->Sync();
            synced = true;
            drained = true;
        }
        if (retiredImport && recorder != nullptr) recorder->FlushDeferredReleases();
    } else {
        std::lock_guard lock(GuestMemory::GpuMutex());
        auto& state = Leases();
        auto* recorder = Recorder::Active();
        const auto target = state.newestLeaseSerial;
        if (recorder != nullptr && recorder == state.leaseRecorder && target > state.finishedLeaseSerial) {
            Recorder::CountSync(4);
            recorder->Submit();
            recorder->FinishUpTo(target);
            state.finishedLeaseSerial = target;
            synced = true;
        } else if (recorder != nullptr && !recorder->Idle()) {
            Recorder::CountSync(4);
            recorder->Sync();
            synced = true;
            drained = true;
        } else {
            std::this_thread::yield();
        }
        if (recorder != nullptr) recorder->FlushDeferredReleases();
        if (recorder != nullptr) GuestBufferMemory::ReleaseRetainedSpaces(recorder->Id(), recorder->CompletedThrough());

        const auto end = static_cast<std::uint64_t>(address) + bytes;
        auto& imports = Imports();
        if (releaseParkedImports(imports, address, end)) retiredImport = true;
        bool overlapsImport = false;
        {
            std::lock_guard importsLock(imports.mutex);
            for (const auto& [base, entry] : imports.imports) {
                if (holdsRangeOver(entry, address, end)) {
                    overlapsImport = true;
                    break;
                }
            }
        }
        if (overlapsImport) {
            if (recorder != nullptr && !recorder->Idle()) {
                Recorder::CountSync(4);
                recorder->Sync();
                synced = true;
                drained = true;
            }
            const auto lease = GuestAllocations::GuestAllocationsAcquireRegistered_nid_postfix();
            {
                std::lock_guard importsLock(imports.mutex);
                for (auto it = imports.imports.begin(); it != imports.imports.end();) {
                    if (!holdsRangeOver(it->second, address, end)) {
                        ++it;
                        continue;
                    }
                    const auto next = std::next(it);
                    retireImport(imports.device, imports, it, lease, false);
                    it = next;
                    retiredImport = true;
                }
            }
            if (retiredImport && recorder != nullptr && !recorder->Idle()) {
                Recorder::CountSync(4);
                recorder->Sync();
                synced = true;
                drained = true;
            }
        }
#ifdef _WIN32
        if (releaseSettleRetired(imports, address, end)) retiredImport = true;
#endif
    }
    auto& state = Leases();
    std::lock_guard lock(state.mutex);
    ++state.stats.contentionWaits;
    if (droppedAddressSpace) ++state.stats.cacheDrops;
    if (synced) ++state.stats.contentionSyncs;
    if (drained) ++state.stats.contentionDrains;
    state.stats.contentionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return synced || retiredImport;
}

void ensurePinWaiter() {
    static const bool registered = [] {
        GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(&WaitForLeasesAndImports);
        return true;
    }();
    static_cast<void>(registered);
}

void finishLeaseBatches(std::uint64_t target, const void* site) {
    const auto& state = Leases();
    auto* recorder = Recorder::Active();
    if (recorder == nullptr || recorder != state.leaseRecorder || target <= recorder->CompletedThrough()) return;
    if (target >= recorder->OpenSerial()) recorder->Submit();
    Recorder::AnnounceSyncSite(site);
    recorder->FinishUpTo(target);
}

}

bool SyncLeaseWork() {
    static const bool sync = std::getenv("APS5_SYNC_LEASE_DISPATCH") != nullptr;
    return sync;
}

void CountLeaseOutcome(bool synced, std::uint64_t batchSerial, bool writes) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& state = Leases();
    if (!synced) {
        // Serials grow with the submissions, so the newest noted one covers every earlier lease batch
        // of the same recorder.
        GuestMemory::AssertGpuLockHeld("CountLeaseOutcome");
        if (const auto* recorder = Recorder::Active(); recorder != state.leaseRecorder) {
            state.leaseRecorder = recorder;
            state.newestLeaseSerial = 0;
            state.finishedLeaseSerial = 0;
            state.newestWritingLeaseSerial = 0;
        }
        state.newestLeaseSerial = std::max(state.newestLeaseSerial, batchSerial);
        if (writes) {
            state.newestWritingLeaseSerial = std::max(state.newestWritingLeaseSerial, batchSerial);
            state.notedWritingLeases.fetch_add(1);
        }
    }
    std::lock_guard lock(state.mutex);
    ++(synced ? state.stats.synced : state.stats.deferred);
    if (writes) ++state.stats.writing;
    if (!profile) return;
    // APS5_PROFILE_DRAW: the leases (dispatches and draws) deferred and synced, the guest's
    // pin-contention waits and the BDA table cache, every 10 s (cumulative counts, like the
    // [recorder] line's), printed by whichever path counts a lease.
    const auto now = std::chrono::steady_clock::now();
    if (now - state.lastReport < std::chrono::seconds(10)) return;
    state.lastReport = now;
    const auto& stats = state.stats;
    const auto table = BdaResources::TableCacheCounters();
    const auto& snapshots = Snapshots();
    AgcDriver::ProfilePrint_nid_no_patch("[address-sync] leases: %llu released at completion, %llu synced at once; %llu by builds that store through addresses; %llu pin-contention waits by guest threads (%llu finished the lease batch, %llu drained the recorder, %llu cache-only drops) %.1f s; BDA table cache %llu hits / %llu misses (%zu tables held); snapshot compares: %llu skipped (live-backed region), %llu made\n", static_cast<unsigned long long>(stats.deferred), static_cast<unsigned long long>(stats.synced), static_cast<unsigned long long>(stats.writing), static_cast<unsigned long long>(stats.contentionWaits), static_cast<unsigned long long>(stats.contentionSyncs), static_cast<unsigned long long>(stats.contentionDrains), static_cast<unsigned long long>(stats.cacheDrops), stats.contentionMs / 1000, static_cast<unsigned long long>(table.hits), static_cast<unsigned long long>(table.misses), table.held, static_cast<unsigned long long>(snapshots.skipped.load(std::memory_order_relaxed)), static_cast<unsigned long long>(snapshots.checked.load(std::memory_order_relaxed)));
}

void FinishLeaseWork() {
    GuestMemory::AssertGpuLockHeld("FinishLeaseWork");
    finishLeaseBatches(Leases().newestLeaseSerial, __builtin_return_address(0));
}

void FinishLeaseWrites() {
    GuestMemory::AssertGpuLockHeld("FinishLeaseWrites");
    auto& state = Leases();
    const auto noted = state.notedWritingLeases.load();
    finishLeaseBatches(state.newestWritingLeaseSerial, __builtin_return_address(0));
    state.finishedWritingLeases.store(noted);
}

bool LeaseWorkPending() {
    GuestMemory::AssertGpuLockHeld("LeaseWorkPending");
    const auto& state = Leases();
    const auto* recorder = Recorder::Active();
    return recorder != nullptr && recorder == state.leaseRecorder && state.newestLeaseSerial > recorder->CompletedThrough();
}

bool LeaseWritesMayBePending() {
    const auto& state = Leases();
    return state.notedWritingLeases.load() > state.finishedWritingLeases.load();
}

LeaseStats LeaseCounters() {
    auto& state = Leases();
    std::lock_guard lock(state.mutex);
    return state.stats;
}

AddressSpaceStats AddressSpaceCounters() {
    const auto& cache = Spaces();
    const auto load = [](const std::atomic<std::uint64_t>& counter) { return counter.load(std::memory_order_relaxed); };
    return {addressSpaceCacheEnabled(), load(cache.hits), load(cache.rebuiltFirst), load(cache.rebuiltGeneration), load(cache.rebuiltEpoch), load(cache.rebuiltDevice), load(cache.rebuiltWaiterDrop), load(cache.unpublished), load(cache.dissolvedOverlap), load(cache.dissolvedImports), load(cache.waiterDrops), load(cache.patchedEpoch), load(cache.rebuiltGrowth)};
}

std::string AddressCopyOverflow(std::vector<AddressCopy> copies, std::uint64_t limit) {
    std::uint64_t total = 0;
    for (const auto& copy : copies) total += copy.committed;
    if (total <= limit) return {};
    std::sort(copies.begin(), copies.end(), [](const AddressCopy& left, const AddressCopy& right) { return left.committed > right.committed; });
    char text[256];
    std::snprintf(text, sizeof(text), "an address-based build copies %llu MiB of %zu registered ranges, past %llu MiB (APS5_ADDRESS_COPY_MAX_MIB); the largest:", static_cast<unsigned long long>(total >> 20u), copies.size(), static_cast<unsigned long long>(limit >> 20u));
    std::string message = text;
    for (std::size_t index = 0; index < copies.size() && index < 8; ++index) {
        const auto& copy = copies[index];
        const char* reason = copy.heap ? heapCopyReason(copy.overBudget, GuestMemory::Watched(copy.begin, copy.end - copy.begin)) : copy.reason;
        std::snprintf(text, sizeof(text), " 0x%llx+0x%llx (%.1f MiB committed, %s)", static_cast<unsigned long long>(copy.begin), static_cast<unsigned long long>(copy.end - copy.begin), copy.committed / 1048576.0, reason);
        message += text;
    }
    return message;
}

MirrorStats MirrorCounters() {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    MirrorStats stats{0, state.heapBytes, state.rebuilds, state.blocksCopied, state.heapRefills, state.sweeps, state.heapChecks, state.sparseBinds, state.evictions};
    for (const auto& [base, mirror] : state.entries) {
        stats.heapMirrors += mirror->heap ? 1 : 0;
        stats.rangeMirrors += mirror->heap ? 0 : 1;
    }
    return stats;
}

std::uint64_t SnapshotMismatches() {
    return Snapshots().mismatched.load(std::memory_order_relaxed);
}

std::shared_ptr<void> HoldHeapMirror(std::uint64_t address) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    auto it = state.entries.find(address);
    if (it != state.entries.end()) return it->second;
    return nullptr;
}

HeapMirrorHold HeapMirrorsWithin(std::uint64_t begin, std::uint64_t end) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    HeapMirrorHold hold;
    for (auto it = state.entries.lower_bound(begin); it != state.entries.end() && it->first < end; ++it) {
        if (!it->second->heap || it->second->range.expired()) continue;
        ++hold.mirrors;
        if (it->second->listed) hold.bytes += it->second->accounted;
    }
    return hold;
}

std::vector<std::uint64_t> AddressTableKey(std::span<const std::pair<std::uint64_t, std::uint64_t>> writes, std::span<const ShaderRecompiler::BdaAbi::Range> own) {
    std::vector<std::uint64_t> key;
    key.reserve(writes.size() * 2u + 1u + own.size() * 4u);
    for (const auto& [begin, end] : writes) key.insert(key.end(), {begin, end});
    key.push_back(std::numeric_limits<std::uint64_t>::max());
    for (const auto& range : own) key.insert(key.end(), {range.begin, range.end, range.deviceAddress, (static_cast<std::uint64_t>(range.reserved) << 32u) | range.permissions});
    return key;
}

void ClearSnapshotCopies(VkDevice device) {
    auto& copies = SnapshotCopyCache();
    std::lock_guard lock(copies.mutex);
    if (copies.device != device) return;
    copies.Clear();
    copies.device = VK_NULL_HANDLE;
}

void ClearImageMirrors(VkDevice device) {
    auto space = Spaces().current.load();
    if (space != nullptr && space->device == device) {
        Spaces().current.compare_exchange_strong(space, std::shared_ptr<const GuestBufferMemory::AddressSpace>{});
    }
    auto& state = Mirrors();
    std::map<std::uint64_t, std::shared_ptr<ImageMirror>> entries;
    {
        std::lock_guard lock(state.mutex);
        if (state.device != device) return;
        entries.swap(state.entries);
        for (auto& [base, entry] : entries) entry->listed = false;
        state.failed.clear();
        state.heapBytes = 0;
        state.device = VK_NULL_HANDLE;
    }
}

void ClearHostImports(VkDevice device) {
    clearCarriedShadows(device);
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.parked.erase(device);
    state.importedAtMap.clear();
#ifdef _WIN32
    if (device == state.device) drainSettles(state);
#endif
    if (const auto found = state.settles.find(device); found != state.settles.end()) {
#ifdef _WIN32
        releaseSettleBatches(device, found->second);
        std::erase_if(state.pendingSettles, [device](const auto& settle) { return settle.second.phase == SettlePhase::Submitted && settle.second.device == device; });
#endif
        const auto& settle = found->second;
        if (settle.settleFence != VK_NULL_HANDLE) settle.destroyFence(device, settle.settleFence, nullptr);
        if (settle.settleTarget != VK_NULL_HANDLE) settle.settleDestroyBuffer(device, settle.settleTarget, nullptr);
        if (settle.settleMemory != VK_NULL_HANDLE) MemoryAccounting::Free(settle.settleFreeMemory, device, settle.settleMemory);
        state.settles.erase(found);
    }
    if (state.device != device) return;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwatched;
    GuestAllocations::Lease lease;
    if (std::any_of(state.imports.begin(), state.imports.end(), [](const auto& import) { return import.second.unwatched; })) {
        lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        for (const auto& [address, entry] : state.imports) noteRetiredUnwatched(unwatched, entry, lease);
    }
    for (const auto& [address, entry] : state.imports) {
        state.destroyBuffer(state.device, entry.buffer, nullptr);
        MemoryAccounting::Free(state.freeMemory, state.device, entry.memory);
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
    }
    state.imports.clear();
    rewatchRetired(state, std::move(unwatched));
    state.pendingImports.clear();
    state.growth.clear();
    state.growthFloor = std::numeric_limits<std::uint64_t>::max();
    state.growthDue = false;
    state.liveBytes = 0;
    state.failed.clear();
    state.failedJoins.clear();
    state.failedBlocks.clear();
    state.refusals.clear();
    state.evictingRefusals.clear();
    state.gateRefusals.clear();
    state.ceiling = 0;
    state.allocationFailures = 0;
    state.allocationFailed = false;
    state.reportNoted = 0;
    state.device = VK_NULL_HANDLE;
    state.refreshedGeneration = 0;
    bumpEpoch(state);
}

namespace {

void runImportProbe(const Context& context, std::uint64_t base, std::uint64_t bytes, const std::function<const char*(HostImport&, VkResult&)>& importStep, const std::function<bool(std::uint32_t&)>& collect, ImportProbe& probe, const std::function<const char*(HostImport&, VkResult&)>& settleStep = {}) {
    HostImport import{base, bytes, VK_NULL_HANDLE, VK_NULL_HANDLE, 0};
    VkBuffer destination = VK_NULL_HANDLE;
    VkDeviceMemory destinationMemory = VK_NULL_HANDLE;
    VkCommandBuffer commands[3]{};
    VkFence fence = VK_NULL_HANDLE;
    bool submitted = false;
    const auto run = [&]() -> const char* {
        std::uint32_t quiet = 0;
        if (!collect(quiet)) return "the first collect";
        if (!collect(quiet)) return "the second collect";
        if (quiet != 0) return "an unwritten scratch range";
        if (const char* step = importStep(import, probe.result)) return step;
        if (!collect(probe.writtenAtImport)) return "the collect after the import";
        if (settleStep) {
            if (const char* step = settleStep(import, probe.result)) return step;
            if (!collect(probe.writtenDuringSettle)) return "the collect after the settle";
            if (!collect(probe.writtenAfterSettle)) return "the collect after a settled import";
        }
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = bytes;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if ((probe.result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &destination)) != VK_SUCCESS) return "vkCreateBuffer";
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, destination, &requirements);
        if (requirements.memoryTypeBits == 0) return "the destination memory type";
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(requirements.memoryTypeBits));
        if ((probe.result = MemoryAccounting::Allocate(context, allocation, "host import probe", &destinationMemory)) != VK_SUCCESS) return "vkAllocateMemory";
        if ((probe.result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, destination, destinationMemory, 0)) != VK_SUCCESS) return "vkBindBufferMemory";
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = context.pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 3;
        if ((probe.result = context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocate, commands)) != VK_SUCCESS) return "vkAllocateCommandBuffers";
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if ((probe.result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands[0], &begin)) != VK_SUCCESS) return "vkBeginCommandBuffer";
        const VkBufferCopy region{0, 0, bytes};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands[0], import.buffer, destination, 1, &region);
        if ((probe.result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands[0])) != VK_SUCCESS) return "vkEndCommandBuffer";
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if ((probe.result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands[1], &begin)) != VK_SUCCESS) return "vkBeginCommandBuffer";
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands[1], import.buffer, 0, bytes, 0x02020202u);
        if ((probe.result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands[1])) != VK_SUCCESS) return "vkEndCommandBuffer";
        if ((probe.result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands[2], &begin)) != VK_SUCCESS) return "vkBeginCommandBuffer";
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands[2], import.buffer, 0, bytes, 0x05050505u);
        if ((probe.result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands[2])) != VK_SUCCESS) return "vkEndCommandBuffer";
        const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if ((probe.result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence)) != VK_SUCCESS) return "vkCreateFence";
        const auto execute = [&](VkCommandBuffer buffer) -> const char* {
            if ((probe.result = context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &fence)) != VK_SUCCESS) return "vkResetFences";
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &buffer;
            if ((probe.result = context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submit, fence)) != VK_SUCCESS) return "vkQueueSubmit";
            submitted = true;
            if ((probe.result = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, 10'000'000'000ull)) != VK_SUCCESS) return "vkWaitForFences";
            submitted = false;
            return nullptr;
        };
        if (const char* step = execute(commands[0])) return step;
        if (!collect(probe.writtenAfterSubmit)) return "the collect after the submission";
        if (const char* step = execute(commands[0])) return step;
        if (!collect(probe.writtenAfterSecondSubmit)) return "the collect after the second submission";
        if (const char* step = execute(commands[1])) return step;
        if (!collect(probe.writtenAfterFill)) return "the collect after the fill";
        *reinterpret_cast<volatile std::uint8_t*>(base) = 2;
        if (!collect(probe.writtenByCpu)) return "the collect after a CPU store";
        for (std::uint64_t offset = 0; offset < bytes; offset += 4096) reinterpret_cast<volatile std::uint8_t*>(base)[offset] = 4;
        if (const char* step = execute(commands[2])) return step;
        for (std::uint64_t offset = 0; offset < bytes; offset += 4096) {
            if (reinterpret_cast<volatile std::uint8_t*>(base)[offset] != 5) ++probe.staleAfterFill;
        }
        return nullptr;
    };
    probe.failure = run();
    if (submitted) context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue);
    if (fence != VK_NULL_HANDLE) context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    if (commands[0] != VK_NULL_HANDLE) context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 3, commands);
    if (destination != VK_NULL_HANDLE) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, destination, nullptr);
    if (destinationMemory != VK_NULL_HANDLE) MemoryAccounting::Free(context, destinationMemory);
    if (import.buffer != VK_NULL_HANDLE) destroyImport(context, import);
}

#ifndef _WIN32
}
}
