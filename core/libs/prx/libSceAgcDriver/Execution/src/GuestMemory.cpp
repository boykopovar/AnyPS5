#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/HandoffSpin.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/WriteWatchCoverage.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include <mutex>
#include <array>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <bit>
#include <emmintrin.h>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <link.h>
#include <pthread.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>
#include "prx/libc/include/specifics/linux/HostMapping.hpp"
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

std::uintptr_t userAddressLimit() {
    static const auto limit = [] {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);
    }();
    return limit;
}
#endif

}

namespace {
std::atomic<std::uint64_t> forgetCalls{0};
// See ForgetSerial: odd while a ForgetPages call stores its page states.
std::atomic<std::uint64_t> forgetSerial{0};
std::atomic<std::uint64_t> forgetBytes{0};
std::atomic<std::uint64_t> collectMemoHits{0};
std::atomic<std::uint64_t> collectEpochBumps{0};
std::atomic<std::uint64_t> unwatchSerial{0};
// Walks that reported at least one written page (the ones the probe pass used to double), and the
// tracker mutex acquisitions that had to wait (APS5_PROFILE_DRAW; see lockTracker).
std::atomic<std::uint64_t> collectDirty{0};
#ifndef _WIN32
std::atomic<std::uint64_t> collectDirtyRuns{0};
#endif
std::atomic<std::uint64_t> trackerWaits{0};
std::atomic<std::uint64_t> trackerAcquisitions{0};
std::uintptr_t PagesBase();
std::size_t PagesSize();
std::uintptr_t ImagePagesBase();
std::size_t ImagePagesSize();
}

// Profile (APS5_PROFILE_DRAW): calls, bytes and time of the guest memory copies and comparisons the
// driver makes, printed every 10 s as [guestmem]. Accumulated per thread (the calling thread is the
// only writer: plain loads and stores, no locked RMW on a cache line every queue worker shares) and
// folded at report time; a finished thread's totals move to `retired`. Nothing is counted without
// the profile switch (the counters are only ever printed under it).
// Debug aid: APS5_SHARED_MEMORY_COUNTERS=1 accumulates into one shared set with atomic RMWs as before.
enum MemoryCounterKind { CounterRead, CounterCompare, CounterWrite, CounterChangedWrite, CounterCollect, CounterVerify, CounterQuery, MemoryCounterCount };
constexpr const char* MemoryCounterNames[MemoryCounterCount] = {"read", "compare", "write", "write-changed", "collect", "verify", "query"};

struct MemoryCounter {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> bytes{0};
    std::atomic<std::uint64_t> nanoseconds{0};

    void Add(std::uint64_t moreBytes, std::uint64_t moreNanoseconds, bool shared) {
        if (shared) {
            calls.fetch_add(1, std::memory_order_relaxed);
            bytes.fetch_add(moreBytes, std::memory_order_relaxed);
            nanoseconds.fetch_add(moreNanoseconds, std::memory_order_relaxed);
            return;
        }
        calls.store(calls.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        bytes.store(bytes.load(std::memory_order_relaxed) + moreBytes, std::memory_order_relaxed);
        nanoseconds.store(nanoseconds.load(std::memory_order_relaxed) + moreNanoseconds, std::memory_order_relaxed);
    }
};

struct MemoryCounters {
    std::array<MemoryCounter, MemoryCounterCount> counters;

