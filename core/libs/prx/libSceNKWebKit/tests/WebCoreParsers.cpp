#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using NKWebKit::WtfAbi::ByteVector;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore parsers check failed at line %u\n", static_cast<unsigned>(location.line()));
        std::abort();
    }
}

template<typename T> void Throws(T action, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        action();
    } catch (const std::exception&) {
        threw = true;
    }
    Check(threw, location);
}

template<typename Function> void ForBothWidths(const char* text, Function function) {
    function(StringView{text, static_cast<unsigned>(std::strlen(text)), true});
    std::u16string wide;
    for (const char* cursor = text; *cursor; ++cursor) wide.push_back(static_cast<unsigned char>(*cursor));
    function(StringView{wide.data(), static_cast<unsigned>(wide.size()), false});
}

void TestXFrameOptions() {
    using Disposition = WebCore::XFrameOptionsDisposition;
    using NKWebKit::WtfAbi::String;
    using NKWebKit::WtfAbi::StringImpl;

    static_assert(static_cast<unsigned>(Disposition::None) == 0);
    static_assert(static_cast<unsigned>(Disposition::Deny) == 1);
    static_assert(static_cast<unsigned>(Disposition::SameOrigin) == 2);
    static_assert(static_cast<unsigned>(Disposition::AllowAll) == 3);
    static_assert(static_cast<unsigned>(Disposition::Invalid) == 4);
    static_assert(static_cast<unsigned>(Disposition::Conflict) == 5);

    constexpr struct {
        const char* text;
        Disposition expected;
    } cases[] = {
        {"", Disposition::None},
        {",,,", Disposition::None},
        {"dEnY", Disposition::Deny},
        {"\vSaMeOrIgIn\r\t", Disposition::SameOrigin},
        {" ALLOWALL ", Disposition::AllowAll},
        {",DENY,,deny,", Disposition::Deny},
        {"deny,sameorigin", Disposition::Conflict},
        {"deny,invalid", Disposition::Conflict},
        {"invalid,deny", Disposition::Conflict},
        {"x,y", Disposition::Invalid},
        {" ", Disposition::Invalid},
        {"deny, ,deny", Disposition::Conflict},
        {"DENY\xa0", Disposition::Invalid},
        {"deny xyz", Disposition::Invalid},
    };
    for (const auto& test : cases) {
        ForBothWidths(test.text, [&](StringView view) {
            StringImpl storage{1, view.length, view.data, view.is8Bit ? 4u : 0u};
            Check(_ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(String(&storage)) == test.expected);
        });
    }
    Check(_ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(String()) == Disposition::None);

    auto checkWide = [](std::u16string_view text, Disposition expected) {
        StringImpl storage{1, static_cast<unsigned>(text.size()), text.data(), 0};
        Check(_ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(String(&storage)) == expected);
    };
    checkWide(u"\u1680DENY\u2028,\u2007deny\u3000", Disposition::Deny);
    checkWide(u"SAMEORIGIN\u2029", Disposition::Invalid);
    checkWide(u"D\u0145NY", Disposition::Invalid);
    checkWide(std::u16string_view(u"DENY\0", 5), Disposition::Invalid);

    StringImpl invalid{1, 1, nullptr, 4};
    Throws([&] { _ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(String(&invalid)); });
}

struct IntegerCase {
    long long expected;
    const char* input;
};

const IntegerCase kIntegerCases[] = {
    {0LL, "0"},
    {0LL, "-0"},
    {0LL, "+0"},
    {123LL, "123"},
    {123LL, "+123"},
    {-123LL, "-123"},
    {123LL, "  123"},
    {123LL, "123   "},
    {123LL, "   123   "},
    {123LL, "123abc"},
    {-123LL, "-123abc"},
    {123LL, "  +123"},
    {-123LL, "  -123"},
    {12LL, "   12 3"},
    {1LL, "1.0"},
    {1LL, "1."},
    {1LL, "1e1"},
    {123LL, " \t\r\n\f123"},
    {-2147483648LL, "-2147483648"},
    {2147483647LL, "2147483647"}
};

const char* const kIntegerFailures[] = {
    "-2147483649",
    "2147483648",
    "111111111111111111",
    "",
    " ",
    "   ",
    "+",
    "+ 123",
    "-",
    "- 123",
    "a",
    "-a",
    "+-123",
    "-+123",
    "++123",
    "--123",
    "\v123",
    "a123",
    "+a123",
    "-a123",
    ".1",
    "infinity"
};

