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
const GuestAllocations::Range* containingRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto found = std::upper_bound(lease.begin(), lease.end(), begin, [](std::uint64_t value, const auto& range) { return value < range->address; });
    if (found == lease.begin()) return nullptr;
    const auto& range = *std::prev(found);
    return range->readable && begin >= range->address && end <= range->address + range->bytes ? range.get() : nullptr;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> changedSpans(std::vector<RefreshedRange>& refreshed, const GuestAllocations::Lease& lease) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spans;
    std::vector<RefreshedRange> next;
    next.reserve(lease.size());
    const auto add = [&](std::uint64_t address, std::uint64_t bytes) {
        if (!spans.empty() && address <= spans.back().second) spans.back().second = std::max(spans.back().second, address + bytes);
        else spans.emplace_back(address, address + bytes);
    };
    const auto take = [&](const std::shared_ptr<const GuestAllocations::Range>& range) {
        add(range->address, range->bytes);
        next.push_back({range->address, range->bytes, range});
    };
    std::size_t first = 0;
    std::size_t second = 0;
    while (first < refreshed.size() || second < lease.size()) {
        if (second == lease.size() || (first < refreshed.size() && refreshed[first].address < lease[second]->address)) {
            add(refreshed[first].address, refreshed[first].bytes);
            ++first;
        } else if (first == refreshed.size() || lease[second]->address < refreshed[first].address) {
            take(lease[second++]);
        } else {
            if (refreshed[first].range.owner_before(lease[second]) || lease[second].owner_before(refreshed[first].range)) {
                add(refreshed[first].address, refreshed[first].bytes);
                take(lease[second]);
            } else {
                next.push_back(std::move(refreshed[first]));
            }
            ++first;
            ++second;
        }
    }
    refreshed = std::move(next);
    return spans;
}

