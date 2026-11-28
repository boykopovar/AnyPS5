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
        std::fputs("GuestStrxfrmBoundary check failed\n", stderr);
        std::abort();
    }
}

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    GuardedBuffer input, output;
    for (std::size_t length = 0; length <= 128; ++length) {
        auto* source = input.At<char>(length + 1);
        for (std::size_t i = 0; i < length; ++i) source[i] = static_cast<char>(1 + (i * 73 + length) % 255);
        source[length] = 0;
        const std::vector<char> saved(source, source + length + 1);
        for (std::size_t capacity : {std::size_t{0}, std::size_t{1}, length, length + 1, length + 7}) {
            auto* destination = output.At<char>(capacity);
            const auto extent = (std::max)(capacity, std::size_t{1});
            std::memset(destination - 8, 0xa5, (extent + 8) * sizeof(char));
            Require(strxfrm_nid_postfix(destination, source, capacity) == length);
            if (length < capacity) Require(std::equal(saved.begin(), saved.end(), destination));
            if (capacity == 0) Require(destination[0] == static_cast<char>(static_cast<char>(0xa5)));
            Prefix(reinterpret_cast<unsigned char*>(destination));
            Require(std::equal(saved.begin(), saved.end(), source));
        }
    }
}
