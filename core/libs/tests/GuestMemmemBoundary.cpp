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
        std::fputs("GuestMemmemBoundary check failed\n", stderr);
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
    GuardedBuffer input, pattern;
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* source = input.At(count);
        for (std::size_t size = 0; size <= 17; ++size) {
            auto* needle = pattern.At(size);
            for (std::size_t index = 0; index < size; ++index) needle[index] = static_cast<unsigned char>(index % 5);
            for (int mode = 0; mode < 4; ++mode) {
                for (std::size_t index = 0; index < (std::max)(count, std::size_t{1}); ++index) source[index] = static_cast<unsigned char>((index + 1) % 5);
                if (count >= size && size != 0) {
                    const auto position = mode == 0 ? 0 : mode == 1 ? (count - size) / 2 : count - size;
                    std::copy(needle, needle + size, source + position);
                    if (mode == 3) source[position + size - 1] = 0xff;
                }
                const std::vector<unsigned char> saved(source, source + (std::max)(count, std::size_t{1}));
                const std::vector<unsigned char> savedNeedle(needle, needle + (std::max)(size, std::size_t{1}));
                const auto found = std::search(saved.begin(), saved.begin() + count, savedNeedle.begin(), savedNeedle.begin() + size);
                const void* expected = size == 0 ? source : found == saved.begin() + count ? nullptr : source + (found - saved.begin());
                Require(memmem_nid_postfix(source, count, needle, size) == expected);
                Require(std::equal(saved.begin(), saved.end(), source));
                Require(std::equal(savedNeedle.begin(), savedNeedle.end(), needle));
            }
        }
    }
}
