#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "URL setter check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

struct Case {
    const char* base;
    const char* input;
    const char* operation;
    const char* argument;
    const char* expected;
};

const Case Cases[] = {
    {R"(\N)", R"(a://example.net)", "setProtocol", R"()", R"(R0 1 0 0 0 1 4 4 4 15 15 15 15 1 a://example.net)"},
    {R"(\N)", R"(a://example.net)", "setProtocol", R"(b)", R"(R1 1 0 0 0 1 4 4 4 15 15 15 15 1 b://example.net)"},
    {R"(\N)", R"(javascript:alert(1))", "setProtocol", R"(defuse)", R"(R1 1 0 1 0 6 7 7 7 7 7 15 15 1 defuse:alert(1))"},
    {R"(\N)", R"(a://example.net)", "setProtocol", R"(B)", R"(R1 1 0 0 0 1 4 4 4 15 15 15 15 1 b://example.net)"},
    {R"(\N)", R"(a://example.net)", "setProtocol", R"(\u00E9)", R"(R0 1 0 0 0 1 4 4 4 15 15 15 15 1 a://example.net)"},
    {R"(\N)", R"(a://example.net)", "setProtocol", R"(0b)", R"(R0 1 0 0 0 1 4 4 4 15 15 15 15 1 a://example.net)"},
    {R"(\N)", R"(a://example.net)", "setProtocol", R"(+b)", R"(R0 1 0 0 0 1 4 4 4 15 15 15 15 1 a://example.net)"},
    {R"(\N)", R"(a://example.net)", "setProtocol", R"(bC0+-.)", R"(R1 1 0 0 0 6 9 9 9 20 20 20 20 1 bc0+-.://example.net)"},
    {R"(\N)", R"(a://example.net)", "setProtocol", R"(b,c)", R"(R0 1 0 0 0 1 4 4 4 15 15 15 15 1 a://example.net)"},
    {R"(\N)", R"(a://example.net)", "setProtocol", R"(b\u00E9)", R"(R0 1 0 0 0 1 4 4 4 15 15 15 15 1 a://example.net)"},
    {R"(\N)", R"(http://test@example.net)", "setProtocol", R"(file)", R"(R1 1 1 0 0 4 7 11 11 23 24 24 24 1 http://test@example.net/)"},
    {R"(\N)", R"(https://example.net:1234)", "setProtocol", R"(file)", R"(R1 1 1 0 5 5 8 8 8 19 25 25 25 1 https://example.net:1234/)"},
    {R"(\N)", R"(wss://x:x@example.net:1234)", "setProtocol", R"(file)", R"(R1 1 0 0 5 3 6 7 9 21 27 27 27 1 wss://x:x@example.net:1234/)"},
    {R"(\N)", R"(file://localhost/)", "setProtocol", R"(http)", R"(R1 1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(\N)", R"(file:///test)", "setProtocol", R"(https)", R"(R1 1 0 0 0 4 7 7 7 7 8 12 12 1 file:///test)"},
    {R"(\N)", R"(file:)", "setProtocol", R"(wss)", R"(R1 1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(\N)", R"(http://example.net)", "setProtocol", R"(b)", R"(R1 1 0 0 0 1 4 4 4 15 16 16 16 1 b://example.net/)"},
    {R"(\N)", R"(file://hi/path)", "setProtocol", R"(s)", R"(R1 1 0 0 0 1 4 4 4 6 7 11 11 1 s://hi/path)"},
    {R"(\N)", R"(https://example.net)", "setProtocol", R"(s)", R"(R1 1 0 0 0 1 4 4 4 15 16 16 16 1 s://example.net/)"},
    {R"(\N)", R"(ftp://example.net)", "setProtocol", R"(test)", R"(R1 1 0 0 0 4 7 7 7 18 19 19 19 1 test://example.net/)"},
    {R"(\N)", R"(mailto:me@example.net)", "setProtocol", R"(http)", R"(R1 1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/)"},
    {R"(\N)", R"(ssh://me@example.net)", "setProtocol", R"(http)", R"(R1 1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/)"},
    {R"(\N)", R"(ssh://me@example.net)", "setProtocol", R"(https)", R"(R1 1 1 0 0 5 8 10 10 22 23 23 23 1 https://me@example.net/)"},
    {R"(\N)", R"(ssh://me@example.net)", "setProtocol", R"(file)", R"(R1 1 0 0 0 3 6 8 8 20 20 20 20 1 ssh://me@example.net)"},
    {R"(\N)", R"(ssh://example.net)", "setProtocol", R"(file)", R"(R1 1 0 0 0 4 7 7 7 18 19 19 19 1 file://example.net/)"},
    {R"(\N)", R"(nonsense:///test)", "setProtocol", R"(https)", R"(R1 1 1 0 0 5 8 8 8 12 13 13 13 1 https://test/)"},
    {R"(\N)", R"(http://example.net)", "setProtocol", R"(https:foo : bar)", R"(R1 1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.net/)"},
    {R"(\N)", R"(data:text/html,<p>Test)", "setProtocol", R"(view-source+data:foo : bar)", R"(R1 1 0 1 0 16 17 17 17 17 22 34 34 1 view-source+data:text/html,<p>Test)"},
    {R"(\N)", R"(http://foo.com:443/)", "setProtocol", R"(https)", R"(R1 1 1 0 0 5 8 8 8 15 16 16 16 1 https://foo.com/)"},
    {R"(\N)", R"(file:///home/you/index.html)", "setUser", R"(me)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://me@/home/you/index.html)"},
    {R"(\N)", R"(unix:/run/foo.socket)", "setUser", R"(me)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 unix://me@/run/foo.socket)"},
    {R"(\N)", R"(mailto:you@example.net)", "setUser", R"(me)", R"(1 0 0 0 6 9 17 17 29 29 29 29 1 mailto://me%40you@example.net)"},
    {R"(\N)", R"(javascript:alert(1))", "setUser", R"(wario)", R"(1 0 0 0 10 13 18 18 27 27 27 27 1 javascript://wario@alert(1))"},
    {R"(\N)", R"(http://example.net)", "setUser", R"(me)", R"(1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/)"},
    {R"(\N)", R"(http://:secret@example.net)", "setUser", R"(me)", R"(1 1 0 0 4 7 9 16 28 29 29 29 1 http://me:secret@example.net/)"},
    {R"(\N)", R"(http://me@example.net)", "setUser", R"()", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/)"},
    {R"(\N)", R"(http://me:secret@example.net)", "setUser", R"()", R"(1 1 0 0 4 7 7 14 26 27 27 27 1 http://:secret@example.net/)"},
    {R"(\N)", R"(http://example.net)", "setUser", R"(\u0000\u0001\u0009\u000A\u000D\u001F !"#$%&'()*+,-./09:;<=>?@AZ[\u005C]^_`az{|}~\u007F\u0080\u0081\u00C9\u00E9)", R"(1 1 0 0 4 7 129 129 141 142 142 142 1 http://%00%01%09%0A%0D%1F%20!%22%23$%&'()*+,-.%2F09%3A%3B%3C%3D%3E%3F%40AZ%5B%5C%5D%5E_%60az%7B%7C%7D~%7F%C2%80%C2%81%C3%89%C3%A9@example.net/)"},
    {R"(\N)", R"(http://example.net)", "setUser", R"(%c3%89t\u00E9)", R"(1 1 0 0 4 7 20 20 32 33 33 33 1 http://%c3%89t%C3%A9@example.net/)"},
    {R"(\N)", R"(sc:///)", "setUser", R"(x)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://x@/)"},
    {R"(\N)", R"(javascript://x/)", "setUser", R"(wario)", R"(1 0 0 0 10 13 18 18 20 21 21 21 1 javascript://wario@x/)"},
    {R"(\N)", R"(file://test/)", "setUser", R"(test)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://test@test/)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"(\u0000)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://\u0000/)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"(\u0009)", R"(1 0 0 0 2 5 5 5 5 6 6 6 1 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"(\u000A)", R"(1 0 0 0 2 5 5 5 5 6 6 6 1 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"(\u000D)", R"(1 0 0 0 2 5 5 5 5 6 6 6 1 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"( )", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc:// /)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"(#)", R"(1 0 0 0 2 5 5 5 5 5 5 5 0 sc://#/)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"(/)", R"(1 0 0 0 2 5 5 5 5 7 7 7 0 sc:////)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"(?)", R"(1 0 0 0 2 5 5 5 5 5 5 7 0 sc://?/)"},
    {R"(\N)", R"(sc://x/)", "setHostAndPort", R"(@)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://@/)"},
    {R"(\N)", R"(mailto:me@example.net)", "setHostAndPort", R"(example.com)", R"(1 0 0 0 6 9 22 22 34 34 34 34 0 mailto://example.comme@example.net)"},
    {R"(\N)", R"(data:text/plain,Stuff)", "setHostAndPort", R"(example.net)", R"(1 0 0 0 4 7 7 7 22 23 34 34 0 data://example.nettext/plain,Stuff)"},
    {R"(\N)", R"(http://example.net)", "setHostAndPort", R"(example.com:8080)", R"(1 1 0 5 4 7 7 7 18 24 24 24 0 http://example.com:8080/)"},
    {R"(\N)", R"(http://example.net:8080)", "setHostAndPort", R"(example.com)", R"(1 1 0 0 4 7 7 7 18 19 19 19 0 http://example.com/)"},
    {R"(\N)", R"(http://example.net:8080)", "setHostAndPort", R"(example.com:)", R"(1 1 0 0 4 7 7 7 18 19 19 19 0 http://example.com/)"},
    {R"(\N)", R"(http://example.net)", "setHostAndPort", R"()", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:///)"},
    {R"(\N)", R"(view-source+http://example.net/foo)", "setHostAndPort", R"()", R"(1 0 0 0 16 19 19 19 19 20 23 23 1 view-source+http:///foo)"},
    {R"(\N)", R"(a:/foo)", "setHostAndPort", R"(example.net)", R"(1 0 0 0 1 4 4 4 15 16 19 19 0 a://example.net/foo)"},
    {R"(\N)", R"(http://example.net)", "setHostAndPort", R"(0x7F000001:8080)", R"(1 1 0 5 4 7 7 7 16 22 22 22 1 http://127.0.0.1:8080/)"},
    {R"(\N)", R"(http://example.net)", "setHostAndPort", R"([::0:01]:2)", R"(1 1 0 2 4 7 7 7 12 15 15 15 1 http://[::1]:2/)"},
    {R"(\N)", R"(http://example.net)", "setHostAndPort", R"([2001:db8::2]:4002)", R"(1 1 0 5 4 7 7 7 20 26 26 26 0 http://[2001:db8::2]:4002/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"(example.com: 8080)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://example.com: 8080/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"(example.com:+8080)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://example.com:+8080/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"(example.com:8080 )", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://example.com:8080 /)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"(example.com:%208080)", R"(1 1 0 0 4 7 7 7 18 19 19 19 0 http://example.com/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"(example.com:08080)", R"(1 1 0 5 4 7 7 7 18 24 24 24 1 http://example.com:8080/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"(example.com:65536)", R"(1 1 0 0 4 7 7 7 18 19 19 19 0 http://example.com/)"},
    {R"(\N)", R"(http://example.net)", "setHostAndPort", R"(example.com:80)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.com/)"},
    {R"(\N)", R"(https://example.net)", "setHostAndPort", R"(example.com:443)", R"(1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.com/)"},
    {R"(\N)", R"(https://example.net)", "setHostAndPort", R"(example.com:80)", R"(1 1 0 3 5 8 8 8 19 23 23 23 0 https://example.com:80/)"},
    {R"(\N)", R"(http://example.net:8080)", "setHostAndPort", R"(example.com:80)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.com/)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com/stuff)", R"(1 1 0 0 4 7 7 7 18 25 29 29 0 http://example.com/stuff/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com:8080/stuff)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com?stuff)", R"(1 1 0 0 4 7 7 7 18 19 19 30 1 http://example.com/?stuff/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com:8080?stuff)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com#stuff)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.com/#stuff/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com:8080#stuff)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com\u005Cstuff)", R"(1 1 0 0 4 7 7 7 18 25 29 29 1 http://example.com/stuff/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com:8080\u005Cstuff)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(view-source+http://example.net/path)", "setHostAndPort", R"(example.com\u005Cstuff)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 view-source+http://example.com\u005Cstuff/path)"},
    {R"(\N)", R"(view-source+http://example.net/path)", "setHostAndPort", R"(example.com:8080stuff2)", R"(1 0 0 0 16 19 19 19 30 31 35 35 0 view-source+http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com:8080stuff2)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com:8080+2)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com:65535)", R"(1 1 0 6 4 7 7 7 18 25 29 29 0 http://example.com:65535/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHostAndPort", R"(example.com:65536)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"([google.com])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[google.com]/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"([::1.2.3.4x])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[:/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"([::1.2.3.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[:/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"([::1.2.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[:/)"},
    {R"(\N)", R"(http://example.net/)", "setHostAndPort", R"([::1.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[:/)"},
    {R"(\N)", R"(file://y/)", "setHostAndPort", R"(x:123)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 file://x:123/)"},
    {R"(\N)", R"(file://y/)", "setHostAndPort", R"(loc%41lhost)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(\N)", R"(file://hi/x)", "setHostAndPort", R"()", R"(1 0 0 0 4 7 7 7 7 8 9 9 1 file:///x)"},
    {R"(\N)", R"(sc://test@test/)", "setHostAndPort", R"()", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://test@/)"},
    {R"(\N)", R"(sc://test:12/)", "setHostAndPort", R"()", R"(1 0 0 0 2 5 5 5 5 6 6 6 1 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"(\u0000)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://\u0000/)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"(\u0009)", R"(1 0 0 0 2 5 5 5 5 6 6 6 1 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"(\u000A)", R"(1 0 0 0 2 5 5 5 5 6 6 6 1 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"(\u000D)", R"(1 0 0 0 2 5 5 5 5 6 6 6 1 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"( )", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc:// /)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"(#)", R"(1 0 0 0 2 5 5 5 5 6 6 6 0 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"(/)", R"(1 0 0 0 2 5 5 5 5 6 6 6 0 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"(?)", R"(1 0 0 0 2 5 5 5 5 6 6 6 0 sc:///)"},
    {R"(\N)", R"(sc://x/)", "setHost", R"(@)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://@/)"},
    {R"(\N)", R"(mailto:me@example.net)", "setHost", R"(example.com)", R"(1 0 0 0 6 9 22 22 34 34 34 34 0 mailto://example.comme@example.net)"},
    {R"(\N)", R"(data:text/plain,Stuff)", "setHost", R"(example.net)", R"(1 0 0 0 4 7 7 7 22 23 34 34 0 data://example.nettext/plain,Stuff)"},
    {R"(\N)", R"(http://example.net:8080)", "setHost", R"(example.com)", R"(1 1 0 5 4 7 7 7 18 24 24 24 0 http://example.com:8080/)"},
    {R"(\N)", R"(http://example.net)", "setHost", R"()", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http:///)"},
    {R"(\N)", R"(view-source+http://example.net/foo)", "setHost", R"()", R"(1 0 0 0 16 19 19 19 19 20 23 23 0 view-source+http:///foo)"},
    {R"(\N)", R"(a:/foo)", "setHost", R"(example.net)", R"(1 0 0 0 1 4 4 4 15 16 19 19 0 a://example.net/foo)"},
    {R"(\N)", R"(http://example.net:8080)", "setHost", R"(0x7F000001)", R"(1 1 0 5 4 7 7 7 16 22 22 22 1 http://127.0.0.1:8080/)"},
    {R"(\N)", R"(http://example.net)", "setHost", R"([::0:01])", R"(1 1 0 0 4 7 7 7 12 13 13 13 1 http://[::1]/)"},
    {R"(\N)", R"(http://example.net/path)", "setHost", R"(example.com:8080)", R"(1 1 0 0 4 7 7 7 18 19 23 23 1 http://example.net/path)"},
    {R"(\N)", R"(http://example.net:8080/path)", "setHost", R"(example.com:)", R"(1 1 0 5 4 7 7 7 18 24 28 28 1 http://example.net:8080/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHost", R"(example.com/stuff)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHost", R"(example.com?stuff)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHost", R"(example.com#stuff)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(http://example.net/path)", "setHost", R"(example.com\u005Cstuff)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/path)"},
    {R"(\N)", R"(view-source+http://example.net/path)", "setHost", R"(example.com\u005Cstuff)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 view-source+http://example.com\u005Cstuff/path)"},
    {R"(\N)", R"(http://example.net/)", "setHost", R"([google.com])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[google.com]/)"},
    {R"(\N)", R"(http://example.net/)", "setHost", R"([::1.2.3.4x])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[::1.2.3.4x]/)"},
    {R"(\N)", R"(http://example.net/)", "setHost", R"([::1.2.3.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[::1.2.3.]/)"},
    {R"(\N)", R"(http://example.net/)", "setHost", R"([::1.2.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[::1.2.]/)"},
    {R"(\N)", R"(http://example.net/)", "setHost", R"([::1.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[::1.]/)"},
    {R"(\N)", R"(file://y/)", "setHost", R"(x:123)", R"(1 0 0 0 4 7 7 7 8 9 9 9 1 file://y/)"},
    {R"(\N)", R"(file://y/)", "setHost", R"(loc%41lhost)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(\N)", R"(file://hi/x)", "setHost", R"()", R"(1 0 0 0 4 7 7 7 7 8 9 9 0 file:///x)"},
    {R"(\N)", R"(sc://test@test/)", "setHost", R"()", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://test@/)"},
    {R"(\N)", R"(sc://test:12/)", "setHost", R"()", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://:12/)"},
    {R"(\N)", R"(non-spec:/.//p)", "setHost", R"(h)", R"(1 0 0 0 8 11 11 11 12 14 15 15 1 non-spec://h//p)"},
    {R"(\N)", R"(non-spec:/.//p)", "setHost", R"()", R"(1 0 0 0 8 11 11 11 11 13 14 14 1 non-spec:////p)"},
    {R"(\N)", R"(mailto:me@example.net)", "setPath", R"(/foo)", R"(1 0 0 0 6 7 7 7 7 8 11 11 1 mailto:/foo)"},
    {R"(\N)", R"(unix:/run/foo.socket?timeout=10)", "setPath", R"(/var/log/../run/bar.socket)", R"(1 0 0 0 4 5 5 5 5 14 24 35 1 unix:/var/run/bar.socket?timeout=10)"},
    {R"(\N)", R"(https://example.net#nav)", "setPath", R"(home)", R"(1 1 0 0 5 8 8 8 19 20 24 24 1 https://example.net/home#nav)"},
    {R"(\N)", R"(https://example.net#nav)", "setPath", R"(../home)", R"(1 1 0 0 5 8 8 8 19 20 24 24 1 https://example.net/home#nav)"},
    {R"(\N)", R"(http://example.net/home?lang=fr#nav)", "setPath", R"(\u005Ca\u005C%2E\u005Cb\u005C%2e.\u005Cc)", R"(1 1 0 0 4 7 7 7 18 21 22 30 1 http://example.net/a/c?lang=fr#nav)"},
    {R"(\N)", R"(view-source+http://example.net/home?lang=fr#nav)", "setPath", R"(\u005Ca\u005C%2E\u005Cb\u005C%2e.\u005Cc)", R"(1 0 0 0 16 19 19 19 30 31 46 54 1 view-source+http://example.net/\u005Ca\u005C%2E\u005Cb\u005C%2e.\u005Cc?lang=fr#nav)"},
    {R"(\N)", R"(a:/)", "setPath", R"(\u0000\u0001\u0009\u000A\u000D\u001F !"#$%&'()*+,-./09:;<=>?@AZ[\u005C]^_`az{|}~\u007F\u0080\u0081\u00C9\u00E9)", R"(1 0 0 0 1 2 2 2 2 34 120 120 1 a:/%00%01%1F%20!%22%23$%&'()*+,-./09:;%3C=%3E%3F@AZ[\u005C]^_%60az%7B|%7D~%7F%C3%82%C2%80%C3%82%C2%81%C3%83%C2%89%C3%83%C2%A9)"},
    {R"(\N)", R"(http://example.net)", "setPath", R"(%2e%2E%c3%89t\u00E9)", R"(1 1 0 0 4 7 7 7 18 19 38 38 1 http://example.net/%2e%2E%c3%89t%C3%A9)"},
    {R"(\N)", R"(http://example.net)", "setPath", R"(?)", R"(1 1 0 0 4 7 7 7 18 19 22 22 1 http://example.net/%3F)"},
    {R"(\N)", R"(http://example.net)", "setPath", R"(#)", R"(1 1 0 0 4 7 7 7 18 19 22 22 1 http://example.net/%23)"},
    {R"(\N)", R"(sc://example.net)", "setPath", R"(?)", R"(1 0 0 0 2 5 5 5 16 17 20 20 1 sc://example.net/%3F)"},
    {R"(\N)", R"(sc://example.net)", "setPath", R"(#)", R"(1 0 0 0 2 5 5 5 16 17 20 20 1 sc://example.net/%23)"},
    {R"(\N)", R"(file://monkey/)", "setPath", R"(\u005C\u005C)", R"(1 0 0 0 4 7 7 7 13 15 15 15 1 file://monkey//)"},
    {R"(\N)", R"(file:///unicorn)", "setPath", R"(//\u005C/)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file://////)"},
    {R"(\N)", R"(file:///unicorn)", "setPath", R"(//monkey/..//)", R"(1 0 0 0 4 7 7 7 7 10 10 10 1 file://///)"},
    {R"(\N)", R"(non-spec:/)", "setPath", R"(/.//p)", R"(1 0 0 0 8 9 9 9 9 13 14 14 1 non-spec:/.//p)"},
    {R"(\N)", R"(non-spec:/)", "setPath", R"(/..//p)", R"(1 0 0 0 8 9 9 9 9 13 14 14 1 non-spec:/.//p)"},
    {R"(\N)", R"(non-spec:/)", "setPath", R"(//p)", R"(1 0 0 0 8 11 11 11 12 12 12 12 1 non-spec://p)"},
    {R"(\N)", R"(non-spec:/.//)", "setPath", R"(p)", R"(1 0 0 0 8 9 9 9 9 10 11 11 1 non-spec:/p)"},
    {R"(\N)", R"(https://example.net#nav)", "setQuery", R"(lang=fr)", R"(1 1 0 0 5 8 8 8 19 20 20 28 1 https://example.net/?lang=fr#nav)"},
    {R"(\N)", R"(https://example.net?lang=en-US#nav)", "setQuery", R"(lang=fr)", R"(1 1 0 0 5 8 8 8 19 20 20 28 1 https://example.net/?lang=fr#nav)"},
    {R"(\N)", R"(https://example.net?lang=en-US#nav)", "setQuery", R"(?lang=fr)", R"(1 1 0 0 5 8 8 8 19 20 20 28 1 https://example.net/?lang=fr#nav)"},
    {R"(\N)", R"(https://example.net?lang=en-US#nav)", "setQuery", R"(??lang=fr)", R"(1 1 0 0 5 8 8 8 19 20 20 29 1 https://example.net/??lang=fr#nav)"},
    {R"(\N)", R"(https://example.net?lang=en-US#nav)", "setQuery", R"(?)", R"(1 1 0 0 5 8 8 8 19 20 20 21 1 https://example.net/?#nav)"},
    {R"(\N)", R"(https://example.net?lang=en-US#nav)", "setQuery", R"()", R"(1 1 0 0 5 8 8 8 19 20 20 21 1 https://example.net/?#nav)"},
    {R"(\N)", R"(https://example.net?lang=en-US)", "setQuery", R"()", R"(1 1 0 0 5 8 8 8 19 20 20 21 1 https://example.net/?)"},
    {R"(\N)", R"(https://example.net)", "setQuery", R"()", R"(1 1 0 0 5 8 8 8 19 20 20 21 1 https://example.net/?)"},
    {R"(\N)", R"(a:/)", "setQuery", R"(\u0000\u0001\u0009\u000A\u000D\u001F !"#$%&'()*+,-./09:;<=>?@AZ[\u005C]^_`az{|}~\u007F\u0080\u0081\u00C9\u00E9)", R"(1 0 0 0 1 2 2 2 2 3 3 20 1 a:/?%00%01%1F%20!%22#$%&'()*+,-./09:;%3C=%3E?@AZ[\u005C]^_%60az{|}~%7F%C2%80%C2%81%C3%89%C3%A9)"},
    {R"(\N)", R"(http://example.net)", "setQuery", R"(%c3%89t\u00E9)", R"(1 1 0 0 4 7 7 7 18 19 19 33 1 http://example.net/?%c3%89t%C3%A9)"},
    {R"(\N)", R"(https://example.net)", "setFragmentIdentifier", R"(main)", R"(1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.net/#main)"},
    {R"(\N)", R"(https://example.net#nav)", "setFragmentIdentifier", R"(main)", R"(1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.net/#main)"},
    {R"(\N)", R"(https://example.net?lang=en-US)", "setFragmentIdentifier", R"(##nav)", R"(1 1 0 0 5 8 8 8 19 20 20 31 1 https://example.net/?lang=en-US###nav)"},
    {R"(\N)", R"(https://example.net?lang=en-US#nav)", "setFragmentIdentifier", R"(#main)", R"(1 1 0 0 5 8 8 8 19 20 20 31 1 https://example.net/?lang=en-US##main)"},
    {R"(\N)", R"(https://example.net?lang=en-US#nav)", "setFragmentIdentifier", R"(#)", R"(1 1 0 0 5 8 8 8 19 20 20 31 1 https://example.net/?lang=en-US##)"},
    {R"(\N)", R"(https://example.net?lang=en-US#nav)", "setFragmentIdentifier", R"()", R"(1 1 0 0 5 8 8 8 19 20 20 31 1 https://example.net/?lang=en-US#)"},
    {R"(\N)", R"(http://example.net)", "setFragmentIdentifier", R"(#foo bar)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/##foo%20bar)"},
    {R"(\N)", R"(http://example.net)", "setFragmentIdentifier", R"(#foo"bar)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/##foo%22bar)"},
    {R"(\N)", R"(http://example.net)", "setFragmentIdentifier", R"(#foo<bar)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/##foo%3Cbar)"},
    {R"(\N)", R"(http://example.net)", "setFragmentIdentifier", R"(#foo>bar)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/##foo%3Ebar)"},
    {R"(\N)", R"(http://example.net)", "setFragmentIdentifier", R"(#foo`bar)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/##foo%60bar)"},
    {R"(\N)", R"(a:/)", "setFragmentIdentifier", R"(\u0000\u0001\u0009\u000A\u000D\u001F !"#$%&'()*+,-./09:;<=>?@AZ[\u005C]^_`az{|}~\u007F\u0080\u0081\u00C9\u00E9)", R"(1 0 0 0 1 2 2 2 2 3 3 3 1 a:/#%00%01%1F%20!%22#$%&'()*+,-./09:;%3C=%3E?@AZ[\u005C]^_%60az{|}~%7F%C2%80%C2%81%C3%89%C3%A9)"},
    {R"(\N)", R"(http://example.net)", "setFragmentIdentifier", R"(a\u0000b)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/#a%00b)"},
    {R"(\N)", R"(non-spec:/)", "setFragmentIdentifier", R"(a\u0000b)", R"(1 0 0 0 8 9 9 9 9 10 10 10 1 non-spec:/#a%00b)"},
    {R"(\N)", R"(http://example.net)", "setFragmentIdentifier", R"(%c3%89t\u00E9)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/#%c3%89t%C3%A9)"},
    {R"(\N)", R"(javascript:alert(1))", "setFragmentIdentifier", R"(castle)", R"(1 0 1 0 10 11 11 11 11 11 19 19 1 javascript:alert(1)#castle)"},
    {R"(sc://host/path)", R"(http:0x))", "fileURLWithFileSystemPath", R"(http%2ea\u20ACxn--Z?#)", R"(1 0 0 0 4 7 7 7 7 8 45 45 1 file:///http%2ea%C3%A2%C2%82%C2%ACxn--Z%3F%23)"},
    {R"(sc://host/path)", R"(C:,%2e-A)", "fileURLWithFileSystemPath", R"(a\uD83D\uDE00http]Z)", R"(1 0 0 0 4 7 7 7 7 8 27 27 1 file:///a%F0%9F%98%80http]Z)"},
    {R"(about:blank)", R"(http://example.com/foo/bar/..)", "fileURLWithFileSystemPath", R"(9_\uD800-http\uD83D\uDE00]%2e)", R"(1 0 0 0 4 7 7 7 7 8 40 40 1 file:///9_%EF%BF%BD-http%F0%9F%98%80]%2e)"},
    {R"(file://host/share/x)", R"(https:/,*httpsC:)", "fileURLWithFileSystemPath", R"(\W65536#0%2e0@a?\u005C9 )", R"(1 0 0 0 4 7 7 7 7 27 28 28 1 file:///65536%230%2e0@a%3F/9)"},
    {R"(sc://host/path)", R"(data:,)", "fileURLWithFileSystemPath", R"(_)", R"(1 0 0 0 4 7 7 7 7 8 9 9 1 file:///_)"},
    {R"(sc://host/path)", R"(]0,*\u00E9Z\u000AA\u0009@127.0.0.1)", "fileURLWithFileSystemPath", R"(8080Z[http)", R"(1 0 0 0 4 7 7 7 7 8 18 18 1 file:///8080Z[http)"},
    {R"(file://host/share/x)", R"(a:)@~-%2e)", "fileURLWithFileSystemPath", R"(\Wa@.\u20AC9.\uD83D\uDE00)", R"(1 0 0 0 4 7 7 7 7 8 34 34 1 file:///a@.%E2%82%AC9.%F0%9F%98%80)"},
    {R"(about:blank)", R"(http://host/path%2e.%2E)", "fileURLWithFileSystemPath", R"(\N)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file:///C:/a/b)", R"(fragment)", "fileURLWithFileSystemPath", R"(0::1)", R"(1 0 0 0 4 7 7 7 7 8 12 12 1 file:///0::1)"},
    {R"(http://example.org/foo/bar)", R"(q=@%20%3C%3E!)", "fileURLWithFileSystemPath", R"(0\u00E9)", R"(1 0 0 0 4 7 7 7 7 8 15 15 1 file:///0%C3%A9)"},
    {R"(sc://host/path)", R"(http://h%6fsT)", "fileURLWithFileSystemPath", R"(]\uD83D\uDE00a08080\u005C%2e\u0009%2efile\u0009)", R"(1 0 0 0 4 7 7 7 7 28 38 38 1 file:///]%F0%9F%98%80a08080/%2e%2efile)"},
    {R"(http://h/a?b#c)", R"(//file,?$Z%2e$#*\u0000?~http$&http[::1]http\u0009-)", "fileURLWithFileSystemPath", R"(\W]\uD83D\uDE00+0\u00E9..%2e\u0009a65536%9)", R"(1 0 0 0 4 7 7 7 7 8 42 42 1 file:///]%F0%9F%98%80+0%C3%A9..%2ea65536%9)"},
    {R"(http://example.org/foo/bar)", R"(http://foo:\uD83D\uDCA9@example.com/bar)", "fileURLWithFileSystemPath", R"(-0..@@0..Z)", R"(1 0 0 0 4 7 7 7 7 8 18 18 1 file:///-0..@@0..Z)"},
    {R"(http://example.org/foo/bar)", R"(http://[0:f::f:f:0:0]/path)", "fileURLWithFileSystemPath", R"(\uD800\uD800\uD800\u0009\u000A%2e0?)", R"(1 0 0 0 4 7 7 7 7 8 69 69 1 file:///%C3%AF%C2%BF%C2%BD%C3%AF%C2%BF%C2%BD%C3%AF%C2%BF%C2%BD%2e0%3F)"},
    {R"(http://example.org/foo/bar)", R"(javascript:..[::1](%2E::b\u00E99,)", "fileURLWithFileSystemPath", R"(65536::1\u0009\uD83D\uDE00_\u0009a8080)", R"(1 0 0 0 4 7 7 7 7 8 34 34 1 file:///65536::1%F0%9F%98%80_a8080)"},
    {R"(sc://host/path)", R"([a:b:c:d:e:f:127.0..0.1])", "fileURLWithFileSystemPath", R"(\uD800\u000A%2e)", R"(1 0 0 0 4 7 7 7 7 8 20 20 1 file:///%EF%BF%BD%2e)"},
    {R"(\N)", R"(file://pAtH)", "fileURLWithFileSystemPath", R"(~%2e_#\uD800 ]]])", R"(1 0 0 0 4 7 7 7 7 8 40 40 1 file:///~%2e_%23%C3%AF%C2%BF%C2%BD%20]]])"},
    {R"(sc://host/path)", R"(?%2e _127.0.0.1/)", "fileURLWithFileSystemPath", R"(\u0009\uD800]@:ahttpa)", R"(1 0 0 0 4 7 7 7 7 8 26 26 1 file:///%EF%BF%BD]@:ahttpa)"},
    {R"(file://host/share/x)", R"(http:(\u005C#\u005C&(A|::\u001F: Z')[::1]0x&\u3000)", "fileURLWithFileSystemPath", R"(\u20AC#+:8080..::1.@file+)", R"(1 0 0 0 4 7 7 7 7 8 47 47 1 file:///%C3%A2%C2%82%C2%AC%23+:8080..::1.@file+)"},
    {R"(file:///C:/a/b)", R"(sc:\u0009)", "fileURLWithFileSystemPath", R"(a\u005C%\u005C)", R"(1 0 0 0 4 7 7 7 7 12 12 12 1 file:///a/%/)"},
    {R"(http://example.org/foo/bar)", R"(http:example.com/)", "fileURLWithFileSystemPath", R"(?9Z\u000A])", R"(1 0 0 0 4 7 7 7 7 8 14 14 1 file:///%3F9Z])"},
    {R"(sc://host/path)", R"(sc:(_file+%9$http'$b[::1]\uD83D\uDE00%2EA)", "fileURLWithFileSystemPath", R"(%:httpZ_655368080file)", R"(1 0 0 0 4 7 7 7 7 8 29 29 1 file:///%:httpZ_655368080file)"},
    {R"(about:blank)", R"(data:xn--%2e\u001Fxn--\u0009|.httpC:\uD83D\uDE00?\u20AC0x%2E\u00E9!%)", "fileURLWithFileSystemPath", R"(\u20ACfilea/...file%2e\u20AC]..)", R"(1 0 0 0 4 7 7 7 7 23 45 45 1 file:///%E2%82%ACfilea/...file%2e%E2%82%AC]..)"},
    {R"(http://h/a?b#c)", R"(file~_0x)", "fileURLWithFileSystemPath", R"(~%2e_http\u20AC\u00E9)", R"(1 0 0 0 4 7 7 7 7 8 32 32 1 file:///~%2e_http%E2%82%AC%C3%A9)"},
    {R"(\N)", R"(javascript:)", "fileURLWithFileSystemPath", R"( ZZ+\u005C%2eZ+8080\u0009)", R"(1 0 0 0 4 7 7 7 7 15 24 24 1 file:///%20ZZ+/%2eZ+8080)"},
    {R"(file:///C:/a/b)", R"(/\u005C\u000A)", "removeCredentials", R"(\u005C\uD800_xn--a[~:)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(http://example.org/foo/bar)", R"(javascript:..[::1](%2E::b\u00E99,)", "removeCredentials", R"(\N)", R"(1 0 1 0 10 11 11 11 11 11 33 33 1 javascript:..[::1](%2E::b%C3%A99,)"},
    {R"(file:///C:/a/b)", R"(..]'!?0a'http#@,C:%2e%:-)", "removeCredentials", R"(\W/)", R"(1 0 0 0 4 7 7 7 7 13 18 28 1 file:///C:/a/..]'!?0a%27http#@,C:%2e%:-)"},
    {R"(file://host/share/x)", R"(ftp:$@-==c'./..?[::1])", "removeCredentials", R"(-?::Z/)", R"(1 0 0 0 3 6 6 6 12 13 13 19 1 ftp://-==c'./?[::1])"},
    {R"(file:///C:/a/b)", R"(http://127.0.0.1.)", "removeCredentials", R"(\W\u0009\u00E9)", R"(1 1 0 0 4 7 7 7 16 17 17 17 1 http://127.0.0.1/)"},
    {R"(file:///C:/a/b)", R"(.?0x':$\u3000\u005C\u0009!]\u00E9)", "removeCredentials", R"(8080)", R"(1 0 0 0 4 7 7 7 7 13 13 39 1 file:///C:/a/?0x%27:$%E3%80%80\u005C!]%C3%A9)"},
    {R"(about:blank)", R"(http://123.234/)", "removeCredentials", R"(/+)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://123.0.0.234/)"},
    {R"(http://example.org/foo/bar)", R"(/C|/foo/bar)", "removeCredentials", R"(\N)", R"(1 1 0 0 4 7 7 7 18 26 29 29 1 http://example.org/C|/foo/bar)"},
    {R"(file:///C:/a/b)", R"(/\uD83D\uDE00|%2e;=-xn--./file\u20AC[cchttps\u0000?)0x127.0.0.1b-!!)", "removeCredentials", R"(?a0::1..~%2e-\u0009Z)", R"(1 0 0 0 4 7 7 7 7 36 60 77 1 file:///C:/%F0%9F%98%80|%2e;=-xn--./file%E2%82%AC[cchttps%00?)0x127.0.0.1b-!!)"},
    {R"(sc://host/path)", R"(ftp:0x=)", "removeCredentials", R"(\N)", R"(1 0 0 0 3 6 6 6 9 10 10 10 1 ftp://0x=/)"},
    {R"(http://h/a?b#c)", R"(file:\u20AC)", "removeCredentials", R"(\N)", R"(1 0 0 0 4 7 7 7 7 8 17 17 1 file:///%E2%82%AC)"},
    {R"(sc://host/path)", R"(data:%2E 0\u00E9)", "removeCredentials", R"(_[#%2e:65536)", R"(1 0 1 0 4 5 5 5 5 5 16 16 1 data:%2E 0%C3%A9)"},
    {R"(about:blank)", R"(data:@(\u001F=file+)", "removeCredentials", R"(file)", R"(1 0 1 0 4 5 5 5 5 5 16 16 1 data:@(%1F=file+)"},
    {R"(http://example.org/foo/bar)", R"(foo://host/#%D0%9F%D0%9F%07%20a%3C/)", "removeCredentials", R"(.%2ea\u00E9:8080::1655360?%)", R"(1 0 0 0 3 6 6 6 10 11 11 11 1 foo://host/#%D0%9F%D0%9F%07%20a%3C/)"},
    {R"(file://host/share/x)", R"(C:%\u001F(c::[::1]Ab127.0.0.1 'c)", "removeCredentials", R"(xn--\u0009#\u000965536)", R"(1 0 1 0 1 2 2 2 2 2 29 29 1 c:%%1F(c::[::1]Ab127.0.0.1 'c)"},
    {R"(file://host/share/x)", R"(sc:&a\u00E9@%2E@?[C:)", "removeCredentials", R"(:[xn--\uD83D\uDE00_)", R"(1 0 1 0 2 3 3 3 3 3 16 20 1 sc:&a%C3%A9@%2E@?[C:)"},
    {R"(file://host/share/x)", R"(sc:&a\u00E9@%2E@?[C:)", "removeCredentials", R"(_)", R"(1 0 1 0 2 3 3 3 3 3 16 20 1 sc:&a%C3%A9@%2E@?[C:)"},
    {R"(http://example.org/foo/bar)", R"(javascript:.\u000A(~&!Z..(.\u0000\u001F::%2e%)", "removeCredentials", R"(_)", R"(1 0 1 0 10 11 11 11 11 11 33 33 1 javascript:.(~&!Z..(.%00%1F::%2e%)"},
    {R"(file://host/share/x)", R"(http:!A%2Ehttp./(/%\u3000)=)", "removeCredentials", R"(\WZ:http65536.?)", R"(1 1 0 0 4 7 7 7 15 18 30 30 1 http://!a.http./(/%%E3%80%80)=)"},
    {R"(file://host/share/x)", R"(file:*()", "removeCredentials", R"(\W?\u000965536 _a.65536?[\u20AC)", R"(1 0 0 0 4 7 7 7 11 18 20 20 1 file://host/share/*()"},
    {R"(\N)", R"(http://hos\u0009t\u000A:\u00091\u000A2\u00093\u0009/\u000Apath)", "removeCredentials", R"(@65536)", R"(1 1 0 4 4 7 7 7 11 16 20 20 1 http://host:123/path)"},
    {R"(sc://host/path)", R"(..[::1]\u30009127.0.0.19#\u0009@::)", "removeCredentials", R"()", R"(1 0 0 0 2 5 5 5 9 10 37 37 1 sc://host/..[::1]%E3%80%809127.0.0.19#@::)"},
    {R"(\N)", R"(http://host/path?query#fragment)", "removeCredentials", R"(file65536[\u20ACxn--)", R"(1 1 0 0 4 7 7 7 11 12 16 22 1 http://host/path?query#fragment)"},
    {R"(sc://host/path)", R"(256)", "removeCredentials", R"(\W.file~\u000AZ%http/)", R"(1 0 0 0 2 5 5 5 9 10 13 13 1 sc://host/256)"},
    {R"(file:///C:/a/b)", R"(http://127.0.0.1.)", "removeCredentials", R"(%\u000Aa:/8080)", R"(1 1 0 0 4 7 7 7 16 17 17 17 1 http://127.0.0.1/)"},
    {R"(sc://host/path)", R"(/a,xn--\u001F\u20AC\u000A\u001F*\u20AC+] httpsxn--!0x(9127.0.0.1b)\u0000)", "setFragmentIdentifier", R"(..~[\uD8009 a\u00E9@xn--\u005C )", R"(1 0 0 0 2 5 5 5 9 10 71 71 1 sc://host/a,xn--%1F%E2%82%AC%1F*%E2%82%AC+]%20httpsxn--!0x(9127.0.0.1b)#..~[%EF%BF%BD9%20a%C3%A9@xn--\u005C)"},
    {R"(sc://host/path)", R"(/a,xn--\u001F\u20AC\u000A\u001F*\u20AC+] httpsxn--!0x(9127.0.0.1b)\u0000)", "setFragmentIdentifier", R"(file ]\u0009file)", R"(1 0 0 0 2 5 5 5 9 10 71 71 1 sc://host/a,xn--%1F%E2%82%AC%1F*%E2%82%AC+]%20httpsxn--!0x(9127.0.0.1b)#file%20]file)"},
    {R"(\N)", R"(file:0,\u001F]\u20ACb\u005C\u000A$[::1]% \u000A\u00E9@\u0000)", "setFragmentIdentifier", R"(@http::165536 file\u0009)", R"(1 0 0 0 4 7 7 7 7 25 42 42 1 file:///0,%1F]%E2%82%ACb/$[::1]%%20%C3%A9@#@http::165536%20file)"},
    {R"(file://host/share/x)", R"(//./].0x?\u3000]\u000A0x;_%=../0xc*!a9;)", "setFragmentIdentifier", R"(.)", R"(1 0 0 0 4 7 7 7 8 9 13 41 1 file://./].0x?%E3%80%80]0x;_%=../0xc*!a9;#.)"},
    {R"(http://h/a?b#c)", R"(..c https|%2E!)", "setFragmentIdentifier", R"(65536\u005C\uD800\u20AC%@+\u00E90http+file)", R"(1 1 0 0 4 7 7 7 8 9 25 25 1 http://h/..c%20https|%2E!#65536\u005C%EF%BF%BD%E2%82%AC%@+%C3%A90http+file)"},
    {R"(about:blank)", R"(ws:127.0.0.1,)", "setFragmentIdentifier", R"(\u005C:--_\u0009::1)", R"(1 0 0 0 2 5 5 5 15 16 16 16 1 ws://127.0.0.1,/#\u005C:--_::1)"},
    {R"(file:///C:/a/b)", R"(javascript:Z(\u00E9Ahttps#xn--\u0000;+)", "setFragmentIdentifier", R"(] )", R"(1 0 1 0 10 11 11 11 11 11 25 25 1 javascript:Z(%C3%A9Ahttps#])"},
    {R"(file:///C:/a/b)", R"(ws:./_:*)*/[::[::1]xn--file\u20AC\uD83D\uDE00xn--\u001F|0x(|(]b')", "setFragmentIdentifier", R"(:/@::1#xn--\u20AC+::1)", R"(1 0 0 0 2 5 5 5 6 13 66 66 1 ws://./_:*)*/[::[::1]xn--file%E2%82%AC%F0%9F%98%80xn--%1F|0x(|(]b'#:/@::1#xn--%E2%82%AC+::1)"},
    {R"(sc://host/path)", R"(ftp:*\u005C@#%/\u3000)[::1]xn--https@..;:!A=_,\u000A9)", "setFragmentIdentifier", R"(\u00E9][.)", R"(1 0 0 0 3 6 6 6 7 8 9 9 1 ftp://*/@#%C3%A9][.)"},
    {R"(http://example.org/foo/bar)", R"(/C|/foo/bar)", "setFragmentIdentifier", R"( xn--/\u20AC ])", R"(1 1 0 0 4 7 7 7 18 26 29 29 1 http://example.org/C|/foo/bar#%20xn--/%E2%82%AC%20])"},
    {R"(sc://host/path)", R"(]0,*\u00E9Z\u000AA\u0009@127.0.0.1)", "setFragmentIdentifier", R"(\uD83D\uDE00)", R"(1 0 0 0 2 5 5 5 9 10 32 32 1 sc://host/]0,*%C3%A9ZA@127.0.0.1#%F0%9F%98%80)"},
    {R"(sc://host/path)", R"(ftp:0_-$=)", "setFragmentIdentifier", R"()", R"(1 0 0 0 3 6 6 6 11 12 12 12 1 ftp://0_-$=/#)"},
    {R"(file://host/share/x)", R"(javascript:file:]+)", "setFragmentIdentifier", R"(%[9%2e\u20AC\u20AC-.filehttp..)", R"(1 0 1 0 10 11 11 11 11 11 18 18 1 javascript:file:]+#%[9%2e%E2%82%AC%E2%82%AC-.filehttp..)"},
    {R"(sc://host/path)", R"(ftp:aa\u005C+a\u005Cc-https)", "setFragmentIdentifier", R"(..0_file9)", R"(1 0 0 0 3 6 6 6 8 12 19 19 1 ftp://aa/+a/c-https#..0_file9)"},
    {R"(about:blank)", R"(#:https\u0009%2eC:0\u000Ahttp[::1](\u20AC)httpsc.)::\u001F)", "setFragmentIdentifier", R"(%@)", R"(1 0 1 0 5 6 6 6 6 6 11 11 1 about:blank#%@)"},
    {R"(http://example.org/foo/bar)", R"(..!./A0&\u20AC./https:0x_%2exn--*%2E)", "setFragmentIdentifier", R"(-)", R"(1 1 0 0 4 7 7 7 18 42 62 62 1 http://example.org/foo/..!./A0&%E2%82%AC./https:0x_%2exn--*%2E#-)"},
    {R"(http://h/a?b#c)", R"(ftp:0(\u005C)C:\u00E9=!,#]\u0000=[@)", "setFragmentIdentifier", R"()", R"(1 0 0 0 3 6 6 6 8 9 21 21 1 ftp://0(/)C:%C3%A9=!,#)"},
    {R"(sc://host/path)", R"(]-::[::1]!\uD83D\uDE00\u00E9b[)\u3000)", "setFragmentIdentifier", R"(\u005C::1#\u005Ca%2e)", R"(1 0 0 0 2 5 5 5 9 10 50 50 1 sc://host/]-::[::1]!%F0%9F%98%80%C3%A9b[)%E3%80%80#\u005C::1#\u005Ca%2e)"},
    {R"(http://example.org/foo/bar)", R"(xn--fa-hia.de)", "setFragmentIdentifier", R"(\W\u20ACxn--9%2e:)", R"(1 1 0 0 4 7 7 7 18 23 36 36 1 http://example.org/foo/xn--fa-hia.de#%E2%82%ACxn--9%2e:)"},
    {R"(http://h/a?b#c)", R"(https:\u001F..0x..\u005C0x9.\u3000\u0009|\u000Ahttp&0x=http)", "setFragmentIdentifier", R"(\Wfile xn--::1\u00E9)", R"(1 1 0 0 5 8 8 8 15 16 42 42 1 https://\u001F..0x../0x9.%E3%80%80|http&0x=http#file%20xn--::1%C3%A9)"},
    {R"(file://host/share/x)", R"(C:http[;&$[::1]'a::\u005C ..,\u001F..%/\u0000*)", "setFragmentIdentifier", R"()", R"(1 0 1 0 1 2 2 2 2 31 35 35 1 c:http[;&$[::1]'a::\u005C ..,%1F..%/%00*#)"},
    {R"(file://host/share/x)", R"(data:9\u0000%2E'_\u0009)", "setFragmentIdentifier", R"()", R"(1 0 1 0 4 5 5 5 5 5 14 14 1 data:9%00%2E'_#)"},
    {R"(about:blank)", R"(data:\u000A,((https9c,@)", "setFragmentIdentifier", R"(\u005C/?\u0009)", R"(1 0 1 0 4 5 5 5 5 5 17 17 1 data:,((https9c,@#\u005C/?)"},
    {R"(sc://host/path)", R"(.\u001F_\u0000a/[::1]file)", "setFragmentIdentifier", R"(\Whttp9[)", R"(1 0 0 0 2 5 5 5 9 20 29 29 0 sc://host/.%1F_%00a/[::1]file#http9[)"},
    {R"(http://example.org/foo/bar)", R"(file::Z[::1]@!_-file*=:\uD83D\uDE00)", "setFragmentIdentifier", R"(+_file%2e\u005C )", R"(1 0 0 0 4 7 7 7 7 8 38 38 1 file:///:Z[::1]@!_-file*=:%F0%9F%98%80#+_file%2e\u005C)"},
    {R"(sc://host/path)", R"(/!b ~|9]&Z\uD83D\uDE00)", "setHost", R"(xn--Zhttp65536::1?..file9::1)", R"(1 0 0 0 2 5 5 5 9 10 33 33 1 sc://host/!b%20~|9]&Z%F0%9F%98%80)"},
    {R"(http://example.org/foo/bar)", R"(http:..']\u0009::@\uD83D\uDE00..,%%])", "setHost", R"(::18080.)", R"(1 1 0 0 4 7 7 7 18 23 48 48 1 http://example.org/foo/..']::@%F0%9F%98%80..,%%])"},
    {R"(http://example.org/foo/bar)", R"(?'xn--#\u001F\u001F&\u00E9\u20AC*.C:0xfile_b\u00000x)", "setHost", R"(@\u0009)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://@\u0009/foo/bar?%27xn--#%1F%1F&%C3%A9%E2%82%AC*.C:0xfile_b%000x)"},
    {R"(file://host/share/x)", R"(sc:https&C:;~\u20AC\u0009https@%2e::%2E))", "setHost", R"(.../.].Z]65536\uD80065536)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://...https&C:;~%E2%82%AChttps@%2e::%2E))"},
    {R"(file://host/share/x)", R"(file:/@Z%[::1]\u0000!)[::1]_+.httpsxn--\u0000%2e~\u005C#?\uD83D\uDE00)", "setHost", R"(\uD83D\uDE00:~@\u005C \u00E9~xn--:65536)", R"(1 0 0 0 4 7 7 7 11 50 50 50 1 file://host/@Z%[::1]%00!)[::1]_+.httpsxn--%00%2e~/#?%F0%9F%98%80)"},
    {R"(about:blank)", R"(sc:\u0009 http127.0.0.1\u3000_$file[::1]$@)", "setHost", R"(]a\uD800::1)", R"(1 0 1 0 2 3 3 3 3 3 39 39 1 sc: http127.0.0.1%E3%80%80_$file[::1]$@)"},
    {R"(sc://host/path)", R"(data:\u0000\uD83D\uDE00[0xn--_~--xn--\u20AC\u20AC.:file]Z)", "setHost", R"(\u0009+%)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 data://\u0009+%%00%F0%9F%98%80[0xn--_~--xn--%E2%82%AC%E2%82%AC.:file]Z)"},
    {R"(sc://host/path)", R"(.-@C:C:%2e\uD83D\uDE00C:%2eC:%#./a+?|https)", "setHost", R"()", R"(1 0 0 0 2 5 5 5 5 6 36 36 0 sc:///.-@C:C:%2e%F0%9F%98%80C:%2eC:%#./a+?|https)"},
    {R"(file:///C:/a/b)", R"(.?0x':$\u3000\u005C\u0009!]\u00E9)", "setHost", R"()", R"(1 0 0 0 4 7 7 7 7 13 13 39 0 file:///C:/a/?0x%27:$%E3%80%80\u005C!]%C3%A9)"},
    {R"(file:///C:/a/b)", R"(/%60%7B%7D)", "setHost", R"(\u0009\u005C@xn--\u005C-@\uD83D\uDE00-)", R"(1 0 0 0 4 7 7 7 7 11 20 20 1 file:///C:/%60%7B%7D)"},
    {R"(http://example.org/foo/bar)", R"(ftp:Z-/\u000A=;https\uD83D\uDE00@#C:,_$,@http:\uD83D\uDE00)", "setHost", R"(\u005C.\uD800\u005C/[http)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 ftp:///=;https%F0%9F%98%80@#C:,_$,@http:%F0%9F%98%80)"},
    {R"(sc://host/path)", R"(C:\u3000)", "setHost", R"(\N)", R"(1 0 0 0 1 4 4 4 13 13 13 13 0 c://%E3%80%80)"},
    {R"(sc://host/path)", R"(foo://host/#\u0007 a</)", "setHost", R"(a\u00E9_+\u00E9@::1)", R"(1 0 0 0 3 6 6 6 10 11 11 11 1 foo://host/#%07%20a%3C/)"},
    {R"(http://example.org/foo/bar)", R"(w:/.//)", "setHost", R"(~.)", R"(1 0 0 0 1 4 4 4 6 8 8 8 1 w://~.//)"},
    {R"(about:blank)", R"(a:[http\u3000,'b~[\u001F=\u00E9Z&?!)", "setHost", R"(+]_/\u005C%)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 a://+]_[http%E3%80%80,'b~[%1F=%C3%A9Z&?!)"},
    {R"(sc://host/path)", R"(http://h%6fsT)", "setHost", R"(%2e_-+65536?\u005C+xn--/)", R"(1 1 0 0 4 7 7 7 16 17 17 17 1 http://._-+65536/)"},
    {R"(http://example.org/foo/bar)", R"(#\u00E9https-0xhttp\u000A!=)", "setHost", R"(\u0009:)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar#%C3%A9https-0xhttp!=)"},
    {R"(about:blank)", R"(http://user:pass@webkit.org:123/path)", "setHost", R"(\W#::1.#\uD800\u0009@)", R"(1 1 0 4 4 7 11 16 27 32 36 36 1 http://user:pass@webkit.org:123/path)"},
    {R"(file://host/share/x)", R"(data:')", "setHost", R"(\W\u005C[]\u005C-0)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 data://\u005C[]\u005C-0')"},
    {R"(http://example.org/foo/bar)", R"(a:::)https9..)", "setHost", R"(9http_\u20AC::1?]\u00E9\u000A0?)", R"(1 0 1 0 1 2 2 2 2 2 13 13 1 a:::)https9..)"},
    {R"(file:///C:/a/b)", R"(..=xn--)", "setHost", R"(]%2e# _]\u20AC0)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 file://]%2e/C:/a/..=xn--)"},
    {R"(file:///C:/a/b)", R"(javascript:;bhttpZ \u00090x::'/C:+\u20AC_A./..)", "setHost", R"(-9--8080file)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 javascript://-9--8080file;bhttpZ 0x::'/C:+%E2%82%AC_A./..)"},
    {R"(http://example.org/foo/bar)", R"(?A\u005C*$[::1])", "setHost", R"(file)", R"(1 1 0 0 4 7 7 7 11 16 19 29 0 http://file/foo/bar?A\u005C*$[::1])"},
    {R"(http://h/a?b#c)", R"(data:[:c\u20AC9)", "setHost", R"(httpZ9%65536)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 data://httpZ9%65536[:c%E2%82%AC9)"},
    {R"(file:///C:/a/b)", R"(..]'!?0a'http#@,C:%2e%:-)", "setHost", R"(..)", R"(1 0 0 0 4 7 7 7 9 15 20 30 0 file://../C:/a/..]'!?0a%27http#@,C:%2e%:-)"},
    {R"(file://host/share/x)", R"(data:Zfile_./)%2E.._\u0000@b@)", "setHostAndPort", R"(::1)", R"(1 0 1 0 4 5 5 5 5 13 26 26 1 data:Zfile_./)%2E.._%00@b@)"},
    {R"(http://example.org/foo/bar)", R"(foo://host/#%D0%9F%D0%9F%07%20a%3C/)", "setHostAndPort", R"(\W.\u0009-Zhttp_)", R"(1 0 0 0 3 6 6 6 14 15 15 15 1 foo://.-Zhttp_/#%D0%9F%D0%9F%07%20a%3C/)"},
    {R"(http://user:pass@host:8080/p?q#f)", R"(http://[::f\u0009:0:0:f:0:0])", "setHostAndPort", R"(.+::1:)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://[::f:0:0:f:0:0]/)"},
    {R"(http://example.org/foo/bar)", R"(http://user:@host)", "setHostAndPort", R"(\W:8080%2e@a_]./)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://user@/)"},
    {R"(about:blank)", R"(C:\u3000./[::1]\u3000~[%2E..\u3000..127.0.0.1file)", "setHostAndPort", R"(.-%2e @9-:file\u20AC:)", R"(1 0 1 0 1 2 2 2 2 13 58 58 1 c:%E3%80%80./[::1]%E3%80%80~[%2E..%E3%80%80..127.0.0.1file)"},
    {R"(http://example.org/foo/bar)", R"(#http&%2e\u0000/~cC:?\u000A|[)", "setHostAndPort", R"(file~..\u005C:@0)", R"(1 1 0 0 4 7 7 7 14 20 23 23 1 http://file~..//foo/bar#http&%2e%00/~cC:?|[)"},
    {R"(http://example.org/foo/bar)", R"(/:)", "setHostAndPort", R"(\N)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:///:)"},
    {R"(file://host/share/x)", R"(a:xn--c\u005C$:(./\u0000]127.0.0.1.\uD83D\uDE00\u0009\u001F\u005C)", "setHostAndPort", R"(%%2efile@ Z8080\u20AC::1)", R"(1 0 1 0 1 2 2 2 2 13 43 43 1 a:xn--c\u005C$:(./%00]127.0.0.1.%F0%9F%98%80%1F\u005C)"},
    {R"(http://example.org/foo/bar)", R"(http:])", "setHostAndPort", R"()", R"(1 1 0 0 4 7 7 7 10 11 12 12 1 http://foo/])"},
    {R"(about:blank)", R"(https:0_/\u00E9@]@%2e=C:%2eA\u001F127.0.0.1)", "setHostAndPort", R"()", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https:///%C3%A9@]@%2e=C:%2eA%1F127.0.0.1)"},
    {R"(http://example.org/foo/bar)", R"(a:0x\u00E9https*(]_#):C:https_file\u0000@;\u001F)", "setHostAndPort", R"(\N)", R"(1 0 1 0 1 2 2 2 2 2 19 19 1 a:0x%C3%A9https*(]_#):C:https_file%00@;)"},
    {R"(file://host/share/x)", R"(javascript:file:]+)", "setHostAndPort", R"(80809]\u000A65536%[)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 javascript://80809]\u000A65536%[file:]+)"},
    {R"(http://example.org/foo/bar)", R"(/:)", "setHostAndPort", R"(+)", R"(1 1 0 0 4 7 7 7 8 9 10 10 0 http://+/:)"},
    {R"(file://host/share/x)", R"(?Z(=127.0.0.1(%2E./_# %2E\u001F +)", "setHostAndPort", R"([aafilea8080:65536\u0009%2e\u0009)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 file://[aafilea8080/share/x?Z(=127.0.0.1(%2E./_#%20%2E%1F%20+)"},
    {R"(\N)", R"(ws:%2eC:Z_0]a\u3000@$0x)", "setHostAndPort", R"(\N)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 ws://%2eC:Z_0%5Da%E3%80%80@/)"},
    {R"(http://h/a?b#c)", R"(ws:127.0.0.1?xn--[::1](xn--c)", "setHostAndPort", R"(\N)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 ws:///?xn--[::1](xn--c)"},
    {R"(sc://host/path)", R"(C:Z$C:./?!&./0x.!\uD83D\uDE00::\u3000)[::1]_%2e]\u20AC0x#)", "setHostAndPort", R"(@@\u000A\u00E9\u005Cxn--_:%\u000A:)", R"(1 0 1 0 1 2 2 2 2 8 8 62 1 c:Z$C:./?!&./0x.!%F0%9F%98%80::%E3%80%80)[::1]_%2e]%E2%82%AC0x#)"},
    {R"(file:///C:/a/b)", R"(fragment)", "setHostAndPort", R"()", R"(1 0 0 0 4 7 7 7 7 13 21 21 1 file:///C:/a/fragment)"},
    {R"(sc://host/path)", R"(]-::[::1]!\uD83D\uDE00\u00E9b[)\u3000)", "setHostAndPort", R"(+-+\u005C)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://+-+\u005C/]-::[::1]!%F0%9F%98%80%C3%A9b[)%E3%80%80)"},
    {R"(about:blank)", R"(http://127.00.0.1/)", "setHostAndPort", R"(%2e\u005Ca\u00E9\u20AC_%0::1a)", R"(1 1 0 0 4 7 7 7 16 17 17 17 1 http://127.0.0.1/)"},
    {R"(sc://host/path)", R"(/[C:)*[::1]A*\u00E90,ac0https\uD83D\uDE00])", "setHostAndPort", R"(\W/%2e+//xn--_)", R"(1 0 0 0 2 5 5 5 5 18 59 59 0 sc:///%2e+//xn--_/[C:)*[::1]A*%C3%A90,ac0https%F0%9F%98%80])"},
    {R"(http://example.org/foo/bar)", R"(http:])", "setHostAndPort", R"([?)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[?/foo/])"},
    {R"(http://example.org/foo/bar)", R"(file:%!(xn--C:127.0.0.1./\u00E9A 127.0.0.1\u00E9\u20AC@C:\u000A)", "setHostAndPort", R"(a)", R"(1 0 0 0 4 7 7 7 8 29 66 66 0 file://a/%!(xn--C:127.0.0.1./%C3%A9A%20127.0.0.1%C3%A9%E2%82%AC@C:)"},
    {R"(about:blank)", R"(http://www.google.com/foo?bar=baz# \u00BB)", "setHostAndPort", R"(\u0009)", R"(1 1 0 0 4 7 7 7 10 11 11 19 1 http://foo/?bar=baz#%20%C2%BB)"},
    {R"(\N)", R"(file:///C:/)", "setHostAndPort", R"(\Wa)", R"(1 0 0 0 4 7 7 7 8 12 12 12 0 file://a/C:/)"},
    {R"(http://example.org/foo/bar)", R"(../-%2e%2eA+@ab..)", "setPath", R"([\u0009file\u00E9a:file[\u20AC8080[)", R"(1 1 0 0 4 7 7 7 18 19 51 51 1 http://example.org/[file%C3%A9a:file[%E2%82%AC8080[)"},
    {R"(\N)", R"(javascript:[::1]\u005C[::1]A:-(%2e\u000A::http0x)Z'xn--[::1]\u20AC\u0000file$0xxn--)", "setPath", R"(\WZ?@xn--#file)", R"(1 0 0 0 10 11 11 11 11 12 28 28 1 javascript:/Z%3F@xn--%23file)"},
    {R"(file://host/share/x)", R"(/b\u0000)", "setPath", R"(8080\u005C..~/\u20AC?\u20AC~http\u0009)", R"(1 0 0 0 4 7 7 7 11 21 65 65 1 file://host/8080/..~/%C3%A2%C2%82%C2%AC%3F%C3%A2%C2%82%C2%AC~http)"},
    {R"(\N)", R"(data:%* ::0x$[%2E~./127.0.0.1\u000A%2E=*$%2ebC:@xn--a()", "setPath", R"(\W]\u0009)", R"(1 0 0 0 4 5 5 5 5 6 7 7 1 data:/])"},
    {R"(file:///C:/a/b)", R"(.file\u0009\u0000 0|&::A%\u3000)\u0000#a%2E$$)", "setPath", R"(8080\uD83D\uDE00~::1-xn--file\u20AC)", R"(1 0 0 0 4 7 7 7 7 8 46 46 1 file:///8080%F0%9F%98%80~::1-xn--file%E2%82%AC#a%2E$$)"},
    {R"(file://host/share/x)", R"(file:!\u3000http\u001F,)", "setPath", R"(\uD8009Z[]9)", R"(1 0 0 0 4 7 7 7 11 12 26 26 1 file://host/%EF%BF%BD9Z[]9)"},
    {R"(file:///C:/a/b)", R"(?)", "setPath", R"(\W[-Z@)", R"(1 0 0 0 4 7 7 7 7 8 12 13 0 file:///[-Z@?)"},
    {R"(http://example.org/foo/bar)", R"(http://user:@host)", "setPath", R"(::1_xn--\u000A++\u20AC\uD800)", R"(1 1 0 0 4 7 11 11 16 17 45 45 1 http://user@host/::1_xn--++%E2%82%AC%EF%BF%BD)"},
    {R"(file://host/share/x)", R"(file:,'*xn--)", "setPath", R"(]\u00E9)", R"(1 0 0 0 4 7 7 7 11 12 19 19 1 file://host/]%C3%A9)"},
    {R"(\N)", R"(a:[\u3000[Ahttp%2e%2E'[%2E)", "setPath", R"(# /]%2e-\uD83D\uDE00)", R"(1 0 0 0 1 2 2 2 2 10 39 39 1 a:/%23%20/]%2e-%C3%B0%C2%9F%C2%98%C2%80)"},
    {R"(sc://host/path)", R"(?127.0.0.1%!\u0009https\u00E9)", "setPath", R"(\W/@filea8080\u000AZxn--::1\u005C@)", R"(1 0 0 0 2 5 5 5 9 10 30 53 1 sc://host/@filea8080Zxn--::1\u005C@?127.0.0.1%!https%C3%A9)"},
    {R"(file:///C:/a/b)", R"(ws:./_:*)*/[::[::1]xn--file\u20AC\uD83D\uDE00xn--\u001F|0x(|(]b')", "setPath", R"()", R"(1 0 0 0 2 5 5 5 6 7 7 7 1 ws://./)"},
    {R"(file://host/share/x)", R"(.?(--$ a0x-\u20AC127.0.0.1$\uD83D\uDE00aC:?)", "setPath", R"(98080[\u005Ca\u00E9]http\uD800].)", R"(1 0 0 0 4 7 7 7 11 19 42 89 1 file://host/98080[/a%C3%A9]http%EF%BF%BD].?(--$%20a0x-%E2%82%AC127.0.0.1$%F0%9F%98%80aC:?)"},
    {R"(about:blank)", R"(#127.0.0.1127.0.0.1|\u20AC\u00E9*@09)", "setPath", R"(\u000A%ahttp@)", R"(1 0 0 0 5 6 6 6 6 7 14 14 1 about:/%ahttp@#127.0.0.1127.0.0.1|%E2%82%AC%C3%A9*@09)"},
    {R"(sc://host/path)", R"(sc:*ac_\u005C[::1]..::|&xn--\u0000\uD83D\uDE00?xn--[::1]httpsAhttps%2e)", "setPath", R"(\uD800file\u0009::1+\u005C)", R"(1 0 0 0 2 3 3 3 3 4 22 46 1 sc:/%EF%BF%BDfile::1+\u005C?xn--[::1]httpsAhttps%2e)"},
    {R"(file:///C:/a/b)", R"(//\u001Fa\u005ChttpsafileaZ+%+/%()", "setPath", R"(\W65536\u00E9)", R"(1 0 0 0 4 7 7 7 9 10 21 21 1 file://\u001Fa/65536%C3%A9)"},
    {R"(sc://host/path)", R"(./[::1].c[.\u3000%::[::1]\u0000,http#a:0x,)", "setPath", R"(\N)", R"(1 0 0 0 2 5 5 5 9 10 10 10 1 sc://host/#a:0x,)"},
    {R"(http://h/a?b#c)", R"(/:#)", "setPath", R"(0\u005C\u005C..9\u20AC::1_..)", R"(1 1 0 0 4 7 7 7 8 12 30 30 1 http://h/0//..9%E2%82%AC::1_..#)"},
    {R"(file://host/share/x)", R"(%2E]\u30000xhttp_(..)", "setPath", R"(.%/)", R"(1 0 0 0 4 7 7 7 11 15 15 15 1 file://host/.%/)"},
    {R"(file:///C:/a/b)", R"(ftp:-./)xn--!*http)", "setPath", R"(\N)", R"(1 0 0 0 3 6 6 6 8 9 9 9 1 ftp://-./)"},
    {R"(file://host/share/x)", R"(sc:https&C:;~\u20AC\u0009https@%2e::%2E))", "setPath", R"(0Z)", R"(1 0 0 0 2 3 3 3 3 4 6 6 1 sc:/0Z)"},
    {R"(\N)", R"(file:([::1]./..=%2e)\u00E9=\u005C+C:C:-)", "setPath", R"(\N)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(http://example.org/foo/bar)", R"(a:!_$\u001F?|/()", "setPath", R"(%xn--:%%2e Z)", R"(1 0 0 0 1 2 2 2 2 3 17 21 1 a:/%xn--:%%2e%20Z?|/()"},
    {R"(http://example.org/foo/bar)", R"(javascript:\u20AC%2eC:?\u00E9%2Ec9[?|,=)", "setPath", R"(_)", R"(1 0 0 0 10 11 11 11 11 12 13 30 1 javascript:/_?%C3%A9%2Ec9[?|,=)"},
    {R"(sc://host/path)", R"(javascript:9https\uD83D\uDE00)", "setPath", R"(.65536:Z+)", R"(1 0 0 0 10 11 11 11 11 12 21 21 1 javascript:/.65536:Z+)"},
    {R"(file://host/share/x)", R"(ws:_[::1](|%2e%xn--%[::1]@file)", "setProtocol", R"(_Zaxn--)", R"(R0 1 0 0 0 2 5 9 43 48 49 49 49 1 ws://_%5B:%3A1%5D(%7C%2e%xn--%%5B%3A%3A1%5D@file/)"},
    {R"(about:blank)", R"(http://host/path%2e.%2E)", "setProtocol", R"(\u005C+00\u00E9)", R"(R0 1 1 0 0 4 7 7 7 11 12 23 23 1 http://host/path%2e.%2E)"},
    {R"(http://user:pass@host:8080/p?q#f)", R"(foo:/#)", "setProtocol", R"(\W.0\u20AC\u20AC..~65536%2e65536)", R"(R0 1 0 0 0 3 4 4 4 4 5 5 5 1 foo:/#)"},
    {R"(about:blank)", R"(data:=http(-[+[@127.0.0.1fileC:=\u005C::\u20ACc\u20AC!Z)", "setProtocol", R"(%2e\u00E98080[8080-#%? \u00E9http)", R"(R0 1 0 1 0 4 5 5 5 5 5 56 56 1 data:=http(-[+[@127.0.0.1fileC:=\u005C::%E2%82%ACc%E2%82%AC!Z)"},
    {R"(http://example.org/foo/bar)", R"(/%[::1]0x;-\u30009http_A)", "setProtocol", R"(\W65536xn--808065536)", R"(R0 1 1 0 0 4 7 7 7 18 19 45 45 1 http://example.org/%[::1]0x;-%E3%80%809http_A)"},
    {R"(http://example.org/foo/bar)", R"(javascript:..[::1](%2E::b\u00E99,)", "setProtocol", R"(.-9file-/-..@file%xn--)", R"(R0 1 0 1 0 10 11 11 11 11 11 33 33 1 javascript:..[::1](%2E::b%C3%A99,)"},
    {R"(http://user:pass@host:8080/p?q#f)", R"(\u005C\u005Cfoo.com)", "setProtocol", R"(\N)", R"(R0 1 1 0 0 4 7 7 7 14 15 15 15 1 http://foo.com/)"},
    {R"(sc://host/path)", R"(?\u000A&_ @=..aZ/#.\u3000?xn--\u005C~ \u3000)", "setProtocol", R"(Z\uD83D\uDE00][0?.\uD83D\uDE00)", R"(R0 1 0 0 0 2 5 5 5 9 10 14 27 1 sc://host/path?&_%20@=..aZ/#.%E3%80%80?xn--\u005C~%20%E3%80%80)"},
    {R"(about:blank)", R"(data:)]c(&:.+,._\u0000)", "setProtocol", R"(:http~ %2e[\uD83D\uDE00)", R"(R0 1 0 1 0 4 5 5 5 5 5 16 16 1 data:)]c(&:.+,._)"},
    {R"(sc://host/path)", R"(]-::[::1]!\uD83D\uDE00\u00E9b[)\u3000)", "setProtocol", R"(/#\u000A:-65536\uD800~[65536)", R"(R0 1 0 0 0 2 5 5 5 9 10 50 50 1 sc://host/]-::[::1]!%F0%9F%98%80%C3%A9b[)%E3%80%80)"},
    {R"(file://host/share/x)", R"(javascript:#%(\u20ACA=[httphttpsZ[::1]\u0000%2e)", "setProtocol", R"(:#\u20AC::1\u20AC/xn--/?\u0009)", R"(R0 1 0 1 0 10 11 11 11 11 11 11 11 1 javascript:#%(%E2%82%ACA=[httphttpsZ[::1]%00%2e)"},
    {R"(sc://host/path)", R"(http://host:81)", "setProtocol", R"(]65536_http8080\u00E9%\u0009a)", R"(R0 1 1 0 3 4 7 7 7 11 15 15 15 1 http://host:81/)"},
    {R"(http://example.org/foo/bar)", R"(..\u005C\u005C)", "setProtocol", R"()", R"(R0 1 1 0 0 4 7 7 7 18 20 20 20 1 http://example.org//)"},
    {R"(http://example.org/foo/bar)", R"(C:?\u0000'Z)", "setProtocol", R"(\u0009)", R"(R0 1 0 1 0 1 2 2 2 2 2 2 8 1 c:?%00'Z)"},
    {R"(http://example.org/foo/bar)", R"([a:b::c:d:e:f:127.0.0.1])", "setProtocol", R"([.\u0009%a]65536)", R"(R0 1 1 0 0 4 7 7 7 18 23 47 47 1 http://example.org/foo/[a:b::c:d:e:f:127.0.0.1])"},
    {R"(http://example.org/foo/bar)", R"(http://[a:b:c:d:e::127.0.0.1]/)", "setProtocol", R"(.\uD800@-\uD800??\uD83D\uDE00xn--..)", R"(R0 1 1 0 0 4 7 7 7 27 28 28 28 1 http://[a:b:c:d:e:0:7f00:1]/)"},
    {R"(http://example.org/foo/bar)", R"(r:/)", "setProtocol", R"(%0_%2e\u00E90\u005C.._)", R"(R0 1 0 0 0 1 2 2 2 2 3 3 3 1 r:/)"},
    {R"(file:///C:/a/b)", R"(https%2E99::\u000Axn--file\u001Fhttp)", "setProtocol", R"(\u00E9?\u005C\uD83D\uDE00/?)", R"(R0 1 0 0 0 4 7 7 7 7 13 40 40 1 file:///C:/a/https%2E99::xn--file%1Fhttp)"},
    {R"(about:blank)", R"(javascript:127.0.0.1%\u001Fa+!:|\uD83D\uDE00~xn--127.0.0.1A.0x._\u000Ahttp%2E)", "setProtocol", R"(http%2e\u000A\u00E9a9~_\uD83D\uDE000xn--)", R"(R0 1 0 1 0 10 11 11 11 11 11 68 68 1 javascript:127.0.0.1%%1Fa+!:|%F0%9F%98%80~xn--127.0.0.1A.0x._http%2E)"},
    {R"(file://host/share/x)", R"(ftp:$@-==c'./..?[::1])", "setProtocol", R"(\W\u00E9##file..%2eZ)", R"(R0 1 0 0 0 3 6 7 7 14 15 15 21 1 ftp://$@-==c'./?[::1])"},
    {R"(file:///C:/a/b)", R"(#\u3000[::1]:c)", "setProtocol", R"(%2e..]\u20AC...)", R"(R0 1 0 0 0 4 7 7 7 7 13 14 14 1 file:///C:/a/b#%E3%80%80[::1]:c)"},
    {R"(file://host/share/x)", R"(file:*()", "setProtocol", R"( 8080\uD800@\uD83D\uDE00xn--)", R"(R0 1 0 0 0 4 7 7 7 11 18 20 20 1 file://host/share/*()"},
    {R"(\N)", R"(http://example.com/path1/path2/..#fragment)", "setProtocol", R"(\N)", R"(R0 1 1 0 0 4 7 7 7 18 25 25 25 1 http://example.com/path1/#fragment)"},
    {R"(http://h/a?b#c)", R"(//Z*/&09filec\u0000\u00E9!/,%2E)", "setProtocol", R"(http/0:9xn--@@)", R"(R0 1 1 0 0 4 7 7 7 9 29 33 33 1 http://z*/&09filec%00%C3%A9!/,%2E)"},
    {R"(http://user:pass@host:8080/p?q#f)", R"(http://f:0/c)", "setProtocol", R"(?::1xn--9)", R"(R0 1 1 0 2 4 7 7 7 8 11 12 12 1 http://f:0/c)"},
    {R"(sc://host/path)", R"(ftp:&\u005C*$&%2e@!\u20AC[.._\u001F+[9$ac[::1]'[%)", "setQuery", R"(file/.?_\u20AC%+\u000A)", R"(1 0 0 0 3 6 6 6 7 8 46 66 1 ftp://&/*$&%2e@!%E2%82%AC[.._%1F+[9$ac[::1]'[%?file/.?_%E2%82%AC%+)"},
    {R"(file://host/share/x)", R"(?Z(=127.0.0.1(%2E./_# %2E\u001F +)", "setQuery", R"(ZZ_ :)", R"(1 0 0 0 4 7 7 7 11 18 19 27 1 file://host/share/x?ZZ_%20:#%20%2E%1F%20+)"},
    {R"(file://host/share/x)", R"(.&_,cc%|https%2E\uD83D\uDE00../a%2e)", "setQuery", R"(http)", R"(1 0 0 0 4 7 7 7 11 49 53 58 1 file://host/share/.&_,cc%|https%2E%F0%9F%98%80../a%2e?http)"},
    {R"(http://user:pass@host:8080/p?q#f)", R"(about)", "setQuery", R"(\W.. #::18080file..?0%)", R"(1 1 0 5 4 7 11 16 21 27 32 38 1 http://user:pass@host:8080/about?..%20#::18080file..?0%)"},
    {R"(file://host/share/x)", R"(.?(--$ a0x-\u20AC127.0.0.1$\uD83D\uDE00aC:?)", "setQuery", R"(\W\u20AC..9a\u20AC)", R"(1 0 0 0 4 7 7 7 11 18 18 41 1 file://host/share/?%E2%82%AC..9a%E2%82%AC)"},
    {R"(sc://host/path)", R"(data:\uD83D\uDE00)https\u0000fileC:+??0x|0&%2E)", "setQuery", R"(-::1-/\u20AC%@?)", R"(1 0 1 0 4 5 5 5 5 5 33 52 1 data:%F0%9F%98%80)https%00fileC:+?-::1-/%E2%82%AC%@?)"},
    {R"(sc://host/path)", R"(data:@*_)", "setQuery", R"(~\u000Ahttp)", R"(1 0 1 0 4 5 5 5 5 5 8 14 1 data:@*_?~http)"},
    {R"(about:blank)", R"(data:)]c(&:.+,._\u0000)", "setQuery", R"(..file:_65536%2e\uD83D\uDE00)", R"(1 0 1 0 4 5 5 5 5 5 16 45 1 data:)]c(&:.+,._?..file:_65536%2e%F0%9F%98%80)"},
    {R"(file:///C:/a/b)", R"(file:#b_=/(]\u3000c [::1]127.0.0.1| )", "setQuery", R"()", R"(1 0 0 0 4 7 7 7 7 13 14 15 1 file:///C:/a/b?#b_=/(]%E3%80%80c%20[::1]127.0.0.1|)"},
    {R"(about:blank)", R"(https:c\u000A\u005CC:\u20ACb-b)9]\u20AC)", "setQuery", R"(/Z)", R"(1 1 0 0 5 8 8 8 9 10 36 39 1 https://c/C:%E2%82%ACb-b)9]%E2%82%AC?/Z)"},
    {R"(file://host/share/x)", R"(file:::c! \u00E9[%2Efile)", "setQuery", R"(\u20ACZ65536@\u0009#)", R"(1 0 0 0 4 7 7 7 11 18 39 56 1 file://host/share/::c!%20%C3%A9[%2Efile?%E2%82%ACZ65536@#)"},
    {R"(sc://host/path)", R"(.-@C:C:%2e\uD83D\uDE00C:%2eC:%#./a+?|https)", "setQuery", R"(Z\u005C::1?\u0009+Z)", R"(1 0 0 0 2 5 5 5 9 10 40 49 1 sc://host/.-@C:C:%2e%F0%9F%98%80C:%2eC:%?Z\u005C::1?+Z#./a+?|https)"},
    {R"(file://host/share/x)", R"(.\u0000\u000A/%2e)", "setQuery", R"(\WZ\u20AC\u000Afile@?::1)", R"(1 0 0 0 4 7 7 7 11 23 23 43 1 file://host/share/.%00/?Z%E2%82%ACfile@?::1)"},
    {R"(file://host/share/x)", R"(?Z(=127.0.0.1(%2E./_# %2E\u001F +)", "setQuery", R"(9xn--.[:~%Z~]Z8080)", R"(1 0 0 0 4 7 7 7 11 18 19 38 1 file://host/share/x?9xn--.[:~%Z~]Z8080#%20%2E%1F%20+)"},
    {R"(about:blank)", R"(http://user:pass@webkit.org:123/path)", "setQuery", R"(_%?+\u000A~file\u20AC[\uD83D\uDE00[%2e)", R"(1 1 0 4 4 7 11 16 27 32 36 72 1 http://user:pass@webkit.org:123/path?_%?+~file%E2%82%AC[%F0%9F%98%80[%2e)"},
    {R"(http://example.org/foo/bar)", R"(?A\u005C*$[::1])", "setQuery", R"(])", R"(1 1 0 0 4 7 7 7 18 23 26 28 1 http://example.org/foo/bar?])"},
    {R"(sc://host/path)", R"(sc:*ac_\u005C[::1]..::|&xn--\u0000\uD83D\uDE00?xn--[::1]httpsAhttps%2e)", "setQuery", R"(\u20AC_\uD800~\u000A)", R"(1 0 1 0 2 3 3 3 3 3 38 59 1 sc:*ac_\u005C[::1]..::|&xn--%00%F0%9F%98%80?%E2%82%AC_%EF%BF%BD~)"},
    {R"(http://h/a?b#c)", R"(..httpA\u001F C:+https*httpxn--\u000A)", "setQuery", R"([a:][\u000Aa-#)", R"(1 1 0 0 4 7 7 7 8 9 39 47 1 http://h/..httpA%1F%20C:+https*httpxn--?[a:][a-#)"},
    {R"(file:///C:/a/b)", R"(?)", "setQuery", R"(\u00E9%2e::19xn--.\u20AC9)", R"(1 0 0 0 4 7 7 7 7 13 14 43 1 file:///C:/a/b?%C3%A9%2e::19xn--.%E2%82%AC9)"},
    {R"(file:///C:/a/b)", R"(..c_\u005C\u00E9\u001F_A,&[::1].-:-http=.._https*?)", "setQuery", R"(/ Z:]_[~+file/)", R"(1 0 0 0 4 7 7 7 7 18 54 71 1 file:///C:/a/..c_/%C3%A9%1F_A,&[::1].-:-http=.._https*?/%20Z:]_[~+file/)"},
    {R"(file://host/share/x)", R"(..::=$_A./#Z\u000A!?\u20AC|)", "setQuery", R"(::1http#65536-)", R"(1 0 0 0 4 7 7 7 11 28 28 36 1 file://host/share/..::=$_A./?::1http#65536-#Z!?%E2%82%AC|)"},
    {R"(file://host/share/x)", R"(data:https#(%2E&!&::::a,[chttp0',[#)", "setQuery", R"(\u00E9xn--\u00E9%8080)", R"(1 0 1 0 4 5 5 5 5 5 10 32 1 data:https?%C3%A9xn--%C3%A9%8080#(%2E&!&::::a,[chttp0',[#)"},
    {R"(http://example.org/foo/bar)", R"(http:example.com/)", "setQuery", R"(9-65536\u005C::1#..xn--90)", R"(1 1 0 0 4 7 7 7 18 35 35 47 1 http://example.org/foo/example.com/?9-65536\u005C::1#..xn--90)"},
    {R"(http://h/a?b#c)", R"(.\uD83D\uDE00\u0009;xn--\u005C%\u0009https[http,~#..? ]\u0009!\u0009http\u0000?)", "setQuery", R"(xn--8080a\u00E9%2efile)", R"(1 1 0 0 4 7 7 7 8 28 41 64 1 http://h/.%F0%9F%98%80;xn--/%https[http,~?xn--8080a%C3%A9%2efile#..?%20]!http%00?)"},
    {R"(file://host/share/x)", R"(/~a[127.0.0.1ba&(&\u005C\u3000http\uD83D\uDE00http#@)", "setQuery", R"(%2e..[9http[.\u005C8080::1)", R"(1 0 0 0 4 7 7 7 11 30 59 81 1 file://host/~a[127.0.0.1ba&(&/%E3%80%80http%F0%9F%98%80http?%2e..[9http[.\u005C8080::1#@)"},
    {R"(http://user:pass@host:8080/p?q#f)", R"(ftp://host:22)", "setUser", R"(~8080)", R"(1 0 0 3 3 6 11 11 16 20 20 20 1 ftp://~8080@host:22/)"},
    {R"(http://h/a?b#c)", R"(..ahttp0x\u000099127.0.0.1Z)", "setUser", R"(\N)", R"(1 1 0 0 4 7 7 7 8 9 33 33 1 http://h/..ahttp0x%0099127.0.0.1Z)"},
    {R"(file:///C:/a/b)", R"(http://[0::0]/)", "setUser", R"(655360)", R"(1 1 0 0 4 7 13 13 18 19 19 19 1 http://655360@[::]/)"},
    {R"(sc://host/path)", R"(..,;@-++!@\u0000@\u0009)", "setUser", R"(_ \u00E9a+Za)", R"(1 0 0 0 2 5 19 19 24 25 39 39 1 sc://_%20%C3%A9a+Za@host/..,;@-++!@%00@)"},
    {R"(sc://host/path)", R"(sc:*ac_\u005C[::1]..::|&xn--\u0000\uD83D\uDE00?xn--[::1]httpsAhttps%2e)", "setUser", R"(?~)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://%3F~@*ac_\u005C[::1]..::|&xn--%00%F0%9F%98%80?xn--[::1]httpsAhttps%2e)"},
    {R"(file:///C:/a/b)", R"(file:0=$)", "setUser", R"( _?#~)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://%20_%3F%23~@/C:/a/0=$)"},
    {R"(file://host/share/x)", R"(ws:0',\u000AZ,~./!%2e[127.0.0.1\u00E9-*\u00090x.*\u20AChttps9..9)", "setUser", R"(]afile)", R"(1 0 0 0 2 5 13 13 21 22 66 66 1 ws://%5Dafile@0',z,~./!%2e[127.0.0.1%C3%A9-*0x.*%E2%82%AChttps9..9)"},
    {R"(sc://host/path)", R"(..,;@-++!@\u0000@\u0009)", "setUser", R"(\N)", R"(1 0 0 0 2 5 5 5 9 10 24 24 1 sc://host/..,;@-++!@%00@)"},
    {R"(http://h/a?b#c)", R"(C:+\u0009|?=:#_\u3000#[+)", "setUser", R"(:..99[#http::1.)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 c://%3A..99%5B%23http%3A%3A1.@+|?=:#_%E3%80%80#[+)"},
    {R"(http://example.org/foo/bar)", R"(127.0.0.1='https%2eC:file@#[::1]\u20ACa\uD83D\uDE00)", "setUser", R"(\N)", R"(1 1 0 0 4 7 7 7 18 23 49 49 1 http://example.org/foo/127.0.0.1='https%2eC:file@#[::1]%E2%82%ACa%F0%9F%98%80)"},
    {R"(file:///C:/a/b)", R"(@test@test@example)", "setUser", R"(#[:)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://%23%5B%3A@/C:/a/@test@test@example)"},
    {R"(file:///C:/a/b)", R"(notspecial:/notuser:notpassword@nothost)", "setUser", R"(\u20AC %)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 notspecial://%E2%82%AC%20%@/notuser:notpassword@nothost)"},
    {R"(file:///C:/a/b)", R"(a:\u001F\u0000%2E$..=..@%/https[xn--%2e+)", "setUser", R"(~9 \u000A\u0009\uD800\u000Aa)", R"(1 0 0 0 1 4 48 48 50 51 65 65 1 a://~9%20%0A%09%EF%BF%BD%0Aa%40%1F%00%2E$..%3D..@%/https[xn--%2e+)"},
    {R"(sc://host/path)", R"(.\u001F_\u0000a/[::1]file)", "setUser", R"(\W\u0009@\u000A\u00E9a@_])", R"(1 0 0 0 2 5 28 28 33 44 53 53 1 sc://%09%40%0A%C3%A9a%40_%5D@host/.%1F_%00a/[::1]file)"},
    {R"(http://user:pass@host:8080/p?q#f)", R"(http://example.com/path1/.%2e)", "setUser", R"(.)", R"(1 1 0 0 4 7 8 8 20 21 21 21 1 http://.@example.com/)"},
    {R"(http://example.org/foo/bar)", R"(http:c C:httphttps&.\u0009(\u00E9. \u20AC\u005C+0A%)", "setUser", R"(\u0009]%2e_\uD83D\uDE00?\uD83D\uDE00\uD800\u005C)", R"(1 1 0 0 4 7 56 56 68 111 115 115 1 http://%09%5D%2e_%F0%9F%98%80%3F%F0%9F%98%80%EF%BF%BD%5C@example.org/foo/c%20C:httphttps&.(%C3%A9.%20%E2%82%AC/+0A%)"},
    {R"(file://host/share/x)", R"(ws:./|bxn--~&,\u005C]\u001Fhttphttps/,\u001F)", "setUser", R"(#\uD83D\uDE00\u00E9Zfile)", R"(1 0 0 0 2 5 31 31 33 58 59 59 1 ws://%23%F0%9F%98%80%C3%A9Zfile@./|bxn--~&,/]%1Fhttphttps/,)"},
    {R"(\N)", R"(http://example.com/../path1/../../path2/path3/../path4)", "setUser", R"()", R"(1 1 0 0 4 7 7 7 18 25 30 30 1 http://example.com/path2/path4)"},
    {R"(file://host/share/x)", R"(/c!%2Ehttps%\u0009Ab(|!$xn--xn--@[::1]+ xn--))", "setUser", R"( _%2e#\u005C)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://%20_%2e%23%5C@host/c!%2Ehttps%Ab(|!$xn--xn--@[::1]+%20xn--))"},
    {R"(about:blank)", R"(wss://host:444/)", "setUser", R"(/00::1)", R"(1 0 0 4 3 6 18 18 23 28 28 28 1 wss://%2F00%3A%3A1@host:444/)"},
    {R"(file:///C:/a/b)", R"(a:a\u20AC%2E%2e xn--&xn--|+afile)", "setUser", R"(\W\uD800Z\uD83D\uDE0080808080..@\u20AC#_)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 a://%EF%BF%BDZ%F0%9F%98%8080808080..%40%E2%82%AC%23_@a%E2%82%AC%2E%2e xn--&xn--|+afile)"},
    {R"(http://example.org/foo/bar)", R"(C:$;9 http= %2e90x;)\u000A.//0)", "setUser", R"([)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 c://%5B@$;9 http= %2e90x;).//0)"},
    {R"(\N)", R"(C:]\u20AC*(/::)", "setUser", R"(65536@ a 0:#\u00E9)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 c://65536%40%20a%200%3A%23%C3%A9@]%E2%82%AC*(/::)"},
    {R"(http://h/a?b#c)", R"(C::*0x]'\u3000%2E\u00E9-;90x\u3000\u001F\u0000\u005C!c; :?A\u3000)", "setUser", R"(-\u000A:)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 c://-%0A%3A@:*0x]'%E3%80%80%2E%C3%A9-;90x%E3%80%80%1F%00\u005C!c; :?A%E3%80%80)"},
    {R"(http://example.org/foo/bar)", R"(http://4294967295/)", "setUser", R"(0_ \u000A0\u00E9%)", R"(1 1 0 0 4 7 23 23 39 40 40 40 1 http://0_%20%0A0%C3%A9%@255.255.255.255/)"},
    {R"(file://host/share/x)", R"(data:httpa0x)", "truncatedForUseAsBase", R"([-::1])", R"(1 0 1 0 4 5 5 5 5 5 5 5 1 data:)"},
    {R"(http://example.org/foo/bar)", R"(?a=b&c=d)", "truncatedForUseAsBase", R"(\uD80065536\uD83D\uDE00%2e:[file)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.org/foo/)"},
    {R"(http://h/a?b#c)", R"(a:=0b_?*\uD83D\uDE000b,9%2e))", "truncatedForUseAsBase", R"(::1)", R"(1 0 1 0 1 2 2 2 2 2 2 2 1 a:)"},
    {R"(about:blank)", R"(https:_?0+.%2E/=\u0000*[)", "truncatedForUseAsBase", R"(8080aZ%xn--?\u00E9@)", R"(1 1 0 0 5 8 8 8 9 10 10 10 1 https://_/)"},
    {R"(file:///C:/a/b)", R"(../path)", "truncatedForUseAsBase", R"(\u005C\u0009_..\u005C]\u005C::1)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file:///C:/)"},
    {R"(about:blank)", R"(sc:]\u20AC[#xn--\u3000].\u00E9)", "truncatedForUseAsBase", R"(\W\uD83D\uDE00\u00E9Z?]\u00E9\u0009?9+ )", R"(1 0 1 0 2 3 3 3 3 3 3 3 1 sc:)"},
    {R"(http://example.org/foo/bar)", R"(file://host?Q)", "truncatedForUseAsBase", R"(#@)", R"(1 0 0 0 4 7 7 7 11 12 12 12 1 file://host/)"},
    {R"(about:blank)", R"(ftp:,A;\u005C -&!]?\u0000;ab#b\u00E9C:*)..)", "truncatedForUseAsBase", R"(?065536/\u20AC-_--::1\uD800)", R"(1 0 0 0 3 6 6 6 9 10 10 10 1 ftp://,a;/)"},
    {R"(http://example.org/foo/bar)", R"(xn--fa-hia.de)", "truncatedForUseAsBase", R"(@)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.org/foo/)"},
    {R"(\N)", R"(file:/pAtH/)", "truncatedForUseAsBase", R"(?%2e)", R"(1 0 0 0 4 7 7 7 7 13 13 13 1 file:///pAtH/)"},
    {R"(sc://host/path)", R"(]0,*\u00E9Z\u000AA\u0009@127.0.0.1)", "truncatedForUseAsBase", R"(\N)", R"(1 0 0 0 2 5 5 5 9 10 10 10 1 sc://host/)"},
    {R"(file:///C:/a/b)", R"(https://@test@test@example:800/)", "truncatedForUseAsBase", R"(\uD83D\uDE00\u000Afile.65536\u000A)", R"(1 1 0 4 5 8 22 22 30 35 35 35 1 https://%40test%40test@example:800/)"},
    {R"(http://example.org/foo/bar)", R"(o:/#)", "truncatedForUseAsBase", R"(http%2e)", R"(1 0 0 0 1 2 2 2 2 3 3 3 1 o:/)"},
    {R"(http://h/a?b#c)", R"(.[::1]=.\u3000127.0.0.1::0x\u20AC[httpa@file\u20AC')", "truncatedForUseAsBase", R"( %2e?)", R"(1 1 0 0 4 7 7 7 8 9 9 9 1 http://h/)"},
    {R"(sc://host/path)", R"([a:b:c:d:e:f:127.0..0.1])", "truncatedForUseAsBase", R"(\u000A65536@http\uD800filea)", R"(1 0 0 0 2 5 5 5 9 10 10 10 1 sc://host/)"},
    {R"(file://host/share/x)", R"(file:,'*xn--)", "truncatedForUseAsBase", R"(\uD800xn--# 9\uD83D\uDE00)", R"(1 0 0 0 4 7 7 7 11 18 18 18 1 file://host/share/)"},
    {R"(about:blank)", R"(C:[::1]&..[%2E0[$;-b:)", "truncatedForUseAsBase", R"(file\uD83D\uDE00#\u0009]8080\u005C\uD800\uD800)", R"(1 0 1 0 1 2 2 2 2 2 2 2 1 c:)"},
    {R"(file://host/share/x)", R"(.\u0000\u000A/%2e)", "truncatedForUseAsBase", R"(..%2ehttp%2e\uD800@@\u000A%2e::1)", R"(1 0 0 0 4 7 7 7 11 23 23 23 1 file://host/share/.%00/)"},
    {R"(file://host/share/x)", R"(https:@b)", "truncatedForUseAsBase", R"()", R"(1 1 0 0 5 8 8 8 9 10 10 10 1 https://b/)"},
    {R"(file:///C:/a/b)", R"(http://0.1.226.64/)", "truncatedForUseAsBase", R"(xn--file@a9..\u0009[\u000A\uD83D\uDE00[)", R"(1 1 0 0 4 7 7 7 17 18 18 18 1 http://0.1.226.64/)"},
    {R"(http://example.org/foo/bar)", R"(file:%!(xn--C:127.0.0.1./\u00E9A 127.0.0.1\u00E9\u20AC@C:\u000A)", "truncatedForUseAsBase", R"(http\u000A~+aZ\u000A)", R"(1 0 0 0 4 7 7 7 7 28 28 28 1 file:///%!(xn--C:127.0.0.1./)"},
    {R"(sc://host/path)", R"(256)", "truncatedForUseAsBase", R"(\u00E9::1~\u0009 file::1\u20AC\uD800]#8080)", R"(1 0 0 0 2 5 5 5 9 10 10 10 1 sc://host/)"},
    {R"(file://host/share/x)", R"(javascript:#/])", "truncatedForUseAsBase", R"(\u005C\uD83D\uDE00\u005C\u0009\u005C\uD83D\uDE00)", R"(1 0 1 0 10 11 11 11 11 11 11 11 1 javascript:)"},
    {R"(file:///C:/a/b)", R"(#127.0.0.1*file,@filexn--\u005C*.\uD83D\uDE00\u0009'&::0%2E::]~\u0009!)", "truncatedForUseAsBase", R"(-\uD83D\uDE00\u000A\u00E9\uD83D\uDE00[)", R"(1 0 0 0 4 7 7 7 7 13 13 13 1 file:///C:/a/)"},
    {R"(file://host/share/x)", R"(data:@0x127.0.0.1%2E'0.\u005C$~https)", "truncatedForUseAsBase", R"(8080~\u00E980809a:\u000A\u20AC\u005C)", R"(1 0 1 0 4 5 5 5 5 5 5 5 1 data:)"},
};

String Decode(std::string text) {
    if (text == "\\N") return String();
    const bool wide = text.rfind("\\W", 0) == 0;
    if (wide) text = text.substr(2);
    std::u16string units;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 5 < text.size() && text[i + 1] == 'u') {
            units.push_back(static_cast<char16_t>(std::stoul(text.substr(i + 2, 4), nullptr, 16)));
            i += 5;
        } else
            units.push_back(static_cast<unsigned char>(text[i]));
    }
    bool latin1 = !wide;
    for (char16_t unit : units) latin1 &= unit <= 0xFF;
    if (latin1) {
        std::string bytes(units.begin(), units.end());
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<unsigned>(bytes.size()));
    }
    return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
}

StringView ViewOf(const String& string) {
    if (!string.impl) return {nullptr, 0, true};
    return {string.impl->data, string.impl->length, !!(string.impl->hashAndFlags & 4)};
}

std::string Describe(const URL& url) {
    const String string(url.impl ? (url.impl->refCount += 2, url.impl) : nullptr);
    std::string text;
    char buffer[8];
    if (!string.impl) text = "\\N";
    else {
        const bool is8Bit = string.impl->hashAndFlags & 4;
        for (unsigned i = 0; i < string.impl->length; ++i) {
            const char16_t unit = is8Bit ? static_cast<const unsigned char*>(string.impl->data)[i] : static_cast<const char16_t*>(string.impl->data)[i];
            if (unit >= 0x20 && unit < 0x7F && unit != '\\') text.push_back(static_cast<char>(unit));
            else {
                std::snprintf(buffer, sizeof buffer, "\\u%04X", unit);
                text += buffer;
            }
        }
    }
    char fields[160];
    const unsigned bits = url.bits;
    std::snprintf(fields, sizeof fields, "%u %u %u %u %u %u %u %u %u %u %u %u %d ", bits & 1, (bits >> 1) & 1, (bits >> 2) & 1, (bits >> 3) & 7, bits >> 6,
        url.userStart, url.userEnd, url.passwordEnd, url.hostEnd, url.pathAfterLastSlash, url.pathEnd, url.queryEnd,
        string.impl ? static_cast<int>(!!(string.impl->hashAndFlags & 4)) : -1);
    return fields + text;
}

URL Parse(const URL& base, const String& input) {
    URL url;
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, nullptr);
    return url;
}

void Replace(URL& url, const URL& replacement) {
    String old(url.impl);
    url = replacement;
}

std::string Apply(URL& url, const std::string& operation, const String& argument) {
    const StringView view = ViewOf(argument);
    std::string returned;
    if (operation == "setProtocol") returned = _ZN3WTF3URL11setProtocolENS_10StringViewE(&url, view) ? "R1 " : "R0 ";
    else if (operation == "setHost") _ZN3WTF3URL7setHostENS_10StringViewE(&url, view);
    else if (operation == "setHostAndPort") _ZN3WTF3URL14setHostAndPortENS_10StringViewE(&url, view);
    else if (operation == "setUser") _ZN3WTF3URL7setUserENS_10StringViewE(&url, view);
    else if (operation == "removeCredentials") _ZN3WTF3URL17removeCredentialsEv(&url);
    else if (operation == "setFragmentIdentifier") _ZN3WTF3URL21setFragmentIdentifierENS_10StringViewE(&url, view);
    else if (operation == "setQuery") _ZN3WTF3URL8setQueryENS_10StringViewE(&url, view);
    else if (operation == "setPath") _ZN3WTF3URL7setPathENS_10StringViewE(&url, view);
    else if (operation == "truncatedForUseAsBase") Replace(url, _ZNK3WTF3URL21truncatedForUseAsBaseEv(&url));
    else if (operation == "fileURLWithFileSystemPath") Replace(url, _ZN3WTF3URL25fileURLWithFileSystemPathENS_10StringViewE(view));
    else Require(false);
    return returned;
}

void TestCases() {
    for (const Case& test : Cases) {
        const URL empty {};
        URL base {};
        const String baseText = Decode(test.base);
        if (baseText.impl) base = Parse(empty, baseText);
        URL url = Parse(base, Decode(test.input));
        const String argument = Decode(test.argument);
        const std::string returned = Apply(url, test.operation, argument);
        const std::string actual = returned + Describe(url);
        if (actual != test.expected) {
            std::fprintf(stderr, "%s <%s> on <%s>\n  expected %s\n  actual   %s\n", test.operation, test.argument, test.input, test.expected, actual.c_str());
            std::abort();
        }
        String owner(url.impl);
        String baseOwner(base.impl);
    }
}

void TestReferences() {
    const URL empty {};
    const String text = Decode("http://user:pass@host/p?q#f");
    URL url = Parse(empty, text);
    const unsigned before = text.impl->refCount;
    _ZN3WTF3URL17removeCredentialsEv(&url);
    Require(Describe(url) == "1 1 0 0 4 7 7 7 11 12 13 15 1 http://host/p?q#f");
    Require(text.impl->refCount == before - 2);
    URL truncated = _ZNK3WTF3URL21truncatedForUseAsBaseEv(&url);
    Require(Describe(truncated) == "1 1 0 0 4 7 7 7 11 12 12 12 1 http://host/");
    String owner(url.impl);
    String truncatedOwner(truncated.impl);
}

void TestNullArguments() {
    const StringView missing {nullptr, 3, true};
    const StringView empty {nullptr, 0, true};
    URL url = Parse(URL {}, Decode("http://h/"));
    String owner(url.impl);
    RequireInvalidArgument([&] { _ZN3WTF3URL11setProtocolENS_10StringViewE(nullptr, empty); });
    RequireInvalidArgument([&] { _ZN3WTF3URL7setHostENS_10StringViewE(nullptr, empty); });
    RequireInvalidArgument([&] { _ZN3WTF3URL17removeCredentialsEv(nullptr); });
    RequireInvalidArgument([&] { _ZNK3WTF3URL21truncatedForUseAsBaseEv(nullptr); });
    RequireInvalidArgument([&] { _ZN3WTF3URL7setPathENS_10StringViewE(&url, missing); });
    RequireInvalidArgument([&] { _ZN3WTF3URL25fileURLWithFileSystemPathENS_10StringViewE(missing); });
    Require(Describe(url) == "1 1 0 0 4 7 7 7 8 9 9 9 1 http://h/");
}

}

int main() {
    TestCases();
    TestReferences();
    TestNullArguments();
    std::puts("URL setter tests passed");
    return 0;
}
