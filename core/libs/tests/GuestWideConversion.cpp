#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

static int failures = 0;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        ++failures;
    }
}

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
        ++failures;
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
    Require(_WStoul_nid_postfix(u"9", nullptr, 10) == 9, "_WStoul accepts a null end pointer");
    Require(Throws([] { _WStoul_nid_postfix(u"1", nullptr, 1); }), "_WStoul throws for base 1");
    Require(Throws([] { _WStoul_nid_postfix(u"1", nullptr, 37); }), "_WStoul throws for base 37");
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
                ++failures;
            }
        }
        Require(_Iswctype_nid_postfix(0xffffffffu, type) == 0, "_Iswctype is false for WEOF");
    }
    Require(_Iswctype_nid_postfix(u'A', 2) != 0 && _Iswctype_nid_postfix(u'1', 2) == 0, "_Iswctype alpha");
    Require(_Iswctype_nid_postfix(u'7', 4) != 0 && _Iswctype_nid_postfix(u'f', 11) != 0, "_Iswctype digit and xdigit");
    Require(_Iswctype_nid_postfix(u'\t', 12) != 0 && _Iswctype_nid_postfix(u'\n', 12) == 0, "_Iswctype blank");
    Require(_Iswctype_nid_postfix(u'\n', 9) != 0 && _Iswctype_nid_postfix(u' ', 5) == 0, "_Iswctype space and graph");
    Require(_Iswctype_nid_postfix(u'a', 6) != 0 && _Iswctype_nid_postfix(u'a', 10) == 0, "_Iswctype lower and upper");
    Require(_Iswctype_nid_postfix(0x7f, 3) != 0 && _Iswctype_nid_postfix(u'!', 8) != 0, "_Iswctype cntrl and punct");
    Require(_Iswctype_nid_postfix(0xe9, 2) == 0 && _Iswctype_nid_postfix(0x3042, 1) == 0, "_Iswctype C locale has no non-ASCII letters");
    Require(Throws([] { _Iswctype_nid_postfix(u'a', 0); }), "_Iswctype throws for class 0");
    Require(Throws([] { _Iswctype_nid_postfix(u'a', 13); }), "_Iswctype throws for class 13");
    Require(Throws([] { _Iswctype_nid_postfix(u'a', 0x100); }), "_Iswctype throws for a rune mask");
}

