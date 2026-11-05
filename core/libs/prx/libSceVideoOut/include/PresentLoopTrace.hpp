#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace VideoOutTrace {

// Opt-in aggregate for the host present loop: how long the thread blocks waiting for a flip
// request and how long the present call takes (that call contains the fence and GPU-mutex waits).
// One bounded line per window; no per-call output, no allocation, no flush except when a line is
// produced. Disabled it only checks a bool.
class PresentLoopTrace {
public:
    explicit PresentLoopTrace(bool enabled) : enabled(enabled) {}

    bool Enabled() const { return enabled; }

    void NoteFrame() {
        if (!enabled) return;
        ++windowFrames;
    }

    void NoteWait(std::uint64_t nanos) {
        if (!enabled) return;
        ++waitCalls;
        waitNanos += nanos;
        if (nanos > waitMax) waitMax = nanos;
    }

    void NotePresent(std::uint64_t nanos) {
        if (!enabled) return;
        ++presentCalls;
        presentNanos += nanos;
        if (nanos > presentMax) presentMax = nanos;
    }

    // Returns true and fills `out` when the window (default 3 s) elapsed; resets the counters.
    bool Report(std::uint64_t nowNanos, char* out, std::size_t size) {
        if (!enabled) return false;
        if (!started) {
            started = true;
            windowStart = nowNanos;
            return false;
        }
        if (nowNanos - windowStart < windowNanos) return false;
        const std::uint64_t span = nowNanos - windowStart;
        std::snprintf(out, size,
            "[present-loop %.3f..%.3f s] frames %llu | flip-wait x%llu %.1fms avg %.2f max %.2f | present x%llu %.1fms avg %.2f max %.2f\n",
            static_cast<double>(windowStart) / 1e9,
            static_cast<double>(nowNanos) / 1e9,
            static_cast<unsigned long long>(windowFrames),
            static_cast<unsigned long long>(waitCalls),
            static_cast<double>(waitNanos) / 1e6,
            waitCalls != 0 ? static_cast<double>(waitNanos) / 1e6 / static_cast<double>(waitCalls) : 0.0,
            static_cast<double>(waitMax) / 1e6,
            static_cast<unsigned long long>(presentCalls),
            static_cast<double>(presentNanos) / 1e6,
            presentCalls != 0 ? static_cast<double>(presentNanos) / 1e6 / static_cast<double>(presentCalls) : 0.0,
            static_cast<double>(presentMax) / 1e6);
        (void)span;
        windowStart = nowNanos;
        windowFrames = waitCalls = presentCalls = 0;
        waitNanos = presentNanos = waitMax = presentMax = 0;
        return true;
    }

private:
    static constexpr std::uint64_t windowNanos = 3000000000ull;

    bool enabled;
    bool started = false;
    std::uint64_t windowStart = 0;
    std::uint64_t windowFrames = 0;
    std::uint64_t waitCalls = 0, waitNanos = 0, waitMax = 0;
    std::uint64_t presentCalls = 0, presentNanos = 0, presentMax = 0;
};

}
