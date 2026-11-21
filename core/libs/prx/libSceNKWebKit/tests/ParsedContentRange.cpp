#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

using NKWebKit::WebCoreAbi::ParsedContentRange;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static constexpr std::int64_t kInvalid = std::numeric_limits<std::int64_t>::min();
static constexpr std::int64_t kUnknown = std::numeric_limits<std::int64_t>::max();

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ParsedContentRange check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static ParsedContentRange Parse(const char* text, bool wide = false) {
    std::u16string wideText;
    for (const char* c = text; *c; ++c) wideText.push_back(static_cast<unsigned char>(*c));
    StringImpl impl = wide ? StringImpl{4, static_cast<unsigned>(wideText.size()), wideText.data(), 0}
                           : StringImpl{4, static_cast<unsigned>(std::strlen(text)), text, 4};
    String string(&impl);
    impl.refCount = 4;
    ParsedContentRange range;
    std::memset(&range, 0x5A, sizeof(range));
    _ZN7WebCore18ParsedContentRangeC1ERKN3WTF6StringE(&range, &string);
    string.impl = nullptr;
    ParsedContentRange second;
    String again(&impl);
    _ZN7WebCore18ParsedContentRangeC2ERKN3WTF6StringE(&second, &again);
    again.impl = nullptr;
    Require(second.firstBytePosition == range.firstBytePosition);
    Require(second.lastBytePosition == range.lastBytePosition);
    Require(second.instanceLength == range.instanceLength);
    return range;
}

static bool Valid(const ParsedContentRange& range) {
    return range.instanceLength != kInvalid;
}

static void ExpectRange(const char* text, std::int64_t first, std::int64_t last, std::int64_t length) {
    for (bool wide : {false, true}) {
        const auto range = Parse(text, wide);
        Require(Valid(range));
        Require(range.firstBytePosition == first);
        Require(range.lastBytePosition == last);
        Require(range.instanceLength == length);
    }
}

static void ExpectInvalid(const char* text) {
    for (bool wide : {false, true}) Require(!Valid(Parse(text, wide)));
}

static std::string Header(const ParsedContentRange& range, bool& isNull) {
    String result = _ZNK7WebCore18ParsedContentRange11headerValueEv(&range);
    isNull = !result.impl;
    std::string text;
    if (result.impl) {
        Require(result.impl->hashAndFlags & 4);
        text.assign(static_cast<const char*>(result.impl->data), result.impl->length);
    }
    result.impl = nullptr;
    return text;
}

static void TestParse() {
    ExpectRange("bytes 0-499/1234", 0, 499, 1234);
    ExpectRange("bytes 500-999/*", 500, 999, kUnknown);
    ExpectRange("bytes 7-7/8", 7, 7, 8);
    ExpectRange("bytes 0-0/1", 0, 0, 1);
    ExpectRange("bytes 9223372036854775806-9223372036854775806/9223372036854775807", kUnknown - 1, kUnknown - 1, kUnknown);
    ExpectRange("bytes 00012-00020/00030", 12, 20, 30);
}

static void TestInvalid() {
    ExpectInvalid("");
    ExpectInvalid("bytes");
    ExpectInvalid("bytes ");
    ExpectInvalid("bytes 0-499");
    ExpectInvalid("bytes 0-499/");
    ExpectInvalid("bytes -499/1234");
    ExpectInvalid("bytes 0-/1234");
    ExpectInvalid("bytes 0499/1234");
    ExpectInvalid("bytes */1234");
    ExpectInvalid("bytes 0-499/**");
    ExpectInvalid("bytes 0-499/ 1234");
    ExpectInvalid("bytes 0-499/1234 ");
    ExpectInvalid("bytes  0-499/1234");
    ExpectInvalid("bytes 0 -499/1234");
    ExpectInvalid("bytes +0-499/1234");
    ExpectInvalid("bytes 0-4x9/1234");
    ExpectInvalid("bytes 0-499/12x4");
    ExpectInvalid("Bytes 0-499/1234");
    ExpectInvalid("bytes=0-499/1234");
    ExpectInvalid("items 0-499/1234");
    ExpectInvalid("bytes 500-499/1234");
    ExpectInvalid("bytes 0-1234/1234");
    ExpectInvalid("bytes 0-1235/1234");
    ExpectInvalid("bytes 0-499/0");
    ExpectInvalid("bytes 0-499/-1");
    ExpectInvalid("bytes 0-499/1234/5");
    ExpectInvalid("bytes 0-499-5/1234");
    ExpectInvalid("bytes 9223372036854775808-9223372036854775809/9223372036854775810");
    ExpectInvalid("bytes 0-9223372036854775808/*");
    ExpectInvalid("bytes 0-499/9223372036854775808");
    ExpectInvalid("bytes 0-499/99999999999999999999999");
}

