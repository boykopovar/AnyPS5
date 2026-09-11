#include "SceTypes.hpp"
#include <cstring>
#include <stdexcept>
#include <cstdio>

extern "C" {
int APS5_VABI snprintf_nid_postfix(char*, size_t, const char*, ...);
int APS5_VABI sprintf_nid_postfix(char*, const char*, ...);
int APS5_VABI printf_nid_postfix(const char*, ...);
int APS5_VABI libc_printf_nid_postfix(const char*, ...);
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
    std::snprintf(expected, sizeof(expected), "%#08x %.3e %a %g %p", 42u, 1.25, 1.25, 1.25, static_cast<void*>(buffer));
    snprintf_nid_postfix(buffer, sizeof(buffer), "%#08x %.3e %a %g %p", 42u, 1.25, 1.25, 1.25, static_cast<void*>(buffer));
    Require(std::strcmp(buffer, expected) == 0);
    snprintf_nid_postfix(buffer, sizeof(buffer), "%.*s", -1, "unlimited");
    Require(std::strcmp(buffer, "unlimited") == 0);
    buffer[5] = '!';
    Require(snprintf_nid_postfix(buffer, 5, "%s", "abcdef") == 6);
    Require(std::strcmp(buffer, "abcd") == 0 && buffer[5] == '!');
    Require(snprintf_nid_postfix(nullptr, 0, "%s:%d", "abcdef", 123) == 10);
    buffer[0] = 'x';
    Require(snprintf_nid_postfix(buffer, 1, "%d", 123) == 3 && buffer[0] == 0);
    Require(sprintf_nid_postfix(buffer, "%hhd %hhu %hd %hu %%", 255, 257, 65535, 65537) == 11);
    Require(std::strcmp(buffer, "-1 1 -1 1 %") == 0);
    long long count = -1;
    int smallCount = -1;
    Require(snprintf_nid_postfix(buffer, 3, "abcd%lnEF%n", &count, &smallCount) == 6);
    Require(count == 4 && smallCount == 6 && std::strcmp(buffer, "ab") == 0);
    Require(snprintf_nid_postfix(buffer, sizeof(buffer), "a%cb", 0) == 3);
    Require(buffer[0] == 'a' && buffer[1] == 0 && buffer[2] == 'b' && buffer[3] == 0);
    Require(printf_nid_postfix("printf: %d %.1f\n", 7, 2.5) == 14);
    Require(libc_printf_nid_postfix("libc_printf: %s\n", "OK") == 16);
    Require(PrintList("vprintf: %d\n", 42) == 12);
    std::puts("Formatting checks passed: 10000 iterations");
}

int main() { RunChecks(); }
