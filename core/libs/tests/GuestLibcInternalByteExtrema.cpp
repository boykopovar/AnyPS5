#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

using Extremum = std::uint8_t (APS5_VABI*)(const void*, const void*);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal byte extrema: %s\n", message);
    std::abort();
}

void CheckValues(Extremum call, bool minimum) {
    for (unsigned first = 0; first < 256; ++first) {
        const std::uint8_t single = static_cast<std::uint8_t>(first);
        Require(call(&single, &single + 1) == single, "a singleton must return its value");
        for (unsigned second = 0; second < 256; ++second) {
            const std::array<std::uint8_t, 2> values{static_cast<std::uint8_t>(first), static_cast<std::uint8_t>(second)};
            const unsigned expected = minimum ? std::min(first, second) : std::max(first, second);
            Require(call(values.data(), values.data() + values.size()) == expected, "unsigned byte ordering must hold for every pair");
        }
    }
}

void CheckRanges(Extremum call, bool minimum) {
    for (const std::size_t size : {1u, 2u, 3u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 512u}) {
        for (std::size_t offset = 1; offset <= 32; ++offset) {
            std::vector<std::uint8_t> storage(offset + size + 1, minimum ? 0 : 255);
            for (std::size_t i = 0; i < size; ++i) storage[offset + i] = static_cast<std::uint8_t>(1 + ((i * 73 + offset * 19) % 254));
            const auto before = storage;
            const auto begin = storage.begin() + offset;
            const auto end = begin + size;
            const auto expected = minimum ? *std::min_element(begin, end) : *std::max_element(begin, end);
            Require(call(storage.data() + offset, storage.data() + offset + size) == expected, "range boundaries or unsigned ordering were ignored");
            Require(storage == before, "the input range was modified");
            for (const std::size_t position : {std::size_t{0}, size / 2, size - 1}) {
                const auto previous = storage[offset + position];
                storage[offset + position] = minimum ? 0 : 255;
                Require(call(storage.data() + offset, storage.data() + offset + size) == (minimum ? 0 : 255), "an extremum at the start, middle or end was missed");
                storage[offset + position] = previous;
            }
        }
    }
}

void CheckInvalidRanges(Extremum call) {
    std::array<std::uint8_t, 4> values{1, 2, 3, 4};
    const auto invalid = [&](const void* first, const void* last) {
        bool threw = false;
        try { static_cast<void>(call(first, last)); }
        catch (const std::invalid_argument&) { threw = true; }
        Require(threw, "a null, empty or reversed range must throw");
    };
    invalid(nullptr, nullptr);
    invalid(nullptr, values.data() + values.size());
    invalid(values.data(), nullptr);
    invalid(values.data(), values.data());
    invalid(values.data() + values.size(), values.data());
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "expected min or max");
    const bool minimum = std::strcmp(argv[1], "min") == 0;
    Require(minimum || std::strcmp(argv[1], "max") == 0, "unknown operation");
    const Extremum call = minimum ? __std_min_1u_nid_postfix : __std_max_1u_nid_postfix;
    CheckValues(call, minimum);
    CheckRanges(call, minimum);
    CheckInvalidRanges(call);
    return 0;
}
