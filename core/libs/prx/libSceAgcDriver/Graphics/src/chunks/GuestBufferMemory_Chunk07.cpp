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
bool mirrorsEnabled() {
    static const bool disabled = std::getenv("APS5_NO_LEASE_MIRROR") != nullptr;
    return !disabled;
}

std::uint64_t heapMirrorBudget() {
    if (const auto test = testHeapMirrorBudget.load(std::memory_order_acquire); test != 0) return test;
    static const std::uint64_t bytes = [] {
        const char* value = std::getenv("APS5_HEAP_MIRROR_MIB");
        return (value ? std::strtoull(value, nullptr, 10) : 24576ull) << 20u;
    }();
    return bytes;
}

std::string heapMirrorError(std::uint64_t address, std::uint64_t bytes, std::uint64_t held, std::string_view reason) {
    constexpr std::string_view prefix = "AGC graphics: ";
    if (reason.starts_with(prefix)) reason.remove_prefix(prefix.size());
    char where[64];
    std::snprintf(where, sizeof(where), "heap mirror of 0x%llx+0x%llx: ", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
    char holding[96];
    std::snprintf(holding, sizeof(holding), "; heap mirrors hold %llu MiB of %llu MiB (APS5_HEAP_MIRROR_MIB)", static_cast<unsigned long long>(held >> 20u), static_cast<unsigned long long>(heapMirrorBudget() >> 20u));
    return std::string(prefix) + where + std::string(reason) + holding;
}

void sweepMirrors(std::uint64_t neededBytes = 0, std::uint64_t protectAddress = 0);

void unlistMirror(ImageMirrors& state, ImageMirror& mirror) {
    if (!mirror.listed) return;
    state.heapBytes -= mirror.accounted;
    mirror.listed = false;
}

struct JoinedHeapRange {
    std::vector<std::weak_ptr<const GuestAllocations::Range>> members;
    std::shared_ptr<const GuestAllocations::Range> range;
};

struct JoinedHeapRanges {
    std::mutex mutex;
    std::map<std::uint64_t, JoinedHeapRange> entries;
};

JoinedHeapRanges& JoinedHeaps() {
    static JoinedHeapRanges ranges;
    return ranges;
}

bool releaseExpiredJoinedHeaps() {
    std::vector<std::shared_ptr<const GuestAllocations::Range>> released;
    {
        auto& joined = JoinedHeaps();
        std::lock_guard lock(joined.mutex);
        for (auto it = joined.entries.begin(); it != joined.entries.end();) {
            const auto& members = it->second.members;
            if (std::none_of(members.begin(), members.end(), [](const auto& member) { return member.expired(); })) {
                ++it;
                continue;
            }
            released.push_back(std::move(it->second.range));
            it = joined.entries.erase(it);
        }
    }
    return !released.empty();
}

std::uint64_t blockBytes(const GuestRuns& runs) {
    constexpr std::uint64_t block = ResidentBuffer::BlockBytes;
    std::uint64_t blocks = 0;
    std::uint64_t last = 0;
    bool any = false;
    for (const auto& [first, end] : runs) {
        if (first >= end) continue;
        auto index = first / block;
        if (any && index == last) ++index;
        if (index <= (end - 1) / block) blocks += (end - 1) / block - index + 1;
        last = (end - 1) / block;
        any = true;
    }
    return blocks * block;
}

void reserveHeapBytes(std::uint64_t address, std::uint64_t bytes, std::uint64_t need) {
    auto& state = Mirrors();
    for (int attempt = 0;; ++attempt) {
        {
            std::lock_guard lock(state.mutex);
            std::uint64_t held = state.heapBytes;
            if (const auto found = state.entries.find(address); found != state.entries.end() && found->second->heap && found->second->listed) held -= found->second->accounted;
            if (held + need <= heapMirrorBudget()) {
                state.heapBytes += need;
                return;
            }
            if (attempt != 0) throw std::runtime_error(heapMirrorError(address, bytes, held, "past the budget"));
        }
        sweepMirrors(need, address);
    }
}

void growResident(ImageMirror& mirror, const GuestRuns& runs) {
    if (mirror.resident == nullptr) return;
    const auto need = mirror.resident->UnboundBytes(runs);
    if (need == 0) return;
    auto& state = Mirrors();
    bool counted = false;
    for (int attempt = 0; !counted; ++attempt) {
        {
            std::lock_guard lock(state.mutex);
            if (!mirror.listed) break;
            if (state.heapBytes + need <= heapMirrorBudget()) {
                state.heapBytes += need;
                mirror.accounted += need;
                counted = true;
                break;
            }
            if (attempt != 0) throw std::runtime_error(heapMirrorError(mirror.base, mirror.reserved, state.heapBytes, "past the budget"));
        }
        sweepMirrors(need, mirror.base);
    }
    try {
        mirror.resident->Bind(runs);
    } catch (const std::runtime_error& error) {
        std::uint64_t held = 0;
        {
            std::lock_guard lock(state.mutex);
            if (counted && mirror.listed) {
                state.heapBytes -= need;
                mirror.accounted -= need;
            }
            held = state.heapBytes;
        }
        throw std::runtime_error(heapMirrorError(mirror.base, mirror.reserved, held, error.what()));
    }
}

std::uint64_t mirrorStart(const ImageMirror& mirror) {
    return mirror.resident != nullptr ? mirror.resident->Origin() : mirror.base;
}

VkBuffer mirrorHandle(const ImageMirror& mirror) {
    return mirror.resident != nullptr ? mirror.resident->Handle() : mirror.buffer->Handle();
}

VkDeviceAddress mirrorAddress(const ImageMirror& mirror) {
    return mirror.resident != nullptr ? mirror.resident->DeviceAddress() : mirror.buffer->DeviceAddress();
}

bool sameRange(const std::weak_ptr<const GuestAllocations::Range>& mirrored, const std::shared_ptr<const GuestAllocations::Range>& range) {
    return !mirrored.owner_before(range) && !range.owner_before(mirrored);
}

struct RefreshBlock {
    ImageMirror* mirror;
    std::uint64_t address;
    std::size_t length;
    std::uint64_t generation = 0;
    std::uint8_t fill = 0;
    mutable bool unchanged = false;
};

constexpr std::uint8_t SparseCopy = 1;
constexpr std::uint8_t SparseZero = 2;

bool compareBlock(const RefreshBlock& block) {
    const auto offset = static_cast<std::size_t>(block.address - block.mirror->base);
    const auto* guest = reinterpret_cast<const std::byte*>(block.address);
    if (block.mirror->resident != nullptr) {
        auto* target = block.mirror->resident->Block(block.address);
        if (block.fill == SparseZero) {
            if (target != nullptr) std::memset(target, 0, block.length);
            return true;
        }
        const auto index = static_cast<std::size_t>(block.address / 65536 - block.mirror->base / 65536);
        if (target != nullptr && block.fill == 0 && block.generation != 0 && std::memcmp(target, guest, block.length) == 0) {
            block.mirror->generations[index] = block.generation;
            block.unchanged = true;
            return false;
        }
        if (target != nullptr) std::memcpy(target, guest, block.length);
        if (block.fill == 0) block.mirror->generations[index] = block.generation;
        return target != nullptr;
    }
    if (block.fill != 0) {
        auto* target = block.mirror->buffer->Bytes().data() + offset;
        if (block.fill == SparseZero) std::memset(target, 0, block.length);
        else std::memcpy(target, guest, block.length);
        return true;
    }
    if (block.mirror->heap) {
        const auto index = static_cast<std::size_t>(block.address / 65536 - block.mirror->base / 65536);
        auto* target = block.mirror->buffer->Bytes().data() + offset;
        const bool same = block.generation != 0 && std::memcmp(target, guest, block.length) == 0;
        if (!same) std::memcpy(target, guest, block.length);
        block.mirror->generations[index] = block.generation;
        return !same;
    }
    if (std::memcmp(block.mirror->shadow.data() + offset, guest, block.length) == 0) return false;
    std::memcpy(block.mirror->buffer->Bytes().data() + offset, guest, block.length);
    std::memcpy(block.mirror->shadow.data() + offset, guest, block.length);
    return true;
}

class RefreshPool {
public:
    static RefreshPool& Get() {
        if (instance == nullptr) instance = new RefreshPool;
        return *instance;
    }

    static void Shutdown() {
        delete std::exchange(instance, nullptr);
    }

    std::uint64_t Run(std::span<const RefreshBlock> blocks) {
        if (helpers == 0 || blocks.size() < 8) {
            std::uint64_t copied = 0;
            for (const auto& block : blocks) copied += compareBlock(block) ? 1 : 0;
            return copied;
        }
        std::lock_guard running(runMutex);
        {
            std::lock_guard lock(mutex);
            job = blocks;
            next.store(0, std::memory_order_relaxed);
            copiedBlocks.store(0, std::memory_order_relaxed);
            finished = 0;
            ++serial;
        }
        wake.notify_all();
        work(blocks);
        std::unique_lock lock(mutex);
        done.wait(lock, [&] { return finished == helpers; });
        job = {};
        return copiedBlocks.load(std::memory_order_relaxed);
    }

private:
    RefreshPool() {
        static const bool disabled = std::getenv("APS5_NO_PARALLEL_MIRROR_REFRESH") != nullptr;
        const char* text = std::getenv("APS5_MIRROR_REFRESH_THREADS");
        const auto requested = text != nullptr ? std::atoi(text) : 3;
        const auto wanted = disabled ? 0u : static_cast<unsigned>(std::clamp(requested, 0, 16));
        for (unsigned i = 0; i < wanted; ++i) {
            threads.push_back(Libc::StartHostJThread([this](std::stop_token token) {
                CpuTopology::PinHelperThread("mirror refresh");
                helper(token);
            }));
            ++helpers;
        }
    }

    void work(std::span<const RefreshBlock> blocks) {
        std::uint64_t copied = 0;
        for (auto index = next.fetch_add(1, std::memory_order_relaxed); index < blocks.size(); index = next.fetch_add(1, std::memory_order_relaxed)) copied += compareBlock(blocks[index]) ? 1 : 0;
        copiedBlocks.fetch_add(copied, std::memory_order_relaxed);
    }

    void helper(std::stop_token token) {
        std::uint64_t seen = 0;
        for (;;) {
            std::span<const RefreshBlock> blocks;
            {
                std::unique_lock lock(mutex);
                if (!wake.wait(lock, token, [&] { return serial != seen; })) return;
                seen = serial;
                blocks = job;
            }
            work(blocks);
            {
                std::lock_guard lock(mutex);
                ++finished;
            }
            done.notify_all();
        }
    }

    inline static RefreshPool* instance = nullptr;
    unsigned helpers = 0;
    AgcDriver::Mutex runMutex;
    std::mutex mutex;
    std::condition_variable_any wake;
    std::condition_variable done;
    std::span<const RefreshBlock> job;
    std::uint64_t serial = 0;
    unsigned finished = 0;
    std::atomic<std::size_t> next{0};
    std::atomic<std::uint64_t> copiedBlocks{0};
    std::vector<std::jthread> threads;
};

void syncRange(std::uint64_t address, std::uint64_t bytes) {
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
    GuestMemory::FlushGpuWrites(address, static_cast<std::size_t>(bytes));
    auto& state = Mirrors();
    if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(address, static_cast<std::size_t>(bytes))) {
        Recorder::CountSync(4);
        ++state.syncs;
        recorder->Sync();
    }
    ++state.refreshes;
}

