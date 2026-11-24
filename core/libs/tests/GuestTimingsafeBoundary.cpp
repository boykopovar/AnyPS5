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
        std::fputs("GuestTimingsafeBoundary check failed\n", stderr);
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
    for (std::size_t length = 0; length <= 128; ++length) {
        auto* left = first.At(length);
        auto* right = second.At(length);
        const auto extent = (std::max)(length, std::size_t{1});
        for (int mode = 0; mode < 4; ++mode) {
            for (std::size_t i = 0; i < extent; ++i) left[i] = right[i] = static_cast<unsigned char>(i * 131 + length);
            if (length && mode < 3) {
                const auto position = mode == 0 ? 0 : mode == 1 ? length / 2 : length - 1;
                left[position] = 0xff; right[position] = 1;
                if (position + 1 < length) { left[position + 1] = 1; right[position + 1] = 0xff; }
            }
            int expected = 0;
            for (std::size_t i = 0; i < length; ++i) if (left[i] != right[i]) { expected = left[i] > right[i] ? 1 : -1; break; }
            const std::vector<unsigned char> a(left, left + extent), b(right, right + extent);
            Require(Sign(timingsafe_memcmp_nid_postfix(left, right, length)) == expected);
            Require(Sign(timingsafe_memcmp_nid_postfix(right, left, length)) == -expected);
            Require(std::equal(a.begin(), a.end(), left) && std::equal(b.begin(), b.end(), right));
        }
    }
}
