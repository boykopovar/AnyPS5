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
std::vector<ImportPiece> importPieces(const Context& context, HostImports& state, const GuestAllocations::Range& range, const GuestAllocations::Lease& lease) {
    const auto limit = importMaxBytes.load(std::memory_order_relaxed);
    const auto window = rangeImportWindow.load(std::memory_order_relaxed);
    const auto alignment = context.hostImportAlignment;
    if (limit < 2 * window || window == 0 || alignment == 0 || window % alignment != 0 || range.address % alignment != 0) return {};
    const auto step = (limit / window - 1) * window;
    const auto end = range.address + range.bytes;
    std::vector<ImportPiece> pieces;
    for (auto cursor = range.address; cursor < end;) {
        const auto wanted = end - cursor < step + window ? end : cursor + step;
        const auto* entry = importAllocation(context, state, cursor, lease, ImportRequester::Build, wanted, false);
        if (entry == nullptr || entry->base > cursor || entry->bytes > limit) return {};
        const auto to = std::min(end, entry->base + entry->bytes);
        if (to <= cursor) return {};
        pieces.push_back({cursor, to, entry});
        cursor = to;
    }
    return pieces;
}

std::vector<ImportPiece> budgetedImportPieces(const Context& context, HostImports& state, const GuestAllocations::Range& range, const GuestAllocations::Lease& lease) {
    const auto window = rangeImportWindow.load(std::memory_order_relaxed);
    const auto alignment = context.hostImportAlignment;
    if (!range.releasable || !GuestMemory::Watched(range.address, range.bytes) || window == 0 || alignment == 0 || window % alignment != 0 || range.address % alignment != 0 || range.bytes <= window) return {};
    const auto end = range.address + range.bytes;
    std::vector<ImportPiece> pieces;
    for (auto cursor = range.address; cursor < end;) {
        const HostImport* entry = nullptr;
        if (const auto* previous = peekImport(state, cursor, cursor + 1); previous != nullptr && previous->base >= range.address && previous->base + previous->bytes <= end) {
            entry = importAllocation(context, state, cursor, lease, ImportRequester::Build, std::min(end, previous->base + previous->bytes), false);
        }
        if (entry == nullptr) {
            const auto budget = effectiveBudget(context, state);
            auto room = budget - std::min(budget, pinnedTotal(state));
            if (const auto limit = importMaxBytes.load(std::memory_order_relaxed); limit != 0) room = std::min(room, limit);
            if (room < window) break;
            auto wanted = cursor + std::min(end - cursor, window);
            if (const auto bounds = importWindowBounds(range, cursor, wanted, alignment); bounds && bounds->second - bounds->first > room) {
                wanted = range.address + (range.bytes / window - 1) * window;
                if (wanted <= cursor) break;
            }
            entry = importAllocation(context, state, cursor, lease, ImportRequester::Build, wanted, false);
        }
        if (entry == nullptr || entry->base > cursor) break;
        const auto to = std::min(end, entry->base + entry->bytes);
        if (to <= cursor) break;
        pieces.push_back({cursor, to, entry});
        cursor = to;
    }
    if (pieces.empty()) return {};
    if (pieces.back().end != end) pieces.push_back({pieces.back().end, end, nullptr});
    return pieces;
}

const HostImport* importJoined(const Context& context, HostImports& state, std::uint64_t begin, std::uint64_t end, const GuestAllocations::Lease& lease) {
    if (const auto* low = peekImport(state, begin, begin + 1); low != nullptr && low->joined) begin = low->base;
    if (const auto* high = peekImport(state, end - 1, end); high != nullptr && high->joined) end = high->base + high->bytes;
    auto run = joinedRun(lease, begin, end);
    if (run.ranges.empty()) return nullptr;
    if (const auto failed = state.failedJoins.find(run.base); failed != state.failedJoins.end() && sameRun(failed->second, run)) return nullptr;
    const auto existing = state.imports.find(run.base);
    const bool existed = existing != state.imports.end() && sameRun(existing->second, run);
    const auto* entry = importRunAt(context, state, std::move(run), true, begin, lease, end, true, ImportRequester::Join);
    if (entry != nullptr && !existed) ++state.epoch;
    return entry;
}

