#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cstdarg>

#if defined(__clang__) && !defined(_WIN32)
#define __builtin_sysv_va_list va_list
#define __builtin_sysv_va_start va_start
#define __builtin_sysv_va_end va_end
#endif

static void Require(bool condition) {
    if (!condition) throw std::runtime_error("Formatting check failed");
}

static int APS5_VABI FormatList(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const VaList original = list;
    const int result = vsnprintf_nid_postfix(buffer, size, format, &list);
    Require(std::memcmp(&list, &original, sizeof(list)) == 0);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI FormatListS(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = vsnprintf_s_nid_postfix(buffer, size, format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI ScanList(const char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = vsscanf_nid_postfix(buffer, format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI ScanListS(const char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = vsscanf_s_nid_postfix(buffer, format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI PrintList(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = vprintf_nid_postfix(format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static void CheckSizeLimits() {
    using Formatter = int (APS5_VABI*)(char*, size_t, const char*, ...);
    for (const Formatter format : {&snprintf_nid_postfix, &FormatList}) {
        char output[8];
        for (const std::size_t oversized : {static_cast<std::size_t>(INT_MAX) + 2, static_cast<std::size_t>(SIZE_MAX)}) {
            for (const bool wide : {false, true}) {
                std::memset(output, 'Z', sizeof(output));
                int consumed = -1;
                *__error_nid_postfix() = 0;
                const int result = wide ? format(output, oversized, "%ls%n", u"x", &consumed)
                                        : format(output, oversized, "x%n", &consumed);
                Require(result == 1 && *__error_nid_postfix() == 0 && std::strcmp(output, "x") == 0 && consumed == 1);
                Require(std::all_of(output + 2, output + sizeof(output), [](char value) { return value == 'Z'; }));
            }
        }
        for (const std::size_t capacity : {std::size_t{0}, std::size_t{1}, static_cast<std::size_t>(INT_MAX), static_cast<std::size_t>(INT_MAX) + 1}) {
            std::memset(output, 'Z', sizeof(output));
            *__error_nid_postfix() = 35;
            Require(format(output, capacity, "x") == 1 && *__error_nid_postfix() == 35);
            Require(output[0] == (capacity == 0 ? 'Z' : capacity == 1 ? '\0' : 'x'));
            const std::size_t written = capacity < 2 ? capacity : 2;
            if (capacity > 1) Require(output[1] == 0);
            Require(std::all_of(output + written, output + sizeof(output), [](char value) { return value == 'Z'; }));
        }
        Require(format(nullptr, 0, "x") == 1);
    }
}

static bool CheckWidePrecision() {
    const char16_t input[] = u"A\u00e9\u20ac\U0001f600Z";
    const char* expected[] = {"", "A", "A", "A\xc3\xa9", "A\xc3\xa9", "A\xc3\xa9", "A\xc3\xa9\xe2\x82\xac",
        "A\xc3\xa9\xe2\x82\xac", "A\xc3\xa9\xe2\x82\xac", "A\xc3\xa9\xe2\x82\xac\xed\xa0\xbd",
        "A\xc3\xa9\xe2\x82\xac\xed\xa0\xbd", "A\xc3\xa9\xe2\x82\xac\xed\xa0\xbd"};
    bool correct = true;
    for (int precision = 0; precision < 12; ++precision) {
        char output[32];
        std::memset(output, '!', sizeof(output));
        const int count = snprintf_nid_postfix(output, sizeof(output), "%.*ls", precision, input);
        const auto expectedSize = std::strlen(expected[precision]);
        const bool matches = count == expectedSize && std::strcmp(output, expected[precision]) == 0 && output[expectedSize + 1] == '!';
        if (!matches) std::fprintf(stderr, "wide precision %d: expected %zu complete UTF-8 bytes, received %d\n", precision, expectedSize, count);
        correct &= matches;
    }
    if (!correct) return false;
    char output[32];
    Require(snprintf_nid_postfix(output, sizeof(output), "[%5.1ls]", u"\u00e9") == 7 && std::strcmp(output, "[     ]") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "[%-5.2ls]", u"\u00e9") == 7 && std::strcmp(output, "[\xc3\xa9   ]") == 0);
    Require(FormatList(output, sizeof(output), "%.3ls", u"\U0001f600") == 3 && std::strcmp(output, "\xed\xa0\xbd") == 0);
    Require(FormatList(output, sizeof(output), "%ls", u"\U0001f600") == 6 && std::strcmp(output, "\xed\xa0\xbd\xed\xb8\x80") == 0);
    Require(FormatList(output, sizeof(output), "%ls[% u][% 4u][%+ lu][%- 3hhu][% x][%+o]", u"", 5u, 5u, 5ULL, 5u, 255u, 8u) == 25
        && std::strcmp(output, "[5][   5][5][5  ][ff][10]") == 0);
    Require(FormatList(output, sizeof(output), "%.*ls:%d", -1, u"\u00e9", 7) == 4 && std::strcmp(output, "\xc3\xa9:7") == 0);
    const char16_t bounded[] = {u'A', u'B'};
    Require(snprintf_nid_postfix(output, sizeof(output), "%.2ls", bounded) == 2 && std::strcmp(output, "AB") == 0);
    Require(snprintf_nid_postfix(nullptr, 0, "%.1ls", u"\u00e9") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "%.1s", "\xc3\xa9") == 1 && static_cast<unsigned char>(output[0]) == 0xc3 && output[1] == 0);
    Require(FormatList(output, sizeof(output), "%S:%lc:%C:%d", u"\u00e9", 0x20ac, 0x41, 7) == 10 && std::strcmp(output, "\xc3\xa9:\xe2\x82\xac:A:7") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "%.*S", 3, u"\u00e9\u20ac") == 2 && std::strcmp(output, "\xc3\xa9") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "[%ls]", static_cast<const char16_t*>(nullptr)) == 8 && std::strcmp(output, "[(null)]") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "a%lcb%C", 0, 0) == 4 && std::memcmp(output, "a\0b\0", 5) == 0);
    Require(FormatList(output, sizeof(output), "%%ls:%2$d:%1$d", 3, 7) == 7 && std::strcmp(output, "%ls:7:3") == 0);
    Require(FormatList(output, sizeof(output), "%ls[%f][%+f][% F][%5.1e]", u"", -NAN, -NAN, NAN, -NAN) == 25
        && std::strcmp(output, "[-nan][-nan][ NAN][ -nan]") == 0);
    Require(FormatList(output, sizeof(output), "%ls[%-6g][%06a][%LG][%+f]", u"", NAN, -NAN, -static_cast<long double>(NAN), -INFINITY) == 28
        && std::strcmp(output, "[nan   ][  -nan][-NAN][-inf]") == 0);
    Require(FormatList(output, sizeof(output), "%ls[%+f][%8.3f|]", u"", NAN, -NAN) == 17 && std::strcmp(output, "[+nan][    -nan|]") == 0);
    for (std::size_t capacity = 0; capacity <= 6; ++capacity) {
        std::memset(output, '!', sizeof(output));
        int count = -1;
        Require(FormatList(capacity ? output : nullptr, capacity, "%ls%n", u"A\u00e9Z", &count) == 4 && count == 4);
        const auto copied = capacity == 0 ? 0 : std::min<std::size_t>(capacity - 1, 4);
        Require(std::memcmp(output, "A\xc3\xa9Z", copied) == 0 && output[capacity] == '!');
        if (capacity) Require(output[copied] == 0);
    }
    return correct;
}

static void CheckSecureFormat() {
    constexpr size_t rsizeMax = SIZE_MAX >> 1;
    char buffer[16];
    std::memset(buffer, '!', sizeof(buffer));
    Require(FormatListS(buffer, 5, "%s", "abcdef") == 6 && std::strcmp(buffer, "abcd") == 0 && buffer[5] == '!');
    Require(snprintf_s_nid_postfix(buffer, 5, "%d-%ls", 42, u"abcd") == 7 && std::strcmp(buffer, "42-a") == 0);
    Require(FormatListS(buffer, 1, "%x", 0xabcu) == 3 && buffer[0] == 0);
    Require(snprintf_s_nid_postfix(buffer, sizeof(buffer), "") == 0 && buffer[0] == 0);
    buffer[0] = 'x';
    Require(FormatListS(buffer, sizeof(buffer), nullptr) < 0 && buffer[0] == 0);
    Require(FormatListS(nullptr, sizeof(buffer), "a") < 0 && snprintf_s_nid_postfix(nullptr, sizeof(buffer), "a") < 0);
    buffer[0] = 'x';
    Require(FormatListS(buffer, 0, "a") < 0 && snprintf_s_nid_postfix(buffer, 0, "a") < 0 && buffer[0] == 'x');
    Require(FormatListS(buffer, rsizeMax + 1, "a") < 0 && snprintf_s_nid_postfix(buffer, rsizeMax + 1, "a") < 0 && buffer[0] == 'x');
    int written = -7;
    Require(FormatListS(buffer, sizeof(buffer), "ab%n", &written) < 0 && buffer[0] == 0 && written == -7);
    buffer[0] = 'x';
    Require(snprintf_s_nid_postfix(buffer, sizeof(buffer), "ab%hhn", &written) < 0 && buffer[0] == 0 && written == -7);
    buffer[0] = 'x';
    Require(FormatListS(buffer, sizeof(buffer), "a%s", static_cast<const char*>(nullptr)) < 0 && buffer[0] == 0);
    buffer[0] = 'x';
    Require(snprintf_s_nid_postfix(buffer, sizeof(buffer), "a%ls", static_cast<const char16_t*>(nullptr)) < 0 && buffer[0] == 0);
    buffer[0] = 'x';
    Require(FormatListS(buffer, sizeof(buffer), "%2$s%1$S", static_cast<const char16_t*>(nullptr), "a") < 0 && buffer[0] == 0);
    Require(snprintf_s_nid_postfix(buffer, sizeof(buffer), "%.0s|%c", "", 'z') == 2 && std::strcmp(buffer, "|z") == 0);
    bool threw = false;
    try {
        FormatListS(buffer, sizeof(buffer), "%y", 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

__attribute__((noinline)) static void APS5_VABI RunChecks() {
    char buffer[1024];
    Require(FormatList(buffer, sizeof(buffer), "Mount requested: %d", 0) == 18);
    Require(std::strcmp(buffer, "Mount requested: 0") == 0);
    for (int index = 0; index < 10000; ++index) {
        snprintf_nid_postfix(buffer, sizeof(buffer), "%d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8);

        Require(std::strcmp(buffer, "1 2 3 4 5 6 7 8") == 0);
        FormatList(buffer, sizeof(buffer), "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %d", 1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 11);
        Require(std::strcmp(buffer, "1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0 11") == 0);
        snprintf_nid_postfix(buffer, sizeof(buffer), "%ld %lu %lld %zu %td %jd", -4294967297LL, 4294967297ULL, -5LL, 7ULL, -8LL, 9LL);

        Require(std::strcmp(buffer, "-4294967297 4294967297 -5 7 -8 9") == 0);
        snprintf_nid_postfix(buffer, sizeof(buffer), "%s:%*.*f:%d", "test", -8, 2, 1.25, 7);

        Require(std::strcmp(buffer, "test:1.25    :7") == 0);
        FormatList(buffer, sizeof(buffer), "%d %d %d %d %d %d %d %.3Lf %.1f", 1, 2, 3, 4, 5, 6, 7, 1.125L, 2.5);
        Require(std::strcmp(buffer, "1 2 3 4 5 6 7 1.125 2.5") == 0);
    }
    snprintf_nid_postfix(buffer, sizeof(buffer), "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f", 1., 2., 3., 4., 5., 6., 7., 8., 9., 10.);

    Require(std::strcmp(buffer, "1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0") == 0);
    snprintf_nid_postfix(buffer, sizeof(buffer), "%.3Lf %d %.1f", 1.125L, 7, 2.5);

    Require(std::strcmp(buffer, "1.125 7 2.5") == 0);
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "%'d|%'u|%'.2f|%'-8ld|%'+05d|%'g", 1234567, 4000000000u, 1234567.891, 1234567LL, 42, 1e6) == 50);
    Require(std::strcmp(buffer, "1234567|4000000000|1234567.89|1234567 |+0042|1e+06") == 0);
    Require(FormatList(buffer, sizeof(buffer), "%'lld %'zu", 1234567890123LL, static_cast<std::size_t>(9876543210)) == 24 && std::strcmp(buffer, "1234567890123 9876543210") == 0);
    Require(FormatList(buffer, sizeof(buffer), "%'d %ls", 1234567, u"x") == 9 && std::strcmp(buffer, "1234567 x") == 0);
    Require(sprintf_nid_postfix(buffer, "%2$s %1$d %2$s", 7, "x") == 5 && std::strcmp(buffer, "x 7 x") == 0);
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "%3$.1Lf|%2$.2f|%1$hhd|%4$s", 257, 2.5, 1.5L, "end") == 14);
    Require(std::strcmp(buffer, "1.5|2.50|1|end") == 0);
    Require(sprintf_nid_postfix(buffer, "%9$d%8$d%7$d%6$d%5$d%4$d%3$d%2$d%1$d", 1, 2, 3, 4, 5, 6, 7, 8, 9) == 9 && std::strcmp(buffer, "987654321") == 0);
    Require(FormatList(buffer, sizeof(buffer), "%11$d %10$g %9$g %8$g %7$g %6$g %5$g %4$g %3$g %2$g %1$g", 1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 11) == 23 &&
        std::strcmp(buffer, "11 10 9 8 7 6 5 4 3 2 1") == 0);
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "[%1$*2$.*3$f][%1$-*2$.1f][%4$*2$c]", 3.5, 7, 2, 'z') == 27);
    Require(std::strcmp(buffer, "[   3.50][3.5    ][      z]") == 0);
    Require(sprintf_nid_postfix(buffer, "[%2$*1$d][%2$.*1$d]", -4, 5) == 9 && std::strcmp(buffer, "[5   ][5]") == 0);
    Require(sprintf_nid_postfix(buffer, "[%1$*2$d]", 5, 3) == 5 && std::strcmp(buffer, "[  5]") == 0);
    Require(FormatList(buffer, sizeof(buffer), "%2$ls|%1$C|%3$d", 0x41, u"\u00e9", 7) == 6 && std::strcmp(buffer, "\xc3\xa9|A|7") == 0);
    Require(FormatList(buffer, sizeof(buffer), "%2$d %d %ls|%*5$d|%1$d", 1, 2, 3, u"x", 4) == 12 && std::strcmp(buffer, "2 3 x|   4|1") == 0);
    Require(FormatList(buffer, sizeof(buffer), "[%*3$d]%ls", 5, u"", 3) == 5 && std::strcmp(buffer, "[  5]") == 0);
    Require(FormatList(buffer, sizeof(buffer), "%100$d %2$d%1$ls", u"", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100) == 4 && std::strcmp(buffer, "99 1") == 0);
    char expected[1024];
    std::snprintf(expected, sizeof(expected), "%#08x %.3e %a %g %llx", 42u, 1.25, 1.25, 1.25,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(buffer)));
    snprintf_nid_postfix(buffer, sizeof(buffer), "%#08x %.3e %a %g %p", 42u, 1.25, 1.25, 1.25, static_cast<void*>(buffer));

    Require(std::strcmp(buffer, expected) == 0);
    char pointers[128];
    const auto address = [](std::uintptr_t value) { return reinterpret_cast<void*>(value); };
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "%p|%p|%p", address(0), address(0x1234), address(0xdeadbeef12)) == 17 &&
        std::strcmp(pointers, "0|1234|deadbeef12") == 0);
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "[%p][%5p][%-5p][%05p][%.8p][%10.6p][%.0p]",
        address(0x1234), address(0x1234), address(0x1234), address(0x1234), address(0x1234), address(0x1234), address(0)) == 51 &&
        std::strcmp(pointers, "[1234][ 1234][1234 ][01234][00001234][    001234][]") == 0);
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "[%#p][%+p][% p][%lp][%20p][%-20p]",
        address(0x1234), address(0x1234), address(0x1234), address(0x1234), address(0x1234), address(0x1234)) == 68 &&
        std::strcmp(pointers, "[1234][1234][1234][1234][                1234][1234                ]") == 0);
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "[%08p][%.6p][%8p]", address(0x1a), address(0x1a), address(0xf)) == 28 &&
        std::strcmp(pointers, "[0000001a][00001a][       f]") == 0);
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "[%*p][%-*p]", 6, address(0x1a), -6, address(0x1a)) == 16 &&
        std::strcmp(pointers, "[    1a][1a    ]") == 0);
    Require(FormatList(pointers, sizeof(pointers), "%2$p:%1$d", 7, address(0)) == 3 && std::strcmp(pointers, "0:7") == 0);
    Require(snprintf_nid_postfix(pointers, 6, "%p", address(0xabcdef)) == 6 && std::strcmp(pointers, "abcde") == 0);
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "%p", address(0xffffffffffffffffull)) == 16 &&
        std::strcmp(pointers, "ffffffffffffffff") == 0);
    snprintf_nid_postfix(buffer, sizeof(buffer), "%.*s", -1, "unlimited");

    Require(std::strcmp(buffer, "unlimited") == 0);
    buffer[5] = '!';
    Require(snprintf_nid_postfix(buffer, 5, "%s", "abcdef") == 6);
    Require(std::strcmp(buffer, "abcd") == 0 && buffer[5] == '!');
    Require(snprintf_nid_postfix(nullptr, 0, "%s:%d", "abcdef", 123) == 10);
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "[%s]", static_cast<const char*>(nullptr)) == 8 && std::strcmp(buffer, "[(null)]") == 0);
    buffer[0] = 'x';
    Require(snprintf_nid_postfix(buffer, 1, "%d", 123) == 3 && buffer[0] == 0);
    Require(sprintf_nid_postfix(buffer, "%hhd %hhu %hd %hu %%", 255, 257, 65535, 65537) == 11);
    Require(std::strcmp(buffer, "-1 1 -1 1 %") == 0);
    Require(sprintf_nid_postfix(buffer, "[%#.0o][%#.o][%#5.0o][%-#4.0o][%#.*o][%#.0o][%.0o][%#o][%#.3o]", 0u, 0u, 0u, 0u, 0, 0u, 8u, 0u, 0u, 0u) == 37);
    Require(std::strcmp(buffer, "[0][0][    0][0   ][0][010][][0][000]") == 0);
    Require(FormatList(buffer, sizeof(buffer), "[%#.0lo][%#.0hho][%#.0x]", 0ull, 0u, 0u) == 8 && std::strcmp(buffer, "[0][0][]") == 0);
    Require(FormatList(buffer, sizeof(buffer), "[%#.0o][%ls]", 0u, u"x") == 6 && std::strcmp(buffer, "[0][x]") == 0);
    Require(sprintf_nid_postfix(buffer, "%d %d %d %d %d %d", 1, 2, 3, 4, 5, 6) == 11);
    Require(std::strcmp(buffer, "1 2 3 4 5 6") == 0);
    int first = 0, second = 0, third = 0, fourth = 0, fifth = 0, sixth = 0;
    char word[16] = {};
    double real = 0.0;
    Require(sscanf_nid_postfix("1 2 3 4 5 6 seven 8.5", "%d %d %d %d %d %d %15s %lf",
        &first, &second, &third, &fourth, &fifth, &sixth, word, &real) == 8);
    Require(first == 1 && second == 2 && third == 3 && fourth == 4 && fifth == 5 && sixth == 6);
    Require(std::strcmp(word, "seven") == 0 && real == 8.5);
    int binary = -1, binaryEnd = -1;
    char binaryRest[8] = {};
    Require(sscanf_nid_postfix("0b101", "%i%n%7s", &binary, &binaryEnd, binaryRest) == 2);
    Require(binary == 0 && binaryEnd == 1 && std::strcmp(binaryRest, "b101") == 0);
    Require(ScanList(" -0B11", "%i", &binary) == 1 && binary == 0);
    Require(ScanListS("0b1", "%i", &binary) == 1 && binary == 0);
    Require(sscanf_nid_postfix("0x1f", "%i", &binary) == 1 && binary == 31);
