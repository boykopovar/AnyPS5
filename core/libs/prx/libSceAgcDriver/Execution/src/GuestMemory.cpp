#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <mutex>
#include <array>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fstream>
#include <sstream>
#endif

namespace AgcDriver::GuestMemory {
namespace {
void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("AGC driver: ") + reason);
}

// A code address relative to its module, for symbolizing debug traces with nm.
unsigned long long ModuleOffset(const void* address) {
#ifdef _WIN32
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(address), &module);
    return reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(module);
#else
    return reinterpret_cast<std::uintptr_t>(address);
#endif
}

#ifdef _WIN32
bool readableProtection(DWORD protection) {
    return protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool writableProtection(DWORD protection) {
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}
#endif

}

namespace {
std::atomic<std::uint64_t> forgetCalls{0};
std::atomic<std::uint64_t> forgetBytes{0};
std::atomic<std::uint64_t> collectMemoHits{0};
std::atomic<std::uint64_t> collectEpochBumps{0};
// Walks that reported at least one written page (the ones the probe pass used to double), and the
// tracker mutex acquisitions that had to wait (APS5_PROFILE_DRAW; see lockTracker).
std::atomic<std::uint64_t> collectDirty{0};
std::atomic<std::uint64_t> trackerWaits{0};
std::atomic<std::uint64_t> trackerAcquisitions{0};
std::uintptr_t PagesBase();
std::size_t PagesSize();
std::uintptr_t ImagePagesBase();
std::size_t ImagePagesSize();
}

// Profile (APS5_PROFILE_DRAW): calls, bytes and time of the guest memory copies and comparisons the
// driver makes, printed every 10 s as [guestmem].
struct MemoryCounter {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> bytes{0};
    std::atomic<std::uint64_t> nanoseconds{0};
};

struct MemoryProfile {
    MemoryCounter read;
    MemoryCounter compare;
    MemoryCounter write;
    MemoryCounter changedWrite;
    MemoryCounter collect;
    MemoryCounter verify;
    MemoryCounter query;
    std::atomic<std::int64_t> lastReport{0};
    // Sampled callers of Read and Write (module offsets), to name what touches guest memory most.
    std::mutex callersMutex;
    std::array<std::pair<unsigned long long, std::uint64_t>, 16> callers{};
    std::array<std::pair<unsigned long long, std::uint64_t>, 16> writeCallers{};
};


MemoryProfile& Profile() {
    static MemoryProfile profile;
    return profile;
}

void CountCaller(std::array<std::pair<unsigned long long, std::uint64_t>, 16>& callers, std::uint32_t& sampled, std::uint32_t every, const void* returnAddress) {
    if (++sampled % every != 0) return;
    auto& state = Profile();
    const auto offset = ModuleOffset(returnAddress);
    std::lock_guard lock(state.callersMutex);
    for (auto& [caller, count] : callers) {
        if (caller == offset || count == 0) {
            caller = offset;
            count += every;
            return;
        }
    }
}

// Attribution tags (see the header): the packet a queue worker executes and the read site the
// driver code named. Plain thread-locals; a store per packet costs nothing measurable.
thread_local PacketTag currentPacket{NoPacket, 0xffffffffu};
thread_local ReadSite currentReadSite = ReadSite::Unknown;
// Sampled reads by site (every 256th Read/ReadCommitted, like the callers), for the [guestmem] line.
std::atomic<std::uint64_t> readSiteSamples[static_cast<std::size_t>(ReadSite::Count)] = {};

void SetCurrentPacket(std::uint32_t opcode, std::uint32_t queue) {
    currentPacket = {opcode, queue};
}

PacketTag CurrentPacket() {
    return currentPacket;
}

const char* ReadSiteName(ReadSite site) {
    static const char* const names[static_cast<std::size_t>(ReadSite::Count)] = {"unknown", "capture", "dispatch-cache", "texture-compare", "texture-read", "buffer-upload", "index-buffer", "vertex-buffer", "registers", "indirect-args", "wait", "label", "scanout", "store"};
    const auto index = static_cast<std::size_t>(site);
    return index < static_cast<std::size_t>(ReadSite::Count) ? names[index] : "?";
}

ReadSite SetReadSite(ReadSite site) {
    return std::exchange(currentReadSite, site);
}

ReadSite CurrentReadSite() {
    return currentReadSite;
}

std::size_t CaptureCallerOffsets(std::span<unsigned long long> frames, unsigned skip) {
    if (frames.empty()) return 0;
#ifdef _WIN32
    // Walks the x64 unwind tables, so it needs no frame pointers (unlike __builtin_return_address(n)
    // for n > 0); frame 0 of the capture is this function's caller.
    void* raw[16];
    const auto wanted = static_cast<DWORD>(std::min<std::size_t>(frames.size(), std::size(raw)));
    const auto captured = RtlCaptureStackBackTrace(1 + skip, wanted, raw, nullptr);
    // The driver's module is resolved once (this function lives in it): a per-frame
    // GetModuleHandleEx would take the loader lock several times per sync under the GpuMutex.
    static const auto module = [] {
        HMODULE handle = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(&CaptureCallerOffsets), &handle);
        std::uintptr_t size = 0;
        if (handle != nullptr) {
            // The image size comes from the mapped PE headers (no psapi dependency).
            const auto* base = reinterpret_cast<const std::byte*>(handle);
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            size = nt->OptionalHeader.SizeOfImage;
        }
        return std::pair(reinterpret_cast<std::uintptr_t>(handle), size);
    }();
    for (std::size_t i = 0; i < captured; ++i) {
        const auto address = reinterpret_cast<std::uintptr_t>(raw[i]);
        frames[i] = module.first != 0 && address >= module.first && address - module.first < module.second ? address - module.first : 0;
    }
    return captured;
#else
    static_cast<void>(skip);
    frames[0] = ModuleOffset(__builtin_return_address(0));
    return 1;
#endif
}

