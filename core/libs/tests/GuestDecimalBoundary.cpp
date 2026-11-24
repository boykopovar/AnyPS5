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
        std::fputs("GuestDecimalBoundary check failed\n", stderr);
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
    GuardedBuffer input;
    for (long long value : {-2147483648LL, -9999LL, -1LL, 0LL, 1LL, 9999LL, 2147483647LL}) {
        const auto number = std::to_string(value);
        for (std::size_t spaces : {0u, 1u, 15u, 64u, 128u}) {
            for (const std::string suffix : {std::string{}, std::string{"tail"}}) {
                const auto text = std::string(spaces, ' ') + "\t" + (value >= 0 ? "+" : "") + number + suffix;
                auto* source = input.At<char>(text.size() + 1);
                std::copy(text.begin(), text.end(), source);
                source[text.size()] = 0;
                const std::string saved(source, text.size() + 1);
                Require(atoi_nid_postfix(source) == value);
                Require(atol_nid_postfix(source) == value);
                Require(atoll_nid_postfix(source) == value);
                Require(std::equal(saved.begin(), saved.end(), source));
            }
        }
    }
    for (const char* zeroText : {"-0", " \t-0tail"}) {
        const std::size_t len = std::strlen(zeroText) + 1;
        auto* source = input.At<char>(len);
        std::memcpy(source, zeroText, len);
        Require(atoi_nid_postfix(source) == 0);
        Require(atol_nid_postfix(source) == 0);
        Require(atoll_nid_postfix(source) == 0);
    }
}
