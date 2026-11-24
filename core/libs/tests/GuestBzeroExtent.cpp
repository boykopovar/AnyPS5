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
        std::fputs("GuestBzeroExtent check failed\n", stderr);
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
    const auto checkExtent = [&output](std::size_t length) {
        auto* destination = output.At(length);
        const auto extent = (std::max)(length, std::size_t{1});
        std::memset(destination - 16, 0xa5, extent + 16);
        bzero_nid_postfix(destination, length);
        for (std::size_t i = 0; i < length; ++i) Require(destination[i] == 0);
        if (!length) Require(destination[0] == 0xa5);
        Prefix(destination);
    };
    for (std::size_t length = 0; length <= 256; ++length) checkExtent(length);
    for (const std::size_t length : {std::size_t{384}, std::size_t{512}, std::size_t{768}, std::size_t{1024}, std::size_t{1536}, std::size_t{2048}, std::size_t{3072}}) {
        checkExtent(length);
    }

    std::array<unsigned char, 256> region{};
    for (std::size_t offset = 0; offset < 16; ++offset) {
        for (const std::size_t length : {std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{15}, std::size_t{31}, std::size_t{64}, std::size_t{128}}) {
            region.fill(0xa5);
            bzero_nid_postfix(region.data() + offset, length);
            for (std::size_t i = 0; i < region.size(); ++i) {
                const bool zeroed = i >= offset && i < offset + length;
                Require(region[i] == (zeroed ? 0 : 0xa5));
            }
        }
    }
    return 0;
}
