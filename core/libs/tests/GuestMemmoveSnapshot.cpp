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
        std::fputs("GuestMemmoveSnapshot check failed\n", stderr);
        std::abort();
    }
}

int main() {
    for (std::size_t length = 0; length <= 96; ++length) {
        for (std::size_t source = 16; source <= 48; ++source) {
            for (std::size_t destination = 16; destination <= 48; ++destination) {
                std::array<unsigned char, 192> original{};
                for (std::size_t i = 0; i < original.size(); ++i) original[i] = static_cast<unsigned char>(i * 73 + length * 11);
                auto actual = original;
                auto expected = original;
                for (std::size_t i = 0; i < length; ++i) expected[destination + i] = original[source + i];
                Require(memmove_nid_postfix(actual.data() + destination, actual.data() + source, length) == actual.data() + destination);
                Require(actual == expected);
            }
        }
    }
}
