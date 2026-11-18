#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Require(bool condition) {
    if (condition) return;
    std::fprintf(stderr, "zero run mismatch\n");
    std::abort();
}

template<typename TValue>
static std::uint32_t Reference(TValue value, bool leading) {
    constexpr unsigned width = std::numeric_limits<TValue>::digits;
    for (unsigned count = 0; count < width; ++count) {
        const unsigned bit = leading ? width - count - 1 : count;
        if ((value & (TValue{1} << bit)) != 0) return count;
    }
    return width;
}

template<typename TValue, typename TCounter>
static void Check(TCounter count, bool leading) {
    constexpr unsigned width = std::numeric_limits<TValue>::digits;
    const auto maximum = std::numeric_limits<TValue>::max();
    Require(count(0) == width);
    Require(count(maximum) == 0);
    if constexpr (width <= 16) {
        for (std::uint32_t value = 0; value <= maximum; ++value) {
            Require(count(static_cast<TValue>(value)) == Reference(static_cast<TValue>(value), leading));
        }
    }
    for (unsigned bit = 0; bit < width; ++bit) {
        const auto value = static_cast<TValue>(TValue{1} << bit);
        Require(count(value) == (leading ? width - bit - 1 : bit));
        Require(count(static_cast<TValue>(~value)) == Reference(static_cast<TValue>(~value), leading));
        for (unsigned other = 0; other < width; ++other) {
            const auto pair = static_cast<TValue>(value | (TValue{1} << other));
            Require(count(pair) == Reference(pair, leading));
        }
    }
    if constexpr (width > 16) {
        std::uint64_t state = 0x981f7a2c40e6d35bULL;
        for (unsigned index = 0; index < 100000; ++index) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            const auto value = static_cast<TValue>(state);
            Require(count(value) == Reference(value, leading));
            const auto complement = static_cast<TValue>(~value);
            Require(count(complement) == Reference(complement, leading));
        }
    }
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const int saved = *__error_nid_postfix();
    *__error_nid_postfix() = 87;
    if (std::strcmp(argv[1], "leading_uc") == 0) Check<std::uint8_t>(stdc_leading_zeros_uc_nid_postfix, true);
    else if (std::strcmp(argv[1], "leading_us") == 0) Check<std::uint16_t>(stdc_leading_zeros_us_nid_postfix, true);
    else if (std::strcmp(argv[1], "leading_ui") == 0) Check<std::uint32_t>(stdc_leading_zeros_ui_nid_postfix, true);
    else if (std::strcmp(argv[1], "leading_ul") == 0) Check<std::uint64_t>(stdc_leading_zeros_ul_nid_postfix, true);
    else if (std::strcmp(argv[1], "leading_ull") == 0) Check<std::uint64_t>(stdc_leading_zeros_ull_nid_postfix, true);
    else if (std::strcmp(argv[1], "trailing_uc") == 0) Check<std::uint8_t>(stdc_trailing_zeros_uc_nid_postfix, false);
    else if (std::strcmp(argv[1], "trailing_us") == 0) Check<std::uint16_t>(stdc_trailing_zeros_us_nid_postfix, false);
    else if (std::strcmp(argv[1], "trailing_ui") == 0) Check<std::uint32_t>(stdc_trailing_zeros_ui_nid_postfix, false);
    else if (std::strcmp(argv[1], "trailing_ul") == 0) Check<std::uint64_t>(stdc_trailing_zeros_ul_nid_postfix, false);
    else if (std::strcmp(argv[1], "trailing_ull") == 0) Check<std::uint64_t>(stdc_trailing_zeros_ull_nid_postfix, false);
    else Require(false);
    Require(*__error_nid_postfix() == 87);
    *__error_nid_postfix() = saved;
}
