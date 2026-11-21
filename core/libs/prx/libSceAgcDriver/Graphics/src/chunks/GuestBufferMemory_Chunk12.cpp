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
namespace {
namespace {
void GuestBufferMemory::validate(std::uint64_t address, std::size_t bytes) const {
    Require(!uploaded, "guest memory ownership is frozen for GPU execution");
    Require(address != 0 && bytes != 0, "empty guest memory range");
    Require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "guest memory range overflow");
}

void GuestBufferMemory::addCopiedRange(const CopiedRange& range) {
    validate(range.begin, range.end - range.begin);
    Region region{range.begin, range.end, range.writable, {}, nullptr};
    region.hostBacked = !range.writable;
    auto committed = GuestMemory::DescribeCommitted(range.begin, range.end - range.begin);
    if (!committed.whole) {
        region.sparse = true;
        region.backed = std::move(committed.ranges);
    }
    regions.push_back(std::move(region));
}

GuestBufferMemory::BaseOverlap GuestBufferMemory::baseOverlap(std::uint64_t begin, std::uint64_t end, const Region** owner) const {
    if (space == nullptr || space->base.empty()) return BaseOverlap::None;
    const auto& base = space->base;
    const auto found = std::upper_bound(base.begin(), base.end(), begin, [](std::uint64_t value, const Region& region) { return value < region.begin; });
    if (found != base.begin()) {
        const auto& previous = *std::prev(found);
        if (begin < previous.end) {
            if (end > previous.end) return BaseOverlap::Partial;
            if (owner != nullptr) *owner = &previous;
            return BaseOverlap::Inside;
        }
    }
    return found != base.end() && found->begin < end ? BaseOverlap::Partial : BaseOverlap::None;
}

void GuestBufferMemory::dissolveSpace(bool resolve) {
    Require(space != nullptr, "no address space to dissolve");
    std::vector<Region> merged;
    merged.reserve(space->base.size() + regions.size());
    if (prepared) {
        std::merge(space->base.begin(), space->base.end(), std::make_move_iterator(regions.begin()), std::make_move_iterator(regions.end()), std::back_inserter(merged), [](const Region& left, const Region& right) { return left.begin < right.begin; });
        std::uint64_t reached = 0;
        for (const auto& region : merged) {
            regionsOverlap = regionsOverlap || region.begin < reached;
            reached = std::max(reached, region.end);
        }
    } else {
        merged.assign(space->base.begin(), space->base.end());
        merged.insert(merged.end(), std::make_move_iterator(regions.begin()), std::make_move_iterator(regions.end()));
        regionsSorted = false;
    }
    if (resolve) {
        for (auto& region : merged) {
            if (region.direct == nullptr) continue;
            region.pending = true;
            region.writable = region.writable || WritesOverlap(region.begin, static_cast<std::size_t>(region.end - region.begin));
        }
    }
    lease = space->lease;
    importsEpoch = space->importsEpoch;
    regions = std::move(merged);
    space.reset();
}

const GuestBufferMemory::Region* GuestBufferMemory::owner(std::uint64_t address, std::uint64_t end) const {
    const auto candidate = [address, end](const std::vector<Region>& list, bool overlapping) -> const Region* {
        const auto found = std::upper_bound(list.begin(), list.end(), address, [](std::uint64_t value, const Region& region) { return value < region.begin; });
        if (found == list.begin()) return nullptr;
        if (overlapping) {
            for (auto it = found; it != list.begin();) {
                --it;
                if (end <= it->end) return &*it;
            }
        }
        return &*std::prev(found);
    };
    const auto* own = candidate(regions, regionsOverlap);
    const auto* shared = space != nullptr ? candidate(space->base, false) : nullptr;
    const auto holds = [end](const Region* region) { return region != nullptr && end <= region->end; };
    if (holds(own) != holds(shared)) return holds(own) ? own : shared;
    if (own == nullptr || (shared != nullptr && shared->begin > own->begin)) return shared;
    return own;
}

namespace {

bool bdaStagingEnabled() {
    static const bool disabled = std::getenv("APS5_NO_BDA_STAGING") != nullptr;
    return !disabled;
}

template<typename Regions>
bool mirrorOverlaps(const Regions& base, std::uint64_t begin, std::uint64_t end) {
    auto found = std::upper_bound(base.begin(), base.end(), begin, [](std::uint64_t value, const auto& region) { return value < region.begin; });
    if (found != base.begin()) --found;
    for (; found != base.end() && found->begin < end; ++found) {
        if (found->end > begin && found->mirror != nullptr) return true;
    }
    return false;
}

}

void GuestBufferMemory::addDescriptorRegion(std::uint64_t address, std::size_t bytes, bool atomic, bool swept, bool readOnly) {
    validate(address, bytes);
    const auto begin = address & ~std::uint64_t{3};
    const auto end = begin == address ? address + bytes : (address + bytes + 3) & ~std::uint64_t{3};
    const Region* inside = nullptr;
    bool overlay = false;
    switch (baseOverlap(begin, end, &inside)) {
        case BaseOverlap::Inside:
            if (!servesView(*inside, begin)) {
                dissolveSpace(false);
                break;
            }
            if (overlayCandidate(*inside, readOnly) && GuestMemory::DescribeCommitted(begin, static_cast<std::size_t>(end - begin)).whole) {
                overlay = true;
                break;
            }
            if (bdaStagingEnabled() && stagingAllowed) servedInsideBase.emplace_back(begin, end);
            return;
        case BaseOverlap::Partial:
            if (readOnly && !mirrorOverlaps(space->base, begin, end) && !WritesOverlap(begin, static_cast<std::size_t>(end - begin))) {
                readsOverBase.emplace_back(begin, end);
                break;
            }
            Spaces().dissolvedOverlap.fetch_add(1, std::memory_order_relaxed);
            dissolveSpace(false);
            break;
        case BaseOverlap::None:
            break;
    }
    Region region{begin, end, true, {}, nullptr};
    region.atomic = atomic;
    region.swept = swept;
    region.lastView = begin;
    region.overlay = overlay;
    auto committed = GuestMemory::DescribeCommitted(begin, static_cast<std::size_t>(end - begin));
    if (!committed.whole) {
        region.sparse = true;
        region.sparseHeap = true;
        region.backed = std::move(committed.ranges);
    }
    regions.push_back(std::move(region));
    regionsSorted = false;
}

bool GuestBufferMemory::servesView(const Region& region, std::uint64_t view) {
    return region.mirror == nullptr || view + 4 <= region.mirror->base + region.mirror->bytes;
}

bool GuestBufferMemory::sparseHeapCandidate(const Region& region) {
    return region.sparseHeap && region.sparse && region.mirror == nullptr && region.direct == nullptr && (region.hostBacked || region.writable) && mirrorsEnabled() && sparseMirrorsEnabled();
}

std::shared_ptr<const GuestAllocations::Range> joinedHeapRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto run = crossedRun(lease, begin, end);
    if (run.ranges.size() < 2 || std::any_of(run.ranges.begin(), run.ranges.end(), [](const auto& range) { return !range->releasable; })) return nullptr;
    releaseExpiredJoinedHeaps();
    auto& joined = JoinedHeaps();
    std::lock_guard lock(joined.mutex);
    auto& entry = joined.entries[run.base];
    bool same = entry.range != nullptr && entry.members.size() == run.ranges.size();
    for (std::size_t index = 0; same && index < run.ranges.size(); ++index) same = sameRange(entry.members[index], run.ranges[index]);
    if (!same) {
        const auto& first = *run.ranges.front();
        entry.members.assign(run.ranges.begin(), run.ranges.end());
        entry.range = std::make_shared<const GuestAllocations::Range>(GuestAllocations::Range{run.base, static_cast<std::size_t>(run.bytes), true, first.writable, first.allocationAddress, first.allocationBytes, true, first.gpu, first.backingSerial});
    }
    return entry.range;
}

