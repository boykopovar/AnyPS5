#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Require(bool condition) {
    if (condition) return;
    std::abort();
}

static std::uint64_t Reference(std::uint64_t value, unsigned width, unsigned operation) {
    if (operation < 4) {
        const bool leading = operation < 2;
        const unsigned wanted = operation & 1u;
        for (unsigned position = 1; position <= width; ++position) {
            const unsigned bit = leading ? width - position : position - 1;
            if (((value >> bit) & 1u) == wanted) return position;
        }
        return 0;
    }
    if (operation == 4) {
        unsigned ones = 0;
        for (unsigned bit = 0; bit < width; ++bit) ones += (value >> bit) & 1u;
        return ones == 1;
    }
    if (operation == 5) {
        unsigned bits = 0;
        for (auto remaining = value; remaining; remaining >>= 1) ++bits;
        return bits;
    }
    if (operation == 6) {
        if (!value) return 0;
        std::uint64_t power = 1;
        for (auto remaining = value; remaining > 1; remaining >>= 1) power *= 2;
        return power;
    }
    const auto maximum = width == 64 ? UINT64_MAX : (std::uint64_t{1} << width) - 1;
    std::uint64_t power = 1;
    while (power < value) {
        if (power > maximum / 2) return 0;
        power *= 2;
    }
    return power;
}