void CountReadCaller(const void* returnAddress) {
    thread_local std::uint32_t sampled = 0;
    // Sampled at the same rate as the callers, in step with them (CountCaller advances the counter).
    if ((sampled + 1) % 256 == 0) readSiteSamples[static_cast<std::size_t>(currentReadSite)].fetch_add(256, std::memory_order_relaxed);
    CountCaller(Profile().callers, sampled, 256, returnAddress);
}

void CountWriteCaller(const void* returnAddress) {
    thread_local std::uint32_t sampled = 0;
    CountCaller(Profile().writeCallers, sampled, 16, returnAddress);
}

class TimedAccess {
public:
    TimedAccess(MemoryCounter& counter, std::uint64_t bytes) : counter(counter), bytes(bytes), start(std::chrono::steady_clock::now()) {}
    ~TimedAccess() {
        const auto now = std::chrono::steady_clock::now();
        counter.calls += 1;
        counter.bytes += bytes;
        counter.nanoseconds += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count());
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        if (!profile) return;
        auto& state = Profile();
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        auto last = state.lastReport.load();
        if (nowMs - last < 10000 || !state.lastReport.compare_exchange_strong(last, nowMs)) return;
        const auto line = [](const char* name, const MemoryCounter& c) {
            const double seconds = c.nanoseconds / 1e9;
            std::fprintf(stderr, " %s %llu calls %.0f MiB %.1f s", name, static_cast<unsigned long long>(c.calls.load()), c.bytes / 1048576.0, seconds);
        };
        std::fprintf(stderr, "[guestmem]");
        line("read", state.read);
        line("compare", state.compare);
        line("write", state.write);
        line("write-changed", state.changedWrite);
        line("collect", state.collect);
        line("verify", state.verify);
        line("query", state.query);
        std::fprintf(stderr, " collect-memo hits %llu, collect epochs %llu", static_cast<unsigned long long>(collectMemoHits.load()), static_cast<unsigned long long>(collectEpochBumps.load()));
        std::fprintf(stderr, " collect-dirty %llu tracker waits %llu / %llu", static_cast<unsigned long long>(collectDirty.load()), static_cast<unsigned long long>(trackerWaits.load()), static_cast<unsigned long long>(trackerAcquisitions.load()));
        std::fprintf(stderr, " | arena 0x%llx+0x%llx image 0x%llx+0x%llx forgets %llu (%.0f MiB)", static_cast<unsigned long long>(PagesBase()), static_cast<unsigned long long>(PagesSize()), static_cast<unsigned long long>(ImagePagesBase()), static_cast<unsigned long long>(ImagePagesSize()), static_cast<unsigned long long>(forgetCalls.load()), forgetBytes.load() / 1048576.0);
        {
            std::lock_guard lock(state.callersMutex);
            std::fprintf(stderr, " | read callers:");
            for (const auto& [caller, count] : state.callers) {
                if (count != 0) std::fprintf(stderr, " +0x%llx=%llu", caller, static_cast<unsigned long long>(count));
            }
            std::fprintf(stderr, " | write callers:");
            for (const auto& [caller, count] : state.writeCallers) {
                if (count != 0) std::fprintf(stderr, " +0x%llx=%llu", caller, static_cast<unsigned long long>(count));
            }
        }
        std::fprintf(stderr, " | read sites:");
        for (std::size_t site = 0; site < static_cast<std::size_t>(ReadSite::Count); ++site) {
            const auto count = readSiteSamples[site].load(std::memory_order_relaxed);
            if (count != 0) std::fprintf(stderr, " %s=%llu", ReadSiteName(static_cast<ReadSite>(site)), static_cast<unsigned long long>(count));
        }
        std::fprintf(stderr, "\n");
    }
private:
    MemoryCounter& counter;
    std::uint64_t bytes;
    std::chrono::steady_clock::time_point start;
};

namespace {

// Page-state cache. VirtualQuery finds the extent of a run of equally mapped pages by walking page
// tables, which costs tens of milliseconds inside the game's committed gigabyte heap, and GPU workers
// verify ranges thousands of times per second. Committed pages of the guest arena are therefore
// remembered, one byte of access flags per 4 KiB page (the table is committed lazily, so only the
// pages ever verified cost memory). libc reports every range whose mapping or protection changed
// (registry mutations and guest heap decommits) through the invalidator, which forgets those pages.
// Only committed pages are cached; reserved pages are queried each time, so a commit made outside the
// registry (the guest heap grows that way) is seen at once.
// A second table covers the main guest image (the PE at 0x7ff6b...): its protections change only
// through registry mutations (mprotect replaces the registered range and invalidates through the same
// callback), and address-based builds verify every image section on each build, so without the table
// each of those verifications was a VirtualQuery. APS5_NO_IMAGE_PAGE_CACHE=1 leaves the image uncached.
constexpr std::size_t PageBytes = 4096;
constexpr std::uint8_t PageReadable = 1;
constexpr std::uint8_t PageWritable = 2;

struct PageSpan {
    std::uintptr_t base = 0;
    std::size_t size = 0;
    // Per page: access flags, zero while unknown.
    std::uint8_t* pages = nullptr;

    // Reserves and commits the table; untouched table pages cost nothing until a guest page is recorded.
    bool allocate() {
        if (size == 0) return false;
        const auto count = (size / PageBytes + 8) & ~static_cast<std::size_t>(7);
#ifdef _WIN32
        pages = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, count, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        pages = static_cast<std::uint8_t*>(std::calloc(count, 1));
#endif
        if (pages == nullptr) size = 0;
        return pages != nullptr;
    }

    bool covers(std::uintptr_t address) const {
        return size != 0 && address >= base && address - base < size;
    }

    std::uint8_t load(std::uintptr_t address) const {
        return std::atomic_ref<const std::uint8_t>(pages[(address - base) / PageBytes]).load(std::memory_order_relaxed);
    }

    void store(std::uintptr_t address, std::uint8_t value) {
        std::atomic_ref<std::uint8_t>(pages[(address - base) / PageBytes]).store(value, std::memory_order_relaxed);
    }

