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
        std::fputs("GuestStrncpyProtected check failed\n", stderr);
        std::abort();
    }
}

int main() {
    GuardedBuffer input, output;
    for (std::size_t count = 0; count <= 192; ++count) {
        for (std::size_t length : {std::size_t{0}, count / 2, count, count + 1}) {
            const std::string original(length, static_cast<char>(0x81));
            auto* source = reinterpret_cast<char*>(input.At(length + 1));
            auto* destination = reinterpret_cast<char*>(output.At(count));
            std::memcpy(source, original.c_str(), length + 1);
            std::memset(destination - 16, 0xa5, (std::max)(count, std::size_t{1}) + 16);
            Require(strncpy_nid_postfix(destination, source, count) == destination);
            for (std::size_t i = 0; i < count; ++i) Require(destination[i] == (i < length ? static_cast<char>(0x81) : '\0'));
            if (count == 0) Require(static_cast<unsigned char>(*destination) == 0xa5);
            for (int i = 1; i <= 16; ++i) Require(static_cast<unsigned char>(destination[-i]) == 0xa5);
            Require(std::memcmp(source, original.c_str(), length + 1) == 0);
        }
    }
}
