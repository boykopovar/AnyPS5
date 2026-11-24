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
        std::fputs("GuestWmemchrBoundary check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    GuardedBuffer input;
    const std::array<char16_t, 7> values{0, 1, 0xff, 0x100, 0x8000, 0xd800, 0xffff};
    for (std::size_t length = 0; length <= 192; ++length) {
        auto* source = input.At<char16_t>(length);
        const auto extent = (std::max)(length, std::size_t{1});
        for (std::size_t i = 0; i < extent; ++i) source[i] = values[(i + length) % values.size()];
        const std::vector<char16_t> saved(source, source + extent);
        for (char16_t target : {0, 1, 0xff, 0x100, 0x8000, 0xd800, 0xdc00, 0xffff}) {
            const auto found = std::find(saved.begin(), saved.begin() + length, target);
            const auto* expected = found == saved.begin() + length ? nullptr : source + (found - saved.begin());
            Require(wmemchr_nid_postfix(source, target, length) == expected);
            Require(std::equal(saved.begin(), saved.end(), source));
        }
    }
}
