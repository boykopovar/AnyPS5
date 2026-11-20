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
Pieces clipPieces(const Pieces& pieces, std::uint64_t begin, std::uint64_t end) {
    Pieces result;
    for (const auto& [first, last] : pieces) {
        if (std::max(first, begin) < std::min(last, end)) result.emplace_back(std::max(first, begin), std::min(last, end));
    }
    return result;
}

bool refreshSparseMirror(ImageMirror& mirror, std::vector<RefreshBlock>& blocks) {
    auto pieces = GuestMemory::DescribeCommitted(mirror.base, static_cast<std::size_t>(mirror.reserved)).ranges;
    if (!pieces.empty() && pieces.back().second > mirror.base + mirror.bytes) return false;
    syncRange(mirror.base, mirror.bytes);
    const auto generation = collectPieces(pieces);
    if (generation == 0) markUnwatched(mirror);
    appendSparseBlocks(mirror, std::move(pieces), generation, blocks);
    return true;
}

void planSpaceMirrors(GuestBufferMemory::AddressSpace& space) {
    for (std::size_t index = 0; index < space.base.size(); ++index) {
        const auto& region = space.base[index];
        if (region.mirror == nullptr) continue;
        space.mirrored = true;
        if (!region.mirror->heap) {
            if (region.mirror->writable) space.writableMirrors.push_back(index);
            continue;
        }
        auto* mirror = region.mirror.get();
        if (mirror->sparse) {
            space.sparseMirrors.push_back(mirror);
            continue;
        }
        if (space.heapRuns.empty() || mirror->base != space.heapMirrors.back()->base + space.heapMirrors.back()->bytes) space.heapRuns.push_back({mirror->base, 0, space.heapMirrors.size(), 0});
        space.heapMirrors.push_back(mirror);
        auto& run = space.heapRuns.back();
        run.bytes = mirror->base + mirror->bytes - run.begin;
        run.last = space.heapMirrors.size();
    }
    space.verified = std::make_unique<std::atomic<std::uint64_t>[]>(space.heapRuns.size());
}

std::vector<std::pair<std::size_t, std::uint64_t>> refreshHeapRuns(const GuestBufferMemory::AddressSpace& space, std::vector<RefreshBlock>& blocks) {
    std::vector<std::pair<std::size_t, std::uint64_t>> checked;
    checked.reserve(space.heapRuns.size());
    for (std::size_t index = 0; index < space.heapRuns.size(); ++index) {
        const auto& run = space.heapRuns[index];
        prepareRange(run.begin, run.bytes);
        const auto generation = GuestMemory::CollectWrites(run.begin, static_cast<std::size_t>(run.bytes));
        const auto since = space.verified[index].load(std::memory_order_relaxed);
        if (since == 0 || !GuestMemory::UnchangedSince(run.begin, static_cast<std::size_t>(run.bytes), since)) appendHeapRun(std::span<ImageMirror* const>(space.heapMirrors).subspan(run.first, run.last - run.first), generation, blocks);
        checked.emplace_back(index, generation);
    }
    return checked;
}

bool heapMirrorWatched(ImageMirror& mirror) {
    const auto serial = GuestMemory::UnwatchSerial();
    if (mirror.watchedSerial.load(std::memory_order_relaxed) == serial) return true;
    if (!GuestMemory::Watched(mirror.base, static_cast<std::size_t>(mirror.bytes))) return false;
    mirror.watchedSerial.store(serial, std::memory_order_relaxed);
    return true;
}

bool importsNow(const Context& context, HostImports& state, const GuestAllocations::Lease& lease, std::uint64_t address) {
    if (context.hostImportAlignment == 0) return false;
    if (importsStale(context, state)) return true;
    const MirrorsYield yield(state, state.useCounter + 1);
    return importAllocation(context, state, address, lease, ImportRequester::Build, 0, true) != nullptr;
}

const char* heapCopyReason(bool overBudget, bool watched) {
    if (overBudget) return watched ? "import over the host import budget, heap mirror refused" : "import over the host import budget, heap not fully write-watched";
    return watched ? "heap mirror refused" : "heap not fully write-watched";
}