const HostImport* importRunAt(const Context& context, HostImports& state, ImportRun run, bool joined, std::uint64_t address, const GuestAllocations::Lease& lease, std::uint64_t end, bool evict, ImportRequester requester) {
    auto& counts = state.counts[static_cast<std::size_t>(requester)];
    const auto base = run.base;
    const auto bytes = run.bytes;
    auto& failures = joined ? state.failedJoins : state.failed;
    const bool stepDown = !joined && run.ranges.size() > 1 && importWindow.load(std::memory_order_relaxed) != 0;
    const auto alignment = context.hostImportAlignment;
    if (joined && (alignment == 0 || base % alignment != 0 || bytes % alignment != 0)) {
        failures.insert_or_assign(base, ImportFailure{base, bytes, GuestAllocations::Mapped(run.ranges.begin(), run.ranges.end())});
        return nullptr;
    }
    if (const auto found = state.imports.find(base); found != state.imports.end() && sameRun(found->second, run)) {
        found->second.lastUse = ++state.useCounter;
        return &found->second;
    }
    if (alignment == 0 || base % alignment != 0 || bytes % alignment != 0) return nullptr;
    if (const auto limit = importMaxBytes.load(std::memory_order_relaxed); limit != 0 && bytes > limit) return nullptr;
    if (const auto failed = failures.find(base); !stepDown && failed != failures.end()) {
        if (sameRun(failed->second, run)) return nullptr;
        failures.erase(failed);
    }
    const auto budget = effectiveBudget(context, state);
    const auto containing = std::find_if(run.ranges.begin(), run.ranges.end(), [address](const auto& range) { return address >= range->address && address < range->address + range->bytes; });
    if (const auto remembered = state.evictingRefusals.find({base, bytes}); evict && !state.mirrorsYield && remembered != state.evictingRefusals.end()) {
        if (remembered->second.joined == joined && evictingRefusalCurrent(state, remembered->second, budget) && sameRun(remembered->second, run)) {
            ++counts.refused;
            counts.refusedBytes += bytes;
            ++counts.remembered;
            if (!joined) oweGrowth(context, state, lease, base, bytes, GrowthBlock::Budget, budget);
            const auto room = remembered->second.room - std::min(remembered->second.room, remembered->second.budget - budget);
            if (stepDown && containing != run.ranges.end() && room >= (*containing)->bytes) return importAllocation(context, state, address, lease, requester, end, evict, std::min(bytes / 2, room));
            return nullptr;
        }
        state.evictingRefusals.erase(remembered);
    }
    if (requester == ImportRequester::ColorTarget) {
#ifdef _WIN32
        const auto commits = GuestArena::GuestArenaCommitGeneration_nid_postfix();
#else
        const std::uint64_t commits = 0;
#endif
        const auto watches = GuestMemory::UnwatchSerial();
        const std::pair key{base, bytes};
        if (const auto refused = state.gateRefusals.find(key); refused != state.gateRefusals.end() && refused->second.commits == commits && refused->second.watches == watches && sameRun(refused->second, run)) return nullptr;
        if (!importStaysWatched(context, state, run)) {
            state.gateRefusals.insert_or_assign(key, GateRefusal{base, bytes, GuestAllocations::Mapped(run.ranges.begin(), run.ranges.end()), commits, watches});
            return nullptr;
        }
        state.gateRefusals.erase(key);
    }
    auto first = state.imports.lower_bound(base);
    if (first != state.imports.begin() && std::prev(first)->second.base + std::prev(first)->second.bytes > base) --first;
    std::uint64_t replaced = 0;
    const auto held = [&](const HostImport& entry) {
        if (joined || !pointerHeld(state, entry)) return false;
        const bool expandedWindow = run.ranges.size() == 1 && entry.ranges.size() == 1 && entry.range == run.ranges.front() && base <= entry.base && base + bytes >= entry.base + entry.bytes && bytes > entry.bytes;
        return !expandedWindow || Recorder::Active() == nullptr;
    };
    for (auto it = first; it != state.imports.end() && it->first < base + bytes; ++it) {
        if (held(it->second)) {
            oweGrowth(context, state, lease, base, bytes, GrowthBlock::Held, budget);
            return stepDown ? importAllocation(context, state, address, lease, requester, end, evict, bytes / 2) : nullptr;
        }
        replaced += it->second.pinnedBytes;
    }
    const bool immediate = retireFreesAtOnce(true);
    const auto pinned = pinnedTotal(state);
    const auto live = pinned - (immediate ? replaced : 0);
    auto ready = std::numeric_limits<std::uint64_t>::max();
    auto readyHeld = std::numeric_limits<std::uint64_t>::max();
    const auto evictable = evict && live + bytes > budget ? evictableBytes(state, base, base + bytes, ready, readyHeld) * (immediate ? 1 : 0) : 0;
    if (live + bytes > budget + evictable) {
        static auto lastReport = std::chrono::steady_clock::now() - std::chrono::seconds(60);
        ++counts.refused;
        counts.refusedBytes += bytes;
        const auto remember = [&](std::uint64_t at, std::uint64_t size, GuestAllocations::Mapped ranges) {
            if (!evict && !joined) state.refusals.insert_or_assign(std::pair{at, size}, ImportRefusal{at, size, std::move(ranges), pinned, budget});
        };
        remember(base, bytes, GuestAllocations::Mapped(run.ranges.begin(), run.ranges.end()));
        if (!joined) oweGrowth(context, state, lease, base, bytes, GrowthBlock::Budget, budget);
        if (std::chrono::steady_clock::now() - lastReport > std::chrono::seconds(10)) {
            lastReport = std::chrono::steady_clock::now();
            std::uint64_t refused = 0;
            std::uint64_t refusedBytes = 0;
            std::uint64_t remembered = 0;
            for (const auto& requested : state.counts) {
                refused += requested.refused;
                refusedBytes += requested.refusedBytes;
                remembered += requested.remembered;
            }
            std::fprintf(stderr, "[gpu] host import budget: %llu MiB pinned of %llu MiB (%s); %llu imports (%llu MiB) refused so far, %llu of them remembered, last 0x%llx+0x%llx by %s\n", static_cast<unsigned long long>(pinned >> 20u), static_cast<unsigned long long>(budget >> 20u), budgetSource(context, state, budget).c_str(), static_cast<unsigned long long>(refused), static_cast<unsigned long long>(refusedBytes >> 20u), static_cast<unsigned long long>(remembered), static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), ImportRequesterName(requester));
        }
        const auto room = budget + evictable > live ? budget + evictable - live : 0;
        if (evict) state.evictingRefusals.insert_or_assign(std::pair{base, bytes}, EvictingRefusal{base, bytes, GuestAllocations::Mapped(run.ranges.begin(), run.ranges.end()), state.epoch, state.framesEnded, pinned, budget, room, importWindow.load(std::memory_order_relaxed), state.buildFloor, ready, readyHeld, joined, immediate});
        if (stepDown && containing != run.ranges.end() && room >= (*containing)->bytes) return importAllocation(context, state, address, lease, requester, end, evict, std::min(bytes / 2, room));
        if (containing != run.ranges.end() && run.ranges.size() > 1) remember((*containing)->address, (*containing)->bytes, GuestAllocations::Mapped{*containing});
        ++state.budgetRefusals;
        return nullptr;
    }
    if (live + bytes > budget) retireLeastRecent(context, state, lease, live + bytes - budget, base, base + bytes);
    ImportConstruction owner(context, base, bytes);
    auto& entry = owner.entry;
    entry.range = run.ranges.front();
    entry.ranges = std::move(run.ranges);
    entry.memoryBase = base;
    entry.pinnedBytes = bytes;
    entry.backingSerial = entry.ranges.size() == 1 ? entry.range->backingSerial : 0;
    const auto failedBlockImport = [&]() -> const HostImport* {
        ++counts.failed;
        state.failedBlocks.insert_or_assign(std::pair{base, bytes}, ImportFailure{base, bytes, GuestAllocations::Mapped(entry.ranges.begin(), entry.ranges.end())});
        return importAllocation(context, state, address, lease, requester, end, evict);
    };
    const auto failedImport = [&]() -> const HostImport* {
        if (stepDown) return failedBlockImport();
        ++counts.failed;
        failures.insert_or_assign(base, ImportFailure{base, bytes, GuestAllocations::Mapped(entry.ranges.begin(), entry.ranges.end())});
        if (!joined) oweGrowth(context, state, lease, base, bytes, GrowthBlock::Failed, budget);
        return nullptr;
    };