void TestParseHTMLInteger() {
    for (const auto& testCase : kIntegerCases) {
        ForBothWidths(testCase.input, [&](StringView view) {
            auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
            Check(result.hasValue);
            Check(result.value == testCase.expected);
        });
    }
    for (const char* input : kIntegerFailures) {
        ForBothWidths(input, [&](StringView view) {
            Check(!_ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view).hasValue);
        });
    }

    ForBothWidths("-2147483649", [](StringView view) {
        auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
        Check(!result.hasValue && result.error == WebCore::HTMLIntegerParsingError::NegativeOverflow);
    });
    ForBothWidths("2147483648", [](StringView view) {
        auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
        Check(!result.hasValue && result.error == WebCore::HTMLIntegerParsingError::PositiveOverflow);
    });
    ForBothWidths("111111111111111111", [](StringView view) {
        auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
        Check(!result.hasValue && result.error == WebCore::HTMLIntegerParsingError::PositiveOverflow);
    });
    for (const char* input : {"", " ", "+", "-", "a", "\v123", ".1"}) {
        ForBothWidths(input, [](StringView view) {
            auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
            Check(!result.hasValue && result.error == WebCore::HTMLIntegerParsingError::Other);
        });
    }

    std::u16string wide = u"12\u0663";
    auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView{wide.data(), static_cast<unsigned>(wide.size()), false});
    Check(result.hasValue && result.value == 12);
    wide = u"\u00a0" u"12";
    Check(!_ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView{wide.data(), static_cast<unsigned>(wide.size()), false}).hasValue);

    Check(!_ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView{nullptr, 0, true}).hasValue);
    Throws([] { _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView{nullptr, 3, true}); });
}

void ExpectPolicy(const char* text, WebCore::CrossOriginResourcePolicy expected, std::source_location location = std::source_location::current()) {
    ForBothWidths(text, [&](StringView view) {
        Check(_ZN7WebCore36parseCrossOriginResourcePolicyHeaderEN3WTF10StringViewE(view) == expected, location);
    });
}