void refreshImports(const Context& context, HostImports& state, const GuestAllocations::Lease& lease) {
    if (state.device != context.device) {
#ifdef _WIN32
        drainSettles(state);
#endif
        while (!state.imports.empty()) {
            auto node = state.imports.extract(state.imports.begin());
            if (state.device != VK_NULL_HANDLE && state.destroyBuffer != nullptr && state.freeMemory != nullptr) {
                auto parked = std::make_shared<RetiredImport>(state.device, state.destroyBuffer, state.freeMemory, std::move(node));
                parked->Pin();
                state.parked[state.device].push_back(std::move(parked));
                continue;
            }
#ifdef _WIN32
            GuestArena::GuestArenaUnmapAlias_nid_postfix(node.mapped().alias);
#endif
        }
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
        state.sparseRunEnds.clear();
        state.sparseRunJoins.clear();
        state.importedAtMap.clear();
        state.device = context.device;
        state.destroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
        state.freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
        state.refreshedRanges.clear();
        state.refreshedWindow = 0;
        state.pendingImports.clear();
        state.growth.clear();
        state.growthFloor = std::numeric_limits<std::uint64_t>::max();
        state.growthDue = false;
        state.refreshedGeneration = 0;
        bumpEpoch(state);
    }
    if (state.destroyBuffer == nullptr) state.destroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
    if (state.freeMemory == nullptr) state.freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (generation == state.refreshedGeneration) return;
    state.refreshedGeneration = generation;
    state.gateRefusals.clear();
    const auto budget = effectiveBudget(context, state);
    const auto window = importWindow.load(std::memory_order_relaxed);
    const bool whole = window == 0 || window != state.refreshedWindow;
    const auto changed = changedSpans(state.refreshedRanges, lease);
    state.refreshedWindow = window;
    auto pending = std::exchange(state.pendingImports, {});
    const auto affected = [&](std::uint64_t base, std::uint64_t bytes) {
        if (whole) return true;
        const auto low = base / window * window;
        const auto high = std::max(low + window, base + bytes);
        const auto found = std::upper_bound(changed.begin(), changed.end(), low, [](std::uint64_t value, const auto& span) { return value < span.second; });
        return found != changed.end() && found->first < high;
    };
    const auto starts = [&](std::uint64_t address) {
        const auto index = leasedIndexContaining(lease, address);
        return index != lease.size() && lease[index]->address == address;
    };
    std::erase_if(state.sparseRunEnds, [&](const auto& entry) { return affected(entry.first, 1) && !starts(entry.first); });
    std::erase_if(state.sparseRunJoins, [&](const auto& entry) { return affected(entry.first, 1) && !starts(entry.first); });
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwatched;
    std::vector<std::uint64_t> kept;
    const auto check = [&](std::map<std::uint64_t, HostImport>::iterator it) {
        if (sameRange(it->second, lease)) {
            if (it->second.unwatched) GuestMemory::Unwatch(it->first, it->second.bytes);
            kept.push_back(it->first);
            return std::next(it);
        }
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        if (trace && leasedRangeOwner(lease, it->first, it->first + it->second.bytes) != nullptr) std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx retired: the range was mapped again\n", static_cast<unsigned long long>(it->first), static_cast<unsigned long long>(it->second.bytes));
        const auto next = std::next(it);
        noteRetiredUnwatched(unwatched, it->second, lease);
        retireImport(context.device, state, it, lease);
        return next;
    };
    if (whole) {
        for (auto it = state.imports.begin(); it != state.imports.end();) it = check(it);
    } else {
        std::uint64_t unchecked = 0;
        for (const auto& [begin, end] : changed) {
            auto it = state.imports.lower_bound(std::max(begin / window * window, unchecked));
            if (it != state.imports.begin() && std::prev(it)->first >= unchecked && std::prev(it)->first + std::prev(it)->second.bytes > begin) --it;
            while (it != state.imports.end() && it->first / window * window < end) {
                pending.erase(it->first);
                it = check(it);
            }
            unchecked = it == state.imports.end() ? std::numeric_limits<std::uint64_t>::max() : it->first;
        }
        for (const auto base : pending) {
            if (const auto it = state.imports.find(base); it != state.imports.end()) check(it);
        }
    }
    rewatchRetired(state, std::move(unwatched));
    for (auto it = state.failed.begin(); it != state.failed.end();) it = !affected(it->second.base, it->second.bytes) || sameRange(it->second, lease, budget) ? std::next(it) : state.failed.erase(it);
    for (auto it = state.failedJoins.begin(); it != state.failedJoins.end();) it = !affected(it->second.base, it->second.bytes) || sameRun(it->second, joinedRun(lease, it->second.base, it->second.base + it->second.bytes)) ? std::next(it) : state.failedJoins.erase(it);
    for (auto it = state.failedBlocks.begin(); it != state.failedBlocks.end();) it = !affected(it->first.first, it->first.second) || sameRun(it->second, exactRun(lease, it->first.first, it->first.second)) ? std::next(it) : state.failedBlocks.erase(it);
    for (auto it = state.refusals.begin(); it != state.refusals.end();) it = !affected(it->first.first, it->first.second) || (stillRefused(state, &it->second, budget) && sameRun(it->second, exactRun(lease, it->first.first, it->first.second))) ? std::next(it) : state.refusals.erase(it);
    for (auto it = state.evictingRefusals.begin(); it != state.evictingRefusals.end();) it = !affected(it->first.first, it->first.second) || (evictingRefusalCurrent(state, it->second, budget) && sameRun(it->second, exactRun(lease, it->first.first, it->first.second))) ? std::next(it) : state.evictingRefusals.erase(it);
    const auto alignment = context.hostImportAlignment;
    for (auto it = state.growth.begin(); it != state.growth.end();) {
        if (!affected(it->first, it->second.end - it->first)) {
            ++it;
            continue;
        }
        const auto desired = desiredRun(lease, it->first, alignment);
        if (!desired || desired->first != it->first || desired->second != it->second.end || peekImport(state, it->first, it->second.end) != nullptr) {
            it = state.growth.erase(it);
            continue;
        }
        if (it->second.blocked == GrowthBlock::Failed) it->second.blocked = GrowthBlock::Untried;
        ++it;
    }
    for (const auto base : kept) {
        const auto found = state.imports.find(base);
        if (found == state.imports.end() || found->second.joined) continue;
        const auto desired = desiredRun(lease, base, alignment);
        if (!desired || desired->first > base || base + found->second.bytes > desired->second || peekImport(state, desired->first, desired->second) != nullptr) continue;
        auto& run = growthAt(state, desired->first, desired->second)->second;
        if (run.blocked == GrowthBlock::Failed) run.blocked = GrowthBlock::Untried;
    }
    resetGrowthFloor(state);
}

