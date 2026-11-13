#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using NKWebKit::WtfAbi::StringView;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SecurityOrigin check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool Local(const char* text) {
    return _ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(StringView{text, static_cast<unsigned>(std::strlen(text)), true});
}

static bool Local16(const std::u16string& text) {
    return _ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(StringView{text.data(), static_cast<unsigned>(text.size()), false});
}

int main() {
    Require(Local("[::1]"));
    Require(!Local("::1"));
    Require(!Local("[::2]"));
    Require(Local("127.0.0.1"));
    Require(Local("127.255.255.255"));
    Require(Local("127.1.2.3"));
    Require(!Local("127.0.0"));
    Require(!Local("127.0.0.1.1"));
    Require(!Local("127.0.0.a"));
    Require(!Local("128.0.0.1"));
    Require(!Local("127."));
    Require(!Local("127"));
    Require(Local("localhost"));
    Require(Local("LocalHost"));
    Require(Local("LOCALHOST"));
    Require(Local("foo.localhost"));
    Require(Local("a.b.LOCALHOST"));
    Require(Local(".localhost"));
    Require(!Local("localhost.example"));
    Require(!Local("notlocalhost"));
    Require(!Local("localhos"));
    Require(!Local("example.com"));
    Require(!Local(""));

    Require(Local16(u"localhost"));
    Require(Local16(u"sub.LocalHost"));
    Require(Local16(u"127.0.0.1"));
    Require(Local16(u"[::1]"));
    Require(!Local16(u"locаlhost"));
    Require(!Local16(u"127.0.0.١"));

    StringView null{nullptr, 0, true};
    Require(!_ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(null));
    return 0;
}
