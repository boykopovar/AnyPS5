#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

template<typename TCall> static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void CheckStoul(const char16_t* text, int base, std::uint64_t value, std::size_t consumed, int error, const char* message) {
    char16_t* end = nullptr;
    errno = 0;
    const std::uint64_t result = _WStoul_nid_postfix(text, &end, base);
    const int saved = errno;
    if (result != value || end != text + consumed || saved != error) {
        std::fprintf(stderr, "_WStoul %s: value %llu, consumed %lld, errno %d\n", message,
            static_cast<unsigned long long>(result), static_cast<long long>(end - text), saved);
        Expect(false);
    }
}

static void CheckStoul() {
    constexpr std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    CheckStoul(u"  4294967296", 10, 4294967296ULL, 12, 0, "keeps 64 bits");
    CheckStoul(u"18446744073709551615", 10, max, 20, 0, "maximum");
    CheckStoul(u"18446744073709551616", 10, max, 20, ERANGE, "overflow");
    CheckStoul(u"-1", 10, max, 2, 0, "negation");
    CheckStoul(u"\t+7xyz", 10, 7, 3, 0, "stops at a non-digit");
    CheckStoul(u"0x1F", 16, 31, 4, 0, "hex prefix with base 16");
    CheckStoul(u"0x1f", 0, 31, 4, 0, "hex prefix with base 0");
    CheckStoul(u"017", 0, 15, 3, 0, "octal with base 0");
    CheckStoul(u"z", 36, 35, 1, 0, "base 36");
    CheckStoul(u"0b1", 0, 0, 1, 0, "no binary prefix");
    CheckStoul(u"0xg", 16, 0, 0, 0, "prefix without digits");
    CheckStoul(u"12é3", 10, 12, 2, 0, "stops at a non-ASCII unit");
    CheckStoul(u"　5", 10, 0, 0, 0, "non-ASCII space is not skipped");
    CheckStoul(u"", 10, 0, 0, 0, "empty string");
    Expect(_WStoul_nid_postfix(u"9", nullptr, 10) == 9, "_WStoul accepts a null end pointer");
    CheckStoul(u" 1", 1, 0, 0, 0, "rejects base 1 without consuming or setting errno");
    CheckStoul(u" 1", 37, 0, 0, 0, "rejects base 37 without consuming or setting errno");
    CheckStoul(u" 1", -2, 0, 0, 0, "rejects a negative base without consuming or setting errno");
}

template<typename TValue, typename TConvert>
static void CheckConversion(TConvert convert, const char16_t* text, long argument, TValue value, std::size_t consumed, int error, const char* message) {
    char16_t* end = nullptr;
    errno = 0;
    const TValue result = convert(text, &end, argument);
    if (result != value || end != text + consumed || errno != error) {
        std::fprintf(stderr, "%s: consumed %lld, errno %d\n", message, static_cast<long long>(end - text), errno);
        Expect(false);
    }
}

static void CheckDinkumwareConversions() {
    constexpr std::uint64_t unsignedMax = std::numeric_limits<std::uint64_t>::max();
    constexpr long long signedMax = std::numeric_limits<long long>::max();
    constexpr long long signedMin = std::numeric_limits<long long>::min();
    const auto stoull = [](const char16_t* text, char16_t** end, long base) { return _WStoull_nid_postfix(text, end, static_cast<int>(base)); };
    const auto stoll = [](const char16_t* text, char16_t** end, long base) { return _WStoll_nid_postfix(text, end, static_cast<int>(base)); };
    CheckConversion<unsigned long long>(stoull, u" -0x10", 16, 18446744073709551600ULL, 6, 0, "_WStoull negates after the prefix");
    CheckConversion<unsigned long long>(stoull, u"0x", 0, 0, 0, 0, "_WStoull prefix without digits converts nothing");
    CheckConversion<unsigned long long>(stoull, u"-18446744073709551616", 10, unsignedMax, 21, ERANGE, "_WStoull overflow ignores the sign");
    CheckConversion<unsigned long long>(stoull, u"0000000000000000000000001", 10, 1, 25, 0, "_WStoull does not count leading zeros");
    CheckConversion<long long>(stoll, u"9223372036854775807", 10, signedMax, 19, 0, "_WStoll maximum");
    CheckConversion<long long>(stoll, u"9223372036854775808", 10, signedMax, 19, ERANGE, "_WStoll positive overflow");
    CheckConversion<long long>(stoll, u"-9223372036854775808", 10, signedMin, 20, 0, "_WStoll minimum");
    CheckConversion<long long>(stoll, u"-9223372036854775809", 10, signedMin, 20, ERANGE, "_WStoll negative overflow");
    CheckConversion<long long>(stoll, u"- 5", 10, -5, 3, 0, "_WStoll skips white space after the sign");
    CheckConversion<long long>(stoll, u"--1", 10, signedMin, 3, ERANGE, "_WStoll second sign overflows");
    CheckConversion<long long>(stoll, u"  +0x7f", 0, 127, 7, 0, "_WStoll base detection");
    CheckConversion<long long>(stoll, u"", 2, 0, 0, 0, "_WStoll empty string");
    CheckConversion<double>(_WStod_nid_postfix, u" 1.5", 2, 150.0, 4, 0, "_WStod scales by a power of ten");
    CheckConversion<double>(_WStod_nid_postfix, u"25x", -1, 2.5, 2, 0, "_WStod stops at a non-digit");
    CheckConversion<double>(_WStod_nid_postfix, u"1e400", 0, std::numeric_limits<double>::infinity(), 5, ERANGE, "_WStod overflow");
    CheckConversion<double>(_WStod_nid_postfix, u"\u30001", 0, 0.0, 0, 0, "_WStod does not skip a non-ASCII space");
    CheckConversion<float>(_WStof_nid_postfix, u" 1.5", 2, 150.0f, 4, 0, "_WStof scales by a power of ten");
    CheckConversion<long double>(_WStold_nid_postfix, u"-2.5e1", 3, -25000.0L, 6, 0, "_WStold scales by a power of ten");
}

