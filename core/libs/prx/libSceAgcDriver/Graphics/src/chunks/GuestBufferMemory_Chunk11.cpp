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
namespace {
bool HostImportUsesDmaBuf(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (importsStale(context, state)) return false;
    const auto* entry = peekImport(state, address, address + bytes);
    return entry != nullptr && entry->dmaBuf;
}

const HostImport* ExistingHostImportFor(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return nullptr;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (importsStale(context, state)) return nullptr;
    return findImport(state, address, address + bytes);
}

bool WatchedAfterSettle(const Context& context, std::uint64_t address, std::size_t bytes) {
#ifdef _WIN32
    if (context.hostImportAlignment != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address && GuestMemory::GpuMutex().HeldByThisThread()) {
        ensurePinWaiter();
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        if (importsStale(context, state)) {
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            refreshImports(context, state, lease);
        }
        settleImportsNow(context, state, address, address + bytes);
    }
#endif
    return GuestMemory::Watched(address, bytes);
}

#ifndef _WIN32
namespace {

bool privateAnonymousReadWrite(std::uint64_t begin, std::uint64_t end) {
#ifdef PROCMAP_QUERY
    static const int mapsFile = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (mapsFile >= 0) {
        procmap_query query{};
        query.size = sizeof(query);
        query.query_addr = begin;
        if (ioctl(mapsFile, PROCMAP_QUERY, &query) == 0) return end <= query.vma_end && query.vma_flags == (PROCMAP_QUERY_VMA_READABLE | PROCMAP_QUERY_VMA_WRITABLE) && query.inode == 0;
        if (errno == ENOENT) return false;
    }
#endif
    std::FILE* maps = std::fopen("/proc/self/maps", "r");
    if (maps == nullptr) return false;
    bool covered = false;
    char line[512];
    while (std::fgets(line, sizeof(line), maps) != nullptr) {
        unsigned long long first = 0;
        unsigned long long last = 0;
        char permissions[5] = {};
        unsigned long long offset = 0;
        unsigned major = 0;
        unsigned minor = 0;
        unsigned long long inode = 0;
        if (std::sscanf(line, "%llx-%llx %4s %llx %x:%x %llu", &first, &last, permissions, &offset, &major, &minor, &inode) != 7 || begin < first || begin >= last) continue;
        covered = end <= last && std::strcmp(permissions, "rw-p") == 0 && inode == 0;
        break;
    }
    std::fclose(maps);
    return covered;
}

}
#endif

std::shared_ptr<HostImport> UnregisteredHostImport(const Context& context, std::uint64_t address, std::size_t bytes) {
    const auto alignment = context.hostImportAlignment;
    if (alignment == 0 || bytes == 0 || address > std::numeric_limits<std::uint64_t>::max() - bytes - 2 * alignment) return nullptr;
    const auto base = address / alignment * alignment;
    const auto end = (address + bytes + alignment - 1) / alignment * alignment;
    if (const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(static_cast<std::uintptr_t>(base), true); range != nullptr && range->address < end) return nullptr;
#ifdef _WIN32
    const void* allocation = nullptr;
    for (std::uint64_t cursor = base; cursor < end;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0 || info.State != MEM_COMMIT || info.Type != MEM_PRIVATE || info.Protect != PAGE_READWRITE) return nullptr;
        if (allocation == nullptr) allocation = info.AllocationBase;
        if (info.AllocationBase != allocation) return nullptr;
        cursor = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
    }
#else
    if (!privateAnonymousReadWrite(base, end)) return nullptr;
#endif
    std::map<std::uint64_t, HostImport> made;
    auto& entry = made.emplace(base, HostImport{base, end - base, VK_NULL_HANDLE, VK_NULL_HANDLE, 0}).first->second;
    VkResult failure = VK_SUCCESS;
    if (createHostPointerImport(context, entry, failure) != nullptr) return nullptr;
    auto holder = std::make_shared<RetiredImport>(context.device, context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer"), context.Function<PFN_vkFreeMemory>("vkFreeMemory"), made.extract(base));
    return std::shared_ptr<HostImport>(holder, &holder->node.mapped());
}

bool ImportMappedRanges(const Context& context, const GuestAllocations::Mapped& ranges, std::uint64_t generation, bool adoptDevice) {
    if (context.hostImportAlignment == 0) return true;
    ensurePinWaiter();
    const bool withoutGpu = !GuestMemory::GpuMutex().HeldByThisThread();
    auto& state = Imports();
    std::unique_lock lock(state.mutex);
    if (!adoptDevice && (state.device != context.device || state.watchDevice != context.device)) return false;
    const bool adopting = state.device != context.device;
    GuestAllocations::Lease current;
    if (adopting) {
        current = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        refreshImports(context, state, current);
    } else if (state.refreshedGeneration > generation) {
        return true;
    }
    for (const auto& mapped : ranges) {
        const auto range = mapped.lock();
        if (range == nullptr || range->bytes <= 0x10000) continue;
        if (current.empty()) current = GuestAllocations::GuestAllocationsAcquireMapped_nid_no_patch(ranges);
        if (leasedRangeOwner(current, range->address) != range) continue;
        try {
            auto run = importRun(current, range->address, std::numeric_limits<std::uint64_t>::max(), effectiveBudget(context, state));
            std::optional<MapImportGate::ImportHold> hold;
            if (withoutGpu) {
                auto stripes = MapImportGate::StripesOf(run.base, run.base + run.bytes);
                bool owned = true;
                for (;;) {
                    lock.unlock();
                    hold.reset();
                    hold.emplace(stripes);
                    MapImportGate::RunUnlockedHookForTest();
                    lock.lock();
                    if (state.device != context.device || state.watchDevice != context.device) return false;
                    if (state.refreshedGeneration > generation) return true;
                    current = GuestAllocations::GuestAllocationsAcquireMapped_nid_no_patch(ranges);
                    owned = leasedRangeOwner(current, range->address) == range;
                    if (!owned) break;
                    run = importRun(current, range->address, std::numeric_limits<std::uint64_t>::max(), effectiveBudget(context, state));
                    if (hold->Covers(run.base, run.base + run.bytes)) break;
                    MapImportGate::CountRetry();
                    stripes |= MapImportGate::StripesOf(run.base, run.base + run.bytes);
                }
                if (!owned) continue;
            }
            const bool replacing = wouldRetireImport(state, run);
            if (withoutGpu) {
                const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> span{{{run.base, run.base + run.bytes}}};
                if (replacing || StorageTexture::UnwatchHookLocksGpu(span)) return false;
            } else if (replacing) {
                current = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
                if (leasedRangeOwner(current, range->address) != range) continue;
            }
            if (!state.importedAtMap.emplace(range->address, range->bytes).second && findImport(state, range->address, range->address + range->bytes) == nullptr) continue;
            importAllocation(context, state, range->address, current, ImportRequester::Map, 0, false);
        } catch (const DmaBufImportFailure&) {
            throw;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] import of GPU memory mapped at 0x%llx+0x%zx failed: %s\n", static_cast<unsigned long long>(range->address), range->bytes, error.what());
        }
    }
    return true;
}

