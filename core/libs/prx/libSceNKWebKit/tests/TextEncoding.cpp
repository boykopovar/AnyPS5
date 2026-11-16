#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>

using WebCore::TextEncoding;
using WebCore::UnencodableHandling;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::Vector;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "TextEncoding check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Construct(TextEncoding& encoding, const char* name) {
    _ZN7WebCore12TextEncodingC1EPKc(&encoding, name);
}

static void Construct(TextEncoding& encoding, const String& name) {
    _ZN7WebCore12TextEncodingC1ERKN3WTF6StringE(&encoding, &name);
}

static String Decode(const TextEncoding& encoding, std::string_view bytes, bool stopOnError, bool& sawError) {
    return _ZNK7WebCore12TextEncoding6decodeEPKcmbRb(&encoding, bytes.data(), bytes.size(), stopOnError, &sawError);
}

static StringView ViewOf(const String& string) {
    return string.impl ? StringView{string.impl->data, string.impl->length, !!(string.impl->hashAndFlags & 4)} : StringView{nullptr, 0, true};
}

static Vector<std::uint8_t> Encode(const TextEncoding& encoding, const String& string, UnencodableHandling handling) {
    return _ZNK7WebCore12TextEncoding6encodeEN3WTF10StringViewENS_19UnencodableHandlingE(&encoding, ViewOf(string), handling);
}

static Vector<std::uint8_t> Encode(const TextEncoding& encoding, StringView string, UnencodableHandling handling) {
    return _ZNK7WebCore12TextEncoding6encodeEN3WTF10StringViewENS_19UnencodableHandlingE(&encoding, string, handling);
}