#ifdef _WIN32
    int scanned = -1;
    int position = -1;
    Require(sscanf_nid_postfix("12", "%*d%d", &scanned) == 0 && scanned == -1);
    Require(sscanf_nid_postfix("12 ", "%*d %d", &scanned) == 0);
    Require(sscanf_nid_postfix("abc", "%*s%d", &scanned) == 0);
    Require(sscanf_nid_postfix("a", "%*c%d", &scanned) == 0);
    Require(sscanf_nid_postfix("abc", "%*[a-z]%d", &scanned) == 0);
    Require(sscanf_nid_postfix("12", "%*d%n%d", &position, &scanned) == 0 && position == 2);
    Require(sscanf_nid_postfix("12", "%*d%d%d", &scanned, &first) == 0);
    Require(sscanf_nid_postfix("1 2", "%d%*d%d", &scanned, &first) == 1 && scanned == 1);
    Require(sscanf_nid_postfix("1 2 3", "%*d %d %*d", &scanned) == 1 && scanned == 2);
    Require(sscanf_nid_postfix("", "%*d", &scanned) == EOF);
    Require(sscanf_nid_postfix("  ", "%*d%d", &scanned) == EOF);
    Require(sscanf_nid_postfix("", "%d", &scanned) == EOF);
#else
    {
        char16_t first[8] = {};
        char16_t letter = u'!';
        Require(sscanf_nid_postfix("ab cd", "%ls %lc", first, &letter) == 2);
        Require(std::u16string(first) == u"ab" && letter == u'c');
        Require(sscanf_nid_postfix("  xy z", "%S %C", first, &letter) == 2 && std::u16string(first) == u"xy" && letter == u'z');
        char16_t three[5] = {u'!', u'!', u'!', u'!', u'!'};
        Require(sscanf_nid_postfix("abcdef", "%3lc", three) == 1);
        Require(three[0] == u'a' && three[1] == u'b' && three[2] == u'c' && three[3] == u'!');
        Require(sscanf_nid_postfix("abcz", "%l[a-c]", first) == 1 && std::u16string(first) == u"abc");
        Require(sscanf_nid_postfix("ab cd", "%l[^ ]", first) == 1 && std::u16string(first) == u"ab");
        Require(sscanf_nid_postfix("]x", "%l[]x]", first) == 1 && std::u16string(first) == u"]x");
        char16_t second[8] = {};
        Require(sscanf_nid_postfix("\xc5\xbelu\xc5\xa5 k\xc5\xaf\xc5\x88", "%ls %ls", first, second) == 2);
        Require(std::u16string(first) == u"\u017elu\u0165" && std::u16string(second) == u"k\u016f\u0148");
        int number = 0;
        Require(sscanf_nid_postfix("\xc5\xbelu\xc5\xa5 7", "%2ls%*ls %d", first, &number) == 2);
        Require(std::u16string(first) == u"\u017el" && number == 7);
        char narrowWord[8] = {};
        Require(sscanf_nid_postfix("12 ab cd", "%d %ls %7s", &number, first, narrowWord) == 3);
        Require(number == 12 && std::u16string(first) == u"ab" && std::strcmp(narrowWord, "cd") == 0);
        int consumed = -1;
        Require(sscanf_nid_postfix("ab cd", "%ls%n", first, &consumed) == 1 && consumed == 2);
        Require(sscanf_nid_postfix("", "%ls", first) == EOF && sscanf_nid_postfix("   ", "%ls", first) == EOF);
        Require(sscanf_nid_postfix("xyz", "%l[a-c]", first) == 0);
        Require(sscanf_nid_postfix("1 xyz", "%d %l[a-c]", &number, first) == 1);
        Require(ScanList("hi there", "%ls %lc", first, &letter) == 2 && std::u16string(first) == u"hi" && letter == u't');
        char16_t fits[6] = {};
        Require(ScanListS("hello world", "%ls", fits, 6u) == 1 && std::u16string(fits) == u"hello");
        char16_t small[5] = {u'!', u'!', u'!', u'!', u'!'};
        Require(ScanListS("hello", "%ls", small, 5u) == 0 && small[0] == 0);
        Require(ScanListS("hello", "%4ls", small, 5u) == 1 && std::u16string(small) == u"hell");
        Require(ScanListS("xy", "%lc", &letter, 1u) == 1 && letter == u'x');
        small[0] = u'!';
        Require(ScanListS("xyz", "%3lc", small, 2u) == 0 && small[0] == 0);
        Require(ScanListS("ab cd", "%*ls %l[a-z]", fits, 6u) == 1 && std::u16string(fits) == u"cd");
        char16_t untouched[4] = {u'!', u'!', u'!', u'!'};
        const auto rejects = [&](const char* input, const char* format) {
            try { sscanf_nid_postfix(input, format, untouched); } catch (const std::runtime_error&) { return untouched[0] == u'!'; }
            return false;
        };
        Require(rejects("\xff", "%ls") && rejects("a b", "%2$ls") && rejects("\xc5\xbe", "%l[\xc5\xbe]"));
        Require(rejects("\xf0\x9f\x98\x80", "%ls"));
        Require(rejects("ab cd", "%2$C") && rejects("ab cd", "%'ls"));
    }
    char narrow[8] = {};
    Require(sscanf_nid_postfix("ab%", "%7[^%ls]", narrow) == 1 && std::strcmp(narrow, "ab") == 0);
