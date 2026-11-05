#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace VideoOutTrace {

// Opt-in aggregate for the guest-facing flip submission: per (handle, OS thread) the interval
// between submissions, the call duration, the queue occupancy seen at submission, and the outcome.
// The interval is guest-facing wall time, not guest CPU time. One bounded line per window; off by
// default, no per-call output or flush. Selection is dynamic: the table keeps up to Slots pairs seen
// in the window and only the PrintKeys busiest are printed, so the dominant submitting thread is
// never crowded out by transient ones (the rest folds into one aggregate line tagged h-1).
class SubmitFlipTrace {
public:
    static constexpr std::size_t PrintKeys = 8;

    bool Enabled() const { return enabled; }

    // Optional sink that receives the submitting thread the probe is currently looking at (the
    // busiest one, or the first seen before a window closes) and 0 when the trace stops. Kept as a
    // plain function pointer so this header still links into a host test without libkernel.
    void SetFocusSink(void (*sink)(unsigned long)) {
        std::lock_guard lock(mutex);
        focusSink = sink;
        notifiedFocus = 0;
    }

    void Note(int handle, unsigned long thread, std::uint64_t startNanos, std::uint64_t endNanos,
              std::uint32_t depth, bool accepted) {
        if (!enabled) return;
        std::lock_guard lock(mutex);
        NotifyFocus(thread);
        Entry& entry = FindOrCreate(handle, thread);
        ++entry.count;
        if (accepted) {
            ++entry.accepted;
        } else {
            ++entry.rejected;
        }
        if (depth > entry.depthMax) entry.depthMax = depth;
        if (entry.hasLast && startNanos >= entry.lastStart) {
            const std::uint64_t interval = startNanos - entry.lastStart;
            entry.intervalSum += interval;
            ++entry.intervalCount;
            if (interval > entry.intervalMax) entry.intervalMax = interval;
        }
        entry.hasLast = true;
        entry.lastStart = startNanos;
        const std::uint64_t duration = endNanos >= startNanos ? endNanos - startNanos : 0;
        entry.durationSum += duration;
        if (duration > entry.durationMax) entry.durationMax = duration;
        ++pending;
    }

    bool Report(std::uint64_t nowNanos, char* out, std::size_t size) {
        if (!enabled) return false;
        std::lock_guard lock(mutex);
        if (pending == 0) return false;
        if (!started) {
            started = true;
            windowStart = nowNanos;
            return false;
        }
        if (nowNanos - windowStart < windowNanos) return false;
        int written = std::snprintf(out, size, "[submit-flip %.3f..%.3f s]",
            static_cast<double>(windowStart) / 1e9, static_cast<double>(nowNanos) / 1e9);
        const auto printEntry = [&](const Entry& entry) {
            if (entry.count == 0 || written <= 0 || static_cast<std::size_t>(written) >= size) return;
            written += std::snprintf(out + written, size - static_cast<std::size_t>(written),
                " | h%d tid %lu x%llu ok%llu err%llu | interval avg %.2f max %.2f ms | call avg %.3f max %.3f ms | queue max %u",
                entry.handle, entry.thread,
                static_cast<unsigned long long>(entry.count),
                static_cast<unsigned long long>(entry.accepted),
                static_cast<unsigned long long>(entry.rejected),
                entry.intervalCount != 0 ? static_cast<double>(entry.intervalSum) / 1e6 / static_cast<double>(entry.intervalCount) : 0.0,
                static_cast<double>(entry.intervalMax) / 1e6,
                static_cast<double>(entry.durationSum) / 1e6 / static_cast<double>(entry.count),
                static_cast<double>(entry.durationMax) / 1e6,
                entry.depthMax);
        };
        // Dynamic bounded selection: rank the pairs seen in this window by submission count and print
        // the busiest PrintKeys. Everything else, plus any pair past Slots, folds into one aggregate
        // line tagged h-1 (exact sums, so no double counting).
        std::size_t order[Slots];
        for (std::size_t index = 0; index < used; ++index) order[index] = index;
        const std::size_t ranked = used < PrintKeys ? used : PrintKeys;
        for (std::size_t i = 0; i < ranked; ++i) {
            std::size_t best = i;
            for (std::size_t j = i + 1; j < used; ++j) {
                if (entries[order[j]].count > entries[order[best]].count) best = j;
            }
            const std::size_t swap = order[i];
            order[i] = order[best];
            order[best] = swap;
        }
        for (std::size_t i = 0; i < ranked; ++i) printEntry(entries[order[i]]);
        if (ranked != 0) NotifyFocus(entries[order[0]].thread);
        Entry folded = overflow;
        for (std::size_t index = ranked; index < used; ++index) MergeEntry(folded, entries[order[index]]);
        folded.handle = -1;
        folded.thread = 0;
        printEntry(folded);
        if (written > 0 && static_cast<std::size_t>(written) < size) std::snprintf(out + written, size - static_cast<std::size_t>(written), "\n");
        windowStart = nowNanos;
        pending = 0;
        for (std::size_t index = 0; index < used; ++index) entries[index] = Entry{};
        // Keys are per window: the bounded table holds the pairs active in this window, so a long run
        // cannot fill it with stale pairs and starve the current submission thread into overflow.
        used = 0;
        overflow = Entry{};
        return true;
    }

private:
    static constexpr std::uint64_t windowNanos = 3000000000ull;
    static constexpr std::size_t Slots = 32;

