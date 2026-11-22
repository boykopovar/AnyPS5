#include "prx/libc/include/Api.hpp"
#include "tests/GuardedBuffer.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestWmemcmpBoundary check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static int Sign(int value) {
    return (value > 0) - (value < 0);
}

int main() {
    GuardedBuffer first, second;
    for (std::size_t count = 1; count <= 128; ++count) {
        auto* left = first.At<char16_t>(count);
        auto* right = second.At<char16_t>(count);
        for (std::size_t position : {std::size_t{0}, count / 2, count - 1}) {
            for (char16_t a : {0, 1, 0xff, 0x100, 0x7fff, 0x8000, 0xd800, 0xffff}) {
                for (char16_t b : {0, 1, 0xff, 0x100, 0x7fff, 0x8000, 0xd800, 0xffff}) {
                    for (std::size_t index = 0; index < count; ++index) left[index] = right[index] = index % 3 == 0 ? 0 : static_cast<char16_t>(index * 577 + count);
                    left[position] = a;
                    right[position] = b;
                    if (position + 1 < count) { left[position + 1] = 0xffff; right[position + 1] = 1; }
                    int expected = 0;
                    for (std::size_t index = 0; index < count; ++index) {
                        if (left[index] != right[index]) { expected = left[index] > right[index] ? 1 : -1; break; }
                    }
                    const std::vector<char16_t> savedLeft(left, left + count), savedRight(right, right + count);
                    Require(Sign(wmemcmp_nid_postfix(left, right, count)) == expected);
                    Require(std::equal(savedLeft.begin(), savedLeft.end(), left) && std::equal(savedRight.begin(), savedRight.end(), right));
                }
            }
        }
    }
}
