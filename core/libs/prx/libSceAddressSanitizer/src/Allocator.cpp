#include "prx/libSceAddressSanitizer/src/Allocator.hpp"
#include "prx/libSceAddressSanitizer/src/BackingMemory.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowRequire.hpp"
#include "prx/libSceAddressSanitizer/src/Container.hpp"
#include "prx/libSceAddressSanitizer/src/Describe.hpp"
#include "prx/libSceAddressSanitizer/src/ErrorReport.hpp"
#include "prx/libSceAddressSanitizer/src/Report.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowAddressSpace.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <utility>

namespace AsanRuntime::Heap {

namespace {

constexpr std::size_t kMinRedzone = 16;
constexpr std::size_t kMinAlignment = 8;
constexpr std::uint64_t kMaxAllowedMallocSize = 1ull << 40;
constexpr std::uint64_t kQuarantineBytes = 256ull << 20;
constexpr std::size_t kMaxPrimarySize = 1ull << 17;
constexpr std::size_t kMaxMallocFillSize = 0x1000;
constexpr std::uint8_t kMallocFillByte = 0xbe;
constexpr std::uint8_t kHeapLeftRedzoneMagic = 0xfa;
constexpr std::uint8_t kHeapFreeMagic = 0xfd;
constexpr std::size_t kPageSize = 4096;
constexpr std::size_t kLookbehind = kPageSize - 1;

struct Chunk {
    std::uintptr_t raw;
    std::size_t total;
    std::size_t alignment;
    std::uintptr_t user;
    std::size_t size;
    bool quarantined;
    std::uintptr_t allocPc;
    std::uintptr_t freePc;
    std::uint32_t allocTid;
    std::uint32_t freeTid;
};

struct State {
    std::recursive_mutex mutex;
    std::map<std::uintptr_t, Chunk> chunks;
    std::deque<std::uintptr_t> quarantine;
    std::uint64_t quarantineBytes = 0;
    Stats stats{};
    std::uint64_t maxMalloced = 0;
};

State& GetState() {
    static State state;
    return state;
}

std::uint32_t NextTid() {
    static std::atomic<std::uint32_t> next{0};
    return next.fetch_add(1, std::memory_order_relaxed);
}

std::size_t RoundUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

bool IsPowerOfTwo(std::size_t value) {
    return (value & (value - 1)) == 0;
}

std::size_t RedzoneSize(std::size_t size) {
    std::size_t log = size <= 64 - 16            ? 0
                      : size <= 128 - 32         ? 1
                      : size <= 512 - 64         ? 2
                      : size <= 4096 - 128       ? 3
                      : size <= (1 << 14) - 256  ? 4
                      : size <= (1 << 15) - 512  ? 5
                      : size <= (1 << 16) - 1024 ? 6
                                                 : 7;
    return kMinRedzone << log;
}

std::size_t SizeClassId(std::size_t size) {
    constexpr std::size_t kMinSizeLog = 4;
    constexpr std::size_t kMidSizeLog = 8;
    constexpr std::size_t kMidSize = 1 << kMidSizeLog;
    constexpr std::size_t kMinSize = 1 << kMinSizeLog;
    constexpr std::size_t kMidClass = kMidSize / kMinSize;
    constexpr std::size_t kS = 2;
    constexpr std::size_t kM = (1 << kS) - 1;
    if (size > kMaxPrimarySize) {
        return 0;
    }
    if (size <= kMidSize) {
        return (size + kMinSize - 1) >> kMinSizeLog;
    }
    std::size_t l = 0;
    for (std::size_t v = size; v > 1; v >>= 1) {
        ++l;
    }
    const std::size_t hbits = (size >> (l - kS)) & kM;
    const std::size_t lbits = size & ((std::size_t{1} << (l - kS)) - 1);
    return kMidClass + ((l - kMidSizeLog) << kS) + hbits + (lbits > 0);
}

Chunk* Locate(State& state, std::uintptr_t address) {
    auto it = state.chunks.upper_bound(address);
    if (it == state.chunks.begin()) {
        return nullptr;
    }
    --it;
    Chunk& chunk = it->second;
    return address < chunk.raw + chunk.total ? &chunk : nullptr;
}

ChunkView ViewOf(const Chunk& chunk) {
    return {chunk.user, chunk.size, chunk.quarantined, chunk.allocPc, chunk.freePc, chunk.allocTid, chunk.quarantined ? chunk.freeTid : std::uint32_t{0xffffffff}};
}

std::string FormatCount(std::uint64_t value) {
    return std::to_string(static_cast<std::int64_t>(value));
}

[[noreturn]] void Fail(const char* bug, std::uintptr_t address, std::uintptr_t pc, std::string text) {
    text += "SUMMARY: AddressSanitizer: ";
    text += bug;
    text += "\n";
    ReportFatal_nid_no_patch({false, pc, 0, 0, address, false, 0, bug, true}, text);
}

std::string Header(const std::string& message) {
    std::string text = ReportSeparator_nid_no_patch();
    text += "ERROR: AddressSanitizer: ";
    text += message;
    return text;
}

std::string Pointer(std::uintptr_t value) {
    std::string out;
    AppendPointer_nid_no_patch(out, value);
    return out;
}

std::string Thread(std::uint32_t tid) {
    return "T" + std::to_string(tid);
}

constexpr const char* kHint = "HINT: if you don't care about these errors you may set allocator_may_return_null=1\n";

[[noreturn]] void ReportDoubleFree(std::uintptr_t address, std::uintptr_t pc) {
    std::string text = Header("attempting double-free on " + Pointer(address) + " in thread " + Thread(CurrentTid_nid_no_patch()) + ":\n");
    text += DescribeAddress_nid_no_patch(address, 1);
    Fail("double-free", address, pc, std::move(text));
}

[[noreturn]] void ReportFreeNotMalloced(std::uintptr_t address, std::uintptr_t pc) {
    std::string text = Header("attempting free on address which was not malloc()-ed: " + Pointer(address) + " in thread " + Thread(CurrentTid_nid_no_patch()) + "\n");
    text += DescribeAddress_nid_no_patch(address, 1);
    Fail("bad-free", address, pc, std::move(text));
}

[[noreturn]] void ReportNotOwned(const char* function, const char* bug, std::uintptr_t address, std::uintptr_t pc) {
    std::string text = Header(std::string("attempting to call ") + function + "() for pointer which is not owned: " + Pointer(address) + "\n");
    text += DescribeAddress_nid_no_patch(address, 1);
    Fail(bug, address, pc, std::move(text));
}

[[noreturn]] void ReportParameters(const char* what, const char* bug, std::uintptr_t pc, std::string message) {
    std::string text = Header(what + std::string(" ") + message + " (thread " + Thread(CurrentTid_nid_no_patch()) + ")\n");
    text += kHint;
    Fail(bug, 0, pc, std::move(text));
}

[[noreturn]] void ReportTooBig(std::size_t userSize, std::size_t totalSize, std::uintptr_t pc) {
    char buffer[200];
    std::snprintf(buffer, sizeof(buffer), "requested allocation size 0x%zx (0x%zx after adjustments for alignment, red zones etc.) exceeds maximum supported size of 0x%llx (thread %s)\n", userSize, totalSize, static_cast<unsigned long long>(kMaxAllowedMallocSize), Thread(CurrentTid_nid_no_patch()).c_str());
    std::string text = Header(buffer);
    text += kHint;
    Fail("allocation-size-too-big", 0, pc, std::move(text));
}

[[noreturn]] void ReportOutOfMemory(std::size_t size, std::uintptr_t pc) {
    std::string text = Header("out of memory: allocator is trying to allocate 0x");
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%zx bytes\n", size);
    text += buffer;
    text += kHint;
    Fail("out-of-memory", 0, pc, std::move(text));
}

void Recycle(State& state, std::uintptr_t raw) {
    auto it = state.chunks.find(raw);
    const Chunk chunk = it->second;
    state.chunks.erase(it);
    PoisonShadow_nid_no_patch(chunk.raw, chunk.total, 0);
    state.stats.realFrees++;
    state.stats.reallyFreed += chunk.size;
    state.stats.munmaps++;
    state.stats.munmaped += chunk.total;
    FreeBacking_nid_no_patch(reinterpret_cast<void*>(chunk.raw), chunk.alignment);
}

void Quarantine(State& state, std::uintptr_t raw, std::size_t size) {
    state.quarantine.push_back(raw);
    state.quarantineBytes += size;
    while (state.quarantineBytes > kQuarantineBytes && !state.quarantine.empty()) {
        const std::uintptr_t oldest = state.quarantine.front();
        state.quarantine.pop_front();
        state.quarantineBytes -= state.chunks.find(oldest)->second.size;
        Recycle(state, oldest);
    }
}

void* AllocateLocked(State& state, std::size_t size, std::size_t alignment, std::uintptr_t pc, bool fill) {
    if (alignment < kMinAlignment) {
        alignment = kMinAlignment;
    }
    if (size == 0) {
        size = 1;
    }
    const std::size_t redzone = RedzoneSize(size);
    std::uint64_t needed = RoundUp(std::max<std::size_t>(size, 16), alignment) + redzone;
    if (alignment > kMinAlignment) {
        needed += alignment;
    }
    if (!(needed <= kMaxPrimarySize && alignment <= kMaxPrimarySize)) {
        needed += redzone;
    }
    if (size > kMaxAllowedMallocSize || needed > kMaxAllowedMallocSize) {
        ReportTooBig(size, static_cast<std::size_t>(needed), pc);
    }
    const std::size_t left = RoundUp(redzone, alignment);
    const std::size_t body = RoundUp(size, kGranularity);
    const std::size_t total = left + body + redzone;
    void* block = AllocateBacking_nid_no_patch(total, alignment);
    if (!block) {
        ReportOutOfMemory(size, pc);
    }
    const auto raw = reinterpret_cast<std::uintptr_t>(block);
    try {
        RequireShadowRange_nid_no_patch(raw, total, __func__);
    } catch (...) {
        FreeBacking_nid_no_patch(block, alignment);
        throw;
    }
    const std::uintptr_t user = raw + left;
    PoisonShadow_nid_no_patch(raw, left, kHeapLeftRedzoneMagic);
    PoisonShadow_nid_no_patch(user, size & ~(kGranularity - 1), 0);
    if (size & (kGranularity - 1)) {
        *ShadowByte_nid_no_patch(user + (size & ~(kGranularity - 1))) = static_cast<std::uint8_t>(size & (kGranularity - 1));
    }
    PoisonShadow_nid_no_patch(user + body, total - left - body, kHeapLeftRedzoneMagic);
    if (fill) {
        std::memset(reinterpret_cast<void*>(user), kMallocFillByte, std::min(size, kMaxMallocFillSize));
    }
    state.chunks[raw] = {raw, total, alignment, user, size, false, pc, 0, CurrentTid_nid_no_patch(), 0};
    state.stats.mallocs++;
    state.stats.malloced += size;
    state.stats.mallocedRedzones += total - size;
    state.stats.mmaps++;
    state.stats.mmaped += total;
    state.maxMalloced = std::max(state.maxMalloced, state.stats.malloced);
    if (total > kMaxPrimarySize) {
        state.stats.mallocLarge++;
    } else {
        state.stats.mallocedBySize[SizeClassId(total)]++;
    }
    return reinterpret_cast<void*>(user);
}

void DeallocateLocked(State& state, void* pointer, std::uintptr_t pc) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    Chunk* chunk = Locate(state, address);
    if (!chunk || chunk->user != address) {
        ReportFreeNotMalloced(address, pc);
    }
    if (chunk->quarantined) {
        ReportDoubleFree(address, pc);
    }
    chunk->quarantined = true;
    chunk->freePc = pc;
    chunk->freeTid = CurrentTid_nid_no_patch();
    state.stats.frees++;
    state.stats.freed += chunk->size;
    PoisonShadow_nid_no_patch(chunk->user, RoundUp(chunk->size, kGranularity), kHeapFreeMagic);
    Quarantine(state, chunk->raw, chunk->size);
}

bool CallocOverflows(std::size_t count, std::size_t size) {
    if (size == 0 || count == 0) {
        return false;
    }
    return (std::numeric_limits<std::size_t>::max() / size) < count;
}

}  // namespace