    void FoldInto(std::array<std::array<std::uint64_t, 3>, MemoryCounterCount>& totals) const {
        for (std::size_t i = 0; i < MemoryCounterCount; ++i) {
            totals[i][0] += counters[i].calls.load(std::memory_order_relaxed);
            totals[i][1] += counters[i].bytes.load(std::memory_order_relaxed);
            totals[i][2] += counters[i].nanoseconds.load(std::memory_order_relaxed);
        }
    }
    // Under the profile's threadsMutex (the only writer of `retired`).
    void AddFrom(const MemoryCounters& other) {
        for (std::size_t i = 0; i < MemoryCounterCount; ++i) {
            counters[i].calls.store(counters[i].calls.load(std::memory_order_relaxed) + other.counters[i].calls.load(std::memory_order_relaxed), std::memory_order_relaxed);
            counters[i].bytes.store(counters[i].bytes.load(std::memory_order_relaxed) + other.counters[i].bytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
            counters[i].nanoseconds.store(counters[i].nanoseconds.load(std::memory_order_relaxed) + other.counters[i].nanoseconds.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
    }
};

struct MemoryProfile {
    AgcDriver::Mutex threadsMutex;
    std::vector<const MemoryCounters*> threads;
    MemoryCounters retired;
    MemoryCounters shared;
    std::atomic<std::int64_t> lastReport{0};
    // Sampled callers of Read and Write (module offsets), to name what touches guest memory most.
    AgcDriver::Mutex callersMutex;
    std::array<std::pair<unsigned long long, std::uint64_t>, 16> callers{};
    std::array<std::pair<unsigned long long, std::uint64_t>, 16> writeCallers{};
};

// Never destroyed: worker threads' counters unregister at thread exit, which can follow static
// destruction at process exit.
MemoryProfile& Profile() {
    static MemoryProfile& profile = *new MemoryProfile();
    return profile;
}

bool SharedMemoryCounters() {
    static const bool shared = std::getenv("APS5_SHARED_MEMORY_COUNTERS") != nullptr;
    return shared;
}

struct ThreadMemoryCounters {
    MemoryCounters counters;

    ThreadMemoryCounters() {
        auto& profile = Profile();
        std::lock_guard lock(profile.threadsMutex);
        profile.threads.push_back(&counters);
    }
    ~ThreadMemoryCounters() {
        auto& profile = Profile();
        std::lock_guard lock(profile.threadsMutex);
        std::erase(profile.threads, &counters);
        profile.retired.AddFrom(counters);
    }
};

MemoryCounter& CounterFor(MemoryCounterKind kind) {
    if (SharedMemoryCounters()) return Profile().shared.counters[kind];
    thread_local ThreadMemoryCounters* thread = nullptr;
    return ShaderRecompiler::ThreadOwned(thread).counters.counters[kind];
}

// Every thread's counters, the retired and the shared ones summed: calls, bytes, nanoseconds per kind.
std::array<std::array<std::uint64_t, 3>, MemoryCounterCount> FoldMemoryCounters() {
    auto& profile = Profile();
    std::array<std::array<std::uint64_t, 3>, MemoryCounterCount> totals{};
    std::lock_guard lock(profile.threadsMutex);
    for (const auto* thread : profile.threads) thread->FoldInto(totals);
    profile.retired.FoldInto(totals);
    profile.shared.FoldInto(totals);
    return totals;
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

struct CurrentPacketSlot {};
struct CurrentReadSiteSlot {};
constexpr std::uint64_t NoPacketBits = NoPacket | (std::uint64_t{0xffffffffu} << 32u);
// Sampled reads by site (every 256th Read/ReadCommitted, like the callers), for the [guestmem] line.
std::atomic<std::uint64_t> readSiteSamples[static_cast<std::size_t>(ReadSite::Count)] = {};

void SetCurrentPacket(std::uint32_t opcode, std::uint32_t queue) {
    HostThreadSlot<std::uint64_t, CurrentPacketSlot>::Set((opcode | (std::uint64_t{queue} << 32u)) ^ NoPacketBits);
}

PacketTag CurrentPacket() {
    const auto bits = HostThreadSlot<std::uint64_t, CurrentPacketSlot>::Get() ^ NoPacketBits;
    return {static_cast<std::uint32_t>(bits), static_cast<std::uint32_t>(bits >> 32u)};
}

const char* ReadSiteName(ReadSite site) {
    static const char* const names[static_cast<std::size_t>(ReadSite::Count)] = {"unknown", "capture", "dispatch-cache", "texture-compare", "texture-read", "buffer-upload", "index-buffer", "vertex-buffer", "registers", "indirect-args", "wait", "label", "scanout", "store", "mirror-refresh", "draw-cache"};
    const auto index = static_cast<std::size_t>(site);
    return index < static_cast<std::size_t>(ReadSite::Count) ? names[index] : "?";
}

ReadSite SetReadSite(ReadSite site) {
    const auto previous = HostThreadSlot<ReadSite, CurrentReadSiteSlot>::Get();
    HostThreadSlot<ReadSite, CurrentReadSiteSlot>::Set(site);
    return previous;
}

ReadSite CurrentReadSite() {
    return HostThreadSlot<ReadSite, CurrentReadSiteSlot>::Get();
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
    if ((sampled + 1) % 256 == 0) readSiteSamples[static_cast<std::size_t>(CurrentReadSite())].fetch_add(256, std::memory_order_relaxed);
    CountCaller(Profile().callers, sampled, 256, returnAddress);
}

void CountWriteCaller(const void* returnAddress) {
    thread_local std::uint32_t sampled = 0;
    CountCaller(Profile().writeCallers, sampled, 16, returnAddress);
}

bool MemoryProfiled() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

class TimedAccess {
public:
    TimedAccess(MemoryCounterKind kind, std::uint64_t bytes) : kind(kind), bytes(bytes), start(MemoryProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}) {}
    ~TimedAccess() {
        if (!MemoryProfiled()) return;
        const auto now = std::chrono::steady_clock::now();
        CounterFor(kind).Add(bytes, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count()), SharedMemoryCounters());
        auto& state = Profile();
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        auto last = state.lastReport.load();
        if (nowMs - last < 10000 || !state.lastReport.compare_exchange_strong(last, nowMs)) return;
        const auto totals = FoldMemoryCounters();
        AgcDriver::ProfilePrint_nid_no_patch("[guestmem]");
        for (std::size_t i = 0; i < MemoryCounterCount; ++i) {
            AgcDriver::ProfilePrint_nid_no_patch(" %s %llu calls %.0f MiB %.1f s", MemoryCounterNames[i], static_cast<unsigned long long>(totals[i][0]), totals[i][1] / 1048576.0, totals[i][2] / 1e9);
        }
        AgcDriver::ProfilePrint_nid_no_patch(" collect-memo hits %llu, collect epochs %llu", static_cast<unsigned long long>(collectMemoHits.load()), static_cast<unsigned long long>(collectEpochBumps.load()));
        AgcDriver::ProfilePrint_nid_no_patch(" collect-dirty %llu tracker waits %llu / %llu", static_cast<unsigned long long>(collectDirty.load()), static_cast<unsigned long long>(trackerWaits.load()), static_cast<unsigned long long>(trackerAcquisitions.load()));
        AgcDriver::ProfilePrint_nid_no_patch(" | arena 0x%llx+0x%llx image 0x%llx+0x%llx forgets %llu (%.0f MiB)", static_cast<unsigned long long>(PagesBase()), static_cast<unsigned long long>(PagesSize()), static_cast<unsigned long long>(ImagePagesBase()), static_cast<unsigned long long>(ImagePagesSize()), static_cast<unsigned long long>(forgetCalls.load()), forgetBytes.load() / 1048576.0);
        {
            std::lock_guard lock(state.callersMutex);
            AgcDriver::ProfilePrint_nid_no_patch(" | read callers:");
            for (const auto& [caller, count] : state.callers) {
                if (count != 0) AgcDriver::ProfilePrint_nid_no_patch(" +0x%llx=%llu", caller, static_cast<unsigned long long>(count));
            }
            AgcDriver::ProfilePrint_nid_no_patch(" | write callers:");
            for (const auto& [caller, count] : state.writeCallers) {
                if (count != 0) AgcDriver::ProfilePrint_nid_no_patch(" +0x%llx=%llu", caller, static_cast<unsigned long long>(count));
            }
        }
        AgcDriver::ProfilePrint_nid_no_patch(" | read sites:");
        for (std::size_t site = 0; site < static_cast<std::size_t>(ReadSite::Count); ++site) {
            const auto count = readSiteSamples[site].load(std::memory_order_relaxed);
            if (count != 0) AgcDriver::ProfilePrint_nid_no_patch(" %s=%llu", ReadSiteName(static_cast<ReadSite>(site)), static_cast<unsigned long long>(count));
        }
        AgcDriver::ProfilePrint_nid_no_patch("\n");
    }
private:
    MemoryCounterKind kind;
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
// A second table covers the main guest image (the PE at 0x7ff6b...): its protections change only
// through registry mutations (mprotect replaces the registered range and invalidates through the same
// callback), and address-based builds verify every image section on each build, so without the table
// each of those verifications was a VirtualQuery. APS5_NO_IMAGE_PAGE_CACHE=1 leaves the image uncached.
constexpr std::size_t PageBytes = 4096;
constexpr std::uint8_t PageReadable = 1;
constexpr std::uint8_t PageWritable = 2;
constexpr std::uint8_t PageInaccessible = 4;

struct PageSpan {
    std::uintptr_t base = 0;
    std::size_t size = 0;
    // Per page: access flags, zero while unknown.
    std::uint8_t* pages = nullptr;
    static constexpr std::size_t ChunkEntries = 65536;
    struct alignas(8) Chunk { std::array<std::uint8_t, ChunkEntries> entries{}; };
    std::atomic<Chunk*>* chunks = nullptr;

    std::uint8_t* chunkData(std::size_t index, bool create) const {
        auto* chunk = chunks[index].load(std::memory_order_acquire);
        if (chunk == nullptr && create) {
            auto fresh = std::unique_ptr<Chunk>(new (std::nothrow) Chunk{});
            if (fresh != nullptr && chunks[index].compare_exchange_strong(chunk, fresh.get(), std::memory_order_acq_rel, std::memory_order_acquire)) chunk = fresh.release();
        }
        return chunk == nullptr ? nullptr : chunk->entries.data();
    }

    std::uint8_t* entry(std::size_t index, bool create) const {
        if (chunks == nullptr) return pages + index;
        auto* data = chunkData(index / ChunkEntries, create);
        return data == nullptr ? nullptr : data + index % ChunkEntries;
    }

    bool allocate() {
        if (size == 0) return false;
        const auto count = (size / PageBytes + 8) & ~static_cast<std::size_t>(7);
#ifdef _WIN32
        const auto chunkCount = (count + ChunkEntries - 1) / ChunkEntries;
        chunks = new (std::nothrow) std::atomic<Chunk*>[chunkCount]{};
        if (chunks == nullptr) size = 0;
        return chunks != nullptr;
#else
        pages = static_cast<std::uint8_t*>(std::calloc(count, 1));
        if (pages == nullptr) size = 0;
        return pages != nullptr;
#endif
    }

    bool covers(std::uintptr_t address) const {
        return size != 0 && address >= base && address - base < size;
    }

    std::uint8_t load(std::uintptr_t address) const {
        auto* value = entry((address - base) / PageBytes, false);
        return value == nullptr ? 0 : std::atomic_ref<std::uint8_t>(*value).load(std::memory_order_relaxed);
    }

    void store(std::uintptr_t address, std::uint8_t value) {
        if (auto* at = entry((address - base) / PageBytes, value != 0 && value != PageInaccessible)) std::atomic_ref<std::uint8_t>(*at).store(value, std::memory_order_relaxed);
    }

    void store(std::uintptr_t first, std::uintptr_t last, std::uint8_t value) {
        auto index = (first - base) / PageBytes;
        const auto stop = (last - 1 - base) / PageBytes + 1;
        while (index < stop) {
            const auto next = std::min(stop, (index / ChunkEntries + 1) * ChunkEntries);
            if (auto* at = entry(index, value != 0 && value != PageInaccessible)) {
                for (; index < next; ++index, ++at) std::atomic_ref<std::uint8_t>(*at).store(value, std::memory_order_relaxed);
            } else index = next;
        }
    }

    // End of the run of pages equal to `value` that starts at `address`, at most `limit`.
    std::uintptr_t runEnd(std::uintptr_t address, std::uint8_t value, std::uintptr_t limit) const {
        auto index = (address - base) / PageBytes;
        const auto stop = std::min<std::uintptr_t>((std::min(limit, base + size) - base + PageBytes - 1) / PageBytes, size / PageBytes);
        const std::uint64_t wide = 0x0101010101010101ull * value;
        while (index < stop) {
            auto* at = entry(index, false);
            if (at == nullptr) {
                if (value != 0) break;
                index = std::min(stop, (index / ChunkEntries + 1) * ChunkEntries);
                continue;
            }
            if (index % 8 == 0 && index + 8 <= stop && std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(at)).load(std::memory_order_relaxed) == wide) {
                index += 8;
                continue;
            }
            if (std::atomic_ref<std::uint8_t>(*at).load(std::memory_order_relaxed) != value) break;
            ++index;
        }
        return std::min(limit, base + index * PageBytes);
    }

    void forget(std::uintptr_t address, std::size_t bytes) {
        if (size == 0 || bytes == 0 || address >= base + size || address + bytes <= base) return;
        const auto first = (std::max(address, base) - base) / PageBytes;
        const auto last = (std::min(address + bytes, base + size) - 1 - base) / PageBytes;
        store(base + first * PageBytes, base + (last + 1) * PageBytes, 0);
    }
};

struct PageStates {
    std::once_flag once;
    std::atomic<bool> ready{false};
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

    std::atomic<std::uint64_t> accessSerial{0};

    void record(std::uintptr_t base, std::uintptr_t regionEnd, bool readable, bool writable, std::uint64_t generation, std::uint64_t access) {
        for (PageSpan* span : {&arena, &image}) {
            if (span->size == 0 || regionEnd <= span->base || base >= span->base + span->size) continue;
            const std::uint8_t value = readable ? static_cast<std::uint8_t>(PageReadable | (writable ? PageWritable : 0u)) : PageInaccessible;
            const auto first = std::max(base, span->base);
            const auto last = std::min(regionEnd, span->base + span->size);
            span->store(first, last, value);
            if (GuestAllocations::GuestAllocationsGeneration_nid_postfix() != generation || accessSerial.load(std::memory_order_acquire) != access) span->forget(first, static_cast<std::size_t>(last - first));
        }
    }

    void forgetInaccessible(std::uintptr_t address, std::size_t bytes) {
        for (PageSpan* span : {&arena, &image}) {
            if (span->size == 0 || bytes == 0 || address >= span->base + span->size || address + bytes <= span->base) continue;
            const auto last = std::min(address + bytes, span->base + span->size);
            for (auto at = std::max(address, span->base) & ~static_cast<std::uintptr_t>(PageBytes - 1); at < last; at += PageBytes) {
                if (span->load(at) == PageInaccessible) span->store(at, 0);
            }
        }
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
    forgetSerial.fetch_add(1, std::memory_order_release);
    Pages().forget(address, bytes);
    forgetSerial.fetch_add(1, std::memory_order_release);
}

void ForgetInaccessiblePages(std::uintptr_t address, std::size_t bytes) {
    Pages().accessSerial.fetch_add(1, std::memory_order_acq_rel);
    Pages().forgetInaccessible(address, bytes);
}

#ifdef __linux__
int mainImageExtent(dl_phdr_info* info, std::size_t, void* data) {
    constexpr auto mask = static_cast<std::uintptr_t>(PageBytes - 1);
    auto& span = *static_cast<PageSpan*>(data);
    std::uintptr_t first = std::numeric_limits<std::uintptr_t>::max();
    std::uintptr_t last = 0;
    for (int index = 0; index < info->dlpi_phnum; ++index) {
        const auto& header = info->dlpi_phdr[index];
        if (header.p_type != PT_LOAD || header.p_memsz == 0) continue;
        first = std::min(first, static_cast<std::uintptr_t>(info->dlpi_addr + header.p_vaddr) & ~mask);
        last = std::max(last, static_cast<std::uintptr_t>(info->dlpi_addr + header.p_vaddr + header.p_memsz + mask) & ~mask);
    }
    if (first < last) {
        span.base = first;
        span.size = static_cast<std::size_t>(last - first);
    }
    return 1;
}
#endif

void PageStates::initialize() {
    if (ready.load(std::memory_order_acquire)) return;
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
#elif defined(__linux__)
        static const bool noImageCache = std::getenv("APS5_NO_IMAGE_PAGE_CACHE") != nullptr;
        if (!noImageCache) {
            dl_iterate_phdr(mainImageExtent, &image);
            imageCached = image.allocate();
        }
#endif
        if (arenaCached || imageCached) {
            GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(&ForgetPages);
            GuestArena::GuestArenaSetAccessObserver_nid_postfix(&ForgetInaccessiblePages);
        }
    });
    ready.store(true, std::memory_order_release);
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
            if (value != 0) {
                const auto next = span->runEnd(cursor, value, end);
                if (!emit(PageRun{cursor, next, (value & PageReadable) != 0, (value & PageWritable) != 0})) return true;
                cursor = next;
                continue;
            }
        }
#ifdef _WIN32
        const TimedAccess timed(CounterQuery, 0);
        MEMORY_BASIC_INFORMATION memory{};
        const auto queryStart = std::chrono::steady_clock::now();
        // A mutation that changes these pages while they are being queried bumps the generation
        // before it invalidates, so a generation change across the query and the store below means
        // the stored pages may describe the old state and are forgotten again.
        const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        const auto access = pages.accessSerial.load(std::memory_order_acquire);
        if (cursor > userAddressLimit()) {
            static_cast<void>(emit(PageRun{cursor, end, false, false}));
            return true;
        }
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
        std::uint32_t logicalProtection = memory.Protect;
        GuestArena::GuestArenaProtection_nid_postfix(cursor, &logicalProtection);
        const auto protection = logicalProtection & 0xffu;
        const bool committed = memory.State == MEM_COMMIT && (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
        const bool readable = committed && readableProtection(protection);
        const bool writable = readable && writableProtection(protection);
        if ((memory.Protect & PAGE_GUARD) == 0) pages.record(base, regionEnd, readable, writable, generation, access);
        const auto next = std::min(end, regionEnd);
        if (!emit(PageRun{cursor, next, readable, writable})) return true;
        cursor = next;
#else
        const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        const auto access = pages.accessSerial.load(std::memory_order_acquire);
        const auto recordRun = [&](std::uintptr_t next, bool readable, bool writable) {
            constexpr auto mask = static_cast<std::uintptr_t>(PageBytes - 1);
            pages.record(cursor & ~mask, (next + mask) & ~mask, readable, writable, generation, access);
        };
        if (const int fd = HostMapping::QueryFd(); fd >= 0) {
            procmap_query query{};
            query.size = sizeof(query);
            query.query_flags = PROCMAP_QUERY_COVERING_OR_NEXT_VMA;
            query.query_addr = cursor;
            if (ioctl(fd, PROCMAP_QUERY, &query) == 0) {
                if (query.vma_end <= cursor || query.vma_start >= query.vma_end) return false;
                if (query.vma_start > cursor) {
                    const auto gapEnd = std::min<std::uintptr_t>(end, query.vma_start);
                    recordRun(gapEnd, false, false);
                    if (!emit(PageRun{cursor, gapEnd, false, false})) return true;
                    cursor = gapEnd;
                    continue;
                }
                const bool readable = (query.vma_flags & PROCMAP_QUERY_VMA_READABLE) != 0;
                const bool writable = readable && (query.vma_flags & PROCMAP_QUERY_VMA_WRITABLE) != 0;
                const auto next = std::min<std::uintptr_t>(end, query.vma_end);
                recordRun(next, readable, writable);
                if (!emit(PageRun{cursor, next, readable, writable})) return true;
                cursor = next;
                continue;
            }
            if (errno == ENOENT) {
                static_cast<void>(emit(PageRun{cursor, end, false, false}));
                return true;
            }
            return false;
        }
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
            if (last <= cursor) continue;
            if (first > cursor) {
                const auto gapEnd = std::min(end, first);
                recordRun(gapEnd, false, false);
                if (!emit(PageRun{cursor, gapEnd, false, false})) return true;
                cursor = gapEnd;
                found = true;
                break;
            }
            const bool readable = permissions[0] == 'r';
            const bool writable = readable && permissions[1] == 'w';
            const auto next = std::min(end, last);
            recordRun(next, readable, writable);
            if (!emit(PageRun{cursor, next, readable, writable})) return true;
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

bool onOwnLiveStack(std::uintptr_t address, std::size_t bytes) {
    struct Bounds {
        std::uintptr_t low = 0;
        std::uintptr_t high = 0;
        Bounds() {
#ifdef _WIN32
            ULONG_PTR lowLimit = 0;
            ULONG_PTR highLimit = 0;
            GetCurrentThreadStackLimits(&lowLimit, &highLimit);
            low = static_cast<std::uintptr_t>(lowLimit);
            high = static_cast<std::uintptr_t>(highLimit);
#else
            pthread_attr_t attributes;
            if (pthread_getattr_np(pthread_self(), &attributes) != 0) return;
            void* base = nullptr;
            std::size_t size = 0;
            if (pthread_attr_getstack(&attributes, &base, &size) == 0 && base != nullptr) {
                low = reinterpret_cast<std::uintptr_t>(base);
                high = low + size;
            }
            pthread_attr_destroy(&attributes);
#endif
        }
    };
    thread_local const Bounds bounds;
    const auto frame = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
    return bounds.high != 0 && frame >= bounds.low && frame < bounds.high && address >= frame && address < bounds.high && bytes <= bounds.high - address;
}

#ifdef __linux__
bool pageStateRecorded(std::uintptr_t address, std::size_t bytes) {
    auto& pages = Pages();
    pages.initialize();
    for (auto at = address & ~static_cast<std::uintptr_t>(PageBytes - 1); at < address + bytes; at += PageBytes) {
        if (const auto* span = pages.spanOf(at); span != nullptr && span->load(at) != 0) return true;
    }
    return false;
}

bool readableWithoutQuery(std::uintptr_t address, std::size_t bytes) {
    constexpr std::size_t limit = 4096;
    if (bytes > limit || HostMapping::QueryFd() >= 0 || pageStateRecorded(address, bytes)) return false;
    thread_local std::array<std::byte, limit> scratch;
    iovec local{scratch.data(), bytes};
    iovec remote{reinterpret_cast<void*>(address), bytes};
    static const pid_t self = getpid();
    return ::process_vm_readv(self, &local, 1, &remote, 1, 0) == static_cast<ssize_t>(bytes);
}
#endif

// Checks that [address, address + bytes) is mapped with read (and, if asked, write) access. Returns
// an empty string when it is, otherwise why it is not.
std::string verify(std::uintptr_t address, std::size_t bytes, bool writable) {
    const TimedAccess timed(CounterVerify, bytes);
    if (onOwnLiveStack(address, bytes)) return {};
#ifdef __linux__
    if (!writable && readableWithoutQuery(address, bytes)) return {};
#endif
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

std::uint64_t ForgetSerial() {
    return forgetSerial.load(std::memory_order_acquire);
}

Commitment DescribeCommitted(std::uint64_t address, std::size_t bytes, bool writable) {
    require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "address range overflow");
    const TimedAccess timed(CounterVerify, bytes);
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
constexpr std::size_t WritePageBytes = 4096;
constexpr std::size_t WritePagesPerBlock = WriteBlockBytes / WritePageBytes;

enum class StampKind : std::uint8_t { Cpu, Driver, ImportWindow, Fresh };

std::uint64_t HashPage(const std::byte* page) {
    constexpr std::uint64_t prime1 = 0x9E3779B185EBCA87ull;
    constexpr std::uint64_t prime2 = 0xC2B2AE3D27D4EB4Full;
    constexpr std::uint64_t prime3 = 0x165667B19E3779F9ull;
    constexpr std::uint64_t prime4 = 0x85EBCA77C2B2AE63ull;
    const auto round = [](std::uint64_t accumulator, std::uint64_t word) { return std::rotl(accumulator + word * prime2, 31) * prime1; };
    std::array<std::uint64_t, 4> lanes{prime1 + prime2, prime2, 0, 0 - prime1};
    for (std::size_t offset = 0; offset < WritePageBytes; offset += 4 * sizeof(std::uint64_t)) {
        std::array<std::uint64_t, 4> words;
        std::memcpy(words.data(), page + offset, sizeof(words));
        for (std::size_t lane = 0; lane < 4; ++lane) lanes[lane] = round(lanes[lane], words[lane]);
    }
    std::uint64_t hash = std::rotl(lanes[0], 1) + std::rotl(lanes[1], 7) + std::rotl(lanes[2], 12) + std::rotl(lanes[3], 18);
    for (const auto lane : lanes) hash = (hash ^ round(0, lane)) * prime1 + prime4;
    hash ^= hash >> 33;
    hash *= prime2;
    hash ^= hash >> 29;
    hash *= prime3;
    return hash ^ (hash >> 32);
}

struct WindowCopy {
    std::uint64_t first;
    std::span<const std::uint64_t> hashes;

    bool Holds(std::uintptr_t page) const {
        return HashPage(reinterpret_cast<const std::byte*>(page)) == hashes[(page - first) / WritePageBytes];
    }
};

#ifdef _WIN32
void watchPrivateMapping(std::uintptr_t address, std::size_t bytes, std::uint64_t generation);
#endif

struct WriteTracker {
    AgcDriver::Mutex mutex;
    bool initialized = false;
    bool watched = false;
    static constexpr std::size_t CoarseBlocks = 64;
    static constexpr std::size_t LeafBlocks = std::size_t{1} << 16;
    struct Leaf {
        std::array<std::uint32_t, LeafBlocks> blocks{};
        std::array<std::uint32_t, LeafBlocks> cpuBlocks{};
        std::array<std::uint32_t, LeafBlocks / CoarseBlocks> coarseBlocks{};
#ifdef _WIN32
        std::array<std::uint32_t, LeafBlocks> hintBlocks{};
#endif
    };
    std::vector<std::unique_ptr<Leaf>> leaves;
#ifdef _WIN32
    std::uintptr_t base = 0;
    std::size_t size = 0;
    WriteWatchCoverage coverage;
#else
    static constexpr std::size_t LeafCount = std::size_t{1} << 15;
#endif
    // Atomic only so a per-thread memo hit can read it without the mutex (every mutation and every
    // stamp still happen under it): a hit must return the current value, including MarkWritten
    // bumps, or an image refreshed after a stamp would keep failing UnchangedSince until the next
    // walk of its range and re-upload at every use of the epoch.
    std::atomic<std::uint32_t> generation{1};
    std::atomic<std::uint32_t> stable{1};
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
        std::uint64_t unwatched = 0;
        std::uint64_t mapping = 0;
    };
    std::array<Memo, 256> memo{};
    std::size_t nextMemo = 0;

    void initialize() {
        if (initialized) return;
        initialized = true;
#ifdef _WIN32
        GuestArena::GuestArenaRange_nid_postfix(&base, &size);
        watched = size != 0 && GuestArena::GuestArenaWriteWatched_nid_postfix();
        if (!watched) return;
        leaves.resize((size / WriteBlockBytes + LeafBlocks) / LeafBlocks);
        coverage.Initialize(base, size);
        pages.resize(1u << 16);
        GuestArena::GuestArenaSetPrivateMappingObserver_nid_postfix(&watchPrivateMapping);
#else
        watched = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        if (watched) leaves.resize(LeafCount);
#endif
    }

    bool covers(std::uint64_t address, std::size_t bytes) const {
#ifdef _WIN32
        return coverage.Covers(address, bytes) && !GuestArena::GuestArenaHostRegionOverlaps_nid_postfix(static_cast<std::uintptr_t>(address), bytes);
#else
        return address + bytes <= LeafCount * LeafBlocks * WriteBlockBytes && GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(static_cast<std::uintptr_t>(address), bytes);
#endif
    }

    std::uint64_t blockOf(std::uint64_t address) const {
#ifdef _WIN32
        return (address - base) / WriteBlockBytes;
#else
        return address / WriteBlockBytes;
#endif
    }

    std::uint64_t blockBegin(std::uint64_t block) const {
#ifdef _WIN32
        return base + block * WriteBlockBytes;
#else
        return block * WriteBlockBytes;
#endif
    }

#ifdef _WIN32
    void startHints(std::uint64_t address, std::size_t bytes) {
        const auto first = std::max<std::uint64_t>(address, base);
        const auto stop = std::min<std::uint64_t>(address + bytes, base + size);
        if (first >= stop) return;
        bool started = false;
        for (auto block = blockOf(first); block <= blockOf(stop - 1); ++block) {
            if (!coverage.Covers(blockBegin(block), static_cast<std::size_t>(std::min<std::uint64_t>(WriteBlockBytes, base + size - blockBegin(block))))) continue;
            if (!started) ++generation;
            started = true;
            stampHint(block);
        }
    }

    bool hinted(std::uint64_t first, std::uint64_t stop) const {
        if (first >= stop || first < base || stop > base + size) return false;
        for (auto block = blockOf(first); block <= blockOf(stop - 1); ++block) {
            if (hintOf(block) == 0 || coverage.Covers(blockBegin(block), static_cast<std::size_t>(std::min<std::uint64_t>(WriteBlockBytes, base + size - blockBegin(block))))) return false;
        }
        return !GuestArena::GuestArenaHostRegionOverlaps_nid_postfix(static_cast<std::uintptr_t>(first), static_cast<std::size_t>(stop - first));
    }

    std::uint32_t newestHint(std::uint64_t first, std::uint64_t stop) const {
        std::uint32_t newest = 0;
        for (auto block = blockOf(first); block <= blockOf(stop - 1); ++block) newest = std::max(newest, hintOf(block));
        return newest;
    }
#endif

    struct DriverPiece {
        std::uint32_t generation = 0;
        std::uint32_t begin = 0;
        std::uint32_t end = 0;
    };
    struct DriverPieces {
        std::uint32_t whole = 0;
        std::uint32_t dropped = 0;
        std::array<DriverPiece, 4> pieces{};
        std::uint32_t possibleWhole = 0;
        std::uint32_t possibleDropped = 0;
        std::array<DriverPiece, 4> possiblePieces{};

        bool covers(std::uint32_t begin, std::uint32_t end, std::uint64_t generation) const {
            if (generation == 0 || begin >= end || dropped > generation) return false;
            if (whole > generation) return true;
            while (begin < end) {
                auto covered = begin;
                for (const auto& piece : pieces) {
                    if (piece.generation > generation && piece.begin <= begin && piece.end > covered) covered = piece.end;
                }
                if (covered == begin) return false;
                begin = covered;
            }
            return true;
        }
    };
    std::unordered_map<std::uint64_t, DriverPieces> driverPieces;

    struct CpuPages {
        std::array<std::uint32_t, WritePagesPerBlock> pages{};
    };
    std::unordered_map<std::uint64_t, CpuPages> cpuPages;
    std::unordered_map<std::uint64_t, CpuPages> walkPages;

    void noteCpuStore(std::uint64_t begin, std::uint64_t end, std::uint32_t stampGeneration) {
        notePages(cpuPages, begin, end, stampGeneration);
    }

    void noteWalkStore(std::uint64_t begin, std::uint64_t end, std::uint32_t stampGeneration) {
        notePages(walkPages, begin, end, stampGeneration);
    }

    void notePages(std::unordered_map<std::uint64_t, CpuPages>& pages, std::uint64_t begin, std::uint64_t end, std::uint32_t stampGeneration) {
        for (auto block = blockOf(begin); block <= blockOf(end - 1); ++block) {
            const auto blockStart = blockBegin(block);
            const auto from = std::max(begin, blockStart) - blockStart;
            const auto to = std::min<std::uint64_t>(end, blockStart + WriteBlockBytes) - blockStart;
            auto& entry = pages[block];
            for (auto page = from / WritePageBytes; page <= (to - 1) / WritePageBytes; ++page) {
                auto& stamp = entry.pages[page];
                stamp = std::max(stamp, stampGeneration);
            }
        }
    }

    bool blockPartWatched(std::uint64_t first, std::uint64_t stop) const {
#ifdef _WIN32
        return covers(first, 1) || covers(stop - 1, 1);
#else
        return GuestWriteWatch::GuestWriteWatchOverlaps_nid_postfix(static_cast<std::uintptr_t>(first), static_cast<std::size_t>(stop - first));
#endif
    }

    void stampWatchedPart(std::uint64_t address, std::uint64_t end, std::uint32_t stampGeneration, bool complete) {
#ifdef _WIN32
        address = std::max<std::uint64_t>(address, base);
        end = std::min<std::uint64_t>(end, base + size);
        if (address >= end) return;
#else
        end = std::min<std::uint64_t>(end, LeafCount * LeafBlocks * WriteBlockBytes);
        if (address >= end || !GuestWriteWatch::GuestWriteWatchOverlaps_nid_postfix(static_cast<std::uintptr_t>(address), static_cast<std::size_t>(end - address))) return;
#endif
        for (auto block = blockOf(address); block <= blockOf(end - 1); ++block) {
            const auto first = std::max(address, blockBegin(block));
            const auto stop = std::min<std::uint64_t>(end, blockBegin(block) + WriteBlockBytes);
            if (!blockPartWatched(first, stop)) continue;
            stamp(block, stampGeneration, StampKind::Driver);
            noteDriverStore(block, address, end, stampGeneration, complete);
        }
    }

    void noteDriverStore(std::uint64_t block, std::uint64_t address, std::uint64_t end, std::uint32_t stampGeneration, bool complete) {
        const auto begin = blockBegin(block);
        const auto from = static_cast<std::uint32_t>(std::max(address, begin) - begin);
        const auto to = static_cast<std::uint32_t>(std::min<std::uint64_t>(end, begin + WriteBlockBytes) - begin);
        if (from == 0 && to == WriteBlockBytes) {
            auto& entry = driverPieces[block];
            if (complete) {
                entry = DriverPieces{};
                entry.whole = stampGeneration;
            } else {
                entry.possibleWhole = stampGeneration;
                entry.possibleDropped = 0;
                entry.possiblePieces = {};
            }
            return;
        }
        auto& entry = driverPieces[block];
        auto& pieces = complete ? entry.pieces : entry.possiblePieces;
        auto& dropped = complete ? entry.dropped : entry.possibleDropped;
        DriverPiece* contained = nullptr;
        for (auto& piece : pieces) {
            if (piece.generation == 0 || piece.begin < from || piece.end > to) continue;
            if (contained == nullptr) contained = &piece;
            else piece = {};
        }
        if (contained != nullptr) {
            *contained = {stampGeneration, from, to};
            return;
        }
        auto* oldest = &pieces.front();
        for (auto& piece : pieces) {
            if (piece.generation < oldest->generation) oldest = &piece;
        }
        dropped = std::max(dropped, oldest->generation);
        *oldest = {stampGeneration, from, to};
    }

    Leaf& leafFor(std::uint64_t block) {
        auto& leaf = leaves[block / LeafBlocks];
        if (leaf == nullptr) leaf = std::make_unique<Leaf>();
        return *leaf;
    }

#ifdef _WIN32
    void stampHint(std::uint64_t block) {
        leafFor(block).hintBlocks[block % LeafBlocks] = generation;
    }

    std::uint32_t hintOf(std::uint64_t block) const {
        const auto& leaf = leaves[block / LeafBlocks];
        return leaf != nullptr ? leaf->hintBlocks[block % LeafBlocks] : 0;
    }
#endif

    std::uint32_t stampOf(std::uint64_t block) const {
        const auto& leaf = leaves[block / LeafBlocks];
        return leaf != nullptr ? leaf->blocks[block % LeafBlocks] : 0;
    }

    static constexpr std::size_t UnstampedSlots = 1u << 16;
    std::vector<std::uint32_t> unstampedStores;

    void noteUnstampedStore(std::uint64_t address, std::uint64_t end) {
        if (unstampedStores.empty()) unstampedStores.assign(UnstampedSlots, 0);
        const auto first = address / WriteBlockBytes;
        const auto last = (end - 1) / WriteBlockBytes;
        if (last - first >= UnstampedSlots) {
            std::fill(unstampedStores.begin(), unstampedStores.end(), generation);
            return;
        }
        for (auto block = first; block <= last; ++block) unstampedStores[block % UnstampedSlots] = generation;
    }

    std::uint32_t newestStamp(std::uint64_t address, std::uint64_t end) const {
        std::uint32_t newest = 0;
        if (!unstampedStores.empty()) {
            const auto first = address / WriteBlockBytes;
            const auto last = (end - 1) / WriteBlockBytes;
            if (last - first >= UnstampedSlots) newest = *std::max_element(unstampedStores.begin(), unstampedStores.end());
            else for (auto block = first; block <= last; ++block) newest = std::max(newest, unstampedStores[block % UnstampedSlots]);
        }
        if (!watched) return newest;
#ifdef _WIN32
        address = std::max<std::uint64_t>(address, base);
        end = std::min<std::uint64_t>(end, base + size);
#else
        end = std::min<std::uint64_t>(end, LeafCount * LeafBlocks * WriteBlockBytes);
#endif
        if (address >= end) return newest;
        for (auto block = blockOf(address); block <= blockOf(end - 1); ++block) newest = std::max(newest, stampOf(block));
        return newest;
    }

    std::uint32_t coarseStampOf(std::uint64_t block) const {
        const auto& leaf = leaves[block / LeafBlocks];
        return leaf != nullptr ? leaf->coarseBlocks[(block % LeafBlocks) / CoarseBlocks] : 0;
    }

    bool unchanged(std::uint64_t first, std::uint64_t last, std::uint64_t since) const {
        if (first == last) return stampOf(first) <= since;
        const auto newer = [since](std::uint32_t stamp) { return stamp > since; };
        while (first <= last) {
            const auto& leaf = leaves[first / LeafBlocks];
            if (leaf == nullptr) {
                first = (first / LeafBlocks + 1) * LeafBlocks;
                continue;
            }
            const auto end = std::min<std::uint64_t>(last + 1, (first / CoarseBlocks + 1) * CoarseBlocks);
            if (end - first < CoarseBlocks || coarseStampOf(first) > since) {
                const auto begin = leaf->blocks.begin() + first % LeafBlocks;
                if (std::any_of(begin, begin + (end - first), newer)) return false;
            }
            first = end;
        }
        return true;
    }

    std::uint32_t cpuStampOf(std::uint64_t block) const {
        const auto& leaf = leaves[block / LeafBlocks];
        return leaf != nullptr ? leaf->cpuBlocks[block % LeafBlocks] : 0;
    }

    bool storedOver(std::uint64_t address, std::uint64_t end, std::uint64_t generation, bool walked) const {
        for (auto block = blockOf(address); block <= blockOf(end - 1); ++block) {
            if (stampOf(block) <= generation) continue;
            const auto begin = blockBegin(block);
            const auto from = std::max(address, begin) - begin;
            const auto to = std::min<std::uint64_t>(end, begin + WriteBlockBytes) - begin;
            const auto newerPage = [&](const std::unordered_map<std::uint64_t, CpuPages>& pages, std::uint64_t first, std::uint64_t stop) {
                const auto found = pages.find(block);
                if (found == pages.end()) return false;
                for (auto page = first / WritePageBytes; page <= (stop - 1) / WritePageBytes; ++page) {
                    if (found->second.pages[page] > generation) return true;
                }
                return false;
            };
            if (cpuPages.find(block) == cpuPages.end() && cpuStampOf(block) > generation) return true;
            if (newerPage(cpuPages, from, to)) return true;
            if (walked && newerPage(walkPages, from, to)) return true;
            const auto found = driverPieces.find(block);
            if (found == driverPieces.end()) {
                if (!walked && newerPage(walkPages, 0, WriteBlockBytes)) return true;
                continue;
            }
            const auto& entry = found->second;
            if (entry.whole > generation || entry.dropped > generation || entry.possibleWhole > generation || entry.possibleDropped > generation) return true;
            for (const auto& piece : entry.pieces) {
                if (piece.generation > generation && piece.begin < to && from < piece.end) return true;
            }
            for (const auto& piece : entry.possiblePieces) {
                if (piece.generation > generation && piece.begin < to && from < piece.end) return true;
            }
        }
        return false;
    }

    bool rangeUnchanged(std::uint64_t address, std::size_t bytes, std::uint64_t since) const {
        return unchanged(blockOf(address), blockOf(address + bytes - 1), since) || !storedOver(address, address + bytes, since, true);
    }

    void stamp(std::uint64_t block, std::uint32_t stampGeneration, StampKind kind) {
        const bool cpu = kind == StampKind::Cpu || kind == StampKind::ImportWindow;
        const bool stored = kind != StampKind::ImportWindow;
        auto& leaf = leafFor(block);
        if (stored) {
            leaf.blocks[block % LeafBlocks] = stampGeneration;
            auto& coarse = leaf.coarseBlocks[(block % LeafBlocks) / CoarseBlocks];
            coarse = std::max(coarse, stampGeneration);
        }
        if (cpu) leaf.cpuBlocks[block % LeafBlocks] = stampGeneration;
    }
};

WriteTracker& Tracker() {
    static WriteTracker tracker;
    return tracker;
}

// Takes the tracker mutex; under APS5_PROFILE_DRAW an acquisition that found it held is counted
// ('tracker waits N / M' in [guestmem]), which says whether walking outside the lock would pay.
class TrackerLock {
public:
    explicit TrackerLock(WriteTracker& tracker) : tracker(tracker), lock(tracker.mutex, std::defer_lock) {
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        if (!profile) {
            lock.lock();
            return;
        }
        if (!lock.try_lock()) {
            trackerWaits.fetch_add(1, std::memory_order_relaxed);
            lock.lock();
        }
        trackerAcquisitions.fetch_add(1, std::memory_order_relaxed);
    }
    ~TrackerLock() {
        if (lock.owns_lock()) publish();
    }
    TrackerLock(const TrackerLock&) = delete;
    TrackerLock& operator=(const TrackerLock&) = delete;

    void Unlock() {
        publish();
        lock.unlock();
    }

    void Suspend() {
        lock.unlock();
    }

    void Resume() {
        lock.lock();
    }

private:
    void publish() {
        const auto current = tracker.generation.load(std::memory_order_relaxed);
        if (tracker.stable.load(std::memory_order_relaxed) != current) tracker.stable.store(current, std::memory_order_release);
    }

    WriteTracker& tracker;
    std::unique_lock<AgcDriver::Mutex> lock;
};

TrackerLock lockTracker(WriteTracker& tracker) {
    return TrackerLock(tracker);
}

#ifdef _WIN32
void watchPrivateMapping(std::uintptr_t address, std::size_t bytes, std::uint64_t generation) {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    if (!tracker.watched || bytes == 0 || bytes > tracker.size || address < tracker.base || address - tracker.base > tracker.size - bytes) return;
    tracker.coverage.Restore(address, bytes, generation);
    ++tracker.generation;
    tracker.noteCpuStore(address, address + bytes, tracker.generation);
    for (auto block = tracker.blockOf(address); block <= tracker.blockOf(address + bytes - 1); ++block) {
        if (const auto& leaf = tracker.leaves[block / WriteTracker::LeafBlocks]; leaf != nullptr) leaf->hintBlocks[block % WriteTracker::LeafBlocks] = 0;
        if (const auto found = tracker.driverPieces.find(block); found != tracker.driverPieces.end()) found->second.dropped = std::max<std::uint32_t>(found->second.dropped, tracker.generation);
        tracker.stamp(block, tracker.generation, StampKind::Cpu);
    }
    unwatchSerial.fetch_add(1, std::memory_order_release);
}
#endif

// Collect epochs are per thread and globally unique: every bump takes a fresh value from one counter,
// so a memo entry (stamped with the epoch of its walk) can only match the thread that made it, and
// only until that thread's next ordering point (the queue workers bump at theirs, see the header). A
// thread that never bumped is given a new epoch for every collect: its walks are never reused (the
// presenter, game threads inside the flush hook), as nothing orders the CPU's writes for it.
std::atomic<std::uint64_t> nextCollectEpoch{1};
struct CollectEpochSlot {};
std::atomic<std::uint64_t> lastBumpedEpoch{0};

std::uint64_t currentCollectEpoch() {
    if (const auto epoch = HostThreadSlot<std::uint64_t, CollectEpochSlot>::Get(); epoch != 0) return epoch;
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
ThreadCollectMemo& threadCollectMemo() {
    struct Slot {};
    auto* memo = HostThreadSlot<ThreadCollectMemo*, Slot>::Get();
    if (memo == nullptr) {
        ShaderRecompiler::ThreadOwned(memo);
        HostThreadSlot<ThreadCollectMemo*, Slot>::Set(memo);
    }
    return *memo;
}

struct EpochCollectMap {
    std::uint64_t epoch = 0;
    std::unordered_map<std::uintptr_t, WriteTracker::Memo> ranges;
};

EpochCollectMap& epochCollectMap(std::uint64_t epoch) {
    thread_local EpochCollectMap* slot = nullptr;
    auto& map = ShaderRecompiler::ThreadOwned(slot);
    if (map.epoch != epoch) {
        map.ranges.clear();
        map.epoch = epoch;
    }
    return map;
}

bool epochCollectMapEnabled() {
    static const bool enabled = std::getenv("APS5_NO_EPOCH_COLLECT_MAP") == nullptr;
    return enabled;
}

bool sharedCollectMemo() {
    static const bool shared = std::getenv("APS5_SHARED_COLLECT_MEMO") != nullptr;
    return shared;
}

struct StampRuns {
    WriteTracker& tracker;
    StampKind kind;
    std::size_t stamped = 0;
    const WindowCopy* window = nullptr;
};

void stampWrittenRun(void* context, std::uintptr_t begin, std::uintptr_t end) {
    auto& runs = *static_cast<StampRuns*>(context);
    auto& tracker = runs.tracker;
    const auto kind = runs.kind;
    if (end <= begin) return;
    ++runs.stamped;
    const auto generation = tracker.generation.load(std::memory_order_relaxed);
    if (kind != StampKind::Driver) tracker.noteCpuStore(begin, end, generation);
    else tracker.noteWalkStore(begin, end, generation);
    if (runs.window != nullptr) {
        for (auto page = begin; page < end; page += WritePageBytes) tracker.stamp(tracker.blockOf(page), generation, runs.window->Holds(page) ? StampKind::ImportWindow : kind);
    } else {
        for (auto block = tracker.blockOf(begin); block <= tracker.blockOf(end - 1); ++block) tracker.stamp(block, generation, kind);
    }
#ifndef _WIN32
    collectDirtyRuns.fetch_add(1, std::memory_order_relaxed);
#endif
}

#ifdef _WIN32
std::atomic<FreshCollectHook> freshCollectHook{nullptr};

bool collectWindowsWrites(StampRuns& runs, std::uint64_t first, std::uint64_t stop, bool armedOnly) {
    auto& tracker = runs.tracker;
    const bool fresh = runs.kind == StampKind::Fresh;
    if (fresh) {
        if (const auto hook = freshCollectHook.load(std::memory_order_acquire)) return hook(static_cast<std::uintptr_t>(first), static_cast<std::size_t>(stop - first), &stampWrittenRun, &runs);
    }
    const auto collect = fresh ? &GuestArena::GuestArenaCollectFreshWrites_nid_postfix : armedOnly ? &GuestArena::GuestArenaCollectArmedWrites_nid_postfix : &GuestArena::GuestArenaCollectWrites_nid_postfix;
    static const bool singlePass = std::getenv("APS5_NO_SINGLE_PASS_COLLECT") == nullptr;
    const auto walk = [&](bool clear) {
        auto cursor = first;
        while (cursor < stop) {
            std::size_t count = tracker.pages.size();
            if (!collect(cursor, static_cast<std::size_t>(stop - cursor), tracker.pages.data(), &count, clear)) return false;
            for (std::size_t i = 0; i < count; ++i) {
                const auto page = reinterpret_cast<std::uintptr_t>(tracker.pages[i]);
                stampWrittenRun(&runs, page, page + WritePageBytes);
            }
            if (count < tracker.pages.size()) break;
            cursor = reinterpret_cast<std::uintptr_t>(tracker.pages[count - 1]) + WritePageBytes;
        }
        return true;
    };
    if (!walk(singlePass || fresh)) return false;
    if (runs.stamped != 0) collectDirty.fetch_add(1, std::memory_order_relaxed);
    return fresh || singlePass || runs.stamped == 0 || walk(true);
}
#endif

bool walkWrites(WriteTracker& tracker, std::uint64_t first, std::uint64_t stop, StampKind kind, bool armedOnly = false, const WindowCopy* window = nullptr) {
    ++tracker.generation;
    StampRuns runs{tracker, kind, 0, window};
#ifdef _WIN32
    return collectWindowsWrites(runs, first, stop, armedOnly);
#else
    const auto dirtyRuns = collectDirtyRuns.load(std::memory_order_relaxed);
    const auto collect = armedOnly ? &GuestWriteWatch::GuestWriteWatchCollectArmed_nid_postfix : &GuestWriteWatch::GuestWriteWatchCollect_nid_postfix;
    const bool complete = collect(static_cast<std::uintptr_t>(first), static_cast<std::size_t>(stop - first), &stampWrittenRun, &runs);
    if (collectDirtyRuns.load(std::memory_order_relaxed) != dirtyRuns) collectDirty.fetch_add(1, std::memory_order_relaxed);
    return complete;
#endif
}

std::uint64_t collectWrites(std::uint64_t address, std::size_t bytes, bool memoized) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return 0;
    auto& tracker = Tracker();
    // Whole pages, so a page shared with the next range is collected with either.
    constexpr std::uint64_t page = 4096;
    auto first = address & ~(page - 1);
    auto stop = (address + bytes + page - 1) & ~(page - 1);
    // One epoch for the lookup and the entry made after the walk: an unbumped thread's fresh epoch
    // must not differ between them.
    const auto epoch = currentCollectEpoch();
    const auto unwatched = unwatchSerial.load(std::memory_order_acquire);
    const auto mapping = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const bool useMemo = memoized && collectMemoEnabled() && bytes != 0;
    if (useMemo && !sharedCollectMemo()) {
        // An entry exists only for a completed walk of an in-arena range, so the tracker is
        // initialized and watched; nothing below the lock needs asking.
        if (epochCollectMapEnabled()) {
            const auto& map = epochCollectMap(epoch);
            if (const auto found = map.ranges.find(first); found != map.ranges.end() && found->second.unwatched == unwatched && found->second.mapping == mapping && stop <= found->second.end) {
                collectMemoHits.fetch_add(1, std::memory_order_relaxed);
                return tracker.stable.load(std::memory_order_acquire);
            }
        }
        for (const auto& entry : threadCollectMemo().entries) {
            if (entry.epoch == epoch && entry.unwatched == unwatched && entry.mapping == mapping && entry.begin <= first && stop <= entry.end) {
                // The current generation, not the memoized one: blocks stamped since (MarkWritten, an
                // overlapping collect) were written before this caller reads, and an older value would
                // make every later UnchangedSince fail until the epoch ends.
                collectMemoHits.fetch_add(1, std::memory_order_relaxed);
                return tracker.stable.load(std::memory_order_acquire);
            }
        }
    }
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address || !tracker.covers(address, bytes)) return 0;
    auto cursor = first;
    if (useMemo && sharedCollectMemo()) {
        for (const auto& entry : tracker.memo) {
            if (entry.epoch == epoch && entry.unwatched == unwatchSerial.load(std::memory_order_relaxed) && entry.mapping == mapping && entry.begin <= cursor && stop <= entry.end) {
                collectMemoHits.fetch_add(1, std::memory_order_relaxed);
                return tracker.generation;
            }
        }
    }
    const TimedAccess timed(CounterCollect, bytes);
#ifndef _WIN32
    if (useMemo && HostThreadSlot<std::uint64_t, CollectEpochSlot>::Get() != 0 && bytes <= WriteBlockBytes) {
        const auto begin = first & ~(WriteBlockBytes - 1);
        const auto end = (stop + WriteBlockBytes - 1) & ~(WriteBlockBytes - 1);
        if (end > begin && tracker.covers(begin, end - begin)) {
            first = begin;
            stop = end;
        }
    }
#endif
    if (!walkWrites(tracker, first, stop, StampKind::Cpu)) return 0;
    // Only a completed walk is remembered; a failed one (uncommitted pages) returned 0 above.
    if (collectMemoEnabled()) {
        const auto serial = unwatchSerial.load(std::memory_order_relaxed);
        if (sharedCollectMemo()) tracker.memo[tracker.nextMemo++ % tracker.memo.size()] = {first, stop, epoch, serial, mapping};
        else {
            auto& memo = threadCollectMemo();
            memo.entries[memo.next++ % memo.entries.size()] = {first, stop, epoch, serial, mapping};
            if (epochCollectMapEnabled()) {
                auto& entry = epochCollectMap(epoch).ranges[first];
                if (entry.unwatched != serial || entry.mapping != mapping || entry.end < stop) entry = {first, stop, epoch, serial, mapping};
            }
        }
    }
    return tracker.generation;
}

}

std::uint64_t CollectWrites(std::uint64_t address, std::size_t bytes) {
    return collectWrites(address, bytes, true);
}

std::uint64_t CollectWritesUncached(std::uint64_t address, std::size_t bytes) {
    return collectWrites(address, bytes, false);
}

void BumpCollectEpoch() {
    const auto epoch = nextCollectEpoch.fetch_add(1, std::memory_order_relaxed) + 1;
    HostThreadSlot<std::uint64_t, CollectEpochSlot>::Set(epoch);
    auto last = lastBumpedEpoch.load(std::memory_order_relaxed);
    while (last < epoch && !lastBumpedEpoch.compare_exchange_weak(last, epoch, std::memory_order_release, std::memory_order_relaxed)) {}
    collectEpochBumps.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t ThreadCollectEpoch() {
    return collectMemoEnabled() ? HostThreadSlot<std::uint64_t, CollectEpochSlot>::Get() : 0;
}

std::uint64_t UnwatchSerial() {
    return unwatchSerial.load(std::memory_order_acquire);
}

void AdoptLatestCollectEpoch() {
    const auto latest = lastBumpedEpoch.load(std::memory_order_acquire);
    if (latest > HostThreadSlot<std::uint64_t, CollectEpochSlot>::Get()) HostThreadSlot<std::uint64_t, CollectEpochSlot>::Set(latest);
}

std::uint64_t CollectEpochBumps() {
    return collectEpochBumps.load(std::memory_order_relaxed);
}

namespace {
std::atomic<std::uint64_t> proofSerial{1};
thread_local bool proofThread = false;
}

std::uint64_t ProofSerial() {
    return proofSerial.load(std::memory_order_acquire);
}

void BreakProofs() {
    proofSerial.fetch_add(1, std::memory_order_acq_rel);
}

void MarkProofThread() {
    proofThread = true;
}

std::uint64_t CollectEpoch() {
    return collectMemoEnabled() ? HostThreadSlot<std::uint64_t, CollectEpochSlot>::Get() : 0;
}

namespace {

std::atomic<void (*)(std::uint64_t, std::size_t)> unwatchHook{nullptr};

void beforeUnwatch(std::uint64_t address, std::size_t bytes) {
    const auto hook = unwatchHook.load(std::memory_order_acquire);
    if (hook == nullptr) return;
#ifdef _WIN32
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    {
        auto& tracker = Tracker();
        const auto lock = lockTracker(tracker);
        tracker.initialize();
        if (!tracker.watched || !tracker.coverage.ExcludedSpan(address, bytes, &begin, &end)) return;
    }
    hook(begin, static_cast<std::size_t>(end - begin));
#else
    hook(address, bytes);
#endif
}

void unwatchLocked(WriteTracker& tracker, std::uint64_t address, std::size_t bytes) {
    if (!tracker.watched) return;
#ifdef _WIN32
    tracker.startHints(address, bytes);
    if (tracker.coverage.Exclude(address, bytes, GuestArena::GuestArenaCommitGeneration_nid_postfix())) unwatchSerial.fetch_add(1, std::memory_order_release);
#else
    if (GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<const void*>(address), bytes)) unwatchSerial.fetch_add(1, std::memory_order_release);
#endif
}

}

void SetUnwatchHook(void (*hook)(std::uint64_t, std::size_t)) {
    unwatchHook.store(hook, std::memory_order_release);
}

#ifdef _WIN32
void SetFreshCollectHook(FreshCollectHook hook) {
    freshCollectHook.store(hook, std::memory_order_release);
}
#endif

void Unwatch(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return;
    beforeUnwatch(address, bytes);
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    unwatchLocked(tracker, address, bytes);
}

void Rewatch(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
#ifdef _WIN32
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched) return;
    bool restored = false;
    for (const auto& [begin, end] : ranges) {
        if (end <= begin || begin < tracker.base || end - tracker.base > tracker.size || (begin - tracker.base) % WriteBlockBytes != 0 || (end - tracker.base) % WriteBlockBytes != 0) continue;
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (tracker.coverage.Covers(begin, bytes) || !walkWrites(tracker, begin, end, StampKind::Cpu) || !tracker.coverage.Restore(begin, bytes)) continue;
        ++tracker.generation;
        tracker.noteCpuStore(begin, end, tracker.generation);
        for (auto block = tracker.blockOf(begin); block <= tracker.blockOf(end - 1); ++block) {
            tracker.driverPieces.erase(block);
            tracker.stamp(block, tracker.generation, StampKind::Cpu);
        }
        restored = true;
    }
    if (restored) unwatchSerial.fetch_add(1, std::memory_order_release);
#else
    static_cast<void>(ranges);
#endif
}

