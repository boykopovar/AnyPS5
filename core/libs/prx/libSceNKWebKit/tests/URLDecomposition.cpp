#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::URLDecomposition;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

struct FakeDecomposition {
    URLDecomposition base;
    URL url;
    int calls;
};

void APS5_VABI Unused(URLDecomposition*) {
    std::abort();
}

void APS5_VABI FullURL(URL* result, const URLDecomposition* self) {
    auto* fake = reinterpret_cast<FakeDecomposition*>(const_cast<URLDecomposition*>(self));
    ++fake->calls;
    *result = fake->url;
    if (result->impl) result->impl->refCount += 2;
}

const void* const Vtable[] = {reinterpret_cast<const void*>(&Unused), reinterpret_cast<const void*>(&Unused), reinterpret_cast<const void*>(&FullURL), reinterpret_cast<const void*>(&Unused)};

std::string Text(const String& string) {
    Require(string.impl != nullptr);
    std::string result;
    for (unsigned i = 0; i < string.impl->length; ++i) {
        const bool is8Bit = string.impl->hashAndFlags & 4;
        const char16_t unit = is8Bit ? static_cast<const unsigned char*>(string.impl->data)[i] : static_cast<const char16_t*>(string.impl->data)[i];
        Require(unit < 0x80);
        result.push_back(static_cast<char>(unit));
    }
    return result;
}

class Fixture {
public:
    Fixture(String text, unsigned bits, unsigned userStart, unsigned userEnd, unsigned passwordEnd, unsigned hostEnd, unsigned pathAfterLastSlash, unsigned pathEnd, unsigned queryEnd)
        : m_text(std::move(text))
        , m_fake{{Vtable}, {m_text.impl, bits, userStart, userEnd, passwordEnd, hostEnd, pathAfterLastSlash, pathEnd, queryEnd}, 0}
    {
    }

    template<typename Getter> std::string Get(Getter getter) {
        const unsigned refCount = m_text.impl ? m_text.impl->refCount : 0;
        const int calls = m_fake.calls;
        String result = getter(&m_fake.base);
        Require(m_fake.calls == calls + 1);
        Require(!m_text.impl || m_text.impl->refCount == refCount);
        return Text(result);
    }

    template<typename Getter> bool IsNull(Getter getter) {
        String result = getter(&m_fake.base);
        return !result.impl;
    }

private:
    String m_text;
    FakeDecomposition m_fake;
};

String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::char_traits<char>::length(text)));
}

void TestFullURL() {
    Fixture url(Latin1("https://alice:secret@api.example.com:8443/v1/data?key=val#section"), 1U | 2U | (5U << 3) | (5U << 6), 8, 13, 20, 36, 45, 49, 57);
    Require(url.Get(_ZNK7WebCore16URLDecomposition8protocolEv) == "https:");
    Require(url.Get(_ZNK7WebCore16URLDecomposition4hostEv) == "api.example.com:8443");
    Require(url.Get(_ZNK7WebCore16URLDecomposition8hostnameEv) == "api.example.com");
    Require(url.Get(_ZNK7WebCore16URLDecomposition4portEv) == "8443");
    Require(url.Get(_ZNK7WebCore16URLDecomposition8pathnameEv) == "/v1/data");
    Require(url.Get(_ZNK7WebCore16URLDecomposition6searchEv) == "?key=val");
    Require(url.Get(_ZNK7WebCore16URLDecomposition4hashEv) == "#section");
}

