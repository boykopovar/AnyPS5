#include "prx/libc/include/Api.hpp"
#include "tests/GuardedBuffer.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestStrcollBoundary check failed\n", stderr);
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
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* left = first.At(count + 1);
        auto* right = second.At(count + 1);
        for (std::size_t index = 0; index < count; ++index) left[index] = static_cast<unsigned char>(1 + (index * 73 + count) % 255);
        left[count] = 0;
        const std::vector<unsigned char> saved(left, left + count + 1);
        for (std::size_t position = 0; position <= count; ++position) {
            for (unsigned char value : {0, 1, 127, 128, 255}) {
                if (position == count && value != 0) continue;
                std::copy(saved.begin(), saved.end(), right);
                right[position] = value;
                int expected = 0;
                for (std::size_t index = 0; index <= count; ++index) {
                    if (left[index] != right[index]) { expected = static_cast<int>(left[index]) - right[index]; break; }
                    if (left[index] == 0) break;
                }
                const std::vector<unsigned char> other(right, right + count + 1);
                Require(Sign(strcoll_nid_postfix(reinterpret_cast<char*>(left), reinterpret_cast<char*>(right))) == Sign(expected));
                Require(std::equal(saved.begin(), saved.end(), left));
                Require(std::equal(other.begin(), other.end(), right));
            }
        }
    }
}
