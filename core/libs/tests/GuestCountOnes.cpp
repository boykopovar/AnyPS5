#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Require(bool condition) {
    if (condition) return;
    std::fprintf(stderr, "count ones mismatch\n");
    std::abort();
}

static std::uint32_t Reference(std::uint64_t value) {
    std::uint32_t result = 0;
    while (value != 0) {
        result += value & 1;
        value >>= 1;
    }
    return result;
}

template<typename TValue, typename TCounter>
static void Check(TCounter count) {
    constexpr unsigned width = std::numeric_limits<TValue>::digits;
    const auto maximum = std::numeric_limits<TValue>::max();
    Require(count(0) == 0);
    Require(count(maximum) == width);
    if constexpr (width <= 16) {
        for (std::uint32_t value = 0; value <= maximum; ++value) {
            Require(count(static_cast<TValue>(value)) == Reference(value));
        }
    }
    for (unsigned bit = 0; bit < width; ++bit) {
        const auto value = static_cast<TValue>(TValue{1} << bit);
        Require(count(value) == 1);
        Require(count(static_cast<TValue>(~value)) == width - 1);
        for (unsigned other = 0; other < width; ++other) {
            const auto pair = static_cast<TValue>(value | (TValue{1} << other));
            Require(count(pair) == (bit == other ? 1u : 2u));
        }
    }
    if constexpr (width > 16) {
        std::uint64_t state = 0x769c81d32f04a5b7ULL;
        for (unsigned index = 0; index < 100000; ++index) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            const auto value = static_cast<TValue>(state);
            Require(count(value) == Reference(value));
            Require(count(static_cast<TValue>(~value)) == width - Reference(value));
        }
    }
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const int saved = *__error_nid_postfix();
    *__error_nid_postfix() = 87;
    if (std::strcmp(argv[1], "uc") == 0) Check<std::uint8_t>(stdc_count_ones_uc_nid_postfix);
    else if (std::strcmp(argv[1], "us") == 0) Check<std::uint16_t>(stdc_count_ones_us_nid_postfix);
    else if (std::strcmp(argv[1], "ui") == 0) Check<std::uint32_t>(stdc_count_ones_ui_nid_postfix);
    else if (std::strcmp(argv[1], "ul") == 0) Check<std::uint64_t>(stdc_count_ones_ul_nid_postfix);
    else if (std::strcmp(argv[1], "ull") == 0) Check<std::uint64_t>(stdc_count_ones_ull_nid_postfix);
    else Require(false);
    Require(*__error_nid_postfix() == 87);
    *__error_nid_postfix() = saved;
}