std::uint32_t CurrentTid_nid_no_patch() {
    static thread_local std::uint32_t tid = NextTid();
    return tid;
}

void* Allocate_nid_no_patch(std::size_t size, std::size_t alignment, std::uintptr_t pc, bool fill) {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    return AllocateLocked(state, size, alignment, pc, fill);
}

void Deallocate_nid_no_patch(void* pointer, std::uintptr_t pc) {
    if (!pointer) {
        return;
    }
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    DeallocateLocked(state, pointer, pc);
}

void* Calloc_nid_no_patch(std::size_t count, std::size_t size, std::uintptr_t pc) {
    if (CallocOverflows(count, size)) {
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), "(%s * %s) cannot be represented in type size_t", FormatCount(count).c_str(), FormatCount(size).c_str());
        ReportParameters("calloc parameters overflow: count * size", "calloc-overflow", pc, buffer);
    }
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    void* pointer = AllocateLocked(state, count * size, kMinAlignment, pc, false);
    std::memset(pointer, 0, count * size);
    return pointer;
}

void* Reallocate_nid_no_patch(void* pointer, std::size_t size, std::uintptr_t pc) {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    if (!pointer) {
        return AllocateLocked(state, size, kMinAlignment, pc, true);
    }
    if (size == 0) {
        DeallocateLocked(state, pointer, pc);
        return nullptr;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    Chunk* chunk = Locate(state, address);
    if (!chunk || chunk->user != address) {
        ReportFreeNotMalloced(address, pc);
    }
    if (chunk->quarantined) {
        ReportDoubleFree(address, pc);
    }
    state.stats.reallocs++;
    state.stats.realloced += size;
    const std::size_t oldSize = chunk->size;
    void* grown = AllocateLocked(state, size, kMinAlignment, pc, true);
    std::memcpy(grown, pointer, std::min(size, oldSize));
    DeallocateLocked(state, pointer, pc);
    return grown;
}

void* Memalign_nid_no_patch(std::size_t alignment, std::size_t size, std::uintptr_t pc) {
    if (!IsPowerOfTwo(alignment)) {
        ReportParameters("invalid allocation alignment:", "invalid-allocation-alignment", pc, std::to_string(static_cast<std::int64_t>(alignment)) + ", alignment must be a power of two");
    }
    return Allocate_nid_no_patch(size, alignment, pc, true);
}

void* AlignedAlloc_nid_no_patch(std::size_t alignment, std::size_t size, std::uintptr_t pc) {
    if (alignment == 0 || !IsPowerOfTwo(alignment) || (size & (alignment - 1)) != 0) {
        char buffer[200];
        std::snprintf(buffer, sizeof(buffer), "%zd, alignment must be a power of two and the requested size 0x%zx must be a multiple of alignment", static_cast<std::ptrdiff_t>(alignment), size);
        ReportParameters("invalid alignment requested in aligned_alloc:", "invalid-aligned-alloc-alignment", pc, buffer);
    }
    return Allocate_nid_no_patch(size, alignment, pc, true);
}

int PosixMemalign_nid_no_patch(void** memptr, std::size_t alignment, std::size_t size, std::uintptr_t pc) {
    if (alignment == 0 || !IsPowerOfTwo(alignment) || (alignment % sizeof(void*)) != 0) {
        char buffer[200];
        std::snprintf(buffer, sizeof(buffer), "%zd, alignment must be a power of two and a multiple of sizeof(void*) == %zd", static_cast<std::ptrdiff_t>(alignment), sizeof(void*));
        ReportParameters("invalid alignment requested in posix_memalign:", "invalid-posix-memalign-alignment", pc, buffer);
    }
    *memptr = Allocate_nid_no_patch(size, alignment, pc, true);
    return 0;
}

std::size_t AllocationSize_nid_no_patch(std::uintptr_t address) {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    const Chunk* chunk = Locate(state, address);
    if (!chunk || chunk->quarantined || chunk->user != address) {
        return 0;
    }
    return chunk->size;
}

std::size_t UsableSize_nid_no_patch(const void* pointer, std::uintptr_t pc) {
    if (!pointer) {
        return 0;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const std::size_t size = AllocationSize_nid_no_patch(address);
    if (size == 0) {
        ReportNotOwned("malloc_usable_size", "bad-malloc_usable_size", address, pc);
    }
    return size;
}

std::size_t AllocatedSizeChecked_nid_no_patch(const void* pointer, std::uintptr_t pc) {
    if (!pointer) {
        return 0;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const std::size_t size = AllocationSize_nid_no_patch(address);
    if (size == 0) {
        ReportNotOwned("__sanitizer_get_allocated_size", "bad-__sanitizer_get_allocated_size", address, pc);
    }
    return size;
}

const void* AllocationBegin_nid_no_patch(const void* pointer) {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    const Chunk* chunk = Locate(state, reinterpret_cast<std::uintptr_t>(pointer));
    if (!chunk || chunk->quarantined) {
        return nullptr;
    }
    return reinterpret_cast<const void*>(chunk->user);
}

bool UpdateContext_nid_no_patch(std::uintptr_t address, std::uintptr_t pc) {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    Chunk* chunk = Locate(state, address);
    if (!chunk || chunk->quarantined || chunk->user != address) {
        return false;
    }
    chunk->allocPc = pc;
    chunk->allocTid = CurrentTid_nid_no_patch();
    return true;
}

Access Classify_nid_no_patch(const ChunkView& view, std::uintptr_t address, std::size_t accessSize, std::intptr_t& offset) {
    const std::uintptr_t end = view.begin + view.size;
    if (address < view.begin) {
        offset = static_cast<std::intptr_t>(view.begin - address);
        return Access::Before;
    }
    if (address + accessSize > end) {
        offset = static_cast<std::intptr_t>(address - end);
        return Access::After;
    }
    offset = static_cast<std::intptr_t>(address - view.begin);
    return Access::Inside;
}

bool FindChunk_nid_no_patch(std::uintptr_t address, ChunkView& view) {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    const Chunk* first = Locate(state, address);
    std::intptr_t offset = 0;
    if (first && Classify_nid_no_patch(ViewOf(*first), address, 1, offset) != Access::Before) {
        view = ViewOf(*first);
        return true;
    }
    const Chunk* second = nullptr;
    auto it = state.chunks.lower_bound(first ? first->raw : address);
    if (it != state.chunks.begin()) {
        --it;
        const Chunk& candidate = it->second;
        if (address - (candidate.raw + candidate.total - 1) <= kLookbehind) {
            second = &candidate;
        }
    }
    const Chunk* pick = first;
    if (second && Classify_nid_no_patch(ViewOf(*second), address, 1, offset) == Access::After) {
        if (!first) {
            pick = second;
        } else if (first->quarantined != second->quarantined) {
            pick = first->quarantined ? second : first;
        } else {
            std::intptr_t leftOffset = 0;
            std::intptr_t rightOffset = 0;
            Classify_nid_no_patch(ViewOf(*second), address, 1, leftOffset);
            Classify_nid_no_patch(ViewOf(*first), address, 1, rightOffset);
            pick = leftOffset < rightOffset ? second : first;
        }
    }
    if (!pick) {
        return false;
    }
    view = ViewOf(*pick);
    return true;
}

std::string DescribeChunk_nid_no_patch(std::uintptr_t address, std::size_t accessSize) {
    ChunkView view{};
    if (!FindChunk_nid_no_patch(address, view)) {
        return "AddressSanitizer can not describe address in more detail (wild memory access suspected).\n";
    }
    std::intptr_t offset = 0;
    std::uintptr_t reported = address;
    Access access = Classify_nid_no_patch(view, address, accessSize, offset);
    if (access == Access::After && offset < 0) {
        reported -= static_cast<std::uintptr_t>(offset);
        offset = 0;
    }
    std::string text = Pointer(reported);
    text += " is located " + std::to_string(offset) + " bytes ";
    text += access == Access::Before ? "before" : access == Access::After ? "after" : "inside of";
    text += " " + std::to_string(view.size) + "-byte region [" + Pointer(view.begin) + "," + Pointer(view.begin + view.size) + ")\n";
    if (view.quarantined) {
        text += "freed by thread " + Thread(view.freeTid) + " here:\n    #0 " + Pointer(view.freePc) + "\n";
        text += "previously allocated by thread " + Thread(view.allocTid) + " here:\n";
    } else {
        text += "allocated by thread " + Thread(view.allocTid) + " here:\n";
    }
    text += "    #0 " + Pointer(view.allocPc) + "\n";
    return text;
}

void Purge_nid_no_patch() {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    while (!state.quarantine.empty()) {
        const std::uintptr_t oldest = state.quarantine.front();
        state.quarantine.pop_front();
        state.quarantineBytes -= state.chunks.find(oldest)->second.size;
        Recycle(state, oldest);
    }
}

Stats GetStats_nid_no_patch() {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    return state.stats;
}

std::uint64_t MaxMalloced_nid_no_patch() {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    return state.maxMalloced;
}

std::size_t CurrentAllocatedBytes_nid_no_patch() {
    const Stats stats = GetStats_nid_no_patch();
    return stats.malloced > stats.freed ? stats.malloced - stats.freed : 0;
}

std::size_t HeapSize_nid_no_patch() {
    const Stats stats = GetStats_nid_no_patch();
    return stats.mmaped - stats.munmaped;
}

std::size_t FreeBytes_nid_no_patch() {
    const Stats stats = GetStats_nid_no_patch();
    const std::uint64_t totalFree = stats.mmaped - stats.munmaped + stats.reallyFreed;
    const std::uint64_t totalUsed = stats.malloced + stats.mallocedRedzones;
    return totalFree > totalUsed ? totalFree - totalUsed : 0;
}

std::string FormatStats_nid_no_patch() {
    const Stats stats = GetStats_nid_no_patch();
    char buffer[256];
    std::string text;
    std::snprintf(buffer, sizeof(buffer), "Stats: %lluM malloced (%lluM for red zones) by %llu calls\n", static_cast<unsigned long long>(stats.malloced >> 20), static_cast<unsigned long long>(stats.mallocedRedzones >> 20), static_cast<unsigned long long>(stats.mallocs));
    text += buffer;
    std::snprintf(buffer, sizeof(buffer), "Stats: %lluM realloced by %llu calls\n", static_cast<unsigned long long>(stats.realloced >> 20), static_cast<unsigned long long>(stats.reallocs));
    text += buffer;
    std::snprintf(buffer, sizeof(buffer), "Stats: %lluM freed by %llu calls\n", static_cast<unsigned long long>(stats.freed >> 20), static_cast<unsigned long long>(stats.frees));
    text += buffer;
    std::snprintf(buffer, sizeof(buffer), "Stats: %lluM really freed by %llu calls\n", static_cast<unsigned long long>(stats.reallyFreed >> 20), static_cast<unsigned long long>(stats.realFrees));
    text += buffer;
    std::snprintf(buffer, sizeof(buffer), "Stats: %lluM (%lluM-%lluM) mmaped; %llu maps, %llu unmaps\n", static_cast<unsigned long long>((stats.mmaped - stats.munmaped) >> 20), static_cast<unsigned long long>(stats.mmaped >> 20), static_cast<unsigned long long>(stats.munmaped >> 20), static_cast<unsigned long long>(stats.mmaps), static_cast<unsigned long long>(stats.munmaps));
    text += buffer;
    text += "  mallocs by size class: ";
    for (std::size_t i = 0; i < std::size(stats.mallocedBySize); ++i) {
        if (stats.mallocedBySize[i]) {
            text += std::to_string(i) + ":" + std::to_string(stats.mallocedBySize[i]) + "; ";
        }
    }
    text += "\n";
    std::snprintf(buffer, sizeof(buffer), "Stats: malloc large: %llu\n", static_cast<unsigned long long>(stats.mallocLarge));
    text += buffer;
    return text;
}

}  // namespace AsanRuntime::Heap
