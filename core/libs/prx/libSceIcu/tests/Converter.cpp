#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;
constexpr std::int32_t Latin1Type = 3;
constexpr std::int32_t Utf8Type = 4;
constexpr std::int32_t Utf16BigEndianType = 5;
constexpr std::int32_t Utf32Type = 30;
constexpr std::int32_t UsAsciiType = 26;
constexpr std::int32_t IbmPlatform = 0;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s returned %lld, expected %lld\n", what, static_cast<long long>(actual), static_cast<long long>(expected));
    std::abort();
}

void ExpectBytes(const char* actual, const char* expected, std::size_t length, const char* what) {
    Expect(std::memcmp(actual, expected, length) == 0, what);
}

template <typename Exception, typename Function>
void ExpectThrow(Function function, const char* what) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    Expect(false, what);
}

Icu::Converter* Open(const char* name) {
    std::int32_t error = Success;
    auto* converter = ucnv_open_nid_postfix(name, &error);
    ExpectInt(error, Success, name);
    Expect(converter != nullptr, name);
    return converter;
}

void TestOpen() {
    auto* utf8 = Open("utf8");
    std::int32_t error = Success;
    Expect(std::strcmp(ucnv_getName_nid_postfix(utf8, &error), "UTF-8") == 0, "ucnv_getName(utf8)");
    ExpectInt(ucnv_getType_nid_postfix(utf8), Utf8Type, "ucnv_getType(UTF-8)");
    ExpectInt(ucnv_getMaxCharSize_nid_postfix(utf8), 3, "ucnv_getMaxCharSize(UTF-8)");
    ExpectInt(ucnv_getMinCharSize_nid_postfix(utf8), 1, "ucnv_getMinCharSize(UTF-8)");
    ExpectInt(ucnv_isFixedWidth_nid_postfix(utf8, &error), 0, "ucnv_isFixedWidth(UTF-8)");
    ExpectInt(ucnv_isAmbiguous_nid_postfix(utf8), 0, "ucnv_isAmbiguous(UTF-8)");
    ExpectInt(ucnv_getCCSID_nid_postfix(utf8, &error), 1208, "ucnv_getCCSID(UTF-8)");
    ExpectInt(ucnv_getPlatform_nid_postfix(utf8, &error), IbmPlatform, "ucnv_getPlatform(UTF-8)");
    ExpectInt(error, Success, "UTF-8 queries status");
    ExpectInt(ucnv_usesFallback_nid_postfix(utf8), 0, "ucnv_usesFallback default");
    ucnv_setFallback_nid_postfix(utf8, 1);
    ExpectInt(ucnv_usesFallback_nid_postfix(utf8), 1, "ucnv_usesFallback after set");
    std::int8_t starters[256] = {};
    ucnv_getStarters_nid_postfix(utf8, starters, &error);
    ExpectInt(error, IllegalArgument, "ucnv_getStarters(UTF-8)");
    ucnv_close_nid_postfix(utf8);

    auto* latin1 = Open("ISO-8859-1");
    error = Success;
    Expect(std::strcmp(ucnv_getName_nid_postfix(latin1, &error), "ISO-8859-1") == 0, "ucnv_getName(ISO-8859-1)");
    ExpectInt(ucnv_getType_nid_postfix(latin1), Latin1Type, "ucnv_getType(ISO-8859-1)");
    ExpectInt(ucnv_getCCSID_nid_postfix(latin1, &error), 819, "ucnv_getCCSID(ISO-8859-1)");
    ucnv_close_nid_postfix(latin1);

    auto* ascii = Open("US-ASCII");
    ExpectInt(ucnv_getType_nid_postfix(ascii), UsAsciiType, "ucnv_getType(US-ASCII)");
    ExpectInt(ucnv_isFixedWidth_nid_postfix(ascii, &error), 1, "ucnv_isFixedWidth(US-ASCII)");
    ucnv_close_nid_postfix(ascii);

    auto* utf32 = Open("UTF-32");
    ExpectInt(ucnv_getType_nid_postfix(utf32), Utf32Type, "ucnv_getType(UTF-32)");
    ExpectInt(ucnv_isFixedWidth_nid_postfix(utf32, &error), 1, "ucnv_isFixedWidth(UTF-32)");
    ucnv_close_nid_postfix(utf32);

    error = Success;
    auto* wide = ucnv_openU_nid_postfix(u"UTF-16BE", &error);
    ExpectInt(error, Success, "ucnv_openU status");
    Expect(std::strcmp(ucnv_getName_nid_postfix(wide, &error), "UTF-16BE") == 0, "ucnv_getName(openU)");
    ExpectInt(ucnv_getType_nid_postfix(wide), Utf16BigEndianType, "ucnv_getType(UTF-16BE)");
    ucnv_close_nid_postfix(wide);

    error = Success;
    Expect(ucnv_open_nid_postfix("", &error) == nullptr, "ucnv_open(empty)");
    ExpectInt(error, IllegalArgument, "ucnv_open(empty) status");
    error = Success;
    const std::string longName(60, 'a');
    Expect(ucnv_open_nid_postfix(longName.c_str(), &error) == nullptr, "ucnv_open(long)");
    ExpectInt(error, IllegalArgument, "ucnv_open(long) status");
    error = Success;
    const std::u16string longWide(60, u'a');
    Expect(ucnv_openU_nid_postfix(longWide.c_str(), &error) == nullptr, "ucnv_openU(long)");
    ExpectInt(error, IllegalArgument, "ucnv_openU(long) status");
    error = BufferOverflow;
    Expect(ucnv_open_nid_postfix("UTF-8", &error) == nullptr, "ucnv_open(failed status)");
    ExpectInt(error, BufferOverflow, "ucnv_open(failed status) keeps status");
    Expect(ucnv_open_nid_postfix(nullptr, &error) == nullptr, "ucnv_open(null, failed status)");
    Expect(ucnv_open_nid_postfix("UTF-8", nullptr) == nullptr, "ucnv_open(null status)");
    ucnv_close_nid_postfix(nullptr);
    ucnv_reset_nid_postfix(nullptr);
}