bool ImportWatched(std::uint64_t address, std::size_t bytes, const std::function<bool()>& import, bool staysWatched) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return import();
    if (!staysWatched) {
        constexpr std::uint64_t page = 4096;
        const auto first = address & ~(page - 1);
        beforeUnwatch(first, static_cast<std::size_t>(((address + bytes + page - 1) & ~(page - 1)) - first));
    }
    auto& tracker = Tracker();
    auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched) {
        lock.Unlock();
        return import();
    }
    constexpr std::uint64_t page = 4096;
    const auto first = address & ~(page - 1);
    const auto stop = (address + bytes + page - 1) & ~(page - 1);
    const bool covered = tracker.covers(first, static_cast<std::size_t>(stop - first));
    const auto runImport = [&] {
#ifdef __APPLE__
        const GuestArena::HostWrite open(reinterpret_cast<void*>(first), static_cast<std::size_t>(stop - first));
#endif
        return import();
    };
    if (!staysWatched) {
        const bool before = covered && walkWrites(tracker, first, stop, StampKind::Cpu, true);
        if (!runImport()) return false;
        if (!before || !walkWrites(tracker, first, stop, StampKind::ImportWindow, true)) unwatchLocked(tracker, first, static_cast<std::size_t>(stop - first));
        unwatchLocked(tracker, address, bytes);
        return true;
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spans;
    if (covered) spans.emplace_back(first, stop);
    else {
        for (auto begin = first; begin < stop;) {
            const auto end = std::min<std::uint64_t>((begin / WriteBlockBytes + 1) * WriteBlockBytes, stop);
            if (tracker.covers(begin, static_cast<std::size_t>(end - begin))) {
                if (!spans.empty() && spans.back().second == begin) spans.back().second = end;
                else spans.emplace_back(begin, end);
            }
            begin = end;
        }
    }
    std::vector<std::uint64_t> hashes(static_cast<std::size_t>((stop - first) / page), 0);
    for (const auto& [begin, end] : spans) {
        CheckRange(reinterpret_cast<const void*>(begin), static_cast<std::size_t>(end - begin), 1, false);
        for (auto cursor = begin; cursor < end; cursor += page) {
            hashes[static_cast<std::size_t>((cursor - first) / page)] = HashPage(reinterpret_cast<const std::byte*>(cursor));
        }
    }
    std::erase_if(spans, [&](const auto& span) { return !walkWrites(tracker, span.first, span.second, StampKind::Cpu, true); });
    if (!runImport()) return false;
    const WindowCopy window{first, hashes};
    auto unwatchedFrom = first;
    for (const auto& [begin, end] : spans) {
        if (!walkWrites(tracker, begin, end, StampKind::Cpu, true, &window)) continue;
        if (unwatchedFrom < begin) unwatchLocked(tracker, unwatchedFrom, static_cast<std::size_t>(begin - unwatchedFrom));
        unwatchedFrom = end;
    }
    if (unwatchedFrom < stop) unwatchLocked(tracker, unwatchedFrom, static_cast<std::size_t>(stop - unwatchedFrom));
    return true;
}

