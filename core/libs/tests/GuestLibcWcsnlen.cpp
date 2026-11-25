#include "prx/libc/include/Api.hpp"
#include "tests/GuardedBuffer.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestLibcWcsnlen check failed\n", stderr);
        std::abort();
    }
}

int main() {
    Require(wcsnlen_nid_postfix(u"", 5) == 0);
    Require(wcsnlen_nid_postfix(u"abc", 10) == 3);
    Require(wcsnlen_nid_postfix(u"abc", 2) == 2);
    Require(wcsnlen_nid_postfix(u"abc", 0) == 0);
    Require(wcsnlen_nid_postfix(u"abcdef", 3) == 3);

    GuardedBuffer input;
    for (std::size_t limit = 0; limit <= 64; ++limit) {
        const auto extent = limit == 0 ? std::size_t{1} : limit;
        auto* source = reinterpret_cast<char16_t*>(input.At(extent * sizeof(char16_t)));
        for (std::size_t stop : {limit, std::size_t{0}, limit / 2}) {
            for (std::size_t i = 0; i < extent; ++i) source[i] = static_cast<char16_t>(0x100 + i);
            if (stop < limit) source[stop] = 0;
            Require(wcsnlen_nid_postfix(source, limit) == stop);
        }
    }

    bool threw = false;
    try {
        wcsnlen_nid_postfix(nullptr, 4);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    return 0;
}
