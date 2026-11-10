#pragma once
#include <cstdint>

namespace KernelSocketArrival {

inline std::int64_t CounterAge(std::uint64_t arrived, std::uint64_t current, std::uint64_t rate, bool systemCounter) {
    constexpr std::uint64_t Second = 1000000000;
    constexpr std::uint64_t LongestSeconds = INT64_MAX / Second - 1;
    if (!systemCounter || rate == 0 || rate > UINT64_MAX / Second || arrived == 0 || arrived > current) return 0;
    const std::uint64_t elapsed = current - arrived;
    if (elapsed / rate > LongestSeconds) return 0;
    return static_cast<std::int64_t>(elapsed / rate * Second + elapsed % rate * Second / rate);
}

}
