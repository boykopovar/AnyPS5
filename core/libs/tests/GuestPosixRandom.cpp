#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Posix random check failed at line %u\n", location.line());
        std::abort();
    }
}

int main() {
    std::array<unsigned char, 4099> first{};
    std::array<unsigned char, 4099> second{};
    first.fill(0xa5);
    arc4random_buf_nid_postfix(first.data(), 0);

    Require(std::all_of(first.begin(), first.end(), [](unsigned char value) { return value == 0xa5; }));
    for (const std::size_t size : {1u, 3u, 5u, 4098u}) {
        first.fill(0xa5);
        arc4random_buf_nid_postfix(first.data(), size);

        Require(std::all_of(first.begin() + static_cast<std::ptrdiff_t>(size), first.end(),
            [](unsigned char value) { return value == 0xa5; }));
    }
    arc4random_buf_nid_postfix(first.data(), first.size());
    arc4random_buf_nid_postfix(second.data(), second.size());

    Require(first != second);
    std::array<std::size_t, 256> counts{};
    for (const auto value : first) ++counts[value];
    Require(*std::max_element(counts.begin(), counts.end()) < 64);

    constexpr std::size_t Draws = 65536;
    static std::array<std::uint32_t, Draws> words{};
    for (auto& word : words) word = arc4random_nid_postfix();
    std::array<std::size_t, 32> bits{};
    std::array<std::size_t, 16> nibbles{};
    for (const auto word : words) {
        for (std::size_t bit = 0; bit < bits.size(); ++bit) bits[bit] += (word >> bit) & 1u;
        ++nibbles[word >> 28];
    }
    for (const auto count : bits) Require(count > Draws / 2 - 1500 && count < Draws / 2 + 1500);
    for (const auto count : nibbles) Require(count > Draws / 16 - 600 && count < Draws / 16 + 600);
    Require(*std::min_element(words.begin(), words.end()) < 0x00100000u);
    Require(*std::max_element(words.begin(), words.end()) > 0xfff00000u);
    std::sort(words.begin(), words.end());
    std::size_t repeats = 0;
    for (std::size_t index = 1; index < Draws; ++index) repeats += words[index] == words[index - 1];
    Require(repeats < 16);
    return 0;
}