void TestUnsupported() {
    std::int32_t error = Success;
    ExpectThrow<std::runtime_error>([&] { ucnv_open_nid_postfix("Shift_JIS", &error); }, "ucnv_open(Shift_JIS)");
    error = Success;
    ExpectThrow<std::runtime_error>([&] { ucnv_open_nid_postfix(nullptr, &error); }, "ucnv_open(default)");
    error = Success;
    ExpectThrow<std::runtime_error>([&] { ucnv_openU_nid_postfix(nullptr, &error); }, "ucnv_openU(default)");
    error = Success;
    ExpectThrow<std::runtime_error>([&] { ucnv_openU_nid_postfix(u"UTF-\u00e9", &error); }, "ucnv_openU(non-ASCII)");
    error = Success;
    ExpectThrow<std::runtime_error>([&] { ucnv_open_nid_postfix("latin1", &error); }, "ucnv_open(alias)");
    auto* utf7 = Open("UTF-7");
    error = Success;
    ExpectThrow<std::runtime_error>([&] { ucnv_getCCSID_nid_postfix(utf7, &error); }, "ucnv_getCCSID(UTF-7)");
    ucnv_close_nid_postfix(utf7);
    char target[8] = {};
    error = Success;
    ExpectThrow<std::runtime_error>([&] { ucnv_convert_nid_postfix("UTF-8", "Shift_JIS", target, 8, "a", 1, &error); },
                                    "ucnv_convert(Shift_JIS)");
    error = Success;
    ExpectThrow<std::runtime_error>([&] { ucnv_convert_nid_postfix(nullptr, "UTF-8", target, 8, "a", 1, &error); },
                                    "ucnv_convert(default target)");
    ExpectThrow<std::invalid_argument>([] { ucnv_getMaxCharSize_nid_postfix(nullptr); }, "ucnv_getMaxCharSize(null)");
    ExpectThrow<std::invalid_argument>([] { ucnv_getName_nid_postfix(nullptr, nullptr); }, "ucnv_getName(null status)");
}