#endif
    {
        char16_t text[8] = {u'!', u'!', u'!', u'!', u'!', u'!', u'!', u'!'};
        char16_t pair[3] = {u'!', u'!', u'!'};
        int number = -1, end = -1;
        const auto wideIs = [](const char16_t* value, const char16_t* expected) { return std::u16string(value) == expected; };
        Require(sscanf_nid_postfix("ab cd", "%ls", text) == 1 && wideIs(text, u"ab"));
        Require(sscanf_nid_postfix(" xyz", "%2lc", pair) == 1 && pair[0] == u' ' && pair[1] == u'x' && pair[2] == u'!');
        Require(sscanf_nid_postfix("abcd1", "%l[a-c]", text) == 1 && wideIs(text, u"abc"));
        Require(sscanf_nid_postfix("ef gh", "%S %C", text, pair) == 2 && wideIs(text, u"ef") && pair[0] == u'g');
        Require(sscanf_nid_postfix("7 word 9", "%d %ls %d%n", &number, text, &first, &end) == 3);
        Require(number == 7 && wideIs(text, u"word") && first == 9 && end == 8);
        Require(sscanf_nid_postfix("abcdef", "%3ls", text) == 1 && wideIs(text, u"abc"));
        Require(sscanf_nid_postfix("ab cd", "%*ls %ls", text) == 1 && wideIs(text, u"cd"));
#ifndef _WIN32
        Require(sscanf_nid_postfix("\xc3\xa9t\xc3\xa9", "%ls", text) == 1 && text[0] == 0x00E9 && text[1] == u't' && text[2] == 0x00E9 && text[3] == 0);
#endif
        Require(sscanf_nid_postfix("1", "%l[a-z]", text) == 0);
        Require(sscanf_nid_postfix("", "%ls", text) == EOF && sscanf_nid_postfix("   ", "%ls", text) == EOF);
        Require(sscanf_nid_postfix("", "%lc", pair) == EOF);
        Require(ScanList("vw xy", "%ls %lc", text, pair) == 2 && wideIs(text, u"vw") && pair[0] == u'x');
        Require(ScanListS("abc", "%ls", text, 4u) == 1 && wideIs(text, u"abc"));
        Require(ScanListS("abcdef", "%ls", text, 4u) == 0 && text[0] == 0);
        Require(ScanListS("abcd", "%3lc", pair, 2u) == 0 && pair[0] == 0);
        Require(ScanListS("abcd", "%2lc", pair, 2u) == 1 && pair[0] == u'a' && pair[1] == u'b');
    }