bool RegisteredReadableCovers(std::uint64_t address, std::size_t bytes, std::vector<RegisteredPiece>* pieces) {
    if (pieces != nullptr) pieces->clear();
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    const auto lease = GuestAllocations::GuestAllocationsAcquireRegistered_nid_postfix();
    const auto end = address + bytes;
    for (auto at = address; at < end;) {
        const auto* range = containingRange(lease, at, at + 1);
        if (range == nullptr) return false;
        if (pieces != nullptr) pieces->push_back({range->address, range->bytes, range->writable, range->gpu});
        at = range->address + range->bytes;
    }
    return true;
}

std::uint64_t RegisteredReadableEnd(std::uint64_t address) {
    if (address == std::numeric_limits<std::uint64_t>::max()) return 0;
    const auto lease = GuestAllocations::GuestAllocationsAcquireRegistered_nid_postfix();
    const auto* range = containingRange(lease, address, address + 1);
    return range != nullptr ? range->address + range->bytes : 0;
}

std::uint64_t HostImportSetGeneration() {
    return importSetGeneration.load(std::memory_order_acquire);
}

bool HostImportCovers(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    // Imports of a replaced device are not this device's; a stale registry only means an import
    // may be dropped at the next reconcile, which the binding path handles.
    return state.device == context.device && peekImport(state, address, address + bytes) != nullptr;
}

