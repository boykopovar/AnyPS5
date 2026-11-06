#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#endif

// Opt-in probe for equeue wait identity and wait/producer correlation
// (APS5_TRACE_EQUEUE_IDENTITY, default off, libkernel only, no exported API).
//
// Records, per equeue object and per 3 s window:
//   - waits and their result: events delivered, empty return, error return;
//   - prod: an observable producer (trigger or triggered add) advanced during the wait;
//     noprod: events were delivered but no instrumented producer advanced. noprod does NOT mean
//     "uninstrumented path": the event can simply have been produced before the wait started (the
//     queue was already armed), exactly like a timer or a descriptor readiness that is not
//     instrumented. The probe cannot tell those apart and does not claim to;
//   - the last waiter (OS tid, call site, duration) and the last observable producer
//     (filter, ident, udata hash, call site, timestamp);
//   - the bounded table of producer combinations (filter, ident, udata hash), 4 slots + overflow.
//
// Clock: durations and window math use the monotonic nanosecond clock supplied by the caller
// (KernelEqueuePrivate::MonotonicNs, std::chrono::steady_clock). The printed window label and the
// producer timestamp are process-relative nanoseconds derived from sceKernelGetProcessTime(), the
// same source the submit probe prints, so lines from different probes can be aligned.
// Correlation is not causality: "prod" only means an instrumented producer ran between the start of
// the wait and its exit; it does not say which event woke the wait or why it took that long.
//
// Privacy: the guest equeue name is never printed (length + 32-bit hash only) and udata is never
// dereferenced and never printed (32-bit hash of the pointer value only).
//
// Accounting identity, per window and per equeue:
//     waits = prod + noprod + empty + err + open   (every closed wait falls in exactly one class;
//                                                   open waits are tracked separately and a wait
//                                                   that closes after a window boundary is counted
//                                                   in the window where it closes)
//     events = events delivered on prod + noprod waits
// A wait that never closes (exception unwinds the guest call) would break the identity, so the
// caller closes it with class "err" from a catch-all; that is the only non-return path. A wait
// beyond the per-equeue open-slot budget (MaxOpenWaits) is closed as "err" at entry instead of
// being lost.
// The aggregate above is not attributed to one call site: the bounded per-(site, tid) table below
// carries its own counters, and the report prints the busiest entries plus a folded total.
//
// Thread safety: the functions below are stateless helpers over a caller-owned State; the caller
// serialises access to that State (in libkernel, a dedicated probe mutex). Nothing here allocates.
// Every helper is `static`: internal linkage per translation unit, no static state, so including this
// header adds no exported symbol and no per-module singleton. (A plain `inline` helper can be emitted
// out of line and the NID patcher would then export it; that happened once during development and is
// the reason for the explicit `static`.)

namespace EqueueIdentityTraceDetail {

struct Combo {
    std::int16_t filter = 0;
    std::uintptr_t ident = 0;
    std::uint32_t udata = 0;
    std::uint64_t count = 0;
};

static inline std::uint32_t HashText(const char* text) {
    std::uint32_t hash = 2166136261u;
    std::size_t index = 0;
    while (text != nullptr && text[index] != '\0' && index < 256) {
        hash ^= static_cast<unsigned char>(text[index]);
        hash *= 16777619u;
        ++index;
    }
    return hash;
}

static inline std::uint32_t TextLength(const char* text) {
    std::uint32_t length = 0;
    while (text != nullptr && text[length] != '\0' && length < 256) ++length;
    return length;
}

// Opaque fingerprint of a guest pointer value: no dereference, no address printed.
static inline std::uint32_t HashPointer(const void* pointer) {
    std::uint64_t value = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdull;
    value ^= value >> 33;
    return static_cast<std::uint32_t>(value ^ (value >> 32));
}

// Host module plus offset, e.g. "libkernel.prx+0x1234" or "dredge.exe+0x1381523".
static inline void FormatSite(const void* address, char* out, std::size_t size) {
    if (out == nullptr || size == 0) return;
    out[0] = '\0';
#if defined(_WIN32)
    HMODULE module = nullptr;
    if (address != nullptr &&
        ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(address), &module) != 0) {
        wchar_t path[MAX_PATH];
        const DWORD length = ::GetModuleFileNameW(module, path, MAX_PATH);
        const wchar_t* base = path;
        for (DWORD index = 0; index < length; ++index) {
            if (path[index] == L'\\' || path[index] == L'/') base = path + index + 1;
        }
        char narrow[64] = {};
        ::WideCharToMultiByte(CP_UTF8, 0, base, -1, narrow, static_cast<int>(sizeof narrow), nullptr, nullptr);
        const std::uintptr_t offset =
            reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(module);
        std::snprintf(out, size, "%s+0x%llx", narrow, static_cast<unsigned long long>(offset));
        return;
    }
#endif
    std::snprintf(out, size, "0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(address)));
}

// Per-equeue probe state. One instance per live equeue, owned by the probe registry.
// One entry per (wait call site, OS tid) inside a window: aggregated counters alone cannot say which
// caller produced them, so the report keeps a bounded table and prints the busiest entries.
struct WaitSite {
    const void* site = nullptr;
    unsigned long tid = 0;
    std::uint64_t waits = 0;
    std::uint64_t producerWaits = 0;
    std::uint64_t noProducerWaits = 0;
    std::uint64_t emptyWaits = 0;
    std::uint64_t errorWaits = 0;
    std::uint64_t events = 0;
    std::uint64_t lastDurationNs = 0;
};

static constexpr std::size_t MaxWaitSites = 8;
static constexpr std::size_t PrintedWaitSites = 4;

struct State {
    std::uint64_t id = 0;             // unique for the process lifetime, never reused
    std::uint64_t handle = 0;
    std::uint32_t nameHash = 0;
    std::uint32_t nameLength = 0;

