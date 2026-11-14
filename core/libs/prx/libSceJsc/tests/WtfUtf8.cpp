#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::CString;
using Jsc::WtfAbi::CStringBuffer;
using Jsc::WtfAbi::ConversionResult;
using Jsc::WtfAbi::ExpectedCString;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringView;
using Jsc::WtfAbi::UTF8ConversionError;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF UTF-8 check failed at line %d\n", line);
        std::abort();
    }
}

template<typename Exception, typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

std::string Bytes(const CString& string) {
    return string.buffer ? std::string(reinterpret_cast<const char*>(string.buffer + 1), string.buffer->length) : std::string();
}

bool HoldsBytes(const CString& string, const std::string& expected) {
    if (!string.buffer || string.buffer->refCount != 1 || string.buffer->length != expected.size()) return false;
    const char* data = reinterpret_cast<const char*>(string.buffer + 1);
    return !std::memcmp(data, expected.data(), expected.size()) && !data[expected.size()];
}

bool HoldsBytes(const ExpectedCString& expected, const std::string& bytes) {
    return expected.hasValue && HoldsBytes(expected.value, bytes);
}

bool HoldsError(const ExpectedCString& expected, UTF8ConversionError error) {
    return !expected.hasValue && expected.error == error;
}

String Make8(const char* text) {
    return _ZN3WTF10StringImpl6createEPKh(reinterpret_cast<const unsigned char*>(text));
}

String Make16(const char16_t* text) {
    String result;
    _ZN3WTF6StringC1EPKDs(&result, text);
    return result;
}

struct Utf16Result {
    ConversionResult result;
    std::size_t consumed;
    std::string output;
};

Utf16Result FromUTF16(const std::u16string& source, std::size_t capacity, bool strict) {
    char target[64] = {};
    const char16_t* sourceStart = source.data();
    char* targetStart = target;
    const ConversionResult result = _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&sourceStart, source.data() + source.size(), &targetStart, target + capacity, strict);
    return {result, static_cast<std::size_t>(sourceStart - source.data()), std::string(target, targetStart)};
}

void TestConvertUTF16ToUTF8() {
    auto valid = FromUTF16(u"Aé€\U0001F600", 64, true);
    Check(valid.result == Jsc::WtfAbi::ConversionOK && valid.consumed == 5 && valid.output == "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", __LINE__);

    const std::u16string loneTrail{u'a', char16_t(0xDC00), u'b'};
    auto lenient = FromUTF16(loneTrail, 64, false);
    Check(lenient.result == Jsc::WtfAbi::ConversionOK && lenient.consumed == 3 && lenient.output == "a\xEF\xBF\xBD" "b", __LINE__);
    auto strict = FromUTF16(loneTrail, 64, true);
    Check(strict.result == Jsc::WtfAbi::SourceIllegal && strict.consumed == 1 && strict.output == "a", __LINE__);

    const std::u16string leadAtEnd{u'a', char16_t(0xD800)};
    auto exhaustedLenient = FromUTF16(leadAtEnd, 64, false);
    Check(exhaustedLenient.result == Jsc::WtfAbi::SourceExhausted && exhaustedLenient.consumed == 1 && exhaustedLenient.output == "a", __LINE__);
    auto exhaustedStrict = FromUTF16(leadAtEnd, 64, true);
    Check(exhaustedStrict.result == Jsc::WtfAbi::SourceExhausted && exhaustedStrict.consumed == 1, __LINE__);

    const std::u16string leadBeforeLetter{char16_t(0xD800), u'x'};
    auto leadMiddle = FromUTF16(leadBeforeLetter, 64, false);
    Check(leadMiddle.result == Jsc::WtfAbi::ConversionOK && leadMiddle.output == "\xEF\xBF\xBDx", __LINE__);

    auto full = FromUTF16(u"aé", 2, true);
    Check(full.result == Jsc::WtfAbi::TargetExhausted && full.consumed == 1 && full.output == "a", __LINE__);
    auto fourBytes = FromUTF16(u"\U0001F600", 3, true);
    Check(fourBytes.result == Jsc::WtfAbi::TargetExhausted && fourBytes.consumed == 0 && fourBytes.output.empty(), __LINE__);
    Check(Throws<std::length_error>([] { FromUTF16(u"ab", 1, true); }), __LINE__);

    Check(Throws<std::invalid_argument>([] {
        char target[4];
        char* targetStart = target;
        _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(nullptr, nullptr, &targetStart, target + 4, true);
    }), __LINE__);
}