void TestToUChars() {
    auto* utf8 = Open("UTF-8");
    const char text[] = "h\xc3\xa9\xe2\x82\xac";
    char16_t buffer[8] = {};
    std::int32_t error = Success;
    ExpectInt(ucnv_toUChars_nid_postfix(utf8, buffer, 8, text, -1, &error), 3, "ucnv_toUChars length");
    ExpectInt(error, Success, "ucnv_toUChars status");
    Expect(buffer[0] == u'h' && buffer[1] == 0x00e9 && buffer[2] == 0x20ac && buffer[3] == 0, "ucnv_toUChars output");
    error = Success;
    ExpectInt(ucnv_toUChars_nid_postfix(utf8, nullptr, 0, text, 6, &error), 3, "ucnv_toUChars preflight");
    ExpectInt(error, BufferOverflow, "ucnv_toUChars preflight status");
    char16_t exact[3] = {};
    error = Success;
    ExpectInt(ucnv_toUChars_nid_postfix(utf8, exact, 3, text, -1, &error), 3, "ucnv_toUChars exact");
    ExpectInt(error, NotTerminated, "ucnv_toUChars exact status");
    char16_t small[2] = {};
    error = Success;
    ExpectInt(ucnv_toUChars_nid_postfix(utf8, small, 2, text, -1, &error), 3, "ucnv_toUChars overflow");
    ExpectInt(error, BufferOverflow, "ucnv_toUChars overflow status");
    Expect(small[0] == u'h' && small[1] == 0x00e9, "ucnv_toUChars overflow prefix");
    error = Success;
    ExpectInt(ucnv_toUChars_nid_postfix(utf8, buffer, 8, "a\xff" "b", 3, &error), 3, "ucnv_toUChars invalid");
    ExpectInt(error, Success, "ucnv_toUChars invalid status");
    Expect(buffer[0] == u'a' && buffer[1] == 0xfffd && buffer[2] == u'b', "ucnv_toUChars substitution");
    char invalid[4] = {};
    std::int8_t invalidLength = 4;
    ucnv_getInvalidChars_nid_postfix(utf8, invalid, &invalidLength, &error);
    ExpectInt(invalidLength, 0, "ucnv_getInvalidChars after flush");
    const char bad[] = "a\xff";
    const char* badCursor = bad;
    char16_t* unitCursor = buffer;
    ucnv_toUnicode_nid_postfix(utf8, &unitCursor, buffer + 8, &badCursor, bad + 2, nullptr, 0, &error);
    ExpectInt(error, Success, "ucnv_toUnicode invalid status");
    invalidLength = 4;
    ucnv_getInvalidChars_nid_postfix(utf8, invalid, &invalidLength, &error);
    ExpectInt(error, Success, "ucnv_getInvalidChars status");
    ExpectInt(invalidLength, 1, "ucnv_getInvalidChars length");
    ExpectInt(static_cast<unsigned char>(invalid[0]), 0xff, "ucnv_getInvalidChars byte");
    invalidLength = 0;
    ucnv_getInvalidChars_nid_postfix(utf8, invalid, &invalidLength, &error);
    ExpectInt(error, IndexOutOfBounds, "ucnv_getInvalidChars small");
    error = Success;
    ExpectInt(ucnv_toUChars_nid_postfix(utf8, buffer, -1, text, -1, &error), 0, "ucnv_toUChars negative capacity");
    ExpectInt(error, IllegalArgument, "ucnv_toUChars negative capacity status");
    error = Success;
    ExpectInt(ucnv_toUChars_nid_postfix(nullptr, buffer, 8, text, -1, &error), 0, "ucnv_toUChars null converter");
    ExpectInt(error, IllegalArgument, "ucnv_toUChars null converter status");
    ucnv_close_nid_postfix(utf8);

    auto* utf16 = Open("UTF-16");
    error = Success;
    ExpectInt(ucnv_toUChars_nid_postfix(utf16, buffer, 8, "\xff\xfe\x41\x00", 4, &error), 1, "ucnv_toUChars(UTF-16 LE BOM)");
    Expect(buffer[0] == u'A' && buffer[1] == 0, "ucnv_toUChars(UTF-16 LE BOM) output");
    ucnv_close_nid_postfix(utf16);
}

