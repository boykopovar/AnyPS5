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
        std::fputs("GuestStrcatExtent check failed\n", stderr);
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
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* source = input.At<char>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char>(1 + (index * 61 + count) % 255);
        source[count] = 0;
        const std::string saved(source, count + 1);
        for (std::size_t prefix : {0u, 1u, 15u, 32u}) {
            auto* destination = output.At<char>(prefix + count + 1);
            std::memset(destination - 16, 0xa5, prefix + count + 17);
            const std::string start(prefix, static_cast<char>(0xe9));
            std::copy(start.begin(), start.end(), destination);
            destination[prefix] = 0;
            const std::string expected = start + saved;
            Require(strcat_nid_postfix(destination, source) == destination);
            Require(std::equal(expected.begin(), expected.end(), destination));
            Require(std::equal(saved.begin(), saved.end(), source));
            Prefix(reinterpret_cast<unsigned char*>(destination));
        }
    }
}