bool HostImportOverlaps(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (state.device != context.device) return false;
    const auto next = state.imports.lower_bound(address);
    if (next != state.imports.end() && next->first < address + bytes) return true;
    return next != state.imports.begin() && std::prev(next)->second.base + std::prev(next)->second.bytes > address;
}

bool ComparedHostImportCovers(const Context& context, std::uint64_t address, std::size_t bytes) {
#ifdef _WIN32
    static_cast<void>(context);
    static_cast<void>(address);
    static_cast<void>(bytes);
    return false;
#else
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    const auto* import = state.device == context.device ? peekImport(state, address, address + bytes) : nullptr;
    return import != nullptr && import->unwatched;
#endif
}

GuestBufferMemory::GuestBufferMemory(const Context& context) : context(context) {}

bool GuestBufferMemory::WritesOverlap(std::uint64_t address, std::size_t bytes) const {
    return std::any_of(writes.begin(), writes.end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; });
}

void GuestBufferMemory::CountAddressBuild(double snapshotsUs) {
    auto& totals = AddressBuilds();
    auto& timing = ThreadAddressTiming();
    std::lock_guard lock(totals.mutex);
    ++totals.builds;
    totals.sums.leaseUs += timing.leaseUs;
    totals.sums.importsUs += timing.importsUs;
    totals.sums.mirrorsUs += timing.mirrorsUs;
    totals.sums.compareUs += timing.compareUs;
    totals.sums.blocksCompared += timing.blocksCompared;
    totals.sums.blocksCopied += timing.blocksCopied;
    totals.snapshotsUs += snapshotsUs;
    timing = {};
    const auto now = std::chrono::steady_clock::now();
    if (now - totals.lastReport < std::chrono::seconds(10)) return;
    totals.lastReport = now;
    const auto per = [&](double us) { return us / static_cast<double>(totals.builds); };
    const auto table = BdaResources::TableCacheCounters();
    const auto& seen = totals.tableSeen;
    const auto delta = [](std::uint64_t now, std::uint64_t before) { return static_cast<unsigned long long>(now - before); };
    const auto space = AddressSpaceCounters();
    const auto& spaceSeen = totals.spaceSeen;
    AgcDriver::ProfilePrint_nid_no_patch("[address] %llu address-based builds (10 s), us per build: lease %.0f, imports pass %.0f, mirror prepare %.0f, compare %.0f (%.0f blocks, %.1f copied), snapshots %.0f; space hits %llu (patched after an import change %llu) / rebuilds: generation %llu, epoch %llu, growth %llu, device %llu, waiter drop %llu, first %llu; unpublished %llu, dissolved: overlap %llu, imports %llu%s; [bda-table] hits %llu / misses %llu (space tables %llu), first entry: expired %llu, hash differs low %llu / heap %llu, same hash %llu, none held %llu\n", static_cast<unsigned long long>(totals.builds), per(totals.sums.leaseUs), per(totals.sums.importsUs), per(totals.sums.mirrorsUs), per(totals.sums.compareUs), per(static_cast<double>(totals.sums.blocksCompared)), per(static_cast<double>(totals.sums.blocksCopied)), per(totals.snapshotsUs), delta(space.hits, spaceSeen.hits), delta(space.patchedEpoch, spaceSeen.patchedEpoch), delta(space.rebuiltGeneration, spaceSeen.rebuiltGeneration), delta(space.rebuiltEpoch, spaceSeen.rebuiltEpoch), delta(space.rebuiltGrowth, spaceSeen.rebuiltGrowth), delta(space.rebuiltDevice, spaceSeen.rebuiltDevice), delta(space.rebuiltWaiterDrop, spaceSeen.rebuiltWaiterDrop), delta(space.rebuiltFirst, spaceSeen.rebuiltFirst), delta(space.unpublished, spaceSeen.unpublished), delta(space.dissolvedOverlap, spaceSeen.dissolvedOverlap), delta(space.dissolvedImports, spaceSeen.dissolvedImports), space.enabled ? "" : " (cache off)", delta(table.hits, seen.hits), delta(table.misses, seen.misses), delta(table.spaceTables, seen.spaceTables), delta(table.firstExpired, seen.firstExpired), delta(table.firstDiffersLow, seen.firstDiffersLow), delta(table.firstDiffersHeap, seen.firstDiffersHeap), delta(table.firstSameHash, seen.firstSameHash), delta(table.firstEmpty, seen.firstEmpty));
    totals.tableSeen = table;
    totals.spaceSeen = space;
    totals.builds = 0;
    totals.sums = {};
    totals.snapshotsUs = 0;
}

