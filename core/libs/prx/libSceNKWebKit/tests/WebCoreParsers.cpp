#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WtfAbi::ByteVector;
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

}  // namespace

int main() {
    TestParseHTMLInteger();
    TestCrossOriginResourcePolicy();
    TestLineEndings();
    TestLineEndingStorage();
    return 0;
}
