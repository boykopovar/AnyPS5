#pragma once
#include <chrono>
#include <cstdint>

namespace KernelSocketSelect {

inline std::chrono::steady_clock::time_point DeadlineAfter(std::chrono::steady_clock::time_point start,
    std::int64_t seconds, std::int64_t microseconds) {
    using Clock = std::chrono::steady_clock;
    constexpr std::int64_t MicrosecondsPerSecond = 1000000;
    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(Clock::time_point::max() - start).count();
    if (seconds > remaining / MicrosecondsPerSecond) return Clock::time_point::max();
    const auto requested = seconds * MicrosecondsPerSecond;
    if (microseconds > remaining - requested) return Clock::time_point::max();
    return start + std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(requested + microseconds));
}

}