template<typename TFloat>
static void CheckDinkumwareNoConversion(TFloat (APS5_VABI *parse)(const char16_t*, char16_t**, std::int64_t)) {
    const struct { const char16_t* text; bool negative; } cases[]{
        {u"", false}, {u" ", false}, {u"z", false}, {u"+", false}, {u"+z", false},
        {u"-", true}, {u"-z", true}, {u" \t-.", true}, {u"-i", false}, {u"-n", false},
        {u"-+1", true}, {u"--1", true}, {u"- 5", true}, {u"\t-.", true}, {u"-\u3000", true}
    };
    for (const auto& test : cases) {
        for (const std::int64_t scale : {std::int64_t{0}, std::int64_t{-10}, std::int64_t{10}, std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) {
            for (const int saved : {0, ERANGE, 97}) for (const bool nullEnd : {false, true}) {
                char16_t* end = nullptr;
                *__error_nid_postfix() = saved;
                const TFloat value = parse(test.text, nullEnd ? nullptr : &end, scale);
                Expect(value == 0 && std::signbit(value) == test.negative, "Dinkumware no-conversion zero sign");
                Expect((nullEnd || end == test.text) && *__error_nid_postfix() == saved, "Dinkumware no-conversion end and errno");
            }
        }
    }
}

static void CheckClassification() {
    int (APS5_VABI* const classes[])(std::uint32_t) = {
        iswalnum_nid_postfix, iswalpha_nid_postfix, iswcntrl_nid_postfix, iswdigit_nid_postfix,
        iswgraph_nid_postfix, iswlower_nid_postfix, iswprint_nid_postfix, iswpunct_nid_postfix,
        iswspace_nid_postfix, iswupper_nid_postfix, iswxdigit_nid_postfix, iswblank_nid_postfix,
    };
    for (std::uint64_t type = 1; type <= 12; ++type) {
        for (std::uint32_t c = 0; c < 0x300; ++c) {
            if ((_Iswctype_nid_postfix(c, type) != 0) != (classes[type - 1](c) != 0)) {
                std::fprintf(stderr, "_Iswctype(%#x, %llu) differs from its isw function\n", c, static_cast<unsigned long long>(type));
                Expect(false);
            }
        }
        Expect(_Iswctype_nid_postfix(0xffffffffu, type) == 0, "_Iswctype is false for WEOF");
    }
    Expect(_Iswctype_nid_postfix(u'A', 2) != 0 && _Iswctype_nid_postfix(u'1', 2) == 0, "_Iswctype alpha");
    Expect(_Iswctype_nid_postfix(u'7', 4) != 0 && _Iswctype_nid_postfix(u'f', 11) != 0, "_Iswctype digit and xdigit");
    Expect(_Iswctype_nid_postfix(u'\t', 12) != 0 && _Iswctype_nid_postfix(u'\n', 12) == 0, "_Iswctype blank");
    Expect(_Iswctype_nid_postfix(u'\n', 9) != 0 && _Iswctype_nid_postfix(u' ', 5) == 0, "_Iswctype space and graph");
    Expect(_Iswctype_nid_postfix(u'a', 6) != 0 && _Iswctype_nid_postfix(u'a', 10) == 0, "_Iswctype lower and upper");
    Expect(_Iswctype_nid_postfix(0x7f, 3) != 0 && _Iswctype_nid_postfix(u'!', 8) != 0, "_Iswctype cntrl and punct");
    Expect(_Iswctype_nid_postfix(0xe9, 2) == 0 && _Iswctype_nid_postfix(0x3042, 1) == 0, "_Iswctype C locale has no non-ASCII letters");
    Expect(_Iswctype_nid_postfix(u'a', 0) == 0, "_Iswctype is false for class 0");
    Expect(_Iswctype_nid_postfix(u'a', 13) == 0, "_Iswctype is false for class 13");
    Expect(Throws([] { _Iswctype_nid_postfix(u'a', 0x100); }), "_Iswctype throws for a rune mask");
}

static void CheckScan() {
    int first = 0;
    int second = 0;
    Expect(swscanf_s_nid_postfix(u"  42 -7", u"%d%d", &first, &second) == 2 && first == 42 && second == -7, "%d");
    Expect(swscanf_s_nid_postfix(u"x=0x1f", u"x=%i", &first) == 1 && first == 31, "%i with a hex prefix");
    Expect(swscanf_s_nid_postfix(u"1 2", u"%*d%d", &first) == 1 && first == 2, "suppressed %d");
    Expect(swscanf_s_nid_postfix(u"12345", u"%3d%d", &first, &second) == 2 && first == 123 && second == 45, "%d width");
    std::int64_t wide = 0;
    Expect(swscanf_s_nid_postfix(u"5000000000", u"%ld", &wide) == 1 && wide == 5000000000LL, "%ld is 64-bit");
    short half = 0;
    Expect(swscanf_s_nid_postfix(u"-2", u"%hd", &half) == 1 && half == -2, "%hd");
    unsigned value = 0;
    Expect(swscanf_s_nid_postfix(u"0xg", u"%x", &value) == 0, "%x prefix without digits is a matching failure");
    void* pointer = nullptr;
    Expect(swscanf_s_nid_postfix(u"0x7ffe12345678 5", u"%p%d", &pointer, &first) == 2 && reinterpret_cast<std::uintptr_t>(pointer) == 0x7ffe12345678u && first == 5, "%p takes no size");
    int counted = -7;
    Expect(swscanf_s_nid_postfix(u"12 34", u"%d%*n%d", &first, &second) == 2 && first == 12 && second == 34, "%*n stores nothing and takes no argument");
    Expect(swscanf_s_nid_postfix(u"12 34", u"%d%5n%d", &first, &counted, &second) == 2 && counted == 2 && second == 34, "%n ignores a width");
    Expect(swscanf_s_nid_postfix(u"12345", u"%0d%n", &first, &counted) == 1 && first == 12345 && counted == 5, "a zero width is no width");
    counted = -7;
    first = -7;
    Expect(swscanf_s_nid_postfix(u"", u"%n%d", &counted, &first) == 0 && counted == 0 && first == -7, "%n before an input failure returns 0");
    Expect(swscanf_s_nid_postfix(u"  ", u"%*n%d", &first) == 0 && first == -7, "%*n before an input failure returns 0");
    Expect(swscanf_nid_postfix(u"", u"%n%d", &counted, &first) == 0, "swscanf %n before an input failure returns 0");

    char16_t text[6] = {u'?'};
    Expect(swscanf_s_nid_postfix(u"hello world", u"%ls", text, std::size_t{6}) == 1 && std::u16string(text) == u"hello", "%ls fits");
    char16_t small[5] = {u'?', u'?', u'?', u'?', u'?'};
    Expect(swscanf_s_nid_postfix(u"hello", u"%ls", small, std::size_t{5}) == 0 && small[0] == 0, "%ls too small");
    Expect(swscanf_s_nid_postfix(u"7 hello", u"%d%ls", &first, small, std::size_t{5}) == 1 && first == 7, "%ls too small after a conversion");
    char narrow[4] = {};
    Expect(swscanf_s_nid_postfix(u"abc def", u"%s", narrow, std::size_t{4}) == 1 && std::strcmp(narrow, "abc") == 0, "%s");
    char16_t pair[3] = {u'?', u'?', u'?'};
    Expect(swscanf_s_nid_postfix(u"xyz", u"%2lc", pair, std::size_t{2}) == 1 && pair[0] == u'x' && pair[1] == u'y' && pair[2] == u'?', "%2lc");
    Expect(swscanf_s_nid_postfix(u"xyz", u"%2lc", pair, std::size_t{1}) == 1 && pair[0] == u'x', "%2lc limited by capacity");
    Expect(swscanf_s_nid_postfix(u"x", u"%2lc", pair, std::size_t{2}) == 1 && pair[0] == u'x', "%2lc short input");
    Expect(swscanf_s_nid_postfix(u"cab12", u"%l[abc]%d", text, std::size_t{6}, &first) == 2 && std::u16string(text) == u"cab" && first == 12, "%l[");
    Expect(swscanf_s_nid_postfix(u"été!", u"%l[^!]", text, std::size_t{6}) == 1 && std::u16string(text) == u"été", "%l[^");
    char16_t untouched[4] = {u'?', u'?', u'?', u'?'};
    Expect(swscanf_s_nid_postfix(u"Āb cd", u"%S", untouched, std::size_t{4}) == 0 && untouched[0] == u'?', "%S is an invalid conversion");
    Expect(swscanf_s_nid_postfix(u"7 Āb", u"%d%lS", &first, untouched, std::size_t{4}) == 1 && first == 7 && untouched[0] == u'?', "%lS stops after the earlier assignment");
    Expect(swscanf_s_nid_postfix(u"Āyz", u"%2C", untouched, std::size_t{4}) == 0 && untouched[0] == u'?', "%C is an invalid conversion");
    Expect(swscanf_s_nid_postfix(u"5 6", u"%hC%d", &first) == 0 && first == 7, "%hC stops before later conversions");
    std::int64_t count = -1;
    Expect(swscanf_s_nid_postfix(u" 12 ab", u"%d %ln", &first, &count) == 1 && count == 4, "%ln");

    double real = 0;
    Expect(swscanf_s_nid_postfix(u"1.5e3", u"%lf", &real) == 1 && real == 1500.0, "%lf");
    Expect(swscanf_s_nid_postfix(u"1e+", u"%lf", &real) == 1 && real == 1.0, "%lf converts the mantissa of an incomplete exponent");
    long double extended = 0;
    Expect(swscanf_s_nid_postfix(u"2.25", u"%Lf", &extended) == 1 && extended == 2.25L, "%Lf");
    float single = 0;
    Expect(swscanf_s_nid_postfix(u"-inf", u"%f", &single) == 1 && single == -std::numeric_limits<float>::infinity(), "%f infinity");

    Expect(swscanf_s_nid_postfix(u"a", u"b%d", &first) == 0, "literal mismatch is a matching failure");
    Expect(swscanf_s_nid_postfix(u"", u"x") == 0, "literal at end of input returns zero");
    Expect(swscanf_s_nid_postfix(u"", u"%%") == -1, "percent literal at end of input is an input failure");
    Expect(swscanf_s_nid_postfix(u"", u"%d", &first) == -1, "empty input is an input failure");
    Expect(swscanf_s_nid_postfix(u"   ", u"%ls", text, std::size_t{6}) == -1, "white space only is an input failure");
    Expect(swscanf_s_nid_postfix(u"5", u"%d%d", &first, &second) == 1, "input failure after a conversion");
    Expect(swscanf_s_nid_postfix(u"5", u"%d", static_cast<int*>(nullptr)) == -1, "null %d argument");
    Expect(swscanf_s_nid_postfix(u"5 abc", u"%d%ls", &first, static_cast<char16_t*>(nullptr), std::size_t{4}) == -1, "null %ls argument");
    Expect(swscanf_s_nid_postfix(nullptr, u"%d", &first) == -1, "null buffer");
    Expect(swscanf_s_nid_postfix(u"5", nullptr) == -1, "null format");
    Expect(swscanf_s_nid_postfix(u"abcz", u"%l[a-c]", text, std::size_t{6}) == 1 && std::u16string(text) == u"abc", "scanset range");
    first = -1;
    Expect(swscanf_s_nid_postfix(u"5", u"%Ld", &first) == 1 && first == 5, "%Ld reads an int");
    Expect(swscanf_s_nid_postfix(u"Ā", u"%s", narrow, std::size_t{4}) == 1 && std::strcmp(narrow, "\xc4\x80") == 0, "narrow %s stores UTF-8");
}

static void CheckUnsizedScan() {
    int first = 0;
    int second = 0;
    Expect(swscanf_nid_postfix(u"  42 -7", u"%d%d", &first, &second) == 2 && first == 42 && second == -7, "swscanf %d");
    Expect(swscanf_nid_postfix(u"x=0x1f", u"x=%i", &first) == 1 && first == 31, "swscanf %i with a hex prefix");
    Expect(swscanf_nid_postfix(u"1 2", u"%*d%d", &first) == 1 && first == 2, "swscanf suppressed %d");
    float scannedFloat = 0;
    double scannedDouble = 0;
    long double scannedLongDouble = 0;
    errno = 0;
    Expect(swscanf_nid_postfix(u"1e39 1e400", u"%f%lf", &scannedFloat, &scannedDouble) == 2 && scannedFloat == std::numeric_limits<float>::infinity() &&
        scannedDouble == std::numeric_limits<double>::infinity() && errno == 0, "swscanf leaves errno alone for an overflowing float and double");
    Expect(swscanf_nid_postfix(u"1e-400", u"%lf", &scannedDouble) == 1 && scannedDouble == 0 && errno == 0, "swscanf leaves errno alone for an underflowing double");
    errno = 0;
    Expect(swscanf_nid_postfix(u"1e-5000", u"%Lf", &scannedLongDouble) == 1 && scannedLongDouble == 0 && errno == ERANGE, "swscanf sets ERANGE for an out-of-range long double");
    errno = 0;
    Expect(swscanf_nid_postfix(u"1e-50", u"%e", &scannedFloat) == 1 && scannedFloat == 0 && errno == 0, "swscanf leaves errno alone for an underflowing float");
    for (const char16_t* input : {u"1.2e4932", u"-1e5000", u"1e-4952"}) {
        errno = 0;
        Expect(swscanf_nid_postfix(input, u"%f", &scannedFloat) == 1 && errno == ERANGE, "swscanf sets ERANGE for a float out of long double range");
        errno = 0;
        Expect(swscanf_nid_postfix(input, u"%lf", &scannedDouble) == 1 && errno == ERANGE, "swscanf sets ERANGE for a double out of long double range");
    }
    errno = 0;
    Expect(swscanf_nid_postfix(u"1e4932", u"%lf", &scannedDouble) == 1 && std::isinf(scannedDouble) && errno == 0, "swscanf leaves errno alone inside long double range");
    errno = 0;
    Expect(swscanf_nid_postfix(u"3e-4951", u"%Lf", &scannedLongDouble) == 1 && scannedLongDouble > 0 && errno == 0, "swscanf leaves errno alone for a subnormal long double");
    errno = 0;
    Expect(swscanf_nid_postfix(u"0x1p16384", u"%Lf", &scannedLongDouble) == 1 && std::isinf(scannedLongDouble) && errno == 0, "swscanf leaves errno alone for a hexadecimal overflow");
    for (const char16_t* input : {u"-nan", u"-NaN(123)"}) {
        Expect(swscanf_nid_postfix(input, u"%f", &scannedFloat) == 1 && std::bit_cast<std::uint32_t>(scannedFloat) == 0x7fc00000u, "swscanf stores the default float NaN");
        Expect(swscanf_nid_postfix(input, u"%lf", &scannedDouble) == 1 && std::bit_cast<std::uint64_t>(scannedDouble) == 0x7ff8000000000000ull, "swscanf stores the default double NaN");
    }
    unsigned char tenBytes[16];
    std::memset(tenBytes, 0x55, sizeof(tenBytes));
    Expect(swscanf_nid_postfix(u"inf", u"%Lf", tenBytes) == 1 && tenBytes[9] == 0x7f && tenBytes[10] == 0x55 && tenBytes[15] == 0x55, "swscanf stores ten bytes for %Lf");
    Expect(swscanf_nid_postfix(u"12345", u"%3d%d", &first, &second) == 2 && first == 123 && second == 45, "swscanf %d width");
    std::int64_t wide = 0;
    Expect(swscanf_nid_postfix(u"5000000000", u"%ld", &wide) == 1 && wide == 5000000000LL, "swscanf %ld is 64-bit");
    short half = 0;
    Expect(swscanf_nid_postfix(u"-2", u"%hd", &half) == 1 && half == -2, "swscanf %hd");
    unsigned value = 0;
    Expect(swscanf_nid_postfix(u"0xg", u"%x", &value) == 0, "swscanf %x prefix without digits is a matching failure");
    void* pointer = nullptr;
    Expect(swscanf_nid_postfix(u"0x7ffe12345678", u"%p", &pointer) == 1 && reinterpret_cast<std::uintptr_t>(pointer) == 0x7ffe12345678u, "swscanf %p stores a whole pointer");
    Expect(swscanf_nid_postfix(u"DeadBeef", u"%p", &pointer) == 1 && reinterpret_cast<std::uintptr_t>(pointer) == 0xdeadbeefu, "swscanf %p without a prefix");
    void* next = nullptr;
    Expect(swscanf_nid_postfix(u"0x1234", u"%4p%p", &pointer, &next) == 2 && reinterpret_cast<std::uintptr_t>(pointer) == 0x12u && reinterpret_cast<std::uintptr_t>(next) == 0x34u, "swscanf %p width");
    Expect(swscanf_nid_postfix(u"0xff 7", u"%*p%d", &first) == 1 && first == 7, "swscanf suppressed %p");
    Expect(swscanf_nid_postfix(u"-1", u"%p", &pointer) == 1 && reinterpret_cast<std::uintptr_t>(pointer) == std::numeric_limits<std::uintptr_t>::max(), "swscanf %p converts as unsigned");
    Expect(swscanf_nid_postfix(u"0xg", u"%p", &pointer) == 0, "swscanf %p prefix without digits is a matching failure");

    char16_t text[16] = {u'?'};
    Expect(swscanf_nid_postfix(u"hello world", u"%ls", text) == 1 && std::u16string(text) == u"hello", "swscanf %ls");
    Expect(swscanf_nid_postfix(u"7 a-long-word", u"%d%ls", &first, text) == 2 && first == 7 && std::u16string(text) == u"a-long-word", "swscanf %ls takes no size");
    Expect(swscanf_nid_postfix(u"hello world", u"%3ls", text) == 1 && std::u16string(text) == u"hel", "swscanf %ls width");
    char narrow[8] = {};
    Expect(swscanf_nid_postfix(u"abc def", u"%s%d", narrow, &first) == 1 && std::strcmp(narrow, "abc") == 0, "swscanf %s");
    char16_t pair[3] = {u'?', u'?', u'?'};
    Expect(swscanf_nid_postfix(u"xyz", u"%2lc", pair) == 1 && pair[0] == u'x' && pair[1] == u'y' && pair[2] == u'?', "swscanf %2lc");
    Expect(swscanf_nid_postfix(u"x", u"%2lc", pair) == 1 && pair[0] == u'x', "swscanf %2lc short input");
    char letter = 0;
    Expect(swscanf_nid_postfix(u" q7", u"%c%d", &letter, &first) == 1 && letter == ' ', "swscanf %c does not skip white space");
    Expect(swscanf_nid_postfix(u"cab12", u"%l[abc]%d", text, &first) == 2 && std::u16string(text) == u"cab" && first == 12, "swscanf %l[");
    Expect(swscanf_nid_postfix(u"été!", u"%l[^!]", text) == 1 && std::u16string(text) == u"été", "swscanf %l[^");
    char16_t untouched[4] = {u'?', u'?', u'?', u'?'};
    Expect(swscanf_nid_postfix(u"Āb cd", u"%S", untouched) == 0 && untouched[0] == u'?', "swscanf %S is an invalid conversion");
    Expect(swscanf_nid_postfix(u"7 Āb 9", u"%d%3S%d", &first, untouched, &second) == 1 && first == 7 && untouched[0] == u'?', "swscanf stops at %S");
    Expect(swscanf_nid_postfix(u"Āyz", u"%lC", untouched) == 0 && untouched[0] == u'?', "swscanf %lC is an invalid conversion");
    Expect(swscanf_nid_postfix(u"abc", u"%*S") == 0, "swscanf suppressed %S is an invalid conversion");
    Expect(swscanf_nid_postfix(u"key=val", u"%[^=]=%s", narrow, narrow + 4) == 2 && std::strcmp(narrow, "key") == 0 && std::strcmp(narrow + 4, "val") == 0, "swscanf %[^");
    std::int64_t count = -1;
    Expect(swscanf_nid_postfix(u" 12 ab", u"%d %ln", &first, &count) == 1 && count == 4, "swscanf %ln");
    int shortCount = -1;
    Expect(swscanf_nid_postfix(u"abc", u"%*ls%n", &shortCount) == 0 && shortCount == 3, "swscanf %n after a suppressed conversion");
    Expect(swscanf_nid_postfix(u"50% off", u"%d%% off", &first) == 1 && first == 50, "swscanf %%");
    Expect(swscanf_nid_postfix(u"50 off", u"%d%%", &first) == 1, "swscanf %% mismatch is a matching failure");

    double real = 0;
    Expect(swscanf_nid_postfix(u"1.5e3", u"%lf", &real) == 1 && real == 1500.0, "swscanf %lf");
    Expect(swscanf_nid_postfix(u"1e+", u"%lf", &real) == 1 && real == 1.0, "swscanf %lf converts the mantissa of an incomplete exponent");
    long double extended = 0;
    Expect(swscanf_nid_postfix(u"2.25", u"%Lf", &extended) == 1 && extended == 2.25L, "swscanf %Lf");
    float single = 0;
    Expect(swscanf_nid_postfix(u"-inf", u"%f", &single) == 1 && single == -std::numeric_limits<float>::infinity(), "swscanf %f infinity");

    Expect(swscanf_nid_postfix(u"a", u"b%d", &first) == 0, "swscanf literal mismatch is a matching failure");
    Expect(swscanf_nid_postfix(u"", u"x") == 0, "swscanf literal at end of input returns zero");
    Expect(swscanf_nid_postfix(u"", u"%%") == -1, "swscanf percent literal at end of input is an input failure");
    Expect(swscanf_nid_postfix(u"3 x", u"%d%d", &first, &second) == 1 && first == 3, "swscanf partial match");
    Expect(swscanf_nid_postfix(u"", u"%d", &first) == -1, "swscanf empty input is an input failure");
    Expect(swscanf_nid_postfix(u"", u"%ls", text) == -1, "swscanf empty input before %ls");
    Expect(swscanf_nid_postfix(u"   ", u"%ls", text) == -1, "swscanf white space only is an input failure");
    Expect(swscanf_nid_postfix(u"5", u"%d%d", &first, &second) == 1, "swscanf input failure after a conversion");
    Expect(swscanf_nid_postfix(nullptr, u"%d", &first) == -1, "swscanf null buffer");
    Expect(swscanf_nid_postfix(u"5", nullptr) == -1, "swscanf null format");
    Expect(swscanf_nid_postfix(u"abcz", u"%l[a-c]", text) == 1 && std::u16string(text) == u"abc", "swscanf scanset range");
    const struct {
        const char16_t* input;
        const char16_t* format;
        int result;
        const char* stored;
    } ranges[] = {
        {u"bca-", u"%[a-c]", 1, "bca"}, {u"za", u"%[z-a]", 0, ""}, {u"-ab", u"%[-a]", 1, "-a"}, {u"a-b", u"%[a-]", 1, "a-"},
        {u"dcb", u"%[^a-c]", 1, "d"}, {u"b", u"%[a-a-c]", 0, ""}, {u"a-c", u"%[a-a-c]", 1, "a-c"}, {u"-d", u"%[a-c-e]", 1, "-"},
        {u"-x", u"%[c-a-z]", 1, "-"}, {u"-", u"%[a--]", 0, ""}, {u"+", u"%[*--]", 1, "+"}, {u"b]", u"%[]-c]", 1, "b]"},
        {u"-]a", u"%[^]-]", 0, ""}, {u"\u00e9a", u"%[a-\u00e9]", 1, "\xc3\xa9" "a"}, {u"ab cd", u"%2hs", 1, "ab"}, {u"ab", u"%lls", 1, "ab"},
        {u"ab", u"%hhs", 1, "ab"}, {u"ab", u"%h[a-z]", 1, "ab"}, {u"ab", u"%y", 0, ""}, {u"ab", u"%5y", 0, ""}, {u"ab", u"%hls", 0, ""},
    };
    for (const auto& item : ranges) {
        char bytes[8] = {'!', '!', '!', '!', '!', '!', '!', '!'};
        Expect(swscanf_nid_postfix(item.input, item.format, bytes) == item.result, "swscanf scanset or length result");
        Expect(item.result == 0 ? bytes[0] == '!' : std::strcmp(bytes, item.stored) == 0, "swscanf scanset or length bytes");
    }
    char letters[4] = {'!', '!', '!', '!'};
    Expect(swscanf_nid_postfix(u"abc", u"%2hc", letters) == 1 && letters[0] == 'a' && letters[1] == 'b' && letters[2] == '!', "swscanf %hc");
    Expect(swscanf_nid_postfix(u"ab", u"%ls%y", text) == 1 && std::u16string(text) == u"ab", "swscanf stops at an unknown conversion");
    first = -1;
    Expect(swscanf_nid_postfix(u"5", u"%Ld", &first) == 1 && first == 5, "swscanf %Ld reads an int");
    pointer = nullptr;
    Expect(swscanf_nid_postfix(u"1f", u"%lp", &pointer) == 1 && pointer == reinterpret_cast<void*>(0x1f), "swscanf %lp reads a pointer");
    pointer = nullptr;
    Expect(swscanf_nid_postfix(u"1f", u"%hp", &pointer) == 1 && pointer == reinterpret_cast<void*>(0x1f), "swscanf %hp reads a pointer");
    char unset[4] = {'!', '!', '!', '!'};
    Expect(swscanf_nid_postfix(u"ab", u"%[^]", unset) == 0 && swscanf_nid_postfix(u"ab", u"%[ab", unset) == 0, "swscanf unterminated set");
    Expect(swscanf_nid_postfix(u"", u"%[^]", unset) == 0 && unset[0] == '!', "swscanf unterminated set at end of input");
    Expect(swscanf_nid_postfix(u"Ā", u"%s", narrow) == 1 && std::strcmp(narrow, "\xc4\x80") == 0, "swscanf narrow %s stores UTF-8");
}

static void CheckShortCharacterFields() {
    const char16_t* inputs[]{u"", u"a", u"ab", u"abc", u"abcd"};
    for (const char16_t* input : inputs) {
        const std::size_t length = std::u16string(input).size();
        const std::size_t consumed = length < 3 ? length : 3;
        for (bool sized : {false, true}) {
            char16_t output[]{0xa55a, 0xa55a, 0xa55a, 0xa55a, 0xa55a};
            int count = -1;
            const int result = sized ? swscanf_s_nid_postfix(input, u"%3lc%n", output + 1, 3u, &count)
                                     : swscanf_nid_postfix(input, u"%3lc%n", output + 1, &count);
            Expect(result == (length == 0 ? EOF : 1), "short wide character field assignment count");
            Expect(count == (length == 0 ? -1 : static_cast<int>(consumed)), "short wide character field consumed count");
            for (std::size_t index = 0; index < 5; ++index) {
                const char16_t expected = index > 0 && index <= consumed ? input[index - 1] : 0xa55a;
                Expect(output[index] == expected, "short wide character field exact write extent without terminator");
            }
            count = -1;
            const int suppressed = sized ? swscanf_s_nid_postfix(input, u"%*3lc%n", &count)
                                         : swscanf_nid_postfix(input, u"%*3lc%n", &count);
            Expect(suppressed == (length == 0 ? EOF : 0), "suppressed short character field assignment count");
            Expect(count == (length == 0 ? -1 : static_cast<int>(consumed)), "suppressed short character field continues to count");
        }
    }
    char16_t output[]{0xa55a, 0xa55a, 0xa55a};
    int value = -1, count = -1;
    Expect(swscanf_nid_postfix(u"5 a", u"%d %3lc%n", &value, output, &count) == 2 && value == 5 && count == 3,
            "short character field after another assignment");
    Expect(output[0] == u'a' && output[1] == 0xa55a && output[2] == 0xa55a, "short field after assignment does not terminate");
    char bytes[]{'?', '?', '?', '?'};
    count = -1;
    Expect(swscanf_nid_postfix(u"ab", u"%3c%n", bytes + 1, &count) == 1 && count == 2,
            "short narrow destination follows FreeBSD character conversion");
    Expect(bytes[0] == '?' && bytes[1] == 'a' && bytes[2] == 'b' && bytes[3] == '?', "short narrow destination bounds");
}

static void CheckZeroCapacityCharacterFields() {
    for (const char16_t* input : {u"a", u"ab", u"abc", u"abcd"}) {
        char16_t output[]{0xa55a, 0xa55a, 0xa55a};
        int count = -1, result = -1;
        *__error_nid_postfix() = 0;
        Expect(!Throws([&] { result = swscanf_s_nid_postfix(input, u"%3lc%n", output + 1, 0u, &count); }),
                "zero-capacity wide character field does not throw");
        const int error = *__error_nid_postfix();
        Expect(result == 0, "zero-capacity wide character field returns no assignment");
        Expect(count == -1, "zero-capacity wide character field does not reach percent n");
        Expect(error == 0, "zero-capacity wide character field preserves errno");
        for (const char16_t unit : output) Expect(unit == 0xa55a, "zero-capacity wide character field leaves output untouched");
    }
    char16_t output[]{0xa55a, 0xa55a, 0xa55a};
    int value = -1, count = -1, result = -1;
    Expect(!Throws([&] { result = swscanf_s_nid_postfix(u"7 a", u"%d %3lc%n", &value, output + 1, 0u, &count); }),
            "zero-capacity field after an assignment does not throw");
    Expect(result == 1 && value == 7 && count == -1, "zero-capacity field retains only prior assignments");
    for (const char16_t unit : output) Expect(unit == 0xa55a, "zero-capacity field after an assignment leaves output untouched");
}

static void CheckSecureCharacterCapacities() {
    const char16_t* inputs[]{u"", u"a", u"ab", u"abc", u"abcd"};
    for (const char16_t* input : inputs) {
        const std::size_t length = std::u16string(input).size();
        for (unsigned int capacity : {1u, 2u, 3u, 4u}) {
            const std::size_t limit = capacity < 3 ? capacity : 3;
            const std::size_t consumed = length < limit ? length : limit;
            char16_t output[]{0xa55a, 0xa55a, 0xa55a, 0xa55a, 0xa55a, 0xa55a};
            char16_t next = 0xa55a;
            int count = -1;
            int result = 0;
            *__error_nid_postfix() = 0;
            if (Throws([&] { result = swscanf_s_nid_postfix(input, u"%3lc%n%lc", output + 1, capacity, &count, &next, 1u); })) {
                Expect(false, "positive secure character capacity does not throw");
                continue;
            }
            const int expected = length == 0 ? EOF : length > consumed ? 2 : 1;
            Expect(result == expected, "positive secure character capacity assignment count");
            Expect(count == (length == 0 ? -1 : static_cast<int>(consumed)), "positive secure character capacity consumed count");
            Expect(next == (length > consumed ? input[consumed] : 0xa55a), "the next conversion sees the capacity-limited remainder");
            Expect(*__error_nid_postfix() == 0, "positive secure character capacity preserves errno");
            for (std::size_t index = 0; index < 6; ++index) {
                const char16_t expectedUnit = index > 0 && index <= consumed ? input[index - 1] : 0xa55a;
                Expect(output[index] == expectedUnit, "positive secure character capacity exact writes without terminator");
            }
        }
    }
    char16_t output[]{0xa55a, 0xa55a, 0xa55a, 0xa55a};
    char16_t next = 0xa55a;
    int value = -1, count = -1;
    Expect(swscanf_s_nid_postfix(u"7 abcZ", u"%d %3lc%n%lc", &value, output + 1, 2u, &count, &next, 1u) == 3
            && value == 7 && count == 4 && next == u'c', "positive secure character capacity after another assignment");
    Expect(output[0] == 0xa55a && output[1] == u'a' && output[2] == u'b' && output[3] == 0xa55a,
            "positive secure character capacity after another assignment preserves bounds");
    count = -1;
    next = 0xa55a;
    Expect(swscanf_s_nid_postfix(u"abcd", u"%*3lc%n%lc", &count, &next, 1u) == 1 && count == 3 && next == u'd',
            "suppressed character fields keep width and consume no capacity argument");
}

int main() {
    CheckZeroCapacityCharacterFields();
    CheckSecureCharacterCapacities();
    CheckShortCharacterFields();
    CheckStoul();
    CheckDinkumwareConversions();
    CheckDinkumwareNoConversion(_WStod_nid_postfix);
    CheckDinkumwareNoConversion(_WStof_nid_postfix);
    CheckDinkumwareNoConversion(_WStold_nid_postfix);
    CheckClassification();
    CheckScan();
    CheckUnsizedScan();
    return Failures() == 0 ? 0 : 1;
}
