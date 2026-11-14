#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WtfUrlEscape check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static String Encode(const String& input) {
    return _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(input);
}

static String Latin1(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static String UTF16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

static void Expect(const String& input, std::string_view expected) {
    auto result = Encode(input);
    Require(result.impl);
    Require(result.impl->length == expected.size());
    for (unsigned i = 0; i < result.impl->length; ++i) {
        unsigned unit = result.impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(result.impl->data)[i] : static_cast<const char16_t*>(result.impl->data)[i];
        Require(unit == static_cast<unsigned char>(expected[i]));
    }
}

static void Shared(const String& input) {
    unsigned refs = input.impl ? input.impl->refCount : 0;
    {
        auto result = Encode(input);
        Require(result.impl == input.impl);
        if (input.impl) Require(input.impl->refCount == refs + 2);
    }
    if (input.impl) Require(input.impl->refCount == refs);
}

int main() {
    Shared(String());
    Shared(Latin1(""));
    Shared(Latin1("abcAZ09!$%&'()*+,-._~"));
    Shared(UTF16(u"unchanged%"));
    Expect(Latin1("a b/@%"), "a%20b%2F%40%");
    constexpr std::string_view safe = "!$%&'()*+,-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~";
    constexpr char hex[] = "0123456789ABCDEF";
    std::string all;
    std::string expected;
    for (unsigned c = 0; c < 128; ++c) {
        all.push_back(static_cast<char>(c));
        std::string encoded;
        if (safe.find(static_cast<char>(c)) != std::string_view::npos) encoded.push_back(static_cast<char>(c));
        else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 15]);
        }
        Expect(Latin1(std::string(1, static_cast<char>(c))), encoded);
        Expect(UTF16(std::u16string(1, static_cast<char16_t>(c))), encoded);
        expected += encoded;
    }
    Expect(Latin1(all), expected);
    Expect(Latin1(std::string("\x80\xA3\xE9\xFF", 4)), "%C2%80%C2%A3%C3%A9%C3%BF");
    Expect(UTF16(u"\u00E9\u4E2D\U0001F600"), "%C3%A9%E4%B8%AD%F0%9F%98%80");
    const char16_t unpaired[] = {0xD800, 'x', 0xDC00, 0xD801};
    Expect(UTF16({unpaired, 4}), "%EF%BF%BDx%EF%BF%BD%ED%A0%81");
    const char16_t high[] = {0xDBFF};
    Expect(UTF16({high, 1}), "%ED%AF%BF");
    const char16_t low[] = {0xDFFF};
    Expect(UTF16({low, 1}), "%EF%BF%BD");
    std::puts("WtfUrlEscape passed");
}
