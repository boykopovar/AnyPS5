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
        std::fputs("GuestWmemcpyExtent check failed\n", stderr);
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
    GuardedBuffer input, output;
    for (std::size_t count = 1; count <= 192; ++count) {
        auto* source = input.At<char16_t>(count);
        auto* destination = output.At<char16_t>(count);
        for (std::size_t index = 0; index < count; ++index) source[index] = index % 7 == 0 ? 0 : static_cast<char16_t>(index * 3571 + count * 256);
        const std::vector<char16_t> saved(source, source + count);
        std::memset(destination - 8, 0xa5, (count + 8) * sizeof(char16_t));
        Require(wmemcpy_nid_postfix(destination, source, count) == destination);
        Require(std::equal(saved.begin(), saved.end(), destination));
        Require(std::equal(saved.begin(), saved.end(), source));
        Prefix(destination);
    }
}
