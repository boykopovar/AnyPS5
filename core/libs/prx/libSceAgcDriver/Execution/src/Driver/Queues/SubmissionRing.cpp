#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libc/include/FailureReport.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::size_t KeptWords = 512;
constexpr std::size_t MaximumCapacity = 4096;
constexpr std::size_t MaximumStallSeconds = 86400;
constexpr std::size_t PayloadWordsShown = 8;
constexpr std::size_t ShaderUsesKept = 512;
constexpr std::size_t ShaderUsesShown = 256;

struct Entry {
    std::uint64_t tenthsOfMs = 0;
    std::uint64_t serial = 0;
    std::uint64_t guest = 0;
    std::size_t dwords = 0;
    std::size_t kept = 0;
    bool rewindTail = false;
    std::array<std::uint32_t, KeptWords> words{};
};

struct ShaderUse {
    std::uint64_t tenthsOfMs = 0;
    std::uint64_t serial = 0;
    std::uint64_t target = 0;
    std::uint64_t codeAddress = 0;
    std::uint64_t entryAddress = 0;
    std::uint32_t stage = 0;
    std::array<std::uint32_t, 3> groups{};
};

struct Ring {
    std::mutex mutex;
    std::once_flag allocated;
    std::atomic<std::uint64_t> recorded{0};
    std::atomic<std::uint64_t> shaderUses{0};
    std::unique_ptr<Entry[]> entries;
    std::unique_ptr<ShaderUse[]> shaderUseEntries;
};

void reportFailure() noexcept;

struct Rings {
    std::array<Ring, SubmissionQueueEnd> queues;
};

Rings& rings() {
    static auto* value = new Rings;
    return *value;
}

struct Registration {
    Registration() { LibcRegisterFailureReport_nid_no_patch(reportFailure); }
    ~Registration() { LibcUnregisterFailureReport_nid_no_patch(reportFailure); }
    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
};

std::size_t environmentCount(const char* name, std::size_t fallback, std::size_t maximum, const char* reason) {
    const char* text = std::getenv(name);
    if (text == nullptr) return fallback;
    const auto* end = text + std::strlen(text);
    std::size_t value = 0;
    const auto [last, error] = std::from_chars(text, end, value);
    require(error == std::errc{} && last == end && value <= maximum, reason);
    return value;
}

bool writeLine(const char* text, std::size_t length) {
#ifdef _WIN32
    const HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    return WriteFile(handle, text, static_cast<DWORD>(length), &written, nullptr) != 0 && written == length;
#else
    while (length != 0) {
        const auto written = ::write(2, text, length);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        text += written;
        length -= static_cast<std::size_t>(written);
    }
    return true;
#endif
}

class Line {
public:
    void Add(const char* format, ...) {
        if (length >= sizeof(text) - 2) return;
        va_list arguments;
        va_start(arguments, format);
        const int added = std::vsnprintf(text + length, sizeof(text) - 1 - length, format, arguments);
        va_end(arguments);
        if (added > 0) length = std::min(length + static_cast<std::size_t>(added), sizeof(text) - 2);
    }

    bool Write() {
        text[length] = '\n';
        const bool written = writeLine(text, length + 1);
        length = 0;
        return written;
    }

private:
    char text[512];
    std::size_t length = 0;
};

void addName(Line& line, std::uint32_t header) {
    const auto name = Pm4::KnownName(header);
    if (name.empty()) line.Add("UNKNOWN_0x%02x", static_cast<unsigned>((header >> 8u) & 0xffu));
    else line.Add("%.*s", static_cast<int>(name.size()), name.data());
}

const char* stageName(std::uint32_t stage) {
    switch (stage) {
    case 0: return "compute";
    case 1: return "vertex";
    case 2: return "tess-control";
    case 3: return "tess-evaluation";
    case 4: return "geometry";
    case 5: return "fragment";
    case 6: return "local";
    case 7: return "mesh";
    default: return "unknown";
    }
}

bool writePackets(const Entry& entry, Line& line) {
    bool written = true;
    for (std::size_t cursor = 0; cursor < entry.kept;) {
        const auto header = entry.words[cursor];
        line.Add("[pm4 ring]   +0x%04zx ", cursor);
        if (Pm4::FillerPacket(header)) {
            std::size_t run = 1;
            while (cursor + run < entry.kept && Pm4::FillerPacket(entry.words[cursor + run])) ++run;
            addName(line, header);
            line.Add(" x%zu", run);
            written = line.Write() && written;
            cursor += run;
            continue;
        }
        if ((header >> 30u) != 3u) {
            line.Add("not a type-3 header:");
            for (std::size_t i = 0; i <= PayloadWordsShown && cursor + i < entry.kept; ++i) line.Add(" %08x", entry.words[cursor + i]);
            written = line.Write() && written;
            break;
        }
        const auto words = Pm4::PacketWords(header);
        const auto shown = std::min(words, PayloadWordsShown + 1);
        addName(line, header);
        for (std::size_t i = 1; i < shown && cursor + i < entry.kept; ++i) line.Add(" %08x", entry.words[cursor + i]);
        if (cursor + shown > entry.kept) line.Add(" (cut)");
        else if (words > shown) line.Add(" (+%zu dwords)", words - shown);
        written = line.Write() && written;
        cursor += words;
    }
    return written;
}

