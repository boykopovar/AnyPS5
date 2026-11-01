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
}
