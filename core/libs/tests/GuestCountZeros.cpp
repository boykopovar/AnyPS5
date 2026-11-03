#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Require(bool condition) {
    if (condition) return;
    std::fprintf(stderr, "count zeros mismatch\n");
    std::abort();
}

template<typename TValue>
static std::uint32_t Reference(TValue value) {
    std::uint32_t result = 0;
    for (unsigned bit = 0; bit < std::numeric_limits<TValue>::digits; ++bit) {
        result += (value & 1) == 0;
        value = static_cast<TValue>(value >> 1);
    }
    return result;
}

template<typename TValue, typename TCounter>
static void Check(TCounter count) {
    constexpr unsigned width = std::numeric_limits<TValue>::digits;
    const auto maximum = std::numeric_limits<TValue>::max();
    Require(count(0) == width);
    Require(count(maximum) == 0);
    if constexpr (width <= 16) {
        for (std::uint32_t value = 0; value <= maximum; ++value) {
            Require(count(static_cast<TValue>(value)) == Reference(static_cast<TValue>(value)));
        }
    }
    for (unsigned bit = 0; bit < width; ++bit) {
        const auto value = static_cast<TValue>(TValue{1} << bit);
        Require(count(value) == width - 1);
        Require(count(static_cast<TValue>(~value)) == 1);
        for (unsigned other = 0; other < width; ++other) {
            const auto pair = static_cast<TValue>(value | (TValue{1} << other));
            Require(count(pair) == width - (bit == other ? 1u : 2u));
        }
    }
    if constexpr (width > 16) {
        std::uint64_t state = 0x6b82439d015af7ceULL;
        for (unsigned index = 0; index < 100000; ++index) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            const auto value = static_cast<TValue>(state);
            Require(count(value) == Reference(value));
            const auto complement = static_cast<TValue>(~value);
            Require(count(complement) == Reference(complement));
        }
    }
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const int saved = *__error_nid_postfix();
    *__error_nid_postfix() = 87;
    if (std::strcmp(argv[1], "uc") == 0) Check<std::uint8_t>(stdc_count_zeros_uc_nid_postfix);
    else if (std::strcmp(argv[1], "us") == 0) Check<std::uint16_t>(stdc_count_zeros_us_nid_postfix);
    else if (std::strcmp(argv[1], "ui") == 0) Check<std::uint32_t>(stdc_count_zeros_ui_nid_postfix);
    else if (std::strcmp(argv[1], "ul") == 0) Check<std::uint64_t>(stdc_count_zeros_ul_nid_postfix);
    else if (std::strcmp(argv[1], "ull") == 0) Check<std::uint64_t>(stdc_count_zeros_ull_nid_postfix);
    else Require(false);
    Require(*__error_nid_postfix() == 87);
    *__error_nid_postfix() = saved;
}