    // End of the run of pages equal to `value` that starts at `address`, at most `limit`.
    std::uintptr_t runEnd(std::uintptr_t address, std::uint8_t value, std::uintptr_t limit) const {
        auto index = (address - base) / PageBytes;
        const auto stop = std::min<std::uintptr_t>((std::min(limit, base + size) - base + PageBytes - 1) / PageBytes, size / PageBytes);
        const std::uint64_t wide = 0x0101010101010101ull * value;
        while (index < stop) {
            if (index % 8 == 0 && index + 8 <= stop && std::atomic_ref<const std::uint64_t>(*reinterpret_cast<const std::uint64_t*>(pages + index)).load(std::memory_order_relaxed) == wide) {
                index += 8;
                continue;
            }
            if (std::atomic_ref<const std::uint8_t>(pages[index]).load(std::memory_order_relaxed) != value) break;
            ++index;
        }
        return std::min(limit, base + index * PageBytes);
    }

    void forget(std::uintptr_t address, std::size_t bytes) {
        if (size == 0 || bytes == 0 || address >= base + size || address + bytes <= base) return;
        const auto first = (std::max(address, base) - base) / PageBytes;
        const auto last = (std::min(address + bytes, base + size) - 1 - base) / PageBytes;
        for (auto page = first; page <= last; ++page) std::atomic_ref<std::uint8_t>(pages[page]).store(0, std::memory_order_relaxed);
    }
};

struct PageStates {
    std::once_flag once;
    PageSpan arena;
    PageSpan image;

    void initialize();

    PageSpan* spanOf(std::uintptr_t address) {
        if (arena.covers(address)) return &arena;
        if (image.covers(address)) return &image;
        return nullptr;
    }

    void forget(std::uintptr_t address, std::size_t bytes) {
        arena.forget(address, bytes);
        image.forget(address, bytes);
    }
};

PageStates& Pages() {
    static PageStates pages;
    return pages;
}

std::uintptr_t PagesBase() {
    return Pages().arena.base;
}

std::size_t PagesSize() {
    return Pages().arena.size;
}

std::uintptr_t ImagePagesBase() {
    return Pages().image.base;
}

std::size_t ImagePagesSize() {
    return Pages().image.size;
}

void ForgetPages(std::uintptr_t address, std::size_t bytes) {
    forgetCalls.fetch_add(1, std::memory_order_relaxed);
    forgetBytes.fetch_add(bytes, std::memory_order_relaxed);
    Pages().forget(address, bytes);
}

void PageStates::initialize() {
    std::call_once(once, [&] {
        GuestArena::GuestArenaRange_nid_postfix(&arena.base, &arena.size);
        const bool arenaCached = arena.allocate();
        bool imageCached = false;
#ifdef _WIN32
        // The image extent is the run of regions sharing the module's allocation base, as the registry
        // walks it when it registers the main image.
        static const bool noImageCache = std::getenv("APS5_NO_IMAGE_PAGE_CACHE") != nullptr;
        if (const auto module = noImageCache ? nullptr : GetModuleHandleW(nullptr)) {
            const auto start = reinterpret_cast<std::uintptr_t>(module);
            auto cursor = start;
            for (;;) {
                MEMORY_BASIC_INFORMATION memory{};
                if (VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) != sizeof(memory) || memory.AllocationBase != module || memory.RegionSize == 0 || memory.RegionSize > std::numeric_limits<std::uintptr_t>::max() - cursor) break;
                cursor += memory.RegionSize;
            }
            image.base = start;
            image.size = cursor - start;
            imageCached = image.allocate();
        }
#endif
        if (arenaCached || imageCached) GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(&ForgetPages);
    });
}

struct PageRun {
    std::uintptr_t begin;
    std::uintptr_t end;
    bool readable;
    bool writable;
};

// Calls `emit` with consecutive runs of uniform accessibility covering [address, address + bytes) in
// order, stopping early when it returns false. Returns false when the address space cannot be queried.
template <class Emit>
bool describePages(std::uintptr_t address, std::size_t bytes, Emit&& emit) {
    auto& pages = Pages();
    pages.initialize();
    const auto end = address + bytes;
    auto cursor = address;
    while (cursor < end) {
        if (const auto* span = pages.spanOf(cursor)) {
            const auto value = span->load(cursor);
            if ((value & 3u) != 0) {
                const auto next = span->runEnd(cursor, value, end);
                if (!emit(PageRun{cursor, next, true, (value & PageWritable) != 0})) return true;
                cursor = next;
                continue;
            }
        }
#ifdef _WIN32
        const TimedAccess timed(Profile().query, 0);
        MEMORY_BASIC_INFORMATION memory{};
        const auto queryStart = std::chrono::steady_clock::now();
        // A mutation that changes these pages while they are being queried bumps the generation
        // before it invalidates, so a generation change across the query and the store below means
        // the stored pages may describe the old state and are forgotten again.
        const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) != sizeof(memory)) return false;
        // Debug aid: APS5_TRACE_QUERY prints every 16th page query with its cost.
        static const bool traceQuery = std::getenv("APS5_TRACE_QUERY") != nullptr;
        if (traceQuery) {
            static std::atomic<std::uint32_t> queries{0};
            if (queries.fetch_add(1) % 16 == 0) {
                const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - queryStart).count();
                std::fprintf(stderr, "[query] 0x%llx range 0x%zx: base 0x%llx size 0x%llx state 0x%lx protect 0x%lx type 0x%lx %.0f us gen %llu from +0x%llx\n", static_cast<unsigned long long>(cursor), bytes, reinterpret_cast<unsigned long long>(memory.BaseAddress), static_cast<unsigned long long>(memory.RegionSize), memory.State, memory.Protect, memory.Type, us, static_cast<unsigned long long>(GuestAllocations::GuestAllocationsGeneration_nid_postfix()), ModuleOffset(__builtin_return_address(0)));
            }
        }
        const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        if (memory.RegionSize > std::numeric_limits<std::uintptr_t>::max() - base || base + memory.RegionSize <= cursor) return false;
        const auto regionEnd = base + memory.RegionSize;
        const auto protection = memory.Protect & 0xffu;
        const bool committed = memory.State == MEM_COMMIT && (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
        const bool readable = committed && readableProtection(protection);
        const bool writable = readable && writableProtection(protection);
        for (PageSpan* span : {&pages.arena, &pages.image}) {
            if (!readable || span->size == 0 || regionEnd <= span->base || base >= span->base + span->size) continue;
            const std::uint8_t value = PageReadable | (writable ? PageWritable : 0u);
            const auto first = std::max(base, span->base);
            const auto last = std::min(regionEnd, span->base + span->size);
            for (auto at = first; at < last; at += PageBytes) span->store(at, value);
            if (GuestAllocations::GuestAllocationsGeneration_nid_postfix() != generation) span->forget(first, static_cast<std::size_t>(last - first));
        }
        const auto next = std::min(end, regionEnd);
        if (!emit(PageRun{cursor, next, readable, writable})) return true;
        cursor = next;
#else
        std::ifstream maps("/proc/self/maps");
        if (!maps.is_open()) return false;
        std::string line;
        bool found = false;
        while (std::getline(maps, line)) {
            std::istringstream fields(line);
            std::uintptr_t first = 0;
            std::uintptr_t last = 0;
            char separator = 0;
            std::string permissions;
            if (!(fields >> std::hex >> first >> separator >> last >> permissions) || separator != '-' || first >= last || permissions.size() < 2) return false;
            if (last <= cursor || first > cursor) continue;
            const auto next = std::min(end, last);
            if (!emit(PageRun{cursor, next, permissions[0] == 'r', permissions[0] == 'r' && permissions[1] == 'w'})) return true;
            cursor = next;
            found = true;
            break;
        }
        if (!found) {
            static_cast<void>(emit(PageRun{cursor, end, false, false}));
            return true;
        }
#endif
    }
    return true;
}