void TestEmptyParts() {
    Fixture url(Latin1("http://example.com/a?#"), 1U | 2U | (4U << 6), 7, 7, 7, 18, 19, 20, 21);
    Require(url.Get(_ZNK7WebCore16URLDecomposition8protocolEv) == "http:");
    Require(url.Get(_ZNK7WebCore16URLDecomposition4hostEv) == "example.com");
    Require(url.Get(_ZNK7WebCore16URLDecomposition4portEv).empty());
    Require(url.Get(_ZNK7WebCore16URLDecomposition8pathnameEv) == "/a");
    Require(url.Get(_ZNK7WebCore16URLDecomposition6searchEv).empty());
    Require(url.Get(_ZNK7WebCore16URLDecomposition4hashEv).empty());

    Fixture file(Latin1("file:///etc/hosts"), 1U | (4U << 6), 7, 7, 7, 7, 12, 17, 17);
    Require(file.Get(_ZNK7WebCore16URLDecomposition8hostnameEv).empty());
    Require(file.Get(_ZNK7WebCore16URLDecomposition4hostEv).empty());
    Require(file.Get(_ZNK7WebCore16URLDecomposition8pathnameEv) == "/etc/hosts");
    Require(file.Get(_ZNK7WebCore16URLDecomposition6searchEv).empty());
}

void TestSixteenBitString() {
    const char16_t text[] = u"http://a.test:81/x";
    Fixture url(_ZN3WTF10StringImpl6createEPKDsj(text, 18), 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 13, 17, 18, 18);
    Require(url.Get(_ZNK7WebCore16URLDecomposition8protocolEv) == "http:");
    Require(url.Get(_ZNK7WebCore16URLDecomposition4hostEv) == "a.test:81");
    Require(url.Get(_ZNK7WebCore16URLDecomposition8hostnameEv) == "a.test");
    Require(url.Get(_ZNK7WebCore16URLDecomposition4portEv) == "81");
    Require(url.Get(_ZNK7WebCore16URLDecomposition8pathnameEv) == "/x");
}

void TestJavaScriptAndInvalid() {
    Fixture script(Latin1("javascript:alert(1)"), 1U | 4U | (10U << 6), 11, 11, 11, 11, 11, 19, 19);
    Require(script.Get(_ZNK7WebCore16URLDecomposition8protocolEv) == "javascript:");
    Require(script.Get(_ZNK7WebCore16URLDecomposition8pathnameEv) == "alert(1)");
    Require(script.Get(_ZNK7WebCore16URLDecomposition8hostnameEv).empty());

    Fixture invalid(Latin1("not a url"), 0, 0, 0, 0, 0, 0, 0, 0);
    Require(invalid.Get(_ZNK7WebCore16URLDecomposition8protocolEv) == ":");
    Require(invalid.IsNull(_ZNK7WebCore16URLDecomposition4hostEv));
    Require(invalid.IsNull(_ZNK7WebCore16URLDecomposition8hostnameEv));
    Require(invalid.IsNull(_ZNK7WebCore16URLDecomposition8pathnameEv));
    Require(invalid.Get(_ZNK7WebCore16URLDecomposition4portEv).empty());
    Require(invalid.Get(_ZNK7WebCore16URLDecomposition6searchEv).empty());
    Require(invalid.Get(_ZNK7WebCore16URLDecomposition4hashEv).empty());

    Fixture null(String(), 0, 0, 0, 0, 0, 0, 0, 0);
    Require(null.Get(_ZNK7WebCore16URLDecomposition8protocolEv) == ":");
    Require(null.IsNull(_ZNK7WebCore16URLDecomposition8pathnameEv));
}

void TestNullObjects() {
    URLDecomposition noVtable{nullptr};
    for (auto getter : {_ZNK7WebCore16URLDecomposition8protocolEv, _ZNK7WebCore16URLDecomposition4hostEv, _ZNK7WebCore16URLDecomposition8hostnameEv,
             _ZNK7WebCore16URLDecomposition4portEv, _ZNK7WebCore16URLDecomposition8pathnameEv, _ZNK7WebCore16URLDecomposition6searchEv,
             _ZNK7WebCore16URLDecomposition4hashEv}) {
        ExpectErr<std::invalid_argument>([&] { getter(nullptr); });
        ExpectErr<std::invalid_argument>([&] { getter(&noVtable); });
    }
}

}

int main() {
    TestFullURL();
    TestEmptyParts();
    TestSixteenBitString();
    TestJavaScriptAndInvalid();
    TestNullObjects();
    std::puts("URLDecomposition tests passed");
    return 0;
}