#ifdef _WIN32
    const GuestArena::HostImportAccess importAccess(reinterpret_cast<void*>(base), static_cast<std::size_t>(bytes));
    const auto pages = importPages(base, bytes);
    if (!pages.committed) return failedImport();
    const bool writable = pages.writable;
    if (!writable && pages.readOnly) return failedImport();
    if (!writable) {
        if (pages.refused.BaseAddress != nullptr) {
            char text[256];
            std::snprintf(text, sizeof(text), "AGC graphics: host import of 0x%llx+0x%llx: memory at 0x%llx (type 0x%lx, protection 0x%lx) is neither read-write, read-only nor a shared mapping", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), reinterpret_cast<unsigned long long>(pages.refused.BaseAddress), pages.refused.Type, pages.refused.Protect);
            throw std::runtime_error(text);
        }
        if (stepDown && !pages.shared) {
            state.failedBlocks.insert_or_assign(std::pair{base, bytes}, ImportFailure{base, bytes, GuestAllocations::Mapped(entry.ranges.begin(), entry.ranges.end())});
            return importAllocation(context, state, address, lease, requester, end, evict);
        }
        std::erase_if(state.gateRefusals, [&](const auto& refusal) { return refusal.first.first < base + bytes && base < refusal.first.first + refusal.first.second; });
        bool boundsRefused = false;
        try {
            entry.alias = entry.ranges.size() == 1
                ? GuestArena::GuestArenaMapAliasForImport_nid_no_patch(static_cast<std::uintptr_t>(base), static_cast<std::size_t>(bytes), &boundsRefused)
                : GuestArena::GuestArenaMapAlias_nid_postfix(static_cast<std::uintptr_t>(base), static_cast<std::size_t>(bytes));
        } catch (const std::runtime_error&) {
            if (entry.ranges.size() == 1) {
                if (!boundsRefused) throw;
                return failedImport();
            }
            return joined ? failedImport() : failedBlockImport();
        }
        if (entry.alias == nullptr) return nullptr;
    }
