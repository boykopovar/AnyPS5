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
void reapSettleBatches(const Context& context, HostImports& state) {
    const auto found = state.settles.find(context.device);
    if (found == state.settles.end()) return;
    auto& deviceSettle = found->second;
    const auto status = context.Resolved(&DeviceFunctions::getFenceStatus, "vkGetFenceStatus");
    for (std::size_t index = 0; index < deviceSettle.batches.size(); ++index) {
        auto& batch = deviceSettle.batches[index];
        if (!batch.busy) continue;
        const auto result = status(context.device, batch.fence);
        if (result == VK_NOT_READY) continue;
        for (const auto base : batch.bases) {
            const auto settle = state.pendingSettles.find(base);
            if (settle == state.pendingSettles.end() || settle->second.phase != SettlePhase::Submitted || settle->second.batch != index || settle->second.device != context.device) continue;
            if (result != VK_SUCCESS) {
                state.pendingSettles.erase(settle);
                continue;
            }
            settle->second.phase = SettlePhase::Settled;
            state.settleReady.push_back(base);
        }
        for (const auto alias : batch.aliases) deviceSettle.settleDestroyBuffer(context.device, alias, nullptr);
        batch.aliases.clear();
        batch.retired.clear();
        batch.bases.clear();
        batch.busy = false;
    }
}

bool comparedImportShares(const HostImports& state, std::uint64_t own, std::uint64_t begin, std::uint64_t end) {
    const auto shares = [&](const HostImport& entry) { return entry.unwatched && entry.memoryBase < end && begin < entry.memoryBase + entry.pinnedBytes; };
    for (const auto& [base, entry] : state.imports) {
        if (base != own && shares(entry)) return true;
    }
    for (const auto& [device, list] : state.parked) {
        for (const auto& retired : list) {
            if (shares(retired->node.mapped())) return true;
        }
    }
    return false;
}

void rewatchSettledImports(HostImports& state) {
    if (state.settleReady.empty()) return;
    std::vector<std::uint64_t> bases;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (const auto base : state.settleReady) {
        const auto settle = state.pendingSettles.find(base);
        if (settle == state.pendingSettles.end() || settle->second.phase != SettlePhase::Settled) continue;
        state.pendingSettles.erase(settle);
        const auto import = state.imports.find(base);
        if (import == state.imports.end() || !import->second.unwatched) continue;
        const auto first = base & ~(settleBlockBytes - 1);
        const auto stop = (base + import->second.bytes + settleBlockBytes - 1) & ~(settleBlockBytes - 1);
        if ((first != base || stop != base + import->second.bytes) && comparedImportShares(state, base, first, stop)) continue;
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> span{{{first, stop}}};
        static const bool keepUnderImages = std::getenv("APS5_NO_REWATCH_UNDER_IMAGES") != nullptr;
        if (keepUnderImages && StorageTexture::AnyLiveOverlaps(span)) {
            ++state.settlesKeptCompared;
            continue;
        }
        bases.push_back(base);
        ranges.emplace_back(base & ~(settlePageBytes - 1), (base + import->second.bytes + settlePageBytes - 1) & ~(settlePageBytes - 1));
    }
    state.settleReady.clear();
    if (ranges.empty()) return;
    GuestMemory::Rewatch(ranges);
    for (const auto base : bases) {
        const auto import = state.imports.find(base);
        if (import == state.imports.end() || !GuestMemory::Watched(base, static_cast<std::size_t>(import->second.bytes))) continue;
        import->second.unwatched = false;
        ++state.settlesRewatched;
    }
}

void settleUnwatchedImports(const Context& context, HostImports& state) {
    if (state.pendingSettles.empty() || importsStale(context, state) || context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr || !GuestMemory::GpuMutex().HeldByThisThread()) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - state.settlePolled < settlePollPeriod) return;
    state.settlePolled = now;
    reapSettleBatches(context, state);
    rewatchSettledImports(state);
    submitSettleBatch(context, state);
}

