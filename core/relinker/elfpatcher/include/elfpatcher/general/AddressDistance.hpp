#pragma once
#include <cstdint>
#include <limits>

namespace Elfpatcher {

[[nodiscard]] inline bool SignedDistance(const std::uint64_t to, const std::uint64_t from, std::int64_t& out) {
    constexpr std::uint64_t kSignBoundary = 1ULL << 63;
    constexpr std::uint64_t kInt64MinMagnitude = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1ULL;
    if ((to < kSignBoundary) == (from < kSignBoundary)) {
        out = static_cast<std::int64_t>(to - from);
        return true;
    }
    const std::uint64_t high = to < kSignBoundary ? from : to;
    const std::uint64_t low = to < kSignBoundary ? to : from;
    const std::uint64_t magnitude = high - low;
    if (magnitude >= kInt64MinMagnitude)
        return false;
    out = high == to ? static_cast<std::int64_t>(magnitude) : -static_cast<std::int64_t>(magnitude);
    return true;
}

}