// Checks that [address, address + bytes) is mapped with read (and, if asked, write) access. Returns
// an empty string when it is, otherwise why it is not.
std::string verify(std::uintptr_t address, std::size_t bytes, bool writable) {
    const TimedAccess timed(Profile().verify, bytes);
    std::string reason;
    const bool queried = describePages(address, bytes, [&](const PageRun& run) {
        if (!run.readable) {
            char text[128];
            std::snprintf(text, sizeof(text), "guest memory is not readable at 0x%llx (range 0x%llx+0x%zx)", static_cast<unsigned long long>(run.begin), static_cast<unsigned long long>(address), bytes);
            reason = text;
        } else if (writable && !run.writable) {
            reason = "guest memory has no write permission";
        }
        return reason.empty();
    });
    if (!queried && reason.empty()) reason = "cannot query guest memory";
    return reason;
}

}

void CheckRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    require(alignment != 0, "zero guest memory alignment");
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && address % alignment == 0, "null or misaligned address");
    require(bytes <= std::numeric_limits<std::uintptr_t>::max() - address, "address range overflow");
    const auto reason = verify(address, bytes, writable);
    if (!reason.empty()) {
        // Debug aid: APS5_TRACE_UNREADABLE names the code that checked an inaccessible range.
        static const bool trace = std::getenv("APS5_TRACE_UNREADABLE") != nullptr;
        if (trace) std::fprintf(stderr, "[unreadable] %s from +0x%llx\n", reason.c_str(), ModuleOffset(__builtin_return_address(0)));
        require(false, reason.c_str());
    }
}

bool Accessible(const void* pointer, std::size_t bytes, bool writable) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return address != 0 && bytes <= std::numeric_limits<std::uintptr_t>::max() - address && verify(address, bytes, writable).empty();
}

Commitment DescribeCommitted(std::uint64_t address, std::size_t bytes, bool writable) {
    require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "address range overflow");
    const TimedAccess timed(Profile().verify, bytes);
    Commitment result;
    const bool queried = describePages(static_cast<std::uintptr_t>(address), bytes, [&](const PageRun& run) {
        if (run.readable && (!writable || run.writable)) {
            if (!result.ranges.empty() && result.ranges.back().second == run.begin) result.ranges.back().second = run.end;
            else result.ranges.emplace_back(run.begin, run.end);
        }
        return true;
    });
    require(queried, "cannot query guest memory");
    result.whole = bytes == 0 || (result.ranges.size() == 1 && result.ranges.front().first == address && result.ranges.front().second == address + bytes);
    return result;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> CommittedRanges(std::uint64_t address, std::size_t bytes, bool writable) {
    return DescribeCommitted(address, bytes, writable).ranges;
}

namespace {

constexpr std::size_t WriteBlockBytes = 65536;

struct WriteTracker {
    std::mutex mutex;
    bool initialized = false;
    bool watched = false;
    std::uintptr_t base = 0;
    std::size_t size = 0;
    // Generation of the last collected write per 64 KiB block of the arena.
    std::vector<std::uint32_t> blocks;
    // Atomic only so a per-thread memo hit can read it without the mutex (every mutation and every
    // stamp still happen under it): a hit must return the current value, including MarkWritten
    // bumps, or an image refreshed after a stamp would keep failing UnchangedSince until the next
    // walk of its range and re-upload at every use of the epoch.
    std::atomic<std::uint32_t> generation{1};
    std::vector<void*> pages;
    // Collect memo: page-rounded ranges walked during the current epoch (see BumpCollectEpoch). A
    // collect fully inside one skips GetWriteWatch, whose cost is proportional to the range's pages,
    // and the same surfaces are collected several times per epoch (each storage mip binding, a sampled
    // view plus its storage refresh, a write-back followed by a refresh, and every dispatch of the
    // epoch binding the same targets). Entries carry the walking thread's epoch, so no entry matches
    // another thread: the ring consulted first is per thread (ThreadCollectMemo, no mutex, no
    // eviction by other threads' walks); this shared ring (8 workers with epochs of several packets
    // hold a few hundred distinct ranges) serves APS5_SHARED_COLLECT_MEMO=1.
    struct Memo {
        std::uintptr_t begin = 0;
        std::uintptr_t end = 0;
        std::uint64_t epoch = 0;
    };
    std::array<Memo, 256> memo{};
    std::size_t nextMemo = 0;