bool GuestBufferMemory::bindSparseMirror(Region& region, const GuestAllocations::Lease& lease) {
    const auto backedEnd = region.backed.empty() ? region.begin : region.backed.back().second;
    const auto neededEnd = std::min(region.end, std::max(backedEnd, std::max(region.lastView, region.begin) + 4u));
    std::shared_ptr<const GuestAllocations::Range> owner;
    bool joined = false;
    if (const auto* range = containingRange(lease, region.begin, neededEnd); range != nullptr) {
        if (!range->releasable) return false;
        owner = leasedRangeOwner(lease, range->address);
    } else {
        joined = true;
        owner = joinedHeapRange(lease, region.begin, neededEnd);
        if (owner == nullptr) return false;
    }
    std::vector<RefreshBlock> blocks;
    std::shared_ptr<ImageMirror> mirror;
    try {
        mirror = acquireMirror(context, owner, blocks, true, 0, 0, joined);
    } catch (const std::runtime_error& error) {
        static std::atomic<int> reported{0};
        if (reported.fetch_add(1, std::memory_order_relaxed) < 4) std::fprintf(stderr, "[gpu] %s; copying the sparse heap instead\n", error.what());
        return false;
    }
    if (mirror != nullptr && mirror->heap && !mirror->sparse) {
        std::vector<ImageMirror*> whole{mirror.get()};
        refreshHeapMirrors(whole, blocks);
    }
    compareBlocks(blocks);
    if (mirror == nullptr || !mirror->heap || region.begin < mirror->base || (region.begin - mirrorStart(*mirror)) % context.limits.minStorageBufferOffsetAlignment != 0) return false;
    region.mirror = std::move(mirror);
    if (!servesView(region, std::max(region.lastView, region.begin))) {
        region.mirror = nullptr;
        return false;
    }
    if (region.mirror->sparse) region.backed = clipPieces(region.mirror->committed, region.begin, region.end);
    else region.backed = clipPieces({{region.mirror->base, region.mirror->base + region.mirror->bytes}}, region.begin, region.end);
    region.snapshot.clear();
    std::lock_guard lock(Mirrors().mutex);
    ++Mirrors().sparseBinds;
    return true;
}