static void CheckScan() {
    int first = 0;
    int second = 0;
    Require(swscanf_s_nid_postfix(u"  42 -7", u"%d%d", &first, &second) == 2 && first == 42 && second == -7, "%d");
    Require(swscanf_s_nid_postfix(u"x=0x1f", u"x=%i", &first) == 1 && first == 31, "%i with a hex prefix");
    Require(swscanf_s_nid_postfix(u"1 2", u"%*d%d", &first) == 1 && first == 2, "suppressed %d");
    Require(swscanf_s_nid_postfix(u"12345", u"%3d%d", &first, &second) == 2 && first == 123 && second == 45, "%d width");
    std::int64_t wide = 0;
    Require(swscanf_s_nid_postfix(u"5000000000", u"%ld", &wide) == 1 && wide == 5000000000LL, "%ld is 64-bit");
    short half = 0;
    Require(swscanf_s_nid_postfix(u"-2", u"%hd", &half) == 1 && half == -2, "%hd");
    unsigned value = 0;
    Require(swscanf_s_nid_postfix(u"0xg", u"%x", &value) == 0, "%x prefix without digits is a matching failure");
    void* pointer = nullptr;
    Require(swscanf_s_nid_postfix(u"0x7ffe12345678 5", u"%p%d", &pointer, &first) == 2 && reinterpret_cast<std::uintptr_t>(pointer) == 0x7ffe12345678u && first == 5, "%p takes no size");

    char16_t text[6] = {u'?'};
    Require(swscanf_s_nid_postfix(u"hello world", u"%ls", text, std::size_t{6}) == 1 && std::u16string(text) == u"hello", "%ls fits");
    char16_t small[5] = {u'?', u'?', u'?', u'?', u'?'};
    Require(swscanf_s_nid_postfix(u"hello", u"%ls", small, std::size_t{5}) == 0 && small[0] == 0, "%ls too small");
    Require(swscanf_s_nid_postfix(u"7 hello", u"%d%ls", &first, small, std::size_t{5}) == 1 && first == 7, "%ls too small after a conversion");
    char narrow[4] = {};
    Require(swscanf_s_nid_postfix(u"abc def", u"%s", narrow, std::size_t{4}) == 1 && std::strcmp(narrow, "abc") == 0, "%s");
    char16_t pair[3] = {u'?', u'?', u'?'};
    Require(swscanf_s_nid_postfix(u"xyz", u"%2lc", pair, std::size_t{2}) == 1 && pair[0] == u'x' && pair[1] == u'y' && pair[2] == u'?', "%2lc");
    Require(swscanf_s_nid_postfix(u"xyz", u"%2lc", pair, std::size_t{1}) == 0, "%2lc too small");
    Require(swscanf_s_nid_postfix(u"x", u"%2lc", pair, std::size_t{2}) == 0, "%2lc short input");
    Require(swscanf_s_nid_postfix(u"cab12", u"%l[abc]%d", text, std::size_t{6}, &first) == 2 && std::u16string(text) == u"cab" && first == 12, "%l[");
    Require(swscanf_s_nid_postfix(u"été!", u"%l[^!]", text, std::size_t{6}) == 1 && std::u16string(text) == u"été", "%l[^");
    char16_t untouched[4] = {u'?', u'?', u'?', u'?'};
    Require(swscanf_s_nid_postfix(u"Āb cd", u"%S", untouched, std::size_t{4}) == 0 && untouched[0] == u'?', "%S is an invalid conversion");
    Require(swscanf_s_nid_postfix(u"7 Āb", u"%d%lS", &first, untouched, std::size_t{4}) == 1 && first == 7 && untouched[0] == u'?', "%lS stops after the earlier assignment");
    Require(swscanf_s_nid_postfix(u"Āyz", u"%2C", untouched, std::size_t{4}) == 0 && untouched[0] == u'?', "%C is an invalid conversion");
    Require(swscanf_s_nid_postfix(u"5 6", u"%hC%d", &first) == 0 && first == 7, "%hC stops before later conversions");
    std::int64_t count = -1;
    Require(swscanf_s_nid_postfix(u" 12 ab", u"%d %ln", &first, &count) == 1 && count == 4, "%ln");

    double real = 0;
    Require(swscanf_s_nid_postfix(u"1.5e3", u"%lf", &real) == 1 && real == 1500.0, "%lf");
    Require(swscanf_s_nid_postfix(u"1e+", u"%lf", &real) == 1 && real == 1.0, "%lf converts the mantissa of an incomplete exponent");
    long double extended = 0;
    Require(swscanf_s_nid_postfix(u"2.25", u"%Lf", &extended) == 1 && extended == 2.25L, "%Lf");
    float single = 0;
    Require(swscanf_s_nid_postfix(u"-inf", u"%f", &single) == 1 && single == -std::numeric_limits<float>::infinity(), "%f infinity");

    Require(swscanf_s_nid_postfix(u"a", u"b%d", &first) == 0, "literal mismatch is a matching failure");
    Require(swscanf_s_nid_postfix(u"", u"x") == 0, "literal at end of input returns zero");
    Require(swscanf_s_nid_postfix(u"", u"%%") == -1, "percent literal at end of input is an input failure");
    Require(swscanf_s_nid_postfix(u"", u"%d", &first) == -1, "empty input is an input failure");
    Require(swscanf_s_nid_postfix(u"   ", u"%ls", text, std::size_t{6}) == -1, "white space only is an input failure");
    Require(swscanf_s_nid_postfix(u"5", u"%d%d", &first, &second) == 1, "input failure after a conversion");
    Require(swscanf_s_nid_postfix(u"5", u"%d", static_cast<int*>(nullptr)) == -1, "null %d argument");
    Require(swscanf_s_nid_postfix(u"5 abc", u"%d%ls", &first, static_cast<char16_t*>(nullptr), std::size_t{4}) == -1, "null %ls argument");
    Require(swscanf_s_nid_postfix(nullptr, u"%d", &first) == -1, "null buffer");
    Require(swscanf_s_nid_postfix(u"5", nullptr) == -1, "null format");
    Require(swscanf_s_nid_postfix(u"abcz", u"%l[a-c]", text, std::size_t{6}) == 1 && std::u16string(text) == u"abc", "scanset range");
    first = -1;
    Require(swscanf_s_nid_postfix(u"5", u"%Ld", &first) == 1 && first == 5, "%Ld reads an int");
    Require(swscanf_s_nid_postfix(u"Ā", u"%s", narrow, std::size_t{4}) == 1 && std::strcmp(narrow, "\xc4\x80") == 0, "narrow %s stores UTF-8");
}

