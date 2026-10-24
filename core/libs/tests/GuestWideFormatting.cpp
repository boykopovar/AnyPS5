#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

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

static void CheckPositional() {
    char16_t buffer[32];
    Require(swprintf_nid_postfix(buffer, 32, u"%2$d-%1$d", 1, 2) == 3 && buffer == std::u16string(u"2-1"),
        "swprintf positional integers");
    Require(swprintf_nid_postfix(buffer, 32, u"%2$ls %1$ls", u"world", u"hello") == 11 && buffer == std::u16string(u"hello world"),
        "swprintf positional wide strings");
    Require(swprintf_nid_postfix(buffer, 32, u"[%1$*2$d|%3$.*2$f]", 7, 2, 2.5) == 9 && buffer == std::u16string(u"[ 7|2.50]"),
        "swprintf positional width and precision");
    Require(swprintf_nid_postfix(buffer, 32, u"%1$Lg %2$d %d", 0.5L, 3, 9) == 7 && buffer == std::u16string(u"0.5 3 9"),
        "swprintf positional long double and the next argument");
    Require(snwprintf_s_nid_postfix(buffer, 32, u"%2$s%1$lc", 0x20ac, "\xc3\xa9") == 2 && buffer == std::u16string(u"\u00e9\u20ac"),
        "snwprintf_s positional arguments");
}