    std::uint64_t attachProcNs = 0;
    std::uint64_t windowStartProcNs = 0;
    std::uint64_t lastPrintedWindow = 0;
    bool firstWindow = true;

    std::uint64_t waits = 0;
    std::uint64_t producerWaits = 0;
    std::uint64_t noProducerWaits = 0;
    std::uint64_t emptyWaits = 0;
    std::uint64_t errorWaits = 0;
    std::uint64_t events = 0;
    std::uint64_t producers = 0;

    WaitSite waitSites[MaxWaitSites];
    std::size_t waitSiteCount = 0;
    WaitSite foldedWaitSite;

    Combo combos[4];
    std::size_t comboCount = 0;
    std::uint64_t comboOverflow = 0;

    // Per-open-wait state. A single (site,tid,generation) snapshot is captured at wait entry
    // so a concurrent or late wait never inherits another waiter's data (F1/F5).
    struct OpenWait {
        bool used = false;
        std::uint64_t startNs = 0;
        std::uint64_t startGen = 0;
        std::uint64_t windowGen = 0;
        unsigned long tid = 0;
        const void* site = nullptr;
    };
    static constexpr std::size_t MaxOpenWaits = MaxWaitSites;
    OpenWait openWaits[MaxOpenWaits]{};
    std::size_t openWaitCount = 0;
    bool retired = false;
    // Incremented by ResetWindow. A wait that closes after a window boundary is counted in the
    // window where it closes, with the site it entered from (F5): otherwise its class would land
    // in the new window while its wait count stayed in the old one, breaking the identity.
    std::uint64_t windowGen = 0;

    bool waiting = false;          // aggregate: true while at least one wait is open
    std::uint64_t waitStartNs = 0;
    std::uint64_t waitStartGen = 0;
    std::uint64_t producerGen = 0;
    unsigned long lastWaitTid = 0;
    const void* lastWaitSite = nullptr;
    std::uint64_t lastWaitDurationNs = 0;
    std::uint64_t lastWaitProcNs = 0;
    int lastWaitDelivered = 0;
    char lastWaitClass = '?';

    std::int16_t lastProducerFilter = 0;
    std::uintptr_t lastProducerIdent = 0;
    std::uint32_t lastProducerUdata = 0;
    const void* lastProducerSite = nullptr;
    std::uint64_t lastProducerProcNs = 0;
};

struct Snapshot {
    std::uint64_t traceId = 0;
    std::uint64_t handle = 0;
    std::uint32_t nameHash = 0;
    std::uint32_t nameLength = 0;
    std::uint64_t windowStartProcNs = 0;

    std::uint64_t waits = 0;
    std::uint64_t producerWaits = 0;
    std::uint64_t noProducerWaits = 0;
    std::uint64_t emptyWaits = 0;
    std::uint64_t errorWaits = 0;
    std::uint64_t openWaits = 0;
    std::uint64_t events = 0;
    std::uint64_t producers = 0;

    WaitSite waitSites[MaxWaitSites];
    std::size_t waitSiteCount = 0;
    WaitSite foldedWaitSite;