    void initialize() {
        if (initialized) return;
        initialized = true;
        GuestArena::GuestArenaRange_nid_postfix(&base, &size);
        watched = size != 0 && GuestArena::GuestArenaWriteWatched_nid_postfix();
        if (!watched) return;
        blocks.assign(size / WriteBlockBytes + 1, 0);
        pages.resize(1u << 16);
    }
};

WriteTracker& Tracker() {
    static WriteTracker tracker;
    return tracker;
}

// Takes the tracker mutex; under APS5_PROFILE_DRAW an acquisition that found it held is counted
// ('tracker waits N / M' in [guestmem]), which says whether walking outside the lock would pay.
std::unique_lock<std::mutex> lockTracker(WriteTracker& tracker) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return std::unique_lock(tracker.mutex);
    std::unique_lock lock(tracker.mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        trackerWaits.fetch_add(1, std::memory_order_relaxed);
        lock.lock();
    }
    trackerAcquisitions.fetch_add(1, std::memory_order_relaxed);
    return lock;
}

// Collect epochs are per thread and globally unique: every bump takes a fresh value from one counter,
// so a memo entry (stamped with the epoch of its walk) can only match the thread that made it, and
// only until that thread's next ordering point (the queue workers bump at theirs, see the header). A
// thread that never bumped is given a new epoch for every collect: its walks are never reused (the
// presenter, game threads inside the flush hook), as nothing orders the CPU's writes for it.
std::atomic<std::uint64_t> nextCollectEpoch{1};
thread_local std::uint64_t threadCollectEpoch = 0;

std::uint64_t currentCollectEpoch() {
    if (threadCollectEpoch != 0) return threadCollectEpoch;
    return nextCollectEpoch.fetch_add(1, std::memory_order_relaxed) + 1;
}

bool collectMemoEnabled() {
    static const bool disabled = std::getenv("APS5_NO_COLLECT_MEMO") != nullptr;
    return !disabled;
}

// The per-thread memo ring (see WriteTracker::Memo): sized to a packet's working set (a dispatch
// collects each of its few dozen image surfaces in stage A and again in stage B). Entries of a
// thread that never bumps its epoch never match (each collect gets a fresh epoch), as intended.
// APS5_SHARED_COLLECT_MEMO=1 uses the tracker's shared ring under its mutex instead.
struct ThreadCollectMemo {
    std::array<WriteTracker::Memo, 64> entries{};
    std::size_t next = 0;
};
thread_local ThreadCollectMemo threadCollectMemo;

bool sharedCollectMemo() {
    static const bool shared = std::getenv("APS5_SHARED_COLLECT_MEMO") != nullptr;
    return shared;
}

std::uint64_t collectWrites(std::uint64_t address, std::size_t bytes, bool memoized) {
    auto& tracker = Tracker();
#ifdef _WIN32
    // Whole pages, so a page shared with the next range is collected with either.
    constexpr std::uint64_t page = 4096;
    const auto first = address & ~(page - 1);
    const auto stop = (address + bytes + page - 1) & ~(page - 1);
    // One epoch for the lookup and the entry made after the walk: an unbumped thread's fresh epoch
    // must not differ between them.
    const auto epoch = currentCollectEpoch();
    const bool useMemo = memoized && collectMemoEnabled() && bytes != 0;
    if (useMemo && !sharedCollectMemo()) {
        // An entry exists only for a completed walk of an in-arena range, so the tracker is
        // initialized and watched; nothing below the lock needs asking.
        for (const auto& entry : threadCollectMemo.entries) {
            if (entry.epoch == epoch && entry.begin <= first && stop <= entry.end) {
                // The current generation, not the memoized one: blocks stamped since (MarkWritten, an
                // overlapping collect) were written before this caller reads, and an older value would
                // make every later UnchangedSince fail until the epoch ends.
                collectMemoHits.fetch_add(1, std::memory_order_relaxed);
                return tracker.generation.load(std::memory_order_relaxed);
            }
        }
    }
#endif
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || bytes == 0 || address < tracker.base || address - tracker.base > tracker.size - bytes) return 0;
#ifdef _WIN32
    auto cursor = first;
    if (useMemo && sharedCollectMemo()) {
        for (const auto& entry : tracker.memo) {
            if (entry.epoch == epoch && entry.begin <= cursor && stop <= entry.end) {
                collectMemoHits.fetch_add(1, std::memory_order_relaxed);
                return tracker.generation;
            }
        }
    }
    const TimedAccess timed(Profile().collect, bytes);
    ++tracker.generation;
    // One resetting walk: the kernel reports and clears a page's dirty bit together, a write landing
    // after the walk passed a page is reported by the next walk, and a clean page is not touched by the
    // reset, so a probe pass first (which doubled the walk of every dirty range) buys nothing.
    // APS5_NO_SINGLE_PASS_COLLECT=1 restores the probe pass followed by a resetting pass when dirty.
    static const bool singlePass = std::getenv("APS5_NO_SINGLE_PASS_COLLECT") == nullptr;
    bool dirty = false;
    while (cursor < stop) {
        ULONG_PTR count = tracker.pages.size();
        DWORD granularity = 0;
        if (GetWriteWatch(singlePass ? WRITE_WATCH_FLAG_RESET : 0, reinterpret_cast<void*>(cursor), static_cast<std::size_t>(stop - cursor), tracker.pages.data(), &count, &granularity) != 0) {
            // Uncommitted pages inside the range make the call fail; such ranges are compared instead.
            return 0;
        }
        if (count != 0) dirty = true;
        for (ULONG_PTR i = 0; i < count; ++i) tracker.blocks[(reinterpret_cast<std::uintptr_t>(tracker.pages[i]) - tracker.base) / WriteBlockBytes] = tracker.generation;
        if (count < tracker.pages.size()) break;
        cursor = reinterpret_cast<std::uintptr_t>(tracker.pages[count - 1]) + (granularity != 0 ? granularity : page);
    }
    if (dirty) collectDirty.fetch_add(1, std::memory_order_relaxed);
    if (dirty && !singlePass) {
        // The resetting pass reports pages again, so a write racing the first pass is stamped too.
        cursor = address & ~(page - 1);
        while (cursor < stop) {
            ULONG_PTR count = tracker.pages.size();
            DWORD granularity = 0;
            if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, reinterpret_cast<void*>(cursor), static_cast<std::size_t>(stop - cursor), tracker.pages.data(), &count, &granularity) != 0) return 0;
            for (ULONG_PTR i = 0; i < count; ++i) tracker.blocks[(reinterpret_cast<std::uintptr_t>(tracker.pages[i]) - tracker.base) / WriteBlockBytes] = tracker.generation;
            if (count < tracker.pages.size()) break;
            cursor = reinterpret_cast<std::uintptr_t>(tracker.pages[count - 1]) + (granularity != 0 ? granularity : page);
        }
    }
    // Only a completed walk is remembered; a failed one (uncommitted pages) returned 0 above.
    if (collectMemoEnabled()) {
        if (sharedCollectMemo()) tracker.memo[tracker.nextMemo++ % tracker.memo.size()] = {first, stop, epoch};
        else threadCollectMemo.entries[threadCollectMemo.next++ % threadCollectMemo.entries.size()] = {first, stop, epoch};
    }
    return tracker.generation;