void prepareRange(std::uint64_t address, std::uint64_t bytes) {
    Require(GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes)), "image mirror range became inaccessible");
    syncRange(address, bytes);
}

bool prepareRefresh(ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    if ((!mirror.writable && !mirror.heap) || bytes == 0) return false;
    prepareRange(address, bytes);
    return true;
}

void appendBlocks(std::vector<RefreshBlock>& blocks, ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    constexpr std::uint64_t block = 65536;
    const auto end = address + bytes;
    for (auto at = address; at < end;) {
        const auto next = std::min(end, (at / block + 1) * block);
        blocks.push_back({&mirror, at, static_cast<std::size_t>(next - at)});
        at = next;
    }
}

void bindHeapBlocks(std::span<const RefreshBlock> blocks) {
    std::vector<std::pair<ImageMirror*, GuestRuns>> wanted;
    for (const auto& block : blocks) {
        if (block.mirror->resident == nullptr || block.fill == SparseZero) continue;
        if (block.mirror->resident->Block(block.address) == nullptr && GuestBytesZero(block.address, block.address + block.length)) continue;
        if (wanted.empty() || wanted.back().first != block.mirror) {
            const auto found = std::find_if(wanted.begin(), wanted.end(), [&](const auto& entry) { return entry.first == block.mirror; });
            if (found == wanted.end()) wanted.emplace_back(block.mirror, GuestRuns{});
            else std::rotate(found, std::next(found), wanted.end());
        }
        auto& runs = wanted.back().second;
        if (!runs.empty() && runs.back().second == block.address) runs.back().second = block.address + block.length;
        else runs.emplace_back(block.address, block.address + block.length);
    }
    for (auto& [mirror, runs] : wanted) {
        std::sort(runs.begin(), runs.end());
        growResident(*mirror, runs);
    }
}