static void CheckPointer() {
    char16_t buffer[64];
    const auto address = [](std::uintptr_t value) { return reinterpret_cast<void*>(value); };
    Require(Format(buffer, 64, u"%p|%p|%p", address(0), address(0x1234), address(0xdeadbeef12)) == 17 &&
        buffer == std::u16string(u"0|1234|deadbeef12"), "vswprintf %p uses the console form");
    Require(Format(buffer, 64, u"[%10p][%-10p][%.8p][%08p]", address(0x1234), address(0x1234), address(0x1234), address(0x1a)) == 44 &&
        buffer == std::u16string(u"[      1234][1234      ][00001234][0000001a]"), "vswprintf %p width and precision");
    Require(Format(buffer, 64, u"[%.0p][%p]", address(0), address(0)) == 5 &&
        buffer == std::u16string(u"[][0]"), "vswprintf %p with a zero precision prints nothing");
    Require(Format(buffer, 64, u"[%lp]", address(0x1234)) == -1, "vswprintf %lp is refused");
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

static void CheckGroupingFlag() {
    char16_t buffer[48];
    Require(swprintf_nid_postfix(buffer, 48, u"%'d|%'-6u|%'.2f|%'+05d|%'g", 1234567, 42u, 1234.5, 12, 1e6) == 34 &&
        buffer == std::u16string(u"1234567|42    |1234.50|+0012|1e+06"), "swprintf ' flag");
    Require(snwprintf_s_nid_postfix(buffer, 48, u"%'lld %ls", 9876543210ll, u"x") == 12 && buffer == std::u16string(u"9876543210 x"),
        "snwprintf_s ' flag");
}

static void CheckSecureWideBoundary() {
    constexpr std::size_t wideRsizeMax = (SIZE_MAX >> 1) / sizeof(char16_t);
    char16_t buffer[4];
    for (auto& unit : buffer) unit = u'x';

    buffer[0] = u'x';
    buffer[1] = u'y';
    Require(swprintf_s_nid_postfix(buffer, wideRsizeMax, u"%n") < 0 && buffer[0] == 0, "swprintf_s accepted-boundary failure clears");
    Require(buffer[1] == u'y', "swprintf_s does not clear beyond the first unit");

    buffer[0] = u'x';
    buffer[1] = u'y';
    Require(swprintf_s_nid_postfix(buffer, wideRsizeMax + 1, u"%n") < 0 && buffer[0] == u'x' && buffer[1] == u'y',
        "swprintf_s oversized size leaves the buffer untouched");

    buffer[0] = u'x';
    buffer[1] = u'y';
    Require(vswprintf_s_nid_postfix(buffer, wideRsizeMax, u"%n", nullptr) < 0 && buffer[0] == 0, "vswprintf_s accepted-boundary failure clears");
    buffer[0] = u'x';
    buffer[1] = u'y';
    Require(vswprintf_s_nid_postfix(buffer, wideRsizeMax + 1, u"%n", nullptr) < 0 && buffer[0] == u'x' && buffer[1] == u'y',
        "vswprintf_s oversized size leaves the buffer untouched");

    buffer[0] = u'x';
    buffer[1] = u'y';
    Require(vsnwprintf_s_nid_postfix(buffer, wideRsizeMax, u"%n", nullptr) < 0 && buffer[0] == 0, "vsnwprintf_s accepted-boundary failure clears");
    buffer[0] = u'x';
    buffer[1] = u'y';
    Require(vsnwprintf_s_nid_postfix(buffer, wideRsizeMax + 1, u"%n", nullptr) < 0 && buffer[0] == u'x' && buffer[1] == u'y',
        "vsnwprintf_s oversized size leaves the buffer untouched");
}

static void CheckSwprintf() {
    char16_t buffer[8];
    for (auto& unit : buffer) unit = u'x';
    Require(swprintf_nid_postfix(buffer, 8, u"%d-%ls", 42, u"ab") == 5, "swprintf length");
    Require(buffer == std::u16string(u"42-ab") && buffer[6] == u'x', "swprintf output");
    Require(swprintf_nid_postfix(buffer, 8, u"中%ls%s", u"文", "\xc3\xa9") == 3, "swprintf non-ASCII length");
    Require(buffer == std::u16string(u"中文é"), "swprintf non-ASCII output");
    Require(swprintf_nid_postfix(buffer, 8, u"%s", "\xf0\x9f\x98\x80") == 2, "swprintf surrogate pair length");
    Require(buffer == std::u16string(u"\U0001f600"), "swprintf surrogate pair output");
    Require(swprintf_nid_postfix(buffer, 8, u"") == 0 && buffer[0] == 0, "swprintf empty format");
    for (auto& unit : buffer) unit = u'x';
    Require(swprintf_nid_postfix(buffer, 3, u"%ls", u"abcd") < 0, "swprintf truncation result");
    Require(buffer == std::u16string(u"ab") && buffer[3] == u'x', "swprintf truncation output");
    Require(swprintf_nid_postfix(buffer, 3, u"abc") < 0, "swprintf terminator does not fit");
    buffer[0] = u'x';
    Require(swprintf_nid_postfix(buffer, 0, u"a") < 0 && buffer[0] == u'x', "swprintf zero size");
    Require(swprintf_nid_postfix(nullptr, 8, u"a") < 0, "swprintf null buffer");
    Require(swprintf_nid_postfix(buffer, 5, u"%ls", u"abcd") == 4 && buffer == std::u16string(u"abcd"), "swprintf exact fit");
    char16_t wide[16];
    Require(swprintf_nid_postfix(wide, 16, u"%5ls|%-3c|%lc", u"wide", 'z', 0x20ac) == 11, "swprintf padded length");
    Require(wide == std::u16string(u" wide|z  |€"), "swprintf padded output");
    Require(swprintf_nid_postfix(wide, 16, u"%q") < 0 && wide[0] == 0, "swprintf invalid conversion");
}

static bool WcsftimeThrows(char16_t* buffer, const char16_t* format, const GuestTm* time) {
    try {
        wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 16, reinterpret_cast<const std::uint16_t*>(format), time);

    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void CheckWcsftime() {
    GuestTm time{};
    time.tm_sec = 9;
    time.tm_min = 5;
    time.tm_hour = 13;
    time.tm_mday = 29;
    time.tm_mon = 1;
    time.tm_year = 124;
    time.tm_wday = 4;
    time.tm_yday = 59;
    time.tm_zone = "UTC";
    struct Case {
        const char16_t* wide;
        const char* narrow;
    };
    const Case cases[] = {
        {u"%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M:%S"},
        {u"%a %b %j %p %Z", "%a %b %j %p %Z"},
        {u"%%", "%%"},
        {u"%k|%l|%v|%+", "%k|%l|%v|%+"},
        {u"\u00e9t\u00e9 %Y", "\xe9t\xe9 %Y"},
    };
    for (const auto& item : cases) {
        char narrow[128];
        char16_t wide[128];
        const std::size_t length = strftime_nid_postfix(narrow, sizeof(narrow), item.narrow, &time);
        Require(length != 0, "wcsftime reference strftime");
        Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(wide), 128, reinterpret_cast<const std::uint16_t*>(item.wide), &time) == length, "wcsftime length matches strftime");
        for (std::size_t index = 0; index <= length; ++index)
            Require(wide[index] == static_cast<unsigned char>(narrow[index]), "wcsftime output matches strftime");
    }
    char16_t buffer[32];
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 32, reinterpret_cast<const std::uint16_t*>(u"%Y-%m-%d %H:%M:%S"), &time) == 19
        && buffer == std::u16string(u"2024-02-29 13:05:09"), "wcsftime date and time");
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 32, reinterpret_cast<const std::uint16_t*>(u"%a %b %j %p"), &time) == 14
        && buffer == std::u16string(u"Thu Feb 060 PM"), "wcsftime names");
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 32, reinterpret_cast<const std::uint16_t*>(u"%%"), &time) == 1 && buffer == std::u16string(u"%"), "wcsftime percent");
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 20, reinterpret_cast<const std::uint16_t*>(u"%Y-%m-%d %H:%M:%S"), &time) == 19, "wcsftime exact fit");

    int& error = *__error_nid_postfix();
    for (auto& unit : buffer) unit = u'x';
    error = 5;
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 19, reinterpret_cast<const std::uint16_t*>(u"%Y-%m-%d %H:%M:%S"), &time) == 0 && error == 5 && buffer[0] == u'x',
        "wcsftime overflow");
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 1, reinterpret_cast<const std::uint16_t*>(u"%Y"), &time) == 0 && error == 5 && buffer[0] == u'x', "wcsftime one unit");
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 0, reinterpret_cast<const std::uint16_t*>(u"%Y"), &time) == 0 && error == 5 && buffer[0] == u'x', "wcsftime zero size");
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 32, reinterpret_cast<const std::uint16_t*>(u""), &time) == 0 && error == 5 && buffer[0] == u'x', "wcsftime empty format");
    Require(wcsftime_nid_postfix(nullptr, 32, reinterpret_cast<const std::uint16_t*>(u"%Y"), &time) == 0 && error == 5, "wcsftime null buffer");
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 32, reinterpret_cast<const std::uint16_t*>(u"%Y \u20ac"), &time) == 6 && buffer == std::u16string(u"2024 \u20ac"),
        "wcsftime passes a character above 0xff through UTF-8");
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), 7, reinterpret_cast<const std::uint16_t*>(u"%Y \u20ac"), &time) == 6, "wcsftime sizes the narrow buffer for three-byte characters");
    for (auto& unit : buffer) unit = u'x';
    error = 5;
    Require(wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(buffer), SIZE_MAX, reinterpret_cast<const std::uint16_t*>(u"%Y"), &time) == 0 && error == 22 && buffer[0] == u'x',
        "wcsftime oversized buffer");
    Require(WcsftimeThrows(buffer, nullptr, &time), "wcsftime null format");
    Require(WcsftimeThrows(buffer, u"%Y", nullptr), "wcsftime null time");
}

