#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

extern "C" {
extern const unsigned char _DefaultRuneLocale_nid_postfix[4224];
extern const unsigned char* _CurrentRuneLocale_nid_postfix;
}

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
    const auto evenBits = static_cast<TValue>(0x5555555555555555ULL);
    const auto oddBits = static_cast<TValue>(0xAAAAAAAAAAAAAAAAULL);
    Require(count(evenBits) == width / 2);
    Require(count(oddBits) == width / 2);
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

static void CheckRuneLocaleRanges() {
    struct TestRangeEntry {
        std::int32_t min;
        std::int32_t max;
        std::int32_t map;
        const std::uint64_t* types;
    };
    struct TestRange {
        std::int32_t count;
        const TestRangeEntry* entries;
    };
    struct LocaleGuard {
        const unsigned char* saved;
        explicit LocaleGuard(const unsigned char* next) : saved(_CurrentRuneLocale_nid_postfix) {
            _CurrentRuneLocale_nid_postfix = next;
        }
        ~LocaleGuard() {
            _CurrentRuneLocale_nid_postfix = saved;
        }
    };
    static_assert(sizeof(TestRangeEntry) == 24);
    static_assert(sizeof(TestRange) == 16);

    alignas(8) unsigned char localeBuffer[4224];
    std::memcpy(localeBuffer, _DefaultRuneLocale_nid_postfix, sizeof(localeBuffer));

    const std::uint64_t lowerPerRuneTypes[2] = {0x41900, 0x141900};
    const TestRangeEntry typeEntries[2] = {
        {0x0391, 0x0393, 0x48900, nullptr},
        {0x03b1, 0x03b2, 0, lowerPerRuneTypes},
    };
    const TestRangeEntry lowerEntries[1] = {
        {0x0391, 0x0393, 0x03b1, nullptr},
    };
    const TestRangeEntry upperEntries[1] = {
        {0x03b1, 0x03b3, 0x0391, nullptr},
    };

    const TestRange typeRange{2, typeEntries};
    const TestRange lowerRange{1, lowerEntries};
    const TestRange upperRange{1, upperEntries};
    std::memcpy(localeBuffer + 4160, &typeRange, sizeof(typeRange));
    std::memcpy(localeBuffer + 4176, &lowerRange, sizeof(lowerRange));
    std::memcpy(localeBuffer + 4192, &upperRange, sizeof(upperRange));

    const std::uint64_t alphaMask = wctype_nid_postfix("alpha");
    const std::uint64_t upperMask = wctype_nid_postfix("upper");
    const std::uint64_t lowerMask = wctype_nid_postfix("lower");
    const std::uint64_t specialMask = wctype_nid_postfix("special");
    Require(alphaMask == 0x100 && upperMask == 0x8000 && lowerMask == 0x1000 && specialMask == 0x100000);

    LocaleGuard guard(localeBuffer);
    for (std::uint32_t c = 0x0391; c <= 0x0393; ++c) {
        Require(iswalpha_nid_postfix(c) != 0);
        Require(iswupper_nid_postfix(c) != 0);
        Require(iswlower_nid_postfix(c) == 0);
        Require(iswctype_nid_postfix(c, alphaMask) != 0);
        Require(iswctype_nid_postfix(c, upperMask) != 0);
        Require(iswctype_nid_postfix(c, lowerMask) == 0);
        Require(_Iswctype_nid_postfix(c, 2) != 0);
        Require(_Iswctype_nid_postfix(c, 10) != 0);
        Require(_Iswctype_nid_postfix(c, 6) == 0);
        Require(towlower_nid_postfix(c) == c + (0x03b1u - 0x0391u));
        Require(towupper_nid_postfix(c) == c);
    }
    for (std::uint32_t c = 0x03b1; c <= 0x03b2; ++c) {
        Require(iswalpha_nid_postfix(c) != 0);
        Require(iswlower_nid_postfix(c) != 0);
        Require(iswupper_nid_postfix(c) == 0);
        Require(iswctype_nid_postfix(c, alphaMask) != 0);
        Require(iswctype_nid_postfix(c, lowerMask) != 0);
        Require(iswctype_nid_postfix(c, upperMask) == 0);
        Require((iswctype_nid_postfix(c, specialMask) != 0) == (c == 0x03b2));
        Require(_Iswctype_nid_postfix(c, 2) != 0);
        Require(_Iswctype_nid_postfix(c, 6) != 0);
        Require(_Iswctype_nid_postfix(c, 10) == 0);
        Require(towupper_nid_postfix(c) == c - (0x03b1u - 0x0391u));
        Require(towlower_nid_postfix(c) == c);
    }
    Require(iswalpha_nid_postfix(0x03b3) == 0);
    Require(towupper_nid_postfix(0x03b3) == 0x0393);

    for (const std::uint32_t miss : {0x0394u, 0x80000000u, 0xffffffffu}) {
        Require(iswalpha_nid_postfix(miss) == 0);
        Require(iswupper_nid_postfix(miss) == 0);
        Require(iswlower_nid_postfix(miss) == 0);
        Require(iswctype_nid_postfix(miss, alphaMask) == 0);
        Require(_Iswctype_nid_postfix(miss, 2) == 0);
        Require(towlower_nid_postfix(miss) == miss);
        Require(towupper_nid_postfix(miss) == miss);
    }

    _CurrentRuneLocale_nid_postfix = nullptr;
    Require(iswalpha_nid_postfix('A') != 0);
    Require(iswupper_nid_postfix('A') != 0);
    Require(iswlower_nid_postfix('a') != 0);
    Require(iswctype_nid_postfix('A', upperMask) != 0);
    Require(_Iswctype_nid_postfix('a', 6) != 0);
    Require(iswalpha_nid_postfix(0x0391) == 0);
    Require(towlower_nid_postfix('A') == 'a');
    Require(towupper_nid_postfix('a') == 'A');
    Require(towlower_nid_postfix(0x0391) == 0x0391);
    Require(towupper_nid_postfix(0x03b1) == 0x03b1);
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
    CheckRuneLocaleRanges();
}