void compareBlocks(std::span<const RefreshBlock> blocks) {
    if (blocks.empty()) return;
    bindHeapBlocks(blocks);
    const auto copied = RefreshPool::Get().Run(blocks);
    for (const auto& block : blocks) {
        if (!block.unchanged && block.mirror->resident != nullptr && block.mirror->resident->Block(block.address) != nullptr) block.mirror->resident->MarkChanged(block.address, block.address + block.length);
    }
    auto& state = Mirrors();
    state.blocksCompared += blocks.size();
    state.blocksCopied += copied;
}

void refreshMirror(ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    if (!prepareRefresh(mirror, address, bytes)) return;
    std::vector<RefreshBlock> blocks;
    appendBlocks(blocks, mirror, address, bytes);
    compareBlocks(blocks);
}

void appendChangedBlocks(std::span<ImageMirror* const> mirrors, std::uint64_t generation, std::vector<RefreshBlock>& blocks) {
    constexpr std::uint64_t block = 65536;
    thread_local std::vector<std::uint8_t>* changedSlot = nullptr;
    auto& changed = ShaderRecompiler::ThreadOwned(changedSlot);
    Mirrors().heapChecks += mirrors.size();
    for (auto* const pointer : mirrors) {
        auto& mirror = *pointer;
        changed.assign(mirror.generations.size(), 0);
        GuestMemory::ChangedBlocks(mirror.base, static_cast<std::size_t>(mirror.bytes), mirror.generations, changed);
        const auto aligned = mirror.base / block * block;
        const auto end = mirror.base + mirror.bytes;
        bool refilled = false;
        for (std::size_t at = 0; at < changed.size(); ++at) {
            if (changed[at] == 0) continue;
            const auto from = std::max(mirror.base, aligned + at * block);
            blocks.push_back({&mirror, from, static_cast<std::size_t>(std::min(end, aligned + (at + 1) * block) - from), generation});
            refilled = true;
        }
        if (refilled) ++Mirrors().heapRefills;
    }
}