void promoteGrowth(const Context& context, HostImports& state, const GuestAllocations::Lease& lease) {
    state.growthDue = false;
    retiredPinnedReleased.store(false, std::memory_order_release);
    if (state.growth.empty()) return;
    const auto budget = effectiveBudget(context, state);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> due;
    for (const auto& [begin, run] : state.growth) {
        if (run.blocked == GrowthBlock::Untried || run.blocked == GrowthBlock::Held || (run.blocked == GrowthBlock::Budget && (budget > run.budget || pinnedTotal(state) < run.pinnedBytes))) due.emplace_back(begin, run.end);
    }
    const auto owed = [&](std::uint64_t begin, std::uint64_t end) {
        const auto found = state.growth.find(begin);
        return found != state.growth.end() && found->second.end == end ? found : state.growth.end();
    };
    for (const auto& [begin, end] : due) {
        auto found = owed(begin, end);
        if (found == state.growth.end()) continue;
        found->second.blocked = GrowthBlock::Failed;
        importAllocation(context, state, begin, lease, ImportRequester::Build, 0, false);
        found = owed(begin, end);
        if (found == state.growth.end()) continue;
        const auto desired = desiredRun(lease, begin, context.hostImportAlignment);
        if (!desired || desired->first != begin || desired->second != end || peekImport(state, begin, end) != nullptr) {
            state.growth.erase(found);
            continue;
        }
        if (const auto refused = state.refusals.find({begin, end - begin}); found->second.blocked == GrowthBlock::Failed && refused != state.refusals.end()) found->second = GrowthRun{end, GrowthBlock::Budget, refused->second.budget, refused->second.pinnedBytes};
    }
    resetGrowthFloor(state);
    state.growthDue = false;
}

const HostImport* peekImport(const HostImports& state, std::uint64_t begin, std::uint64_t end) {
    auto found = state.imports.upper_bound(begin);
    if (found == state.imports.begin()) return nullptr;
    --found;
    const auto& entry = found->second;
    return begin < entry.base || end > entry.base + entry.bytes ? nullptr : &entry;
}

const HostImport* findImport(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    const auto* entry = peekImport(state, begin, end);
    if (entry != nullptr) entry->lastUse = ++state.useCounter;
    return entry;
}

const HostImport* importAccess(const Context& context, HostImports& state, std::uint64_t begin, std::uint64_t end, const GuestAllocations::Lease& lease, ImportRequester requester) {
    if (!runCovers(lease, begin, end)) return findImport(state, begin, end);
    const auto* entry = importAllocation(context, state, begin, lease, requester, end);
    return entry != nullptr && findImport(state, begin, end) == entry ? entry : nullptr;
}

bool importsStale(const Context& context, const HostImports& state) {
    return state.device != context.device || GuestAllocations::GuestAllocationsGeneration_nid_postfix() != state.refreshedGeneration;
}