void TestCrossOriginResourcePolicy() {
    using WebCore::CrossOriginResourcePolicy;
    ExpectPolicy("", CrossOriginResourcePolicy::None);
    ExpectPolicy(" \t\r\n ", CrossOriginResourcePolicy::None);
    ExpectPolicy("same-origin", CrossOriginResourcePolicy::SameOrigin);
    ExpectPolicy("  same-origin\t", CrossOriginResourcePolicy::SameOrigin);
    ExpectPolicy("same-site", CrossOriginResourcePolicy::SameSite);
    ExpectPolicy("\r\nsame-site\r\n", CrossOriginResourcePolicy::SameSite);
    ExpectPolicy("cross-origin", CrossOriginResourcePolicy::CrossOrigin);
    ExpectPolicy("Cross-Origin", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("same-origin, same-site", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("same-origi", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("same-origins", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("\fsame-origin", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("\vsame-site", CrossOriginResourcePolicy::Invalid);
    Check(_ZN7WebCore36parseCrossOriginResourcePolicyHeaderEN3WTF10StringViewE(StringView{nullptr, 0, true}) == CrossOriginResourcePolicy::None);
    Throws([] { _ZN7WebCore36parseCrossOriginResourcePolicyHeaderEN3WTF10StringViewE(StringView{nullptr, 4, true}); });
}

ByteVector Make(std::initializer_list<unsigned char> bytes) {
    ByteVector vector;
    if (bytes.size()) {
        vector.buffer = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(bytes.size()));
        std::memcpy(vector.buffer, bytes.begin(), bytes.size());
        vector.capacity = static_cast<unsigned>(bytes.size());
        vector.size = static_cast<unsigned>(bytes.size());
    }
    return vector;
}

std::string Stringify(const ByteVector& vector) {
    std::string out;
    for (unsigned i = 0; i < vector.size; ++i) {
        switch (vector.buffer[i]) {
        case '\0': out += "<0>"; break;
        case '\r': out += "<CR>"; break;
        case '\n': out += "<LF>"; break;
        default: out.push_back(static_cast<char>(vector.buffer[i]));
        }
    }
    return out;
}

constexpr unsigned char null = 0;
constexpr unsigned char CR = '\r';
constexpr unsigned char LF = '\n';
constexpr unsigned char letterA = 'a';
constexpr unsigned char letterB = 'b';

std::string ToLF(std::initializer_list<unsigned char> bytes) {
    ByteVector input = Make(bytes);
    ByteVector output = _ZN3WTF24normalizeLineEndingsToLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(input));
    Check(input.buffer == nullptr && input.size == 0 && input.capacity == 0);
    return Stringify(output);
}

std::string ToCRLF(std::initializer_list<unsigned char> bytes) {
    ByteVector input = Make(bytes);
    ByteVector output = _ZN3WTF26normalizeLineEndingsToCRLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(input));
    return Stringify(output);
}

std::string ToNative(std::initializer_list<unsigned char> bytes) {
    ByteVector input = Make(bytes);
    ByteVector output = _ZN3WTF28normalizeLineEndingsToNativeEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(input));
    return Stringify(output);
}

void TestLineEndings() {
    Check(ToLF({}) == "");
    Check(ToLF({null}) == "<0>");
    Check(ToLF({letterA}) == "a");
    Check(ToLF({CR}) == "<LF>");
    Check(ToLF({LF}) == "<LF>");
    Check(ToLF({CR, LF}) == "<LF>");
    Check(ToLF({LF, LF}) == "<LF><LF>");
    Check(ToLF({CR, CR}) == "<LF><LF>");
    Check(ToLF({LF, CR}) == "<LF><LF>");
    Check(ToLF({letterA, CR}) == "a<LF>");
    Check(ToLF({letterA, LF}) == "a<LF>");
    Check(ToLF({letterA, CR, LF}) == "a<LF>");
    Check(ToLF({letterA, LF, CR}) == "a<LF><LF>");
    Check(ToLF({letterA, CR, letterB}) == "a<LF>b");
    Check(ToLF({letterA, LF, letterB}) == "a<LF>b");
    Check(ToLF({letterA, CR, LF, letterB}) == "a<LF>b");
    Check(ToLF({letterA, LF, CR, letterB}) == "a<LF><LF>b");

    Check(ToCRLF({}) == "");
    Check(ToCRLF({null}) == "<0>");
    Check(ToCRLF({letterA}) == "a");
    Check(ToCRLF({CR}) == "<CR><LF>");
    Check(ToCRLF({LF}) == "<CR><LF>");
    Check(ToCRLF({CR, LF}) == "<CR><LF>");
    Check(ToCRLF({LF, LF}) == "<CR><LF><CR><LF>");
    Check(ToCRLF({CR, CR}) == "<CR><LF><CR><LF>");
    Check(ToCRLF({LF, CR}) == "<CR><LF><CR><LF>");
    Check(ToCRLF({letterA, CR}) == "a<CR><LF>");
    Check(ToCRLF({letterA, LF}) == "a<CR><LF>");
    Check(ToCRLF({letterA, CR, LF}) == "a<CR><LF>");
    Check(ToCRLF({letterA, LF, CR}) == "a<CR><LF><CR><LF>");
    Check(ToCRLF({letterA, CR, letterB}) == "a<CR><LF>b");
    Check(ToCRLF({letterA, LF, letterB}) == "a<CR><LF>b");
    Check(ToCRLF({letterA, CR, LF, letterB}) == "a<CR><LF>b");
    Check(ToCRLF({letterA, LF, CR, letterB}) == "a<CR><LF><CR><LF>b");

    Check(ToNative({letterA, CR, LF, letterB, CR, CR}) == "a<LF>b<LF><LF>");
    Check(ToNative({}) == "");
}

void TestLineEndingStorage() {
    ByteVector input = Make({letterA, CR, LF, letterB});
    unsigned char* buffer = input.buffer;
    ByteVector lf = _ZN3WTF24normalizeLineEndingsToLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(input));
    Check(lf.buffer == buffer && lf.size == 3 && lf.capacity == 4);

    ByteVector clean = Make({letterA, letterB});
    unsigned char* cleanBuffer = clean.buffer;
    ByteVector same = _ZN3WTF26normalizeLineEndingsToCRLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(clean));
    Check(same.buffer == cleanBuffer && same.size == 2 && clean.buffer == nullptr);

    ByteVector grown = Make({LF, letterA});
    unsigned char* original = grown.buffer;
    ByteVector crlf = _ZN3WTF26normalizeLineEndingsToCRLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(grown));
    Check(crlf.buffer != original && crlf.size == 3 && crlf.capacity == 3);
    Check(grown.buffer == original && grown.size == 2);
}