void TestFromUChars() {
    auto* utf8 = Open("UTF-8");
    char buffer[16] = {};
    std::int32_t error = Success;
    ExpectInt(ucnv_fromUChars_nid_postfix(utf8, buffer, 16, u"\U0001F600", -1, &error), 4, "ucnv_fromUChars supplementary");
    ExpectInt(error, Success, "ucnv_fromUChars supplementary status");
    ExpectBytes(buffer, "\xf0\x9f\x98\x80", 5, "ucnv_fromUChars supplementary bytes");
    const char16_t lone[] = {u'a', 0xd800, u'b', 0};
    error = Success;
    ExpectInt(ucnv_fromUChars_nid_postfix(utf8, buffer, 16, lone, 3, &error), 5, "ucnv_fromUChars lone surrogate");
    ExpectBytes(buffer, "a\xef\xbf\xbd" "b", 6, "ucnv_fromUChars substitution");
    const char16_t trail[] = {0xdc00, u'a'};
    const char16_t* trailCursor = trail;
    char* byteCursor = buffer;
    error = Success;
    ucnv_fromUnicode_nid_postfix(utf8, &byteCursor, buffer + 16, &trailCursor, trail + 2, nullptr, 0, &error);
    ExpectInt(error, Success, "ucnv_fromUnicode invalid status");
    ExpectInt(byteCursor - buffer, 4, "ucnv_fromUnicode invalid length");
    char16_t invalid[4] = {};
    std::int8_t invalidLength = 4;
    ucnv_getInvalidUChars_nid_postfix(utf8, invalid, &invalidLength, &error);
    ExpectInt(error, Success, "ucnv_getInvalidUChars status");
    ExpectInt(invalidLength, 1, "ucnv_getInvalidUChars length");
    ExpectInt(invalid[0], 0xdc00, "ucnv_getInvalidUChars unit");
    ucnv_getInvalidUChars_nid_postfix(utf8, nullptr, &invalidLength, &error);
    ExpectInt(error, IllegalArgument, "ucnv_getInvalidUChars null buffer");
    error = Success;
    ExpectInt(ucnv_fromUChars_nid_postfix(utf8, nullptr, 0, u"\u20ac", -1, &error), 3, "ucnv_fromUChars preflight");
    ExpectInt(error, BufferOverflow, "ucnv_fromUChars preflight status");
    error = Success;
    ExpectInt(ucnv_fromUChars_nid_postfix(utf8, buffer, 16, u"", -1, &error), 0, "ucnv_fromUChars empty");
    ExpectInt(error, Success, "ucnv_fromUChars empty status");
    ucnv_close_nid_postfix(utf8);

    auto* utf16 = Open("UTF-16");
    error = Success;
    ExpectInt(ucnv_fromUChars_nid_postfix(utf16, buffer, 16, u"A", 1, &error), 4, "ucnv_fromUChars(UTF-16)");
    ExpectBytes(buffer, "\xff\xfe\x41\x00", 4, "ucnv_fromUChars(UTF-16) BOM");
    ucnv_close_nid_postfix(utf16);
}

void TestSubstitution() {
    auto* latin1 = Open("ISO-8859-1");
    char bytes[4] = {};
    std::int8_t length = 4;
    std::int32_t error = Success;
    ucnv_getSubstChars_nid_postfix(latin1, bytes, &length, &error);
    ExpectInt(error, Success, "ucnv_getSubstChars status");
    ExpectInt(length, 1, "ucnv_getSubstChars length");
    ExpectInt(bytes[0], 0x1a, "ucnv_getSubstChars byte");
    char buffer[8] = {};
    ExpectInt(ucnv_fromUChars_nid_postfix(latin1, buffer, 8, u"\u00e9\u20ac", 2, &error), 2, "latin1 default substitution");
    ExpectBytes(buffer, "\xe9\x1a", 3, "latin1 default substitution bytes");
    ucnv_setSubstChars_nid_postfix(latin1, "?", 1, &error);
    ExpectInt(error, Success, "ucnv_setSubstChars status");
    ExpectInt(ucnv_fromUChars_nid_postfix(latin1, buffer, 8, u"\u20ac", 1, &error), 1, "latin1 custom substitution");
    ExpectInt(buffer[0], '?', "latin1 custom substitution byte");
    ucnv_setSubstChars_nid_postfix(latin1, "??", 2, &error);
    ExpectInt(error, IllegalArgument, "ucnv_setSubstChars too long");
    error = Success;
    length = 0;
    ucnv_getSubstChars_nid_postfix(latin1, bytes, &length, &error);
    ExpectInt(error, IndexOutOfBounds, "ucnv_getSubstChars small");
    error = Success;
    ucnv_setSubstString_nid_postfix(latin1, u"<>", -1, &error);
    ExpectInt(error, Success, "ucnv_setSubstString status");
    ExpectInt(ucnv_fromUChars_nid_postfix(latin1, buffer, 8, u"a\u20ac", 2, &error), 3, "latin1 string substitution");
    ExpectBytes(buffer, "a<>", 4, "latin1 string substitution bytes");
    ucnv_setSubstString_nid_postfix(latin1, u"\u20ac", 1, &error);
    Expect(error > 0, "ucnv_setSubstString unmappable");
    ucnv_close_nid_postfix(latin1);
}

