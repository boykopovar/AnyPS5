#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::ConversionResult;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF::Unicode check failed at line %d\n", line);
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

struct Utf16Result {
    bool ok;
    std::u16string text;
    bool ascii;
};

template<bool Replace> Utf16Result Utf16(const std::string& input, std::size_t capacity = 64) {
    char16_t buffer[64];
    std::fill(buffer, buffer + 64, u'#');
    char16_t* target = buffer;
    bool ascii = false;
    const bool ok = Replace
        ? _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(input.data(), input.data() + input.size(), &target, buffer + capacity, &ascii)
        : _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(input.data(), input.data() + input.size(), &target, buffer + capacity, &ascii);
    return {ok, std::u16string(buffer, target), ascii};
}

struct Utf8Result {
    ConversionResult result;
    std::size_t consumed;
    std::string text;
};

Utf8Result Utf8(const std::u16string& input, std::size_t capacity, bool strict) {
    char buffer[64];
    const char16_t* source = input.data();
    char* target = buffer;
    const auto result = _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, input.data() + input.size(), &target, buffer + capacity, strict);
    return {result, static_cast<std::size_t>(source - input.data()), std::string(buffer, target)};
}

void TestUtf8ToUtf16() {
    auto result = Utf16<false>("abc");
    Check(result.ok && result.text == u"abc" && result.ascii, __LINE__);
    result = Utf16<false>("a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
    Check(result.ok && result.text == u"aé€\U0001F600" && !result.ascii, __LINE__);
    result = Utf16<false>("");
    Check(result.ok && result.text.empty() && result.ascii, __LINE__);

    Check(!Utf16<false>("a\x80").ok, __LINE__);
    Check(!Utf16<false>("\xC0\xAF").ok, __LINE__);
    Check(!Utf16<false>("\xED\xA0\x80").ok, __LINE__);
    Check(!Utf16<false>("\xE2\x82").ok, __LINE__);
    Check(!Utf16<false>("\xF4\x90\x80\x80").ok, __LINE__);

    result = Utf16<true>("a\x80" "b\xE2\x82" "c\xED\xA0\x80" "d");
    Check(result.ok && result.text == u"a�b�c���d" && !result.ascii, __LINE__);
    result = Utf16<true>("\xF0\x9F\x98");
    Check(result.ok && result.text == u"�", __LINE__);

    Check(Throws<std::out_of_range>([] { Utf16<false>("abc", 2); }), __LINE__);
    Check(Throws<std::out_of_range>([] { Utf16<true>("a\x80", 1); }), __LINE__);
    Check(Utf16<false>("ab", 2).ok, __LINE__);
    Check(!Utf16<false>("\xF0\x9F\x98\x80", 1).ok, __LINE__);

    char16_t buffer[4];
    char16_t* target = buffer;
    const char* text = "x";
    Check(_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(text, text + 1, &target, buffer + 4, nullptr) && target == buffer + 1 && buffer[0] == u'x', __LINE__);
    target = buffer;
    const char* invalid = "\x80";
    Check(!_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(invalid, invalid + 1, &target, buffer + 4, nullptr) && target == buffer, __LINE__);

    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(text, text + 1, nullptr, buffer + 4, nullptr); }), __LINE__);
    Check(Throws<std::length_error>([&] {
        _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(text, text + std::size_t{1} + std::numeric_limits<int>::max(), &target, buffer + 4, nullptr);
    }), __LINE__);
}

void TestUtf16ToUtf8() {
    auto result = Utf8(u"aé€\U0001F600", 64, true);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 5 && result.text == "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", __LINE__);

    result = Utf8(std::u16string(u"a") + char16_t(0xDC00) + u"b", 64, true);
    Check(result.result == ConversionResult::SourceIllegal && result.consumed == 1 && result.text == "a", __LINE__);
    result = Utf8(std::u16string(u"a") + char16_t(0xDC00) + u"b", 64, false);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 3 && result.text == "a\xEF\xBF\xBD" "b", __LINE__);
    result = Utf8(std::u16string(u"a") + char16_t(0xD800) + u"b", 64, false);
    Check(result.result == ConversionResult::ConversionOK && result.text == "a\xEF\xBF\xBD" "b", __LINE__);

    result = Utf8(std::u16string(u"a") + char16_t(0xD800), 64, false);
    Check(result.result == ConversionResult::SourceExhausted && result.consumed == 1 && result.text == "a", __LINE__);
    result = Utf8(std::u16string(u"a") + char16_t(0xDC00), 64, false);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 2, __LINE__);

    result = Utf8(u"a€", 3, true);
    Check(result.result == ConversionResult::TargetExhausted && result.consumed == 1 && result.text == "a", __LINE__);
    result = Utf8(u"a€", 4, true);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 2, __LINE__);

    Check(Throws<std::out_of_range>([] { Utf8(u"\u00E9a", 2, true); }), __LINE__);

    char buffer[4];
    char* target = buffer;
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(nullptr, nullptr, &target, buffer + 4, true); }), __LINE__);
}

void TestLatin1ToUtf8() {
    const unsigned char input[] = {'a', 0x7F, 0x80, 0xE9, 0xFF};
    char buffer[16];
    std::memset(buffer, '#', sizeof(buffer));
    const unsigned char* source = input;
    char* target = buffer;
    Check(_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, input + 5, &target, buffer + 16), __LINE__);
    Check(source == input + 5 && std::string(buffer, target) == "a\x7F\xC2\x80\xC3\xA9\xC3\xBF", __LINE__);

    source = input;
    target = buffer;
    Check(!_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, input + 5, &target, buffer + 7), __LINE__);
    Check(source == input && target == buffer, __LINE__);

    Check(Throws<std::out_of_range>([&] { _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, input + 2, &target, buffer + 1); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, input + 5, nullptr, buffer + 16); }), __LINE__);
}

}

int main() {
    TestUtf8ToUtf16();
    TestUtf16ToUtf8();
    TestLatin1ToUtf8();
    return 0;
}
