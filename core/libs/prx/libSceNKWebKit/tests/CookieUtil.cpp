#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string>
#include <string_view>

namespace {

using namespace NKWebKit::WtfAbi;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Cookie path check failed at line %u\n", location.line());
        std::abort();
    }
}

void Text(const String& value, std::u16string_view expected) {
    Require(value.impl && value.impl->length == expected.size());
    for (unsigned i = 0; i < expected.size(); ++i) {
        char16_t character = value.impl->hashAndFlags & 4
            ? static_cast<const unsigned char*>(value.impl->data)[i]
            : static_cast<const char16_t*>(value.impl->data)[i];
        Require(character == expected[i]);
    }
}

void Check(std::u16string_view path, std::u16string_view expected, bool wide, bool valid = true) {
    std::u16string text = u"custom:";
    text += path;
    unsigned pathEnd = text.size();
    text += u"?query/ignored#fragment/ignored";
    std::string narrow(text.begin(), text.end());
    auto input = wide ? _ZN3WTF10StringImpl6createEPKDsj(text.data(), text.size())
        : _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(narrow.data()), narrow.size());
    auto lastSlash = path.rfind('/');
    unsigned afterLastSlash = lastSlash == std::u16string_view::npos ? 7 : 8 + lastSlash;
    URL url{input.impl, (6U << 6) | (valid ? 1U : 0U), 7, 7, 7, 7, afterLastSlash, pathEnd, pathEnd + 14};
    unsigned originalRefs = input.impl->refCount;
    auto retained = input;
    Require(input.impl->refCount == originalRefs + 2);
    {
        auto result = _ZN7WebCore10CookieUtil17defaultPathForURLERKN3WTF3URLE(&url);
        Text(result, expected);
        Require(result.impl != input.impl);
        Require(input.impl->refCount == originalRefs + 2);
        bool fallback = !valid || path.empty() || path.front() != '/' || path.rfind('/') == 0;
        if (fallback) {
            Require(result.impl->hashAndFlags & 4);
            Require(result.impl->refCount == 2);
            Require((result.impl->hashAndFlags & 3) == 0);
            auto again = _ZN7WebCore10CookieUtil17defaultPathForURLERKN3WTF3URLE(&url);
            Require(again.impl != result.impl);
            Require(again.impl->data == result.impl->data);
        } else {
            Require(!!(result.impl->hashAndFlags & 4) == !wide);
            Require(result.impl->refCount == 2);
            {
                auto copy = result;
                Require(result.impl->refCount == 4);
            }
            Require(result.impl->refCount == 2);
            auto again = _ZN7WebCore10CookieUtil17defaultPathForURLERKN3WTF3URLE(&url);
            Require(again.impl != result.impl);
        }
    }
    Require(input.impl->refCount == originalRefs + 2);
    retained = String();
    Require(input.impl->refCount == originalRefs);
}

void Run() {
    URL empty{};
    Text(_ZN7WebCore10CookieUtil17defaultPathForURLERKN3WTF3URLE(&empty), u"/");
    for (bool wide : {false, true}) {
        Check(u"", u"/", wide);
        Check(u"/", u"/", wide);
        Check(u"/file", u"/", wide);
        Check(u"/a/b", u"/a", wide);
        Check(u"/a/b/", u"/a/b", wide);
        Check(u"//x", u"/", wide);
        Check(u"a/b", u"/", wide);
        Check(u"/invalid/path", u"/", wide, false);
        Check(u"/abcdefghijklmnop/file", u"/abcdefghijklmnop", wide);
    }
    Check(u"/\u03a9/\U0001f600/file", u"/\u03a9/\U0001f600", true);
}

}  // namespace

int main() {
    try {
        Run();
        std::puts("Cookie default path checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Cookie path exception: %s\n", error.what());
        return 1;
    }
}