void settleImportsNow(const Context& context, HostImports& state, std::uint64_t begin, std::uint64_t end) {
    if (context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr || !GuestMemory::GpuMutex().HeldByThisThread()) return;
    std::vector<std::uint64_t> needed;
    auto it = state.imports.lower_bound(begin);
    if (it != state.imports.begin()) --it;
    for (; it != state.imports.end() && it->first < end; ++it) {
        auto& entry = it->second;
        if (entry.base + entry.bytes <= begin || !entry.unwatched) continue;
        auto settle = state.pendingSettles.find(it->first);
        if (settle == state.pendingSettles.end()) {
            if (!entry.settleOnDemand || !deferredSettles.load(std::memory_order_relaxed)) continue;
            entry.settleOnDemand = false;
            settle = state.pendingSettles.insert_or_assign(it->first, PendingSettle{}).first;
        }
        if (settle->second.phase == SettlePhase::Wanted) {
            std::erase(state.settleWanted, it->first);
            state.settleWanted.insert(state.settleWanted.begin(), it->first);
        }
        needed.push_back(it->first);
    }
    if (needed.empty()) return;
    auto& deviceSettle = settleFor(context, state);
    const auto wanted = [&] {
        return std::any_of(needed.begin(), needed.end(), [&](std::uint64_t base) {
            const auto settle = state.pendingSettles.find(base);
            return settle != state.pendingSettles.end() && settle->second.phase == SettlePhase::Wanted;
        });
    };
    while (wanted()) {
        if (std::all_of(deviceSettle.batches.begin(), deviceSettle.batches.end(), [](const SettleBatch& batch) { return batch.busy; })) {
            for (const auto& batch : deviceSettle.batches) waitSettleBatch(deviceSettle, context.device, batch);
            reapSettleBatches(context, state);
        }
        submitSettleBatch(context, state);
    }
    for (const auto base : needed) {
        const auto settle = state.pendingSettles.find(base);
        if (settle != state.pendingSettles.end() && settle->second.phase == SettlePhase::Submitted && settle->second.device == context.device) waitSettleBatch(deviceSettle, context.device, deviceSettle.batches[settle->second.batch]);
    }
    reapSettleBatches(context, state);
    rewatchSettledImports(state);
}
#endif