struct RangeResult {
    bool accepted;
    long long offset;
    long long end;
    long long suffixLength;
};

RangeResult ParseRange(const String& string) {
    RangeResult result{false, 7, 7, 7};
    result.accepted = _ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(&string, &result.offset, &result.end, &result.suffixLength);
    return result;
}

String MakeNarrow(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

String MakeWide(const std::u16string& text) {
    String string = _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
    if (!text.empty()) Check(!(string.impl->hashAndFlags & 4));
    return string;
}

template<typename Function> void ForBothStringWidths(const char* text, Function function) {
    function(MakeNarrow(text));
    std::u16string wide;
    for (const char* cursor = text; *cursor; ++cursor) wide.push_back(static_cast<unsigned char>(*cursor));
    function(MakeWide(wide));
}

void ExpectRange(const char* text, long long offset, long long end, long long suffixLength, std::source_location location = std::source_location::current()) {
    ForBothStringWidths(text, [&](const String& string) {
        RangeResult result = ParseRange(string);
        Check(result.accepted && result.offset == offset && result.end == end && result.suffixLength == suffixLength, location);
    });
}

void ExpectRejected(const char* text, std::source_location location = std::source_location::current()) {
    ForBothStringWidths(text, [&](const String& string) {
        RangeResult result = ParseRange(string);
        Check(!result.accepted && result.offset == -1 && result.end == -1 && result.suffixLength == -1, location);
    });
}

bool IsRangeWhitespace(char16_t unit) {
    return (unit >= 0x9 && unit <= 0xD) || unit == 0x20 || unit == 0x1680 || (unit >= 0x2000 && unit <= 0x200A) || unit == 0x2028 || unit == 0x205F || unit == 0x3000;
}

bool IsZeroToFive(const std::u16string& text) {
    RangeResult result = ParseRange(MakeWide(text));
    return result.accepted && result.offset == 0 && result.end == 5 && result.suffixLength == -1;
}

void TestParseRangeForms() {
    constexpr long long maximum = std::numeric_limits<long long>::max();
    constexpr long long minimum = std::numeric_limits<long long>::min();

    ExpectRange("bytes=0-499", 0, 499, -1);
    ExpectRange("bytes=500-", 500, -1, -1);
    ExpectRange("bytes=-500", -1, -1, 500);
    ExpectRange("bytes=0-0", 0, 0, -1);
    ExpectRange("bytes=7-7", 7, 7, -1);
    ExpectRange("bytes=00012-00015", 12, 15, -1);
    ExpectRange("bytes=+0-+5", 0, 5, -1);
    ExpectRange("bytes=-+5", -1, -1, 5);
    ExpectRange("bytes=-0", -1, -1, 0);

    ExpectRange("BYTES=0-1", 0, 1, -1);
    ExpectRange("Bytes=2-3", 2, 3, -1);
    ExpectRange("bYtEs=4-5", 4, 5, -1);
    ExpectRange("bytes\x1d" "0-1", 0, 1, -1);

    ExpectRange("bytes= 0 - 499 ", 0, 499, -1);
    ExpectRange("bytes=\t0-\t1\t", 0, 1, -1);
    ExpectRange("bytes=\v0-1\f", 0, 1, -1);
    ExpectRange("bytes=\r\n0-1\r\n", 0, 1, -1);
    ExpectRange("bytes= 5 -", 5, -1, -1);
    ExpectRange("bytes=5-   ", 5, -1, -1);
    ExpectRange("bytes=- 500 ", -1, -1, 500);

    ExpectRange("bytes=0-9223372036854775807", 0, maximum, -1);
    ExpectRange("bytes=9223372036854775807-9223372036854775807", maximum, maximum, -1);
    ExpectRange("bytes=9223372036854775807-", maximum, -1, -1);
    ExpectRange("bytes=-9223372036854775807", -1, -1, maximum);
    ExpectRange("bytes=-9223372036854775808", -1, -1, -1);

    ExpectRange("bytes=5--1", 5, -1, -1);
    ExpectRange("bytes=--5", -1, -1, -5);
    ExpectRange("bytes=--9223372036854775807", -1, -1, -maximum);
    ExpectRange("bytes=--9223372036854775808", -1, -1, minimum);
}

void TestParseRangeSuffixLeniency() {
    ExpectRange("bytes=-", -1, -1, -1);
    ExpectRange("bytes=- ", -1, -1, -1);
    ExpectRange("bytes=-abc", -1, -1, -1);
    ExpectRange("bytes=-5,7", -1, -1, -1);
    ExpectRange("bytes=-5-6", -1, -1, -1);
    ExpectRange("bytes=-5 5", -1, -1, -1);
    ExpectRange("bytes=-9223372036854775809", -1, -1, -1);
    ExpectRange("bytes=--9223372036854775809", -1, -1, -1);
}

void TestParseRangeRejections() {
    static const char* const rejected[] = {
        "",
        "b",
        "bytes",
        "bytes=",
        "bytes=5",
        "bytes=5 ",
        "byte=0-1",
        "bytes 0-1",
        "bytes:0-1",
        "bytes0-1",
        "bytess=0-1",
        "items=0-1",
        " bytes=0-1",
        "\tbytes=0-1",
        "bytes =0-1",
        "bytes=a-5",
        "bytes=5-a",
        "bytes=5-5a",
        "bytes=0x5-7",
        "bytes=1.5-2",
        "bytes=1-2.5",
        "bytes=1 2-3",
        "bytes=1-2 3",
        "bytes=+-5",
        "bytes=5-4",
        "bytes=1-0",
        "bytes=10-9",
        "bytes=9223372036854775807-0",
        "bytes=5--2",
        "bytes= -5",
        "bytes= -5-6",
        "bytes=0-1,5-6",
        "bytes=0-1, 5-6",
        "bytes=0-,5-",
        "bytes=0-1,",
        "bytes=0-1;5-6",
        "bytes=0-9223372036854775808",
        "bytes=9223372036854775808-",
        "bytes=99999999999999999999-",
        "bytes=0-99999999999999999999",
        "bytes=\xa0" "0-5",
        "bytes=0-5\xa0",
        "bytes=\x85" "0-5",
    };
    for (const char* text : rejected) ExpectRejected(text);

    RangeResult result = ParseRange(String());
    Check(!result.accepted && result.offset == -1 && result.end == -1 && result.suffixLength == -1);
}

void TestParseRangeWhitespaceUnits() {
    for (unsigned value = 0; value <= 0xFFFF; ++value) {
        const char16_t unit = static_cast<char16_t>(value);
        std::u16string leading = u"bytes=";
        leading.push_back(unit);
        leading += u"0-5";
        Check(IsZeroToFive(leading) == (IsRangeWhitespace(unit) || unit == u'+' || unit == u'0'));

        std::u16string trailing = u"bytes=0-5";
        trailing.push_back(unit);
        Check(IsZeroToFive(trailing) == IsRangeWhitespace(unit));

        std::u16string separator = u"bytes";
        separator.push_back(unit);
        separator += u"0-5";
        Check(IsZeroToFive(separator) == (unit == u'=' || unit == 0x1D));
    }
}

void TestParseRangeArguments() {
    String text = MakeNarrow("bytes=0-1");
    const unsigned references = text.impl->refCount;
    long long offset = 0;
    long long end = 0;
    long long suffixLength = 0;
    Check(_ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(&text, &offset, &end, &suffixLength));
    Check(text.impl->refCount == references && offset == 0 && end == 1 && suffixLength == -1);

    Throws([&] { _ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(nullptr, &offset, &end, &suffixLength); });
    Throws([&] { _ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(&text, nullptr, &end, &suffixLength); });
    Throws([&] { _ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(&text, &offset, nullptr, &suffixLength); });
    Throws([&] { _ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(&text, &offset, &end, nullptr); });

    NKWebKit::WtfAbi::StringImpl broken{2, 9, nullptr, 4};
    String brokenString(&broken);
    Throws([&] { ParseRange(brokenString); });
    brokenString.impl = nullptr;
}

}  // namespace

int main() {
    TestParseHTMLInteger();
    TestCrossOriginResourcePolicy();
    TestXFrameOptions();
    TestLineEndings();
    TestLineEndingStorage();
    TestParseRangeForms();
    TestParseRangeSuffixLeniency();
    TestParseRangeRejections();
    TestParseRangeWhitespaceUnits();
    TestParseRangeArguments();
    return 0;
}