void TestConvertLatin1ToUTF8() {
    const unsigned char source[] = {'A', 0xE9, 0xFF};
    char target[8] = {};
    const unsigned char* sourceStart = source;
    char* targetStart = target;
    Check(_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&sourceStart, source + 3, &targetStart, target + 8), __LINE__);
    Check(sourceStart == source + 3 && std::string(target, targetStart) == "A\xC3\xA9\xC3\xBF", __LINE__);

    const unsigned char twoAccents[] = {0xE9, 0xE9};
    char small[2] = {};
    sourceStart = twoAccents;
    targetStart = small;
    Check(!_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&sourceStart, twoAccents + 2, &targetStart, small + 2), __LINE__);
    Check(sourceStart == twoAccents && targetStart == small, __LINE__);
    Check(static_cast<unsigned char>(small[0]) == 0xC3 && static_cast<unsigned char>(small[1]) == 0xA9, __LINE__);
}

struct Utf8Result {
    bool ok;
    std::u16string output;
    bool ascii;
};

Utf8Result FromUTF8(const std::string& source, bool replace, std::size_t capacity = 32) {
    char16_t target[32] = {};
    char16_t* targetStart = target;
    bool ascii = false;
    const bool ok = replace
        ? _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(source.data(), source.data() + source.size(), &targetStart, target + capacity, &ascii)
        : _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(source.data(), source.data() + source.size(), &targetStart, target + capacity, &ascii);
    return {ok, std::u16string(target, targetStart), ascii};
}

void TestConvertUTF8ToUTF16() {
    auto valid = FromUTF8("A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", false);
    Check(valid.ok && valid.output == std::u16string{u'A', char16_t(0xE9), char16_t(0x20AC), char16_t(0xD83D), char16_t(0xDE00)} && !valid.ascii, __LINE__);
    auto ascii = FromUTF8("abc", false);
    Check(ascii.ok && ascii.output == u"abc" && ascii.ascii, __LINE__);
    auto empty = FromUTF8("", false);
    Check(empty.ok && empty.output.empty() && empty.ascii, __LINE__);

    Check(!FromUTF8("\xC0\x80", false).ok, __LINE__);
    Check(!FromUTF8("\xED\xA0\x80", false).ok, __LINE__);
    Check(!FromUTF8("\xF4\x90\x80\x80", false).ok, __LINE__);
    Check(!FromUTF8("\xE2\x82", false).ok, __LINE__);

    const char16_t r = 0xFFFD;
    Check(FromUTF8("\xC0\x80", true).output == std::u16string{r, r}, __LINE__);
    Check(FromUTF8("\xE0\x80\x80", true).output == std::u16string{r, r, r}, __LINE__);
    Check(FromUTF8("\xED\xA0\x80", true).output == std::u16string{r, r, r}, __LINE__);
    Check(FromUTF8("\xE2\x82" "A", true).output == std::u16string{r, u'A'}, __LINE__);
    Check(FromUTF8("\xF4\x90\x80\x80", true).output == std::u16string{r, r, r, r}, __LINE__);
    Check(FromUTF8("\xF0\x9F\x98", true).output == std::u16string{r}, __LINE__);
    auto replaced = FromUTF8("a\xFF", true);
    Check(replaced.ok && replaced.output == std::u16string{u'a', r} && !replaced.ascii, __LINE__);

    Check(!FromUTF8("\xF0\x9F\x98\x80", false, 1).ok, __LINE__);
    Check(Throws<std::length_error>([] { FromUTF8("ab", false, 1); }), __LINE__);

    char16_t target[4];
    char16_t* targetStart = target;
    bool untouched = true;
    const char* bad = "\xFF";
    Check(!_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(bad, bad + 1, &targetStart, target + 4, &untouched), __LINE__);
    Check(untouched && targetStart == target, __LINE__);
}

