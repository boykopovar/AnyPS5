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
        std::fputs("GuestStrncatProtected check failed\n", stderr);
        std::abort();
    }
}

int main() {
    GuardedBuffer input, output;
    for (std::size_t count = 0; count <= 128; ++count) {
        for (std::size_t length : {std::size_t{0}, count / 2, count, count + 1}) {
            for (std::size_t prefix : std::array<std::size_t, 4>{0, 1, 15, 31}) {
                const std::string original(length, static_cast<char>(0xff));
                const std::string expected = std::string(prefix, 'p') + original.substr(0, count);
                auto* source = reinterpret_cast<char*>(input.At(length + 1));
                auto* destination = reinterpret_cast<char*>(output.At(expected.size() + 1));
                std::memcpy(source, original.c_str(), length + 1);
                std::memset(destination - 16, 0xa5, expected.size() + 17);
                std::memset(destination, 'p', prefix);
                destination[prefix] = 0;
                Require(strncat_nid_postfix(destination, source, count) == destination);
                Require(std::memcmp(destination, expected.c_str(), expected.size() + 1) == 0);
                Require(std::memcmp(source, original.c_str(), length + 1) == 0);
                for (int i = 1; i <= 16; ++i) Require(static_cast<unsigned char>(destination[-i]) == 0xa5);
            }
        }
    }
    for (std::size_t size : {256, 512, 1024}) {
        const std::string original(size, 'z');
        auto* source = reinterpret_cast<char*>(input.At(size + 1));
        std::memcpy(source, original.c_str(), size + 1);

        auto* destination = reinterpret_cast<char*>(output.At(size + 1));
        std::memset(destination - 16, 0xa5, size + 17);
        destination[0] = 0;

        Require(strncat_nid_postfix(destination, source, size) == destination);
        Require(std::memcmp(destination, original.c_str(), size + 1) == 0);
        for (int i = 1; i <= 16; ++i) Require(static_cast<unsigned char>(destination[-i]) == 0xa5);

        Require(strncat_nid_postfix(destination, source, 0) == destination);
        Require(std::memcmp(destination, original.c_str(), size + 1) == 0);
        for (int i = 1; i <= 16; ++i) Require(static_cast<unsigned char>(destination[-i]) == 0xa5);
    }
}
