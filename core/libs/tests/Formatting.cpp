#include "SceTypes.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cstdint>
#include <cstdio>

extern "C" {
int APS5_VABI snprintf_nid_postfix(char*, size_t, const char*, ...);
int APS5_VABI sprintf_nid_postfix(char*, const char*, ...);
int APS5_VABI printf_nid_postfix(const char*, ...);
int APS5_VABI libc_printf_nid_postfix(const char*, ...);
int APS5_VABI sscanf_nid_postfix(const char*, const char*, ...);
int APS5_VABI vsnprintf_nid_postfix(char*, size_t, const char*, VaList*);
int APS5_VABI vprintf_nid_postfix(const char*, VaList*);
}

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

static int APS5_VABI PrintList(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = vprintf_nid_postfix(format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static bool CheckWidePrecision() {
    const char16_t input[] = u"A\u00e9\u20ac\U0001f600Z";
    const char* expected[] = {"", "A", "A", "A\xc3\xa9", "A\xc3\xa9", "A\xc3\xa9", "A\xc3\xa9\xe2\x82\xac",
        "A\xc3\xa9\xe2\x82\xac", "A\xc3\xa9\xe2\x82\xac", "A\xc3\xa9\xe2\x82\xac",
        "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80Z"};
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
    Require(FormatList(output, sizeof(output), "%.3ls", u"\U0001f600") == 0 && output[0] == 0);
    Require(FormatList(output, sizeof(output), "%.*ls:%d", -1, u"\u00e9", 7) == 4 && std::strcmp(output, "\xc3\xa9:7") == 0);
    const char16_t bounded[] = {u'A', u'B'};
    Require(snprintf_nid_postfix(output, sizeof(output), "%.2ls", bounded) == 2 && std::strcmp(output, "AB") == 0);
    Require(snprintf_nid_postfix(nullptr, 0, "%.1ls", u"\u00e9") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "%.1s", "\xc3\xa9") == 1 && static_cast<unsigned char>(output[0]) == 0xc3 && output[1] == 0);
    Require(FormatList(output, sizeof(output), "%S:%lc:%C:%d", u"\u00e9", 0x20ac, 0x41, 7) == 10 && std::strcmp(output, "\xc3\xa9:\xe2\x82\xac:A:7") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "%.*S", 3, u"\u00e9\u20ac") == 2 && std::strcmp(output, "\xc3\xa9") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "[%ls]", static_cast<const char16_t*>(nullptr)) == 8 && std::strcmp(output, "[(null)]") == 0);
    Require(snprintf_nid_postfix(output, sizeof(output), "a%lcb%C", 0, 0) == 4 && std::memcmp(output, "a\0b\0", 5) == 0);
#ifndef _WIN32
    Require(FormatList(output, sizeof(output), "%%ls:%2$d:%1$d", 3, 7) == 7 && std::strcmp(output, "%ls:7:3") == 0);
#endif
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
    char expected[1024];
    std::snprintf(expected, sizeof(expected), "%#08x %.3e %a %g 0x%llx", 42u, 1.25, 1.25, 1.25,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(buffer)));
    snprintf_nid_postfix(buffer, sizeof(buffer), "%#08x %.3e %a %g %p", 42u, 1.25, 1.25, 1.25, static_cast<void*>(buffer));
    Require(std::strcmp(buffer, expected) == 0);
#ifdef _WIN32
    char pointers[128];
    const auto address = [](std::uintptr_t value) { return reinterpret_cast<void*>(value); };
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "%p|%p|%p", address(0), address(0x1234), address(0xdeadbeef12)) == 23 &&
        std::strcmp(pointers, "0x0|0x1234|0xdeadbeef12") == 0);
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "[%8p][%-8p][%08p][%.6p][%10.4p][%.0p]",
        address(0x1a), address(0x1a), address(0x1a), address(0x1a), address(0x1a), address(0)) == 56 &&
        std::strcmp(pointers, "[    0x1a][0x1a    ][0x00001a][0x00001a][    0x001a][0x]") == 0);
    Require(snprintf_nid_postfix(pointers, sizeof(pointers), "[%+p][% p][%#p][%*p][%-*p]", address(0x1a), address(0x1a),
        address(0x1a), 6, address(0xf), -6, address(0xf)) == 34 &&
        std::strcmp(pointers, "[0x1a][0x1a][0x1a][   0xf][0xf   ]") == 0);
    Require(snprintf_nid_postfix(pointers, 6, "%p", address(0xabcdef)) == 8 && std::strcmp(pointers, "0xabc") == 0);
#endif
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
    Require(sprintf_nid_postfix(buffer, "%d %d %d %d %d %d", 1, 2, 3, 4, 5, 6) == 11);
    Require(std::strcmp(buffer, "1 2 3 4 5 6") == 0);
    int first = 0, second = 0, third = 0, fourth = 0, fifth = 0, sixth = 0;
    char word[16] = {};
    double real = 0.0;
    Require(sscanf_nid_postfix("1 2 3 4 5 6 seven 8.5", "%d %d %d %d %d %d %15s %lf",
        &first, &second, &third, &fourth, &fifth, &sixth, word, &real) == 8);
    Require(first == 1 && second == 2 && third == 3 && fourth == 4 && fifth == 5 && sixth == 6);
    Require(std::strcmp(word, "seven") == 0 && real == 8.5);
    long long count = -1;
    int smallCount = -1;
    Require(snprintf_nid_postfix(buffer, 3, "abcd%lnEF%n", &count, &smallCount) == 6);
    Require(count == 4 && smallCount == 6 && std::strcmp(buffer, "ab") == 0);
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "a%cb", 0) == 3);
    Require(buffer[0] == 'a' && buffer[1] == 0 && buffer[2] == 'b' && buffer[3] == 0);
    Require(printf_nid_postfix("printf: %d %.1f\n", 7, 2.5) == 14);
    Require(libc_printf_nid_postfix("libc_printf: %s\n", "OK") == 16);
    Require(libc_printf_nid_postfix("%d %d %d %d %d %d %d\n", 1, 22, 333, 4444, 55555, 666666, 7777777) == 35);
    Require(PrintList("vprintf: %d\n", 42) == 12);
    std::puts("Formatting checks passed: 10000 iterations");
}

int main() { RunChecks(); return CheckWidePrecision() ? 0 : 1; }