static void CheckAlternateOctalZero() {
    struct Case { const char16_t* format; const char16_t* expected; unsigned value; };
    for (const auto& test : {
        Case{u"%#.0o", u"0", 0}, Case{u"%#.o", u"0", 0}, Case{u"%#.00o", u"0", 0},
        Case{u"[%#5.0o]", u"[    0]", 0}, Case{u"[%-#5.0o]", u"[0    ]", 0},
        Case{u"[%#05.0o]", u"[    0]", 0}, Case{u"%.0o", u"", 0}, Case{u"%#.0x", u"", 0},
        Case{u"%#.0o", u"01", 1}, Case{u"%#.1o", u"0", 0}, Case{u"%#.3o", u"000", 0},
        Case{u"%#.0hho", u"0", 256}, Case{u"%#.0ho", u"0", 65536}
    }) {
        char16_t buffer[32];
        const auto expected = std::u16string(test.expected);
        Require(swprintf_nid_postfix(buffer, 32, test.format, test.value) == static_cast<int>(expected.size()) && buffer == expected, "swprintf alternate octal zero");
        Require(Format(buffer, 32, test.format, test.value) == static_cast<int>(expected.size()) && buffer == expected, "vswprintf alternate octal zero");
        Require(snwprintf_s_nid_postfix(buffer, 32, test.format, test.value) == static_cast<int>(expected.size()) && buffer == expected, "snwprintf_s alternate octal zero");
    }
    char16_t buffer[16];
    Require(swprintf_nid_postfix(buffer, 16, u"%#.*o", 0, 0u) == 1 && buffer == std::u16string(u"0"), "octal zero dynamic precision");
    Require(swprintf_nid_postfix(buffer, 16, u"%#.*o", -1, 0u) == 1 && buffer == std::u16string(u"0"), "octal zero omitted precision");
    Require(swprintf_nid_postfix(buffer, 16, u"%2$#.*1$o", 0, 0u) == 1 && buffer == std::u16string(u"0"), "octal zero positional precision");
    Require(swprintf_nid_postfix(buffer, 16, u"%#.0llo", 0ull) == 1 && buffer == std::u16string(u"0"), "octal zero long long");
    buffer[0] = u'x';
    buffer[1] = u'y';
    Require(snwprintf_s_nid_postfix(buffer, 1, u"%#.0o", 0u) == 1 && buffer[0] == 0 && buffer[1] == u'y', "octal zero bounded output");
}

int main() {
    CheckAlternateOctalZero();
    CheckBounded();
    CheckPositional();
    CheckPointer();
    CheckCount();
    CheckSwprintf();
    CheckSecureWideBoundary();
    CheckWcsftime();
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
    CheckGroupingFlag();
}
