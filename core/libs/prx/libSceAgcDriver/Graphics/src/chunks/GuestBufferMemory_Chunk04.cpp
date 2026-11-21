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
ImportRun importRun(const GuestAllocations::Lease& lease, std::uint64_t address, std::uint64_t largest = std::numeric_limits<std::uint64_t>::max(), std::uint64_t budget = 0) {
    const auto bounds = importRunBounds(lease, address, largest, budget);
    return bounds.found ? leasedRun(lease, bounds.first, bounds.last) : ImportRun{};
}

template<typename Entry>
bool sameRun(const Entry& entry, const GuestAllocations::Lease& lease, const RunBounds& bounds) {
    if (!bounds.found || lease[bounds.first]->address != entry.base || lease[bounds.last]->address + lease[bounds.last]->bytes - entry.base != entry.bytes || bounds.last - bounds.first + 1 != entry.ranges.size()) return false;
    for (std::size_t index = 0; index < entry.ranges.size(); ++index) {
        if (entry.ranges[index].owner_before(lease[bounds.first + index]) || lease[bounds.first + index].owner_before(entry.ranges[index])) return false;
    }
    return true;
}

ImportRun exactRun(const GuestAllocations::Lease& lease, std::uint64_t base, std::uint64_t bytes) {
    const auto first = leasedIndexContaining(lease, base);
    if (first == lease.size() || lease[first]->address != base) return {};
    auto last = first;
    while (lease[last]->address + lease[last]->bytes < base + bytes) {
        if (last + 1 == lease.size() || !continuesRun(*lease[last], *lease[last + 1])) return {};
        ++last;
    }
    return lease[last]->address + lease[last]->bytes == base + bytes ? leasedRun(lease, first, last) : ImportRun{};
}

bool runCovers(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto bounds = importRunBounds(lease, begin);
    return bounds.found && end <= lease[bounds.last]->address + lease[bounds.last]->bytes;
}

ImportRun crossedRun(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    auto last = leasedIndexContaining(lease, begin);
    if (last == lease.size()) return {};
    const auto first = last;
    while (lease[last]->address + lease[last]->bytes < end) {
        if (last + 1 == lease.size() || !continuesRun(*lease[last], *lease[last + 1])) return {};
        ++last;
    }
    if (first == last) return {};
    ImportRun run{lease[first]->address, lease[last]->address + lease[last]->bytes - lease[first]->address, {}};
    run.ranges.assign(lease.begin() + static_cast<std::ptrdiff_t>(first), lease.begin() + static_cast<std::ptrdiff_t>(last) + 1);
    return run;
}

ImportRun joinedRun(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto window = importWindow.load(std::memory_order_relaxed);
    if (window == 0) return {};
    auto run = crossedRun(lease, begin, end);
    if (run.ranges.empty()) return run;
    if (std::all_of(run.ranges.begin(), run.ranges.end(), [window](const auto& range) { return range->bytes > window; })) return run;
    if (begin == run.base) return run;
    if (run.ranges.front()->bytes > window && run.bytes - run.ranges.front()->bytes <= window) return run;
    return {};
}

bool joinedCovers(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    return !crossedRun(lease, begin, end).ranges.empty();
}

template<typename Entry>
bool sameRun(const Entry& entry, const ImportRun& run) {
    if (run.base != entry.base || run.bytes != entry.bytes || run.ranges.size() != entry.ranges.size()) return false;
    for (std::size_t index = 0; index < run.ranges.size(); ++index) {
        if (entry.ranges[index].owner_before(run.ranges[index]) || run.ranges[index].owner_before(entry.ranges[index])) return false;
    }
    return true;
}

bool wouldRetireImport(const HostImports& state, const ImportRun& run) {
    if (run.ranges.empty()) return false;
    const auto next = state.imports.lower_bound(run.base);
    if (next != state.imports.end()) {
        if (next->first == run.base && sameRun(next->second, run)) return false;
        if (next->first < run.base + run.bytes) return true;
    }
    if (next == state.imports.begin()) return false;
    const auto& previous = std::prev(next)->second;
    return previous.base + previous.bytes > run.base;
}

bool sameRange(const ImportFailure& entry, const GuestAllocations::Lease& lease, std::uint64_t budget = 0) {
    const auto current = entry.ranges.size() == 1 ? leasedRangeOwner(lease, entry.base, entry.base + entry.bytes) : nullptr;
    if (current != nullptr && entry.bytes < current->bytes) return !entry.ranges.front().owner_before(current) && !current.owner_before(entry.ranges.front());
    return sameRun(entry, lease, importRunBounds(lease, entry.base, std::numeric_limits<std::uint64_t>::max(), budget));
}