bool WriteWatched() {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    return tracker.watched;
}

bool Watched(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    return tracker.watched && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address && tracker.covers(address, bytes);
}

bool UnchangedSince(std::uint64_t address, std::size_t bytes, std::uint64_t generation) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || generation == 0 || bytes == 0 || !tracker.covers(address, bytes)) return false;
    return tracker.rangeUnchanged(address, bytes, generation);
}

bool UnchangedSinceAll(std::span<const UnchangedQuery> queries) {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched) return false;
    for (const auto& [address, bytes, generation] : queries) {
        if (generation == 0 || bytes == 0 || !tracker.covers(address, bytes)) return false;
        if (!tracker.rangeUnchanged(address, bytes, generation)) return false;
    }
    return true;
}

std::uint64_t CollectCommittedWrites(std::uint64_t address, std::size_t bytes, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) {
    ranges.clear();
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address || !WriteWatched()) return 0;
    auto committed = CommittedRanges(address, bytes);
    auto generation = TrackerGeneration();
    for (const auto& [begin, end] : committed) {
        generation = CollectWrites(begin, static_cast<std::size_t>(end - begin));
        if (generation == 0) return 0;
    }
    ranges = std::move(committed);
    return generation;
}

bool UnchangedCommittedSince(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges, std::uint64_t generation) {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || generation == 0) return false;
    for (const auto& [begin, end] : ranges) {
        if (end <= begin || !tracker.covers(begin, static_cast<std::size_t>(end - begin)) || tracker.newestStamp(begin, end) > generation) return false;
    }
    return true;
}

