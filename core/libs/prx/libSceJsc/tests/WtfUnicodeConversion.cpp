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

void TestUtf16ToUtf8SurrogateResumeForMode(bool strict) {
    const std::u16string input{u'a', char16_t(0xD83D), char16_t(0xDE00), u'z'};
    const char16_t* const inputEnd = input.data() + input.size();

    char splitBuffer[16]{};
    const char16_t* splitSource = input.data();
    char* splitTarget = splitBuffer;
    Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&splitSource, input.data() + 2, &splitTarget, splitBuffer + sizeof(splitBuffer), strict) == ConversionResult::SourceExhausted, __LINE__);
    Check(splitSource == input.data() + 1 && splitTarget == splitBuffer + 1, __LINE__);
    Check(*splitBuffer == 'a', __LINE__);
    Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&splitSource, inputEnd, &splitTarget, splitBuffer + sizeof(splitBuffer), strict) == ConversionResult::ConversionOK, __LINE__);
    Check(splitSource == inputEnd && splitTarget == splitBuffer + 6, __LINE__);
    Check(std::string(splitBuffer, splitTarget) == "a\xF0\x9F\x98\x80" "z", __LINE__);

    for (std::size_t remaining = 0; remaining < 4; ++remaining) {
        char firstBuffer[16];
        std::memset(firstBuffer, '#', sizeof(firstBuffer));
        const char16_t* source = input.data();
        char* firstTarget = firstBuffer;
        const auto firstResult = _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, inputEnd, &firstTarget, firstBuffer + 1 + remaining, strict);
        Check(firstResult == ConversionResult::TargetExhausted, __LINE__);
        Check(source == input.data() + 1, __LINE__);
        Check(firstTarget == firstBuffer + 1, __LINE__);
        Check(std::string(firstBuffer, firstTarget) == "a", __LINE__);
        for (std::size_t index = 1; index < sizeof(firstBuffer); ++index) {
            Check(firstBuffer[index] == '#', __LINE__);
        }

        char secondBuffer[16];
        std::memset(secondBuffer, '#', sizeof(secondBuffer));
        char* secondTarget = secondBuffer;
        const auto secondResult = _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, inputEnd, &secondTarget, secondBuffer + sizeof(secondBuffer), strict);
        Check(secondResult == ConversionResult::ConversionOK, __LINE__);
        Check(source == inputEnd, __LINE__);
        Check(secondTarget == secondBuffer + 5, __LINE__);
        Check(std::string(secondBuffer, secondTarget) == "\xF0\x9F\x98\x80" "z", __LINE__);
        Check(std::string(firstBuffer, firstTarget) + std::string(secondBuffer, secondTarget) == "a\xF0\x9F\x98\x80" "z", __LINE__);
    }

    const std::u16string leadingInput{char16_t(0xD83D), char16_t(0xDE00), u'z'};
    const char16_t* const leadingEnd = leadingInput.data() + leadingInput.size();
    for (std::size_t remaining = 0; remaining < 4; ++remaining) {
        char firstBuffer[8];
        std::memset(firstBuffer, '#', sizeof(firstBuffer));
        const char16_t* source = leadingInput.data();
        char* firstTarget = firstBuffer;
        const auto firstResult = _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, leadingEnd, &firstTarget, firstBuffer + remaining, strict);
        Check(firstResult == ConversionResult::TargetExhausted, __LINE__);
        Check(source == leadingInput.data(), __LINE__);
        Check(firstTarget == firstBuffer, __LINE__);
        for (char byte : firstBuffer) {
            Check(byte == '#', __LINE__);
        }

        char secondBuffer[8];
        std::memset(secondBuffer, '#', sizeof(secondBuffer));
        char* secondTarget = secondBuffer;
        const auto secondResult = _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, leadingEnd, &secondTarget, secondBuffer + sizeof(secondBuffer), strict);
        Check(secondResult == ConversionResult::ConversionOK, __LINE__);
        Check(source == leadingEnd, __LINE__);
        Check(std::string(secondBuffer, secondTarget) == "\xF0\x9F\x98\x80" "z", __LINE__);
    }

    const std::u16string chainedInput{u'a', char16_t(0xD83D), char16_t(0xDE00), char16_t(0xD83D), char16_t(0xDE01), u'z'};
    const char16_t* const chainedEnd = chainedInput.data() + chainedInput.size();
    char firstChunk[8];
    std::memset(firstChunk, '#', sizeof(firstChunk));
    const char16_t* source = chainedInput.data();
    char* firstTarget = firstChunk;
    Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, chainedEnd, &firstTarget, firstChunk + 4, strict) == ConversionResult::TargetExhausted, __LINE__);
    Check(source == chainedInput.data() + 1, __LINE__);
    Check(std::string(firstChunk, firstTarget) == "a", __LINE__);

    char secondChunk[8];
    std::memset(secondChunk, '#', sizeof(secondChunk));
    char* secondTarget = secondChunk;
    Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, chainedEnd, &secondTarget, secondChunk + 7, strict) == ConversionResult::TargetExhausted, __LINE__);
    Check(source == chainedInput.data() + 3, __LINE__);
    Check(std::string(secondChunk, secondTarget) == "\xF0\x9F\x98\x80", __LINE__);

    char thirdChunk[8];
    std::memset(thirdChunk, '#', sizeof(thirdChunk));
    char* thirdTarget = thirdChunk;
    Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, chainedEnd, &thirdTarget, thirdChunk + sizeof(thirdChunk), strict) == ConversionResult::ConversionOK, __LINE__);
    Check(source == chainedEnd, __LINE__);
    Check(std::string(thirdChunk, thirdTarget) == "\xF0\x9F\x98\x81" "z", __LINE__);
    Check(std::string(firstChunk, firstTarget) + std::string(secondChunk, secondTarget) + std::string(thirdChunk, thirdTarget) == "a\xF0\x9F\x98\x80\xF0\x9F\x98\x81" "z", __LINE__);
}

void TestUtf16ToUtf8SurrogateResume() {
    TestUtf16ToUtf8SurrogateResumeForMode(true);
    TestUtf16ToUtf8SurrogateResumeForMode(false);

    const std::u16string unpairedInput{u'a', char16_t(0xD800), u'z'};
    const char16_t* const unpairedEnd = unpairedInput.data() + unpairedInput.size();
    for (std::size_t remaining = 0; remaining < 3; ++remaining) {
        char firstBuffer[8];
        std::memset(firstBuffer, '#', sizeof(firstBuffer));
        const char16_t* source = unpairedInput.data();
        char* firstTarget = firstBuffer;
        Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, unpairedEnd, &firstTarget, firstBuffer + 1 + remaining, false) == ConversionResult::TargetExhausted, __LINE__);
        Check(source == unpairedInput.data() + 1, __LINE__);
        Check(std::string(firstBuffer, firstTarget) == "a", __LINE__);

        char secondBuffer[8];
        std::memset(secondBuffer, '#', sizeof(secondBuffer));
        char* secondTarget = secondBuffer;
        Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, unpairedEnd, &secondTarget, secondBuffer + sizeof(secondBuffer), false) == ConversionResult::ConversionOK, __LINE__);
        Check(source == unpairedEnd, __LINE__);
        Check(std::string(firstBuffer, firstTarget) + std::string(secondBuffer, secondTarget) == "a\xEF\xBF\xBD" "z", __LINE__);
    }
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
    TestUtf16ToUtf8SurrogateResume();
    TestLatin1ToUtf8();
    return 0;
}