bool sameRange(const HostImport& entry, const GuestAllocations::Lease& lease) {
    return std::all_of(entry.ranges.begin(), entry.ranges.end(), [&](const auto& range) {
        const auto current = leasedRangeOwner(lease, range->address);
        return current != nullptr && !range.owner_before(current) && !current.owner_before(range);
    });
}

std::uint64_t effectiveUse(const HostImports& state, const HostImport& entry) {
    if (entry.spaceSerial == 0) return entry.lastUse;
    const auto found = state.spaceUses.find(entry.spaceSerial);
    return found == state.spaceUses.end() ? entry.lastUse : std::max(entry.lastUse, found->second);
}

bool pointerHeld(const HostImports& state, const HostImport& entry) {
    return effectiveUse(state, entry) >= state.buildFloor;
}

constexpr std::uint64_t ProtectedUses = 256;

bool mirrorsEnabled();

bool mirrorStandsIn(const HostImport& entry) {
    return mirrorsEnabled() && !entry.joined && entry.ranges.size() == 1 && entry.range != nullptr && entry.range->releasable && GuestMemory::Watched(entry.range->address, entry.range->bytes);
}

bool hasFrameHistory(const HostImports& state) {
    return state.previousFrameFloor != 0;
}

bool evictable(const HostImports& state, const HostImport& entry) {
    if (pointerHeld(state, entry)) return false;
    if (state.mirrorsYield && mirrorStandsIn(entry)) return true;
    if (!hasFrameHistory(state)) return effectiveUse(state, entry) + ProtectedUses < state.useCounter;
    return effectiveUse(state, entry) < state.previousFrameFloor;
}

struct MirrorsYield {
    HostImports& state;
    std::uint64_t floor;
    MirrorsYield(HostImports& state, std::uint64_t buildFloor) : state(state), floor(state.buildFloor) {
        state.mirrorsYield = true;
        state.buildFloor = buildFloor;
    }
    ~MirrorsYield() {
        state.mirrorsYield = false;
        state.buildFloor = floor;
    }
    MirrorsYield(const MirrorsYield&) = delete;
    MirrorsYield& operator=(const MirrorsYield&) = delete;
};

bool evictingRefusalCurrent(const HostImports& state, const EvictingRefusal& refusal, std::uint64_t budget) {
    return refusal.epoch == state.epoch && refusal.frames == state.framesEnded && refusal.pinnedBytes == pinnedTotal(state) && refusal.immediate == retireFreesAtOnce(true) && budget <= refusal.budget && refusal.window == importWindow.load(std::memory_order_relaxed) && state.useCounter < refusal.ready && (state.buildFloor <= refusal.floor || state.useCounter < refusal.readyHeld);
}

void noteRetiredUnwatched(std::vector<std::pair<std::uint64_t, std::uint64_t>>& spans, const HostImport& entry, const GuestAllocations::Lease& lease) {
    if (!entry.unwatched) return;
    for (const auto& range : entry.ranges) {
        if (leasedRangeOwner(lease, range->address) != range) continue;
        const auto begin = std::max(range->address, entry.base);
        const auto end = std::min(range->address + range->bytes, entry.base + entry.bytes);
        if (begin < end) spans.emplace_back(begin, end);
    }
}

void rewatchRetired(const HostImports& state, std::vector<std::pair<std::uint64_t, std::uint64_t>> spans) {
    std::erase_if(spans, [&](const auto& span) {
        auto it = state.imports.upper_bound(span.first);
        if (it != state.imports.begin()) --it;
        for (; it != state.imports.end() && it->first < span.second; ++it) {
            if (it->second.unwatched && span.first < it->first + it->second.bytes) return true;
        }
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> one{{span}};
        return StorageTexture::AnyLiveOverlaps(one);
    });
    if (!spans.empty()) GuestMemory::Rewatch(spans);
}