    Combo combos[4];
    std::size_t comboCount = 0;
    std::uint64_t comboOverflow = 0;
    std::uint64_t slotsOverflow = 0;

    unsigned long lastWaitTid = 0;
    const void* lastWaitSite = nullptr;
    std::uint64_t lastWaitDurationNs = 0;
    std::uint64_t lastWaitProcNs = 0;
    int lastWaitDelivered = 0;
    char lastWaitClass = '?';

    std::int16_t lastProducerFilter = 0;
    std::uintptr_t lastProducerIdent = 0;
    std::uint32_t lastProducerUdata = 0;
    const void* lastProducerSite = nullptr;
    std::uint64_t lastProducerProcNs = 0;
};

static inline void ResetWindow(State& state, std::uint64_t nowProcNs) {
    state.windowStartProcNs = nowProcNs;
    state.firstWindow = false;
    ++state.windowGen;
    // ZEROS all per-window aggregate counters:
    state.waits = 0;
    state.producerWaits = 0;
    state.noProducerWaits = 0;
    state.emptyWaits = 0;
    state.errorWaits = 0;
    state.events = 0;
    state.producers = 0;
    state.comboCount = 0;
    state.comboOverflow = 0;
    // The per-(site,tid) table belongs to the same window as the aggregate counters. Keeping rows
    // from earlier windows made a line print "waits 0" next to non-zero site rows and made
    // sites-folded count rows that were already printed. A wait that is still open keeps its own
    // snapshot and is attributed when it closes (see WaitExit).
    state.waitSiteCount = 0;
    for (std::size_t index = 0; index < MaxWaitSites; ++index) state.waitSites[index] = WaitSite{};
    state.foldedWaitSite = WaitSite{};
    for (std::size_t index = 0; index < 4; ++index) state.combos[index] = Combo{};
    state.lastWaitTid = 0;
    state.lastWaitSite = nullptr;
    state.lastWaitDurationNs = 0;
    state.lastWaitProcNs = 0;
    state.lastWaitDelivered = 0;
    state.lastWaitClass = '?';
    state.lastProducerFilter = 0;
    state.lastProducerIdent = 0;
    state.lastProducerUdata = 0;
    state.lastProducerSite = nullptr;
    state.lastProducerProcNs = 0;
}

static inline void Attach(State& state, std::uint64_t id, std::uint64_t handle, const char* name, std::uint64_t procNs) {
    state = State{};
    state.id = id;
    state.handle = handle;
    state.attachProcNs = procNs;
    state.nameHash = HashText(name);
    state.nameLength = TextLength(name);
}

static inline void Retire(State& state) {
    state.retired = true;
}

static inline WaitSite& WaitSiteFor(State& state, const void* site, unsigned long tid) {
    for (std::size_t index = 0; index < state.waitSiteCount; ++index) {
        if (state.waitSites[index].site == site && state.waitSites[index].tid == tid) return state.waitSites[index];
    }
    if (state.waitSiteCount < MaxWaitSites) {
        WaitSite& entry = state.waitSites[state.waitSiteCount++];
        entry.site = site;
        entry.tid = tid;
        return entry;
    }
    return state.foldedWaitSite;
}

static inline int WaitEnter(State& state, std::uint64_t nowNs, std::uint64_t procNs, unsigned long tid, const void* site) {
    int token = -1;
    for (std::size_t index = 0; index < State::MaxOpenWaits; ++index) {
        if (!state.openWaits[index].used) { token = static_cast<int>(index); break; }
    }
    if (token >= 0) {
        state.openWaits[token].used = true;
        state.openWaits[token].startNs = nowNs;
        state.openWaits[token].startGen = state.producerGen;
        state.openWaits[token].windowGen = state.windowGen;
        state.openWaits[token].tid = tid;
        state.openWaits[token].site = site;
        ++state.openWaitCount;
    } else {
        // Every per-equeue open slot is busy (more than MaxOpenWaits concurrent waits). The wait
        // still enters, so it must be classified exactly once instead of disappearing from the
        // identity: close it immediately as an error, attributed to its own site.
        WaitSite& entry = WaitSiteFor(state, site, tid);
        ++entry.waits;
        ++entry.errorWaits;
        ++state.errorWaits;
        state.lastWaitClass = 'e';
        state.lastWaitDelivered = -1;
    }
    ++state.waits;
    if (state.windowStartProcNs == 0) state.windowStartProcNs = procNs;
    state.waiting = state.openWaitCount != 0;
    state.waitStartNs = nowNs;
    state.waitStartGen = state.producerGen;
    state.lastWaitTid = tid;
    state.lastWaitSite = site;
    return token;
}

static inline void WaitExit(State& state, int token, std::uint64_t nowNs, std::uint64_t procNs, int delivered) {
    if (token < 0 || token >= static_cast<int>(State::MaxOpenWaits) || !state.openWaits[token].used) return;
    const State::OpenWait wait = state.openWaits[token];
    state.openWaits[token] = State::OpenWait{};
    if (state.openWaitCount > 0) --state.openWaitCount;
    const std::uint64_t duration = nowNs >= wait.startNs ? nowNs - wait.startNs : 0;
    state.lastWaitDurationNs = duration;
    state.lastWaitProcNs = procNs;
    state.lastWaitDelivered = delivered;
    state.lastWaitTid = wait.tid;
    state.lastWaitSite = wait.site;
    // A wait entered before a window boundary closes into the new window: count it there as well,
    // otherwise the class of the exit would be counted without the matching wait (F5).
    if (wait.windowGen != state.windowGen) ++state.waits;
    WaitSite& entry = WaitSiteFor(state, wait.site, wait.tid);
    ++entry.waits;
    if (duration > entry.lastDurationNs) entry.lastDurationNs = duration;
    if (delivered < 0) {
        ++state.errorWaits;
        ++entry.errorWaits;
        state.lastWaitClass = 'e';
    } else if (delivered == 0) {
        ++state.emptyWaits;
        ++entry.emptyWaits;
        state.lastWaitClass = 'n';
    } else {
        state.events += static_cast<std::uint64_t>(delivered);
        entry.events += static_cast<std::uint64_t>(delivered);
        if (state.producerGen != wait.startGen) {
            ++state.producerWaits;
            ++entry.producerWaits;
            state.lastWaitClass = 'p';
        } else {
            ++state.noProducerWaits;
            ++entry.noProducerWaits;
            state.lastWaitClass = 'u';
        }
    }
    state.waiting = state.openWaitCount != 0;
}

// A retired slot (its equeue was deleted) may be released only by the registry owner, which reports
// the tombstone's final counters first: releasing inside WaitExit would throw away the late
// classification the tombstone exists for (F3).
static inline bool ReleaseIfUnused(State& state) {
    if (!state.retired || state.openWaitCount != 0) return false;
    state = State{};
    return true;
}

// Observable producer of a queue event: a trigger, or an add that arrives already triggered.
static inline void Producer(State& state, std::int16_t filter, std::uintptr_t ident, const void* udata, const void* site,
                     std::uint64_t procNs) {
    ++state.producers;
    ++state.producerGen;
    if (state.windowStartProcNs == 0) state.windowStartProcNs = procNs;
    const std::uint32_t udataHash = HashPointer(udata);
    state.lastProducerFilter = filter;
    state.lastProducerIdent = ident;
    state.lastProducerUdata = udataHash;
    state.lastProducerSite = site;
    state.lastProducerProcNs = procNs;
    for (std::size_t index = 0; index < state.comboCount; ++index) {
        if (state.combos[index].filter == filter && state.combos[index].ident == ident &&
            state.combos[index].udata == udataHash) {
            ++state.combos[index].count;
            return;
        }
    }
    if (state.comboCount < 4) {
        state.combos[state.comboCount].filter = filter;
        state.combos[state.comboCount].ident = ident;
        state.combos[state.comboCount].udata = udataHash;
        state.combos[state.comboCount].count = 1;
        ++state.comboCount;
        return;
    }
    ++state.comboOverflow;
}

static inline bool HasActivity(const State& state) {
    return state.waits != 0 || state.producers != 0 || state.events != 0 || state.comboOverflow != 0;
}

static inline void TakeSnapshot(const State& state, Snapshot& out, std::uint64_t nowProcNs) {
    out.traceId = state.id;
    out.handle = state.handle;
    out.nameHash = state.nameHash;
    out.nameLength = state.nameLength;
    // The first label of a queue starts at its creation, so a report that lands on the very first
    // activity cannot print a zero-length window. Later windows start at the previous report.
    out.windowStartProcNs = state.firstWindow
        ? (state.attachProcNs != 0 ? state.attachProcNs : nowProcNs)
        : (state.windowStartProcNs != 0 ? state.windowStartProcNs : nowProcNs);
    out.waits = state.waits;
    out.producerWaits = state.producerWaits;
    out.noProducerWaits = state.noProducerWaits;
    out.emptyWaits = state.emptyWaits;
    out.errorWaits = state.errorWaits;
    out.openWaits = static_cast<std::uint64_t>(state.openWaitCount);
    out.events = state.events;
    out.producers = state.producers;
    out.waitSiteCount = state.waitSiteCount;
    for (std::size_t index = 0; index < state.waitSiteCount; ++index) out.waitSites[index] = state.waitSites[index];
    out.foldedWaitSite = state.foldedWaitSite;
    out.comboCount = state.comboCount;
    out.comboOverflow = state.comboOverflow;
    for (std::size_t index = 0; index < state.comboCount; ++index) out.combos[index] = state.combos[index];
    out.lastWaitTid = state.lastWaitTid;
    out.lastWaitSite = state.lastWaitSite;
    out.lastWaitDurationNs = state.lastWaitDurationNs;
    out.lastWaitProcNs = state.lastWaitProcNs;
    out.lastWaitDelivered = state.lastWaitDelivered;
    out.lastWaitClass = state.lastWaitClass;
    out.lastProducerFilter = state.lastProducerFilter;
    out.lastProducerIdent = state.lastProducerIdent;
    out.lastProducerUdata = state.lastProducerUdata;
    out.lastProducerSite = state.lastProducerSite;
    out.lastProducerProcNs = state.lastProducerProcNs;
}

// Process-wide output budget state: at most MaxLines lines per WindowNanos, one line per equeue per
// window, suppressed queues counted in a running total.
struct CoordinatorState {
    static constexpr std::uint64_t WindowNanos = 3000000000ull;
    static constexpr std::size_t MaxLines = 8;