bool dump(std::string_view reason, bool failing) {
    const auto capacity = SubmissionRingCapacity();
    const auto label = static_cast<int>(std::min<std::size_t>(reason.size(), 200));
    Line line;
    if (capacity == 0) {
        line.Add("[pm4 ring] %.*s: disabled (APS5_PM4_RING=0)", label, reason.data());
        return line.Write();
    }
    auto& state = rings();
    std::size_t used = 0;
    for (const auto& ring : state.queues) used += ring.recorded.load(std::memory_order_acquire) != 0 ? 1 : 0;
    if (used == 0) {
        line.Add("[pm4 ring] %.*s: no submissions recorded", label, reason.data());
        return line.Write();
    }
    line.Add("[pm4 ring] %.*s: %zu queue(s), up to %zu submissions each, oldest first", label, reason.data(), used, capacity);
    bool written = line.Write();
    for (std::size_t queue = 0; queue < state.queues.size(); ++queue) {
        auto& ring = state.queues[queue];
        if (ring.recorded.load(std::memory_order_acquire) == 0) continue;
        std::unique_lock lock(ring.mutex, std::defer_lock);
        if (failing) {
            for (int attempt = 0; attempt < 100 && !lock.try_lock(); ++attempt) std::this_thread::yield();
        } else {
            lock.lock();
        }
        const auto recorded = ring.recorded.load(std::memory_order_acquire);
        if (!lock.owns_lock()) {
            line.Add("[pm4 ring] queue 0x%zx: %llu submissions recorded, ring busy", queue, static_cast<unsigned long long>(recorded));
            written = line.Write() && written;
            continue;
        }
        const auto first = recorded > capacity ? recorded - capacity : 0;
        line.Add("[pm4 ring] queue 0x%zx: %llu submissions recorded, last %llu kept", queue, static_cast<unsigned long long>(recorded), static_cast<unsigned long long>(recorded - first));
        written = line.Write() && written;
        for (auto ordinal = first; ordinal < recorded; ++ordinal) {
            const auto& entry = ring.entries[ordinal % capacity];
            line.Add("[pm4 ring] queue 0x%zx #%llu serial %llu t=%llu.%llu ms: %zu dwords from 0x%llx%s, first %zu kept", queue, static_cast<unsigned long long>(ordinal + 1), static_cast<unsigned long long>(entry.serial),
                     static_cast<unsigned long long>(entry.tenthsOfMs / 10), static_cast<unsigned long long>(entry.tenthsOfMs % 10),
                     entry.dwords, static_cast<unsigned long long>(entry.guest), entry.rewindTail ? " (rewind tail)" : "", entry.kept);
            written = line.Write() && written;
            written = writePackets(entry, line) && written;
        }
        const auto shaderUses = ring.shaderUses.load(std::memory_order_acquire);
        if (shaderUses != 0 && ring.shaderUseEntries != nullptr) {
            const auto firstUse = shaderUses > ShaderUsesShown ? shaderUses - ShaderUsesShown : 0;
            line.Add("[pm4 ring] queue 0x%zx: %llu shader uses recorded, last %llu shown", queue, static_cast<unsigned long long>(shaderUses), static_cast<unsigned long long>(shaderUses - firstUse));
            written = line.Write() && written;
            for (auto use = firstUse; use < shaderUses; ++use) {
                const auto& entry = ring.shaderUseEntries[use % ShaderUsesKept];
                line.Add("[pm4 ring]   shader serial=%llu t=%llu.%llu ms stage=%s target=0x%llx code=0x%llx entry=0x%llx",
                         static_cast<unsigned long long>(entry.serial), static_cast<unsigned long long>(entry.tenthsOfMs / 10), static_cast<unsigned long long>(entry.tenthsOfMs % 10),
                         stageName(entry.stage), static_cast<unsigned long long>(entry.target), static_cast<unsigned long long>(entry.codeAddress), static_cast<unsigned long long>(entry.entryAddress));
                if (entry.stage == 0 && (entry.groups[0] != 0 || entry.groups[1] != 0 || entry.groups[2] != 0))
                    line.Add(" groups=%ux%ux%u", entry.groups[0], entry.groups[1], entry.groups[2]);
                written = line.Write() && written;
            }
        }
    }
    return written;
}

