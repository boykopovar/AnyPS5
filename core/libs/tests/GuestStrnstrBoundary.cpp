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
        std::fputs("GuestStrnstrBoundary check failed\n", stderr);
        std::abort();
    }
}

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    GuardedBuffer input, pattern;
    for (std::size_t length = 0; length <= 128; ++length) {
        auto* source = input.At<char>(length);
        const auto extent = (std::max)(length, std::size_t{1});
        for (std::size_t size = 0; size <= 17; ++size) {
            auto* needle = pattern.At<char>(size + 1);
            std::fill(needle, needle + size, 'a');
            if (size) needle[size - 1] = 'b';
            needle[size] = 0;
            const std::string wanted(needle, size);
            for (int mode = 0; mode < 4; ++mode) {
                std::fill(source, source + extent, 'a');
                if (size && size <= length) {
                    const auto position = mode == 0 ? 0 : length - size;
                    std::copy(wanted.begin(), wanted.end(), source + position);
                    if (mode == 2) source[position + size - 1] = 'c';
                }
                if (mode == 3 && length) source[length / 2] = 0;
                const std::string saved(source, extent);
                const auto zero = saved.find(char{0});
                const auto available = (std::min)(length, zero == std::string::npos ? length : zero);
                const auto found = saved.substr(0, available).find(wanted);
                const auto* expected = found == std::string::npos ? nullptr : source + found;
                Require(strnstr_nid_postfix(source, needle, length) == expected);
                Require(std::equal(saved.begin(), saved.end(), source));
                Require(std::equal(wanted.begin(), wanted.end(), needle) && needle[size] == 0);
            }
        }
    }
}