#else
    static_cast<void>(memoized);
    static_cast<void>(threadCollectMemo);
    return 0;
#endif
}

}

std::uint64_t CollectWrites(std::uint64_t address, std::size_t bytes) {
    return collectWrites(address, bytes, true);
}

std::uint64_t CollectWritesUncached(std::uint64_t address, std::size_t bytes) {
    return collectWrites(address, bytes, false);
}

void BumpCollectEpoch() {
    threadCollectEpoch = nextCollectEpoch.fetch_add(1, std::memory_order_relaxed) + 1;
    collectEpochBumps.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t CollectEpochBumps() {
    return collectEpochBumps.load(std::memory_order_relaxed);
}

bool UnchangedSince(std::uint64_t address, std::size_t bytes, std::uint64_t generation) {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || generation == 0 || bytes == 0 || address < tracker.base || address - tracker.base > tracker.size - bytes) return false;
    const auto first = (address - tracker.base) / WriteBlockBytes;
    const auto last = (address - tracker.base + bytes - 1) / WriteBlockBytes;
    for (auto block = first; block <= last; ++block) {
        if (tracker.blocks[block] > generation) return false;
    }
    return true;
}

void MarkWritten(std::uint64_t address, std::size_t bytes) {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || bytes == 0 || address < tracker.base || address - tracker.base > tracker.size - bytes) return;
    const auto first = (address - tracker.base) / WriteBlockBytes;
    const auto last = (address - tracker.base + bytes - 1) / WriteBlockBytes;
    ++tracker.generation;
    for (auto block = first; block <= last; ++block) tracker.blocks[block] = tracker.generation;
}

namespace {

// Per-thread GpuMutex wait accounting (APS5_PROFILE_DRAW): a queue worker tags its thread, other
// threads (the presenter) report untagged.
constexpr std::size_t GpuLockSiteCount = static_cast<std::size_t>(GpuLockSite::Count);
constexpr const char* GpuLockSiteNames[GpuLockSiteCount] = {"other", "dispatch", "indirect", "draw", "hook", "wait", "label", "flush", "present", "fill", "try"};

struct GpuLockStats {
    std::uint32_t tag = 0xffffffffu;
    std::uint64_t acquisitions = 0;
    std::uint64_t waits = 0;
    double waitedMs = 0;
    // The same split by the site tagged before the acquisition (TagGpuLockSite); the tag of the
    // next lock() is kept here and consumed by it.
    std::array<std::uint64_t, GpuLockSiteCount> siteAcquisitions{};
    std::array<std::uint64_t, GpuLockSiteCount> siteWaits{};
    std::array<double, GpuLockSiteCount> siteWaitedMs{};
    GpuLockSite nextSite = GpuLockSite::Other;
    // Holds: the outermost (depth 0 -> 1) acquisition of this thread until the matching unlock,
    // charged to that acquisition's site (a nested hook lock inside a dispatch's hold extends the
    // dispatch's hold). The waits above say who waits; these say whom they wait for.
    std::array<std::uint64_t, GpuLockSiteCount> siteHolds{};
    std::array<double, GpuLockSiteCount> siteHeldMs{};
    std::array<double, GpuLockSiteCount> siteMaxHeldMs{};
    std::size_t holdSite = 0;
    std::chrono::steady_clock::time_point holdStart;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

GpuLockStats& LockStats() {
    thread_local GpuLockStats stats;
    return stats;
}

bool GpuLockProfiled() {
    static const bool profiled = std::getenv("APS5_PROFILE_DRAW") != nullptr && std::getenv("APS5_NO_LOCK_PROFILE") == nullptr;
    return profiled;
}

// Hold timing costs two clock reads per outermost acquisition (~20000 per 10 s on queue 0, well
// under a millisecond); APS5_NO_HOLD_PROFILE=1 keeps only the wait timing.
bool GpuHoldProfiled() {
    static const bool profiled = GpuLockProfiled() && std::getenv("APS5_NO_HOLD_PROFILE") == nullptr;
    return profiled;
}

void BeginHold(std::size_t site) {
    auto& stats = LockStats();
    stats.holdSite = site;
    stats.holdStart = std::chrono::steady_clock::now();
}

void EndHold() {
    auto& stats = LockStats();
    const auto heldMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - stats.holdStart).count();
    ++stats.siteHolds[stats.holdSite];
    stats.siteHeldMs[stats.holdSite] += heldMs;
    stats.siteMaxHeldMs[stats.holdSite] = std::max(stats.siteMaxHeldMs[stats.holdSite], heldMs);
}

}

// The owner token is a thread-local's address: unique per live thread and free to read.
namespace {
const void* ThreadToken() {
    thread_local char token;
    return &token;
}
}

void GpuMutexType::acquired() {
    owner.store(ThreadToken(), std::memory_order_relaxed);
    ++depth;
}