static String Latin1(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static String UTF16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

static void ExpectString(const String& result, std::u16string_view expected, bool expectedIs8Bit) {
    Require(result.impl);
    Require(result.impl->length == expected.size());
    Require(!!(result.impl->hashAndFlags & 4) == expectedIs8Bit);
    for (unsigned i = 0; i < result.impl->length; ++i) {
        auto unit = result.impl->hashAndFlags & 4
            ? static_cast<char16_t>(static_cast<const unsigned char*>(result.impl->data)[i])
            : static_cast<const char16_t*>(result.impl->data)[i];
        Require(unit == expected[i]);
    }
}

static void ExpectDecode(const char* name, std::string_view bytes, std::u16string_view expected, bool expectedIs8Bit) {
    TextEncoding encoding;
    Construct(encoding, name);
    bool sawError = false;
    auto result = Decode(encoding, bytes, false, sawError);
    ExpectString(result, expected, expectedIs8Bit);
    Require(!sawError);
}

static void ExpectBytes(const Vector<std::uint8_t>& bytes, std::initializer_list<unsigned> expected) {
    Require(bytes.size == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) Require(bytes.buffer[i] == *(expected.begin() + i));
}

static void RequireThrowsDecode(const char* name, std::string_view bytes) {
    TextEncoding encoding;
    Construct(encoding, name);
    bool sawError = false;
    bool threw = false;
    try {
        Decode(encoding, bytes, false, sawError);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    {
        TextEncoding encoding;
        Construct(encoding, "UTF-8");
        Require(encoding.name && !std::strcmp(encoding.name, "UTF-8"));
        Require(encoding.backslashAsCurrencySymbol == '\\');
        auto domName = _ZNK7WebCore12TextEncoding7domNameEv(&encoding);
        Require(domName && !std::strcmp(domName, "UTF-8"));

        TextEncoding alias;
        Construct(alias, "utf8");
        Require(alias.name == encoding.name);

        TextEncoding shiftJis;
        Construct(shiftJis, "Shift_JIS");
        Require(shiftJis.backslashAsCurrencySymbol == 0x00A5);
        auto shiftJisDomName = _ZNK7WebCore12TextEncoding7domNameEv(&shiftJis);
        Require(shiftJisDomName && !std::strcmp(shiftJisDomName, "Shift_JIS"));

        TextEncoding windows949;
        Construct(windows949, "windows-949");
        auto windows949DomName = _ZNK7WebCore12TextEncoding7domNameEv(&windows949);
        Require(windows949DomName && !std::strcmp(windows949DomName, "EUC-KR"));
        TextEncoding eucKr;
        Construct(eucKr, "EUC-KR");
        auto eucKrDomName = _ZNK7WebCore12TextEncoding7domNameEv(&eucKr);
        Require(eucKrDomName && !std::strcmp(eucKrDomName, "EUC-KR"));

        TextEncoding iso885911;
        Construct(iso885911, "iso-8859-11");
        Require(iso885911.name && !std::strcmp(iso885911.name, "windows-874"));

        TextEncoding blocklisted;
        Construct(blocklisted, "UTF-7");
        Require(!blocklisted.name);
        TextEncoding empty;
        Construct(empty, "");
        Require(!empty.name);
        Require(empty.backslashAsCurrencySymbol == '\\');
        TextEncoding nullName;
        Construct(nullName, nullptr);
        Require(!nullName.name);

        TextEncoding registered;
        Construct(registered, "KOI8-R");
        Require(registered.name && !std::strcmp(registered.name, "KOI8-R"));
        RequireThrowsDecode("KOI8-R", "ab");
        RequireThrowsDecode("macintosh", "ab");
        RequireThrowsDecode("windows-1251", "ab");
        RequireThrowsDecode("EUC-TW", "ab");
    }

    {
        TextEncoding eightBit;
        Construct(eightBit, Latin1("utf8"));
        TextEncoding wide;
        Construct(wide, UTF16(u"UTF-8"));
        Require(eightBit.name && eightBit.name == wide.name);

        TextEncoding nonAscii;
        Construct(nonAscii, UTF16(u"utf8\u00e9"));
        Require(!nonAscii.name);

        std::string longName(64, 'x');
        TextEncoding tooLong;
        Construct(tooLong, Latin1(longName));
        Require(!tooLong.name);
        std::string longestName(63, 'x');
        TextEncoding longest;
        Construct(longest, Latin1(longestName));
        Require(!longest.name);

        TextEncoding fromNull;
        Construct(fromNull, String());
        Require(!fromNull.name);

        TextEncoding second;
        String secondName = UTF16(u"utf-8");
        _ZN7WebCore12TextEncodingC2ERKN3WTF6StringE(&second, &secondName);
        Require(second.name && !std::strcmp(second.name, "UTF-8"));
        TextEncoding third;
        _ZN7WebCore12TextEncodingC2EPKc(&third, "windows-1252");
        Require(third.name && !std::strcmp(third.name, "windows-1252"));
    }

    ExpectDecode("UTF-8", "abc", u"abc", true);
    ExpectDecode("UTF-8", "\xC3\xA9", u"\u00E9", true);
    ExpectDecode("UTF-8", "\xE4\xB8\xAD", u"\u4E2D", false);
    ExpectDecode("UTF-8", "\xF0\x9F\x98\x80", u"\U0001F600", false);
    ExpectDecode("UTF-8", "\xEF\xBB\xBF" "a", u"\uFEFFa", false);
    ExpectDecode("windows-1252", "\x80", u"\u20AC", false);
    ExpectDecode("windows-1252", "\x93\x9F", u"\u201C\u0178", false);
    ExpectDecode("windows-1252", "\xFF", u"\u00FF", true);
    ExpectDecode("windows-1252", "a", u"a", true);
    ExpectDecode("Shift_JIS", "\x82\x60", u"\uFF21", false);
    ExpectDecode("Shift_JIS", "\x93\xFA", u"\u65E5", false);
    ExpectDecode("EUC-JP", "\xA1\xA1", u"\u3000", false);
    ExpectDecode("EUC-JP", "\xC6\xFC", u"\u65E5", false);
    ExpectDecode("ISO-2022-JP", "\x1B$B$\x22", u"\u3042", false);
    ExpectDecode("ISO-2022-JP", "\x1B(J\\", u"\u00A5", true);
    ExpectDecode("EUC-KR", "\xB0\xA1", u"\uAC00", false);
    ExpectDecode("Big5", "\xA4\x40", u"\u4E00", false);
    ExpectDecode("gb18030", "\x81\x40", u"\u4E02", false);
    ExpectDecode("gb18030", "\x81\x7E", u"\u4E8A", false);
    ExpectDecode("gb18030", "\x80", u"\u20AC", false);
    ExpectDecode("gb18030", "\x81\x30\x81\x30", u"\u0080", true);
    ExpectDecode("GBK", "\x81\x40", u"\u4E02", false);
    ExpectDecode("x-user-defined", "A", u"A", true);
    ExpectDecode("x-user-defined", "\x80", u"\uF780", false);
    ExpectDecode("x-user-defined", "\xFF", u"\uF7FF", false);
    ExpectDecode("ISO-8859-7", "\xA1\xA4", u"\u2018\u20AC", false);
    ExpectDecode("ISO-8859-8", "\xE0", u"\u05D0", false);
    ExpectDecode("windows-874", "\x80\xA1", u"\u20AC\u0E01", false);
    ExpectDecode("IBM866", "\x80", u"\u0410", false);
    ExpectDecode("KOI8-U", "\x80", u"\u2500", false);
    ExpectDecode("UTF-16LE", std::string_view("a\0" "b\0", 4), u"ab", true);
    ExpectDecode("UTF-16", std::string_view("a\0" "b\0", 4), u"ab", true);
    ExpectDecode("UTF-16BE", std::string_view("\0a\0b", 4), u"ab", true);

    {
        TextEncoding encoding;
        Construct(encoding, "ISO-8859-7");
        bool sawError = false;
        auto result = Decode(encoding, "\xFF", false, sawError);
        ExpectString(result, u"\uFFFD", false);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, "\x41\xFF\x42", false, sawError);
        ExpectString(result, u"A\uFFFDB", false);
        Require(sawError);
    }

    {
        TextEncoding encoding;
        Construct(encoding, "UTF-8");
        bool sawError = false;
        auto result = Decode(encoding, "\xC3\x28", false, sawError);
        ExpectString(result, u"\uFFFD(", false);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, "\xC3\x28", true, sawError);
        ExpectString(result, u"", true);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, "\xE4\xB8", false, sawError);
        ExpectString(result, u"\uFFFD", false);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, "\xC0\x80", false, sawError);
        ExpectString(result, u"\uFFFD\uFFFD", false);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, "ab", false, sawError);
        ExpectString(result, u"ab", true);
        Require(!sawError);
    }

    {
        TextEncoding encoding;
        Construct(encoding, "UTF-8");
        bool sawError = false;
        auto result = Decode(encoding, "A\xC3", true, sawError);
        ExpectString(result, u"A", false);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, "\xC3", true, sawError);
        ExpectString(result, u"", true);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, "A\xE4\xB8", true, sawError);
        ExpectString(result, u"A", false);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, "A\xC3", false, sawError);
        ExpectString(result, u"A\uFFFD", false);
        Require(sawError);
    }

    {
        TextEncoding replacement;
        Construct(replacement, "replacement");
        bool sawError = false;
        auto result = Decode(replacement, "abc", false, sawError);
        ExpectString(result, u"\uFFFD", false);
        Require(sawError);
    }

    {
        TextEncoding invalid;
        Construct(invalid, "UTF-7");
        bool sawError = true;
        auto result = Decode(invalid, "ab", false, sawError);
        Require(!result.impl);
        Require(sawError);
    }

    {
        TextEncoding encoding;
        Construct(encoding, "UTF-8");
        bool sawError = false;
        bool threw = false;
        try {
            Decode(encoding, std::string_view(nullptr, 2), false, sawError);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
    }

    {
        TextEncoding encoding;
        Construct(encoding, "UTF-8");
        bool ignored = false;
        auto result = Decode(encoding, "", false, ignored);
        Require(result.impl && !result.impl->length);
    }

    {
        TextEncoding encoding;
        Construct(encoding, "UTF-8");
        auto bytes = Encode(encoding, Latin1("abc"), UnencodableHandling::URLEncodedEntities);
        Require(bytes.buffer && bytes.capacity == 9 && bytes.size == 3);
        ExpectBytes(bytes, {'a', 'b', 'c'});
        bytes = Encode(encoding, UTF16(u"ab"), UnencodableHandling::Entities);
        Require(bytes.capacity == 6 && bytes.size == 2);
        ExpectBytes(bytes, {'a', 'b'});
        bytes = Encode(encoding, UTF16(u"\u00E9"), UnencodableHandling::Entities);
        Require(bytes.capacity == 3 && bytes.size == 2);
        ExpectBytes(bytes, {0xC3, 0xA9});
        bytes = Encode(encoding, UTF16(u"\u4E2D"), UnencodableHandling::Entities);
        Require(bytes.size == 3);
        ExpectBytes(bytes, {0xE4, 0xB8, 0xAD});
        bytes = Encode(encoding, UTF16(u"\U0001F600"), UnencodableHandling::Entities);
        Require(bytes.size == 4);
        ExpectBytes(bytes, {0xF0, 0x9F, 0x98, 0x80});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "windows-1252");
        auto bytes = Encode(encoding, Latin1("abc"), UnencodableHandling::Entities);
        Require(bytes.capacity == 3 && bytes.size == 3);
        ExpectBytes(bytes, {'a', 'b', 'c'});
        bytes = Encode(encoding, UTF16(u"\u00E9\u201A"), UnencodableHandling::Entities);
        Require(bytes.size == 2);
        ExpectBytes(bytes, {0xE9, 0x82});
        bytes = Encode(encoding, UTF16(u"\u201C"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x93});
        bytes = Encode(encoding, UTF16(u"\u06DE"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {'&', '#', '1', '7', '5', '8', ';'});
        Require(bytes.capacity == 16);
        bytes = Encode(encoding, UTF16(u"\u06DE"), UnencodableHandling::URLEncodedEntities);
        ExpectBytes(bytes, {'%', '2', '6', '%', '2', '3', '1', '7', '5', '8', '%', '3', 'B'});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "UTF-16LE");
        auto bytes = Encode(encoding, Latin1("ab"), UnencodableHandling::Entities);
        Require(bytes.capacity == 4 && bytes.size == 4);
        ExpectBytes(bytes, {'a', 0, 'b', 0});
        TextEncoding bigEndian;
        Construct(bigEndian, "UTF-16BE");
        bytes = Encode(bigEndian, Latin1("ab"), UnencodableHandling::Entities);
        Require(bytes.capacity == 4 && bytes.size == 4);
        ExpectBytes(bytes, {0, 'a', 0, 'b'});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "ISO-8859-7");
        auto bytes = Encode(encoding, UTF16(u"\u0391"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0xC1});
        bytes = Encode(encoding, UTF16(u"\u20AC"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0xA4});
        bytes = Encode(encoding, UTF16(u"\u00E9"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {'&', '#', '2', '3', '3', ';'});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "x-user-defined");
        auto bytes = Encode(encoding, UTF16(u"\uF780"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x80});
        bytes = Encode(encoding, UTF16(u"\u00E9"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {'&', '#', '2', '3', '3', ';'});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "Shift_JIS");
        auto bytes = Encode(encoding, UTF16(u"\u65E5"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x93, 0xFA});
        bytes = Encode(encoding, Latin1("abc"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {'a', 'b', 'c'});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "ISO-2022-JP");
        auto bytes = Encode(encoding, Latin1("abc"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {'a', 'b', 'c'});
        bytes = Encode(encoding, UTF16(u"\u00A5"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x1B, 0x28, 0x4A, 0x5C, 0x1B, 0x28, 0x42});
        bytes = Encode(encoding, UTF16(u"\u3042"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x1B, 0x24, 0x42, 0x24, 0x22, 0x1B, 0x28, 0x42});
        bytes = Encode(encoding, UTF16(u"a\u3042"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {'a', 0x1B, 0x24, 0x42, 0x24, 0x22, 0x1B, 0x28, 0x42});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "EUC-KR");
        auto bytes = Encode(encoding, UTF16(u"\uAC00"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0xB0, 0xA1});
        bytes = Encode(encoding, UTF16(u"\u06DE"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {'&', '#', '1', '7', '5', '8', ';'});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "GBK");
        auto bytes = Encode(encoding, UTF16(u"\u20AC"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x80});
        bytes = Encode(encoding, UTF16(u"\u06DE"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {'&', '#', '1', '7', '5', '8', ';'});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "gb18030");
        auto bytes = Encode(encoding, UTF16(u"\u4E02"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x81, 0x40});
        bytes = Encode(encoding, UTF16(u"\u00A5"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x81, 0x30, 0x84, 0x36});
        bytes = Encode(encoding, UTF16(u"\u4E02"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0x81, 0x40});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "Big5");
        auto bytes = Encode(encoding, UTF16(u"\u4E00"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0xA4, 0x40});
    }

    {
        TextEncoding encoding;
        Construct(encoding, "windows-1252");
        auto bytes = Encode(encoding, UTF16(u"e\u0301"), UnencodableHandling::Entities);
        ExpectBytes(bytes, {0xE9});
        bytes = Encode(encoding, UTF16(u"e\u0301"), UnencodableHandling::URLEncodedEntities);
        ExpectBytes(bytes, {0xE9});
    }

    {
        TextEncoding invalid;
        Construct(invalid, "UTF-7");
        auto bytes = Encode(invalid, Latin1("abc"), UnencodableHandling::Entities);
        Require(!bytes.buffer && !bytes.capacity && !bytes.size);
        TextEncoding empty;
        Construct(empty, "UTF-8");
        bytes = Encode(empty, Latin1(""), UnencodableHandling::Entities);
        Require(!bytes.buffer && !bytes.capacity && !bytes.size);
        bytes = Encode(empty, StringView{nullptr, 0, false}, UnencodableHandling::Entities);
        Require(!bytes.buffer && !bytes.capacity && !bytes.size);
    }

    {
        TextEncoding encoding;
        Construct(encoding, "windows-1252");
        using EncodeForURLParsing = Vector<std::uint8_t> (APS5_VABI*)(const void*, StringView);
        auto slot = reinterpret_cast<const void* const*>(encoding.vptr);
        auto encodeForURLParsing = reinterpret_cast<EncodeForURLParsing>(slot[0]);
        auto bytes = encodeForURLParsing(&encoding, StringView{u"e\u0301", 2, false});
        std::string expected = "e%26%23769%3B";
        Require(bytes.size == expected.size());
        for (unsigned i = 0; i < bytes.size; ++i) Require(bytes.buffer[i] == static_cast<unsigned char>(expected[i]));
    }

    {
        TextEncoding encoding;
        Construct(encoding, "ISO-8859-7");
        bool sawError = false;
        auto result = Decode(encoding, "\x41\xFF\x42", true, sawError);
        ExpectString(result, u"A\uFFFD", false);
        Require(sawError);
    }

    {
        TextEncoding encoding;
        Construct(encoding, "UTF-16LE");
        bool sawError = false;
        auto result = Decode(encoding, std::string_view("a\0b\0c", 5), false, sawError);
        ExpectString(result, u"ab\uFFFD", false);
        Require(sawError);
        sawError = false;
        result = Decode(encoding, std::string_view("a\0b\0c", 5), true, sawError);
        ExpectString(result, u"ab\uFFFD", false);
        Require(sawError);
    }

    {
        TextEncoding unknown;
        Construct(unknown, "UTF-7");
        Require(!_ZNK7WebCore12TextEncoding7domNameEv(&unknown));
    }

    {
        bool sawError = false;
        bool threw = false;
        try {
            (void)_ZNK7WebCore12TextEncoding6decodeEPKcmbRb(nullptr, "a", 1, false, &sawError);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            (void)_ZNK7WebCore12TextEncoding6decodeEPKcmbRb(nullptr, "a", 1, false, nullptr);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            (void)_ZNK7WebCore12TextEncoding6encodeEN3WTF10StringViewENS_19UnencodableHandlingE(nullptr, StringView{"a", 1, true}, UnencodableHandling::Entities);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            (void)_ZNK7WebCore12TextEncoding7domNameEv(nullptr);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            (void)_ZN7WebCore12TextEncodingC1EPKc(nullptr, "UTF-8");
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            (void)_ZN7WebCore12TextEncodingC1ERKN3WTF6StringE(nullptr, nullptr);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
    }

    {
        TextEncoding encoding;
        Construct(encoding, "UTF-8");
        bool sawError = false;
        bool threw = false;
        try {
            (void)_ZNK7WebCore12TextEncoding6encodeEN3WTF10StringViewENS_19UnencodableHandlingE(&encoding, StringView{nullptr, 2, true}, UnencodableHandling::Entities);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        sawError = false;
        auto result = Decode(encoding, "", false, sawError);
        Require(result.impl && !result.impl->length);
        Require(!sawError);
    }

    std::puts("TextEncoding passed");
}