bool GuestBufferMemory::HoldsMirror(const std::shared_ptr<ImageMirror>& mirror) const {
    for (const auto& region : regions) {
        if (region.mirror == mirror) return true;
    }
    return false;
}

bool GuestBufferMemory::EvictMirror(const std::shared_ptr<ImageMirror>& mirror) {
    bool found = false;
    for (auto& region : regions) {
        if (region.mirror == mirror) {
            region.mirror.reset();
            region.hostBacked = !region.writable;
            auto committed = GuestMemory::DescribeCommitted(region.begin, region.end - region.begin);
            if (!committed.whole) {
                region.sparse = true;
                region.backed = std::move(committed.ranges);
            } else {
                region.sparse = false;
                region.backed.clear();
            }
            found = true;
        }
    }
    return found;
}

bool GuestBufferMemory::WasEvicted(std::uint64_t address) const {
    return std::find(evictedRanges.begin(), evictedRanges.end(), address) != evictedRanges.end();
}

void GuestBufferMemory::RecordEvicted(std::uint64_t address) {
    if (!WasEvicted(address)) evictedRanges.push_back(address);
}

namespace {

void stampSpaceUse(HostImports& state, const GuestBufferMemory::AddressSpace& space) {
    const auto stamp = ++state.useCounter;
    static const bool stampOnce = std::getenv("APS5_NO_SPACE_STAMP_ONCE") == nullptr;
    auto& spaceUse = state.spaceUses[space.serial];
    if (!stampOnce || spaceUse == 0) {
        for (const auto& region : space.base) {
            if (region.direct == nullptr) continue;
            region.direct->lastUse = stamp;
            if (stampOnce) region.direct->spaceSerial = space.serial;
        }
    }
    spaceUse = stamp;
    if (state.spaceUses.size() > 64) {
        const auto oldest = std::min_element(state.spaceUses.begin(), state.spaceUses.end(), [](const auto& left, const auto& right) { return left.second < right.second; });
        for (auto& [base, entry] : state.imports) {
            if (entry.spaceSerial != oldest->first) continue;
            entry.lastUse = std::max(entry.lastUse, oldest->second);
            entry.spaceSerial = 0;
        }
        state.spaceUses.erase(oldest);
    }
}

}