void GuestBufferMemory::AddWritable(std::uint64_t address, std::size_t bytes, bool atomic, bool swept) {
    if (space != nullptr && std::any_of(readsOverBase.begin(), readsOverBase.end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; })) {
        Spaces().dissolvedOverlap.fetch_add(1, std::memory_order_relaxed);
        dissolveSpace(false);
    }
    addDescriptorRegion(address, bytes, atomic, swept, false);
    writes.emplace_back(address, address + bytes);
}

void GuestBufferMemory::AddReadable(std::uint64_t address, std::size_t bytes) {
    addDescriptorRegion(address, bytes, false, false, true);
    reads.emplace_back(address, address + bytes);
}

bool GuestBufferMemory::SpaceOnly() const {
    return space != nullptr && lease.empty() && regions.empty() && space->sparseMirrors.empty() && space->copied.empty();
}

bool GuestBufferMemory::SpaceHeld() const {
    GuestMemory::AssertGpuLockHeld("GuestBufferMemory::SpaceHeld");
    return space != nullptr;
}

void GuestBufferMemory::HoldSpaceThrough(std::uint64_t recorder, std::uint64_t serial) {
    GuestMemory::AssertGpuLockHeld("GuestBufferMemory::HoldSpaceThrough");
    if (space == nullptr) return;
    spaceHeldBy = recorder;
    spaceHeldThrough = serial;
}

bool GuestBufferMemory::ReleaseCompletedSpace(std::uint64_t recorder, std::uint64_t completedThrough) {
    GuestMemory::AssertGpuLockHeld("GuestBufferMemory::ReleaseCompletedSpace");
    if (space == nullptr || spaceHeldThrough == 0) return false;
    if (spaceHeldBy == recorder ? spaceHeldThrough > completedThrough : Recorder::Alive(spaceHeldBy)) return false;
    static_cast<void>(ReleaseSpace());
    return true;
}

std::shared_ptr<const void> GuestBufferMemory::ReleaseSpace() {
    GuestMemory::AssertGpuLockHeld("GuestBufferMemory::ReleaseSpace");
    if (space == nullptr) return {};
    spaceHeldThrough = 0;
    spaceHeldBy = 0;
    releasedSpace = space;
    return std::move(space);
}

void GuestBufferMemory::ensureRetention() {
    if (retention == nullptr) retention = std::make_unique<RetainedAddressSpace>();
}

void GuestBufferMemory::ReleaseRetainedSpaces(std::uint64_t recorder, std::uint64_t completedThrough) {
    GuestMemory::AssertGpuLockHeld("GuestBufferMemory::ReleaseRetainedSpaces");
    if (retainedSpaces.load(std::memory_order_acquire) == nullptr) return;
    pruneRetained([&](const RetainedAddressSpace& node) { return node.recorder == recorder ? node.serial <= completedThrough : !Recorder::Alive(node.recorder); });
}

void ClearRetainedSpaces(VkDevice device) {
    pruneRetained([device](const RetainedAddressSpace& node) { return node.device == device; });
}

void ClearUploadCopies(VkDevice device) {
    auto& copies = UploadCopyCache();
    std::lock_guard lock(copies.mutex);
    if (copies.device == device) resetUploadCache(copies, VK_NULL_HANDLE);
}

std::size_t RetainedSpaceCount() {
    std::size_t count = 0;
    pruneRetained([&count](const RetainedAddressSpace&) {
        ++count;
        return false;
    });
    return count;
}

