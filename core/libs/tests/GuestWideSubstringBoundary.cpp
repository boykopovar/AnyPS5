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
        std::fputs("GuestWideSubstringBoundary check failed\n", stderr);
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
    GuardedBuffer input, pattern;
    for (std::size_t length = 0; length <= 128; ++length) {
        auto* source = input.At<char16_t>(length + 1);
        for (std::size_t size = 0; size <= 20; ++size) {
            auto* needle = pattern.At<char16_t>(size + 1);
            std::fill(needle, needle + size, 0x100);
            if (size) needle[size - 1] = 0xd800;
            needle[size] = 0;
            const std::u16string wanted(needle, size);
            for (int mode = 0; mode < 4; ++mode) {
                std::fill(source, source + length, 0x100);
                if (size && size <= length) {
                    const auto position = mode == 0 ? 0 : mode == 1 ? (length - size) / 2 : length - size;
                    std::copy(wanted.begin(), wanted.end(), source + position);
                    if (mode == 3) source[position + size - 1] = 0xffff;
                }
                source[length] = 0;
                const std::u16string saved(source, length);
                const auto found = saved.find(wanted);
                const auto* expected = found == std::u16string::npos ? nullptr : source + found;
                Require(wcsstr_nid_postfix(source, needle) == expected);
                Require(std::equal(saved.begin(), saved.end(), source) && source[length] == 0);
                Require(std::equal(wanted.begin(), wanted.end(), needle) && needle[size] == 0);
            }
        }
    }
}
