#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

struct Parsed {
    std::uint64_t value;
    int end;
    int error;
    bool range;
};

struct Case {
    const char* label;
    const char* text;
    int base;
    Parsed results[4];
};

const Case Cases[] = {
    {"decimal", "12345", 10, {{0x3039ULL, 5, 0, false}, {0x3039ULL, 5, 0, false}, {0x3039ULL, 5, 0, false}, {0x3039ULL, 5, 0, false}}},
    {"spaces and trailing text", "  42xyz", 0, {{0x2aULL, 4, 0, false}, {0x2aULL, 4, 0, false}, {0x2aULL, 4, 0, false}, {0x2aULL, 4, 0, false}}},
    {"negative", "-42", 10, {{0xffffffffffffffd6ULL, 3, 0, false}, {0xffffffffffffffd6ULL, 3, 0, false}, {0xffffffd6ULL, 3, 0, false}, {0xffffffd6ULL, 3, 0, false}}},
    {"plus sign", "+7", 10, {{0x7ULL, 2, 0, false}, {0x7ULL, 2, 0, false}, {0x7ULL, 2, 0, false}, {0x7ULL, 2, 0, false}}},
    {"hexadecimal prefix with base 0", "0x1A", 0, {{0x1aULL, 4, 0, false}, {0x1aULL, 4, 0, false}, {0x1aULL, 4, 0, false}, {0x1aULL, 4, 0, false}}},
    {"hexadecimal prefix with base 16", "0Xff", 16, {{0xffULL, 4, 0, false}, {0xffULL, 4, 0, false}, {0xffULL, 4, 0, false}, {0xffULL, 4, 0, false}}},
    {"hexadecimal prefix alone", "0x", 16, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"hexadecimal prefix without digits", "0xg", 0, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"octal with base 0", "0755", 0, {{0x1edULL, 4, 0, false}, {0x1edULL, 4, 0, false}, {0x1edULL, 4, 0, false}, {0x1edULL, 4, 0, false}}},
    {"octal stops at 8", "08", 0, {{0x0ULL, 1, 0, false}, {0x0ULL, 1, 0, false}, {0x0ULL, 1, 0, false}, {0x0ULL, 1, 0, false}}},
    {"zero with base 0", "0", 0, {{0x0ULL, 1, 0, false}, {0x0ULL, 1, 0, false}, {0x0ULL, 1, 0, false}, {0x0ULL, 1, 0, false}}},
    {"base 36", "Zz", 36, {{0x50fULL, 2, 0, false}, {0x50fULL, 2, 0, false}, {0x50fULL, 2, 0, false}, {0x50fULL, 2, 0, false}}},
    {"base 1", "12", 1, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"base 37", "12", 37, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"negative base", "12", -1, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"no digits", "abc", 10, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"empty text", "", 10, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"sign alone", "-", 10, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"space after the sign", "- 5", 10, {{0xfffffffffffffffbULL, 3, 0, false}, {0x0ULL, 0, 0, false}, {0xfffffffbULL, 3, 0, false}, {0x0ULL, 0, 0, false}}},
    {"two minus signs", "--5", 10, {{0x8000000000000000ULL, 3, 1, true}, {0x0ULL, 0, 0, false}, {0x80000000ULL, 3, 1, true}, {0x0ULL, 0, 0, false}}},
    {"plus then minus", "+-5", 10, {{0x7fffffffffffffffULL, 3, 1, true}, {0x0ULL, 0, 0, false}, {0x7fffffffULL, 3, 1, true}, {0x0ULL, 0, 0, false}}},
    {"every C space", "\t\n\v\f\r 9", 10, {{0x9ULL, 7, 0, false}, {0x9ULL, 7, 0, false}, {0x9ULL, 7, 0, false}, {0x9ULL, 7, 0, false}}},
    {"no-break space", "\xa0" "5", 10, {{0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}, {0x0ULL, 0, 0, false}}},
    {"leading zeros", "000000000000000000000000000001", 10, {{0x1ULL, 30, 0, false}, {0x1ULL, 30, 0, false}, {0x1ULL, 30, 0, false}, {0x1ULL, 30, 0, false}}},
    {"largest 64-bit value", "18446744073709551615", 10, {{0x7fffffffffffffffULL, 20, 1, true}, {0xffffffffffffffffULL, 20, 0, false}, {0x7fffffffULL, 20, 1, true}, {0xffffffffULL, 20, 1, true}}},
    {"one past 64 bits", "18446744073709551616", 10, {{0x7fffffffffffffffULL, 20, 1, true}, {0xffffffffffffffffULL, 20, 1, true}, {0x7fffffffULL, 20, 1, true}, {0xffffffffULL, 20, 1, true}}},
    {"largest signed 64-bit value", "9223372036854775807", 10, {{0x7fffffffffffffffULL, 19, 0, false}, {0x7fffffffffffffffULL, 19, 0, false}, {0x7fffffffULL, 19, 1, true}, {0xffffffffULL, 19, 1, true}}},
    {"one past signed 64 bits", "9223372036854775808", 10, {{0x7fffffffffffffffULL, 19, 1, true}, {0x8000000000000000ULL, 19, 0, false}, {0x7fffffffULL, 19, 1, true}, {0xffffffffULL, 19, 1, true}}},
    {"lowest signed 64-bit value", "-9223372036854775808", 10, {{0x8000000000000000ULL, 20, 0, false}, {0x8000000000000000ULL, 20, 0, false}, {0x80000000ULL, 20, 1, true}, {0xffffffffULL, 20, 1, true}}},
    {"one below signed 64 bits", "-9223372036854775809", 10, {{0x8000000000000000ULL, 20, 1, true}, {0x7fffffffffffffffULL, 20, 0, false}, {0x80000000ULL, 20, 1, true}, {0xffffffffULL, 20, 1, true}}},
    {"largest 32-bit value", "4294967295", 10, {{0xffffffffULL, 10, 0, false}, {0xffffffffULL, 10, 0, false}, {0x7fffffffULL, 10, 1, true}, {0xffffffffULL, 10, 0, false}}},
    {"one past 32 bits", "4294967296", 10, {{0x100000000ULL, 10, 0, false}, {0x100000000ULL, 10, 0, false}, {0x7fffffffULL, 10, 1, true}, {0xffffffffULL, 10, 1, true}}},
    {"lowest signed 32-bit value", "-2147483648", 10, {{0xffffffff80000000ULL, 11, 0, false}, {0xffffffff80000000ULL, 11, 0, false}, {0x80000000ULL, 11, 0, false}, {0x80000000ULL, 11, 0, false}}},
    {"one below signed 32 bits", "-2147483649", 10, {{0xffffffff7fffffffULL, 11, 0, false}, {0xffffffff7fffffffULL, 11, 0, false}, {0x80000000ULL, 11, 1, true}, {0x7fffffffULL, 11, 0, false}}},
    {"one past signed 32 bits", "2147483648", 10, {{0x80000000ULL, 10, 0, false}, {0x80000000ULL, 10, 0, false}, {0x7fffffffULL, 10, 1, true}, {0x80000000ULL, 10, 0, false}}},
    {"negated 32-bit maximum", "-4294967295", 10, {{0xffffffff00000001ULL, 11, 0, false}, {0xffffffff00000001ULL, 11, 0, false}, {0x80000000ULL, 11, 1, true}, {0x1ULL, 11, 0, false}}},
    {"65 binary digits", "10000000000000000000000000000000000000000000000000000000000000000", 2, {{0x7fffffffffffffffULL, 65, 1, true}, {0xffffffffffffffffULL, 65, 1, true}, {0x7fffffffULL, 65, 1, true}, {0xffffffffULL, 65, 1, true}}},
    {"64 binary ones", "1111111111111111111111111111111111111111111111111111111111111111", 2, {{0x7fffffffffffffffULL, 64, 1, true}, {0xffffffffffffffffULL, 64, 0, false}, {0x7fffffffULL, 64, 1, true}, {0xffffffffULL, 64, 1, true}}},
    {"33 binary digits", "100000000000000000000000000000000", 2, {{0x100000000ULL, 33, 0, false}, {0x100000000ULL, 33, 0, false}, {0x7fffffffULL, 33, 1, true}, {0xffffffffULL, 33, 1, true}}},
    {"base 36 at the 64-bit limit", "3w5e11264sgsf", 36, {{0x7fffffffffffffffULL, 13, 1, true}, {0xffffffffffffffffULL, 13, 0, false}, {0x7fffffffULL, 13, 1, true}, {0xffffffffULL, 13, 1, true}}},
    {"base 36 past the 64-bit limit", "3w5e11264sgsg", 36, {{0x7fffffffffffffffULL, 13, 1, true}, {0xffffffffffffffffULL, 13, 1, true}, {0x7fffffffULL, 13, 1, true}, {0xffffffffULL, 13, 1, true}}},
    {"64-bit hexadecimal", "0xffffffffffffffff", 0, {{0x7fffffffffffffffULL, 18, 1, true}, {0xffffffffffffffffULL, 18, 0, false}, {0x7fffffffULL, 18, 1, true}, {0xffffffffULL, 18, 1, true}}},
    {"33-bit hexadecimal", "0x100000000", 0, {{0x100000000ULL, 11, 0, false}, {0x100000000ULL, 11, 0, false}, {0x7fffffffULL, 11, 1, true}, {0xffffffffULL, 11, 1, true}}},
};