namespace {

bool epochMirrors() {
    return DrawPipelineEnabled();
}

void noteEpochMirrors(bool reused, const GuestBufferMemory::AddressSpace& space) {
    static std::atomic<std::uint64_t> reuses{0}, refreshes{0}, compared{0}, differing{0};
    static const bool verify = std::getenv("APS5_VERIFY_DRAW_PIPELINE") != nullptr;
    if (reused && verify && reuses.load(std::memory_order_relaxed) % 64u == 0u) {
        for (const auto index : space.writableMirrors) {
            const auto& region = space.base[index];
            const auto& mirror = *region.mirror;
            if (!mirror.writable || mirror.heap || mirror.shadow.empty()) continue;
            constexpr std::uint64_t block = 65536;
            for (auto at = region.begin; at < region.end;) {
                const auto next = std::min(region.end, (at / block + 1) * block);
                const auto offset = static_cast<std::size_t>(at - mirror.base);
                compared.fetch_add(1, std::memory_order_relaxed);
                if (std::memcmp(mirror.shadow.data() + offset, reinterpret_cast<const void*>(at), static_cast<std::size_t>(next - at)) != 0) differing.fetch_add(1, std::memory_order_relaxed);
                at = next;
            }
        }
    }
    const auto count = (reused ? reuses : refreshes).fetch_add(1, std::memory_order_relaxed) + 1u;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!verify && !profile) return;
    const bool report = count % 65536u == 0u || (count & (count - 1u)) == 0u;
    if (report) {
        std::fprintf(stderr, "[epoch-mirrors] address space uses: %llu reused the epoch's refresh, %llu refreshed; verify: %llu blocks compared, %llu differ\n",
                     static_cast<unsigned long long>(reuses.load()), static_cast<unsigned long long>(refreshes.load()), static_cast<unsigned long long>(compared.load()), static_cast<unsigned long long>(differing.load()));
    }
}

}

bool GuestBufferMemory::RetakeSpace() {
    GuestMemory::AssertGpuLockHeld("GuestBufferMemory::RetakeSpace");
    if (space != nullptr) return true;
    ensureRetention();
    auto kept = releasedSpace.lock();
    if (kept == nullptr) return false;
    auto& spaces = Spaces();
    auto current = spaces.current.load();
    if (current != kept) return false;
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    {
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        if (current->device != context.device || current->generation != generation || current->importsEpoch != state.epoch) return false;
        importFloor = state.useCounter + 1;
        stampSpaceUse(state, *current);
    }
    spaces.hits.fetch_add(1, std::memory_order_relaxed);
    space = std::move(current);
    std::vector<RefreshBlock> blocks;
    const auto epoch = GuestMemory::CollectEpoch();
    const auto proof = GuestMemory::ProofSerial();
    const bool reuse = epochMirrors() && epoch != 0 && space->refreshedEpoch.load(std::memory_order_relaxed) == epoch && space->refreshedProof.load(std::memory_order_relaxed) == proof;
    if (reuse) {
        noteEpochMirrors(true, *space);
    } else {
        for (const auto index : space->writableMirrors) {
            const auto& region = space->base[index];
            if (prepareRefresh(*region.mirror, region.begin, region.end - region.begin)) appendBlocks(blocks, *region.mirror, region.begin, region.end - region.begin);
        }
        if (epochMirrors()) noteEpochMirrors(false, *space);
    }
    const auto checked = refreshHeapRuns(*space, blocks);
    compareBlocks(blocks);
    for (const auto& [run, collected] : checked) space->verified[run].store(collected, std::memory_order_relaxed);
    if (!reuse && epoch != 0) {
        space->refreshedEpoch.store(epoch, std::memory_order_relaxed);
        space->refreshedProof.store(proof, std::memory_order_relaxed);
    }
    {
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        if (space->importsEpoch != state.epoch) {
            releasedSpace = space;
            space.reset();
            return false;
        }
    }
    return true;
}

SnapshotService GuestBufferMemory::SnapshotsServedInPlace(std::span<const GuestMemorySnapshot> snapshots) const {
    if (space == nullptr) return SnapshotService::Outside;
    for (const auto& snapshot : snapshots) {
        const Region* owner = nullptr;
        const auto overlap = baseOverlap(snapshot.address, snapshot.address + snapshot.bytes.size(), &owner);
        if (overlap == BaseOverlap::Partial) return SnapshotService::Partial;
        if (overlap != BaseOverlap::Inside || owner == nullptr) return SnapshotService::Outside;
        if (!(owner->writable || owner->hostBacked || owner->mirror != nullptr)) return SnapshotService::Outside;
        if (AnyShadowedOverlaps(snapshot.address, snapshot.bytes.size())) return SnapshotService::Changed;
        if (std::memcmp(reinterpret_cast<const void*>(snapshot.address), snapshot.bytes.data(), snapshot.bytes.size()) != 0) return SnapshotService::Changed;
    }
    return SnapshotService::Served;
}

}
}
}
}
}