template<typename TValue, typename TFunction>
static void Check(TFunction function, unsigned operation, const char* name) {
    constexpr unsigned width = std::numeric_limits<TValue>::digits;
    const auto maximum = std::numeric_limits<TValue>::max();
    const auto check = [&](TValue value) {
        const auto expected = Reference(value, width, operation);
        const auto actual = static_cast<std::uint64_t>(function(value));
        if (actual == expected) return;
        std::fprintf(stderr, "%s input %llx expected %llx got %llx\n", name,
                     static_cast<unsigned long long>(value), static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(actual));
        std::abort();
    };
    check(0);
    check(1);
    check(maximum);
    check(static_cast<TValue>(0xaaaaaaaaaaaaaaaaULL));
    check(static_cast<TValue>(0x5555555555555555ULL));
    if constexpr (width <= 16) {
        for (std::uint32_t value = 0; value <= maximum; ++value) check(static_cast<TValue>(value));
    }
    for (unsigned bit = 0; bit < width; ++bit) {
        const auto one = static_cast<TValue>(TValue{1} << bit);
        check(one);
        check(static_cast<TValue>(one - 1));
        check(static_cast<TValue>(one + 1));
        check(static_cast<TValue>(~one));
        for (unsigned other = 0; other < width; ++other) {
            const auto pair = static_cast<TValue>(one | (TValue{1} << other));
            check(pair);
            check(static_cast<TValue>(~pair));
        }
    }
    if constexpr (width > 16) {
        std::uint64_t state = 0x8c314fb296ea570dULL;
        for (unsigned index = 0; index < 100000; ++index) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            check(static_cast<TValue>(state));
            check(static_cast<TValue>(~state));
        }
    }
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const int saved = *__error_nid_postfix();
    *__error_nid_postfix() = 87;
    if (std::strcmp(argv[1], "first_leading_zero_uc") == 0) Check<std::uint8_t>(stdc_first_leading_zero_uc_nid_postfix, 0, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_zero_us") == 0) Check<std::uint16_t>(stdc_first_leading_zero_us_nid_postfix, 0, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_zero_ui") == 0) Check<std::uint32_t>(stdc_first_leading_zero_ui_nid_postfix, 0, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_zero_ul") == 0) Check<std::uint64_t>(stdc_first_leading_zero_ul_nid_postfix, 0, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_zero_ull") == 0) Check<std::uint64_t>(stdc_first_leading_zero_ull_nid_postfix, 0, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_one_uc") == 0) Check<std::uint8_t>(stdc_first_leading_one_uc_nid_postfix, 1, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_one_us") == 0) Check<std::uint16_t>(stdc_first_leading_one_us_nid_postfix, 1, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_one_ui") == 0) Check<std::uint32_t>(stdc_first_leading_one_ui_nid_postfix, 1, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_one_ul") == 0) Check<std::uint64_t>(stdc_first_leading_one_ul_nid_postfix, 1, argv[1]);
    else if (std::strcmp(argv[1], "first_leading_one_ull") == 0) Check<std::uint64_t>(stdc_first_leading_one_ull_nid_postfix, 1, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_zero_uc") == 0) Check<std::uint8_t>(stdc_first_trailing_zero_uc_nid_postfix, 2, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_zero_us") == 0) Check<std::uint16_t>(stdc_first_trailing_zero_us_nid_postfix, 2, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_zero_ui") == 0) Check<std::uint32_t>(stdc_first_trailing_zero_ui_nid_postfix, 2, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_zero_ul") == 0) Check<std::uint64_t>(stdc_first_trailing_zero_ul_nid_postfix, 2, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_zero_ull") == 0) Check<std::uint64_t>(stdc_first_trailing_zero_ull_nid_postfix, 2, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_one_uc") == 0) Check<std::uint8_t>(stdc_first_trailing_one_uc_nid_postfix, 3, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_one_us") == 0) Check<std::uint16_t>(stdc_first_trailing_one_us_nid_postfix, 3, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_one_ui") == 0) Check<std::uint32_t>(stdc_first_trailing_one_ui_nid_postfix, 3, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_one_ul") == 0) Check<std::uint64_t>(stdc_first_trailing_one_ul_nid_postfix, 3, argv[1]);
    else if (std::strcmp(argv[1], "first_trailing_one_ull") == 0) Check<std::uint64_t>(stdc_first_trailing_one_ull_nid_postfix, 3, argv[1]);
    else if (std::strcmp(argv[1], "has_single_bit_uc") == 0) Check<std::uint8_t>(stdc_has_single_bit_uc_nid_postfix, 4, argv[1]);
    else if (std::strcmp(argv[1], "has_single_bit_us") == 0) Check<std::uint16_t>(stdc_has_single_bit_us_nid_postfix, 4, argv[1]);
    else if (std::strcmp(argv[1], "has_single_bit_ui") == 0) Check<std::uint32_t>(stdc_has_single_bit_ui_nid_postfix, 4, argv[1]);
    else if (std::strcmp(argv[1], "has_single_bit_ul") == 0) Check<std::uint64_t>(stdc_has_single_bit_ul_nid_postfix, 4, argv[1]);
    else if (std::strcmp(argv[1], "has_single_bit_ull") == 0) Check<std::uint64_t>(stdc_has_single_bit_ull_nid_postfix, 4, argv[1]);
    else if (std::strcmp(argv[1], "bit_width_uc") == 0) Check<std::uint8_t>(stdc_bit_width_uc_nid_postfix, 5, argv[1]);
    else if (std::strcmp(argv[1], "bit_width_us") == 0) Check<std::uint16_t>(stdc_bit_width_us_nid_postfix, 5, argv[1]);
    else if (std::strcmp(argv[1], "bit_width_ui") == 0) Check<std::uint32_t>(stdc_bit_width_ui_nid_postfix, 5, argv[1]);
    else if (std::strcmp(argv[1], "bit_width_ul") == 0) Check<std::uint64_t>(stdc_bit_width_ul_nid_postfix, 5, argv[1]);
    else if (std::strcmp(argv[1], "bit_width_ull") == 0) Check<std::uint64_t>(stdc_bit_width_ull_nid_postfix, 5, argv[1]);
    else if (std::strcmp(argv[1], "bit_floor_uc") == 0) Check<std::uint8_t>(stdc_bit_floor_uc_nid_postfix, 6, argv[1]);
    else if (std::strcmp(argv[1], "bit_floor_us") == 0) Check<std::uint16_t>(stdc_bit_floor_us_nid_postfix, 6, argv[1]);
    else if (std::strcmp(argv[1], "bit_floor_ui") == 0) Check<std::uint32_t>(stdc_bit_floor_ui_nid_postfix, 6, argv[1]);
    else if (std::strcmp(argv[1], "bit_floor_ul") == 0) Check<std::uint64_t>(stdc_bit_floor_ul_nid_postfix, 6, argv[1]);
    else if (std::strcmp(argv[1], "bit_floor_ull") == 0) Check<std::uint64_t>(stdc_bit_floor_ull_nid_postfix, 6, argv[1]);
    else if (std::strcmp(argv[1], "bit_ceil_uc") == 0) Check<std::uint8_t>(stdc_bit_ceil_uc_nid_postfix, 7, argv[1]);
    else if (std::strcmp(argv[1], "bit_ceil_us") == 0) Check<std::uint16_t>(stdc_bit_ceil_us_nid_postfix, 7, argv[1]);
    else if (std::strcmp(argv[1], "bit_ceil_ui") == 0) Check<std::uint32_t>(stdc_bit_ceil_ui_nid_postfix, 7, argv[1]);
    else if (std::strcmp(argv[1], "bit_ceil_ul") == 0) Check<std::uint64_t>(stdc_bit_ceil_ul_nid_postfix, 7, argv[1]);
    else if (std::strcmp(argv[1], "bit_ceil_ull") == 0) Check<std::uint64_t>(stdc_bit_ceil_ull_nid_postfix, 7, argv[1]);
    else Require(false);
    Require(*__error_nid_postfix() == 87);
    *__error_nid_postfix() = saved;
}
