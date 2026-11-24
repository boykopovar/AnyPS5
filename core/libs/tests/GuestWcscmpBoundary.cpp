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
        std::fputs("GuestWcscmpBoundary check failed\n", stderr);
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
        auto* left = first.At<char16_t>(length + 1);
        auto* right = second.At<char16_t>(length + 1);
        for (std::size_t i = 0; i < length; ++i) left[i] = static_cast<char16_t>(0x100 + i * 257);
        left[length] = 0;
        const std::u16string saved(left, length + 1);
        for (std::size_t position = 0; position <= length; ++position) {
            for (char16_t value : {0, 1, 0xff, 0x100, 0x8000, 0xd800, 0xffff}) {
                if (position == length && value != 0) continue;
                std::copy(saved.begin(), saved.end(), right);
                right[position] = value;
                int expected = 0;
                for (std::size_t i = 0; i <= length; ++i) {
                    if (left[i] != right[i]) { expected = left[i] > right[i] ? 1 : -1; break; }
                    if (left[i] == 0) break;
                }
                const std::u16string other(right, length + 1);
                Require(Sign(wcscmp_nid_postfix(left, right)) == expected);
                Require(Sign(wcscmp_nid_postfix(right, left)) == -expected);
                Require(std::equal(saved.begin(), saved.end(), left) && std::equal(other.begin(), other.end(), right));
            }
        }
    }
}