void markUnwatched(ImageMirror& mirror) {
    ++Mirrors().heapUnwatched;
    std::fill(mirror.generations.begin(), mirror.generations.end(), 0);
}

void appendHeapRun(std::span<ImageMirror* const> mirrors, std::uint64_t generation, std::vector<RefreshBlock>& blocks) {
    if (generation != 0) {
        appendChangedBlocks(mirrors, generation, blocks);
        return;
    }
    for (std::size_t index = 0; index < mirrors.size(); ++index) {
        auto& mirror = *mirrors[index];
        const auto own = GuestMemory::CollectWrites(mirror.base, static_cast<std::size_t>(mirror.bytes));
        if (own == 0) markUnwatched(mirror);
        appendChangedBlocks(mirrors.subspan(index, 1), own, blocks);
    }
}

void refreshHeapMirrors(std::vector<ImageMirror*>& mirrors, std::vector<RefreshBlock>& blocks) {
    std::sort(mirrors.begin(), mirrors.end(), [](const ImageMirror* left, const ImageMirror* right) { return left->base < right->base; });
    for (std::size_t first = 0; first < mirrors.size();) {
        auto last = first + 1;
        while (last < mirrors.size() && mirrors[last]->base == mirrors[last - 1]->base + mirrors[last - 1]->bytes) ++last;
        const auto begin = mirrors[first]->base;
        const auto bytes = mirrors[last - 1]->base + mirrors[last - 1]->bytes - begin;
        prepareRange(begin, bytes);
        const auto generation = GuestMemory::CollectWrites(begin, static_cast<std::size_t>(bytes));
        appendHeapRun(std::span<ImageMirror* const>(mirrors).subspan(first, last - first), generation, blocks);
        first = last;
    }
}

