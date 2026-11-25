#pragma once
#include <chrono>
#include <cstdint>
#include <ratio>
#include <type_traits>

namespace KernelSocketSelect {

inline std::chrono::steady_clock::time_point DeadlineAfter(std::chrono::steady_clock::time_point start,
    std::int64_t seconds, std::int64_t microseconds) {
    using Clock = std::chrono::steady_clock;
    using Rep = Clock::rep;
    using UnsignedRep = std::make_unsigned_t<Rep>;
    using SecRatio = std::ratio_divide<std::ratio<1>, Clock::period>;
    using UsecRatio = std::ratio_divide<std::micro, Clock::period>;

    constexpr std::int64_t MicrosecondsPerSecond = 1000000;
    std::int64_t extraSeconds = microseconds / MicrosecondsPerSecond;
    std::int64_t remMicroseconds = microseconds % MicrosecondsPerSecond;
    if (remMicroseconds < 0) {
        remMicroseconds += MicrosecondsPerSecond;
        --extraSeconds;
    }

    const auto absUnsigned = [](std::int64_t value) -> std::uint64_t {
        return value >= 0
            ? static_cast<std::uint64_t>(value)
            : (0ULL - static_cast<std::uint64_t>(value));
    };

    bool sumNegative = false;
    std::uint64_t absSumSeconds = 0;
    if ((seconds >= 0) == (extraSeconds >= 0)) {
        sumNegative = seconds < 0;
        absSumSeconds = absUnsigned(seconds) + absUnsigned(extraSeconds);
    } else {
        const std::uint64_t absA = absUnsigned(seconds);
        const std::uint64_t absB = absUnsigned(extraSeconds);
        if (absA >= absB) {
            sumNegative = seconds < 0;
            absSumSeconds = absA - absB;
        } else {
            sumNegative = extraSeconds < 0;
            absSumSeconds = absB - absA;
        }
    }

    bool negative = false;
    std::uint64_t wholeSeconds = absSumSeconds;
    std::uint64_t fracMicroseconds = static_cast<std::uint64_t>(remMicroseconds);
    if (sumNegative && absSumSeconds > 0) {
        negative = true;
        if (remMicroseconds > 0) {
            --wholeSeconds;
            fracMicroseconds = static_cast<std::uint64_t>(MicrosecondsPerSecond - remMicroseconds);
        }
    }

    const UnsignedRep startTicks = static_cast<UnsignedRep>(start.time_since_epoch().count());
    const UnsignedRep maxTicks = static_cast<UnsignedRep>(Clock::time_point::max().time_since_epoch().count());
    const UnsignedRep minTicks = static_cast<UnsignedRep>(Clock::time_point::min().time_since_epoch().count());
    const UnsignedRep headroom = negative ? (startTicks - minTicks) : (maxTicks - startTicks);
    const Clock::time_point limit = negative ? Clock::time_point::min() : Clock::time_point::max();

    constexpr UnsignedRep ticksPerSecond = static_cast<UnsignedRep>(SecRatio::num);
    constexpr std::uint64_t usecNum = static_cast<std::uint64_t>(UsecRatio::num);
    constexpr std::uint64_t usecDen = static_cast<std::uint64_t>(UsecRatio::den);

    UnsignedRep deltaTicks = 0;
    if constexpr (SecRatio::den == 1) {
        if (wholeSeconds > static_cast<std::uint64_t>(headroom / ticksPerSecond)) return limit;
        const UnsignedRep secTicks = static_cast<UnsignedRep>(wholeSeconds) * ticksPerSecond;
        const UnsignedRep remHeadroom = headroom - secTicks;
        const std::uint64_t fracTicks = (fracMicroseconds * usecNum) / usecDen;
        if (fracTicks > static_cast<std::uint64_t>(remHeadroom)) return limit;
        deltaTicks = secTicks + static_cast<UnsignedRep>(fracTicks);
    } else {
        const std::uint64_t secDen = static_cast<std::uint64_t>(SecRatio::den);
        const std::uint64_t q = wholeSeconds / secDen;
        const std::uint64_t r = wholeSeconds % secDen;
        if (q > static_cast<std::uint64_t>(headroom / ticksPerSecond)) return limit;
        const UnsignedRep qTicks = static_cast<UnsignedRep>(q) * ticksPerSecond;
        const UnsignedRep remHeadroom = headroom - qTicks;
        const std::uint64_t rTicks = ((r * static_cast<std::uint64_t>(MicrosecondsPerSecond) + fracMicroseconds) * usecNum) / usecDen;
        if (rTicks > static_cast<std::uint64_t>(remHeadroom)) return limit;
        deltaTicks = qTicks + static_cast<UnsignedRep>(rTicks);
    }

    const UnsignedRep resultTicks = negative ? (startTicks - deltaTicks) : (startTicks + deltaTicks);
    return Clock::time_point(Clock::duration(static_cast<Rep>(resultTicks)));
}

}