struct ImplResult {
    UTF8ConversionError error;
    std::string output;
};

ImplResult Utf8Impl(const std::u16string& source, Jsc::WtfAbi::ConversionMode mode) {
    char storage[64] = {};
    char* buffer = storage;
    const UTF8ConversionError error = _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(source.data(), static_cast<unsigned>(source.size()), buffer, source.size() * 3, mode);
    return {error, std::string(storage, buffer)};
}

void TestUtf8Impl() {
    const std::u16string leadAtEnd{u'a', char16_t(0xD800)};
    auto lenientEnd = Utf8Impl(leadAtEnd, Jsc::WtfAbi::LenientConversion);
    Check(lenientEnd.error == UTF8ConversionError::None && lenientEnd.output == "a\xED\xA0\x80", __LINE__);
    auto strictEnd = Utf8Impl(leadAtEnd, Jsc::WtfAbi::StrictConversion);
    Check(strictEnd.error == UTF8ConversionError::SourceExhausted && strictEnd.output == "a", __LINE__);
    auto replacedEnd = Utf8Impl(leadAtEnd, Jsc::WtfAbi::StrictConversionReplacingUnpairedSurrogatesWithFFFD);
    Check(replacedEnd.error == UTF8ConversionError::None && replacedEnd.output == "a\xEF\xBF\xBD", __LINE__);

    const std::u16string leadFirst{char16_t(0xD800), u'a'};
    Check(Utf8Impl(leadFirst, Jsc::WtfAbi::LenientConversion).output == "\xEF\xBF\xBD" "a", __LINE__);
    auto strictIllegal = Utf8Impl(leadFirst, Jsc::WtfAbi::StrictConversion);
    Check(strictIllegal.error == UTF8ConversionError::IllegalSource && strictIllegal.output.empty(), __LINE__);

    const std::u16string mixed{char16_t(0xDC00), u'a', char16_t(0xD83D), char16_t(0xDE00), char16_t(0xD800)};
    auto mixedReplaced = Utf8Impl(mixed, Jsc::WtfAbi::StrictConversionReplacingUnpairedSurrogatesWithFFFD);
    Check(mixedReplaced.error == UTF8ConversionError::None && mixedReplaced.output == "\xEF\xBF\xBD" "a\xF0\x9F\x98\x80\xEF\xBF\xBD", __LINE__);

    Check(Throws<std::length_error>([] {
        const char16_t source[] = {0x20AC};
        char storage[2];
        char* buffer = storage;
        _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(source, 1, buffer, 2, Jsc::WtfAbi::LenientConversion);
    }), __LINE__);
}

void TestStringImplUtf8() {
    String latin1 = Make8("caf\xE9");
    Check(HoldsBytes(_ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE(latin1.impl, Jsc::WtfAbi::StrictConversion), "caf\xC3\xA9"), __LINE__);
    Check(HoldsBytes(_ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE(latin1.impl, 3, 1, Jsc::WtfAbi::StrictConversion), "\xC3\xA9"), __LINE__);
    Check(HoldsBytes(_ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE(latin1.impl, 4, 0, Jsc::WtfAbi::StrictConversion), ""), __LINE__);
    Check(Throws<std::out_of_range>([&] { _ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE(latin1.impl, 2, 3, Jsc::WtfAbi::LenientConversion); }), __LINE__);
    Check(Throws<std::out_of_range>([&] { _ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE(latin1.impl, 5, 0, Jsc::WtfAbi::LenientConversion); }), __LINE__);

    const char16_t text[] = {u'x', 0xDC00, u'y', 0x4E2D, 0};
    String wide = Make16(text);
    Check(HoldsBytes(_ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE(wide.impl, Jsc::WtfAbi::LenientConversion), "x\xEF\xBF\xBDy\xE4\xB8\xAD"), __LINE__);
    Check(HoldsError(_ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE(wide.impl, Jsc::WtfAbi::StrictConversion), UTF8ConversionError::IllegalSource), __LINE__);
    Check(HoldsBytes(_ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE(wide.impl, 2, 2, Jsc::WtfAbi::StrictConversion), "y\xE4\xB8\xAD"), __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE(nullptr, Jsc::WtfAbi::LenientConversion); }), __LINE__);
}

