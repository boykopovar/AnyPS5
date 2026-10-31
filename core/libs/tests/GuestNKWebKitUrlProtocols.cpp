#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

void Check(bool result, int line) {
    if (!result) {
        std::fprintf(stderr, "WTF URL protocol test failed at line %d\n", line);
        std::abort();
    }
}

#define Require(result) Check((result), __LINE__)

static_assert(sizeof(NKWebKit::StringView) == 16);
static_assert(offsetof(NKWebKit::StringView, data) == 0);
static_assert(offsetof(NKWebKit::StringView, length) == 8);
static_assert(offsetof(NKWebKit::StringView, is8Bit) == 12);

bool JavaScript(NKWebKit::StringView value) {
    return _ZN3WTF20protocolIsJavaScriptENS_10StringViewE(value);
}

bool HttpFamily(NKWebKit::StringView value) {
    return _ZN3WTF22protocolIsInHTTPFamilyENS_10StringViewE(value);
}

template<typename TCharacter>
NKWebKit::StringView View(std::basic_string_view<TCharacter> text) {
    return {text.data(), static_cast<std::uint32_t>(text.size()), sizeof(TCharacter) == 1};
}

void Sample(std::string_view text, bool javascript, bool http) {
    const auto wide = std::u16string(text.begin(), text.end());
    Require(JavaScript(View(text)) == javascript);
    Require(HttpFamily(View(text)) == http);
    Require(JavaScript(View(std::u16string_view(wide))) == javascript);
    Require(HttpFamily(View(std::u16string_view(wide))) == http);
}

void SourceGoldens() {
    Sample("", false, false);
    Sample(" ", false, false);
    Sample("javascript:", true, false);
    Sample("JaVaScRiPt:alert(1)", true, false);
    Sample("\x01\x02 \tJa\nVa\rScRiPt:alert(1)", true, false);
    Sample("javascript:\nhttps://a.test", true, false);
    Sample("JAVASCRIPT:", true, false);
    Sample("javascript", false, false);
    Sample("javascriptx:", false, false);
    Sample("java\vscript:", false, false);
    Sample("\x7fjavascript:", false, false);
    Sample("javascript\t:\nnext", true, false);
    Sample("java\tscript:\rnext", true, false);
    Sample("http:", false, true);
    Sample("hTtP:example", false, true);
    Sample("https:", false, true);
    Sample("HtTpS://example.com", false, true);
    Sample("HTTP://example", false, true);
    Sample("http", false, false);
    Sample("https", false, false);
    Sample("httpx:", false, false);
    Sample("httpsx:", false, false);
    Sample("\thttp:", false, false);
    Sample("http\n:", false, false);
    Sample("httpS:\nnext", false, true);
    Sample("https://javascript:", false, true);
    Sample(" javascript:", true, false);
}

void WideCharacters() {
    Require(!JavaScript(View(std::u16string_view(u"java\u00e9script:"))));
    Require(!JavaScript(View(std::u16string_view(u"javascript\u00e9:"))));
    Require(!JavaScript(View(std::u16string_view(u"\uffffjavascript:"))));
    Require(!HttpFamily(View(std::u16string_view(u"ht\u00e9p:"))));
    Require(!HttpFamily(View(std::u16string_view(u"http\u00e9:"))));
    Require(JavaScript(View(std::u16string_view(u"\u001f\u0020Ja\nVaScRiPt:foo\u2603"))));
    Require(HttpFamily(View(std::u16string_view(u"HTTPS:\u2603"))));
    const std::string latin1 = std::string("http") + static_cast<char>(0xe9) + ":";
    Require(!HttpFamily(View(std::string_view(latin1))));
}

void ExplicitLengths() {
    const char bytes[] = {'j','a','v','a','s','c','r','i','p','t',':','x'};
    Require(JavaScript({bytes, 11, true}));
    Require(JavaScript({bytes, 12, true}));
    Require(!JavaScript({bytes, 10, true}));
    const char16_t wide[] = {u'h',u't',u't',u'p',u's',u':',u'x'};
    Require(HttpFamily({wide, 6, false}));
    Require(!HttpFamily({wide, 5, false}));
    Require(!HttpFamily({wide, 4, false}));
    Require(!JavaScript({nullptr, 0, false}));
    Require(!HttpFamily({nullptr, 0, true}));
}

#ifndef _WIN32
template<typename TFunction>
bool InvalidArgument(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void InvalidArguments() {
    Require(InvalidArgument([] { JavaScript({nullptr, 1, true}); }));
    Require(InvalidArgument([] { JavaScript({nullptr, 2, false}); }));
    Require(InvalidArgument([] { HttpFamily({nullptr, 6, true}); }));
    Require(InvalidArgument([] { HttpFamily({nullptr, 1, false}); }));
}
#endif

}

int main() {
    SourceGoldens();
    WideCharacters();
    ExplicitLengths();
#ifndef _WIN32
    InvalidArguments();
#endif
}