    std::uint64_t windowStartNs = 0;
    std::uint64_t windowIndex = 0;
    std::size_t lines = 0;
    std::uint64_t suppressed = 0;
};

struct Decision {
    bool admit = false;
    std::uint64_t windowIndex = 0;
};

static inline Decision Admit(CoordinatorState& coordinator, std::uint64_t nowNs, std::uint64_t lastPrintedWindow) {
    if (coordinator.windowStartNs == 0) {
        // First call only opens the process-wide window: the first line is printed after a full
        // interval, so it never reports a truncated window.
        coordinator.windowStartNs = nowNs;
        coordinator.windowIndex = 1;
        return {false, coordinator.windowIndex};
    }
    if (nowNs - coordinator.windowStartNs >= CoordinatorState::WindowNanos) {
        coordinator.windowStartNs = nowNs;
        ++coordinator.windowIndex;
        coordinator.lines = 0;
    }
    if (lastPrintedWindow == coordinator.windowIndex) return {false, coordinator.windowIndex};
    if (coordinator.lines >= CoordinatorState::MaxLines) {
        ++coordinator.suppressed;
        return {false, coordinator.windowIndex};
    }
    ++coordinator.lines;
    return {true, coordinator.windowIndex};
}

// One bounded line per admitted window. Returns false if the buffer cannot hold the full line.
static inline bool FormatLine(const Snapshot& snap, std::uint64_t windowEndProcNs, char* out, std::size_t size) {
    if (out == nullptr || size == 0) return false;
    char waitSite[96];
    char producerSite[96];
    FormatSite(snap.lastWaitSite, waitSite, sizeof waitSite);
    FormatSite(snap.lastProducerSite, producerSite, sizeof producerSite);
    const double startSeconds = static_cast<double>(snap.windowStartProcNs) / 1e9;
    const double endSeconds = static_cast<double>(windowEndProcNs) / 1e9;
    int written = std::snprintf(out, size,
        "[eq-id %.3f..%.3f s] eq %llu/%llu nh%08x nl%u | waits %llu prod %llu noprod %llu empty %llu err %llu "
        "| events %llu producers %llu | open %llu",
        startSeconds, endSeconds,
        static_cast<unsigned long long>(snap.handle), static_cast<unsigned long long>(snap.traceId),
        snap.nameHash, snap.nameLength,
        static_cast<unsigned long long>(snap.waits),
        static_cast<unsigned long long>(snap.producerWaits),
        static_cast<unsigned long long>(snap.noProducerWaits),
        static_cast<unsigned long long>(snap.emptyWaits),
        static_cast<unsigned long long>(snap.errorWaits),
        static_cast<unsigned long long>(snap.events),
        static_cast<unsigned long long>(snap.producers),
        static_cast<unsigned long long>(snap.openWaits));
    if (written <= 0 || static_cast<std::size_t>(written) >= size) return false;
    for (std::size_t index = 0; index < snap.comboCount; ++index) {
        const Combo& combo = snap.combos[index];
        const int step = std::snprintf(out + written, size - static_cast<std::size_t>(written),
            " | f%d i%llx u%08x x%llu",
            static_cast<int>(combo.filter), static_cast<unsigned long long>(combo.ident), combo.udata,
            static_cast<unsigned long long>(combo.count));
        if (step <= 0 || static_cast<std::size_t>(written + step) >= size) return false;
        written += step;
    }
    {
        const int step = std::snprintf(out + written, size - static_cast<std::size_t>(written),
            " | combos-overflow %llu slots-overflow %llu", static_cast<unsigned long long>(snap.comboOverflow),
            static_cast<unsigned long long>(snap.slotsOverflow));
        if (step <= 0 || static_cast<std::size_t>(written + step) >= size) return false;
        written += step;
    }
    // Busiest (site, tid) entries first, then the folded total: the aggregate counters of the line
    // are never attributed to a single call site.
    std::size_t siteOrder[MaxWaitSites];
    for (std::size_t index = 0; index < snap.waitSiteCount; ++index) siteOrder[index] = index;
    const std::size_t printedSites = snap.waitSiteCount < PrintedWaitSites ? snap.waitSiteCount : PrintedWaitSites;
    for (std::size_t i = 0; i < printedSites; ++i) {
        std::size_t best = i;
        for (std::size_t j = i + 1; j < snap.waitSiteCount; ++j) {
            if (snap.waitSites[siteOrder[j]].waits > snap.waitSites[siteOrder[best]].waits) best = j;
        }
        const std::size_t swap = siteOrder[i];
        siteOrder[i] = siteOrder[best];
        siteOrder[best] = swap;
    }
    for (std::size_t i = 0; i < printedSites; ++i) {
        const WaitSite& entry = snap.waitSites[siteOrder[i]];
        if (entry.waits == 0) continue;
        char siteText[96];
        FormatSite(entry.site, siteText, sizeof siteText);
        const int step = std::snprintf(out + written, size - static_cast<std::size_t>(written),
            " | site %s tid %lu n%llu p%llu u%llu 0%llu e%llu ev%llu max%.3fms",
            siteText, entry.tid,
            static_cast<unsigned long long>(entry.waits),
            static_cast<unsigned long long>(entry.producerWaits),
            static_cast<unsigned long long>(entry.noProducerWaits),
            static_cast<unsigned long long>(entry.emptyWaits),
            static_cast<unsigned long long>(entry.errorWaits),
            static_cast<unsigned long long>(entry.events),
            static_cast<double>(entry.lastDurationNs) / 1e6);
        if (step <= 0 || static_cast<std::size_t>(written + step) >= size) return false;
        written += step;
    }
    {
        std::uint64_t foldedWaits = snap.foldedWaitSite.waits;
        for (std::size_t index = printedSites; index < snap.waitSiteCount; ++index) foldedWaits += snap.waitSites[siteOrder[index]].waits;
        const int step = std::snprintf(out + written, size - static_cast<std::size_t>(written),
            " | sites-folded %llu", static_cast<unsigned long long>(foldedWaits));
        if (step <= 0 || static_cast<std::size_t>(written + step) >= size) return false;
        written += step;
    }
    if (snap.waits != 0) {
        const int step = std::snprintf(out + written, size - static_cast<std::size_t>(written),
            " | lastw tid %lu site %s dur %.3fms ev %d class %c proc %.3fms",
            snap.lastWaitTid, waitSite,
            static_cast<double>(snap.lastWaitDurationNs) / 1e6, snap.lastWaitDelivered, snap.lastWaitClass,
            static_cast<double>(snap.lastWaitProcNs) / 1e6);
        if (step <= 0 || static_cast<std::size_t>(written + step) >= size) return false;
        written += step;
    }
    if (snap.producers != 0) {
        const int step = std::snprintf(out + written, size - static_cast<std::size_t>(written),
            " | lastp f%d i%llx u%08x site %s proc %.3fms",
            static_cast<int>(snap.lastProducerFilter), static_cast<unsigned long long>(snap.lastProducerIdent),
            snap.lastProducerUdata, producerSite,
            static_cast<double>(snap.lastProducerProcNs) / 1e6);
        if (step <= 0 || static_cast<std::size_t>(written + step) >= size) return false;
        written += step;
    }
    if (static_cast<std::size_t>(written) + 2 > size) return false;
    out[written] = '\n';
    out[written + 1] = '\0';
    return true;
}

}  // namespace EqueueIdentityTraceDetail
