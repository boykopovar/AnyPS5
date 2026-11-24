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
        std::fputs("GuestWmemsetExtent check failed\n", stderr);
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
    GuardedBuffer output;
    for (std::size_t count = 1; count <= 192; ++count) {
        auto* destination = output.At<char16_t>(count);
        for (char16_t value : {0, 1, 0x0100, 0x7fff, 0x8000, 0xd800, 0xdc00, 0xffff}) {
            std::memset(destination - 8, 0xa5, (count + 8) * sizeof(char16_t));
            Require(wmemset_nid_postfix(destination, value, count) == destination);
            for (std::size_t index = 0; index < count; ++index) Require(destination[index] == value);
            Prefix(destination);
        }
    }
}
