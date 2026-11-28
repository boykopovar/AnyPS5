#include "prx/libc/include/SyncTrace.hpp"
#include "prx/libc/include/LogFile.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <pthread.h>
#else
#include <sys/syscall.h>
#endif
#endif
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::size_t RingCapacity = 65536;
constexpr std::size_t NameCapacity = 4096;
constexpr std::size_t NameBytes = 56;
constexpr std::size_t NameProbeLimit = 8;

struct RingSlot {
    std::atomic<std::uint64_t> sequence{0};
    std::atomic<std::uint64_t> micros{0};
    std::atomic<std::uint64_t> thread{0};
    std::atomic<std::uint64_t> object{0};
    std::atomic<std::uint64_t> value{0};
    std::atomic<std::uint64_t> result{0};
    std::atomic<std::uint32_t> operation{0};
};

struct NameSlot {
    std::atomic<std::uint64_t> object{0};
    std::atomic<unsigned> ready{0};
    char text[NameBytes]{};
};

struct Valid {
    std::uint64_t sequence;
    std::uint64_t micros;
    std::uint64_t thread;
    std::uint64_t object;
    std::uint64_t value;
    std::uint64_t result;
    std::uint32_t operation;
};

RingSlot g_ring[RingCapacity];
NameSlot g_names[NameCapacity];
Valid g_valids[RingCapacity];
std::atomic<std::uint64_t> g_sequence{0};
std::atomic<std::uint64_t> g_filteredObject{0};

unsigned ParseMode() {
    const char* value = std::getenv("APS5_TRACE_SYNC");
    if (value == nullptr) return 0;
    if (std::strcmp(value, "ring") == 0) return 1;
    if (std::strncmp(value, "ring:", 5) == 0 && value[5] != '\0') return 2;
    return 0;
}

unsigned RingMode() {
    static const unsigned mode = ParseMode();
    return mode;
}

const char* FilterName() {
    static const char* name = std::getenv("APS5_TRACE_SYNC") == nullptr ? "" : std::getenv("APS5_TRACE_SYNC") + 5;
    return name;
}

std::uint64_t CurrentThread() {
#ifdef _WIN32
    return GetCurrentThreadId();
#elif defined(__APPLE__)
    std::uint64_t thread = 0;
    pthread_threadid_np(nullptr, &thread);
    return thread;
#else
    return static_cast<std::uint64_t>(syscall(SYS_gettid));
#endif
}

std::uint64_t NowMicros() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::size_t NameHome(std::uint64_t object) {
    return static_cast<std::size_t>(((object >> 4) * 2654435761u) % NameCapacity);
}

void NoteName(const void* object, const char* name) {
    if (object == nullptr || name == nullptr || *name == '\0') return;
    const std::uint64_t key = reinterpret_cast<std::uint64_t>(object);
    const std::size_t home = NameHome(key);
    for (std::size_t probe = 0; probe < NameProbeLimit; ++probe) {
        NameSlot& slot = g_names[(home + probe) % NameCapacity];
        const std::uint64_t existing = slot.object.load(std::memory_order_acquire);
        if (existing == key) return;
        if (existing != 0) continue;
        std::uint64_t expected = 0;
        if (!slot.object.compare_exchange_strong(expected, key, std::memory_order_acq_rel)) {
            if (expected == key) return;
            continue;
        }
        char copy[NameBytes]{};
        std::strncpy(copy, name, NameBytes - 1);
        std::memcpy(slot.text, copy, NameBytes);
        slot.ready.store(1, std::memory_order_release);
        return;
    }
}

const char* NameOf(std::uint64_t object) {
    if (object == 0) return "?";
    const std::size_t home = NameHome(object);
    for (std::size_t probe = 0; probe < NameProbeLimit; ++probe) {
        NameSlot& slot = g_names[(home + probe) % NameCapacity];
        const std::uint64_t existing = slot.object.load(std::memory_order_acquire);
        if (existing == object) return slot.ready.load(std::memory_order_acquire) != 0 ? slot.text : "?";
        if (existing == 0) break;
    }
    return "?";
}

bool PassesFilter(unsigned operation, const char* name) {
    if (RingMode() == 1) return true;
    if (operation != SyncTraceSignal && operation != SyncTraceWaitEnd) return false;
    return name != nullptr && std::strcmp(name, FilterName()) == 0;
}

void WriteLine(const char* text, std::size_t length) {
    log_file::CrashWrite(log_file::StandardError, text, length);
    log_file::CrashWrite(log_file::StandardError, "\n", 1);
}

}