void decideImportWatch(const Context& context, HostImports& state) {
    if (state.watchDevice == context.device) return;
#ifdef _WIN32
    state.watchDevice = context.device;
    state.unwatchImports = true;
    state.unwatchDmaBufImports = false;
    state.watchPrivateImports = false;
    state.dmaBufSelected = false;
    if (context.hostImportAlignment == 0 || !GuestMemory::WriteWatched()) return;
    if (context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr) {
        std::fprintf(stderr, "[write-watch] Windows direct host imports use comparisons (no second queue to settle imports on); separate shared aliases retain guest write tracking\n");
        return;
    }
    const auto probe = ProbeImportWriteProtection(context);
    if (probe.failure != nullptr) {
        std::fprintf(stderr, "[write-watch] host imports of write-watched memory keep the write watch: unknown (probe failed at %s, %d); direct host imports use comparisons, separate shared aliases retain guest write tracking\n", probe.failure, static_cast<int>(probe.result));
        return;
    }
    state.watchPrivateImports = probe.writtenAfterSettle == 0 && probe.writtenAfterSubmit == 0 && probe.writtenAfterSecondSubmit == 0 && probe.writtenAfterFill == 0 && probe.writtenByCpu != 0;
    std::fprintf(stderr, "[write-watch] host imports of write-watched memory keep the write watch: %s (%u of %u scratch pages reported written at the import, %u by its 4-byte settle read, %u after it, %u after a full GPU read, %u after a second, %u after a GPU write, %u after a CPU store); direct host imports of private memory %s, separate shared aliases retain guest write tracking\n", state.watchPrivateImports ? "yes" : "no", probe.writtenAtImport, probe.pages, probe.writtenDuringSettle, probe.writtenAfterSettle, probe.writtenAfterSubmit, probe.writtenAfterSecondSubmit, probe.writtenAfterFill, probe.writtenByCpu, state.watchPrivateImports ? "are settled and stay watched" : "use comparisons");
#else
    const auto request = importWatchRequest();
    state.watchDevice = context.device;
    state.unwatchImports = false;
    state.unwatchDmaBufImports = false;
    state.dmaBufSelected = false;
    if (context.hostImportAlignment == 0) return;
    if (!GuestMemory::WriteWatched()) {
        if (context.dmaBufImport) {
            const auto dmaBuf = ProbeDmaBufImportWriteProtection(context);
            if (dmaBuf.failure != nullptr) {
                std::fprintf(stderr, "[write-watch] no write watch: host imports are compared; dma-buf coherence not measured (probe failed at %s, %d), dma-buf imports %s used\n", dmaBuf.failure, static_cast<int>(dmaBuf.result), state.dmaBufImportsCoherent ? "are" : "are not");
            } else {
                state.dmaBufImportsCoherent = dmaBuf.staleAfterFill == 0;
                std::fprintf(stderr, "[write-watch] no write watch: host imports are compared; dma-buf imports %s CPU-coherent (%u of %u pages stale after a GPU store over CPU writes)\n", state.dmaBufImportsCoherent ? "are" : "are not", dmaBuf.staleAfterFill, dmaBuf.pages);
            }
        }
        return;
    }
    if (request == ImportWatchRequest::Watch) {
        std::fprintf(stderr, "[write-watch] host imports stay watched (APS5_WRITE_WATCH_IMPORTS=watch)\n");
        return;
    }
    if (request == ImportWatchRequest::Unwatch) {
        state.unwatchImports = true;
        state.unwatchDmaBufImports = true;
        std::fprintf(stderr, "[write-watch] host imports are compared, not watched (APS5_WRITE_WATCH_IMPORTS=unwatch)\n");
        return;
    }
    const auto probe = context.hostPointerImport ? ProbeImportWriteProtection(context) : ImportProbe{};
    if (!context.hostPointerImport) {
        state.unwatchImports = true;
        std::fprintf(stderr, "[write-watch] no host pointer imports (the device has no VK_EXT_external_memory_host); only dma-buf imports are probed\n");
    } else if (probe.failure != nullptr) {
        state.unwatchImports = true;
        std::fprintf(stderr, "[write-watch] host imports resolve write protection: unknown (probe failed at %s, %d); imported ranges are compared\n", probe.failure, static_cast<int>(probe.result));
    } else {
        state.unwatchImports = probe.writtenAfterSubmit != 0 || probe.writtenAfterFill != 0;
        std::fprintf(stderr, "[write-watch] host imports resolve write protection: %s (%u of %u scratch pages written after a GPU read, %u after a GPU write, %u after the import); imported ranges %s\n", state.unwatchImports ? "yes" : "no", probe.writtenAfterSubmit, probe.pages, probe.writtenAfterFill, probe.writtenAtImport, state.unwatchImports ? "are compared" : "stay watched");
    }
    state.unwatchDmaBufImports = state.unwatchImports;
    if (!context.dmaBufImport) return;
    const auto dmaBuf = ProbeDmaBufImportWriteProtection(context);
    if (dmaBuf.failure != nullptr) {
        state.unwatchDmaBufImports = true;
        std::fprintf(stderr, "[write-watch] dma-buf imports keep write protection: unknown (probe failed at %s, %d); imported ranges are compared\n", dmaBuf.failure, static_cast<int>(dmaBuf.result));
        return;
    }
    state.unwatchDmaBufImports = dmaBuf.writtenAfterSubmit != 0 || dmaBuf.writtenAfterFill != 0 || dmaBuf.writtenByCpu == 0;
    state.dmaBufImportsCoherent = dmaBuf.staleAfterFill == 0;
    std::fprintf(stderr, "[write-watch] dma-buf imports keep write protection: %s (%u of %u scratch pages written after a GPU read, %u after a GPU write, %u after the import, %u seen after a CPU store, %u stale after a GPU store over CPU writes); imported ranges %s%s\n", state.unwatchDmaBufImports ? "no" : "yes", dmaBuf.writtenAfterSubmit, dmaBuf.pages, dmaBuf.writtenAfterFill, dmaBuf.writtenAtImport, dmaBuf.writtenByCpu, dmaBuf.staleAfterFill, state.unwatchDmaBufImports ? "are compared" : "stay watched", state.dmaBufImportsCoherent ? "" : "; dma-buf imports are not CPU-coherent and are not used");
    state.dmaBufSelected = state.unwatchImports && !state.unwatchDmaBufImports && state.dmaBufImportsCoherent;
#endif
}

