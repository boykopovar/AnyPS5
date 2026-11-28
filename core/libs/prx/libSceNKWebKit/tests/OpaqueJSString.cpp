#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "OpaqueJSString check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static String Latin1(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static String UTF16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

static void Expect(const GuestJSString* string, std::u16string_view expected) {
    Require(string);
    Require(!string->nullString);
    Require(string->references.load() == 1);
    Require(JSStringGetLength_nid_postfix(string) == expected.size());
    const auto* characters = JSStringGetCharactersPtr_nid_postfix(string);
    for (std::size_t i = 0; i < expected.size(); ++i) Require(characters[i] == expected[i]);
}

int main() {
    {
        String null;
        Require(!_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(&null).ptr);
        Require(!_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(&null).ptr);
    }
    {
        String empty = Latin1("");
        auto result = _ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(&empty);
        Expect(result.ptr, u"");
    }
    {
        String latin1 = Latin1("caf\xe9");
        auto result = _ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(&latin1);
        Expect(result.ptr, u"café");
        Require(latin1.impl && latin1.impl->length == 4);
    }
    {
        String wide = UTF16(u"あ\U0001F600");
        auto result = _ZN14OpaqueJSString9tryCreateEON3WTF6StringE(&wide);
        Expect(result.ptr, u"あ\U0001F600");
        Require(!wide.impl);
        JSStringRetain_nid_postfix(result.ptr);
        JSStringRelease_nid_postfix(result.ptr);
        Require(result.ptr->references.load() == 1);
    }
    bool threw = false;
    try {
        _ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    return 0;
}