extern "C" {

unsigned SyncTraceRingMode_nid_no_patch() noexcept {
    return RingMode();
}

void SyncTraceRecord_nid_no_patch(unsigned operation, const void* object, const char* name, std::uint64_t value, std::uint64_t result) noexcept {
    const unsigned mode = RingMode();
    if (mode == 0) return;
    if (!PassesFilter(operation, name)) return;
    if (operation == SyncTraceCreate) {
        NoteName(object, name);
    } else if (mode == 2 && reinterpret_cast<std::uint64_t>(object) != g_filteredObject.load(std::memory_order_acquire)) {
        NoteName(object, name);
        g_filteredObject.store(reinterpret_cast<std::uint64_t>(object), std::memory_order_release);
    }
    const std::uint64_t sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    RingSlot& slot = g_ring[sequence % RingCapacity];
    slot.sequence.store(0, std::memory_order_release);
    slot.micros.store(NowMicros(), std::memory_order_relaxed);
    slot.thread.store(CurrentThread(), std::memory_order_relaxed);
    slot.object.store(reinterpret_cast<std::uint64_t>(object), std::memory_order_relaxed);
    slot.value.store(value, std::memory_order_relaxed);
    slot.result.store(result, std::memory_order_relaxed);
    slot.operation.store(operation, std::memory_order_relaxed);
    slot.sequence.store(sequence, std::memory_order_release);
}

void SyncTraceReportRing_nid_no_patch() noexcept {
    if (RingMode() == 0) return;
    const std::uint64_t last = g_sequence.load(std::memory_order_acquire);
    if (last == 0) return;
    const std::uint64_t span = last < RingCapacity ? last : RingCapacity;
    const std::uint64_t first = last - span + 1;
    std::size_t count = 0;
    std::uint64_t base = 0;
    for (std::uint64_t sequence = first; sequence <= last; ++sequence) {
        const RingSlot& slot = g_ring[sequence % RingCapacity];
        if (slot.sequence.load(std::memory_order_acquire) != sequence) continue;
        Valid copy{sequence, slot.micros.load(std::memory_order_relaxed), slot.thread.load(std::memory_order_relaxed), slot.object.load(std::memory_order_relaxed),
            slot.value.load(std::memory_order_relaxed), slot.result.load(std::memory_order_relaxed), slot.operation.load(std::memory_order_relaxed)};
        if (slot.sequence.load(std::memory_order_acquire) != sequence) continue;
        if (count == 0) base = copy.micros;
        g_valids[count++] = copy;
    }
    if (count == 0) return;
    char line[384];
    int length = std::snprintf(line, sizeof(line), "[sema] sync ring dump: %zu records, t in microseconds relative to the first record", count);
    if (length > 0) WriteLine(line, static_cast<std::size_t>(length) < sizeof(line) ? static_cast<std::size_t>(length) : sizeof(line) - 1);
    for (std::size_t index = 0; index < count; ++index) {
        const Valid& record = g_valids[index];
        const unsigned long long micros = static_cast<unsigned long long>(record.micros - base);
        const unsigned long long thread = static_cast<unsigned long long>(record.thread);
        const unsigned long long sequence = static_cast<unsigned long long>(record.sequence);
        const char* name = NameOf(record.object);
        const void* object = reinterpret_cast<void*>(static_cast<std::uintptr_t>(record.object));
        switch (record.operation) {
        case SyncTraceCreate:
            length = std::snprintf(line, sizeof(line), "[sema] create %p '%s' attr=0x%llx init=%d max=%d", object, name,
                static_cast<unsigned long long>(record.value >> 32), static_cast<int>(static_cast<std::uint32_t>(record.value)), static_cast<int>(record.result));
            break;
        case SyncTraceSignal:
            length = std::snprintf(line, sizeof(line), "[sema] signal %p '%s' count=%llu have=%llu->%llu", object, name,
                static_cast<unsigned long long>(record.value), static_cast<unsigned long long>(static_cast<std::uint32_t>(record.result)), static_cast<unsigned long long>(record.result >> 32));
            break;
        case SyncTraceWaitBegin:
            length = std::snprintf(line, sizeof(line), "[sema] wait begin %p '%s' need=%llu have=%llu timeout=%lld", object, name,
                static_cast<unsigned long long>(static_cast<std::uint32_t>(record.value)), static_cast<unsigned long long>(record.value >> 32), static_cast<long long>(record.result));
            break;
        case SyncTraceWaitEnd:
            length = std::snprintf(line, sizeof(line), "[sema] wait end %p '%s' result=0x%llx have=%llu", object, name,
                static_cast<unsigned long long>(record.value), static_cast<unsigned long long>(record.result));
            break;
        case SyncTracePoll:
            length = std::snprintf(line, sizeof(line), "[sema] poll %p '%s' need=%llu have=%llu result=0x%llx", object, name,
                static_cast<unsigned long long>(static_cast<std::uint32_t>(record.value)), static_cast<unsigned long long>(record.value >> 32), static_cast<unsigned long long>(record.result));
            break;
        case SyncTraceCancel:
            length = std::snprintf(line, sizeof(line), "[sema] cancel %p '%s' count=%lld result=0x%llx", object, name,
                static_cast<long long>(record.value), static_cast<unsigned long long>(record.result));
            break;
        case SyncTraceDelete:
            length = std::snprintf(line, sizeof(line), "[sema] delete %p '%s'", object, name);
            break;
        default:
            length = std::snprintf(line, sizeof(line), "[sema] operation %u %p '%s' value=0x%llx result=0x%llx", record.operation, object, name,
                static_cast<unsigned long long>(record.value), static_cast<unsigned long long>(record.result));
            break;
        }
        if (length <= 0) continue;
        std::size_t used = static_cast<std::size_t>(length) < sizeof(line) ? static_cast<std::size_t>(length) : sizeof(line) - 1;
        const int extra = std::snprintf(line + used, sizeof(line) - used, " tid=%llu t=%llu seq=%llu", thread, micros, sequence);
        if (extra > 0) used += static_cast<std::size_t>(extra) < sizeof(line) - used ? static_cast<std::size_t>(extra) : sizeof(line) - used - 1;
        WriteLine(line, used);
    }
}

}
