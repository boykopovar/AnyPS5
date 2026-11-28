#ifndef CORE_LIBS_PRX_LIBKERNEL_SYS_MQUEUEWAIT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYS_MQUEUEWAIT_HPP

#include <cstdint>
#include <limits>

namespace MqueueWait {

inline std::uint64_t MonotonicDeadline(std::int64_t absoluteSeconds, std::int64_t absoluteNanoseconds,
                                       std::int64_t realtimeSeconds, std::int64_t realtimeNanoseconds,
                                       std::uint64_t monotonicNow) {
    if (absoluteSeconds < realtimeSeconds ||
        (absoluteSeconds == realtimeSeconds && absoluteNanoseconds <= realtimeNanoseconds)) return monotonicNow;
    std::uint64_t seconds = static_cast<std::uint64_t>(absoluteSeconds) - static_cast<std::uint64_t>(realtimeSeconds);
    std::int64_t nanoseconds = absoluteNanoseconds - realtimeNanoseconds;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000;
    }
    constexpr std::uint64_t NanosecondsPerSecond = 1000000000ULL;
    constexpr std::uint64_t Maximum = std::numeric_limits<std::uint64_t>::max();
    if (seconds > (Maximum - monotonicNow) / NanosecondsPerSecond) return Maximum;
    const std::uint64_t deadline = monotonicNow + seconds * NanosecondsPerSecond;
    const std::uint64_t remainder = static_cast<std::uint64_t>(nanoseconds);
    return remainder > Maximum - deadline ? Maximum : deadline + remainder;
}

template <typename Ready, typename Now, typename WaitUntil>
bool WaitUntilMonotonic(std::uint64_t deadline, Ready ready, Now now, WaitUntil waitUntil) {
    constexpr std::uint64_t MaxWaitChunkNanos = 24ULL * 60 * 60 * 1000000000ULL;
    for (;;) {
        if (ready()) return true;
        const std::uint64_t current = now();
        if (deadline <= current) return ready();
        const std::uint64_t remaining = deadline - current;
        const std::uint64_t chunk = remaining < MaxWaitChunkNanos ? remaining : MaxWaitChunkNanos;
        if (waitUntil(current + chunk, ready)) return true;
        if (chunk == remaining) return ready();
    }
}

}

#endif