void TestStreaming() {
    auto* utf8 = Open("UTF-8");
    const char16_t source[] = u"abc";
    const char16_t* sourceCursor = source;
    char target[2] = {};
    char* targetCursor = target;
    std::int32_t error = Success;
    ucnv_fromUnicode_nid_postfix(utf8, &targetCursor, target + 2, &sourceCursor, source + 3, nullptr, 1, &error);
    ExpectInt(error, BufferOverflow, "ucnv_fromUnicode overflow");
    ExpectInt(targetCursor - target, 2, "ucnv_fromUnicode target advance");
    ExpectInt(sourceCursor - source, 2, "ucnv_fromUnicode source advance");

    ucnv_resetFromUnicode_nid_postfix(utf8);
    const char16_t lead[] = {0xd83d};
    sourceCursor = lead;
    char wide[8] = {};
    targetCursor = wide;
    error = Success;
    ucnv_fromUnicode_nid_postfix(utf8, &targetCursor, wide + 8, &sourceCursor, lead + 1, nullptr, 0, &error);
    ExpectInt(error, Success, "ucnv_fromUnicode partial");
    ExpectInt(targetCursor - wide, 0, "ucnv_fromUnicode partial output");
    ExpectInt(ucnv_fromUCountPending_nid_postfix(utf8, &error), 1, "ucnv_fromUCountPending");
    const char16_t trail[] = {0xde00};
    sourceCursor = trail;
    ucnv_fromUnicode_nid_postfix(utf8, &targetCursor, wide + 8, &sourceCursor, trail + 1, nullptr, 1, &error);
    ExpectInt(error, Success, "ucnv_fromUnicode completion");
    ExpectInt(targetCursor - wide, 4, "ucnv_fromUnicode completion length");
    ExpectBytes(wide, "\xf0\x9f\x98\x80", 4, "ucnv_fromUnicode completion bytes");

    const char partial[] = "\xe2\x82";
    const char* byteCursor = partial;
    char16_t units[4] = {};
    char16_t* unitCursor = units;
    ucnv_toUnicode_nid_postfix(utf8, &unitCursor, units + 4, &byteCursor, partial + 2, nullptr, 0, &error);
    ExpectInt(error, Success, "ucnv_toUnicode partial");
    ExpectInt(unitCursor - units, 0, "ucnv_toUnicode partial output");
    ExpectInt(ucnv_toUCountPending_nid_postfix(utf8, &error), 2, "ucnv_toUCountPending");
    const char rest[] = "\xac" "A";
    byteCursor = rest;
    ucnv_toUnicode_nid_postfix(utf8, &unitCursor, units + 4, &byteCursor, rest + 2, nullptr, 1, &error);
    ExpectInt(error, Success, "ucnv_toUnicode completion");
    ExpectInt(unitCursor - units, 2, "ucnv_toUnicode completion length");
    Expect(units[0] == 0x20ac && units[1] == u'A', "ucnv_toUnicode completion units");
    byteCursor = partial;
    ucnv_toUnicode_nid_postfix(utf8, &unitCursor, units + 4, &byteCursor, partial + 2, nullptr, 0, &error);
    ucnv_resetToUnicode_nid_postfix(utf8);
    ExpectInt(ucnv_toUCountPending_nid_postfix(utf8, &error), 0, "ucnv_toUCountPending after reset");

    const char whole[] = "A\xe2\x82\xac" "B";
    byteCursor = whole;
    unitCursor = units;
    std::int32_t offsets[4] = {-9, -9, -9, -9};
    ucnv_toUnicode_nid_postfix(utf8, &unitCursor, units + 4, &byteCursor, whole + 5, offsets, 1, &error);
    ExpectInt(error, Success, "ucnv_toUnicode offsets status");
    ExpectInt(unitCursor - units, 3, "ucnv_toUnicode offsets length");
    Expect(offsets[0] == 0 && offsets[1] == 1 && offsets[2] == 4 && offsets[3] == -9, "ucnv_toUnicode offsets");

    const char sequence[] = "\xf0\x9f\x98\x80" "A";
    byteCursor = sequence;
    ExpectInt(ucnv_getNextUChar_nid_postfix(utf8, &byteCursor, sequence + 5, &error), 0x1f600, "ucnv_getNextUChar supplementary");
    ExpectInt(ucnv_getNextUChar_nid_postfix(utf8, &byteCursor, sequence + 5, &error), u'A', "ucnv_getNextUChar ASCII");
    ExpectInt(error, Success, "ucnv_getNextUChar status");
    ExpectInt(ucnv_getNextUChar_nid_postfix(utf8, &byteCursor, sequence + 5, &error), 0xffff, "ucnv_getNextUChar end");
    ExpectInt(error, IndexOutOfBounds, "ucnv_getNextUChar end status");
    ucnv_close_nid_postfix(utf8);
}

