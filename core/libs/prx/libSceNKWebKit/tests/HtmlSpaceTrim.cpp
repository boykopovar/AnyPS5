#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::String;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HTML space trim assertion failed at line %u\n", location.line());
        std::abort();
    }
}

String Trim(const String& input) {
    return _ZN7WebCore33stripLeadingAndTrailingHTMLSpacesERKN3WTF6StringE(input);
}

template<typename Character> String Create(std::basic_string_view<Character> text) {
    if constexpr (sizeof(Character) == 1)
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
    else
        return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

template<typename Character> void Check(std::basic_string_view<Character> text, std::basic_string_view<Character> expected) {
    auto input = Create(text);
    const unsigned originalReferences = input.impl->refCount;
    {
        auto result = Trim(input);
        Require(result.impl != nullptr);
        Require(result.impl->length == expected.size());
        if (!expected.empty()) {
            Require(!!(result.impl->hashAndFlags & 4) == (sizeof(Character) == 1));
            auto* data = static_cast<const Character*>(result.impl->data);
            Require(std::basic_string_view<Character>(data, result.impl->length) == expected);
            if (text == expected) {
                Require(result.impl == input.impl);
                Require(input.impl->refCount == originalReferences + 2);
            } else {
                Require(result.impl != input.impl);
                Require(input.impl->refCount == originalReferences);
            }
        } else {
            auto empty = Create(std::basic_string_view<Character>{});
            Require(result.impl == empty.impl);
        }
    }
    Require(input.impl->refCount == originalReferences);
}

template<typename Character> void TestWidth() {
    auto convert = [](std::u16string_view text) {
        return std::basic_string<Character>(text.begin(), text.end());
    };
    auto check = [&](std::u16string_view input, std::u16string_view expected) {
        const auto text = convert(input);
        const auto result = convert(expected);
        Check<Character>(text, result);
    };
    check(u"", u"");
    check(u"\t\n\f\r ", u"");
    check(u"a", u"a");
    check(u"one \t two", u"one \t two");
    check(u"\t\n\f\r x \t\n\f\r", u"x");
    check(u"  leading", u"leading");
    check(u"trailing\n\r", u"trailing");
    check(u"\v", u"\v");
    check(u"\u00a0", u"\u00a0");
    check(u" \vtext\v ", u"\vtext\v");
    check(u"\t\u00a0text\u00a0\n", u"\u00a0text\u00a0");
    check(std::u16string_view(u" \0 ", 3), std::u16string_view(u"\0", 1));
    for (char16_t character = 0; character < 256; ++character) {
        const bool space = character == '\t' || character == '\n' || character == '\f' || character == '\r' || character == ' ';
        const char16_t text[]{character, 'x', character};
        check(std::u16string_view(text, 3), space ? u"x" : std::u16string_view(text, 3));
    }
}

}  // namespace

int main() {
    static_assert(!std::is_trivially_destructible_v<String>);
    try {
        String null;
        Require(Trim(null).impl == nullptr);
        NKWebKit::WtfAbi::StringImpl emptyImpl{2, 0, nullptr, 0};
        {
            String empty(&emptyImpl);
            auto result = Trim(empty);
            auto canonical = Create(std::u16string_view{});
            Require(result.impl == canonical.impl);
            Require(result.impl != &emptyImpl);
            Require(emptyImpl.refCount == 2);
            empty.impl = nullptr;
        }
        TestWidth<unsigned char>();
        TestWidth<char16_t>();
        Check<char16_t>(u" \u3000\u1234\t", u"\u3000\u1234");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
