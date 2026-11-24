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
        std::fputs("GuestMemcmpProtected check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static int Sign(int value) { return (value > 0) - (value < 0); }

int main() {
    GuardedBuffer left, right;
    auto* a = left.At(1);
    auto* b = right.At(1);
    for (unsigned x = 0; x < 256; ++x) {
        for (unsigned y = 0; y < 256; ++y) {
            *a = static_cast<unsigned char>(x);
            *b = static_cast<unsigned char>(y);
            Require(Sign(memcmp_nid_postfix(a, b, 1)) == Sign(static_cast<int>(x) - static_cast<int>(y)));
            Require(*a == x && *b == y);
        }
    }
    for (std::size_t length = 0; length <= 128; ++length) {
        a = left.At(length);
        b = right.At(length);
        const auto extent = (std::max)(length, std::size_t{1});
        std::memset(a, 0x80, extent);
        std::memset(b, 0x80, extent);
        Require(memcmp_nid_postfix(a, b, length) == 0);
        for (std::size_t position = 0; position < length; ++position) {
            a[position] = 0xff;
            b[position] = 0x7f;
            if (position + 1 < length) { a[position + 1] = 0; b[position + 1] = 0xff; }
            Require(memcmp_nid_postfix(a, b, length) > 0);
            Require(memcmp_nid_postfix(b, a, length) < 0);
            Require(memcmp_nid_postfix(a, b, position) == 0);
            std::memset(a, 0x80, extent);
            std::memset(b, 0x80, extent);
        }
    }
    for (std::size_t length : {160, 256, 512, 1024, 2048}) {
        a = left.At(length);
        b = right.At(length);
        std::memset(a, 0x33, length);
        std::memset(b, 0x33, length);
        Require(memcmp_nid_postfix(a, b, length) == 0);
        Require(memcmp_nid_postfix(a, a, length) == 0);

        a[0] = 0x55;
        Require(memcmp_nid_postfix(a, b, length) > 0);
        Require(memcmp_nid_postfix(b, a, length) < 0);
        a[0] = 0x33;

        a[length - 1] = 0x77;
        Require(memcmp_nid_postfix(a, b, length) > 0);
        Require(memcmp_nid_postfix(b, a, length) < 0);
        Require(memcmp_nid_postfix(a, b, length - 1) == 0);
        a[length - 1] = 0x33;
    }
}