std::uint64_t storeOwn(std::uint64_t address, std::size_t bytes, const std::function<std::pair<std::uint64_t, std::uint64_t>()>& store) {
    if (bytes == 0) return 0;
    BreakProofs();
    const auto stampStored = [](WriteTracker& tracker, std::pair<std::uint64_t, std::uint64_t> stored) -> std::uint64_t {
        if (stored.second <= stored.first) return 0;
        ++tracker.generation;
        if (!tracker.watched || !tracker.covers(stored.first, static_cast<std::size_t>(stored.second - stored.first))) {
            tracker.noteUnstampedStore(stored.first, stored.second);
            if (tracker.watched) tracker.stampWatchedPart(stored.first, stored.second, tracker.generation, true);
            return 0;
        }
        for (auto block = tracker.blockOf(stored.first); block <= tracker.blockOf(stored.second - 1); ++block) {
            tracker.stamp(block, tracker.generation, StampKind::Driver);
            tracker.noteDriverStore(block, stored.first, stored.second, tracker.generation, true);
        }
        return tracker.generation;
    };
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || bytes > std::numeric_limits<std::uint64_t>::max() - address || !tracker.covers(address, bytes)) return stampStored(tracker, store());
    constexpr std::uint64_t page = 4096;
    const auto first = address & ~(page - 1);
    const auto stop = (address + bytes + page - 1) & ~(page - 1);
    if (!walkWrites(tracker, first, stop, StampKind::Cpu)) return stampStored(tracker, store());
    const auto stored = store();
    walkWrites(tracker, first, stop, StampKind::Driver);
    return stampStored(tracker, stored);
}

