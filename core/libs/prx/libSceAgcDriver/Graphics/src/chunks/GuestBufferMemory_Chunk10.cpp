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
bool collectWriteWatch(std::uint64_t base, std::uint64_t bytes, std::uint32_t& pages) {
    pages = 0;
    return GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(static_cast<std::uintptr_t>(base), static_cast<std::size_t>(bytes), [](void* count, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::uint32_t*>(count) += static_cast<std::uint32_t>((end - begin) / 4096); }, &pages);
}
#endif

}

ImportProbe ProbeImportWriteProtection(const Context& context) {
    ImportProbe probe;
#ifdef _WIN32
    if (context.hostImportAlignment == 0) {
        probe.failure = "host import support";
        return probe;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t alignment = std::max<std::uint64_t>(context.hostImportAlignment, page);
    const std::uint64_t bytes = (4096 * page + alignment - 1) / alignment * alignment;
    probe.pages = static_cast<std::uint32_t>(bytes / page);
    void* raw = VirtualAlloc(nullptr, static_cast<SIZE_T>(bytes + alignment), MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
    if (raw == nullptr) {
        probe.failure = "VirtualAlloc";
        return probe;
    }
    const auto base = (reinterpret_cast<std::uint64_t>(raw) + alignment - 1) / alignment * alignment;
    auto* scratch = reinterpret_cast<volatile std::uint8_t*>(base);
    for (std::uint64_t offset = 0; offset < bytes; offset += page) scratch[offset] = 1;
    std::vector<void*> written(static_cast<std::size_t>(bytes / page));
    const auto collect = [&](std::uint32_t& pages) {
        ULONG_PTR count = written.size();
        DWORD granularity = 0;
        if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, reinterpret_cast<void*>(base), static_cast<SIZE_T>(bytes), written.data(), &count, &granularity) != 0) return false;
        pages = static_cast<std::uint32_t>(count);
        return true;
    };
    std::uint32_t touched = 0;
    if (!collect(touched)) probe.failure = "GetWriteWatch";
    else if (context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr) probe.failure = "the settle queue";
    else runImportProbe(context, base, bytes, [&](HostImport& import, VkResult& result) { return createImport(context, import, result, false); }, collect, probe, [&](HostImport& import, VkResult& result) { return settleImport(context, Imports(), import, result); });
    VirtualFree(raw, 0, MEM_RELEASE);
    return probe;
#else
    if (context.hostImportAlignment == 0) {
        probe.failure = "host import support";
        return probe;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t alignment = std::max<std::uint64_t>(context.hostImportAlignment, page);
    const std::uint64_t bytes = (4 * page + alignment - 1) / alignment * alignment;
    probe.pages = static_cast<std::uint32_t>(bytes / page);
    void* raw = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        probe.failure = "mmap";
        return probe;
    }
    const auto base = (reinterpret_cast<std::uint64_t>(raw) + alignment - 1) / alignment * alignment;
    auto* scratch = reinterpret_cast<volatile std::uint8_t*>(base);
    for (std::uint64_t offset = 0; offset < bytes; offset += page) scratch[offset] = 1;
    if (GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    runImportProbe(context, base, bytes, [&](HostImport& import, VkResult& result) { return createImport(context, import, result, false); }, [&](std::uint32_t& pages) { return collectWriteWatch(base, bytes, pages); }, probe);
    if (GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    munmap(raw, bytes + alignment);
    return probe;
#endif
}

ImportProbe ProbeDmaBufImportWriteProtection(const Context& context) {
    ImportProbe probe;
#ifdef _WIN32
    static_cast<void>(context);
    probe.failure = "the Linux write watch";
    return probe;
#elif defined(__APPLE__)
    static_cast<void>(context);
    probe.failure = "dma-buf import support";
    return probe;
#else
    if (!context.dmaBufImport) {
        probe.failure = "dma-buf import support";
        return probe;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t alignment = std::max<std::uint64_t>(context.hostImportAlignment, page);
    const std::uint64_t bytes = (4 * page + alignment - 1) / alignment * alignment;
    probe.pages = static_cast<std::uint32_t>(bytes / page);
    const int file = memfd_create("aps5-dma-buf-import-probe", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (file < 0) {
        probe.failure = "memfd_create";
        return probe;
    }
    void* mapped = MAP_FAILED;
    if (ftruncate(file, static_cast<off_t>(bytes)) == 0 && fcntl(file, F_ADD_SEALS, F_SEAL_SHRINK) == 0) mapped = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (mapped == MAP_FAILED) {
        close(file);
        probe.failure = "the shared scratch mapping";
        return probe;
    }
    const auto base = reinterpret_cast<std::uint64_t>(mapped);
    auto* scratch = reinterpret_cast<volatile std::uint8_t*>(base);
    for (std::uint64_t offset = 0; offset < bytes; offset += page) scratch[offset] = 1;
    if (GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    runImportProbe(context, base, bytes, [&](HostImport& import, VkResult& result) -> const char* {
        const int copy = dup(file);
        if (copy < 0) {
            result = VK_ERROR_INITIALIZATION_FAILED;
            return "dup";
        }
        const GuestArena::SharedBackingRun whole{0, import.bytes};
        return createDmaBufImport(context, import, copy, std::span<const GuestArena::SharedBackingRun>(&whole, 1), result);
    }, [&](std::uint32_t& pages) { return GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix() ? collectWriteWatch(base, bytes, pages) : (pages = 0, true); }, probe);
    if (GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    munmap(mapped, bytes);
    close(file);
    return probe;
#endif
}

ImportWatch PrepareImportWatch(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    decideImportWatch(context, state);
    return state.unwatchImports ? ImportWatch::Unwatch : ImportWatch::Watch;
}

bool DmaBufImportSelected(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    decideImportWatch(context, state);
    return state.dmaBufSelected;
}

void SetImportWatch(const Context& context, ImportWatch watch) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.watchDevice = context.device;
    state.unwatchImports = watch == ImportWatch::Unwatch;
    state.unwatchDmaBufImports = state.unwatchImports;
    state.dmaBufSelected = false;
    state.gateRefusals.clear();
}

void SetHostImportWindow(std::uint64_t bytes) {
    importWindow.store(bytes, std::memory_order_relaxed);
}

void SetHostImportRangeWindow(std::uint64_t bytes) {
    rangeImportWindow.store(bytes, std::memory_order_relaxed);
}

void SetHostImportMaxBytes(std::uint64_t bytes) {
    importMaxBytes.store(bytes, std::memory_order_relaxed);
}

void NoteHostImportFrame() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.previousFrameFloor = state.frameFloor;
    state.frameFloor = state.useCounter + 1;
    ++state.framesEnded;
}

std::uint64_t HostImportEpoch() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return state.epoch;
}

std::uint64_t HostImportFramesEnded() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return state.framesEnded;
}

void SetHostImportBudget(std::uint64_t bytes) {
    budgetOverride.store(bytes, std::memory_order_relaxed);
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.ceiling = 0;
    state.allocationFailures = 0;
    state.allocationFailed = false;
    state.reportNoted = 0;
    state.growthDue = state.growthFloor != std::numeric_limits<std::uint64_t>::max();
}

std::uint64_t HostImportBudgetFor(const VkPhysicalDeviceMemoryProperties& memory, const VkPhysicalDeviceMemoryBudgetPropertiesEXT* reported, std::uint64_t configured, std::uint64_t liveBytes) {
    const auto heap = hostHeapIndex(memory);
    if (reported == nullptr || !heap || reported->heapBudget[*heap] == 0) return configured;
    const auto granted = reported->heapBudget[*heap];
    const auto others = reported->heapUsage[*heap] - std::min<std::uint64_t>(reported->heapUsage[*heap], liveBytes);
    const auto held = others + granted / 8u;
    constexpr std::uint64_t step = 64ull << 20u;
    return std::min(configured, granted > held ? (granted - held) / step * step : 0u);
}

bool HostImportsInHeapUsage(VkDriverId driver) {
    return driver == VK_DRIVER_ID_MOLTENVK;
}

VkPhysicalDeviceMemoryBudgetPropertiesEXT UsageWithoutHostImports(const VkPhysicalDeviceMemoryProperties& memory, VkPhysicalDeviceMemoryBudgetPropertiesEXT reported, std::uint64_t liveBytes, bool importsInUsage) {
    if (!importsInUsage) return reported;
    if (const auto heap = hostHeapIndex(memory)) reported.heapUsage[*heap] -= std::min<VkDeviceSize>(reported.heapUsage[*heap], liveBytes);
    return reported;
}

std::uint64_t HostImportBudgetWith(std::uint64_t configured, bool set, std::uint64_t reported, bool allocationFailed, std::uint64_t ceiling) {
    const auto budget = set || !allocationFailed ? configured : std::max<std::uint64_t>(std::min(configured, reported), 1);
    return ceiling != 0 ? std::min(budget, ceiling) : budget;
}

void SetAddressCopyLimit(std::uint64_t bytes) {
    addressCopyLimitOverride.store(bytes, std::memory_order_relaxed);
}

AddressCopyCounters& AddressCopyCounters::ThisThread() {
    auto& lease = HostThreadLocal<AddressCopyLease, AddressCopyLease>();
    if (lease.counters == nullptr) {
        auto& registry = addressCopyRegistry();
        std::lock_guard lock(registry.mutex);
        if (registry.unused.empty()) {
            lease.counters = &registry.counters.emplace_back();
        } else {
            lease.counters = registry.unused.back();
            registry.unused.pop_back();
        }
    }
    return *lease.counters;
}

void AddressCopyCounters::Count(std::uint64_t copied) {
    bytes.store(bytes.load(std::memory_order_relaxed) + copied, std::memory_order_relaxed);
    const auto epoch = addressCopyEpoch.load(std::memory_order_relaxed) & AddressCopyEpochMask;
    const auto kept = largest.load(std::memory_order_relaxed);
    const auto clamped = std::min(copied, AddressCopyLargestMask);
    if ((kept >> AddressCopyEpochShift) != epoch || (kept & AddressCopyLargestMask) < clamped) largest.store(epoch << AddressCopyEpochShift | clamped, std::memory_order_relaxed);
}

AddressCopyCounters::Totals AddressCopyCounters::Fold() {
    auto& registry = addressCopyRegistry();
    std::lock_guard lock(registry.mutex);
    const auto epoch = addressCopyEpoch.load(std::memory_order_relaxed);
    Totals totals;
    for (const auto& counters : registry.counters) {
        totals.bytes += counters.bytes.load(std::memory_order_relaxed);
        const auto kept = counters.largest.load(std::memory_order_relaxed);
        if ((kept >> AddressCopyEpochShift) == (epoch & AddressCopyEpochMask)) totals.largest = std::max(totals.largest, kept & AddressCopyLargestMask);
    }
    addressCopyEpoch.store(epoch + 1, std::memory_order_relaxed);
    return totals;
}

std::uint64_t AddressCopyCounters::Limit() {
    return addressCopyLimit();
}

void SetHostImportDeferredSettles(bool enabled) {
#ifdef _WIN32
    deferredSettles.store(enabled, std::memory_order_relaxed);
#else
    static_cast<void>(enabled);
#endif
}

std::uint64_t HostImportBudget(const VkPhysicalDeviceMemoryProperties& memory, const std::uint64_t physicalBytes) {
    std::uint64_t hostHeap = 0;
    for (std::uint32_t index = 0; index < std::min<std::uint32_t>(memory.memoryHeapCount, VK_MAX_MEMORY_HEAPS); ++index) {
        if ((memory.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0) hostHeap = std::max<std::uint64_t>(hostHeap, memory.memoryHeaps[index].size);
    }
    const auto fromMemory = std::max<std::uint64_t>(6144ull << 20u, physicalBytes / 2);
    const auto reserve = std::max<std::uint64_t>(2048ull << 20u, std::min<std::uint64_t>(hostHeap / 2, 4096ull << 20u));
    return hostHeap > reserve ? std::min(fromMemory, hostHeap - reserve) : fromMemory;
}

std::uint64_t HostImportLiveBytes() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return state.liveBytes;
}

std::uint64_t HostImportBudgetRemaining(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    const auto budget = effectiveBudget(context, state);
    const auto pinned = pinnedTotal(state);
    return budget > pinned ? budget - pinned : 0;
}

const char* ImportRequesterName(ImportRequester requester) {
    switch (requester) {
        case ImportRequester::Map: return "map";
        case ImportRequester::Build: return "build";
        case ImportRequester::Access: return "access";
        case ImportRequester::Join: return "join";
        case ImportRequester::Segment: return "segment";
        case ImportRequester::ColorTarget: return "color target";
    }
    return "unknown";
}

HostImportCounts HostImportCounters() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return {state.imports.size(), state.liveBytes, state.counts, state.growth.size(), state.budgetChecks, retiredPinnedBytes.load(std::memory_order_acquire)};
}

std::string FormatHostImportCounts(const HostImportCounts& counts) {
    HostImportRequesterCounts total;
    std::string requesters;
    for (std::size_t index = 0; index < counts.requesters.size(); ++index) {
        const auto& requested = counts.requesters[index];
        total.made += requested.made;
        total.madeBytes += requested.madeBytes;
        total.refused += requested.refused;
        total.refusedBytes += requested.refusedBytes;
        total.failed += requested.failed;
        total.remembered += requested.remembered;
        char part[96];
        std::snprintf(part, sizeof(part), "%s%s %llu/%llu/%llu", index == 0 ? "" : ", ", ImportRequesterName(static_cast<ImportRequester>(index)), static_cast<unsigned long long>(requested.made), static_cast<unsigned long long>(requested.refused), static_cast<unsigned long long>(requested.failed));
        requesters += part;
    }
    char text[256];
    std::snprintf(text, sizeof(text), "%llu live (%.1f MiB); made %llu (%.1f MiB), refused %llu (%.1f MiB, %llu remembered), failed %llu; by requester (made/refused/failed): ", static_cast<unsigned long long>(counts.live), counts.liveBytes / 1048576.0, static_cast<unsigned long long>(total.made), total.madeBytes / 1048576.0, static_cast<unsigned long long>(total.refused), total.refusedBytes / 1048576.0, static_cast<unsigned long long>(total.remembered), static_cast<unsigned long long>(total.failed));
    if (counts.retiredPinnedBytes != 0) {
        char pinned[64];
        std::snprintf(pinned, sizeof(pinned), "; %.1f MiB retired and still pinned", counts.retiredPinnedBytes / 1048576.0);
        requesters += pinned;
    }
    return text + requesters;
}

void ReportHostImports() {
    const auto counts = HostImportCounters();
    if (std::all_of(counts.requesters.begin(), counts.requesters.end(), [](const HostImportRequesterCounts& requested) { return requested.made == 0 && requested.refused == 0 && requested.failed == 0; })) return;
    std::fprintf(stderr, "[gpu] host imports at shutdown: %s\n", FormatHostImportCounts(counts).c_str());
}

HostImportSettleCounts HostImportSettles() {
#ifdef _WIN32
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return {state.settlesSubmitted, state.settlesWaited, state.settlesRewatched, state.settlesParked, state.settleUnmapWaits, state.settlesKeptCompared};
#else
    return {};
#endif
}

std::uint64_t HostImportsSettled() {
    return settledImports.load(std::memory_order_relaxed);
}

const HostImport* HostImportFor(const Context& context, std::uint64_t address, std::size_t bytes, ImportRequester requester) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return nullptr;
    ensurePinWaiter();
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
#ifdef _WIN32
    settleUnwatchedImports(context, state);
#endif
    if (!importsStale(context, state)) {
        if (recentImportsEnabled()) {
            if (const auto* entry = recentImport(state, address, address + bytes)) {
                entry->lastUse = ++state.useCounter;
                return entry;
            }
        }
        if (const auto* entry = findImport(state, address, address + bytes)) {
            entry->lastUse = ++state.useCounter;
            if (recentImportsEnabled()) rememberImport(state, entry);
            return entry;
        }
    }
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    refreshImports(context, state, lease);
    return importAccess(context, state, address, address + bytes, lease, requester);
}

void RequireGpuLockBeforeImports(const char* where) {
    if (!importsLockOrderChecked || importsHeld == 0 || GuestMemory::GpuMutex().HeldByThisThread()) return;
    std::fprintf(stderr, "[lock] ASSERT: %s takes GuestMemory::GpuMutex while this thread holds the host imports mutex without it\n", where);
    std::abort();
}

bool ImportsHeldByThisThread() {
    return importsHeld != 0;
}

}
}