struct ImportWatching {
    bool settled = false;
    bool hostPointer = false;
    bool dmaBuf = false;
};

ImportWatching importWatching(const Context& context, HostImports& state, bool alias, bool writable, std::uint64_t bytes) {
    decideImportWatch(context, state);
#ifdef _WIN32
    const bool settled = !alias && state.watchPrivateImports && writable && bytes >= settleFloorBytes;
    const bool watched = alias || !state.unwatchImports || settled;
    return {settled, watched, watched};
#else
    static_cast<void>(alias);
    static_cast<void>(writable);
    static_cast<void>(bytes);
    return {false, !state.unwatchImports, !state.unwatchDmaBufImports};
#endif
}

#ifdef _WIN32
struct ImportPages {
    bool committed = true;
    bool writable = true;
    bool readOnly = true;
    bool shared = true;
    MEMORY_BASIC_INFORMATION refused{};
};

bool writableProtection(std::uint32_t protection) {
    protection &= 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_WRITECOPY;
}

ImportPages importPages(std::uint64_t base, std::uint64_t bytes, bool opening = false) {
    ImportPages pages;
    for (std::uint64_t cursor = base; cursor < base + bytes;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0 || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            pages.committed = false;
            return pages;
        }
        const auto protection = info.Protect & 0xffu;
        const bool pageWritable = (info.Type == MEM_PRIVATE || info.Type == MEM_IMAGE) && writableProtection(protection);
        const bool pageReadOnly = (info.Type != MEM_MAPPED || (info.AllocationProtect & 0xffu) == PAGE_READONLY) && (protection == PAGE_READONLY || protection == PAGE_EXECUTE_READ);
        if (!pageReadOnly) pages.readOnly = false;
        if (info.Type != MEM_MAPPED) pages.shared = false;
        if (info.Type != MEM_MAPPED && !pageWritable && !pageReadOnly && pages.refused.BaseAddress == nullptr) pages.refused = info;
        if (!pageWritable) pages.writable = false;
        if (opening && !pages.writable && !pages.shared) return pages;
        cursor = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
    }
    return pages;
}
#endif

std::shared_ptr<const GuestAllocations::Range> leasedRangeOwner(const GuestAllocations::Lease& lease, std::uint64_t base) {
    const auto found = std::lower_bound(lease.begin(), lease.end(), base, [](const auto& range, std::uint64_t value) { return range->address < value; });
    return found != lease.end() && (*found)->address == base ? *found : nullptr;
}

std::shared_ptr<const GuestAllocations::Range> leasedRangeOwner(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto found = std::upper_bound(lease.begin(), lease.end(), begin, [](std::uint64_t value, const auto& range) { return value < range->address; });
    if (found == lease.begin()) return nullptr;
    const auto& range = *std::prev(found);
    return begin >= range->address && end <= range->address + range->bytes ? range : nullptr;
}

struct ImportRun {
    std::uint64_t base = 0;
    std::uint64_t bytes = 0;
    std::vector<std::shared_ptr<const GuestAllocations::Range>> ranges;
};