// The mirror for a leased image range: the existing one, or a new one when none exists or the
// registered Range object changed (a protection change). Null when the range cannot be mirrored. An
// existing writable mirror's blocks are appended to `blocks` for the caller's one compare of every
std::shared_ptr<ImageMirror> acquireMirror(const Context& context, const std::shared_ptr<const GuestAllocations::Range>& range, std::vector<RefreshBlock>& blocks, bool heap, std::uint64_t begin = 0, std::uint64_t bytes = 0, bool joinedHeap = false) {
    auto selected = *range;
    if (bytes != 0) {
        Require(begin >= range->address && bytes <= range->bytes && begin - range->address <= range->bytes - bytes, "heap mirror selection exceeds its registered range");
        selected.address = begin;
        selected.bytes = static_cast<std::size_t>(bytes);
    }
    auto& state = Mirrors();
    std::shared_ptr<ImageMirror> mirror;
    {
        std::lock_guard lock(state.mutex);
        if (state.device != context.device) {
            for (auto& [base, entry] : state.entries) entry->listed = false;
            state.entries.clear();
            state.failed.clear();
            state.heapBytes = 0;
            state.device = context.device;
            ++state.version;
        }
        if (state.failed.contains(selected.address)) return nullptr;
        const auto found = state.entries.find(selected.address);
        if (found != state.entries.end() && sameRange(found->second->range, range) && found->second->reserved == selected.bytes && found->second->writable == range->writable && found->second->heap == heap && found->second->joinedHeap == joinedHeap) mirror = found->second;
    }
    if (mirror != nullptr) {
        if (!mirror->sparse) {
            if (!heap && prepareRefresh(*mirror, selected.address, selected.bytes)) appendBlocks(blocks, *mirror, selected.address, selected.bytes);
            return mirror;
        }
        if (refreshSparseMirror(*mirror, blocks)) return mirror;
    }
    Pieces pieces;
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(selected.address), selected.bytes, range->writable && !heap)) {
        if (!heap || !sparseMirrorsEnabled()) return nullptr;
        pieces = GuestMemory::DescribeCommitted(selected.address, selected.bytes).ranges;
        if (pieces.empty()) return nullptr;
    }
    const bool sparse = !pieces.empty();
    mirror = std::make_shared<ImageMirror>();
    mirror->base = selected.address;
    mirror->bytes = sparse ? sparseExtent(selected.address, selected.bytes, pieces) : selected.bytes;
    mirror->reserved = selected.bytes;
    mirror->sparse = sparse;
    mirror->writable = range->writable;
    mirror->heap = heap;
    mirror->joinedHeap = joinedHeap;
    mirror->range = range;
    GuestRuns backed;
    if (heap) {
        std::uint64_t generation = 0;
        const auto serial = GuestMemory::UnwatchSerial();
        if (sparse) {
            syncRange(mirror->base, mirror->bytes);
            if (GuestMemory::Watched(mirror->base, static_cast<std::size_t>(mirror->bytes))) generation = collectPieces(pieces);
        } else {
            prepareRefresh(*mirror, selected.address, selected.bytes);
            generation = GuestMemory::CollectWrites(selected.address, selected.bytes);
        }
        constexpr std::uint64_t block = 65536;
        mirror->generations.assign(static_cast<std::size_t>(((mirror->base + mirror->bytes + block - 1) / block) - mirror->base / block), generation);
        if (generation == 0) {
            std::shared_ptr<ImageMirror> stale;
            std::lock_guard lock(state.mutex);
            ++state.heapUnwatched;
            if (const auto found = state.entries.find(selected.address); found != state.entries.end() && found->second->heap) {
                unlistMirror(state, *found->second);
                stale = std::move(found->second);
                state.entries.erase(found);
                ++state.version;
            }
            return nullptr;
        }
        mirror->watchedSerial.store(serial, std::memory_order_relaxed);
        if (context.sparseBinding && context.sparseResidencyBuffer && context.sparseResidencyAliased) backed = GuestNonzeroBlocks(sparse ? clipPieces(pieces, mirror->base, mirror->base + mirror->bytes) : GuestRuns{{mirror->base, mirror->base + mirror->bytes}});
        auto need = blockBytes(backed);
        reserveHeapBytes(selected.address, selected.bytes, need);
        try {
            mirror->resident = ResidentBuffer::Create(context, mirror->base, mirror->bytes, backed);
        } catch (const std::runtime_error& error) {
            std::uint64_t held = 0;
            {
                std::lock_guard lock(state.mutex);
                state.heapBytes -= need;
                held = state.heapBytes;
            }
            throw std::runtime_error(heapMirrorError(selected.address, selected.bytes, held, error.what()));
        }
        if (mirror->resident == nullptr) {
            {
                std::lock_guard lock(state.mutex);
                state.heapBytes -= need;
            }
            need = mirror->bytes;
            reserveHeapBytes(selected.address, selected.bytes, need);
        }
        mirror->accounted = need;
    }
    if (mirror->resident != nullptr) {
        {
            const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
            for (const auto& [first, last] : backed) {
                for (auto at = first; at < last;) {
                    const auto next = std::min(last, (at / ResidentBuffer::BlockBytes + 1) * ResidentBuffer::BlockBytes);
                    GuestMemory::Read(at, std::span<std::byte>(mirror->resident->Block(at), static_cast<std::size_t>(next - at)));
                    at = next;
                }
                mirror->resident->MarkChanged(first / ResidentBuffer::BlockBytes * ResidentBuffer::BlockBytes, (last + ResidentBuffer::BlockBytes - 1) / ResidentBuffer::BlockBytes * ResidentBuffer::BlockBytes);
            }
        }
        mirror->committed = std::move(pieces);
        std::lock_guard lock(state.mutex);
        ++state.rebuilds;
        mirror->serial = (1ull << 63u) | ++state.serials;
        if (const auto found = state.entries.find(selected.address); found != state.entries.end() && found->second->heap) unlistMirror(state, *found->second);
        mirror->listed = true;
        state.entries[selected.address] = mirror;
        ++state.version;
        return mirror;
    }
    try {
        mirror->buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(mirror->bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    } catch (const std::runtime_error& error) {
        if (heap) {
            std::uint64_t held = 0;
            {
                std::lock_guard lock(state.mutex);
                state.heapBytes -= mirror->accounted;
                held = state.heapBytes;
            }
            throw std::runtime_error(heapMirrorError(selected.address, selected.bytes, held, error.what()));
        }
        std::fprintf(stderr, "[gpu] image mirror of 0x%llx+0x%llx failed: %s; falling back to copies\n", static_cast<unsigned long long>(selected.address), static_cast<unsigned long long>(selected.bytes), error.what());
        std::lock_guard lock(state.mutex);
        state.failed.insert(selected.address);
        return nullptr;
    }
    // Read stores GPU results pending in the range first (write-backs of earlier copies of it);
    // named for the [hooksync] attribution like the refresh's flush.
    {
        const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
        const auto target = mirror->buffer->Bytes().first(static_cast<std::size_t>(mirror->bytes));
        if (!sparse) GuestMemory::Read(selected.address, target);
        std::size_t cursor = 0;
        for (const auto& [first, last] : pieces) {
            const auto at = static_cast<std::size_t>(first - mirror->base);
            std::memset(target.data() + cursor, 0, at - cursor);
            GuestMemory::Read(first, target.subspan(at, static_cast<std::size_t>(last - first)));
            cursor = static_cast<std::size_t>(last - mirror->base);
        }
        if (sparse) std::memset(target.data() + cursor, 0, target.size() - cursor);
        mirror->committed = std::move(pieces);
    }
    if (range->writable && !heap) mirror->shadow.assign(mirror->buffer->Bytes().begin(), mirror->buffer->Bytes().end());
    std::lock_guard lock(state.mutex);
    ++state.rebuilds;
    mirror->serial = (1ull << 63u) | ++state.serials;
    // A replaced mirror lives on in the regions of recorded work that bound it.
    if (const auto found = state.entries.find(selected.address); found != state.entries.end() && found->second->heap) unlistMirror(state, *found->second);
    mirror->listed = heap;
    state.entries[selected.address] = mirror;
    ++state.version;
    return mirror;
}

