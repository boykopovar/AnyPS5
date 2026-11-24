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
        std::fputs("GuestStrpbrkBoundary check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

int main() {
    GuardedBuffer input, accept;
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* source = input.At<char>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char>(1 + (index * 31 + count) % 255);
        source[count] = 0;
        const std::string saved(source, count + 1);
        for (std::size_t size : {0u, 1u, 3u, 16u, 255u}) {
            auto* set = accept.At<char>(size + 1);
            std::array<bool, 256> members{};
            for (std::size_t index = 0; index < size; ++index) {
                set[index] = static_cast<char>(1 + (index * 73 + count) % 255);
                members[static_cast<unsigned char>(set[index])] = true;
            }
            set[size] = 0;
            const std::string setSaved(set, size + 1);
            const char* expected = nullptr;
            for (std::size_t index = 0; index < count; ++index) {
                if (members[static_cast<unsigned char>(source[index])]) { expected = source + index; break; }
            }
            Require(strpbrk_nid_postfix(source, set) == expected);
            Require(std::equal(saved.begin(), saved.end(), source));
            Require(std::equal(setSaved.begin(), setSaved.end(), set));
        }
    }
}
