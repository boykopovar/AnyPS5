#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_RELOCATIONRUNS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_RELOCATIONRUNS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace AgcDriver::DriverDetail {

template <typename Emit>
void ForEachRelocationRun(std::uint64_t address, std::size_t words, std::span<const std::pair<std::uint64_t, std::int32_t>> reads, std::uint64_t relocatable, Emit&& emit) {
    if (words == 0) return;
    auto read = std::lower_bound(reads.begin(), reads.end(), std::make_pair(address, std::numeric_limits<std::int32_t>::min()));
    std::size_t start = 0;
    std::size_t taggedEnd = 0;
    std::int8_t current = -1;
    const auto append = [&](std::size_t end) {
        if (start != end) emit(start, end, current);
        start = end;
    };
    while (read != reads.end()) {
        const auto [at, origin] = *read;
        do { ++read; } while (read != reads.end() && read->first == at);
        const auto distance = at - address;
        if (distance / sizeof(std::uint32_t) >= words) break;
        if (distance % sizeof(std::uint32_t) != 0 || origin < 0 || origin >= 64 || (relocatable >> origin & 1u) == 0) continue;
        const auto word = static_cast<std::size_t>(distance / sizeof(std::uint32_t));
        if (word > taggedEnd && current != -1) {
            append(taggedEnd);
            current = -1;
        }
        const auto next = static_cast<std::int8_t>(origin);
        if (next != current) {
            append(word);
            current = next;
        }
        taggedEnd = word + 1;
    }
    if (current != -1 && taggedEnd < words) {
        append(taggedEnd);
        current = -1;
    }
    append(words);
}

}

#endif