#else
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(base), static_cast<std::size_t>(bytes), true)) return failedImport();
#endif
    VkResult result = VK_SUCCESS;
    const char* step = nullptr;
#ifdef _WIN32
    const auto watching = importWatching(context, state, entry.alias != nullptr, writable, bytes);
    const bool settled = watching.settled;
#else
    const auto watching = importWatching(context, state, false, true, bytes);
#endif
    const bool watchedHostPointer = watching.hostPointer;
    const bool watchedDmaBuf = watching.dmaBuf;
#ifndef _WIN32
    const bool dmaBufOnly = state.dmaBufSelected && sharedBacked(context, base, bytes);
#else
    constexpr bool dmaBufOnly = false;
#endif
    if (!dmaBufOnly) GuestMemory::ImportWatched(base, bytes, [&] {
        step = watchedDmaBuf == watchedHostPointer ? createImport(context, entry, result, state.dmaBufImportsCoherent) : createHostPointerImport(context, entry, result);
#ifdef _WIN32
        if (step == nullptr && settled && (step = settleImport(context, state, entry, result)) != nullptr) {
            destroyImport(context, entry);
            entry.buffer = VK_NULL_HANDLE;
            entry.memory = VK_NULL_HANDLE;
        }
#endif
        return step == nullptr;
    }, watchedHostPointer);