    struct Entry {
        int handle = 0;
        unsigned long thread = 0;
        std::uint64_t count = 0, accepted = 0, rejected = 0;
        bool hasLast = false;
        std::uint64_t lastStart = 0;
        std::uint64_t intervalSum = 0, intervalMax = 0, intervalCount = 0;
        std::uint64_t durationSum = 0, durationMax = 0;
        std::uint32_t depthMax = 0;
    };

    static void MergeEntry(Entry& into, const Entry& from) {
        into.count += from.count;
        into.accepted += from.accepted;
        into.rejected += from.rejected;
        into.intervalSum += from.intervalSum;
        into.intervalCount += from.intervalCount;
        into.durationSum += from.durationSum;
        if (from.intervalMax > into.intervalMax) into.intervalMax = from.intervalMax;
        if (from.durationMax > into.durationMax) into.durationMax = from.durationMax;
        if (from.depthMax > into.depthMax) into.depthMax = from.depthMax;
    }

    std::mutex mutex;

    void NotifyFocus(unsigned long thread) {
        if (focusSink == nullptr || thread == notifiedFocus || thread == 0) return;
        notifiedFocus = thread;
        focusSink(thread);
    }

    Entry& FindOrCreate(int handle, unsigned long thread) {
        for (std::size_t index = 0; index < used; ++index) {
            if (entries[index].handle == handle && entries[index].thread == thread) return entries[index];
        }
        if (used < Slots) {
            entries[used].handle = handle;
            entries[used].thread = thread;
            return entries[used++];
        }
        // Bounded cardinality: everything past Slots pairs folds into the aggregate slot.
        if (overflow.handle == 0) overflow.handle = -1;
        return overflow;
    }

    bool enabled = std::getenv("APS5_TRACE_SUBMIT_FLIP") != nullptr;
    bool started = false;
    std::uint64_t windowStart = 0;
    std::uint64_t pending = 0;
    void (*focusSink)(unsigned long) = nullptr;
    unsigned long notifiedFocus = 0;
    Entry entries[Slots]{};
    Entry overflow{};
    std::size_t used = 0;
};

// Second aggregate, single bucket (no per-thread cardinality): the guest-facing wait for flip-queue
// room. It is wall time spent blocking before a submission is accepted, not guest CPU time.
class RoomWaitTrace {
public:
    bool Enabled() const { return enabled; }

    void Note(std::uint64_t startNanos, std::uint64_t endNanos) {
        if (!enabled) return;
        std::lock_guard lock(mutex);
        const std::uint64_t duration = endNanos >= startNanos ? endNanos - startNanos : 0;
        ++count;
        sum += duration;
        if (duration > max) max = duration;
        if (duration > 1000000ull) ++over1ms;
        ++pending;
    }

    bool Report(std::uint64_t nowNanos, char* out, std::size_t size) {
        if (!enabled) return false;
        std::lock_guard lock(mutex);
        if (pending == 0) return false;
        if (!started) {
            started = true;
            windowStart = nowNanos;
            return false;
        }
        if (nowNanos - windowStart < windowNanos) return false;
        std::snprintf(out, size, "[flip-room %.3f..%.3f s] wait x%llu total %.1fms avg %.3f max %.3f ms over1ms %llu\n",
            static_cast<double>(windowStart) / 1e9, static_cast<double>(nowNanos) / 1e9,
            static_cast<unsigned long long>(count), static_cast<double>(sum) / 1e6,
            count != 0 ? static_cast<double>(sum) / 1e6 / static_cast<double>(count) : 0.0,
            static_cast<double>(max) / 1e6, static_cast<unsigned long long>(over1ms));
        windowStart = nowNanos;
        pending = 0;
        count = 0;
        sum = 0;
        max = 0;
        over1ms = 0;
        return true;
    }

private:
    static constexpr std::uint64_t windowNanos = 3000000000ull;
    bool enabled = std::getenv("APS5_TRACE_SUBMIT_FLIP") != nullptr;
    bool started = false;
    std::uint64_t windowStart = 0;
    std::uint64_t pending = 0;
    std::uint64_t count = 0, sum = 0, max = 0, over1ms = 0;
    std::mutex mutex;
};

inline RoomWaitTrace& FlipRoomStats() {
    static RoomWaitTrace trace;
    return trace;
}

inline SubmitFlipTrace& SubmitFlipStats() {
    static SubmitFlipTrace trace;
    return trace;
}

}