std::shared_ptr<ImageMirror> findMirror(const Context& context, std::uint64_t begin, std::uint64_t end) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    if (state.device != context.device || state.entries.empty()) return nullptr;
    auto found = state.entries.upper_bound(begin);
    if (found == state.entries.begin()) return nullptr;
    --found;
    const auto& mirror = found->second;
    if (mirror->heap || begin < mirror->base || end > mirror->base + mirror->bytes || mirror->range.expired()) return nullptr;
    return mirror;
}

std::shared_ptr<const GuestAllocations::Range> fixedImageRange(std::uint64_t begin, std::uint64_t end) {
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    auto found = std::upper_bound(lease.begin(), lease.end(), begin, [](std::uint64_t address, const auto& range) { return address < range->address; });
    if (found == lease.begin()) return nullptr;
    const auto& range = *--found;
    if (!range->readable || range->writable || range->releasable || end > range->address + range->bytes) return nullptr;
    return range;
}

void sweepMirrors(std::uint64_t neededBytes, std::uint64_t protectAddress) {
    ReferenceStorage::Shared().ReleaseIdle();
    const bool released = releaseExpiredJoinedHeaps();
    auto space = Spaces().current.load();
    auto& state = Mirrors();
    std::vector<std::shared_ptr<ImageMirror>> dropped;
    std::lock_guard lock(state.mutex);
    if (released) ++state.version;
    if (neededBytes == 0 && space != nullptr && state.pinnedSpace == space->serial && state.pinnedVersion == state.version) return;
    if (neededBytes != 0 && state.heapBytes + neededBytes > heapMirrorBudget()) {
        if (auto cached = Spaces().current.exchange(nullptr); cached != nullptr) {
            space.reset();
            cached.reset();
        }
    }
    ++state.sweeps;
    bool pinned = space != nullptr;
    std::size_t leased = 0;
    for (auto it = state.entries.begin(); it != state.entries.end();) {
        if (!it->second->range.expired()) {
            if (pinned) {
                while (leased < space->lease.size() && space->lease[leased]->address < it->first) ++leased;
                pinned = leased < space->lease.size() && sameRange(it->second->range, space->lease[leased]);
            }
            ++it;
            continue;
        }
        if (it->second->heap) unlistMirror(state, *it->second);
        dropped.push_back(std::move(it->second));
        it = state.entries.erase(it);
        ++state.version;
    }
    if (neededBytes != 0 && state.heapBytes + neededBytes > heapMirrorBudget()) {
        std::unordered_set<const ImageMirror*> queued;
        if (buildingBlocks != nullptr) {
            for (const auto& block : *buildingBlocks) queued.insert(block.mirror);
        }
        while (state.heapBytes + neededBytes > heapMirrorBudget()) {
            auto victim = state.entries.end();
            for (auto it = state.entries.begin(); it != state.entries.end(); ++it) {
                if (!it->second->heap || it->first == protectAddress || queued.count(it->second.get()) != 0) continue;
                const bool buildingHolds = buildingBufferMemory != nullptr && buildingBufferMemory->HoldsMirror(it->second);
                const auto allowedCount = (buildingHolds && !it->second->writable) ? 2 : 1;
                if (it->second.use_count() == allowedCount) {
                    if (victim == state.entries.end()) {
                        victim = it;
                    } else if (it->second->lastDispatch != victim->second->lastDispatch) {
                        if (it->second->lastDispatch < victim->second->lastDispatch) victim = it;
                    } else if (it->second->writable != victim->second->writable) {
                        if (!it->second->writable && victim->second->writable) victim = it;
                    } else if (it->first < victim->first) {
                        victim = it;
                    }
                }
            }
            if (victim == state.entries.end()) break;
            if (buildingBufferMemory != nullptr) {
                buildingBufferMemory->RecordEvicted(victim->first);
                if (buildingBufferMemory->HoldsMirror(victim->second)) {
                    buildingBufferMemory->EvictMirror(victim->second);
                }
            }
            unlistMirror(state, *victim->second);
            dropped.push_back(std::move(victim->second));
            state.entries.erase(victim);
            ++state.version;
            ++state.evictions;
        }
    }
    state.pinnedSpace = pinned ? space->serial : 0;
    state.pinnedVersion = state.version;
}