bool GpuMutexType::try_lock() {
    if (!GpuLockProfiled()) {
        if (!mutex.try_lock()) return false;
        acquired();
        return true;
    }
    // The tag is consumed whether or not the try succeeds (a failed try must not name the next
    // acquisition); an untagged try's hold is charged to 'try' (a boundary reap, an idle worker).
    auto& stats = LockStats();
    const auto tagged = std::exchange(stats.nextSite, GpuLockSite::Other);
    if (!mutex.try_lock()) return false;
    acquired();
    if (depth == 1 && GpuHoldProfiled()) BeginHold(static_cast<std::size_t>(tagged == GpuLockSite::Other ? GpuLockSite::Try : tagged));
    return true;
}

namespace {
std::atomic<void (*)()> gpuUnlockHook{nullptr};
}

void SetGpuUnlockHook(void (*hook)()) {
    gpuUnlockHook.store(hook, std::memory_order_release);
}

void GpuMutexType::unlock() {
    const bool outermost = --depth == 0;
    if (outermost) {
        owner.store(nullptr, std::memory_order_relaxed);
        // Still the holder here: the hold ends when the mutex is given up, not before.
        if (GpuHoldProfiled()) EndHold();
    }
    mutex.unlock();
    // After the release, so what the hook does (destroying a batch's kept objects) is neither part
    // of the hold nor waited for by the next holder.
    if (outermost) {
        if (auto* hook = gpuUnlockHook.load(std::memory_order_acquire); hook != nullptr) hook();
    }
}

bool GpuMutexType::HeldByThisThread() const {
    return owner.load(std::memory_order_relaxed) == ThreadToken();
}

std::uint32_t GpuMutexType::DepthOnThisThread() const {
    // `depth` is written by the holder only, so it is read only when that is this thread.
    return HeldByThisThread() ? depth : 0;
}

void AssertGpuLockHeld(const char* where) {
    static const bool enabled = std::getenv("APS5_ASSERT_GPU_LOCK") != nullptr;
    if (!enabled || GpuMutex().HeldByThisThread()) return;
    std::fprintf(stderr, "[lock] ASSERT: %s reached without GuestMemory::GpuMutex on thread tagged 0x%x\n", where, GpuLockThreadTag());
    std::abort();
}

void GpuMutexType::lock() {
    if (!GpuLockProfiled()) {
        mutex.lock();
        acquired();
        return;
    }
    auto& stats = LockStats();
    ++stats.acquisitions;
    const auto site = static_cast<std::size_t>(std::exchange(stats.nextSite, GpuLockSite::Other));
    ++stats.siteAcquisitions[site];
    // An uncontended (or recursive) acquisition costs no clock reads for the wait; only a wait is
    // timed. The hold timer (outermost acquisitions only) starts once the mutex is held.
    if (!mutex.try_lock()) {
        const auto start = std::chrono::steady_clock::now();
        mutex.lock();
        acquired();
        const auto now = std::chrono::steady_clock::now();
        if (depth == 1 && GpuHoldProfiled()) {
            stats.holdSite = site;
            stats.holdStart = now;
        }
        const auto waitedMs = std::chrono::duration<double, std::milli>(now - start).count();
        stats.waitedMs += waitedMs;
        ++stats.waits;
        stats.siteWaitedMs[site] += waitedMs;
        ++stats.siteWaits[site];
        if (now - stats.lastReport < std::chrono::seconds(10)) return;
        // The line is printed from inside a contended acquisition, so a thread that rarely waits
        // reports a longer span than 10 s: the real interval is printed.
        const auto interval = std::chrono::duration<double>(now - stats.lastReport).count();
        stats.lastReport = now;
        std::string sites;
        for (std::size_t i = 0; i < GpuLockSiteCount; ++i) {
            if (stats.siteAcquisitions[i] == 0) continue;
            char text[96];
            std::snprintf(text, sizeof(text), " %s %llu/%llu %.0f ms", GpuLockSiteNames[i], static_cast<unsigned long long>(stats.siteAcquisitions[i]), static_cast<unsigned long long>(stats.siteWaits[i]), stats.siteWaitedMs[i]);
            sites += text;
        }
        // Holds by site, longest total first: the sites whose holds the other threads wait behind.
        std::string holds;
        if (GpuHoldProfiled()) {
            std::array<std::size_t, GpuLockSiteCount> order{};
            for (std::size_t i = 0; i < GpuLockSiteCount; ++i) order[i] = i;
            std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return stats.siteHeldMs[a] > stats.siteHeldMs[b]; });
            double heldMs = 0;
            for (std::size_t i = 0; i < GpuLockSiteCount; ++i) heldMs += stats.siteHeldMs[i];
            char text[112];
            std::snprintf(text, sizeof(text), "; held %.0f ms in total, by site (holds, held, max):", heldMs);
            holds += text;
            for (const auto i : order) {
                if (stats.siteHolds[i] == 0) continue;
                std::snprintf(text, sizeof(text), " %s %llu %.0f ms (max %.1f)", GpuLockSiteNames[i], static_cast<unsigned long long>(stats.siteHolds[i]), stats.siteHeldMs[i], stats.siteMaxHeldMs[i]);
                holds += text;
            }
        }
        if (stats.tag == 0xffffffffu) std::fprintf(stderr, "[lock] untagged thread waited %.0f ms for the GPU mutex in %llu of %llu acquisitions (%.0f s); by site (acquisitions/waits, waited):%s%s\n", stats.waitedMs, static_cast<unsigned long long>(stats.waits), static_cast<unsigned long long>(stats.acquisitions), interval, sites.c_str(), holds.c_str());
        else std::fprintf(stderr, "[lock] queue 0x%x waited %.0f ms for the GPU mutex in %llu of %llu acquisitions (%.0f s); by site (acquisitions/waits, waited):%s%s\n", stats.tag, stats.waitedMs, static_cast<unsigned long long>(stats.waits), static_cast<unsigned long long>(stats.acquisitions), interval, sites.c_str(), holds.c_str());
        stats.waitedMs = 0;
        stats.waits = 0;
        stats.acquisitions = 0;
        stats.siteAcquisitions.fill(0);
        stats.siteWaits.fill(0);
        stats.siteWaitedMs.fill(0);
        stats.siteHolds.fill(0);
        stats.siteHeldMs.fill(0);
        stats.siteMaxHeldMs.fill(0);
        return;
    }
    acquired();
    if (depth == 1 && GpuHoldProfiled()) BeginHold(site);
}