bool importStaysWatched(const Context& context, HostImports& state, const ImportRun& run) {
    if (std::any_of(run.ranges.begin(), run.ranges.end(), [](const auto& range) { return !range->writable; })) return false;
    if (!GuestMemory::Watched(run.base, static_cast<std::size_t>(run.bytes))) return false;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION first{};
    if (!importWatching(context, state, false, true, run.bytes).hostPointer && (VirtualQuery(reinterpret_cast<const void*>(run.base), &first, sizeof(first)) == 0 || first.Type != MEM_MAPPED)) return false;
    const auto pages = importPages(run.base, run.bytes, true);
    if (!pages.committed || (!pages.writable && !pages.shared)) return false;
    return importWatching(context, state, !pages.writable, pages.writable, run.bytes).hostPointer;
#else
    const auto watching = importWatching(context, state, false, true, run.bytes);
    return sharedBacked(context, run.base, run.bytes) ? watching.dmaBuf && (watching.hostPointer || state.dmaBufSelected) : watching.hostPointer;
#endif
}

bool continuesRun(const GuestAllocations::Range& before, const GuestAllocations::Range& after) {
    return before.address + before.bytes == after.address && before.readable && after.readable && before.writable == after.writable && before.gpu == after.gpu;
}

std::size_t leasedIndexContaining(const GuestAllocations::Lease& lease, std::uint64_t address) {
    const auto found = std::upper_bound(lease.begin(), lease.end(), address, [](std::uint64_t value, const auto& range) { return value < range->address; });
    if (found == lease.begin()) return lease.size();
    const auto index = static_cast<std::size_t>(std::prev(found) - lease.begin());
    const auto& range = *lease[index];
    return range.readable && address < range.address + range.bytes ? index : lease.size();
}

std::atomic<std::uint64_t> importWindow{0};

ImportRun leasedRun(const GuestAllocations::Lease& lease, std::size_t first, std::size_t last) {
    ImportRun run{lease[first]->address, lease[last]->address + lease[last]->bytes - lease[first]->address, {}};
    run.ranges.assign(lease.begin() + static_cast<std::ptrdiff_t>(first), lease.begin() + static_cast<std::ptrdiff_t>(last) + 1);
    return run;
}

template<typename Remembered>
const Remembered* rememberedRun(const std::map<std::pair<std::uint64_t, std::uint64_t>, Remembered>& runs, const GuestAllocations::Lease& lease, std::size_t first, std::size_t last) {
    const auto base = lease[first]->address;
    const auto found = runs.find({base, lease[last]->address + lease[last]->bytes - base});
    if (found == runs.end() || found->second.ranges.size() != last - first + 1) return nullptr;
    for (std::size_t index = first; index <= last; ++index) {
        const auto& remembered = found->second.ranges[index - first];
        if (remembered.owner_before(lease[index]) || lease[index].owner_before(remembered)) return nullptr;
    }
    return &found->second;
}

bool stillRefused(const HostImports& state, const ImportRefusal* refusal, std::uint64_t budget) {
    return refusal != nullptr && budget != 0 && budget <= refusal->budget && pinnedTotal(state) >= refusal->pinnedBytes;
}

struct RunBounds {
    std::size_t first = 0;
    std::size_t last = 0;
    bool found = false;
};

std::pair<std::size_t, std::size_t> walkRun(const GuestAllocations::Lease& lease, std::size_t index, std::uint64_t low, std::uint64_t high) {
    auto first = index;
    while (first > 0 && lease[first - 1]->address >= low && continuesRun(*lease[first - 1], *lease[first])) --first;
    auto last = index;
    while (last + 1 < lease.size() && lease[last + 1]->address + lease[last + 1]->bytes <= high && continuesRun(*lease[last], *lease[last + 1])) ++last;
    return {first, last};
}

