#include "prx/libSceNpCommon/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

int main() {
    constexpr int tooSmall = static_cast<int>(0x80558008);
    const std::array<unsigned char, 4> input{0, 0x7f, 0x80, 0xff};
    std::array<char, 16> output{};
    for (const auto length : {std::numeric_limits<std::size_t>::max() / 2 + 1,
                              std::numeric_limits<std::size_t>::max()}) {
        output.fill('!');
        Require(sceNpStrBuildHex(output.data() + 1, 8, input.data(), length, false) == tooSmall);
        Require(std::all_of(output.begin(), output.end(), [](char c) { return c == '!'; }));
    }
    for (const bool uppercase : {false, true}) {
        for (std::size_t count = 0; count <= input.size(); ++count) {
            for (std::size_t capacity = 0; capacity <= count * 2 + 2; ++capacity) {
                output.fill('!');
                const auto result = sceNpStrBuildHex(output.data() + 1, capacity, input.data(), count, uppercase);
                Require(output.front() == '!' && output[1 + capacity] == '!');
                if (capacity <= count * 2) {
                    Require(result == tooSmall);
                    Require(std::all_of(output.begin(), output.end(), [](char c) { return c == '!'; }));
                } else {
                    Require(result == 0 && output[1 + count * 2] == '\0');
                    const char* expected = uppercase ? "007F80FF" : "007f80ff";
                    Require(std::memcmp(output.data() + 1, expected, count * 2) == 0);
                }
            }
        }
    }
}