static void CheckUnsizedScan() {
    int first = 0;
    int second = 0;
    Require(swscanf_nid_postfix(u"  42 -7", u"%d%d", &first, &second) == 2 && first == 42 && second == -7, "swscanf %d");
    Require(swscanf_nid_postfix(u"x=0x1f", u"x=%i", &first) == 1 && first == 31, "swscanf %i with a hex prefix");
    Require(swscanf_nid_postfix(u"1 2", u"%*d%d", &first) == 1 && first == 2, "swscanf suppressed %d");
    float scannedFloat = 0;
    double scannedDouble = 0;
    long double scannedLongDouble = 0;
    errno = 0;
    Require(swscanf_nid_postfix(u"1e39 1e400", u"%f%lf", &scannedFloat, &scannedDouble) == 2 && scannedFloat == std::numeric_limits<float>::infinity() &&
        scannedDouble == std::numeric_limits<double>::infinity() && errno == 0, "swscanf leaves errno alone for an overflowing float and double");
    Require(swscanf_nid_postfix(u"1e-400", u"%lf", &scannedDouble) == 1 && scannedDouble == 0 && errno == 0, "swscanf leaves errno alone for an underflowing double");
    errno = 0;
    Require(swscanf_nid_postfix(u"1e-5000", u"%Lf", &scannedLongDouble) == 1 && scannedLongDouble == 0 && errno == ERANGE, "swscanf sets ERANGE for an out-of-range long double");
    errno = 0;
    Require(swscanf_nid_postfix(u"1e-50", u"%e", &scannedFloat) == 1 && scannedFloat == 0 && errno == 0, "swscanf leaves errno alone for an underflowing float");
    Require(swscanf_nid_postfix(u"12345", u"%3d%d", &first, &second) == 2 && first == 123 && second == 45, "swscanf %d width");
    std::int64_t wide = 0;
    Require(swscanf_nid_postfix(u"5000000000", u"%ld", &wide) == 1 && wide == 5000000000LL, "swscanf %ld is 64-bit");
    short half = 0;
    Require(swscanf_nid_postfix(u"-2", u"%hd", &half) == 1 && half == -2, "swscanf %hd");
    unsigned value = 0;
    Require(swscanf_nid_postfix(u"0xg", u"%x", &value) == 0, "swscanf %x prefix without digits is a matching failure");
    void* pointer = nullptr;
    Require(swscanf_nid_postfix(u"0x7ffe12345678", u"%p", &pointer) == 1 && reinterpret_cast<std::uintptr_t>(pointer) == 0x7ffe12345678u, "swscanf %p stores a whole pointer");
    Require(swscanf_nid_postfix(u"DeadBeef", u"%p", &pointer) == 1 && reinterpret_cast<std::uintptr_t>(pointer) == 0xdeadbeefu, "swscanf %p without a prefix");
    void* next = nullptr;
    Require(swscanf_nid_postfix(u"0x1234", u"%4p%p", &pointer, &next) == 2 && reinterpret_cast<std::uintptr_t>(pointer) == 0x12u && reinterpret_cast<std::uintptr_t>(next) == 0x34u, "swscanf %p width");
    Require(swscanf_nid_postfix(u"0xff 7", u"%*p%d", &first) == 1 && first == 7, "swscanf suppressed %p");
    Require(swscanf_nid_postfix(u"-1", u"%p", &pointer) == 1 && reinterpret_cast<std::uintptr_t>(pointer) == std::numeric_limits<std::uintptr_t>::max(), "swscanf %p converts as unsigned");
    Require(swscanf_nid_postfix(u"0xg", u"%p", &pointer) == 0, "swscanf %p prefix without digits is a matching failure");

    char16_t text[16] = {u'?'};
    Require(swscanf_nid_postfix(u"hello world", u"%ls", text) == 1 && std::u16string(text) == u"hello", "swscanf %ls");
    Require(swscanf_nid_postfix(u"7 a-long-word", u"%d%ls", &first, text) == 2 && first == 7 && std::u16string(text) == u"a-long-word", "swscanf %ls takes no size");
    Require(swscanf_nid_postfix(u"hello world", u"%3ls", text) == 1 && std::u16string(text) == u"hel", "swscanf %ls width");
    char narrow[8] = {};
    Require(swscanf_nid_postfix(u"abc def", u"%s%d", narrow, &first) == 1 && std::strcmp(narrow, "abc") == 0, "swscanf %s");
    char16_t pair[3] = {u'?', u'?', u'?'};
    Require(swscanf_nid_postfix(u"xyz", u"%2lc", pair) == 1 && pair[0] == u'x' && pair[1] == u'y' && pair[2] == u'?', "swscanf %2lc");
    Require(swscanf_nid_postfix(u"x", u"%2lc", pair) == 0, "swscanf %2lc short input");
    char letter = 0;
    Require(swscanf_nid_postfix(u" q7", u"%c%d", &letter, &first) == 1 && letter == ' ', "swscanf %c does not skip white space");
    Require(swscanf_nid_postfix(u"cab12", u"%l[abc]%d", text, &first) == 2 && std::u16string(text) == u"cab" && first == 12, "swscanf %l[");
    Require(swscanf_nid_postfix(u"été!", u"%l[^!]", text) == 1 && std::u16string(text) == u"été", "swscanf %l[^");
    char16_t untouched[4] = {u'?', u'?', u'?', u'?'};
    Require(swscanf_nid_postfix(u"Āb cd", u"%S", untouched) == 0 && untouched[0] == u'?', "swscanf %S is an invalid conversion");
    Require(swscanf_nid_postfix(u"7 Āb 9", u"%d%3S%d", &first, untouched, &second) == 1 && first == 7 && untouched[0] == u'?', "swscanf stops at %S");
    Require(swscanf_nid_postfix(u"Āyz", u"%lC", untouched) == 0 && untouched[0] == u'?', "swscanf %lC is an invalid conversion");
    Require(swscanf_nid_postfix(u"abc", u"%*S") == 0, "swscanf suppressed %S is an invalid conversion");
    Require(swscanf_nid_postfix(u"key=val", u"%[^=]=%s", narrow, narrow + 4) == 2 && std::strcmp(narrow, "key") == 0 && std::strcmp(narrow + 4, "val") == 0, "swscanf %[^");
    std::int64_t count = -1;
    Require(swscanf_nid_postfix(u" 12 ab", u"%d %ln", &first, &count) == 1 && count == 4, "swscanf %ln");
    int shortCount = -1;
    Require(swscanf_nid_postfix(u"abc", u"%*ls%n", &shortCount) == 0 && shortCount == 3, "swscanf %n after a suppressed conversion");
    Require(swscanf_nid_postfix(u"50% off", u"%d%% off", &first) == 1 && first == 50, "swscanf %%");
    Require(swscanf_nid_postfix(u"50 off", u"%d%%", &first) == 1, "swscanf %% mismatch is a matching failure");

    double real = 0;
    Require(swscanf_nid_postfix(u"1.5e3", u"%lf", &real) == 1 && real == 1500.0, "swscanf %lf");
    Require(swscanf_nid_postfix(u"1e+", u"%lf", &real) == 1 && real == 1.0, "swscanf %lf converts the mantissa of an incomplete exponent");
    long double extended = 0;
    Require(swscanf_nid_postfix(u"2.25", u"%Lf", &extended) == 1 && extended == 2.25L, "swscanf %Lf");
    float single = 0;
    Require(swscanf_nid_postfix(u"-inf", u"%f", &single) == 1 && single == -std::numeric_limits<float>::infinity(), "swscanf %f infinity");

    Require(swscanf_nid_postfix(u"a", u"b%d", &first) == 0, "swscanf literal mismatch is a matching failure");
    Require(swscanf_nid_postfix(u"", u"x") == 0, "swscanf literal at end of input returns zero");
    Require(swscanf_nid_postfix(u"", u"%%") == -1, "swscanf percent literal at end of input is an input failure");
    Require(swscanf_nid_postfix(u"3 x", u"%d%d", &first, &second) == 1 && first == 3, "swscanf partial match");
    Require(swscanf_nid_postfix(u"", u"%d", &first) == -1, "swscanf empty input is an input failure");
    Require(swscanf_nid_postfix(u"", u"%ls", text) == -1, "swscanf empty input before %ls");
    Require(swscanf_nid_postfix(u"   ", u"%ls", text) == -1, "swscanf white space only is an input failure");
    Require(swscanf_nid_postfix(u"5", u"%d%d", &first, &second) == 1, "swscanf input failure after a conversion");
    Require(swscanf_nid_postfix(nullptr, u"%d", &first) == -1, "swscanf null buffer");
    Require(swscanf_nid_postfix(u"5", nullptr) == -1, "swscanf null format");
    Require(swscanf_nid_postfix(u"abcz", u"%l[a-c]", text) == 1 && std::u16string(text) == u"abc", "swscanf scanset range");
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
        Require(swscanf_nid_postfix(item.input, item.format, bytes) == item.result, "swscanf scanset or length result");
        Require(item.result == 0 ? bytes[0] == '!' : std::strcmp(bytes, item.stored) == 0, "swscanf scanset or length bytes");
    }
    char letters[4] = {'!', '!', '!', '!'};
    Require(swscanf_nid_postfix(u"abc", u"%2hc", letters) == 1 && letters[0] == 'a' && letters[1] == 'b' && letters[2] == '!', "swscanf %hc");
    Require(swscanf_nid_postfix(u"ab", u"%ls%y", text) == 1 && std::u16string(text) == u"ab", "swscanf stops at an unknown conversion");
    first = -1;
    Require(swscanf_nid_postfix(u"5", u"%Ld", &first) == 1 && first == 5, "swscanf %Ld reads an int");
    pointer = nullptr;
    Require(swscanf_nid_postfix(u"1f", u"%lp", &pointer) == 1 && pointer == reinterpret_cast<void*>(0x1f), "swscanf %lp reads a pointer");
    pointer = nullptr;
    Require(swscanf_nid_postfix(u"1f", u"%hp", &pointer) == 1 && pointer == reinterpret_cast<void*>(0x1f), "swscanf %hp reads a pointer");
    char unset[4] = {'!', '!', '!', '!'};
    Require(swscanf_nid_postfix(u"ab", u"%[^]", unset) == 0 && swscanf_nid_postfix(u"ab", u"%[ab", unset) == 0, "swscanf unterminated set");
    Require(swscanf_nid_postfix(u"", u"%[^]", unset) == 0 && unset[0] == '!', "swscanf unterminated set at end of input");
    Require(swscanf_nid_postfix(u"Ā", u"%s", narrow) == 1 && std::strcmp(narrow, "\xc4\x80") == 0, "swscanf narrow %s stores UTF-8");
}

int main() {
    CheckStoul();
    CheckClassification();
    CheckScan();
    CheckUnsizedScan();
    return failures == 0 ? 0 : 1;
}
