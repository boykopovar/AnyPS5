#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::URLDecomposition;
using NKWebKit::WebCoreAbi::URLDecompositionVTable;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

void Require(bool condition) {
    if (!condition) std::abort();
}

template <typename F>
void RequireThrows(F&& f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception&) {
        caught = true;
    }
    Require(caught);
}

struct FakeDecomposition {
    URLDecomposition base;
    URL url;
    unsigned calls;
};

URL APS5_VABI FullURLOf(const URLDecomposition* self) {
    auto* fake = reinterpret_cast<FakeDecomposition*>(const_cast<URLDecomposition*>(self));
    ++fake->calls;
    URL url = fake->url;
    if (url.impl) url.impl->refCount += 2;
    return url;
}

const URLDecompositionVTable FakeVTable{nullptr, nullptr, FullURLOf, nullptr};

std::string Text(const String& s) {
    Require(s.impl != nullptr);
    std::string result;
    bool is8 = (s.impl->hashAndFlags & 4) != 0;
    for (unsigned i = 0; i < s.impl->length; ++i) {
        result.push_back(is8 ? static_cast<char>(static_cast<const unsigned char*>(s.impl->data)[i])
                             : static_cast<char>(static_cast<const char16_t*>(s.impl->data)[i]));
    }
    return result;
}

String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

void TestFullUrl() {
    auto string = Latin1("https://user:pw@api.example.com:8443/a/b?x=1#frag");
    Require(string.impl->length == 49);
    FakeDecomposition fake{{&FakeVTable}, {string.impl, 1U | 2U | (5U << 3) | (5U << 6), 8, 12, 15, 31, 39, 40, 44}, 0};
    unsigned refCount = string.impl->refCount;
    Require(Text(_ZNK7WebCore16URLDecomposition8protocolEv(&fake.base)) == "https:");
    Require(Text(_ZNK7WebCore16URLDecomposition4hostEv(&fake.base)) == "api.example.com:8443");
    Require(Text(_ZNK7WebCore16URLDecomposition8hostnameEv(&fake.base)) == "api.example.com");
    Require(Text(_ZNK7WebCore16URLDecomposition4portEv(&fake.base)) == "8443");
    Require(Text(_ZNK7WebCore16URLDecomposition8pathnameEv(&fake.base)) == "/a/b");
    Require(Text(_ZNK7WebCore16URLDecomposition6searchEv(&fake.base)) == "?x=1");
    Require(Text(_ZNK7WebCore16URLDecomposition4hashEv(&fake.base)) == "#frag");
    Require(fake.calls == 7);
    Require(string.impl->refCount == refCount);
}

void TestMissingParts() {
    auto string = Latin1("http://example.com/");
    FakeDecomposition fake{{&FakeVTable}, {string.impl, 1U | 2U | (4U << 6), 7, 7, 7, 18, 19, 19, 19}, 0};
    Require(Text(_ZNK7WebCore16URLDecomposition4hostEv(&fake.base)) == "example.com");
    Require(Text(_ZNK7WebCore16URLDecomposition4portEv(&fake.base)).empty());
    Require(Text(_ZNK7WebCore16URLDecomposition6searchEv(&fake.base)).empty());
    Require(Text(_ZNK7WebCore16URLDecomposition4hashEv(&fake.base)).empty());
    Require(Text(_ZNK7WebCore16URLDecomposition8pathnameEv(&fake.base)) == "/");
}

void TestEmptyQueryAndFragment() {
    auto string = Latin1("http://example.com/p?#");
    FakeDecomposition fake{{&FakeVTable}, {string.impl, 1U | 2U | (4U << 6), 7, 7, 7, 18, 19, 20, 21}, 0};
    Require(Text(_ZNK7WebCore16URLDecomposition6searchEv(&fake.base)).empty());
    Require(Text(_ZNK7WebCore16URLDecomposition4hashEv(&fake.base)).empty());
    Require(Text(_ZNK7WebCore16URLDecomposition8pathnameEv(&fake.base)) == "/p");
}

void TestJavaScript() {
    auto string = Latin1("javascript:alert(1)");
    FakeDecomposition fake{{&FakeVTable}, {string.impl, 1U | 4U | (10U << 6), 11, 11, 11, 11, 11, 19, 19}, 0};
    Require(Text(_ZNK7WebCore16URLDecomposition8protocolEv(&fake.base)) == "javascript:");
    Require(Text(_ZNK7WebCore16URLDecomposition8pathnameEv(&fake.base)) == "alert(1)");
}

void TestSixteenBit() {
    const char16_t text[] = u"ws://hé.test:80/x";
    auto string = _ZN3WTF10StringImpl6createEPKDsj(text, 17);
    FakeDecomposition fake{{&FakeVTable}, {string.impl, 1U | (3U << 3) | (2U << 6), 5, 5, 5, 12, 16, 17, 17}, 0};
    auto protocol = _ZNK7WebCore16URLDecomposition8protocolEv(&fake.base);
    Require((protocol.impl->hashAndFlags & 4) == 0);
    Require(Text(protocol) == "ws:");
    auto hostname = _ZNK7WebCore16URLDecomposition8hostnameEv(&fake.base);
    Require(hostname.impl->length == 7);
    Require(static_cast<const char16_t*>(hostname.impl->data)[1] == u'é');
    Require(Text(_ZNK7WebCore16URLDecomposition4portEv(&fake.base)) == "80");
}

void TestInvalid() {
    auto string = Latin1("not a url");
    FakeDecomposition fake{{&FakeVTable}, {string.impl, 0, 0, 0, 0, 0, 0, 0, 0}, 0};
    Require(Text(_ZNK7WebCore16URLDecomposition8protocolEv(&fake.base)) == ":");
    Require(!_ZNK7WebCore16URLDecomposition4hostEv(&fake.base).impl);
    Require(!_ZNK7WebCore16URLDecomposition8hostnameEv(&fake.base).impl);
    Require(!_ZNK7WebCore16URLDecomposition8pathnameEv(&fake.base).impl);
    Require(Text(_ZNK7WebCore16URLDecomposition4portEv(&fake.base)).empty());
    Require(Text(_ZNK7WebCore16URLDecomposition6searchEv(&fake.base)).empty());
    Require(Text(_ZNK7WebCore16URLDecomposition4hashEv(&fake.base)).empty());
}

void TestNullThis() {
    RequireThrows([] { _ZNK7WebCore16URLDecomposition8protocolEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore16URLDecomposition4hostEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore16URLDecomposition8hostnameEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore16URLDecomposition4portEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore16URLDecomposition8pathnameEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore16URLDecomposition6searchEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore16URLDecomposition4hashEv(nullptr); });
}

}  // namespace

int main() {
    TestFullUrl();
    TestMissingParts();
    TestEmptyQueryAndFragment();
    TestJavaScript();
    TestSixteenBit();
    TestInvalid();
    TestNullThis();
    return 0;
}
