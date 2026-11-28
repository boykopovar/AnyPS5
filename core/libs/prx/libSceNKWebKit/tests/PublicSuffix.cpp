#include "prx/libSceNKWebKit/include/PublicSuffixExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using SuffixQuery = bool (APS5_VABI*)(const String*);
static_assert(std::is_same_v<decltype(&_ZN7WebCore14isPublicSuffixERKN3WTF6StringE), SuffixQuery>);
using DomainQuery = String (APS5_VABI*)(const String*);
static_assert(std::is_same_v<decltype(&_ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE), DomainQuery>);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "public suffix check failed at line %u\n", location.line());
        std::abort();
    }
}

void Check(std::string_view text, bool expected) {
    const auto string = _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(text.data()), text.size());
    const auto* original = string.impl;
    const auto refs = original ? original->refCount : 0;
    const bool actual = _ZN7WebCore14isPublicSuffixERKN3WTF6StringE(&string);
    if (actual != expected) std::fprintf(stderr, "suffix '%.*s': got %d, expected %d\n", static_cast<int>(text.size()), text.data(), actual, expected);
    Require(actual == expected);
    Require(string.impl == original && (!original || original->refCount == refs));
}

void CheckWide(std::u16string_view text, bool expected) {
    StringImpl impl{3, static_cast<unsigned>(text.size()), text.data(), 0};
    const String string(&impl);
    Require(_ZN7WebCore14isPublicSuffixERKN3WTF6StringE(&string) == expected);
    Require(impl.refCount == 3 && impl.data == text.data());
}

template <typename Text>
bool Is(const String& string, Text expected) {
    if (!string.impl || string.impl->length != expected.size()) return false;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned i = 0; i < view.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, i) != static_cast<char16_t>(expected[i])) return false;
    }
    return true;
}

void CheckTop(std::string_view text, std::optional<std::string_view> expected) {
    StringImpl impl{3, static_cast<unsigned>(text.size()), text.data(), 4};
    const String string(&impl);
    {
        const auto result = _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(&string);
        Require(expected ? Is(result, *expected) : !result.impl);
    }
    Require(impl.refCount == 3 && impl.data == text.data());
}

void CheckTopWide(std::u16string_view text, std::optional<std::string_view> expected) {
    StringImpl impl{3, static_cast<unsigned>(text.size()), text.data(), 0};
    const String string(&impl);
    {
        const auto result = _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(&string);
        Require(expected ? Is(result, *expected) : !result.impl);
    }
    Require(impl.refCount == 3 && impl.data == text.data());
}

template <typename Text>
void CheckTopUnchanged(Text text, bool is8Bit) {
    StringImpl impl{3, static_cast<unsigned>(text.size()), text.data(), is8Bit ? 4u : 0u};
    const String string(&impl);
    {
        const auto result = _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(&string);
        Require(result.impl == &impl && impl.refCount == 5);
    }
    Require(impl.refCount == 3);
}

void CheckTopPrivatelyControlledDomain() {
    CheckTop("www.example.com", "example.com");
    CheckTop("a.b.example.co.uk", "example.co.uk");
    CheckTop("example.com", "example.com");
    CheckTop("WWW.Example.CO.UK", "example.co.uk");
    CheckTop("user.github.io", "user.github.io");
    CheckTop("a.user.github.io", "user.github.io");
    CheckTop("localhost", "localhost");
    CheckTop("LOCALHOST", "localhost");
    CheckTop("..example.com", "example.com");
    CheckTop("..WWW.Example.com", "example.com");
    for (auto text : {"com", "co.uk", "CO.UK", "github.io", "...", ".", ""}) CheckTop(text, std::nullopt);
    CheckTopWide(u"WWW.EXAMPLE.CO.UK", "example.co.uk");
    CheckTopWide(u"localhost", "localhost");
    CheckTopWide(u"co.uk", std::nullopt);
    CheckTopWide(u"..", std::nullopt);
    for (auto text : {std::string_view("192.168.0.1"), std::string_view("::1"), std::string_view("2001:DB8::1"),
             std::string_view("B\xfc" "cher.DE")}) CheckTopUnchanged(text, true);
    for (auto text : {std::u16string_view(u"192.168.0.1"), std::u16string_view(u"FE80::1"),
             std::u16string_view(u"пример.РФ")}) CheckTopUnchanged(text, false);
    String nullString;
    Require(!_ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(&nullString).impl);
    StringImpl empty{3, 0, nullptr, 4};
    const String emptyString(&empty);
    Require(!_ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(&emptyString).impl);
    StringImpl unreadable{3, 4, nullptr, 4};
    const String unreadableString(&unreadable);
    for (const String* argument : {static_cast<const String*>(nullptr), &unreadableString}) {
        bool threw = false;
        try { _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(argument); }
        catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    }
}

}

int main() {
    const std::array<std::string_view, 15> suffixes{
        "com", "COM", "Co.Uk", "com.ar", "cc.ar.us", ".cc.ar.us", "xxx.ck",
        "blogspot.com", "github.io", "compute.amazonaws.com", "x.compute.amazonaws.com",
        "xn--p1ai", "\xd1\x80\xd1\x84", "\xd0\xa0\xd0\xa4", "\xe5\x95\x86\xe6\xa0\x87"};
    const std::array<std::string_view, 15> domains{
        "", "example.com", "www.com.ar", "www.cc.ar.us", "www.ck", "abc.www.ck", "www.xxx.ck",
        "example.blogspot.com", "example.github.io", "x.y.compute.amazonaws.com",
        "an-unlisted-tld-for-anyps5", "localhost", ".", "..com", "www.xn--p1ai"};
    for (auto text : suffixes) Check(text, true);
    for (auto text : domains) Check(text, false);
    for (const auto text : {u"com", u"COM", u"cO.Uk", u"blogspot.com", u"\u0420\u0424", u"\u5546\u6807"}) CheckWide(text, true);
    for (const auto text : {u"", u"www.ck", u"example.com", u"localhost", u"\U0001f600"}) CheckWide(text, false);
    const std::array<char16_t, 1> surrogate{0xd800};
    CheckWide({surrogate.data(), surrogate.size()}, false);
    Check(std::string_view("com\0.example", 12), true);
    CheckWide(std::u16string_view(u"com\0.example", 12), true);
    String nullString;
    Require(!_ZN7WebCore14isPublicSuffixERKN3WTF6StringE(&nullString));
    StringImpl empty{3, 0, nullptr, 4};
    const String emptyString(&empty);
    Require(!_ZN7WebCore14isPublicSuffixERKN3WTF6StringE(&emptyString));
    bool threw = false;
    try { _ZN7WebCore14isPublicSuffixERKN3WTF6StringE(nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    CheckTopPrivatelyControlledDomain();
    std::puts("public suffix: ICANN/private, wildcard/exception, Unicode, UTF-16, no-star and registrable-domain cases passed");
}