static void TestInvalidKeepsParsedFields() {
    const auto range = Parse("bytes 600-500/1000");
    Require(!Valid(range));
    Require(range.firstBytePosition == 600);
    Require(range.lastBytePosition == 500);
    const auto noSeparator = Parse("nothing");
    Require(!Valid(noSeparator));
    Require(noSeparator.firstBytePosition == 0);
    Require(noSeparator.lastBytePosition == 0);
}

static void TestNullString() {
    String nullString;
    ParsedContentRange range;
    std::memset(&range, 0x5A, sizeof(range));
    _ZN7WebCore18ParsedContentRangeC1ERKN3WTF6StringE(&range, &nullString);
    Require(!Valid(range));
    Require(range.firstBytePosition == 0 && range.lastBytePosition == 0);
}

static void TestValueConstructors() {
    ParsedContentRange range;
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 10, 20, 30);
    Require(Valid(range) && range.firstBytePosition == 10 && range.lastBytePosition == 20 && range.instanceLength == 30);
    _ZN7WebCore18ParsedContentRangeC2Elll(&range, 0, 0, kUnknown);
    Require(Valid(range) && range.instanceLength == kUnknown);
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 0, 29, 30);
    Require(Valid(range));
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 0, 30, 30);
    Require(!Valid(range) && range.firstBytePosition == 0 && range.lastBytePosition == 30);
    _ZN7WebCore18ParsedContentRangeC2Elll(&range, -1, 5, 30);
    Require(!Valid(range) && range.firstBytePosition == -1);
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 6, 5, kUnknown);
    Require(!Valid(range));
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 0, 5, kInvalid);
    Require(!Valid(range));
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, kUnknown, kUnknown, kUnknown);
    Require(Valid(range));
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 0, kUnknown, kUnknown);
    Require(Valid(range));
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 0, kUnknown, kUnknown - 1);
    Require(!Valid(range));
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, kInvalid, kInvalid, kInvalid);
    Require(!Valid(range));
}

static void TestHeaderValue() {
    bool isNull = false;
    ParsedContentRange range;
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 0, 499, 1234);
    Require(Header(range, isNull) == "bytes 0-499/1234" && !isNull);
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 500, 999, kUnknown);
    Require(Header(range, isNull) == "bytes 500-999/*" && !isNull);
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, kUnknown - 1, kUnknown - 1, kUnknown);
    Require(Header(range, isNull) == "bytes 9223372036854775806-9223372036854775806/*");
    _ZN7WebCore18ParsedContentRangeC1Elll(&range, 5, 4, 10);
    Require(Header(range, isNull).empty() && isNull);
    const auto parsed = Parse("bytes 00012-00020/00030");
    Require(Header(parsed, isNull) == "bytes 12-20/30");
    Require(Header(Parse("garbage"), isNull).empty() && isNull);
}

static void TestNull() {
    bool threw = false;
    try {
        _ZNK7WebCore18ParsedContentRange11headerValueEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZN7WebCore18ParsedContentRangeC1Elll(nullptr, 0, 0, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    ParsedContentRange range;
    threw = false;
    try {
        _ZN7WebCore18ParsedContentRangeC1ERKN3WTF6StringE(&range, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        String nullString;
        _ZN7WebCore18ParsedContentRangeC2ERKN3WTF6StringE(nullptr, &nullString);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestParse();
    TestInvalid();
    TestInvalidKeepsParsedFields();
    TestNullString();
    TestValueConstructors();
    TestHeaderValue();
    TestNull();
    return 0;
}