#ifndef _WIN32
    if (dmaBufOnly) {
        GuestMemory::ImportWatched(base, bytes, [&] {
            try {
                step = createSharedBackingImport(context, entry, result);
            } catch (const std::exception& error) {
                throw DmaBufImportFailure(error.what());
            }
            return step == nullptr;
        }, watchedDmaBuf);
        if (step != nullptr && (std::strcmp(step, "UDMABUF_CREATE") == 0 || std::strcmp(step, "UDMABUF_CREATE_LIST") == 0)) {
            if (requester == ImportRequester::ColorTarget && !watchedHostPointer) return failedImport();
            std::fprintf(stderr, "[gpu] dma-buf import of 0x%llx+0x%llx refused by udmabuf; host-pointer import\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes));
            GuestMemory::ImportWatched(base, bytes, [&] {
                step = createHostPointerImport(context, entry, result);
                return step == nullptr;
            }, watchedHostPointer);
        } else if (step != nullptr) {
            char message[160];
            std::snprintf(message, sizeof(message), "AGC graphics: dma-buf import of 0x%llx+0x%llx failed at %s (%d)", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), step, static_cast<int>(result));
            throw DmaBufImportFailure(message);
        }
    } else if (step != nullptr && watchedDmaBuf != watchedHostPointer && state.dmaBufImportsCoherent) {
        int runsFile = -1;
        std::array<GuestArena::SharedBackingRun, DmaBufRunLimit> runs{};
        std::size_t count = 0;
        if (resolveBackingRuns(entry, runsFile, runs, count)) {
            OwnedDescriptor owned(runsFile);
            GuestMemory::ImportWatched(base, bytes, [&] {
                step = createBackingImport(context, entry, owned.Release(), runs.data(), count, result);
                return step == nullptr;
            }, watchedDmaBuf, false);
        }
    }
    if (step == nullptr && entry.dmaBuf && bytes >= settleFloorBytes && context.settleQueue != VK_NULL_HANDLE && context.settleQueueMutex != nullptr) {
        static const bool traceSettle = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        const auto start = std::chrono::steady_clock::now();
        if ((step = settleImport(context, state, entry, result)) != nullptr) {
            destroyImport(context, entry);
            entry.buffer = VK_NULL_HANDLE;
            entry.memory = VK_NULL_HANDLE;
            entry.chunk.reset();
            if (dmaBufOnly) {
                char message[160];
                std::snprintf(message, sizeof(message), "AGC graphics: dma-buf import of 0x%llx+0x%llx failed at %s (%d)", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), step, static_cast<int>(result));
                throw DmaBufImportFailure(message);
            }
        } else if (traceSettle) {
            std::fprintf(stderr, "[gpu] settled the dma-buf import of 0x%llx+0x%llx on the settle queue in %.2f ms\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        }
    }
#endif
    entry.unwatched = step == nullptr && !(entry.dmaBuf ? watchedDmaBuf : watchedHostPointer);
    if (step != nullptr) {
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(std::exchange(entry.alias, nullptr));
#endif
        std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx failed at %s (%d); falling back to copies, requested by %s\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), step, static_cast<int>(result), ImportRequesterName(requester));
        const bool exhausted = result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY;
        if (std::string_view(step) == "vkAllocateMemory") {
            state.allocationFailed = true;
            if (!exhausted) state.allocationFailures = 0;
            if (exhausted && (++state.allocationFailures >= 3 || live + bytes + budget / 8 >= budget)) {
                state.ceiling = std::max<std::uint64_t>(pinnedTotal(state), 1);
                std::fprintf(stderr, "[gpu] host import budget lowered to the %.1f MiB pinned after vkAllocateMemory failed %s\n", pinnedTotal(state) / 1048576.0, state.allocationFailures >= 3 ? "3 times in a row" : "within an eighth of the budget");
            }
        }