struct ImageMirrors {
    AgcDriver::Mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::map<std::uint64_t, std::shared_ptr<ImageMirror>> entries;
    std::set<std::uint64_t> failed;
    std::uint64_t heapBytes = 0;
    std::uint64_t builds = 0;
    std::uint64_t heapRefills = 0;
    std::uint64_t heapUnwatched = 0;
    std::uint64_t heapImported = 0;
    std::uint64_t heapImportedBytes = 0;
    std::uint64_t subranges = 0;
    std::uint64_t sparseBinds = 0;
    std::uint64_t rebuilds = 0;
    std::uint64_t refreshes = 0;
    std::uint64_t blocksCompared = 0;
    std::uint64_t blocksCopied = 0;
    std::uint64_t syncs = 0;
    std::uint64_t serials = 0;
    std::uint64_t version = 0;
    std::uint64_t pinnedSpace = 0;
    std::uint64_t pinnedVersion = 0;
    std::uint64_t sweeps = 0;
    std::uint64_t heapChecks = 0;
    std::uint64_t evictions = 0;
    std::uint64_t dispatchCounter = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

struct AddressBuildTiming {
    double leaseUs = 0;
    double importsUs = 0;
    double mirrorsUs = 0;
    double compareUs = 0;
    std::uint64_t blocksCompared = 0;
    std::uint64_t blocksCopied = 0;
};

AddressBuildTiming& ThreadAddressTiming() {
    thread_local AddressBuildTiming timing;
    return timing;
}

struct AddressBuildTotals {
    AgcDriver::Mutex mutex;
    std::uint64_t builds = 0;
    AddressBuildTiming sums;
    double snapshotsUs = 0;
    BdaResources::TableCacheStats tableSeen;
    AddressSpaceStats spaceSeen;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

AddressBuildTotals& AddressBuilds() {
    static AddressBuildTotals totals;
    return totals;
}

ImageMirrors& Mirrors() {
    static ImageMirrors mirrors;
    return mirrors;
}

using SnapshotCopies = Detail::SnapshotCopies;

SnapshotCopies& SnapshotCopyCache() {
    static SnapshotCopies copies;
    return copies;
}

std::shared_ptr<Buffer> snapshotCopy(const Context& context, std::uint64_t address, std::span<const std::byte> bytes, VkBufferUsageFlags usage) {
    auto& copies = SnapshotCopyCache();
    std::lock_guard lock(copies.mutex);
    if (copies.device != context.device) {
        copies.Clear();
        copies.device = context.device;
    }
    const SnapshotCopies::Key key{address, bytes.size(), usage};
    if (const auto found = copies.entries.find(key); found != copies.entries.end()) {
        if (std::memcmp(found->second.bytes.data(), bytes.data(), bytes.size()) == 0) {
            copies.recency.splice(copies.recency.end(), copies.recency, found->second.recent);
            return found->second.buffer;
        }
        copies.recency.erase(found->second.recent);
        copies.bytes -= bytes.size();
        copies.entries.erase(found);
    }
    auto buffer = std::make_shared<Buffer>(context, bytes.size(), usage);
    std::memcpy(buffer->Bytes().data(), bytes.data(), bytes.size());
    if (bytes.size() > SnapshotCopies::Budget) return buffer;
    while (!copies.recency.empty() && (copies.bytes + bytes.size() > SnapshotCopies::Budget || copies.entries.size() >= SnapshotCopies::MaxEntries)) {
        const auto oldest = copies.entries.find(copies.recency.front());
        copies.bytes -= std::get<1>(oldest->first);
        copies.entries.erase(oldest);
        copies.recency.pop_front();
    }
    copies.Insert(key, buffer, bytes);
    return buffer;
}

struct UploadCopies {
    using Key = std::tuple<std::uint64_t, std::size_t, VkBufferUsageFlags>;
    using Pieces = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
    AgcDriver::Mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    struct Entry {
        std::shared_ptr<Buffer> buffer;
        std::uint64_t generation = 0;
        Pieces pieces;
        std::list<Key>::iterator recent;
    };
    std::map<Key, Entry> entries;
    std::list<Key> recency;
    std::size_t bytes = 0;
};

std::size_t uploadCacheBudget() {
    static const std::size_t bytes = [] {
        const char* value = std::getenv("APS5_UPLOAD_CACHE_MIB");
        return static_cast<std::size_t>(value != nullptr ? std::strtoull(value, nullptr, 10) : 512ull) << 20u;
    }();
    return bytes;
}

constexpr std::size_t uploadCacheEntries = 256;

UploadCopies& UploadCopyCache() {
    static UploadCopies copies;
    return copies;
}

void resetUploadCache(UploadCopies& copies, VkDevice device) {
    copies.entries.clear();
    copies.recency.clear();
    copies.bytes = 0;
    copies.device = device;
}

std::uint64_t collectUploadWrites(std::uint64_t begin, bool sparse, const UploadCopies::Pieces& backed, std::size_t bytes) {
    if (!GuestMemory::WriteWatched()) return 0;
    if (!sparse) return GuestMemory::CollectWrites(begin, bytes);
    std::uint64_t generation = 0;
    for (const auto& [first, last] : backed) {
        generation = GuestMemory::CollectWrites(first, static_cast<std::size_t>(last - first));
        if (generation == 0) return 0;
    }
    return generation;
}

bool uploadUnchanged(std::uint64_t begin, bool sparse, const UploadCopies::Pieces& backed, std::size_t bytes, std::uint64_t generation) {
    if (generation == 0) return false;
    if (!sparse) return GuestMemory::CollectWritesUncached(begin, bytes) != 0 && GuestMemory::UnchangedSince(begin, bytes, generation);
    for (const auto& [first, last] : backed) {
        const auto size = static_cast<std::size_t>(last - first);
        if (GuestMemory::CollectWritesUncached(first, size) == 0 || !GuestMemory::UnchangedSince(first, size, generation)) return false;
    }
    return true;
}

std::shared_ptr<Buffer> findUploaded(const Context& context, std::uint64_t begin, bool sparse, const UploadCopies::Pieces& backed, std::size_t bytes, VkBufferUsageFlags usage, std::uint64_t& generation) {
    generation = 0;
    if (uploadCacheBudget() == 0) return nullptr;
    auto& copies = UploadCopyCache();
    const UploadCopies::Key key{begin, bytes, usage};
    {
        std::lock_guard lock(copies.mutex);
        if (copies.device != context.device) {
            resetUploadCache(copies, context.device);
            return nullptr;
        }
        const auto found = copies.entries.find(key);
        if (found == copies.entries.end() || found->second.pieces != backed) return nullptr;
        generation = found->second.generation;
    }
    if (!uploadUnchanged(begin, sparse, backed, bytes, generation)) return nullptr;
    std::lock_guard lock(copies.mutex);
    const auto found = copies.entries.find(key);
    if (found == copies.entries.end() || found->second.generation != generation) return nullptr;
    copies.recency.splice(copies.recency.end(), copies.recency, found->second.recent);
    return found->second.buffer;
}

void insertUploaded(const Context& context, std::uint64_t begin, const UploadCopies::Pieces& backed, std::size_t bytes, VkBufferUsageFlags usage, const std::shared_ptr<Buffer>& buffer, std::uint64_t generation) {
    const auto budget = uploadCacheBudget();
    if (budget == 0 || generation == 0 || bytes > budget) return;
    auto& copies = UploadCopyCache();
    const UploadCopies::Key key{begin, bytes, usage};
    std::lock_guard lock(copies.mutex);
    if (copies.device != context.device) resetUploadCache(copies, context.device);
    if (const auto found = copies.entries.find(key); found != copies.entries.end()) {
        copies.recency.erase(found->second.recent);
        copies.bytes -= std::get<1>(found->first);
        copies.entries.erase(found);
    }
    while (!copies.recency.empty() && (copies.bytes + bytes > budget || copies.entries.size() >= uploadCacheEntries)) {
        const auto oldest = copies.entries.find(copies.recency.front());
        copies.bytes -= std::get<1>(oldest->first);
        copies.entries.erase(oldest);
        copies.recency.pop_front();
    }
    copies.recency.push_back(key);
    copies.entries.emplace(key, UploadCopies::Entry{buffer, generation, backed, std::prev(copies.recency.end())});
    copies.bytes += bytes;
}

void forgetUploaded(const Context& context, std::uint64_t begin, std::size_t bytes, VkBufferUsageFlags usage) {
    auto& copies = UploadCopyCache();
    std::lock_guard lock(copies.mutex);
    if (copies.device != context.device) return;
    const UploadCopies::Key key{begin, bytes, usage};
    if (const auto found = copies.entries.find(key); found != copies.entries.end()) {
        copies.recency.erase(found->second.recent);
        copies.bytes -= std::get<1>(found->first);
        copies.entries.erase(found);
    }
}

}
}
