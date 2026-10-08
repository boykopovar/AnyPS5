#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" int APS5_VABI vswprintf_nid_postfix(char16_t*, std::size_t, const char16_t*, VaList*);
extern "C" int APS5_VABI snwprintf_s_nid_postfix(char16_t*, std::size_t, const char16_t*, ...);

static int APS5_VABI Format(char16_t* buffer, std::size_t size, const char16_t* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vswprintf_nid_postfix(buffer, size, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

static void Check(const char16_t* format, const char* input, const char16_t* expected) {
    char16_t buffer[32]{};
    const int result = Format(buffer, 32, format, input);
    const std::u16string wanted(expected);
    if (result != static_cast<int>(wanted.size()) || buffer != wanted) {
        std::fprintf(stderr, "wide precision: expected %zu units, got %d\n", wanted.size(), result);
        std::abort();
    }
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "snwprintf_s: %s\n", message);
        std::abort();
    }
}

static void CheckBounded() {
    constexpr std::size_t rsizeMax = SIZE_MAX >> 1;
    const char16_t* const nullWide = nullptr;
    const char* const nullNarrow = nullptr;
    char16_t buffer[16];

    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, u"%d-%ls-%s", 42, u"ab", "\xc3\xa9") == 7, "complete output length");
    Require(buffer == std::u16string(u"42-ab-\u00e9"), "complete output");

    buffer[5] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 5, u"%d-%ls", 42, u"abcd") == 7, "truncated output length");
    Require(buffer == std::u16string(u"42-a") && buffer[5] == u'x', "truncated output");

    buffer[0] = u'x';
    buffer[1] = u'y';
    Require(snwprintf_s_nid_postfix(buffer, 1, u"%x", 0xabcu) == 3, "one unit buffer length");
    Require(buffer[0] == 0 && buffer[1] == u'y', "one unit buffer");

    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, u"") == 0 && buffer[0] == 0, "empty format");

    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, nullptr) < 0 && buffer[0] == 0, "null format");
    Require(snwprintf_s_nid_postfix(nullptr, 16, u"a") < 0, "null buffer");

    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 0, u"a") < 0 && buffer[0] == u'x', "zero size");

    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, rsizeMax + 1, u"a") < 0 && buffer[0] == u'x', "size above RSIZE_MAX");

    int written = -7;
    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, u"ab%n", &written) < 0 && buffer[0] == 0 && written == -7, "%n");

    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, u"a%ls", nullWide) < 0 && buffer[0] == 0, "null %ls argument");

    buffer[0] = u'x';
    Require(snwprintf_s_nid_postfix(buffer, 16, u"a%s", nullNarrow) < 0 && buffer[0] == 0, "null %s argument");

    Require(Format(buffer, 16, u"%s", nullNarrow) == 6 && buffer == std::u16string(u"(null)"), "vswprintf null %s argument");
}

static void CheckCount() {
    char16_t buffer[16];
    int count = -7;
    Require(Format(buffer, 16, u"ab%ncd", &count) == 4 && buffer == std::u16string(u"abcd") && count == 2, "vswprintf %n");
    count = -7;
    Require(Format(buffer, 16, u"%s%n!", "\xf0\x9f\x98\x80", &count) == 3 && count == 2, "vswprintf %n counts UTF-16 units");
    count = -7;
    Require(Format(buffer, 16, u"%n", &count) == 0 && buffer[0] == 0 && count == 0, "vswprintf %n at start");
    unsigned char bytes[2] = {0xaa, 0xaa};
    Require(Format(buffer, 16, u"%3d%hhn", 5, bytes) == 3 && bytes[0] == 3 && bytes[1] == 0xaa, "vswprintf %hhn");
    unsigned short halves[2] = {0xaaaa, 0xaaaa};
    Require(Format(buffer, 16, u"%5d%hn", 5, halves) == 5 && halves[0] == 5 && halves[1] == 0xaaaa, "vswprintf %hn");
    long long wide = -1;
    Require(Format(buffer, 16, u"%4d%lln", 5, &wide) == 4 && wide == 4, "vswprintf %lln");
    wide = -1;
    Require(Format(buffer, 16, u"%2d%ln", 5, &wide) == 2 && wide == 2, "vswprintf %ln");
    count = -7;
    Require(Format(buffer, 16, u"a%5n", &count) < 0 && count == -7, "vswprintf %n with a width");
    Require(Format(buffer, 16, u"a%Ln", &count) < 0 && count == -7, "vswprintf %Ln");
    const int* const nullCount = nullptr;
    Require(Format(buffer, 16, u"a%n", nullCount) < 0, "vswprintf null %n argument");
}

int main() {
    CheckBounded();
    CheckCount();
    Check(u"%.2s", "\xc3\xa9\xc3\xa8", u"\u00e9\u00e8");
    Check(u"%.1s", "\xc3\xa9\xc3\xa8", u"\u00e9");
    Check(u"%.0s", "\xc3\xa9", u"");
    Check(u"%.3s", "\xc3\xa9", u"\u00e9");
    Check(u"%s", "\xc3\xa9\xc3\xa8", u"\u00e9\u00e8");
    Check(u"%.2s", "abcd", u"ab");
    Check(u"%4.2s", "\xc3\xa9\xc3\xa8", u"  \u00e9\u00e8");
    Check(u"%-4.2s", "\xc3\xa9\xc3\xa8", u"\u00e9\u00e8  ");
    Check(u"%.2s", "\xf0\x9f\x98\x80x", u"\U0001f600");
    Check(u"%.1s", "\xf0\x9f\x98\x80x", u"");
    Check(u"%.3s", "\xf0\x9f\x98\x80x", u"\U0001f600x");
}
