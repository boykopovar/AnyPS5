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

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestWcslenBoundary check failed\n", stderr);
        std::abort();
    }
}

int main() {
    GuardedBuffer input;
    const std::array<char16_t, 6> values{0x100, 1, 0x8000, 0xd800, 0xdc00, 0xffff};
    for (std::size_t length = 0; length <= 256; ++length) {
        auto* source = input.At<char16_t>(length + 1);
        for (std::size_t i = 0; i < length; ++i) source[i] = values[(i + length) % values.size()];
        source[length] = 0;
        const std::u16string saved(source, length + 1);
        Require(wcslen_nid_postfix(source) == length);
        Require(std::equal(saved.begin(), saved.end(), source));
    }
}