void reportFailure() noexcept {
    try {
        dump("failure report", true);
    } catch (...) {
        static constexpr char text[] = "[pm4 ring] failure report: dump failed\n";
        writeLine(text, sizeof(text) - 1);
    }
}

}

std::size_t SubmissionRingCapacity() {
    static const std::size_t value = environmentCount("APS5_PM4_RING", 16, MaximumCapacity, "APS5_PM4_RING must be a number of submissions from 0 to 4096");
    return value;
}

std::chrono::seconds StallWatchdogPeriod() {
    static const std::chrono::seconds value(environmentCount("APS5_STALL_WATCHDOG_SECS", 0, MaximumStallSeconds, "APS5_STALL_WATCHDOG_SECS must be a number of seconds from 0 to 86400"));
    return value;
}

bool ReadSubmissionRingSettings() {
    StallWatchdogPeriod();
    return SubmissionRingCapacity() != 0;
}

void PrepareSubmissionRing(std::uint32_t queue) {
    const auto capacity = SubmissionRingCapacity();
    require(capacity != 0, "the PM4 ring is disabled");
    require(queue < SubmissionQueueEnd, "PM4 ring queue out of range");
    static const Registration registration;
    auto& ring = rings().queues[queue];
    std::call_once(ring.allocated, [&] {
        std::lock_guard lock(ring.mutex);
        ring.entries = std::make_unique<Entry[]>(capacity);
        ring.shaderUseEntries = std::make_unique<ShaderUse[]>(ShaderUsesKept);
    });
}

void RecordSubmission(std::uint32_t queue, std::uint64_t serial, std::span<const std::uint32_t> commands, const void* guest, bool rewindTail) {
    const auto capacity = SubmissionRingCapacity();
    require(capacity != 0, "the PM4 ring is disabled");
    require(queue < SubmissionQueueEnd, "PM4 ring queue out of range");
    auto& ring = rings().queues[queue];
    require(ring.entries != nullptr, "PM4 ring recorded before it was prepared");
    const auto tenthsOfMs = static_cast<std::uint64_t>(std::max(TraceMs(), 0.0) * 10.0 + 0.5);
    std::lock_guard lock(ring.mutex);
    const auto recorded = ring.recorded.load(std::memory_order_relaxed);
    auto& entry = ring.entries[recorded % capacity];
    entry.tenthsOfMs = tenthsOfMs;
    entry.serial = serial;
    entry.guest = reinterpret_cast<std::uintptr_t>(guest);
    entry.dwords = commands.size();
    entry.kept = std::min(commands.size(), KeptWords);
    entry.rewindTail = rewindTail;
    std::copy_n(commands.begin(), entry.kept, entry.words.begin());
    ring.recorded.store(recorded + 1, std::memory_order_release);
}

void RecordShaderUse(std::uint32_t queue, std::uint64_t serial, std::uint32_t stage, std::uint64_t target, std::uint64_t codeAddress, std::uint64_t entryAddress, const std::array<std::uint32_t, 3>& groups) {
    if (queue >= SubmissionQueueEnd) return;
    auto& ring = rings().queues[queue];
    const auto tenthsOfMs = static_cast<std::uint64_t>(std::max(TraceMs(), 0.0) * 10.0 + 0.5);
    std::lock_guard lock(ring.mutex);
    if (ring.shaderUseEntries == nullptr) return;
    const auto recorded = ring.shaderUses.load(std::memory_order_relaxed);
    auto& entry = ring.shaderUseEntries[recorded % ShaderUsesKept];
    entry.tenthsOfMs = tenthsOfMs;
    entry.serial = serial;
    entry.target = target;
    entry.codeAddress = codeAddress;
    entry.entryAddress = entryAddress;
    entry.stage = stage;
    entry.groups = groups;
    ring.shaderUses.store(recorded + 1, std::memory_order_release);
}

void DumpSubmissionRings(std::string_view reason) {
    require(dump(reason, false), "cannot write the PM4 ring dump to stderr");
}

void DumpSubmissionRingsAfterFailure() noexcept {
    reportFailure();
}

void StallWatch::Check(const char* waiter, std::uint64_t value) {
    if (dumped) return;
    const auto period = StallWatchdogPeriod();
    if (period.count() == 0) return;
    const auto now = std::chrono::steady_clock::now();
    if (since == std::chrono::steady_clock::time_point{}) {
        since = now;
        return;
    }
    if (now - since < period) return;
    dumped = true;
    char reason[192];
    std::snprintf(reason, sizeof(reason), "queue 0x%x %s 0x%llx has not progressed for %lld s (APS5_STALL_WATCHDOG_SECS)", queue, waiter, static_cast<unsigned long long>(value), static_cast<long long>(period.count()));
    DumpSubmissionRings(reason);
}

}