std::uint64_t StoreOwnBytes(std::uint64_t address, std::size_t bytes, const std::function<void()>& store) {
    return storeOwn(address, bytes, [&] {
        store();
        return std::pair<std::uint64_t, std::uint64_t>{address, address + bytes};
    });
}

std::uint64_t StoreOwnBytes(std::uint64_t address, std::span<const std::byte> source) {
    return StoreOwnBytes(address, source.size(), [&] {
#ifndef _WIN32
        if (GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix() && GuestArena::GuestArenaWriteSharedBacking_nid_no_patch(address, source.data(), source.size())) return;
#endif
        std::memcpy(reinterpret_cast<void*>(address), source.data(), source.size());
    });
}

static std::uint64_t markWritten(std::uint64_t address, std::size_t bytes, bool complete) {
    BreakProofs();
    auto& tracker = Tracker();
    auto lock = lockTracker(tracker);
    tracker.initialize();
    if (bytes == 0) return 0;
    ++tracker.generation;
    if (!tracker.watched || !tracker.covers(address, bytes)) {
        tracker.noteUnstampedStore(address, address + bytes);
        if (tracker.watched) tracker.stampWatchedPart(address, address + bytes, tracker.generation, complete);
        return 0;
    }
    {
        const auto blockFirst = address & ~(std::uint64_t{WriteBlockBytes} - 1);
        const auto blockStop = (address + bytes + WriteBlockBytes - 1) & ~(std::uint64_t{WriteBlockBytes} - 1);
        StampRuns runs{tracker, StampKind::Fresh};
#ifdef _WIN32
        const bool collected = collectWindowsWrites(runs, blockFirst, blockStop, false);
#else
        const bool collected = GuestWriteWatch::GuestWriteWatchCollectFresh_nid_postfix(static_cast<std::uintptr_t>(blockFirst), static_cast<std::size_t>(blockStop - blockFirst), &stampWrittenRun, &runs);
#endif
        if (!collected) {
            lock.Suspend();
            beforeUnwatch(blockFirst, static_cast<std::size_t>(blockStop - blockFirst));
            lock.Resume();
            unwatchLocked(tracker, blockFirst, static_cast<std::size_t>(blockStop - blockFirst));
            tracker.noteUnstampedStore(address, address + bytes);
            return 0;
        }
        if (runs.stamped != 0) ++tracker.generation;
    }
    const auto first = tracker.blockOf(address);
    const auto last = tracker.blockOf(address + bytes - 1);
    for (auto block = first; block <= last; ++block) {
        tracker.stamp(block, tracker.generation, StampKind::Driver);
        tracker.noteDriverStore(block, address, address + bytes, tracker.generation, complete);
    }
    return tracker.generation;
}

std::uint64_t MarkWritten(std::uint64_t address, std::size_t bytes) {
    return markWritten(address, bytes, true);
}

std::uint64_t MarkPossiblyWritten(std::uint64_t address, std::size_t bytes) {
    return markWritten(address, bytes, false);
}

bool StoredOver(std::uint64_t address, std::size_t bytes, std::uint64_t generation) {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || generation == 0 || bytes == 0 || !tracker.covers(address, bytes)) return true;
#ifdef _WIN32
    if (GuestArena::GuestArenaHostRegionOverlaps_nid_postfix(address, bytes)) return true;
#endif
    return tracker.storedOver(address, address + bytes, generation, false);
}

bool StoredAllSince_nid_no_patch(std::uint64_t address, std::size_t bytes, std::uint64_t generation) {
    if (generation == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || !tracker.covers(address, bytes)) return false;
    const auto end = address + bytes;
    if (!tracker.unstampedStores.empty()) {
        const auto first = address / WriteBlockBytes;
        const auto last = (end - 1u) / WriteBlockBytes;
        if (last - first >= WriteTracker::UnstampedSlots) {
            if (std::ranges::any_of(tracker.unstampedStores, [&](auto stamp) { return stamp > generation; })) return false;
        } else {
            for (auto block = first; block <= last; ++block) if (tracker.unstampedStores[block % WriteTracker::UnstampedSlots] > generation) return false;
        }
    }
    for (auto block = tracker.blockOf(address); block <= tracker.blockOf(end - 1u); ++block) {
        const auto found = tracker.driverPieces.find(block);
        if (found == tracker.driverPieces.end()) return false;
        const auto start = tracker.blockBegin(block);
        const auto from = static_cast<std::uint32_t>(std::max(address, start) - start);
        const auto to = static_cast<std::uint32_t>(std::min<std::uint64_t>(end, start + WriteBlockBytes) - start);
        if (!found->second.covers(from, to, generation)) return false;
    }
    return true;
}

std::uint64_t TrackerGeneration() {
    auto& tracker = Tracker();
    // Under the mutex: a collect bumps the generation before its walk, and a walk still running
    // when the value is read absorbs stores made after it with a stamp not newer than the value.
    const auto lock = lockTracker(tracker);
    return tracker.generation;
}

std::uint64_t NewestStamp(std::uint64_t address, std::size_t bytes) {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    return bytes == 0 ? 0 : tracker.newestStamp(address, address + bytes);
}

#ifdef _WIN32
namespace {

thread_local ThreadCollectMemo threadHintMemo;

bool walkHints(WriteTracker& tracker, std::uint64_t first, std::uint64_t stop) {
    auto cursor = first;
    bool marked = false;
    while (cursor < stop) {
        std::size_t count = tracker.pages.size();
        if (!GuestArena::GuestArenaCollectWrites_nid_postfix(static_cast<std::uintptr_t>(cursor), static_cast<std::size_t>(stop - cursor), tracker.pages.data(), &count, true)) return false;
        if (count != 0 && !marked) ++tracker.generation;
        marked = marked || count != 0;
        for (std::size_t i = 0; i < count; ++i) tracker.stampHint(tracker.blockOf(reinterpret_cast<std::uintptr_t>(tracker.pages[i])));
        if (count < tracker.pages.size()) break;
        cursor = reinterpret_cast<std::uintptr_t>(tracker.pages[count - 1]) + WritePageBytes;
    }
    if (marked) collectDirty.fetch_add(1, std::memory_order_relaxed);
    return true;
}

}
#endif