#ifdef _WIN32
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        for (std::uint64_t cursor = base; trace && cursor < base + bytes;) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) break;
            const auto regionEnd = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
            std::fprintf(stderr, "[gpu]   0x%llx+0x%llx state 0x%lx protect 0x%lx type 0x%lx allocation 0x%llx\n", static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(std::min(regionEnd, base + bytes) - cursor), info.State, info.Protect, info.Type, reinterpret_cast<unsigned long long>(info.AllocationBase));
            cursor = regionEnd;
        }
#else
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        if (std::FILE* maps = trace ? std::fopen("/proc/self/maps", "r") : nullptr) {
            char line[512];
            while (std::fgets(line, sizeof(line), maps) != nullptr) {
                unsigned long long first = 0;
                unsigned long long last = 0;
                if (std::sscanf(line, "%llx-%llx", &first, &last) != 2 || last <= base || first >= base + bytes) continue;
                std::fprintf(stderr, "[gpu]   %s", line);
            }
            std::fclose(maps);
        }
#endif
        return failedImport();
    }
    if (requester == ImportRequester::ColorTarget && !GuestMemory::Watched(base, static_cast<std::size_t>(bytes))) return failedImport();
    first = state.imports.lower_bound(base);
    if (first != state.imports.begin() && std::prev(first)->second.base + std::prev(first)->second.bytes > base) --first;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwatched;
    for (auto it = first; it != state.imports.end() && it->first < base + bytes;) {
        const auto next = std::next(it);
        noteRetiredUnwatched(unwatched, it->second, lease);
        retireImport(context.device, state, it, lease);
        it = next;
    }
    if (pinnedTotal(state) + bytes > budget) retireLeastRecent(context, state, lease, pinnedTotal(state) + bytes - budget, base, base + bytes);
    static std::uint64_t importedBytes = 0;
    importedBytes += bytes;
    static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
    state.liveBytes += bytes;
    state.allocationFailures = 0;
    ++counts.made;
    counts.madeBytes += bytes;
    for (auto it = state.refusals.lower_bound({base, 0}); it != state.refusals.end() && it->first.first < base + bytes;) it = it->first.first + it->first.second <= base + bytes ? state.refusals.erase(it) : std::next(it);
    if (trace) std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx ok (%zu live, %.1f MiB live, %.1f MiB ever)%s by %s\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), state.imports.size() + 1, state.liveBytes / 1048576.0, importedBytes / 1048576.0, joined ? ", joined at bind" : "", ImportRequesterName(requester));
    entry.joined = joined;
    entry.lastUse = ++state.useCounter;
    state.pendingImports.insert(base);
#ifdef _WIN32
    if (deferredSettles.load(std::memory_order_relaxed) && entry.alias == nullptr && state.watchPrivateImports && writable) {
        if (entry.unwatched) {
            state.pendingSettles.insert_or_assign(base, PendingSettle{});
            state.settleWanted.push_back(base);
        } else if (base % settleBlockBytes == 0 && bytes % settleBlockBytes == 0 && settled && !GuestMemory::Watched(base, static_cast<std::size_t>(bytes))) {
            entry.unwatched = true;
            state.pendingSettles.insert_or_assign(base, PendingSettle{SettlePhase::Settled, 0});
            state.settleReady.push_back(base);
        }
    }
#endif
    const auto [placed, added] = state.imports.emplace(base, entry);
    if (added) {
        ++state.inserted;
        importSetChanged();
    }
    coverGrowth(state, base, bytes);
    owner.armed = false;
    rewatchRetired(state, std::move(unwatched));
    return &placed->second;
}

}
}
