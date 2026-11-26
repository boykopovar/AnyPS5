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
        std::fputs("GuestStrcpyExtent check failed\n", stderr);
        std::abort();
    }
}

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    GuardedBuffer input, output;
    for (std::size_t count = 0; count <= 192; ++count) {
        auto* source = input.At<char>(count + 1);
        auto* destination = output.At<char>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char>(1 + (index * 97 + count) % 255);
        source[count] = 0;
        const std::string saved(source, count + 1);
        std::memset(destination - 16, 0xa5, count + 17);
        Require(strcpy_nid_postfix(destination, source) == destination);
        Require(std::equal(saved.begin(), saved.end(), destination));
        Require(std::equal(saved.begin(), saved.end(), source));
        Prefix(reinterpret_cast<unsigned char*>(destination));
    }
    for (std::size_t count : {512, 1024, 2048}) {
        auto* source = input.At<char>(count + 1);
        auto* destination = output.At<char>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char>(1 + (index * 13) % 250);
        source[count] = 0;
        std::memset(destination - 16, 0xa5, count + 17);
        Require(strcpy_nid_postfix(destination, source) == destination);
        Require(std::memcmp(destination, source, count + 1) == 0);
        Prefix(reinterpret_cast<unsigned char*>(destination));
    }
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* source = input.At<char16_t>(count + 1);
        auto* destination = output.At<char16_t>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char16_t>(1 + (index * 97 + count) % 65534);
        source[count] = 0;
        std::u16string saved(source, count + 1);
        std::memset(destination - 16, 0xa5, (count + 17) * sizeof(char16_t));
        Require(wcscpy_nid_postfix(destination, source) == destination);
        Require(std::equal(saved.begin(), saved.end(), destination));
        Require(std::equal(saved.begin(), saved.end(), source));
        Prefix(destination);
    }
}
