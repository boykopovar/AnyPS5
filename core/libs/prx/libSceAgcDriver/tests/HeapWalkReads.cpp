#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void walkReadRunsTests() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    const auto next = [&] {
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        return state;
    };
    for (int round = 0; round < 512; ++round) {
        std::vector<std::uint64_t> words;
        const auto count = 1u + next() % 96u;
        std::uint64_t cursor = 0x10000u + (next() % 64u) * 4u;
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto pick = next() % 8u;
            if (pick < 4u) cursor += 4u;
            else if (pick == 4u) cursor = 0x10000u + (next() % 512u) * 4u;
            else if (pick == 5u) cursor += 2u;
            else if (pick == 6u && cursor >= 0x10008u) cursor -= 8u;
            words.push_back(cursor);
        }
        std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
        for (const auto word : words) AgcDriver::DriverDetail::AppendWalkRead(runs, word);
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        AgcDriver::DriverDetail::CoalesceWalkReads(runs, ranges);
        auto sorted = words;
        std::sort(sorted.begin(), sorted.end());
        std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
        for (const auto word : sorted) {
            if (!expected.empty() && word <= expected.back().second) {
                expected.back().second = std::max(expected.back().second, word + 4u);
                continue;
            }
            expected.emplace_back(word, word + 4u);
        }
        require(ranges == expected, "the heap walk's read ranges in round " + std::to_string(round) + " differ from the merged ranges of its " + std::to_string(words.size()) + " word reads");
    }
    std::cout << "the heap walk's read runs merge to the same ranges as its sorted word reads\n";
}

}

int main() {
    try {
        walkReadRunsTests();
        std::cout << "heap walk read tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "heap walk read tests failed: " << error.what() << '\n';
        return 1;
    }
}