#ifdef _WIN32
    char freebsd[64];
    Require(snprintf_nid_postfix(freebsd, sizeof(freebsd), "%qd|%qu|%qx|%qo") == 15 &&
        std::strcmp(freebsd, "%qd|%qu|%qx|%qo") == 0);
    Require(snprintf_nid_postfix(freebsd, sizeof(freebsd), "%qd|%d", 7) == 5 && std::strcmp(freebsd, "%qd|7") == 0);
    Require(snprintf_nid_postfix(freebsd, sizeof(freebsd), "%D|%hD|%O|%U|%jD") == 14 &&
        std::strcmp(freebsd, "%D|%D|%O|%U|%D") == 0);
    Require(snprintf_nid_postfix(freebsd, sizeof(freebsd), "[%5%][%-5%][%y][%5y][%-3y][%hhz]") == 34 &&
        std::strcmp(freebsd, "[    %][%    ][%y][   %y][%y ][%z]") == 0);
    Require(snprintf_nid_postfix(freebsd, sizeof(freebsd), "[%#y][%.2y][%-#4.1y]") == 14 &&
        std::strcmp(freebsd, "[%y][%y][%y  ]") == 0);
    bool incomplete = false;
    try { snprintf_nid_postfix(freebsd, sizeof(freebsd), "100%"); } catch (const std::invalid_argument&) { incomplete = true; }
    Require(incomplete);