std::uint64_t retireLeastRecent(const Context& context, HostImports& state, const GuestAllocations::Lease& lease, std::uint64_t wanted, std::uint64_t begin, std::uint64_t end) {
    std::uint64_t freed = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwatched;
    while (freed < wanted) {
        auto oldest = state.imports.end();
        for (auto it = state.imports.begin(); it != state.imports.end(); ++it) {
            if (it->second.base + it->second.bytes > begin && it->second.base < end) continue;
            if (evictable(state, it->second) && (oldest == state.imports.end() || effectiveUse(state, it->second) < effectiveUse(state, oldest->second))) oldest = it;
        }
        if (oldest == state.imports.end()) break;
        freed += oldest->second.pinnedBytes;
        noteRetiredUnwatched(unwatched, oldest->second, lease);
        retireImport(context.device, state, oldest, lease);
    }
    rewatchRetired(state, std::move(unwatched));
    return freed;
}

std::uint64_t physicalMemoryBytes() {
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
#else
    const auto pages = sysconf(_SC_PHYS_PAGES);
    const auto pageSize = sysconf(_SC_PAGESIZE);
    return pages > 0 && pageSize > 0 ? static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageSize) : 0;
#endif
}

std::atomic<std::uint64_t> budgetOverride{0};
std::atomic<std::uint64_t> addressCopyLimitOverride{0};

std::uint64_t addressCopyLimit() {
    static const std::uint64_t configured = [] {
        const char* value = std::getenv("APS5_ADDRESS_COPY_MAX_MIB");
        return (value ? std::strtoull(value, nullptr, 10) : 64ull) << 20u;
    }();
    const auto limit = addressCopyLimitOverride.load(std::memory_order_relaxed);
    return limit != 0 ? limit : configured;
}

constexpr unsigned AddressCopyEpochShift = 48;
constexpr std::uint64_t AddressCopyEpochMask = 0xffff;
constexpr std::uint64_t AddressCopyLargestMask = (std::uint64_t{1} << AddressCopyEpochShift) - 1;
std::atomic<std::uint64_t> addressCopyEpoch{0};

struct AddressCopyRegistry {
    std::mutex mutex;
    std::deque<AddressCopyCounters> counters;
    std::vector<AddressCopyCounters*> unused;
};

AddressCopyRegistry& addressCopyRegistry() {
    static auto* registry = new AddressCopyRegistry;
    return *registry;
}

struct AddressCopyLease {
    AddressCopyCounters* counters = nullptr;
    ~AddressCopyLease() {
        if (counters == nullptr) return;
        auto& registry = addressCopyRegistry();
        std::lock_guard lock(registry.mutex);
        registry.unused.push_back(counters);
    }
};

std::uint64_t configuredBudget() {
    if (const auto set = budgetOverride.load(std::memory_order_relaxed); set != 0) return set;
    static const std::uint64_t configured = []() -> std::uint64_t {
        const char* value = std::getenv("APS5_HOST_IMPORT_MIB");
        return value != nullptr ? std::strtoull(value, nullptr, 10) << 20u : 0u;
    }();
    return configured;
}

std::uint64_t importBudget(const Context& context) {
    if (const auto configured = configuredBudget(); configured != 0) return configured;
    return HostImportBudget(context.memory, physicalMemoryBytes());
}