void TestStringViewUtf8() {
    StringView null{nullptr, 0, true};
    Check(HoldsBytes(_ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(&null, Jsc::WtfAbi::StrictConversion), ""), __LINE__);
    const unsigned char latin1[] = {'a', 0xFC};
    StringView narrow{latin1, 2, true};
    Check(HoldsBytes(_ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(&narrow, Jsc::WtfAbi::StrictConversion), "a\xC3\xBC"), __LINE__);
    const char16_t utf16[] = {0xD83D, 0xDE00, 0xD83D};
    StringView wide{utf16, 3, false};
    Check(HoldsError(_ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(&wide, Jsc::WtfAbi::StrictConversion), UTF8ConversionError::SourceExhausted), __LINE__);
    Check(HoldsBytes(_ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(&wide, Jsc::WtfAbi::StrictConversionReplacingUnpairedSurrogatesWithFFFD), "\xF0\x9F\x98\x80\xEF\xBF\xBD"), __LINE__);
    Check(HoldsBytes(_ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(&wide, Jsc::WtfAbi::LenientConversion), "\xF0\x9F\x98\x80\xED\xA0\xBD"), __LINE__);
    StringView emptyWide{utf16, 0, false};
    Check(HoldsBytes(_ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(&emptyWide, Jsc::WtfAbi::StrictConversion), ""), __LINE__);
}

void TestStringUtf8() {
    String null;
    Check(HoldsBytes(_ZNK3WTF6String10tryGetUtf8Ev(&null), ""), __LINE__);
    Check(HoldsBytes(_ZNK3WTF6String4utf8Ev(&null), ""), __LINE__);

    const char16_t text[] = {u'a', 0xDFFF, 0};
    String wide = Make16(text);
    Check(HoldsBytes(_ZNK3WTF6String10tryGetUtf8Ev(&wide), "a\xEF\xBF\xBD"), __LINE__);
    Check(HoldsBytes(_ZNK3WTF6String4utf8Ev(&wide), "a\xEF\xBF\xBD"), __LINE__);
    Check(HoldsError(_ZNK3WTF6String10tryGetUtf8ENS_14ConversionModeE(&wide, Jsc::WtfAbi::StrictConversion), UTF8ConversionError::IllegalSource), __LINE__);
    Check(HoldsBytes(_ZNK3WTF6String10tryGetUtf8ENS_14ConversionModeE(&wide, Jsc::WtfAbi::StrictConversionReplacingUnpairedSurrogatesWithFFFD), "a\xEF\xBF\xBD"), __LINE__);

    String ascii = Make8("hello");
    CString converted = _ZNK3WTF6String4utf8Ev(&ascii);
    Check(Bytes(converted) == "hello", __LINE__);
    ++converted.buffer->refCount;
    {
        CString second(converted.buffer);
    }
    Check(converted.buffer->refCount == 1, __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZNK3WTF6String4utf8Ev(nullptr); }), __LINE__);
}

}  // namespace

int main() {
    TestConvertUTF16ToUTF8();
    TestConvertLatin1ToUTF8();
    TestConvertUTF8ToUTF16();
    TestUtf8Impl();
    TestStringImplUtf8();
    TestStringViewUtf8();
    TestStringUtf8();
    std::puts("WTF UTF-8 tests passed");
    return 0;
}