std::pair<std::size_t, std::size_t> continuedRun(const GuestAllocations::Lease& lease, std::size_t index, std::uint64_t window, std::uint64_t low, std::uint64_t high) {
    static const bool memo = std::getenv("APS5_NO_RUN_MEMO") == nullptr;
    if (!memo) return walkRun(lease, index, low, high);
    struct Memo {
        std::weak_ptr<const GuestAllocations::Lease::Ranges> snapshot;
        std::size_t index = 0;
        std::uint64_t window = 0;
        std::uint64_t low = 0;
        std::pair<std::size_t, std::size_t> run;
    };
    struct MemoStorage {};
    auto& memos = HostThreadLocal<std::array<Memo, 1024>, MemoStorage>();
    const auto& snapshot = lease.Snapshot();
    const auto block = window != 0 ? low / window : 0;
    const auto slot = (reinterpret_cast<std::uintptr_t>(snapshot.get()) / 16u ^ index * 0x9e3779b1u ^ block * 0x85ebca6bu) % memos.size();
    auto& entry = memos[slot];
    if (!entry.snapshot.owner_before(snapshot) && !snapshot.owner_before(entry.snapshot) && entry.index == index && entry.window == window && entry.low == low) return entry.run;
    entry = Memo{snapshot, index, window, low, walkRun(lease, index, low, high)};
    return entry.run;
}

bool joinedWithin(const HostImports& state, std::uint64_t begin, std::uint64_t end) {
    auto it = state.imports.lower_bound(begin);
    if (it != state.imports.begin() && std::prev(it)->second.joined && std::prev(it)->first + std::prev(it)->second.bytes > begin) return true;
    for (; it != state.imports.end() && it->first < end; ++it) {
        if (it->second.joined) return true;
    }
    return false;
}

RunBounds importRunBounds(const GuestAllocations::Lease& lease, std::uint64_t address, std::uint64_t largest = std::numeric_limits<std::uint64_t>::max(), std::uint64_t budget = 0) {
    const auto index = leasedIndexContaining(lease, address);
    if (index == lease.size()) return {};
    const RunBounds single{index, index, true};
    const auto& containing = *lease[index];
    const auto& state = Imports();
    const auto window = importWindow.load(std::memory_order_relaxed);
    std::uint64_t low = 0;
    std::uint64_t high = std::numeric_limits<std::uint64_t>::max();
    if (window != 0) {
        low = address / window * window;
        high = low + window;
        if (containing.address < low || containing.address + containing.bytes > high) return single;
    }
    const auto [first, last] = continuedRun(lease, index, window, low, high);
    const auto skipped = [&](std::size_t from, std::size_t to) { return rememberedRun(state.failedBlocks, lease, from, to) != nullptr || stillRefused(state, rememberedRun(state.refusals, lease, from, to), budget); };
    if (window == 0) return first == last || lease[last]->address + lease[last]->bytes - lease[first]->address > largest || skipped(first, last) ? single : RunBounds{first, last, true};
    const auto bounding = [&](std::size_t from, std::size_t to, std::uint64_t value, bool start) {
        const auto found = std::lower_bound(lease.begin() + static_cast<std::ptrdiff_t>(from), lease.begin() + static_cast<std::ptrdiff_t>(to) + 1, value, [start](const auto& range, std::uint64_t wanted) { return (start ? range->address : range->address + range->bytes) < wanted; });
        const auto position = static_cast<std::size_t>(found - lease.begin());
        return position <= to && (start ? (*found)->address : (*found)->address + (*found)->bytes) == value ? position : lease.size();
    };
    for (auto size = window; size >= containing.bytes; size /= 2) {
        if (size > largest) continue;
        const auto blockLow = address / size * size;
        const auto blockHigh = blockLow + size;
        if (containing.address < blockLow || containing.address + containing.bytes > blockHigh) break;
        if (blockLow < lease[first]->address || blockHigh > lease[last]->address + lease[last]->bytes) continue;
        const auto blockFirst = bounding(first, index, blockLow, true);
        const auto blockLast = bounding(index, last, blockHigh, false);
        if (blockFirst == lease.size() || blockLast == lease.size()) continue;
        if (blockFirst == blockLast) break;
        if (skipped(blockFirst, blockLast) || joinedWithin(state, blockLow, blockHigh)) continue;
        return {blockFirst, blockLast, true};
    }
    return single;
}

}
}
