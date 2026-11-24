#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;
using OriginRef = WebCore::Ref<SecurityOrigin>;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SecurityOriginFromString check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

String Text(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

std::string Narrow(const String& value) {
    std::string result;
    if (!value.impl) return "<null>";
    for (unsigned i = 0; i < value.impl->length; ++i) {
        char16_t unit = (value.impl->hashAndFlags & 4) ? static_cast<const unsigned char*>(value.impl->data)[i] : static_cast<const char16_t*>(value.impl->data)[i];
        result.push_back(unit < 0x80 ? static_cast<char>(unit) : '?');
    }
    return result;
}

OriginRef FromString(const String& text) {
    return _ZN7WebCore14SecurityOrigin16createFromStringERKN3WTF6StringE(&text);
}

OriginRef FromParsedURL(const String& text) {
    const URL base{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &text, nullptr);
    const String owner(url.impl);
    return _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(&url);
}

std::string ToString(const OriginRef& origin) {
    return Narrow(_ZNK7WebCore14SecurityOrigin8toStringEv(origin.get()));
}

std::string ToRawString(const OriginRef& origin) {
    return Narrow(_ZNK7WebCore14SecurityOrigin11toRawStringEv(origin.get()));
}

std::string Origin(std::string_view text) {
    String input = Text(text);
    unsigned before = input.impl->refCount;
    std::string result;
    {
        OriginRef origin = FromString(input);
        Require(origin.get() != nullptr);
        Require(origin.get()->refCount.load() == 1);
        result = ToString(origin);
    }
    Require(input.impl->refCount == before);
    return result;
}

void RequireSameAsParsedURL(std::string_view text, bool opaque) {
    String input = Text(text);
    OriginRef fromString = FromString(input);
    OriginRef fromURL = FromParsedURL(input);
    Require(fromString.get() != fromURL.get());
    Require(ToString(fromString) == ToString(fromURL));
    Require(ToRawString(fromString) == ToRawString(fromURL));
    Require(_ZNK7WebCore14SecurityOrigin20isSameSchemeHostPortERKS0_(fromString.get(), fromURL.get()) == !opaque);
    Require(_ZNK7WebCore14SecurityOrigin5equalEPKS0_(fromString.get(), fromURL.get()) == !opaque);
}

void TestTupleOrigins() {
    Require(Origin("https://Example.COM:443/path/to?q=1#frag") == "https://example.com");
    Require(Origin("http://example.com:8080") == "http://example.com:8080");
    Require(Origin("HTTP://user:pw@a.test/x") == "http://a.test");
    Require(Origin("http://127.0.0.1:80/") == "http://127.0.0.1");
    Require(Origin("https://[::1]:8443/") == "https://[::1]:8443");
    Require(Origin("  https://trimmed.test/  ") == "https://trimmed.test");
    Require(Origin("ws://socket.test:81/chat") == "ws://socket.test:81");

    for (std::string_view text : {"https://Example.COM:443/path/to?q=1#frag", "http://example.com:8080", "HTTP://user:pw@a.test/x",
             "http://127.0.0.1:80/", "https://[::1]:8443/", "ws://socket.test:81/chat", "ftp://files.test/pub"}) {
        RequireSameAsParsedURL(text, false);
    }
}

void TestOpaqueOrigins() {
    {
        OriginRef firstOpaque = FromParsedURL(Text(""));
    }
    for (std::string_view text : {"", "not a url", "/relative/path", "about:blank", "data:text/plain,hi", "javascript:void(0)", "http://", "https://[::1/"}) {
        Require(Origin(text) == "null");
        RequireSameAsParsedURL(text, true);
    }
    String empty;
    OriginRef null = FromString(empty);
    Require(ToString(null) == "null");

    String text = Text("about:blank");
    OriginRef first = FromString(text);
    OriginRef second = FromString(text);
    Require(first.get() != second.get());
    Require(!_ZNK7WebCore14SecurityOrigin5equalEPKS0_(first.get(), second.get()));
}

void TestNullArgument() {
    bool threw = false;
    try {
        _ZN7WebCore14SecurityOrigin16createFromStringERKN3WTF6StringE(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    TestTupleOrigins();
    TestOpaqueOrigins();
    TestNullArgument();
    return 0;
}