std::optional<std::uint32_t> hostHeapIndex(const VkPhysicalDeviceMemoryProperties& memory) {
    std::optional<std::uint32_t> heap;
    for (std::uint32_t index = 0; index < memory.memoryHeapCount; ++index) {
        if ((memory.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0 && (!heap || memory.memoryHeaps[index].size > memory.memoryHeaps[*heap].size)) heap = index;
    }
    if (heap) return heap;
    constexpr VkMemoryPropertyFlags shared = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (std::uint32_t index = 0; index < std::min<std::uint32_t>(memory.memoryTypeCount, VK_MAX_MEMORY_TYPES); ++index) {
        const auto candidate = memory.memoryTypes[index].heapIndex;
        if ((memory.memoryTypes[index].propertyFlags & shared) == shared && candidate < memory.memoryHeapCount && (!heap || memory.memoryHeaps[candidate].size > memory.memoryHeaps[*heap].size)) heap = candidate;
    }
    return heap;
}

void noteReportedBudget(HostImports& state, std::uint64_t configured, bool set, std::uint64_t reported) {
    if (reported >= configured || state.reportedHeapBudget == 0) return;
    const std::uint8_t stage = !set && state.allocationFailed ? 2 : 1;
    if (state.reportNoted >= stage) return;
    state.reportNoted = stage;
    const char* outcome = set ? "the setting stays, as the report never lowers it" : stage == 2 ? "imports are held to it since an import's vkAllocateMemory failed" : "the default stays until an import's vkAllocateMemory fails";
    std::fprintf(stderr, "[gpu] host import budget: host heap %u reports a VK_EXT_memory_budget budget of %llu MiB and a usage of %llu MiB with %llu MiB of imports live, which leaves imports %llu MiB of the %llu MiB %s; %s\n", state.reportedHeap, static_cast<unsigned long long>(state.reportedHeapBudget >> 20u), static_cast<unsigned long long>(state.reportedHeapUsage >> 20u), static_cast<unsigned long long>(state.liveBytes >> 20u), static_cast<unsigned long long>(reported >> 20u), static_cast<unsigned long long>(configured >> 20u), set ? "APS5_HOST_IMPORT_MIB sets" : "of the default", outcome);
}

std::uint64_t effectiveBudget(const Context& context, HostImports& state) {
    ++state.budgetChecks;
    const auto configured = importBudget(context);
    const bool set = configuredBudget() != 0;
    auto reported = std::numeric_limits<std::uint64_t>::max();
    if (context.memoryProperties2 != nullptr && context.physical != VK_NULL_HANDLE) {
        const auto now = std::chrono::steady_clock::now();
        if (state.reportedDevice != context.physical || now - state.reportedAt >= std::chrono::seconds(1)) {
            VkPhysicalDeviceMemoryBudgetPropertiesEXT report{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
            VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &report};
            context.memoryProperties2(context.physical, &properties);
            state.reportedBudget = HostImportBudgetFor(context.memory, &report, std::numeric_limits<std::uint64_t>::max(), pinnedTotal(state));
            const auto heap = hostHeapIndex(context.memory);
            state.reportedHeap = heap.value_or(0);
            state.reportedHeapBudget = heap ? report.heapBudget[*heap] : 0;
            state.reportedHeapUsage = heap ? report.heapUsage[*heap] : 0;
            state.reportedDevice = context.physical;
            state.reportedAt = now;
        }
        reported = state.reportedBudget;
    }
    noteReportedBudget(state, configured, set, reported);
    const auto budget = HostImportBudgetWith(configured, set, reported, state.allocationFailed, state.ceiling);
    if (budget > state.growthFloor) state.growthDue = true;
    return budget;
}

std::string budgetSource(const Context& context, const HostImports& state, std::uint64_t budget) {
    const auto configured = importBudget(context);
    const char* source = state.ceiling != 0 && budget == state.ceiling ? "lowered after vkAllocateMemory failed" : budget < configured ? "the host heap's VK_EXT_memory_budget report" : configuredBudget() != 0 ? "APS5_HOST_IMPORT_MIB" : "the default";
    if (state.reportedHeapBudget == 0) return source;
    char text[192];
    std::snprintf(text, sizeof(text), "%s; host heap %u reports a budget of %llu MiB and a usage of %llu MiB", source, state.reportedHeap, static_cast<unsigned long long>(state.reportedHeapBudget >> 20u), static_cast<unsigned long long>(state.reportedHeapUsage >> 20u));
    return text;
}

std::atomic<std::uint64_t> rangeImportWindow{64ull << 20u};
std::atomic<std::uint64_t> importMaxBytes{0};

std::optional<std::pair<std::uint64_t, std::uint64_t>> importWindowBounds(const GuestAllocations::Range& range, std::uint64_t begin, std::uint64_t end, std::uint64_t alignment) {
    const auto window = rangeImportWindow.load(std::memory_order_relaxed);
    if (window == 0 || alignment == 0 || window % alignment != 0 || range.bytes <= window || begin < range.address || end > range.address + range.bytes || end <= begin) return std::nullopt;
    const auto lastCell = range.bytes / window - 1;
    const auto cell = [&](std::uint64_t address) { return std::min((address - range.address) / window, lastCell); };
    const auto windowBegin = range.address + cell(begin) * window;
    const auto endCell = cell(end - 1);
    const auto windowEnd = endCell == lastCell ? range.address + range.bytes : range.address + (endCell + 1) * window;
    if (windowBegin % alignment != 0 || (windowEnd - windowBegin) % alignment != 0) return std::nullopt;
    return std::pair{windowBegin, windowEnd};
}

ImportRun windowRun(const HostImports& state, ImportRun run, std::uint64_t begin, std::uint64_t end, std::uint64_t alignment) {
    if (run.ranges.size() != 1) return run;
    const auto& range = *run.ranges.front();
    const auto bounds = importWindowBounds(range, begin, end, alignment);
    if (!bounds) return run;
    auto [windowBegin, windowEnd] = *bounds;
    for (bool grown = true; grown;) {
        grown = false;
        auto it = state.imports.upper_bound(windowBegin);
        if (it != state.imports.begin()) --it;
        for (; it != state.imports.end() && it->first < windowEnd; ++it) {
            const auto importEnd = it->second.base + it->second.bytes;
            if (importEnd <= windowBegin || it->second.base < range.address || importEnd > range.address + range.bytes) continue;
            if (it->second.base < windowBegin || importEnd > windowEnd) grown = true;
            windowBegin = std::min(windowBegin, it->second.base);
            windowEnd = std::max(windowEnd, importEnd);
        }
    }
    if (windowBegin % alignment != 0 || (windowEnd - windowBegin) % alignment != 0) return run;
    const auto committed = GuestMemory::DescribeCommitted(windowBegin, windowEnd - windowBegin);
    if (!committed.whole) {
        const auto part = std::find_if(committed.ranges.begin(), committed.ranges.end(), [&](const auto& span) { return span.first <= begin && span.second >= end; });
        if (part == committed.ranges.end()) return run;
        const auto clippedBegin = part->first + (alignment - part->first % alignment) % alignment;
        const auto clippedEnd = part->second - part->second % alignment;
        if (clippedBegin > begin || clippedEnd < end) return run;
        auto it = state.imports.upper_bound(clippedBegin);
        if (it != state.imports.begin()) --it;
        for (; it != state.imports.end() && it->first < clippedEnd; ++it) {
            const auto importEnd = it->second.base + it->second.bytes;
            if (importEnd > clippedBegin && (it->second.base < clippedBegin || importEnd > clippedEnd)) return run;
        }
        windowBegin = clippedBegin;
        windowEnd = clippedEnd;
    }
    run.base = windowBegin;
    run.bytes = windowEnd - windowBegin;
    return run;
}

const HostImport* peekImport(const HostImports& state, std::uint64_t begin, std::uint64_t end);
const HostImport* findImport(HostImports& state, std::uint64_t begin, std::uint64_t end);
const HostImport* importRunAt(const Context& context, HostImports& state, ImportRun run, bool joined, std::uint64_t address, const GuestAllocations::Lease& lease, std::uint64_t end, bool evict, ImportRequester requester);

std::optional<std::pair<std::uint64_t, std::uint64_t>> desiredRun(const GuestAllocations::Lease& lease, std::uint64_t address, std::uint64_t alignment) {
    const auto limit = importMaxBytes.load(std::memory_order_relaxed);
    const auto bounds = importRunBounds(lease, address, limit != 0 ? limit : std::numeric_limits<std::uint64_t>::max());
    if (!bounds.found || bounds.first == bounds.last) return std::nullopt;
    const auto begin = lease[bounds.first]->address;
    const auto end = lease[bounds.last]->address + lease[bounds.last]->bytes;
    if (alignment == 0 || begin % alignment != 0 || (end - begin) % alignment != 0) return std::nullopt;
    return std::pair{begin, end};
}

std::map<std::uint64_t, GrowthRun>::iterator growthAt(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    auto it = state.growth.lower_bound(begin);
    if (it != state.growth.begin() && std::prev(it)->second.end > begin) --it;
    while (it != state.growth.end() && it->first < end) {
        if (it->first == begin && it->second.end == end) return it;
        it = state.growth.erase(it);
    }
    return state.growth.emplace_hint(it, begin, GrowthRun{end});
}

void oweGrowth(const Context& context, HostImports& state, const GuestAllocations::Lease& lease, std::uint64_t base, std::uint64_t bytes, GrowthBlock blocked, std::uint64_t budget) {
    const auto desired = desiredRun(lease, base, context.hostImportAlignment);
    if (!desired || desired->first != base || desired->second != base + bytes) return;
    auto& run = growthAt(state, base, base + bytes)->second;
    run.blocked = blocked;
    run.budget = budget;
    run.pinnedBytes = pinnedTotal(state);
    if (blocked == GrowthBlock::Budget) state.growthFloor = std::min(state.growthFloor, budget);
}

void resetGrowthFloor(HostImports& state) {
    state.growthFloor = std::numeric_limits<std::uint64_t>::max();
    for (const auto& [begin, run] : state.growth) {
        if (run.blocked == GrowthBlock::Budget) state.growthFloor = std::min(state.growthFloor, run.budget);
    }
}

void coverGrowth(HostImports& state, std::uint64_t base, std::uint64_t bytes) {
    const auto first = state.growth.lower_bound(base);
    auto last = first;
    while (last != state.growth.end() && last->second.end <= base + bytes) ++last;
    if (first == last) return;
    state.growth.erase(first, last);
    resetGrowthFloor(state);
}

std::uint64_t evictableBytes(const HostImports& state, std::uint64_t begin, std::uint64_t end, std::uint64_t& ready, std::uint64_t& readyHeld) {
    std::uint64_t bytes = 0;
    for (const auto& [base, entry] : state.imports) {
        if (entry.base + entry.bytes > begin && entry.base < end) continue;
        if (evictable(state, entry)) {
            bytes += entry.pinnedBytes;
            continue;
        }
        if (!hasFrameHistory(state)) {
            auto& horizon = pointerHeld(state, entry) ? readyHeld : ready;
            horizon = std::min(horizon, effectiveUse(state, entry) + ProtectedUses + 1);
        } else if (pointerHeld(state, entry) && effectiveUse(state, entry) < state.previousFrameFloor) readyHeld = 0;
    }
    return bytes;
}

struct ImportConstruction {
    ImportConstruction(const Context& context, std::uint64_t base, std::uint64_t bytes)
        : device(context.device), destroyBuffer(context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")), freeMemory(context.Function<PFN_vkFreeMemory>("vkFreeMemory")), entry{base, bytes, VK_NULL_HANDLE, VK_NULL_HANDLE, 0} {}
    ImportConstruction(const ImportConstruction&) = delete;
    ImportConstruction& operator=(const ImportConstruction&) = delete;
    ~ImportConstruction() {
        if (!armed) return;
        if (entry.buffer != VK_NULL_HANDLE) destroyBuffer(device, entry.buffer, nullptr);
        if (entry.memory != VK_NULL_HANDLE) MemoryAccounting::Free(freeMemory, device, entry.memory);
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
    }
    VkDevice device;
    PFN_vkDestroyBuffer destroyBuffer;
    PFN_vkFreeMemory freeMemory;
    HostImport entry;
    bool armed = true;
};

void ensurePinWaiter();

const HostImport* importAllocation(const Context& context, HostImports& state, std::uint64_t address, const GuestAllocations::Lease& lease, ImportRequester requester, std::uint64_t end = 0, bool evict = true, std::uint64_t largest = std::numeric_limits<std::uint64_t>::max()) {
    ensurePinWaiter();
#ifdef _WIN32
    settleUnwatchedImports(context, state);
#endif
    const bool fresh = !importsStale(context, state);
    if (fresh && end != 0) {
        if (const auto* current = findImport(state, address, end)) return current;
    } else if (!fresh) {
        if (const auto* current = peekImport(state, address, address + 1); current != nullptr && current->joined && sameRange(*current, lease)) {
            current->lastUse = ++state.useCounter;
            return current;
        }
    }
    const auto budget = evict ? 0 : effectiveBudget(context, state);
    const auto index = leasedIndexContaining(lease, address);
    const auto windowBounds = index != lease.size() && end != 0 ? importWindowBounds(*lease[index], address, end, context.hostImportAlignment) : std::nullopt;
    const bool complete = !windowBounds || (windowBounds->first == lease[index]->address && windowBounds->second == lease[index]->address + lease[index]->bytes);
    if (budget != 0 && index != lease.size() && complete && stillRefused(state, rememberedRun(state.refusals, lease, index, index), budget) && findImport(state, address, address + 1) == nullptr) {
        auto& counts = state.counts[static_cast<std::size_t>(requester)];
        ++counts.refused;
        counts.refusedBytes += lease[index]->bytes;
        ++counts.remembered;
        ++state.budgetRefusals;
        return nullptr;
    }
    const auto bounds = importRunBounds(lease, address, largest, budget);
    if (fresh && end == 0) {
        if (const auto* current = findImport(state, address, bounds.found ? lease[bounds.last]->address + lease[bounds.last]->bytes : address + 1)) return current;
    }
    auto run = bounds.found ? leasedRun(lease, bounds.first, bounds.last) : ImportRun{};
    if (end != 0) run = windowRun(state, std::move(run), address, end, context.hostImportAlignment);
    if (run.ranges.empty()) return nullptr;
    return importRunAt(context, state, std::move(run), false, address, lease, end, evict, requester);
}

struct ImportPiece {
    std::uint64_t begin;
    std::uint64_t end;
    const HostImport* entry;
};

}
}