void GuestBufferMemory::AcquireRegistered() {
    struct BuildingScope {
        explicit BuildingScope(GuestBufferMemory* self) { buildingBufferMemory = self; }
        ~BuildingScope() {
            buildingBufferMemory = nullptr;
            buildingBlocks = nullptr;
        }
    };
    const BuildingScope buildingScope(this);
    Require(!uploaded && regions.empty() && lease.empty() && space == nullptr, "guest allocation lease must precede resource registration");
    ensurePinWaiter();
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& timing = ThreadAddressTiming();
    const auto lap = [&](double& into, std::chrono::steady_clock::time_point& from) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        into += std::chrono::duration<double, std::micro>(now - from).count();
        from = now;
    };
    auto at = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool importable = context.hostImportAlignment != 0;
    // APS5_NO_SORTED_SNAPSHOT_LOOKUP=1: AddSnapshot scans the regions as before instead of searching.
    static const bool sortedLookup = std::getenv("APS5_NO_SORTED_SNAPSHOT_LOOKUP") == nullptr;
    std::vector<RefreshBlock> blocks;
    buildingBlocks = &blocks;
    std::vector<ImageMirror*> heaps;
    bool mirrored = false;
    bool served = false;
    auto& spaces = Spaces();
    // Read before the lease below is acquired: a mutation ending in between leaves the space
    // stale by its generation, never newer than it (see AddressSpace).
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (addressSpaceCacheEnabled()) {
        auto current = spaces.current.load();
        const auto published = current;
        bool hit = false;
        static const bool patchSpaces = std::getenv("APS5_NO_SPACE_PATCH") == nullptr;
        const auto patchedSpace = [&](HostImports& state, std::shared_ptr<const AddressSpace> old) -> std::shared_ptr<const AddressSpace> {
            if (!patchSpaces) return nullptr;
            if (importable) refreshImports(context, state, old->lease);
            if (old->generation != GuestAllocations::GuestAllocationsGeneration_nid_postfix()) return nullptr;
            auto built = std::make_shared<AddressSpace>();
            built->base = old->base;
            for (auto& region : built->base) {
                if (region.direct == nullptr) continue;
                const auto* entry = peekImport(state, region.begin, region.end);
                if (entry == nullptr) return nullptr;
                region.direct = entry;
            }
            built->generation = old->generation;
            built->importsEpoch = state.epoch;
            built->device = old->device;
            built->serial = spaces.serials.fetch_add(1, std::memory_order_relaxed) + 1;
            built->lease = old->lease;
            built->copied = old->copied;
            built->directReads = old->directReads;
            built->ranges.reserve(built->base.size());
            for (const auto& region : built->base) built->ranges.push_back(addressRange(region));
            planSpaceMirrors(*built);
            const auto imported = [&](ImageMirror* mirror) { return importsNow(context, state, old->lease, mirror->base); };
            if (std::any_of(built->heapMirrors.begin(), built->heapMirrors.end(), imported) || std::any_of(built->sparseMirrors.begin(), built->sparseMirrors.end(), imported)) return nullptr;
            for (std::size_t index = 0; index < built->heapRuns.size() && index < old->heapRuns.size(); ++index) built->verified[index].store(old->verified[index].load(std::memory_order_relaxed), std::memory_order_relaxed);
            return built;
        };
        {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            const auto keeps = [&](ImageMirror* mirror) { return heapMirrorWatched(*mirror) || !importsNow(context, state, current->lease, mirror->base); };
            if (current == nullptr) (spaces.droppedByWaiter.exchange(false, std::memory_order_relaxed) ? spaces.rebuiltWaiterDrop : spaces.rebuiltFirst).fetch_add(1, std::memory_order_relaxed);
            else if (current->device != context.device) spaces.rebuiltDevice.fetch_add(1, std::memory_order_relaxed);
            else if (current->generation != generation) spaces.rebuiltGeneration.fetch_add(1, std::memory_order_relaxed);
            else if (current->importsEpoch != state.epoch && (current = patchedSpace(state, std::move(current))) == nullptr) spaces.rebuiltEpoch.fetch_add(1, std::memory_order_relaxed);
            else if (state.growthDue || (state.growthFloor != std::numeric_limits<std::uint64_t>::max() && retiredPinnedReleased.load(std::memory_order_acquire))) spaces.rebuiltGrowth.fetch_add(1, std::memory_order_relaxed);
            else hit = std::all_of(current->heapMirrors.begin(), current->heapMirrors.end(), keeps) && std::all_of(current->sparseMirrors.begin(), current->sparseMirrors.end(), keeps);
            if (hit && current != published) {
                spaces.patchedEpoch.fetch_add(1, std::memory_order_relaxed);
                auto expected = published;
                spaces.current.compare_exchange_strong(expected, current);
            }
            importFloor = state.useCounter + 1;
            if (hit) {
                stampSpaceUse(state, *current);
            }
        }
        if (hit) {
            for (auto* mirror : current->sparseMirrors) {
                if (refreshSparseMirror(*mirror, blocks)) continue;
                hit = false;
                blocks.clear();
                spaces.current.compare_exchange_strong(current, std::shared_ptr<const AddressSpace>{});
                break;
            }
        }
        if (hit) {
            spaces.hits.fetch_add(1, std::memory_order_relaxed);
            ensureRetention();
            space = std::move(current);
            lap(timing.leaseUs, at);
            for (const auto& range : space->copied) addCopiedRange(range);
            regionsSorted = sortedLookup;
            lap(timing.importsUs, at);
            // Writable mirrors are compared with guest memory per build, as acquireMirror does for
            // an existing mirror. The preparation may wait for recorded work, whose completions can
            // retire an import the space's regions point at: UploadFinish re-checks the epoch
            // before any of them is dereferenced.
            served = true;
            mirrored = space->mirrored;
            for (const auto index : space->writableMirrors) {
                const auto& region = space->base[index];
                if (prepareRefresh(*region.mirror, region.begin, region.end - region.begin)) appendBlocks(blocks, *region.mirror, region.begin, region.end - region.begin);
            }
            lap(timing.mirrorsUs, at);
        }
    }
    if (space == nullptr) {
        // Pending GPU results in these ranges are stored when Upload prepares each region.
        lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        lap(timing.leaseUs, at);
        {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            if (importable) refreshImports(context, state, lease);
            if (importFloor == 0) importFloor = state.useCounter + 1;
            state.buildFloor = importFloor;
            if (importable) promoteGrowth(context, state, lease);
            // Taken before the pass: the imports it takes are the registry's ones (just reconciled, so
            // none is retired by a make below), and a mirror's preparation may sync the recorder, whose
            // completions can retire an import already taken; that moves the epoch, and Upload then
            // re-checks every pointer instead of trusting it.
            importsEpoch = state.epoch;
        }
        lap(timing.importsUs, at);
        // One pass in the registry's (ascending) order, so the regions come out sorted for AddSnapshot.
        // The import lock is taken per range: a mirror's preparation may wait for recorded work, which
        // must not happen under it.
        regions.reserve(lease.size());
        static const std::uint64_t copyLimit = [] {
            const char* value = std::getenv("APS5_ADDRESS_COPY_MAX_MIB");
            return (value ? std::strtoull(value, nullptr, 10) : 64ull) << 20u;
        }();
        const auto copyOverride = addressCopyLimitOverride.load(std::memory_order_relaxed);
        const auto cap = copyOverride != 0 ? copyOverride : copyLimit;
        std::vector<AddressCopy> copies;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> imported;
        const HostImport* covering = nullptr;
        std::uint64_t coveredBegin = 0;
        std::uint64_t coveredEnd = 0;
        std::uint64_t copiedBytes = 0;
        for (const auto& range : lease) {
            if (!range->readable || !range->gpu) continue;
            validate(range->address, range->bytes);
            Region region{range->address, range->address + range->bytes, range->writable, {}, nullptr};
            const HostImport* entry = nullptr;
            bool overBudget = false;
            std::vector<ImportPiece> pieces;
            const auto limit = importMaxBytes.load(std::memory_order_relaxed);
            if (importable && covering != nullptr && range->address >= coveredBegin && range->address + range->bytes <= coveredEnd) {
                entry = covering;
            } else if (importable && limit != 0 && range->bytes > limit) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                pieces = importPieces(context, state, *range, lease);
            } else if (importable) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                const auto refusals = state.budgetRefusals;
                entry = importAllocation(context, state, range->address, lease, ImportRequester::Build, 0, false);
                overBudget = state.budgetRefusals != refusals;
                covering = entry;
                if (entry != nullptr) {
                    coveredBegin = entry->base;
                    coveredEnd = entry->base + entry->bytes;
                }
            }
            if (importable && mirrorsEnabled() && entry == nullptr && pieces.empty()) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                pieces = budgetedImportPieces(context, state, *range, lease);
            }
            lap(timing.importsUs, at);
            if (!pieces.empty()) {
                const auto firstImported = imported.size();
                for (const auto& piece : pieces) if (piece.entry != nullptr) imported.emplace_back(piece.begin, piece.end);
                releaseImportedMirrors(context, std::span(imported).subspan(firstImported));
                for (const auto& piece : pieces) {
                    Region part{piece.begin, piece.end, range->writable, {}, nullptr};
                    if (piece.entry != nullptr) {
                        part.hostBacked = true;
                        part.direct = piece.entry;
                    } else {
                        part.mirror = acquireMirror(context, range, blocks, true, piece.begin, piece.end - piece.begin);
                        lap(timing.mirrorsUs, at);
                        Require(part.mirror != nullptr, "a partial heap import has no mirror for its remaining bytes");
                        mirrored = true;
                        if (part.mirror->sparse) {
                            part.sparse = true;
                            part.backed = part.mirror->committed;
                        }
                    }
                    regions.push_back(std::move(part));
                }
                continue;
            }
            if (entry != nullptr) {
                region.hostBacked = true;
                // Reused by Upload while no import was dropped since (see importsEpoch).
                region.direct = entry;
                regions.push_back(std::move(region));
                imported.emplace_back(range->address, range->address + range->bytes);
                continue;
            }
            if (mirrorsEnabled() && !WasEvicted(range->address)) {
                region.mirror = acquireMirror(context, range, blocks, range->releasable);
                lap(timing.mirrorsUs, at);
                if (region.mirror != nullptr) {
                    mirrored = true;
                    if (region.mirror->sparse) {
                        region.sparse = true;
                        region.backed = region.mirror->committed;
                    }
                    regions.push_back(std::move(region));
                    continue;
                }
            }
            addCopiedRange({range->address, range->address + range->bytes, range->writable});
            const auto& copied = regions.back();
            std::uint64_t committed = copied.end - copied.begin;
            if (copied.sparse) {
                committed = 0;
                for (const auto& [first, last] : copied.backed) committed += last - first;
            }
            if (overBudget && copiedBytes + committed > cap) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                {
                    const MirrorsYield yield(state, state.buildFloor);
                    entry = importAllocation(context, state, range->address, lease, ImportRequester::Build, 0, true);
                }
                if (entry != nullptr) {
                    covering = entry;
                    coveredBegin = entry->base;
                    coveredEnd = entry->base + entry->bytes;
                }
                static auto lastReport = std::chrono::steady_clock::now() - std::chrono::seconds(60);
                if (entry != nullptr && std::chrono::steady_clock::now() - lastReport > std::chrono::seconds(10)) {
                    lastReport = std::chrono::steady_clock::now();
                    std::fprintf(stderr, "[gpu] host import budget: 0x%llx+0x%llx imported by evicting idle imports, as copying it would pass APS5_ADDRESS_COPY_MAX_MIB\n", static_cast<unsigned long long>(range->address), static_cast<unsigned long long>(range->bytes));
                }
            }
            if (entry != nullptr) {
                regions.pop_back();
                region.hostBacked = true;
                region.direct = entry;
                regions.push_back(std::move(region));
                imported.emplace_back(range->address, range->address + range->bytes);
                continue;
            }
            copiedBytes += committed;
            const char* reason = !mirrorsEnabled() ? "mirrors disabled by APS5_NO_LEASE_MIRROR" : copied.sparse ? "unreadable pages" : !range->releasable ? "image mirror refused" : nullptr;
            copies.push_back({copied.begin, copied.end, committed, reason, reason == nullptr, overBudget});
        }
        releaseImportedMirrors(context, imported);
        for (const auto& region : regions) {
            if (region.mirror != nullptr && region.mirror->heap && !region.mirror->sparse) heaps.push_back(region.mirror.get());
        }
        regionsSorted = sortedLookup;
        if (!copies.empty()) {
            std::uint64_t copied = 0;
            for (const auto& copy : copies) copied += copy.committed;
            AddressCopyCounters::ThisThread().Count(copied);
        }
        if (const auto overflow = AddressCopyOverflow(std::move(copies), cap); !overflow.empty()) {
            std::fprintf(stderr, "FATAL: %s\n", overflow.c_str());
            std::fflush(stderr);
            std::abort();
        }
        lap(timing.importsUs, at);
        if (addressSpaceCacheEnabled()) {
            // Published only while the imports the regions point at stand: the table entries below
            // read them, and a retire during the pass (a mirror preparation's sync) has already
            // moved the epoch, in which case this build keeps the per-build form and Upload
            // re-resolves every pointer as before.
            std::vector<ShaderRecompiler::BdaAbi::Range> ranges;
            bool publish = true;
            {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                if (state.epoch != importsEpoch) {
                    publish = false;
                    spaces.unpublished.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::size_t kept = 0;
                    for (std::size_t index = 0; index < regions.size(); ++index) {
                        auto& region = regions[index];
                        if (kept != 0) {
                            auto& previous = regions[kept - 1];
                            if (region.direct != nullptr && previous.direct == region.direct && previous.end == region.begin && previous.writable == region.writable && region.mirror == nullptr && previous.mirror == nullptr && !region.sparse && !previous.sparse && region.end <= region.direct->base + region.direct->bytes) {
                                previous.end = region.end;
                                continue;
                            }
                        }
                        if (kept != index) regions[kept] = std::move(region);
                        ++kept;
                    }
                    regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(kept), regions.end());
                    for (const auto& region : regions) {
                        if (region.direct != nullptr || region.mirror != nullptr) ranges.push_back(addressRange(region));
                    }
                }
            }
            if (publish) {
                auto built = std::make_shared<AddressSpace>();
                built->generation = generation;
                built->importsEpoch = importsEpoch;
                built->device = context.device;
                built->serial = spaces.serials.fetch_add(1, std::memory_order_relaxed) + 1;
                built->lease = std::move(lease);
                built->ranges = std::move(ranges);
                std::vector<Region> extras;
                for (auto& region : regions) {
                    if (region.direct != nullptr || region.mirror != nullptr) {
                        built->base.push_back(std::move(region));
                    } else {
                        built->copied.push_back({region.begin, region.end, region.writable});
                        extras.push_back(std::move(region));
                    }
                }
                regions = std::move(extras);
                lease.clear();
                planSpaceMirrors(*built);
                {
                    auto reads = std::make_shared<Recorder::ReadList>();
                    for (const auto& region : built->base) {
                        if (region.direct != nullptr) reads->emplace_back(region.begin, region.end);
                    }
                    if (Recorder::ValidReadList(*reads)) built->directReads = std::move(reads);
                }
                ensureRetention();
                space = std::move(built);
                spaces.droppedByWaiter.store(false, std::memory_order_relaxed);
                if (evictedRanges.empty()) spaces.current.store(space);
            }
        }
    }
    std::vector<std::pair<std::size_t, std::uint64_t>> checked;
    if (served) checked = refreshHeapRuns(*space, blocks);
    else refreshHeapMirrors(heaps, blocks);
    lap(timing.mirrorsUs, at);
    const auto copiedBefore = Mirrors().blocksCopied;
    compareBlocks(blocks);
    for (const auto& [run, collected] : checked) space->verified[run].store(collected, std::memory_order_relaxed);
    lap(timing.compareUs, at);
    if (profile) {
        timing.blocksCompared += blocks.size();
        timing.blocksCopied += Mirrors().blocksCopied - copiedBefore;
    }
    if (mirrorsEnabled()) {
        if (mirrored) ++Mirrors().builds;
        sweepMirrors();
        reportMirrors();
    }
}

}
}
}