std::uint64_t UntrackedHint(std::uint64_t address, std::size_t bytes) {
#ifdef _WIN32
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return 0;
    constexpr std::uint64_t page = 4096;
    const auto first = address & ~(page - 1);
    const auto stop = (address + bytes + page - 1) & ~(page - 1);
    const auto epoch = currentCollectEpoch();
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || !tracker.hinted(first, stop)) return 0;
    const auto unwatched = unwatchSerial.load(std::memory_order_relaxed);
    const auto mapping = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const bool memoized = collectMemoEnabled() && std::any_of(threadHintMemo.entries.begin(), threadHintMemo.entries.end(), [&](const WriteTracker::Memo& entry) {
        return entry.epoch == epoch && entry.unwatched == unwatched && entry.mapping == mapping && entry.begin <= first && stop <= entry.end;
    });
    if (memoized) {
        collectMemoHits.fetch_add(1, std::memory_order_relaxed);
    } else {
        const TimedAccess timed(CounterCollect, bytes);
        if (!walkHints(tracker, first, stop)) return 0;
        if (collectMemoEnabled()) threadHintMemo.entries[threadHintMemo.next++ % threadHintMemo.entries.size()] = {first, stop, epoch, unwatched, mapping};
    }
    return tracker.newestHint(first, stop);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    return 0;
#endif
}

bool UnchangedSinceCollected(std::uint64_t address, std::size_t bytes, std::uint64_t generation) {
    if (CollectWritesUncached(address, bytes) == 0) return false;
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    if (!tracker.watched || generation == 0 || bytes == 0 || !tracker.covers(address, bytes)) return false;
    const auto first = tracker.blockOf(address);
    const auto last = tracker.blockOf(address + bytes - 1);
    const auto end = address + bytes;
    for (auto block = first; block <= last; ++block) {
        if (const auto found = tracker.cpuPages.find(block); found != tracker.cpuPages.end()) {
            const auto begin = tracker.blockBegin(block);
            const auto from = std::max(address, begin) - begin;
            const auto to = std::min<std::uint64_t>(end, begin + WriteBlockBytes) - begin;
            for (auto page = from / WritePageBytes; page <= (to - 1) / WritePageBytes; ++page) {
                if (found->second.pages[page] > generation) return false;
            }
        } else if (tracker.cpuStampOf(block) > generation) {
            return false;
        }
    }
    return true;
}

bool WrittenSince(std::uint64_t address, std::size_t bytes, std::uint64_t generation) {
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || generation == 0 || bytes == 0 || !tracker.covers(address, bytes)) return false;
    const auto first = tracker.blockOf(address);
    const auto last = tracker.blockOf(address + bytes - 1);
    for (auto block = first; block <= last; ++block) {
        if (tracker.stampOf(block) > generation) return true;
    }
    return false;
}

bool ChangedBlocks(std::uint64_t address, std::size_t bytes, std::span<const std::uint64_t> generations, std::span<std::uint8_t> changed, std::span<std::uint8_t> cpu) {
    std::fill(changed.begin(), changed.end(), BlockWritten);
    std::fill(cpu.begin(), cpu.end(), std::uint8_t{1});
    auto& tracker = Tracker();
    const auto lock = lockTracker(tracker);
    tracker.initialize();
    if (!tracker.watched || bytes == 0 || !tracker.covers(address, bytes)) return false;
    const auto first = tracker.blockOf(address);
    const auto last = tracker.blockOf(address + bytes - 1);
    for (auto block = first; block <= last; ++block) {
        const auto k = block - first;
        if (k >= generations.size() || k >= changed.size()) break;
        const auto generation = generations[k];
        changed[k] = generation == 0 || tracker.stampOf(block) > generation ? BlockWritten : BlockUnchanged;
        if (k < cpu.size()) cpu[k] = generation == 0 || tracker.cpuStampOf(block) > generation ? 1 : 0;
    }
    return true;
}

namespace {

// Per-thread GpuMutex wait accounting (APS5_PROFILE_DRAW): a queue worker tags its thread, other
// threads (the presenter) report untagged.
constexpr std::size_t GpuLockSiteCount = static_cast<std::size_t>(GpuLockSite::Count);
constexpr const char* GpuLockSiteNames[GpuLockSiteCount] = {"other", "dispatch", "indirect", "draw", "hook", "wait", "label", "flush", "present", "fill", "copy", "end", "try"};

// Hold-side holder matrix: nanoseconds held per (holder thread, site), added by EndHold. Column 0
// is every untagged thread (the presenter, game threads); the queue workers take a column each on
// their first hold. A contended lock() snapshots the matrix before and after its wait and credits
// the deltas (the holds that ended during the wait) to the waiter's site.
constexpr std::size_t HolderColumns = 12;
constexpr std::uint32_t UnassignedHolder = 0xfffffffeu;
constexpr std::uint32_t PresenterHolderTag = 0xfffffffdu;
constexpr int PresenterColumn = 1;
using HolderMatrixValues = std::array<std::array<std::uint64_t, GpuLockSiteCount>, HolderColumns>;
struct HolderMatrix {
    std::array<std::atomic<std::uint32_t>, HolderColumns> tags{};
    std::array<std::array<std::atomic<std::uint64_t>, GpuLockSiteCount>, HolderColumns> heldNs{};
    HolderMatrix() {
        for (auto& tag : tags) tag.store(UnassignedHolder, std::memory_order_relaxed);
        tags[0].store(0xffffffffu, std::memory_order_relaxed);
        tags[PresenterColumn].store(PresenterHolderTag, std::memory_order_relaxed);
    }
    void Snapshot(HolderMatrixValues& into) const {
        for (std::size_t h = 0; h < HolderColumns; ++h) {
            for (std::size_t s = 0; s < GpuLockSiteCount; ++s) into[h][s] = heldNs[h][s].load(std::memory_order_relaxed);
        }
    }
};

HolderMatrix& Holders() {
    static HolderMatrix matrix;
    return matrix;
}

std::size_t HolderColumnFor(std::uint32_t tag) {
    if (tag == 0xffffffffu) return 0;
    auto& holders = Holders();
    for (std::size_t i = 1; i < HolderColumns; ++i) {
        auto expected = UnassignedHolder;
        if (holders.tags[i].load(std::memory_order_acquire) == tag) return i;
        if (holders.tags[i].compare_exchange_strong(expected, tag, std::memory_order_acq_rel)) return i;
        if (expected == tag) return i;
    }
    return 0;
}

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
    // This thread's column of the holder matrix (-1 until its first hold), and what its contended
    // acquisitions waited behind: holds by (holder, site) that ended during the waits.
    int holderColumn = -1;
    HolderMatrixValues waitedBehindNs{};
    std::uint64_t tries = 0;
    std::uint64_t triesFailed = 0;
    std::uint64_t lockedGpuWaits = 0;
    double lockedGpuWaitMs = 0;
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
    const auto heldNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - stats.holdStart).count();
    const auto heldMs = static_cast<double>(heldNs) / 1e6;
    ++stats.siteHolds[stats.holdSite];
    stats.siteHeldMs[stats.holdSite] += heldMs;
    stats.siteMaxHeldMs[stats.holdSite] = std::max(stats.siteMaxHeldMs[stats.holdSite], heldMs);
    if (stats.holderColumn < 0) stats.holderColumn = static_cast<int>(HolderColumnFor(stats.tag));
    Holders().heldNs[static_cast<std::size_t>(stats.holderColumn)][stats.holdSite].fetch_add(static_cast<std::uint64_t>(heldNs), std::memory_order_relaxed);
}

const char* HolderName(std::size_t column, char* text, std::size_t size) {
    const auto tag = Holders().tags[column].load(std::memory_order_relaxed);
    if (tag == 0xffffffffu) std::snprintf(text, size, "untagged");
    else if (tag == PresenterHolderTag) std::snprintf(text, size, "presenter");
    else std::snprintf(text, size, "q0x%x", tag);
    return text;
}