void TestConvert() {
    char target[16] = {};
    std::int32_t error = Success;
    ExpectInt(ucnv_convert_nid_postfix("UTF-16BE", "UTF-8", target, 16, "A\xe2\x82\xac", -1, &error), 4, "ucnv_convert length");
    ExpectInt(error, Success, "ucnv_convert status");
    ExpectBytes(target, "\x00\x41\x20\xac\x00", 5, "ucnv_convert bytes");
    error = Success;
    ExpectInt(ucnv_convert_nid_postfix("UTF-16BE", "UTF-8", nullptr, 0, "A\xe2\x82\xac", -1, &error), 4, "ucnv_convert preflight");
    ExpectInt(error, BufferOverflow, "ucnv_convert preflight status");
    error = Success;
    ExpectInt(ucnv_convert_nid_postfix(nullptr, nullptr, nullptr, 0, "", -1, &error), 0, "ucnv_convert empty");
    ExpectInt(error, NotTerminated, "ucnv_convert empty status");
    error = Success;
    ExpectInt(ucnv_convert_nid_postfix(nullptr, "", target, 16, "a", 1, &error), 0, "ucnv_convert bad source name");
    ExpectInt(error, IllegalArgument, "ucnv_convert bad source name status");

    auto* utf8 = Open("UTF-8");
    error = Success;
    ExpectInt(ucnv_fromAlgorithmic_nid_postfix(utf8, Utf16BigEndianType, target, 16, "\x00\x41\x20\xac", 4, &error), 4,
              "ucnv_fromAlgorithmic length");
    ExpectBytes(target, "A\xe2\x82\xac\x00", 5, "ucnv_fromAlgorithmic bytes");
    error = Success;
    ExpectInt(ucnv_toAlgorithmic_nid_postfix(Utf16BigEndianType, utf8, target, 16, "A", 1, &error), 2, "ucnv_toAlgorithmic length");
    ExpectBytes(target, "\x00\x41\x00", 3, "ucnv_toAlgorithmic bytes");
    error = Success;
    ExpectInt(ucnv_fromAlgorithmic_nid_postfix(utf8, 99, target, 16, "A", 1, &error), 0, "ucnv_fromAlgorithmic bad type");
    ExpectInt(error, IllegalArgument, "ucnv_fromAlgorithmic bad type status");
    error = Success;
    ExpectInt(ucnv_toAlgorithmic_nid_postfix(-1, utf8, target, 16, "A", 1, &error), 0, "ucnv_toAlgorithmic bad type");
    ExpectInt(error, IllegalArgument, "ucnv_toAlgorithmic bad type status");

    auto* utf16 = Open("UTF-16LE");
    const char source[] = "A\xe2\x82\xac";
    const char* sourceCursor = source;
    char output[8] = {};
    char* outputCursor = output;
    error = Success;
    ucnv_convertEx_nid_postfix(utf16, utf8, &outputCursor, output + 8, &sourceCursor, source + 4, nullptr, nullptr, nullptr, nullptr, 1, 1,
                               &error);
    ExpectInt(error, Success, "ucnv_convertEx status");
    ExpectInt(outputCursor - output, 4, "ucnv_convertEx length");
    ExpectBytes(output, "\x41\x00\xac\x20", 4, "ucnv_convertEx bytes");
    char16_t pivot[4] = {};
    char16_t* pivotSource = pivot;
    char16_t* pivotTarget = pivot;
    sourceCursor = source;
    outputCursor = output;
    ucnv_convertEx_nid_postfix(utf16, utf8, &outputCursor, output + 8, &sourceCursor, source + 4, pivot, &pivotSource, &pivotTarget, pivot,
                               1, 1, &error);
    ExpectInt(error, IllegalArgument, "ucnv_convertEx empty pivot");
    ucnv_close_nid_postfix(utf16);
    ucnv_close_nid_postfix(utf8);
}

}

int main() {
    TestOpen();
    TestUnsupported();
    TestToUChars();
    TestFromUChars();
    TestSubstitution();
    TestStreaming();
    TestConvert();
    return 0;
}