using Parse = std::uint64_t (*)(const char*, char**, int, int*);

std::uint64_t Stollx(const char* text, char** end, int base, int* error) {
    return static_cast<std::uint64_t>(_ZNSt8__sce_v27_StollxEPKcPPciPi_nid_postfix(text, end, base, error));
}

std::uint64_t Stoullx(const char* text, char** end, int base, int* error) {
    return _ZNSt8__sce_v28_StoullxEPKcPPciPi_nid_postfix(text, end, base, error);
}

std::uint64_t Stolx(const char* text, char** end, int base, int* error) {
    return static_cast<std::uint64_t>(_ZNSt8__sce_v26_StolxEPKcPPciPi_nid_postfix(text, end, base, error));
}

std::uint64_t Stoulx(const char* text, char** end, int base, int* error) {
    return _ZNSt8__sce_v27_StoulxEPKcPPciPi_nid_postfix(text, end, base, error);
}

const Parse Parsers[] = {Stollx, Stoullx, Stolx, Stoulx};
const char* const Names[] = {"_Stollx", "_Stoullx", "_Stolx", "_Stoulx"};

}

int main() {
    for (const auto& entry : Cases) {
        for (int f = 0; f < 4; ++f) {
            const Parsed& expected = entry.results[f];
            char* end = nullptr;
            int error = 555;
            *__error_nid_postfix() = 777;
            const std::uint64_t value = Parsers[f](entry.text, &end, entry.base, &error);
            const int guestErrno = *__error_nid_postfix();
            const bool endMatches = end == entry.text + expected.end;
            if (value == expected.value && endMatches && error == expected.error && guestErrno == (expected.range ? 34 : 777)) {
                *__error_nid_postfix() = 777;
                if (Parsers[f](entry.text, nullptr, entry.base, nullptr) == value && *__error_nid_postfix() == guestErrno) continue;
            }
            std::fprintf(stderr, "sce v2 %s: %s: value %llx error %d errno %d\n", Names[f], entry.label, static_cast<unsigned long long>(value), error, guestErrno);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
