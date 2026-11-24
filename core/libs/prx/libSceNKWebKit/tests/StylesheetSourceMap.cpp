#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "StylesheetSourceMap check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

String Latin1(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

String Utf16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

bool Is8Bit(const StringImpl* impl) {
    return (impl->hashAndFlags & 4) != 0;
}

std::u16string Units(const StringImpl* impl) {
    std::u16string result;
    for (unsigned i = 0; i < impl->length; ++i) {
        result.push_back(Is8Bit(impl) ? static_cast<const unsigned char*>(impl->data)[i] : static_cast<const char16_t*>(impl->data)[i]);
    }
    return result;
}

String Find(const String& content) {
    return _ZN9Inspector22ContentSearchUtilities26findStylesheetSourceMapURLERKN3WTF6StringE(&content);
}

bool Finds(const String& content, std::u16string_view expected) {
    unsigned before = content.impl ? content.impl->refCount : 0;
    String result = Find(content);
    if (content.impl) Require(content.impl->refCount == before);
    return result.impl && Units(result.impl) == expected;
}

bool FindsNothing(const String& content) {
    return Find(content).impl == nullptr;
}

std::u16string Widen(std::string_view text) {
    return std::u16string(text.begin(), text.end());
}

void TestMatches() {
    Require(Finds(Latin1("body { color: red; }\n/*# sourceMappingURL=style.css.map */\n"), u"style.css.map"));
    Require(Finds(Latin1("/*@\tsourceMappingURL=\t \tlegacy.map\t */"), u"legacy.map"));
    Require(Finds(Latin1("/*# sourceMappingURL=data:application/json;base64,eyJ2IjozfQ==*/"), u"data:application/json;base64,eyJ2IjozfQ=="));
    Require(Finds(Latin1("/*# sourceMappingURL=a*/b*/c"), u"a*/b"));
    Require(Finds(Latin1("/*# sourceMappingURL=first.map */ /*# sourceMappingURL=second.map */"), u"first.map"));
    Require(Finds(Latin1("/*# sourceMappingURL=bad value */ /*# sourceMappingURL=good.map */"), u"good.map"));
    Require(Finds(Latin1("/*/*# sourceMappingURL=nested.map */"), u"nested.map"));
    Require(Finds(Latin1("x/*# sourceMappingURL=line1\n*/ /*# sourceMappingURL=line2 */"), u"line2"));

    String whole = Latin1("/*# sourceMappingURL=a.map */");
    String result = Find(whole);
    Require(result.impl && result.impl != whole.impl && Is8Bit(result.impl));
    Require(result.impl->refCount == 2);

    String empty = Find(Latin1("/*# sourceMappingURL= */"));
    Require(empty.impl && empty.impl->length == 0);
    Require(empty.impl == NKWebKit::WtfAbi::EmptyStringImpl());
}

void TestMismatches() {
    Require(FindsNothing(String()));
    Require(FindsNothing(Latin1("")));
    Require(FindsNothing(Latin1("body { color: red; }")));
    Require(FindsNothing(Latin1("/*#  sourceMappingURL=two-spaces.map */")));
    Require(FindsNothing(Latin1("/*#sourceMappingURL=no-space.map */")));
    Require(FindsNothing(Latin1("/*! sourceMappingURL=wrong-marker.map */")));
    Require(FindsNothing(Latin1("/*# sourcemappingurl=lowercase.map */")));
    Require(FindsNothing(Latin1("/*# sourceMappingURL='quoted.map' */")));
    Require(FindsNothing(Latin1("/*# sourceMappingURL=\"quoted.map\" */")));
    Require(FindsNothing(Latin1("/*# sourceMappingURL=line.map\n*/")));
    Require(FindsNothing(Latin1("/*# sourceMappingURL=unterminated.map")));
    Require(FindsNothing(Latin1("//# sourceMappingURL=script-style.map")));
    Require(FindsNothing(Latin1("/*# sourceMappingURL=a.map * /")));
    Require(FindsNothing(Latin1("/*#")));
}

void TestCharacterClasses() {
    Require(FindsNothing(Latin1("/*# sourceMappingURL=a\xA0*/")));
    Require(Finds(Latin1("/*# sourceMappingURL=a\x85\xE9*/"), u"a\u0085é"));
    Require(FindsNothing(Latin1("/*# sourceMappingURL=a\v*/")));
    Require(FindsNothing(Latin1("/*# sourceMappingURL=a\f*/")));

    for (char16_t space : {u' ', u' ', u' ', u' ', u' ', u' ', u' ', u' ', u' ', u'　', u'﻿'}) {
        std::u16string text = u"/*# sourceMappingURL=a";
        text.push_back(space);
        text += u"*/";
        Require(FindsNothing(Utf16(text)));
    }
    for (char16_t other : {u'᠎', u'​', u'\u0085', static_cast<char16_t>(0xD83D), u'￿'}) {
        std::u16string text = u"/*# sourceMappingURL=a";
        text.push_back(other);
        text += u"*/";
        std::u16string expected = u"a";
        expected.push_back(other);
        Require(Finds(Utf16(text), expected));
    }

    String wide = Utf16(u"p{}/*# sourceMappingURL=été.map */");
    String result = Find(wide);
    Require(result.impl && !Is8Bit(result.impl));
    Require(Units(result.impl) == u"été.map");

    Require(Finds(Utf16(Widen("/*@ sourceMappingURL=ascii-in-utf16.map*/")), u"ascii-in-utf16.map"));
    Require(FindsNothing(Utf16(u"/*＃ sourceMappingURL=fullwidth.map */")));
}

void TestNullArgument() {
    bool threw = false;
    try {
        _ZN9Inspector22ContentSearchUtilities26findStylesheetSourceMapURLERKN3WTF6StringE(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    TestMatches();
    TestMismatches();
    TestCharacterClasses();
    TestNullArgument();
    return 0;
}
