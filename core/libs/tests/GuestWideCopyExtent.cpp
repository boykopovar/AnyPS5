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
        std::fputs("GuestWideCopyExtent check failed\n", stderr);
        std::abort();
    }
}

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    GuardedBuffer input, output;
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* source = input.At<char16_t>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char16_t>(0x100 + index * 257);
        source[count] = 0;
        const std::u16string saved(source, count + 1);
        auto* copied = output.At<char16_t>(count + 1);
        std::memset(copied - 8, 0xa5, (count + 9) * sizeof(char16_t));
        Require(wcscpy_nid_postfix(copied, source) == copied);
        Require(std::equal(saved.begin(), saved.end(), copied));
        Prefix(copied);
        for (std::size_t prefix : {0u, 1u, 15u, 33u}) {
            auto* destination = output.At<char16_t>(prefix + count + 1);
            std::memset(destination - 8, 0xa5, (prefix + count + 9) * sizeof(char16_t));
            const std::u16string start(prefix, 0xd800);
            std::copy(start.begin(), start.end(), destination);
            destination[prefix] = 0;
            const auto expected = start + saved;
            Require(wcscat_nid_postfix(destination, source) == destination);
            Require(std::equal(expected.begin(), expected.end(), destination));
            Require(std::equal(saved.begin(), saved.end(), source));
            Prefix(destination);
        }
    }
}
