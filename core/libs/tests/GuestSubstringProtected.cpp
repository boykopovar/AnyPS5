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
        std::fputs("GuestSubstringProtected check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

int main() {
    GuardedBuffer hayBuffer, needleBuffer;
    for (std::size_t length = 0; length <= 160; ++length) {
        std::string repeating(length, 'a');
        if (!repeating.empty()) repeating.back() = 'b';
        const std::string high = std::string(length, static_cast<char>(0x80)) + static_cast<char>(0xff);
        for (const auto& haystack : {repeating, high}) {
            for (const auto& needle : {std::string{}, std::string("aab"), std::string("ba"), repeating,
                                      std::string("aaaaac"), std::string(3, static_cast<char>(0x80)) + static_cast<char>(0xff),
                                      haystack + "extra"}) {
                auto* hay = reinterpret_cast<char*>(hayBuffer.At(haystack.size() + 1));
                auto* query = reinterpret_cast<char*>(needleBuffer.At(needle.size() + 1));
                std::memcpy(hay, haystack.c_str(), haystack.size() + 1);
                std::memcpy(query, needle.c_str(), needle.size() + 1);
                const auto position = haystack.find(needle);
                Require(strstr_nid_postfix(hay, query) == (position == std::string::npos ? nullptr : hay + position));
                Require(std::memcmp(hay, haystack.c_str(), haystack.size() + 1) == 0);
                Require(std::memcmp(query, needle.c_str(), needle.size() + 1) == 0);
            }
        }
    }
    for (std::size_t length : {256, 512, 1024}) {
        std::string haystack(length, 'x');
        haystack[0] = 'S';
        haystack[length / 2] = 'M';
        haystack[length - 1] = 'E';

        auto* hay = reinterpret_cast<char*>(hayBuffer.At(haystack.size() + 1));
        std::memcpy(hay, haystack.c_str(), haystack.size() + 1);

        for (const auto& needle : {std::string("S"), std::string("M"), std::string("E"), std::string("x"),
                                   std::string("xx"), std::string("xxx"), std::string("notfound"), haystack}) {
            auto* query = reinterpret_cast<char*>(needleBuffer.At(needle.size() + 1));
            std::memcpy(query, needle.c_str(), needle.size() + 1);

            const auto position = haystack.find(needle);
            Require(strstr_nid_postfix(hay, query) == (position == std::string::npos ? nullptr : hay + position));
            Require(std::memcmp(hay, haystack.c_str(), haystack.size() + 1) == 0);
            Require(std::memcmp(query, needle.c_str(), needle.size() + 1) == 0);
        }
    }
}
