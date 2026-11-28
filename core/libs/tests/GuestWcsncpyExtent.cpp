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
        std::fputs("GuestWcsncpyExtent check failed\n", stderr);
        std::abort();
    }
}

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    GuardedBuffer input, output;
    for (std::size_t count = 1; count <= 128; ++count) {
        for (std::size_t length : {std::size_t{0}, std::size_t{1}, count - 1, count, count + 7}) {
            auto* source = input.At<char16_t>(length + 1);
            for (std::size_t index = 0; index < length; ++index) source[index] = static_cast<char16_t>(0x100 + index * 257);
            source[length] = 0;
            const std::vector<char16_t> saved(source, source + length + 1);
            auto* destination = output.At<char16_t>(count);
            std::memset(destination - 8, 0xa5, (count + 8) * sizeof(char16_t));
            Require(wcsncpy_nid_postfix(destination, source, count) == destination);
            for (std::size_t index = 0; index < count; ++index) Require(destination[index] == (index < length ? saved[index] : 0));
            Require(std::equal(saved.begin(), saved.end(), source));
            Prefix(destination);
        }
    }
}