// The waiter's matrix as text, largest first, entries of at least a millisecond.
std::string WaitedBehindReport(const HolderMatrixValues& waited) {
    std::vector<std::tuple<std::uint64_t, std::size_t, std::size_t>> entries;
    for (std::size_t h = 0; h < HolderColumns; ++h) {
        for (std::size_t s = 0; s < GpuLockSiteCount; ++s) {
            if (waited[h][s] >= 1000000) entries.emplace_back(waited[h][s], h, s);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
    std::string report;
    char text[96];
    char holder[24];
    for (std::size_t i = 0; i < entries.size() && i < 16; ++i) {
        const auto& [ns, h, s] = entries[i];
        std::snprintf(text, sizeof(text), " %s/%s %.0f", HolderName(h, holder, sizeof(holder)), GpuLockSiteNames[s], static_cast<double>(ns) / 1e6);
        report += text;
    }
    return report;
}

}

namespace {
std::atomic<std::uint64_t> gpuHoldSerial{0};
}

void GpuMutexType::enter() {
    if (!HeldByThisThread()) LockSpinning(mutex, HandoffSpinWindow());
}

bool GpuMutexType::tryEnter() {
    return HeldByThisThread() || mutex.try_lock();
}

void GpuMutexType::acquired() {
    owner.store(CurrentThreadToken(), std::memory_order_relaxed);
    if (++depth == 1) {
        gpuHoldSerial.fetch_add(1, std::memory_order_relaxed);
        if (!proofThread) BreakProofs();
    }
}

bool GpuMutexType::try_lock() {
    if (!GpuLockProfiled()) {
        if (!tryEnter()) return false;
        acquired();
        return true;
    }
    // The tag is consumed whether or not the try succeeds (a failed try must not name the next
    // acquisition); an untagged try's hold is charged to 'try' (a boundary reap, an idle worker).
    auto& stats = LockStats();
    const auto tagged = std::exchange(stats.nextSite, GpuLockSite::Other);
    ++stats.tries;
    if (!tryEnter()) {
        ++stats.triesFailed;
        return false;
    }
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

std::uint64_t GpuHoldSerial() {
    return gpuHoldSerial.load(std::memory_order_relaxed);
}

void GpuMutexType::unlock() {
    const bool outermost = --depth == 0;
    if (outermost) {
        owner.store(nullptr, std::memory_order_relaxed);
        // Still the holder here: the hold ends when the mutex is given up, not before.
        if (GpuHoldProfiled()) EndHold();
        mutex.unlock();
    }
    // After the release, so what the hook does (destroying a batch's kept objects) is neither part
    // of the hold nor waited for by the next holder.
    if (outermost) {
        if (auto* hook = gpuUnlockHook.load(std::memory_order_acquire); hook != nullptr) hook();
    }
}

bool GpuMutexType::HeldByThisThread() const {
    return owner.load(std::memory_order_relaxed) == CurrentThreadToken();
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
        auto* trace = PerformanceTrace::Recording() ? PerformanceTrace::Thread() : nullptr;
        if (trace == nullptr) {
            enter();
        } else {
            const auto site = static_cast<std::size_t>(std::exchange(LockStats().nextSite, GpuLockSite::Other));
            if (!tryEnter()) {
                const auto start = std::chrono::steady_clock::now();
                LockSpinning(mutex, HandoffSpinWindow());
                trace->Add("GpuMutex.wait", GpuLockSiteNames[site], start, std::chrono::steady_clock::now());
            }
        }
        acquired();
        return;
    }
    auto& stats = LockStats();
    ++stats.acquisitions;
    const auto site = static_cast<std::size_t>(std::exchange(stats.nextSite, GpuLockSite::Other));
    ++stats.siteAcquisitions[site];
    // An uncontended (or recursive) acquisition costs no clock reads for the wait; only a wait is
    // timed. The hold timer (outermost acquisitions only) starts once the mutex is held.
    if (!tryEnter()) {
        const bool holders = GpuHoldProfiled();
        HolderMatrixValues before{};
        if (holders) Holders().Snapshot(before);
        const auto start = std::chrono::steady_clock::now();
        LockSpinning(mutex, HandoffSpinWindow());
        acquired();
        const auto now = std::chrono::steady_clock::now();
        if (auto* trace = PerformanceTrace::Recording() ? PerformanceTrace::Thread() : nullptr) trace->Add("GpuMutex.wait", GpuLockSiteNames[site], start, now);
        if (holders) {
            HolderMatrixValues after{};
            Holders().Snapshot(after);
            for (std::size_t h = 0; h < HolderColumns; ++h) {
                for (std::size_t s = 0; s < GpuLockSiteCount; ++s) stats.waitedBehindNs[h][s] += after[h][s] - before[h][s];
            }
        }
        if (depth == 1 && holders) {
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
            holds += "; waited behind (holder/site ms):" + WaitedBehindReport(stats.waitedBehindNs);
        }
        char counts[160];
        std::snprintf(counts, sizeof(counts), "; lock() %llu / try %llu / try failed %llu; locked GPU waits %llu / %.0f ms", static_cast<unsigned long long>(stats.acquisitions), static_cast<unsigned long long>(stats.tries), static_cast<unsigned long long>(stats.triesFailed), static_cast<unsigned long long>(stats.lockedGpuWaits), stats.lockedGpuWaitMs);
        holds += counts;
        if (stats.tag == 0xffffffffu) AgcDriver::ProfilePrint_nid_no_patch("[lock] %s waited %.0f ms for the GPU mutex in %llu of %llu acquisitions (%.0f s); by site (acquisitions/waits, waited):%s%s\n", stats.holderColumn == PresenterColumn ? "presenter" : "untagged thread", stats.waitedMs, static_cast<unsigned long long>(stats.waits), static_cast<unsigned long long>(stats.acquisitions), interval, sites.c_str(), holds.c_str());
        else AgcDriver::ProfilePrint_nid_no_patch("[lock] queue 0x%x waited %.0f ms for the GPU mutex in %llu of %llu acquisitions (%.0f s); by site (acquisitions/waits, waited):%s%s\n", stats.tag, stats.waitedMs, static_cast<unsigned long long>(stats.waits), static_cast<unsigned long long>(stats.acquisitions), interval, sites.c_str(), holds.c_str());
        stats.waitedMs = 0;
        stats.waits = 0;
        stats.acquisitions = 0;
        stats.siteAcquisitions.fill(0);
        stats.siteWaits.fill(0);
        stats.siteWaitedMs.fill(0);
        stats.siteHolds.fill(0);
        stats.siteHeldMs.fill(0);
        stats.siteMaxHeldMs.fill(0);
        for (auto& row : stats.waitedBehindNs) row.fill(0);
        stats.tries = 0;
        stats.triesFailed = 0;
        stats.lockedGpuWaits = 0;
        stats.lockedGpuWaitMs = 0;
        return;
    }
    acquired();
    if (depth == 1 && GpuHoldProfiled()) BeginHold(site);
}

void TagGpuLockSite(GpuLockSite site) {
    // Off the profiled path the tag is never read, so the thread-local is not touched either.
    if (!GpuLockProfiled() && !PerformanceTrace::Recording()) return;
    LockStats().nextSite = site;
}

void MarkPresenterThread() {
    LockStats().holderColumn = PresenterColumn;
}

void NoteLockedGpuWait(double ms) {
    if (!GpuLockProfiled() || GpuMutex().DepthOnThisThread() == 0) return;
    auto& stats = LockStats();
    ++stats.lockedGpuWaits;
    stats.lockedGpuWaitMs += ms;
}

std::uint64_t ThreadCollectedBytes() {
    if (!MemoryProfiled()) return 0;
    return CounterFor(CounterCollect).bytes.load(std::memory_order_relaxed);
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
std::atomic<void (*)(std::uint64_t, std::size_t)> readGuard{nullptr};

void PrepareHostRead(std::uintptr_t address, std::size_t bytes) {
    if (const auto guard = readGuard.load(std::memory_order_acquire)) guard(address, bytes);
    FlushGpuWrites(address, bytes);
}
}

void SetFlushHook(void (*hook)(std::uint64_t, std::size_t)) {
    flushHook.store(hook, std::memory_order_release);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(&PrepareHostRead);
}

void SetReadGuard(void (*guard)(std::uint64_t address, std::size_t bytes)) {
    readGuard.store(guard, std::memory_order_release);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(&PrepareHostRead);
}

void GuardRead(std::uint64_t address, std::size_t bytes) {
    if (const auto guard = readGuard.load(std::memory_order_acquire)) guard(address, bytes);
}

void FlushGpuWrites(std::uint64_t address, std::size_t bytes) {
    if (const auto hook = flushHook.load(std::memory_order_acquire)) hook(address, bytes);
}

void Read(std::uint64_t address, std::span<std::byte> destination, std::size_t alignment) {
    if (destination.empty()) return;
    GuardRead(address, destination.size());
    FlushGpuWrites(address, destination.size());
    const TimedAccess timed(CounterRead, destination.size());
    CountReadCaller(__builtin_return_address(0));
    const auto* source = reinterpret_cast<const void*>(address);
    try {
        CheckRange(source, destination.size(), alignment);
    } catch (const std::runtime_error&) {
        static const bool trace = std::getenv("APS5_TRACE_UNREADABLE") != nullptr;
        if (trace) std::fprintf(stderr, "[unreadable]   read from +0x%llx\n", ModuleOffset(__builtin_return_address(0)));
        throw;
    }
    CopyGuestBytes(destination.data(), source, destination.size());
}

#ifdef ANYPS5_ENABLE_ASAN
[[gnu::no_sanitize("address")]] void CopyGuestBytes(void* destination, const void* source, std::size_t bytes) {
    auto* to = static_cast<unsigned char*>(destination);
    const auto* from = static_cast<const unsigned char*>(source);
    std::size_t index = 0;
    for (; index + sizeof(std::uint64_t) <= bytes; index += sizeof(std::uint64_t)) {
        const std::uint64_t word = *reinterpret_cast<const volatile std::uint64_t*>(from + index);
        std::memcpy(to + index, &word, sizeof(word));
    }
    for (; index < bytes; ++index) to[index] = static_cast<const volatile unsigned char*>(from)[index];
}
#else
void CopyGuestBytes(void* destination, const void* source, std::size_t bytes) {
    std::memcpy(destination, source, bytes);
}
#endif

namespace {

void copyCommitted(std::uint64_t address, std::span<std::byte> destination) {
    if (Accessible(reinterpret_cast<const void*>(address), destination.size())) {
        std::memcpy(destination.data(), reinterpret_cast<const void*>(address), destination.size());
        return;
    }
    std::memset(destination.data(), 0, destination.size());
    for (const auto& [begin, end] : CommittedRanges(address, destination.size())) std::memcpy(destination.data() + (begin - address), reinterpret_cast<const void*>(begin), static_cast<std::size_t>(end - begin));
}

}

void ReadCommitted(std::uint64_t address, std::span<std::byte> destination) {
    if (destination.empty()) return;
    GuardRead(address, destination.size());
    FlushGpuWrites(address, destination.size());
    const TimedAccess timed(CounterRead, destination.size());
    CountReadCaller(__builtin_return_address(0));
    copyCommitted(address, destination);
}

void ReadCommittedUnsynced(std::uint64_t address, std::span<std::byte> destination) {
    if (destination.empty()) return;
    GuardRead(address, destination.size());
    const TimedAccess timed(CounterRead, destination.size());
    CountReadCaller(__builtin_return_address(0));
    copyCommitted(address, destination);
}

bool EqualsCommitted(std::uint64_t address, std::span<const std::byte> bytes) {
    if (bytes.empty()) return true;
    FlushGpuWrites(address, bytes.size());
    return EqualsCommittedUnsynced(address, bytes);
}

bool EqualsCommittedUnsynced(std::uint64_t address, std::span<const std::byte> bytes) {
    if (bytes.empty()) return true;
    const TimedAccess timed(CounterCompare, bytes.size());
    if (Accessible(reinterpret_cast<const void*>(address), bytes.size())) return std::memcmp(reinterpret_cast<const void*>(address), bytes.data(), bytes.size()) == 0;
    for (const auto& [begin, end] : CommittedRanges(address, bytes.size())) {
        if (std::memcmp(reinterpret_cast<const void*>(begin), bytes.data() + (begin - address), static_cast<std::size_t>(end - begin)) != 0) return false;
    }
    return true;
}

Compare CompareMapped(std::uint64_t address, std::span<const std::byte> bytes) {
    if (bytes.empty()) return Compare::Equal;
    if (address == 0 || bytes.size() > std::numeric_limits<std::uintptr_t>::max() - address) return Compare::Unmapped;
    auto outcome = Compare::Equal;
    // Each run's page state is consulted right before its bytes are compared.
    const bool queried = describePages(static_cast<std::uintptr_t>(address), bytes.size(), [&](const PageRun& run) {
        if (!run.readable) outcome = Compare::Unmapped;
        else if (std::memcmp(reinterpret_cast<const void*>(run.begin), bytes.data() + (run.begin - address), static_cast<std::size_t>(run.end - run.begin)) != 0) outcome = Compare::Differs;
        return outcome == Compare::Equal;
    });
    return queried ? outcome : Compare::Unmapped;
}

Compare CopyMapped(std::uint64_t address, std::span<std::byte> out) {
    if (out.empty()) return Compare::Equal;
    if (address == 0 || out.size() > std::numeric_limits<std::uintptr_t>::max() - address) return Compare::Unmapped;
    auto outcome = Compare::Equal;
    const bool queried = describePages(static_cast<std::uintptr_t>(address), out.size(), [&](const PageRun& run) {
        if (!run.readable) outcome = Compare::Unmapped;
        else std::memcpy(out.data() + (run.begin - address), reinterpret_cast<const void*>(run.begin), static_cast<std::size_t>(run.end - run.begin));
        return outcome == Compare::Equal;
    });
    return queried ? outcome : Compare::Unmapped;
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
    BreakProofs();
    if (current.empty()) return;
    // A store: the hook sync it makes orders this write after a pending GPU write, whatever a read
    // site above it was doing (the [hooksync] line counts stores apart).
    const ReadSiteScope site(ReadSite::Store);
    FlushGpuWrites(address, current.size());
    const TimedAccess timed(CounterChangedWrite, current.size());
    auto* destination = reinterpret_cast<std::byte*>(address);
    CheckRange(destination, current.size(), 1, true);
    const auto size = current.size();
    // Stamped like a GPU write: a collect memoized for this packet would not see the page fault.
    storeOwn(address, size, [&] {
        std::size_t firstChanged = size;
        std::size_t lastChanged = 0;
        std::size_t at = 0;
        for (; at + 16 <= size; at += 16) {
            const auto bytes = _mm_loadu_si128(reinterpret_cast<const __m128i*>(current.data() + at));
            const auto same = _mm_cmpeq_epi8(bytes, _mm_loadu_si128(reinterpret_cast<const __m128i*>(original.data() + at)));
            const auto equal = static_cast<std::uint32_t>(_mm_movemask_epi8(same));
            if (equal == 0xffffu) continue;
            if (equal == 0) _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + at), bytes);
            else _mm_maskmoveu_si128(bytes, _mm_xor_si128(same, _mm_set1_epi8(-1)), reinterpret_cast<char*>(destination + at));
            const auto changed = ~equal & 0xffffu;
            firstChanged = std::min(firstChanged, at + static_cast<std::size_t>(std::countr_zero(changed)));
            lastChanged = std::max(lastChanged, at + 32 - static_cast<std::size_t>(std::countl_zero(changed)));
        }
        _mm_sfence();
        for (; at < size; ++at) {
            if (current[at] == original[at]) continue;
            destination[at] = current[at];
            firstChanged = std::min(firstChanged, at);
            lastChanged = std::max(lastChanged, at + 1);
        }
        return firstChanged < lastChanged ? std::pair<std::uint64_t, std::uint64_t>{address + firstChanged, address + lastChanged} : std::pair<std::uint64_t, std::uint64_t>{0, 0};
    });
}

void Write(std::uint64_t address, std::span<const std::byte> source, std::size_t alignment) {
    if (source.empty()) return;
    // A store, as in WriteChanged.
    const ReadSiteScope site(ReadSite::Store);
    FlushGpuWrites(address, source.size());
    const TimedAccess timed(CounterWrite, source.size());
    CountWriteCaller(__builtin_return_address(0));
    auto* destination = reinterpret_cast<void*>(address);
    CheckRange(destination, source.size(), alignment, true);
    // Stamped like a GPU write: a collect memoized for this packet would not see the page fault.
    if (source.size() >= PageBytes) StoreOwnBytes(address, source);
    else StoreOwnBytes(address, source.size(), [&] { std::memcpy(destination, source.data(), source.size()); });
}

}

extern "C" void AgcDriverCheckGuestMemory_nid_postfix(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    AgcDriver::GuestMemory::CheckRange(pointer, bytes, alignment, writable);
}

extern "C" void AgcDriverStoreGuestLabel_nid_postfix(std::uint64_t* pointer, std::uint64_t value) {
    AgcDriver::GuestMemory::CheckRange(pointer, sizeof(*pointer), std::atomic_ref<std::uint64_t>::required_alignment, true);
    AgcDriver::GuestMemory::StoreOwnBytes(reinterpret_cast<std::uintptr_t>(pointer), sizeof(*pointer), [&] {
        std::atomic_ref(*pointer).store(value, std::memory_order_release);
    });
}
