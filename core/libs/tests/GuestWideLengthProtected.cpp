#include "prx/libc/include/Api.hpp"
#include "tests/GuardedBuffer.hpp"
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
        std::fputs("GuestWideLengthProtected check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

int main() {
    GuardedBuffer input;
    constexpr std::array<char16_t, 5> values = {0x100, 0xff00, 0xd800, 0xdc00, 0xffff};
    for (std::size_t limit = 0; limit <= 256; ++limit) {
        const auto extent = (std::max)(limit, std::size_t{1});
        auto* source = reinterpret_cast<char16_t*>(input.At(extent * sizeof(char16_t)));
        for (std::size_t stop : {limit, std::size_t{0}, limit / 2}) {
            for (std::size_t i = 0; i < extent; ++i) source[i] = values[i % values.size()];
            if (stop < limit) source[stop] = 0;
            const std::vector<char16_t> before(source, source + extent);
            Require(wcsnlen_s_nid_postfix(source, limit) == stop);
            Require(std::equal(before.begin(), before.end(), source));
        }
    }
}
