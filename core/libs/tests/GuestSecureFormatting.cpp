#include "SceTypes.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
int APS5_VABI vsnprintf_s_nid_postfix(char*, std::size_t, const char*, VaList*);
int APS5_VABI vsscanf_s_nid_postfix(const char*, const char*, VaList*);
}

static int failures = 0;

static void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

#ifdef _WIN32
#define GUEST_VA_BEGIN(last) __builtin_sysv_va_list args; __builtin_sysv_va_start(args, last)
#define GUEST_VA_LIST reinterpret_cast<VaList*>(args)
#define GUEST_VA_END() __builtin_sysv_va_end(args)
#else
#define GUEST_VA_BEGIN(last) std::va_list args; va_start(args, last)
#define GUEST_VA_LIST reinterpret_cast<VaList*>(&args)
#define GUEST_VA_END() va_end(args)
#endif

static int APS5_VABI Format(char* buffer, std::size_t size, const char* format, ...) {
    GUEST_VA_BEGIN(format);
    const int result = vsnprintf_s_nid_postfix(buffer, size, format, GUEST_VA_LIST);
    GUEST_VA_END();
    return result;
}

static int APS5_VABI Scan(const char* input, const char* format, ...) {
    GUEST_VA_BEGIN(format);
    const int result = vsscanf_s_nid_postfix(input, format, GUEST_VA_LIST);
    GUEST_VA_END();
    return result;
}

static void CheckFormat() {
    char buffer[32];
    Require(Format(buffer, sizeof(buffer), "%d-%s", 42, "ab") == 5 && std::strcmp(buffer, "42-ab") == 0, "formats");
    Require(Format(buffer, 4, "%s", "abcdef") == 6 && std::strcmp(buffer, "abc") == 0, "truncates and counts the full length");
    Require(Format(buffer, sizeof(buffer), "100%%n") == 5 && std::strcmp(buffer, "100%n") == 0, "a literal %n is allowed");
    Require(Format(buffer, sizeof(buffer), "%*d %.2f %Lf %c %p %lld %s", 3, 7, 1.5, static_cast<long double>(2), 'x',
        static_cast<void*>(nullptr), -5LL, "ok") > 0, "walks every argument kind");
    Require(std::strncmp(buffer, "  7 1.50 2.000000 x ", 20) == 0, "argument order");

    std::memset(buffer, 'x', sizeof(buffer));
    int count = 0;
    Require(Format(buffer, sizeof(buffer), "ab%n", &count) < 0 && buffer[0] == '\0' && count == 0, "%n is rejected");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(Format(buffer, sizeof(buffer), "%-3hhd%5.1n", 1, &count) < 0 && buffer[0] == '\0', "%n with flags is rejected");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(Format(buffer, sizeof(buffer), "%s", static_cast<const char*>(nullptr)) < 0 && buffer[0] == '\0',
        "a null %s argument is rejected");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(Format(buffer, sizeof(buffer), "%*d %.2f %Lf %c %p %lld %s", 3, 7, 1.5, static_cast<long double>(2), 'x',
        static_cast<void*>(buffer), -5LL, static_cast<const char*>(nullptr)) < 0 && buffer[0] == '\0',
        "a null %s argument after other kinds is rejected");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(Format(buffer, sizeof(buffer), "%ls", static_cast<const char16_t*>(nullptr)) < 0 && buffer[0] == '\0',
        "a null %ls argument is rejected");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(Format(buffer, sizeof(buffer), nullptr) < 0 && buffer[0] == '\0', "a null format is rejected");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(Format(buffer, 0, "%d", 1) < 0 && buffer[0] == 'x', "a zero size leaves the buffer alone");
    Require(Format(buffer, SIZE_MAX, "%d", 1) < 0 && buffer[0] == 'x', "a size above RSIZE_MAX leaves the buffer alone");
    Require(Format(nullptr, sizeof(buffer), "%d", 1) < 0, "a null buffer is rejected");
}

static void CheckScan() {
    int number = 0;
    char word[8] = {};
    Require(Scan("12 abc", "%d %s", &number, word, sizeof(word)) == 2 && number == 12 && std::strcmp(word, "abc") == 0,
        "scans with buffer sizes");
    std::memset(word, 'x', sizeof(word));
    Require(Scan("abcdef", "%s", word, static_cast<std::size_t>(3)) == 0 && word[0] == '\0', "a short buffer fails");
    char pair[2] = {};
    Require(Scan("xy", "%2c", pair, sizeof(pair)) == 1 && pair[0] == 'x' && pair[1] == 'y', "%c takes a size");
    Require(Scan("abc123", "%[a-z]%d", word, sizeof(word), &number) == 2 && std::strcmp(word, "abc") == 0 && number == 123,
        "scan sets take a size");
    Require(Scan("", "%d", &number) == EOF, "empty input");
    Require(Scan(nullptr, "%d", &number) == EOF, "a null input is rejected");
    Require(Scan("1", nullptr) == EOF, "a null format is rejected");
    Require(Scan("5", "%d", static_cast<int*>(nullptr)) == EOF, "a null target is rejected");
    number = 0;
    Require(Scan("4 5", "%d %d", &number, static_cast<int*>(nullptr)) == EOF && number == 4, "a later null target is rejected");
}

int main() {
    CheckFormat();
    CheckScan();
    if (failures != 0) return 1;
    std::puts("guest secure formatting passed");
    return 0;
}