bool sparseMirrorsEnabled() {
    static const bool disabled = std::getenv("APS5_NO_SPARSE_HEAP_MIRROR") != nullptr;
    return !disabled;
}

constexpr std::uint32_t MaxSparseRunJoins = 64;

using Pieces = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

std::uint64_t sparseExtent(std::uint64_t base, std::uint64_t reserved, const Pieces& pieces) {
    constexpr std::uint64_t granule = std::uint64_t{1} << 20u;
    const auto used = pieces.empty() ? 0 : pieces.back().second - base;
    const auto wanted = used + used / 8;
    return std::min(reserved, std::max<std::uint64_t>(granule, (wanted + granule - 1) / granule * granule));
}

std::uint64_t collectPieces(const Pieces& pieces) {
    std::uint64_t generation = 0;
    for (const auto& [first, last] : pieces) {
        generation = GuestMemory::CollectWrites(first, static_cast<std::size_t>(last - first));
        if (generation == 0) return 0;
    }
    return generation != 0 ? generation : GuestMemory::TrackerGeneration();
}

bool covers(const Pieces& pieces, std::uint64_t address) {
    return std::any_of(pieces.begin(), pieces.end(), [&](const auto& piece) { return piece.first <= address && address < piece.second; });
}

void forgetCommitChanges(ImageMirror& mirror, const Pieces& pieces) {
    if (pieces == mirror.committed) return;
    std::vector<std::uint64_t> edges;
    for (const auto* list : std::array<const Pieces*, 2>{&pieces, &mirror.committed}) {
        for (const auto& [first, last] : *list) {
            edges.push_back(first);
            edges.push_back(last);
        }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    constexpr std::uint64_t block = 65536;
    const auto end = mirror.base + mirror.bytes;
    for (std::size_t at = 0; at + 1 < edges.size(); ++at) {
        const auto from = std::max(edges[at], mirror.base);
        const auto to = std::min(edges[at + 1], end);
        if (from >= to || covers(pieces, from) == covers(mirror.committed, from)) continue;
        for (auto index = from / block - mirror.base / block; index <= (to - 1) / block - mirror.base / block; ++index) mirror.generations[static_cast<std::size_t>(index)] = 0;
    }
}

void appendSparseBlocks(ImageMirror& mirror, Pieces pieces, std::uint64_t generation, std::vector<RefreshBlock>& blocks) {
    constexpr std::uint64_t block = 65536;
    thread_local std::vector<std::uint8_t>* changedSlot = nullptr;
    auto& changed = ShaderRecompiler::ThreadOwned(changedSlot);
    forgetCommitChanges(mirror, pieces);
    ++Mirrors().heapChecks;
    changed.assign(mirror.generations.size(), 0);
    GuestMemory::ChangedBlocks(mirror.base, static_cast<std::size_t>(mirror.bytes), mirror.generations, changed);
    const auto aligned = mirror.base / block * block;
    const auto end = mirror.base + mirror.bytes;
    bool refilled = false;
    std::size_t piece = 0;
    for (std::size_t at = 0; at < changed.size(); ++at) {
        if (changed[at] == 0) continue;
        refilled = true;
        mirror.generations[at] = generation;
        const auto from = std::max(mirror.base, aligned + at * block);
        const auto to = std::min(end, aligned + (at + 1) * block);
        auto cursor = from;
        while (piece < pieces.size() && pieces[piece].second <= from) ++piece;
        for (auto next = piece; next < pieces.size() && pieces[next].first < to; ++next) {
            const auto first = std::max(pieces[next].first, from);
            const auto last = std::min(pieces[next].second, to);
            if (cursor < first) blocks.push_back({&mirror, cursor, static_cast<std::size_t>(first - cursor), 0, SparseZero});
            blocks.push_back({&mirror, first, static_cast<std::size_t>(last - first), 0, SparseCopy});
            cursor = last;
        }
        if (cursor < to) blocks.push_back({&mirror, cursor, static_cast<std::size_t>(to - cursor), 0, SparseZero});
    }
    if (refilled) ++Mirrors().heapRefills;
    mirror.committed = std::move(pieces);
}

}
}
