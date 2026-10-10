#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>

extern "C" void APS5_VABI arc4random_buf_nid_postfix(void*, std::size_t);

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
    return 0;
}