#endif
    long long count = -1;
    int smallCount = -1;
    Require(snprintf_nid_postfix(buffer, 3, "abcd%lnEF%n", &count, &smallCount) == 6);
    Require(count == 4 && smallCount == 6 && std::strcmp(buffer, "ab") == 0);
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "a%cb", 0) == 3);
    Require(buffer[0] == 'a' && buffer[1] == 0 && buffer[2] == 'b' && buffer[3] == 0);
    *__error_nid_postfix() = 35;
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "[%m][%.8m][%-10.5m][%d]", 7) == 23);
    Require(std::strcmp(buffer, "[%m][%m][%m        ][7]") == 0);
    Require(*__error_nid_postfix() == 35);
    Require(FormatList(buffer, sizeof(buffer), "%s: %m", "open") == 8 && std::strcmp(buffer, "open: %m") == 0);
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "[%5k][%-3y][%lk][%#.2k][%y%d][%0$d]", 42) == 31);
    Require(std::strcmp(buffer, "[   %k][%y ][%k][%k][%y42][%$d]") == 0);
    Require(printf_nid_postfix("printf: %d %.1f\n", 7, 2.5) == 14);
    Require(libc_printf_nid_postfix("libc_printf: %s\n", "OK") == 16);
    Require(libc_printf_nid_postfix("%d %d %d %d %d %d %d\n", 1, 22, 333, 4444, 55555, 666666, 7777777) == 35);
    Require(PrintList("vprintf: %d\n", 42) == 12);
    Require(FormatListS(buffer, sizeof(buffer), "%d-%s", 42, "x") == 4 && std::strcmp(buffer, "42-x") == 0);
    CheckSecureFormat();
    int scanNum = 0;
    char scanWord[8] = {};
    Require(ScanListS("123 test", "%d %s", &scanNum, scanWord, 8u) == 2 && scanNum == 123 && std::strcmp(scanWord, "test") == 0);
    char tooSmall[4] = {'x', 'x', 'x', 'x'};
    Require(ScanListS("abcdef", "%s", tooSmall, 4u) == 0 && tooSmall[0] == '\0');
    std::puts("Formatting checks passed: 10000 iterations");
}

static void* APS5_VABI Allocate(std::size_t bytes) { return GuestHeap::GuestHeapAllocate_nid_postfix(bytes); }
static void APS5_VABI Release(void* pointer) { GuestHeap::GuestHeapFree_nid_postfix(pointer); }
static void* APS5_VABI Unused() { std::abort(); }

static void CheckAllocatingFormat() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&Unused),
        reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    char* allocated = nullptr;
    Require(asprintf_nid_postfix(&allocated, "%ls:%S:%lc:%d", u"\u00e9", u"ab", 0x20ac, 7) == 11 && std::strcmp(allocated, "\xc3\xa9:ab:\xe2\x82\xac:7") == 0);
    free_nid_postfix(allocated);

}

int main() { CheckSizeLimits(); RunChecks(); CheckAllocatingFormat(); return CheckWidePrecision() ? 0 : 1; }