void TagGpuLockSite(GpuLockSite site) {
    // Off the profiled path the tag is never read, so the thread-local is not touched either.
    if (!GpuLockProfiled()) return;
    LockStats().nextSite = site;
}

std::uint32_t GpuLockThreadTag() {
    return LockStats().tag;
}

GpuMutexType& GpuMutex() {
    static GpuMutexType mutex;
    return mutex;
}

void TagGpuLockThread(std::uint32_t queue) {
    LockStats().tag = queue;
}

unsigned long long CodeOffset(const void* address) {
    return ModuleOffset(address);
}

namespace {
std::atomic<void (*)(std::uint64_t, std::size_t)> flushHook{nullptr};
}

void SetFlushHook(void (*hook)(std::uint64_t, std::size_t)) {
    flushHook.store(hook, std::memory_order_release);
}

void FlushGpuWrites(std::uint64_t address, std::size_t bytes) {
    if (const auto hook = flushHook.load(std::memory_order_acquire)) hook(address, bytes);
}

void Read(std::uint64_t address, std::span<std::byte> destination, std::size_t alignment) {
    if (destination.empty()) return;
    FlushGpuWrites(address, destination.size());
    const TimedAccess timed(Profile().read, destination.size());
    CountReadCaller(__builtin_return_address(0));
    const auto* source = reinterpret_cast<const void*>(address);
    try {
        CheckRange(source, destination.size(), alignment);
    } catch (const std::runtime_error&) {
        static const bool trace = std::getenv("APS5_TRACE_UNREADABLE") != nullptr;
        if (trace) std::fprintf(stderr, "[unreadable]   read from +0x%llx\n", ModuleOffset(__builtin_return_address(0)));
        throw;
    }
    std::memcpy(destination.data(), source, destination.size());
}

void ReadCommitted(std::uint64_t address, std::span<std::byte> destination) {
    if (destination.empty()) return;
    FlushGpuWrites(address, destination.size());
    const TimedAccess timed(Profile().read, destination.size());
    CountReadCaller(__builtin_return_address(0));
    if (Accessible(reinterpret_cast<const void*>(address), destination.size())) {
        std::memcpy(destination.data(), reinterpret_cast<const void*>(address), destination.size());
        return;
    }
    std::memset(destination.data(), 0, destination.size());
    for (const auto& [begin, end] : CommittedRanges(address, destination.size())) std::memcpy(destination.data() + (begin - address), reinterpret_cast<const void*>(begin), static_cast<std::size_t>(end - begin));
}

bool EqualsCommitted(std::uint64_t address, std::span<const std::byte> bytes) {
    if (bytes.empty()) return true;
    FlushGpuWrites(address, bytes.size());
    const TimedAccess timed(Profile().compare, bytes.size());
    if (Accessible(reinterpret_cast<const void*>(address), bytes.size())) return std::memcmp(reinterpret_cast<const void*>(address), bytes.data(), bytes.size()) == 0;
    for (const auto& [begin, end] : CommittedRanges(address, bytes.size())) {
        if (std::memcmp(reinterpret_cast<const void*>(begin), bytes.data() + (begin - address), static_cast<std::size_t>(end - begin)) != 0) return false;
    }
    return true;
}

void WriteChangedCommitted(std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original) {
    require(current.size() == original.size(), "write-back snapshot sizes differ");
    if (Accessible(reinterpret_cast<const void*>(address), current.size(), true)) {
        WriteChanged(address, current, original);
        return;
    }
    for (const auto& [begin, end] : CommittedRanges(address, current.size(), true)) {
        const auto offset = static_cast<std::size_t>(begin - address);
        const auto length = static_cast<std::size_t>(end - begin);
        WriteChanged(begin, current.subspan(offset, length), original.subspan(offset, length));
    }
}

void WriteChanged(std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original) {
    require(current.size() == original.size(), "write-back snapshot sizes differ");
    if (current.empty()) return;
    // A store: the hook sync it makes orders this write after a pending GPU write, whatever a read
    // site above it was doing (the [hooksync] line counts stores apart).
    const ReadSiteScope site(ReadSite::Store);
    FlushGpuWrites(address, current.size());
    const TimedAccess timed(Profile().changedWrite, current.size());
    auto* destination = reinterpret_cast<std::byte*>(address);
    CheckRange(destination, current.size(), 1, true);
    constexpr std::size_t block = 256;
    const auto size = current.size();
    const auto differs = [&](std::size_t at) {
        const auto length = std::min(block, size - at);
        return std::memcmp(current.data() + at, original.data() + at, length) != 0;
    };
    std::size_t firstChanged = size;
    std::size_t lastChanged = 0;
    for (std::size_t at = 0; at < size; at += block) {
        if (!differs(at)) continue;
        std::memcpy(destination + at, current.data() + at, std::min(block, size - at));
        firstChanged = std::min(firstChanged, at);
        lastChanged = std::min(at + block, size);
    }
    // Stamped like a GPU write: a collect memoized for this packet would not see the page fault.
    if (firstChanged < lastChanged) MarkWritten(address + firstChanged, lastChanged - firstChanged);
}

void Write(std::uint64_t address, std::span<const std::byte> source, std::size_t alignment) {
    if (source.empty()) return;
    // A store, as in WriteChanged.
    const ReadSiteScope site(ReadSite::Store);
    FlushGpuWrites(address, source.size());
    const TimedAccess timed(Profile().write, source.size());
    CountWriteCaller(__builtin_return_address(0));
    auto* destination = reinterpret_cast<void*>(address);
    CheckRange(destination, source.size(), alignment, true);
    std::memcpy(destination, source.data(), source.size());
    // Stamped like a GPU write: a collect memoized for this packet would not see the page fault.
    MarkWritten(address, source.size());
}

}

extern "C" void AgcDriverCheckGuestMemory_nid_postfix(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    AgcDriver::GuestMemory::CheckRange(pointer, bytes, alignment, writable);
}
