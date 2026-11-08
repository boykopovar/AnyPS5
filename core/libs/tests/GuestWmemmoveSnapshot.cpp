#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestWmemmoveSnapshot check failed\n", stderr);
        std::abort();
    }
}

int main() {
    constexpr std::array<char16_t, 8> values = {0, 1, 0xff, 0xff00, 0xd800, 0xdc00, 0xffff, 0x1234};
    for (std::size_t count = 0; count <= 96; ++count) {
        for (std::size_t source = 16; source <= 48; ++source) {
            for (std::size_t destination = 16; destination <= 48; ++destination) {
                std::array<char16_t, 192> original{};
                for (std::size_t i = 0; i < original.size(); ++i) original[i] = values[(i + count) % values.size()];
                auto actual = original, expected = original;
                for (std::size_t i = 0; i < count; ++i) expected[destination + i] = original[source + i];
                Require(wmemmove_nid_postfix(actual.data() + destination, actual.data() + source, count) == actual.data() + destination);
                Require(actual == expected);
            }
        }
    }

    std::vector<char16_t> large(2048);
    for (std::size_t i = 0; i < large.size(); ++i) large[i] = static_cast<char16_t>(i * 31 + 7);
    const auto expectedLarge = large;
    Require(wmemmove_nid_postfix(large.data(), large.data(), large.size()) == large.data());
    Require(large == expectedLarge);

    auto forwardActual = large;
    auto forwardExpected = large;
    std::memmove(forwardExpected.data() + 64, forwardExpected.data() + 16, 1024 * sizeof(char16_t));
    Require(wmemmove_nid_postfix(forwardActual.data() + 64, forwardActual.data() + 16, 1024) == forwardActual.data() + 64);
    Require(forwardActual == forwardExpected);

    auto backwardActual = large;
    auto backwardExpected = large;
    std::memmove(backwardExpected.data() + 16, backwardExpected.data() + 64, 1024 * sizeof(char16_t));
    Require(wmemmove_nid_postfix(backwardActual.data() + 16, backwardActual.data() + 64, 1024) == backwardActual.data() + 16);
    Require(backwardActual == backwardExpected);

    Require(wmemmove_nid_postfix(large.data() + 100, large.data() + 200, 0) == large.data() + 100);
    Require(large == expectedLarge);
}
