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
        std::fputs("GuestWcsncatExtent check failed\n", stderr);
        std::abort();
    }
}

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    GuardedBuffer input, output;
    for (std::size_t limit = 0; limit <= 128; ++limit) {
        for (std::size_t length : {std::size_t{0}, std::size_t{1}, limit, limit + 3}) {
            auto* source = input.At<char16_t>(length + 1);
            for (std::size_t i = 0; i < length; ++i) source[i] = static_cast<char16_t>(0x100 + i * 257);
            source[length] = 0;
            const std::u16string saved(source, length + 1);
            const auto copied = (std::min)(length, limit);
            for (std::size_t prefix : {0u, 1u, 15u, 31u}) {
                auto* destination = output.At<char16_t>(prefix + copied + 1);
                std::memset(destination - 8, 0xa5, (prefix + copied + 9) * sizeof(char16_t));
                const std::u16string start(prefix, 0xd800);
                std::copy(start.begin(), start.end(), destination);
                destination[prefix] = 0;
                const auto expected = start + saved.substr(0, copied) + std::u16string(1, 0);
                Require(wcsncat_nid_postfix(destination, source, limit) == destination);
                Require(std::equal(expected.begin(), expected.end(), destination));
                Require(std::equal(saved.begin(), saved.end(), source));
                Prefix(destination);
            }
        }
    }
    for (std::size_t size : {256, 512, 1024}) {
        auto* source = input.At<char16_t>(size + 1);
        for (std::size_t i = 0; i < size; ++i) source[i] = static_cast<char16_t>(0x200 + i);
        source[size] = 0;

        auto* destination = output.At<char16_t>(size + 1);
        std::memset(destination - 8, 0xa5, (size + 9) * sizeof(char16_t));
        destination[0] = 0;

        Require(wcsncat_nid_postfix(destination, source, size) == destination);
        Require(std::char_traits<char16_t>::length(destination) == size);
        Require(std::char_traits<char16_t>::compare(destination, source, size) == 0);
        Prefix(destination);

        Require(wcsncat_nid_postfix(destination, source, 0) == destination);
        Require(std::char_traits<char16_t>::length(destination) == size);
        Prefix(destination);
    }
}