void releaseImportedMirrors(const Context& context, std::span<const std::pair<std::uint64_t, std::uint64_t>> imported) {
    if (imported.empty()) return;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> coverage(imported.begin(), imported.end());
    std::sort(coverage.begin(), coverage.end());
    std::size_t count = 0;
    for (const auto& interval : coverage) {
        if (count != 0 && interval.first <= coverage[count - 1].second) coverage[count - 1].second = std::max(coverage[count - 1].second, interval.second);
        else coverage[count++] = interval;
    }
    coverage.resize(count);
    std::vector<std::shared_ptr<ImageMirror>> released;
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    if (state.device != context.device || state.entries.empty()) return;
    for (const auto& [begin, end] : imported) {
        auto it = state.entries.upper_bound(begin);
        if (it != state.entries.begin()) --it;
        while (it != state.entries.end() && it->first < end) {
            const auto& mirror = it->second;
            if (!mirror->heap || mirror->base + mirror->bytes <= begin) {
                ++it;
                continue;
            }
            if (mirror->joinedHeap) {
                const auto covered = std::upper_bound(coverage.begin(), coverage.end(), mirror->base, [](std::uint64_t address, const auto& interval) { return address < interval.first; });
                if (covered == coverage.begin() || std::prev(covered)->second < mirror->base + mirror->reserved) {
                    ++it;
                    continue;
                }
            }
            ++state.heapImported;
            state.heapImportedBytes += mirror->accounted;
            unlistMirror(state, *mirror);
            released.push_back(std::move(it->second));
            it = state.entries.erase(it);
            ++state.version;
        }
    }
}

}
}
