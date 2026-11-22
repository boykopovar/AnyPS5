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
        std::fputs("GuestWideCharBoundary check failed\n", stderr);
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
    GuardedBuffer buffer;
    const std::array<char16_t, 6> values{1, 0x100, 0x7fff, 0x8000, 0xd800, 0xffff};
    for (std::size_t length = 0; length <= 192; ++length) {
        auto* source = buffer.At<char16_t>(length + 1);
        for (std::size_t i = 0; i < length; ++i) source[i] = values[(i + length) % values.size()];
        source[length] = 0;
        const std::u16string saved(source, length + 1);
        for (char16_t target : {0, 1, 0x100, 0x7fff, 0x8000, 0xd800, 0xdc00, 0xffff}) {
            const char16_t* first = nullptr;
            const char16_t* last = nullptr;
            for (std::size_t i = 0; i <= length; ++i) if (source[i] == target) {
                if (!first) first = source + i;
                last = source + i;
            }
            Require(wcschr_nid_postfix(source, target) == first);
            Require(wcsrchr_nid_postfix(source, target) == last);
            Require(std::equal(saved.begin(), saved.end(), source));
        }
    }
}
