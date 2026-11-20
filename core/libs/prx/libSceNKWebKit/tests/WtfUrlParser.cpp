#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "URL parser check failed at line %u\n", location.line());
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
    const char* expected;
};

const Case Cases[] = {
    {R"(http://example.org/foo/bar)", R"(http://example\u0009.\u000Aorg)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.org/)"},
    {R"(http://example.org/foo/bar)", R"(http://user:pass@foo:21/bar;par?b#c)", R"(1 1 0 3 4 7 11 16 20 24 31 33 1 http://user:pass@foo:21/bar;par?b#c)"},
    {R"(about:blank)", R"(https://test:@test)", R"(1 1 0 0 5 8 12 12 17 18 18 18 1 https://test@test/)"},
    {R"(about:blank)", R"(https://:@test)", R"(1 1 0 0 5 8 8 8 12 13 13 13 1 https://test/)"},
    {R"(about:blank)", R"(non-special://test:@test/x)", R"(1 0 0 0 11 14 18 18 23 24 25 25 1 non-special://test@test/x)"},
    {R"(about:blank)", R"(non-special://:@test/x)", R"(1 0 0 0 11 14 14 14 18 19 20 20 1 non-special://test/x)"},
    {R"(http://example.org/foo/bar)", R"(http:foo.com)", R"(1 1 0 0 4 7 7 7 18 23 30 30 1 http://example.org/foo/foo.com)"},
    {R"(http://example.org/foo/bar)", R"(\u0009   :foo.com   \u000A)", R"(1 1 0 0 4 7 7 7 18 23 31 31 1 http://example.org/foo/:foo.com)"},
    {R"(http://example.org/foo/bar)", R"( foo.com  )", R"(1 1 0 0 4 7 7 7 18 23 30 30 1 http://example.org/foo/foo.com)"},
    {R"(http://example.org/foo/bar)", R"(a:\u0009 foo.com)", R"(1 0 1 0 1 2 2 2 2 2 10 10 1 a: foo.com)"},
    {R"(http://example.org/foo/bar)", R"(http://f:21/ b ? d # e )", R"(1 1 0 3 4 7 7 7 8 12 19 27 1 http://f:21/%20b%20?%20d%20#%20e)"},
    {R"(about:blank)", R"(lolscheme:x x#x x)", R"(1 0 1 0 9 10 10 10 10 10 13 13 1 lolscheme:x x#x%20x)"},
    {R"(http://example.org/foo/bar)", R"(http://f:/c)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://f/c)"},
    {R"(http://example.org/foo/bar)", R"(http://f:0/c)", R"(1 1 0 2 4 7 7 7 8 11 12 12 1 http://f:0/c)"},
    {R"(http://example.org/foo/bar)", R"(http://f:00000000000000/c)", R"(1 1 0 2 4 7 7 7 8 11 12 12 1 http://f:0/c)"},
    {R"(http://example.org/foo/bar)", R"(http://f:00000000000000000000080/c)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://f/c)"},
    {R"(http://example.org/foo/bar)", R"(http://f:b/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:b/c)"},
    {R"(http://example.org/foo/bar)", R"(http://f: /c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f: /c)"},
    {R"(http://example.org/foo/bar)", R"(http://f:\u000A/c)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://f/c)"},
    {R"(http://example.org/foo/bar)", R"(http://f:fifty-two/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:fifty-two/c)"},
    {R"(http://example.org/foo/bar)", R"(http://f:999999/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:999999/c)"},
    {R"(http://example.org/foo/bar)", R"(non-special://f:999999/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 non-special://f:999999/c)"},
    {R"(http://example.org/foo/bar)", R"(http://f: 21 / b ? d # e )", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f: 21 / b ? d # e )"},
    {R"(http://example.org/foo/bar)", R"()", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar)"},
    {R"(http://example.org/foo/bar)", R"(  \u0009)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar)"},
    {R"(http://example.org/foo/bar)", R"(:foo.com/)", R"(1 1 0 0 4 7 7 7 18 32 32 32 1 http://example.org/foo/:foo.com/)"},
    {R"(http://example.org/foo/bar)", R"(:foo.com\u005C)", R"(1 1 0 0 4 7 7 7 18 32 32 32 1 http://example.org/foo/:foo.com/)"},
    {R"(http://example.org/foo/bar)", R"(:)", R"(1 1 0 0 4 7 7 7 18 23 24 24 1 http://example.org/foo/:)"},
    {R"(http://example.org/foo/bar)", R"(:a)", R"(1 1 0 0 4 7 7 7 18 23 25 25 1 http://example.org/foo/:a)"},
    {R"(http://example.org/foo/bar)", R"(:/)", R"(1 1 0 0 4 7 7 7 18 25 25 25 1 http://example.org/foo/:/)"},
    {R"(http://example.org/foo/bar)", R"(:\u005C)", R"(1 1 0 0 4 7 7 7 18 25 25 25 1 http://example.org/foo/:/)"},
    {R"(http://example.org/foo/bar)", R"(:#)", R"(1 1 0 0 4 7 7 7 18 23 24 24 1 http://example.org/foo/:#)"},
    {R"(http://example.org/foo/bar)", R"(#)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar#)"},
    {R"(http://example.org/foo/bar)", R"(#/)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar#/)"},
    {R"(http://example.org/foo/bar)", R"(#\u005C)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar#\u005C)"},
    {R"(http://example.org/foo/bar)", R"(#;?)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar#;?)"},
    {R"(http://example.org/foo/bar)", R"(?)", R"(1 1 0 0 4 7 7 7 18 23 26 27 1 http://example.org/foo/bar?)"},
    {R"(http://example.org/foo/bar)", R"(/)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.org/)"},
    {R"(http://example.org/foo/bar)", R"(:23)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/:23)"},
    {R"(http://example.org/foo/bar)", R"(/:23)", R"(1 1 0 0 4 7 7 7 18 19 22 22 1 http://example.org/:23)"},
    {R"(http://example.org/foo/bar)", R"(::)", R"(1 1 0 0 4 7 7 7 18 23 25 25 1 http://example.org/foo/::)"},
    {R"(http://example.org/foo/bar)", R"(::23)", R"(1 1 0 0 4 7 7 7 18 23 27 27 1 http://example.org/foo/::23)"},
    {R"(http://example.org/foo/bar)", R"(foo://)", R"(1 0 0 0 3 6 6 6 6 6 6 6 1 foo://)"},
    {R"(http://example.org/foo/bar)", R"(http://a:b@c:29/d)", R"(1 1 0 3 4 7 8 10 12 16 17 17 1 http://a:b@c:29/d)"},
    {R"(http://example.org/foo/bar)", R"(http::@c:29)", R"(1 1 0 0 4 7 7 7 18 23 29 29 1 http://example.org/foo/:@c:29)"},
    {R"(http://example.org/foo/bar)", R"(http://&a:foo(b]c@d:2/)", R"(1 1 0 2 4 7 9 19 21 24 24 24 1 http://&a:foo(b%5Dc@d:2/)"},
    {R"(http://example.org/foo/bar)", R"(http://::@c@d:2)", R"(1 1 0 2 4 7 7 15 17 20 20 20 1 http://:%3A%40c@d:2/)"},
    {R"(http://example.org/foo/bar)", R"(http://foo.com:b@d/)", R"(1 1 0 0 4 7 14 16 18 19 19 19 1 http://foo.com:b@d/)"},
    {R"(http://example.org/foo/bar)", R"(http://foo.com/\u005C@)", R"(1 1 0 0 4 7 7 7 14 16 17 17 1 http://foo.com//@)"},
    {R"(http://example.org/foo/bar)", R"(http:\u005C\u005Cfoo.com\u005C)", R"(1 1 0 0 4 7 7 7 14 15 15 15 1 http://foo.com/)"},
    {R"(http://example.org/foo/bar)", R"(http:\u005C\u005Ca\u005Cb:c\u005Cd@foo.com\u005C)", R"(1 1 0 0 4 7 7 7 8 23 23 23 1 http://a/b:c/d@foo.com/)"},
    {R"(http://example.org/foo/bar)", R"(foo:/)", R"(1 0 0 0 3 4 4 4 4 5 5 5 1 foo:/)"},
    {R"(http://example.org/foo/bar)", R"(foo:/bar.com/)", R"(1 0 0 0 3 4 4 4 4 13 13 13 1 foo:/bar.com/)"},
    {R"(http://example.org/foo/bar)", R"(foo://///////)", R"(1 0 0 0 3 6 6 6 6 13 13 13 1 foo://///////)"},
    {R"(http://example.org/foo/bar)", R"(foo://///////bar.com/)", R"(1 0 0 0 3 6 6 6 6 21 21 21 1 foo://///////bar.com/)"},
    {R"(http://example.org/foo/bar)", R"(foo:////://///)", R"(1 0 0 0 3 6 6 6 6 14 14 14 1 foo:////://///)"},
    {R"(http://example.org/foo/bar)", R"(c:/foo)", R"(1 0 0 0 1 2 2 2 2 3 6 6 1 c:/foo)"},
    {R"(http://example.org/foo/bar)", R"(//foo/bar)", R"(1 1 0 0 4 7 7 7 10 11 14 14 1 http://foo/bar)"},
    {R"(http://example.org/foo/bar)", R"(http://foo/path;a??e#f#g)", R"(1 1 0 0 4 7 7 7 10 11 17 20 1 http://foo/path;a??e#f#g)"},
    {R"(http://example.org/foo/bar)", R"(http://foo/abcd?efgh?ijkl)", R"(1 1 0 0 4 7 7 7 10 11 15 25 1 http://foo/abcd?efgh?ijkl)"},
    {R"(http://example.org/foo/bar)", R"(http://foo/abcd#foo?bar)", R"(1 1 0 0 4 7 7 7 10 11 15 15 1 http://foo/abcd#foo?bar)"},
    {R"(http://example.org/foo/bar)", R"([61:24:74]:98)", R"(1 1 0 0 4 7 7 7 18 23 36 36 1 http://example.org/foo/[61:24:74]:98)"},
    {R"(http://example.org/foo/bar)", R"(http:[61:27]/:foo)", R"(1 1 0 0 4 7 7 7 18 31 35 35 1 http://example.org/foo/[61:27]/:foo)"},
    {R"(http://example.org/foo/bar)", R"(http://[1::2]:3:4)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[1::2]:3:4)"},
    {R"(http://example.org/foo/bar)", R"(http://2001::1)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://2001::1)"},
    {R"(http://example.org/foo/bar)", R"(http://2001::1])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://2001::1])"},
    {R"(http://example.org/foo/bar)", R"(http://2001::1]:80)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://2001::1]:80)"},
    {R"(http://example.org/foo/bar)", R"(http://[2001::1])", R"(1 1 0 0 4 7 7 7 16 17 17 17 1 http://[2001::1]/)"},
    {R"(http://example.org/foo/bar)", R"(http://[::127.0.0.1])", R"(1 1 0 0 4 7 7 7 17 18 18 18 1 http://[::7f00:1]/)"},
    {R"(http://example.org/foo/bar)", R"(http://[0:0:0:0:0:0:13.1.68.3])", R"(1 1 0 0 4 7 7 7 19 20 20 20 1 http://[::d01:4403]/)"},
    {R"(http://example.org/foo/bar)", R"(http://[2001::1]:80)", R"(1 1 0 0 4 7 7 7 16 17 17 17 1 http://[2001::1]/)"},
    {R"(http://example.org/foo/bar)", R"(http:/example.com/)", R"(1 1 0 0 4 7 7 7 18 31 31 31 1 http://example.org/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(ftp:/example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 ftp://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(https:/example.com/)", R"(1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(madeupscheme:/example.com/)", R"(1 0 0 0 12 13 13 13 13 26 26 26 1 madeupscheme:/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(file:/example.com/)", R"(1 0 0 0 4 7 7 7 7 20 20 20 1 file:///example.com/)"},
    {R"(about:blank)", R"(file://example:1/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://example:1/)"},
    {R"(about:blank)", R"(file://example:test/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://example:test/)"},
    {R"(about:blank)", R"(file://example%/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://example%/)"},
    {R"(about:blank)", R"(file://[example]/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://[example]/)"},
    {R"(http://example.org/foo/bar)", R"(ftps:/example.com/)", R"(1 0 0 0 4 5 5 5 5 18 18 18 1 ftps:/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(gopher:/example.com/)", R"(1 0 0 0 6 7 7 7 7 20 20 20 1 gopher:/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(ws:/example.com/)", R"(1 0 0 0 2 5 5 5 16 17 17 17 1 ws://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(wss:/example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 wss://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(data:/example.com/)", R"(1 0 0 0 4 5 5 5 5 18 18 18 1 data:/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(javascript:/example.com/)", R"(1 0 0 0 10 11 11 11 11 24 24 24 1 javascript:/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(mailto:/example.com/)", R"(1 0 0 0 6 7 7 7 7 20 20 20 1 mailto:/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(http:example.com/)", R"(1 1 0 0 4 7 7 7 18 35 35 35 1 http://example.org/foo/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(ftp:example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 ftp://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(https:example.com/)", R"(1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(madeupscheme:example.com/)", R"(1 0 1 0 12 13 13 13 13 25 25 25 1 madeupscheme:example.com/)"},
    {R"(http://example.org/foo/bar)", R"(ftps:example.com/)", R"(1 0 1 0 4 5 5 5 5 17 17 17 1 ftps:example.com/)"},
    {R"(http://example.org/foo/bar)", R"(gopher:example.com/)", R"(1 0 1 0 6 7 7 7 7 19 19 19 1 gopher:example.com/)"},
    {R"(http://example.org/foo/bar)", R"(ws:example.com/)", R"(1 0 0 0 2 5 5 5 16 17 17 17 1 ws://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(wss:example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 wss://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(data:example.com/)", R"(1 0 1 0 4 5 5 5 5 17 17 17 1 data:example.com/)"},
    {R"(http://example.org/foo/bar)", R"(javascript:example.com/)", R"(1 0 1 0 10 11 11 11 11 23 23 23 1 javascript:example.com/)"},
    {R"(http://example.org/foo/bar)", R"(mailto:example.com/)", R"(1 0 1 0 6 7 7 7 7 19 19 19 1 mailto:example.com/)"},
    {R"(http://example.org/foo/bar)", R"(/a/b/c)", R"(1 1 0 0 4 7 7 7 18 23 24 24 1 http://example.org/a/b/c)"},
    {R"(http://example.org/foo/bar)", R"(/a/ /c)", R"(1 1 0 0 4 7 7 7 18 25 26 26 1 http://example.org/a/%20/c)"},
    {R"(http://example.org/foo/bar)", R"(/a%2fc)", R"(1 1 0 0 4 7 7 7 18 19 24 24 1 http://example.org/a%2fc)"},
    {R"(http://example.org/foo/bar)", R"(/a/%2f/c)", R"(1 1 0 0 4 7 7 7 18 25 26 26 1 http://example.org/a/%2f/c)"},
    {R"(http://example.org/foo/bar)", R"(#\u03B2)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar#%CE%B2)"},
    {R"(http://example.org/foo/bar)", R"(data:text/html,test#test)", R"(1 0 1 0 4 5 5 5 5 10 19 19 1 data:text/html,test#test)"},
    {R"(http://example.org/foo/bar)", R"(tel:1234567890)", R"(1 0 1 0 3 4 4 4 4 4 14 14 1 tel:1234567890)"},
    {R"(http://example.org/)", R"(ssh://example.com/foo/bar.git)", R"(1 0 0 0 3 6 6 6 17 22 29 29 1 ssh://example.com/foo/bar.git)"},
    {R"(file:///tmp/mock/path)", R"(file:c:\u005Cfoo\u005Cbar.html)", R"(1 0 0 0 4 7 7 7 7 15 23 23 1 file:///c:/foo/bar.html)"},
    {R"(file:///tmp/mock/path)", R"(  File:c|////foo\u005Cbar.html)", R"(1 0 0 0 4 7 7 7 7 18 26 26 1 file:///c:////foo/bar.html)"},
    {R"(file:///tmp/mock/path)", R"(C|/foo/bar)", R"(1 0 0 0 4 7 7 7 7 15 18 18 1 file:///C:/foo/bar)"},
    {R"(file:///tmp/mock/path)", R"(/C|\u005Cfoo\u005Cbar)", R"(1 0 0 0 4 7 7 7 7 15 18 18 1 file:///C:/foo/bar)"},
    {R"(file:///tmp/mock/path)", R"(//C|/foo/bar)", R"(1 0 0 0 4 7 7 7 7 15 18 18 1 file:///C:/foo/bar)"},
    {R"(file:///tmp/mock/path)", R"(//server/file)", R"(1 0 0 0 4 7 7 7 13 14 18 18 1 file://server/file)"},
    {R"(file:///tmp/mock/path)", R"(\u005C\u005Cserver\u005Cfile)", R"(1 0 0 0 4 7 7 7 13 14 18 18 1 file://server/file)"},
    {R"(file:///tmp/mock/path)", R"(/\u005Cserver/file)", R"(1 0 0 0 4 7 7 7 13 14 18 18 1 file://server/file)"},
    {R"(file:///tmp/mock/path)", R"(file:///foo/bar.txt)", R"(1 0 0 0 4 7 7 7 7 12 19 19 1 file:///foo/bar.txt)"},
    {R"(file:///tmp/mock/path)", R"(file:///home/me)", R"(1 0 0 0 4 7 7 7 7 13 15 15 1 file:///home/me)"},
    {R"(file:///tmp/mock/path)", R"(//)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file:///tmp/mock/path)", R"(///)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file:///tmp/mock/path)", R"(///test)", R"(1 0 0 0 4 7 7 7 7 8 12 12 1 file:///test)"},
    {R"(file:///tmp/mock/path)", R"(file://test)", R"(1 0 0 0 4 7 7 7 11 12 12 12 1 file://test/)"},
    {R"(file:///tmp/mock/path)", R"(file://localhost)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file:///tmp/mock/path)", R"(file://localhost/)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file:///tmp/mock/path)", R"(file://localhost/test)", R"(1 0 0 0 4 7 7 7 7 8 12 12 1 file:///test)"},
    {R"(file:///tmp/mock/path)", R"(test)", R"(1 0 0 0 4 7 7 7 7 17 21 21 1 file:///tmp/mock/test)"},
    {R"(file:///tmp/mock/path)", R"(file:test)", R"(1 0 0 0 4 7 7 7 7 17 21 21 1 file:///tmp/mock/test)"},
    {R"(about:blank)", R"(http://example.com/././foo)", R"(1 1 0 0 4 7 7 7 18 19 22 22 1 http://example.com/foo)"},
    {R"(about:blank)", R"(http://example.com/./.foo)", R"(1 1 0 0 4 7 7 7 18 19 23 23 1 http://example.com/.foo)"},
    {R"(about:blank)", R"(http://example.com/foo/.)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.com/foo/)"},
    {R"(about:blank)", R"(http://example.com/foo/./)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.com/foo/)"},
    {R"(about:blank)", R"(http://example.com/foo/bar/..)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.com/foo/)"},
    {R"(about:blank)", R"(http://example.com/foo/bar/../)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.com/foo/)"},
    {R"(about:blank)", R"(http://example.com/foo/..bar)", R"(1 1 0 0 4 7 7 7 18 23 28 28 1 http://example.com/foo/..bar)"},
    {R"(about:blank)", R"(http://example.com/foo/bar/../ton)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.com/foo/ton)"},
    {R"(about:blank)", R"(http://example.com/foo/bar/../ton/../../a)", R"(1 1 0 0 4 7 7 7 18 19 20 20 1 http://example.com/a)"},
    {R"(about:blank)", R"(http://example.com/foo/../../..)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.com/)"},
    {R"(about:blank)", R"(http://example.com/foo/../../../ton)", R"(1 1 0 0 4 7 7 7 18 19 22 22 1 http://example.com/ton)"},
    {R"(about:blank)", R"(http://example.com/foo/%2e)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.com/foo/)"},
    {R"(about:blank)", R"(http://example.com/foo/%2e%2)", R"(1 1 0 0 4 7 7 7 18 23 28 28 1 http://example.com/foo/%2e%2)"},
    {R"(about:blank)", R"(http://example.com/foo/%2e./%2e%2e/.%2e/%2e.bar)", R"(1 1 0 0 4 7 7 7 18 19 26 26 1 http://example.com/%2e.bar)"},
    {R"(about:blank)", R"(http://example.com////../..)", R"(1 1 0 0 4 7 7 7 18 20 20 20 1 http://example.com//)"},
    {R"(about:blank)", R"(http://example.com/foo/bar//../..)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.com/foo/)"},
    {R"(about:blank)", R"(http://example.com/foo/bar//..)", R"(1 1 0 0 4 7 7 7 18 27 27 27 1 http://example.com/foo/bar/)"},
    {R"(about:blank)", R"(http://example.com/foo)", R"(1 1 0 0 4 7 7 7 18 19 22 22 1 http://example.com/foo)"},
    {R"(about:blank)", R"(http://example.com/%20foo)", R"(1 1 0 0 4 7 7 7 18 19 25 25 1 http://example.com/%20foo)"},
    {R"(about:blank)", R"(http://example.com/foo%)", R"(1 1 0 0 4 7 7 7 18 19 23 23 1 http://example.com/foo%)"},
    {R"(about:blank)", R"(http://example.com/foo%2)", R"(1 1 0 0 4 7 7 7 18 19 24 24 1 http://example.com/foo%2)"},
    {R"(about:blank)", R"(http://example.com/foo%2zbar)", R"(1 1 0 0 4 7 7 7 18 19 28 28 1 http://example.com/foo%2zbar)"},
    {R"(about:blank)", R"(http://example.com/foo%2\u00C2\u00A9zbar)", R"(1 1 0 0 4 7 7 7 18 19 40 40 1 http://example.com/foo%2%C3%82%C2%A9zbar)"},
    {R"(about:blank)", R"(http://example.com/foo%41%7a)", R"(1 1 0 0 4 7 7 7 18 19 28 28 1 http://example.com/foo%41%7a)"},
    {R"(about:blank)", R"(http://example.com/foo\u0009\u0091%91)", R"(1 1 0 0 4 7 7 7 18 19 31 31 1 http://example.com/foo%C2%91%91)"},
    {R"(about:blank)", R"(http://example.com/foo%00%51)", R"(1 1 0 0 4 7 7 7 18 19 28 28 1 http://example.com/foo%00%51)"},
    {R"(about:blank)", R"(http://example.com/(%28:%3A%29))", R"(1 1 0 0 4 7 7 7 18 19 31 31 1 http://example.com/(%28:%3A%29))"},
    {R"(about:blank)", R"(http://example.com/%3A%3a%3C%3c)", R"(1 1 0 0 4 7 7 7 18 19 31 31 1 http://example.com/%3A%3a%3C%3c)"},
    {R"(about:blank)", R"(http://example.com/foo\u0009bar)", R"(1 1 0 0 4 7 7 7 18 19 25 25 1 http://example.com/foobar)"},
    {R"(about:blank)", R"(http://example.com\u005C\u005Cfoo\u005C\u005Cbar)", R"(1 1 0 0 4 7 7 7 18 25 28 28 1 http://example.com//foo//bar)"},
    {R"(about:blank)", R"(http://example.com/%7Ffp3%3Eju%3Dduvgw%3Dd)", R"(1 1 0 0 4 7 7 7 18 19 42 42 1 http://example.com/%7Ffp3%3Eju%3Dduvgw%3Dd)"},
    {R"(about:blank)", R"(http://example.com/@asdf%40)", R"(1 1 0 0 4 7 7 7 18 19 27 27 1 http://example.com/@asdf%40)"},
    {R"(about:blank)", R"(http://example.com/\u4F60\u597D\u4F60\u597D)", R"(1 1 0 0 4 7 7 7 18 19 55 55 1 http://example.com/%E4%BD%A0%E5%A5%BD%E4%BD%A0%E5%A5%BD)"},
    {R"(about:blank)", R"(http://example.com/\u2025/foo)", R"(1 1 0 0 4 7 7 7 18 29 32 32 1 http://example.com/%E2%80%A5/foo)"},
    {R"(about:blank)", R"(http://example.com/\uFEFF/foo)", R"(1 1 0 0 4 7 7 7 18 29 32 32 1 http://example.com/%EF%BB%BF/foo)"},
    {R"(about:blank)", R"(http://example.com/\u202E/foo/\u202D/bar)", R"(1 1 0 0 4 7 7 7 18 43 46 46 1 http://example.com/%E2%80%AE/foo/%E2%80%AD/bar)"},
    {R"(about:blank)", R"(http://www.google.com/foo?bar=baz#)", R"(1 1 0 0 4 7 7 7 21 22 25 33 1 http://www.google.com/foo?bar=baz#)"},
    {R"(about:blank)", R"(http://www.google.com/foo?bar=baz# \u00BB)", R"(1 1 0 0 4 7 7 7 21 22 25 33 1 http://www.google.com/foo?bar=baz#%20%C2%BB)"},
    {R"(about:blank)", R"(data:test# \u00BB)", R"(1 0 1 0 4 5 5 5 5 5 9 9 1 data:test#%20%C2%BB)"},
    {R"(about:blank)", R"(http://www.google.com)", R"(1 1 0 0 4 7 7 7 21 22 22 22 1 http://www.google.com/)"},
    {R"(about:blank)", R"(http://192.0x00A80001)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://192.168.0.1/)"},
    {R"(about:blank)", R"(http://www/foo%2Ehtml)", R"(1 1 0 0 4 7 7 7 10 11 21 21 1 http://www/foo%2Ehtml)"},
    {R"(about:blank)", R"(http://www/foo/%2E/html)", R"(1 1 0 0 4 7 7 7 10 15 19 19 1 http://www/foo/html)"},
    {R"(about:blank)", R"(http://user:pass@/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://user:pass@/)"},
    {R"(about:blank)", R"(http://%25DOMAIN:foobar@foodomain.com/)", R"(1 1 0 0 4 7 16 23 37 38 38 38 1 http://%25DOMAIN:foobar@foodomain.com/)"},
    {R"(about:blank)", R"(http:\u005C\u005Cwww.google.com\u005Cfoo)", R"(1 1 0 0 4 7 7 7 21 22 25 25 1 http://www.google.com/foo)"},
    {R"(about:blank)", R"(http://foo:80/)", R"(1 1 0 0 4 7 7 7 10 11 11 11 1 http://foo/)"},
    {R"(about:blank)", R"(http://foo:81/)", R"(1 1 0 3 4 7 7 7 10 14 14 14 1 http://foo:81/)"},
    {R"(about:blank)", R"(httpa://foo:80/)", R"(1 0 0 3 5 8 8 8 11 15 15 15 1 httpa://foo:80/)"},
    {R"(about:blank)", R"(http://foo:-80/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://foo:-80/)"},
    {R"(about:blank)", R"(https://foo:443/)", R"(1 1 0 0 5 8 8 8 11 12 12 12 1 https://foo/)"},
    {R"(about:blank)", R"(https://foo:80/)", R"(1 1 0 3 5 8 8 8 11 15 15 15 1 https://foo:80/)"},
    {R"(about:blank)", R"(ftp://foo:21/)", R"(1 0 0 0 3 6 6 6 9 10 10 10 1 ftp://foo/)"},
    {R"(about:blank)", R"(ftp://foo:80/)", R"(1 0 0 3 3 6 6 6 9 13 13 13 1 ftp://foo:80/)"},
    {R"(about:blank)", R"(gopher://foo:70/)", R"(1 0 0 3 6 9 9 9 12 16 16 16 1 gopher://foo:70/)"},
    {R"(about:blank)", R"(gopher://foo:443/)", R"(1 0 0 4 6 9 9 9 12 17 17 17 1 gopher://foo:443/)"},
    {R"(about:blank)", R"(ws://foo:80/)", R"(1 0 0 0 2 5 5 5 8 9 9 9 1 ws://foo/)"},
    {R"(about:blank)", R"(ws://foo:81/)", R"(1 0 0 3 2 5 5 5 8 12 12 12 1 ws://foo:81/)"},
    {R"(about:blank)", R"(ws://foo:443/)", R"(1 0 0 4 2 5 5 5 8 13 13 13 1 ws://foo:443/)"},
    {R"(about:blank)", R"(ws://foo:815/)", R"(1 0 0 4 2 5 5 5 8 13 13 13 1 ws://foo:815/)"},
    {R"(about:blank)", R"(wss://foo:80/)", R"(1 0 0 3 3 6 6 6 9 13 13 13 1 wss://foo:80/)"},
    {R"(about:blank)", R"(wss://foo:81/)", R"(1 0 0 3 3 6 6 6 9 13 13 13 1 wss://foo:81/)"},
    {R"(about:blank)", R"(wss://foo:443/)", R"(1 0 0 0 3 6 6 6 9 10 10 10 1 wss://foo/)"},
    {R"(about:blank)", R"(wss://foo:815/)", R"(1 0 0 4 3 6 6 6 9 14 14 14 1 wss://foo:815/)"},
    {R"(about:blank)", R"(http:/example.com/)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.com/)"},
    {R"(about:blank)", R"(ftp:/example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 ftp://example.com/)"},
    {R"(about:blank)", R"(https:/example.com/)", R"(1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.com/)"},
    {R"(about:blank)", R"(madeupscheme:/example.com/)", R"(1 0 0 0 12 13 13 13 13 26 26 26 1 madeupscheme:/example.com/)"},
    {R"(about:blank)", R"(file:/example.com/)", R"(1 0 0 0 4 7 7 7 7 20 20 20 1 file:///example.com/)"},
    {R"(about:blank)", R"(ftps:/example.com/)", R"(1 0 0 0 4 5 5 5 5 18 18 18 1 ftps:/example.com/)"},
    {R"(about:blank)", R"(gopher:/example.com/)", R"(1 0 0 0 6 7 7 7 7 20 20 20 1 gopher:/example.com/)"},
    {R"(about:blank)", R"(ws:/example.com/)", R"(1 0 0 0 2 5 5 5 16 17 17 17 1 ws://example.com/)"},
    {R"(about:blank)", R"(wss:/example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 wss://example.com/)"},
    {R"(about:blank)", R"(data:/example.com/)", R"(1 0 0 0 4 5 5 5 5 18 18 18 1 data:/example.com/)"},
    {R"(about:blank)", R"(javascript:/example.com/)", R"(1 0 0 0 10 11 11 11 11 24 24 24 1 javascript:/example.com/)"},
    {R"(about:blank)", R"(mailto:/example.com/)", R"(1 0 0 0 6 7 7 7 7 20 20 20 1 mailto:/example.com/)"},
    {R"(about:blank)", R"(http:example.com/)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.com/)"},
    {R"(about:blank)", R"(ftp:example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 ftp://example.com/)"},
    {R"(about:blank)", R"(https:example.com/)", R"(1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.com/)"},
    {R"(about:blank)", R"(madeupscheme:example.com/)", R"(1 0 1 0 12 13 13 13 13 25 25 25 1 madeupscheme:example.com/)"},
    {R"(about:blank)", R"(ftps:example.com/)", R"(1 0 1 0 4 5 5 5 5 17 17 17 1 ftps:example.com/)"},
    {R"(about:blank)", R"(gopher:example.com/)", R"(1 0 1 0 6 7 7 7 7 19 19 19 1 gopher:example.com/)"},
    {R"(about:blank)", R"(ws:example.com/)", R"(1 0 0 0 2 5 5 5 16 17 17 17 1 ws://example.com/)"},
    {R"(about:blank)", R"(wss:example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 wss://example.com/)"},
    {R"(about:blank)", R"(data:example.com/)", R"(1 0 1 0 4 5 5 5 5 17 17 17 1 data:example.com/)"},
    {R"(about:blank)", R"(javascript:example.com/)", R"(1 0 1 0 10 11 11 11 11 23 23 23 1 javascript:example.com/)"},
    {R"(about:blank)", R"(mailto:example.com/)", R"(1 0 1 0 6 7 7 7 7 19 19 19 1 mailto:example.com/)"},
    {R"(about:blank)", R"(http:@www.example.com)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://www.example.com/)"},
    {R"(about:blank)", R"(http:/@www.example.com)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://www.example.com/)"},
    {R"(about:blank)", R"(http://@www.example.com)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://www.example.com/)"},
    {R"(about:blank)", R"(http:a:b@www.example.com)", R"(1 1 0 0 4 7 8 10 26 27 27 27 1 http://a:b@www.example.com/)"},
    {R"(about:blank)", R"(http:/a:b@www.example.com)", R"(1 1 0 0 4 7 8 10 26 27 27 27 1 http://a:b@www.example.com/)"},
    {R"(about:blank)", R"(http://a:b@www.example.com)", R"(1 1 0 0 4 7 8 10 26 27 27 27 1 http://a:b@www.example.com/)"},
    {R"(about:blank)", R"(http://@pple.com)", R"(1 1 0 0 4 7 7 7 15 16 16 16 1 http://pple.com/)"},
    {R"(about:blank)", R"(http::b@www.example.com)", R"(1 1 0 0 4 7 7 9 25 26 26 26 1 http://:b@www.example.com/)"},
    {R"(about:blank)", R"(http:/:b@www.example.com)", R"(1 1 0 0 4 7 7 9 25 26 26 26 1 http://:b@www.example.com/)"},
    {R"(about:blank)", R"(http://:b@www.example.com)", R"(1 1 0 0 4 7 7 9 25 26 26 26 1 http://:b@www.example.com/)"},
    {R"(about:blank)", R"(http:/:@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:/:@/www.example.com)"},
    {R"(about:blank)", R"(http://user@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://user@/www.example.com)"},
    {R"(about:blank)", R"(http:@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:@/www.example.com)"},
    {R"(about:blank)", R"(http:/@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:/@/www.example.com)"},
    {R"(about:blank)", R"(http://@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://@/www.example.com)"},
    {R"(about:blank)", R"(https:@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https:@/www.example.com)"},
    {R"(about:blank)", R"(http:a:b@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:a:b@/www.example.com)"},
    {R"(about:blank)", R"(http:/a:b@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:/a:b@/www.example.com)"},
    {R"(about:blank)", R"(http://a:b@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://a:b@/www.example.com)"},
    {R"(about:blank)", R"(http::@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http::@/www.example.com)"},
    {R"(about:blank)", R"(http:a:@www.example.com)", R"(1 1 0 0 4 7 8 8 24 25 25 25 1 http://a@www.example.com/)"},
    {R"(about:blank)", R"(http:/a:@www.example.com)", R"(1 1 0 0 4 7 8 8 24 25 25 25 1 http://a@www.example.com/)"},
    {R"(about:blank)", R"(http://a:@www.example.com)", R"(1 1 0 0 4 7 8 8 24 25 25 25 1 http://a@www.example.com/)"},
    {R"(about:blank)", R"(http://www.@pple.com)", R"(1 1 0 0 4 7 11 11 20 21 21 21 1 http://www.@pple.com/)"},
    {R"(about:blank)", R"(http:@:www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:@:www.example.com)"},
    {R"(about:blank)", R"(http:/@:www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:/@:www.example.com)"},
    {R"(about:blank)", R"(http://@:www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://@:www.example.com)"},
    {R"(about:blank)", R"(http://:@www.example.com)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://www.example.com/)"},
    {R"(http://www.example.com/test)", R"(/)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://www.example.com/)"},
    {R"(http://www.example.com/test)", R"(/test.txt)", R"(1 1 0 0 4 7 7 7 22 23 31 31 1 http://www.example.com/test.txt)"},
    {R"(http://www.example.com/test)", R"(.)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://www.example.com/)"},
    {R"(http://www.example.com/test)", R"(..)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://www.example.com/)"},
    {R"(http://www.example.com/test)", R"(test.txt)", R"(1 1 0 0 4 7 7 7 22 23 31 31 1 http://www.example.com/test.txt)"},
    {R"(http://www.example.com/test)", R"(./test.txt)", R"(1 1 0 0 4 7 7 7 22 23 31 31 1 http://www.example.com/test.txt)"},
    {R"(http://www.example.com/test)", R"(../test.txt)", R"(1 1 0 0 4 7 7 7 22 23 31 31 1 http://www.example.com/test.txt)"},
    {R"(http://www.example.com/test)", R"(../aaa/test.txt)", R"(1 1 0 0 4 7 7 7 22 27 35 35 1 http://www.example.com/aaa/test.txt)"},
    {R"(http://www.example.com/test)", R"(../../test.txt)", R"(1 1 0 0 4 7 7 7 22 23 31 31 1 http://www.example.com/test.txt)"},
    {R"(http://www.example.com/test)", R"(\u4E2D/test.txt)", R"(1 1 0 0 4 7 7 7 22 33 41 41 1 http://www.example.com/%E4%B8%AD/test.txt)"},
    {R"(http://www.example.com/test)", R"(http://www.example2.com)", R"(1 1 0 0 4 7 7 7 23 24 24 24 1 http://www.example2.com/)"},
    {R"(http://www.example.com/test)", R"(//www.example2.com)", R"(1 1 0 0 4 7 7 7 23 24 24 24 1 http://www.example2.com/)"},
    {R"(http://www.example.com/test)", R"(file:...)", R"(1 0 0 0 4 7 7 7 7 8 11 11 1 file:///...)"},
    {R"(http://www.example.com/test)", R"(file:..)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(http://www.example.com/test)", R"(file:a)", R"(1 0 0 0 4 7 7 7 7 8 9 9 1 file:///a)"},
    {R"(http://other.com/)", R"(http://ExAmPlE.CoM)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.com/)"},
    {R"(http://other.com/)", R"(http://example example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://example example.com)"},
    {R"(http://other.com/)", R"(http://Goo%20 goo%7C|.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://Goo%20 goo%7C|.com)"},
    {R"(http://other.com/)", R"(http://[])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[])"},
    {R"(http://other.com/)", R"(http://[:])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[:])"},
    {R"(about:blank)", R"(\u0000\u001B\u0004\u0012 http://example.com/\u001F \u000D )", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.com/)"},
    {R"(about:blank)", R"(https://x/\uFFFD?\uFFFD#\uFFFD)", R"(1 1 0 0 5 8 8 8 9 10 19 29 1 https://x/%EF%BF%BD?%EF%BF%BD#%EF%BF%BD)"},
    {R"(about:blank)", R"(sc://fa\u00DF.ExAmPlE/)", R"(1 0 0 0 2 5 5 5 21 22 22 22 1 sc://fa%C3%9F.ExAmPlE/)"},
    {R"(http://other.com/)", R"(http://%zz%66%a.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://%zz%66%a.com)"},
    {R"(http://other.com/)", R"(http://%25)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://%25)"},
    {R"(http://other.com/)", R"(http://hello%00)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://hello%00)"},
    {R"(http://other.com/)", R"(http://%30%78%63%30%2e%30%32%35%30.01)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://192.168.0.1/)"},
    {R"(http://other.com/)", R"(http://%30%78%63%30%2e%30%32%35%30.01%2e)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://192.168.0.1/)"},
    {R"(http://other.com/)", R"(http://192.168.0.257)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://192.168.0.257)"},
    {R"(http://other.com/)", R"(http://%3g%78%63%30%2e%30%32%35%30%2E.01)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://%3g%78%63%30%2e%30%32%35%30%2E.01)"},
    {R"(http://other.com/)", R"(http://192.168.0.1 hello)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://192.168.0.1 hello)"},
    {R"(about:blank)", R"(https://x x:12)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://x x:12)"},
    {R"(about:blank)", R"(http://./)", R"(1 1 0 0 4 7 7 7 8 9 9 9 1 http://./)"},
    {R"(about:blank)", R"(http://../)", R"(1 1 0 0 4 7 7 7 9 10 10 10 1 http://../)"},
    {R"(about:blank)", R"(http://0..0x300/)", R"(1 1 0 0 4 7 7 7 15 16 16 16 1 http://0..0x300/)"},
    {R"(about:blank)", R"(http://[www.google.com]/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[www.google.com]/)"},
    {R"(http://other.com/)", R"(http://[google.com])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[google.com])"},
    {R"(http://other.com/)", R"(http://[::1.2.3.4x])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[::1.2.3.4x])"},
    {R"(http://other.com/)", R"(http://[::1.2.3.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[::1.2.3.])"},
    {R"(http://other.com/)", R"(http://[::1.2.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[::1.2.])"},
    {R"(http://other.com/)", R"(http://[::1.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[::1.])"},
    {R"(http://other.com/)", R"(http://foo:\uD83D\uDCA9@example.com/bar)", R"(1 1 0 0 4 7 10 23 35 36 39 39 1 http://foo:%F0%9F%92%A9@example.com/bar)"},
    {R"(test:test)", R"(#)", R"(1 0 0 0 4 5 5 5 5 5 9 9 1 test:test#)"},
    {R"(mailto:x@x.com)", R"(#x)", R"(1 0 0 0 6 7 7 7 7 7 14 14 1 mailto:x@x.com#x)"},
    {R"(data:,)", R"(#x)", R"(1 0 0 0 4 5 5 5 5 5 6 6 1 data:,#x)"},
    {R"(about:blank)", R"(#x)", R"(1 0 0 0 5 6 6 6 6 6 11 11 1 about:blank#x)"},
    {R"(test:test?test)", R"(#)", R"(1 0 0 0 4 5 5 5 5 5 9 14 1 test:test?test#)"},
    {R"(http://doesnotmatter/)", R"(https://@test@test@example:800/)", R"(1 1 0 4 5 8 22 22 30 35 35 35 1 https://%40test%40test@example:800/)"},
    {R"(http://doesnotmatter/)", R"(https://@@@example)", R"(1 1 0 0 5 8 14 14 22 23 23 23 1 https://%40%40@example/)"},
    {R"(http://doesnotmatter/)", R"(http://`{}:`{}@h/`{}?`{})", R"(1 1 0 0 4 7 16 26 28 29 38 42 1 http://%60%7B%7D:%60%7B%7D@h/%60%7B%7D?`{})"},
    {R"(about:blank)", R"(http://host/?')", R"(1 1 0 0 4 7 7 7 11 12 12 16 1 http://host/?%27)"},
    {R"(about:blank)", R"(notspecial://host/?')", R"(1 0 0 0 10 13 13 13 17 18 18 20 1 notspecial://host/?')"},
    {R"(http://user@example.org/smth)", R"(/some/path)", R"(1 1 0 0 4 7 11 11 23 29 33 33 1 http://user@example.org/some/path)"},
    {R"(http://user:pass@example.org:21/smth)", R"()", R"(1 1 0 3 4 7 11 16 28 32 36 36 1 http://user:pass@example.org:21/smth)"},
    {R"(http://user:pass@example.org:21/smth)", R"(/some/path)", R"(1 1 0 3 4 7 11 16 28 37 41 41 1 http://user:pass@example.org:21/some/path)"},
    {R"(sc:sd)", R"(i)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 i)"},
    {R"(sc:sd/sd)", R"(i)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 i)"},
    {R"(sc:/pa/pa)", R"(i)", R"(1 0 0 0 2 3 3 3 3 7 8 8 1 sc:/pa/i)"},
    {R"(sc://ho/pa)", R"(i)", R"(1 0 0 0 2 5 5 5 7 8 9 9 1 sc://ho/i)"},
    {R"(sc:///pa/pa)", R"(i)", R"(1 0 0 0 2 5 5 5 5 9 10 10 1 sc:///pa/i)"},
    {R"(sc:sd)", R"(../i)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 ../i)"},
    {R"(sc:sd/sd)", R"(../i)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 ../i)"},
    {R"(sc:/pa/pa)", R"(../i)", R"(1 0 0 0 2 3 3 3 3 4 5 5 1 sc:/i)"},
    {R"(sc://ho/pa)", R"(../i)", R"(1 0 0 0 2 5 5 5 7 8 9 9 1 sc://ho/i)"},
    {R"(sc:///pa/pa)", R"(../i)", R"(1 0 0 0 2 5 5 5 5 6 7 7 1 sc:///i)"},
    {R"(sc:sd)", R"(/i)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 /i)"},
    {R"(sc:sd/sd)", R"(/i)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 /i)"},
    {R"(sc:/pa/pa)", R"(/i)", R"(1 0 0 0 2 3 3 3 3 4 5 5 1 sc:/i)"},
    {R"(sc://ho/pa)", R"(/i)", R"(1 0 0 0 2 5 5 5 7 8 9 9 1 sc://ho/i)"},
    {R"(sc:///pa/pa)", R"(/i)", R"(1 0 0 0 2 5 5 5 5 6 7 7 1 sc:///i)"},
    {R"(sc:sd)", R"(?i)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 ?i)"},
    {R"(sc:sd/sd)", R"(?i)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 ?i)"},
    {R"(sc:/pa/pa)", R"(?i)", R"(1 0 0 0 2 3 3 3 3 7 9 11 1 sc:/pa/pa?i)"},
    {R"(sc://ho/pa)", R"(?i)", R"(1 0 0 0 2 5 5 5 7 8 10 12 1 sc://ho/pa?i)"},
    {R"(sc:///pa/pa)", R"(?i)", R"(1 0 0 0 2 5 5 5 5 9 11 13 1 sc:///pa/pa?i)"},
    {R"(sc:sd)", R"(#i)", R"(1 0 0 0 2 3 3 3 3 3 5 5 1 sc:sd#i)"},
    {R"(sc:sd/sd)", R"(#i)", R"(1 0 0 0 2 3 3 3 3 6 8 8 1 sc:sd/sd#i)"},
    {R"(sc:/pa/pa)", R"(#i)", R"(1 0 0 0 2 3 3 3 3 7 9 9 1 sc:/pa/pa#i)"},
    {R"(sc://ho/pa)", R"(#i)", R"(1 0 0 0 2 5 5 5 7 8 10 10 1 sc://ho/pa#i)"},
    {R"(sc:///pa/pa)", R"(#i)", R"(1 0 0 0 2 5 5 5 5 9 11 11 1 sc:///pa/pa#i)"},
    {R"(about:blank)", R"(about:/../)", R"(1 0 0 0 5 6 6 6 6 7 7 7 1 about:/)"},
    {R"(about:blank)", R"(data:/../)", R"(1 0 0 0 4 5 5 5 5 6 6 6 1 data:/)"},
    {R"(about:blank)", R"(javascript:/../)", R"(1 0 0 0 10 11 11 11 11 12 12 12 1 javascript:/)"},
    {R"(about:blank)", R"(mailto:/../)", R"(1 0 0 0 6 7 7 7 7 8 8 8 1 mailto:/)"},
    {R"(about:blank)", R"(sc://\u00F1.test/)", R"(1 0 0 0 2 5 5 5 16 17 17 17 1 sc://%C3%B1.test/)"},
    {R"(about:blank)", R"(sc://\u0000/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://\u0000/)"},
    {R"(about:blank)", R"(sc:// /)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc:// /)"},
    {R"(about:blank)", R"(sc://%/)", R"(1 0 0 0 2 5 5 5 6 7 7 7 1 sc://%/)"},
    {R"(about:blank)", R"(sc://@/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://@/)"},
    {R"(about:blank)", R"(sc://te@s:t@/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://te@s:t@/)"},
    {R"(about:blank)", R"(sc://:/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://:/)"},
    {R"(about:blank)", R"(sc://:12/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://:12/)"},
    {R"(about:blank)", R"(sc://[/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://[/)"},
    {R"(about:blank)", R"(sc://\u005C/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://\u005C/)"},
    {R"(about:blank)", R"(sc://]/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 sc://]/)"},
    {R"(sc://\u00F1)", R"(x)", R"(1 0 0 0 2 5 5 5 11 12 13 13 1 sc://%C3%B1/x)"},
    {R"(about:blank)", R"(sc:\u005C../)", R"(1 0 1 0 2 3 3 3 3 7 7 7 1 sc:\u005C../)"},
    {R"(about:blank)", R"(sc::a@example.net)", R"(1 0 1 0 2 3 3 3 3 3 17 17 1 sc::a@example.net)"},
    {R"(about:blank)", R"(wow:%NBD)", R"(1 0 1 0 3 4 4 4 4 4 8 8 1 wow:%NBD)"},
    {R"(about:blank)", R"(wow:%1G)", R"(1 0 1 0 3 4 4 4 4 4 7 7 1 wow:%1G)"},
    {R"(about:blank)", R"(wow:\uFFFF)", R"(1 0 1 0 3 4 4 4 4 4 13 13 1 wow:%EF%BF%BF)"},
    {R"(about:blank)", R"(http://example.com/\uD800\uD801\uDFFE\uDFFF\uFDD0\uFDCF\uFDEF\uFDF0\uFFFE\uFFFF?\uD800\uD801\uDFFE\uDFFF\uFDD0\uFDCF\uFDEF\uFDF0\uFFFE\uFFFF)", R"(1 1 0 0 4 7 7 7 18 19 103 188 1 http://example.com/%EF%BF%BD%F0%90%9F%BE%EF%BF%BD%EF%B7%90%EF%B7%8F%EF%B7%AF%EF%B7%B0%EF%BF%BE%EF%BF%BF?%EF%BF%BD%F0%90%9F%BE%EF%BF%BD%EF%B7%90%EF%B7%8F%EF%B7%AF%EF%B7%B0%EF%BF%BE%EF%BF%BF)"},
    {R"(about:blank)", R"(http://a<b)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://a<b)"},
    {R"(about:blank)", R"(http://a>b)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://a>b)"},
    {R"(about:blank)", R"(http://a^b)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://a^b)"},
    {R"(about:blank)", R"(non-special://a<b)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 non-special://a<b)"},
    {R"(about:blank)", R"(non-special://a>b)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 non-special://a>b)"},
    {R"(about:blank)", R"(non-special://a^b)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 non-special://a^b)"},
    {R"(about:blank)", R"(http://\u001F!"$&'()*+,-.;=_`{}~/)", R"(1 1 0 0 4 7 7 7 27 28 28 28 1 http://\u001F!"$&'()*+,-.;=_`{}~/)"},
    {R"(about:blank)", R"(sc://\u001F!"$&'()*+,-.;=_`{}~/)", R"(1 0 0 0 2 5 5 5 27 28 28 28 1 sc://%1F!"$&'()*+,-.;=_`{}~/)"},
    {R"(about:blank)", R"(ftp://example.com%80/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 ftp://example.com%80/)"},
    {R"(about:blank)", R"(ftp://example.com%A0/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 ftp://example.com%A0/)"},
    {R"(about:blank)", R"(https://example.com%80/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://example.com%80/)"},
    {R"(about:blank)", R"(https://example.com%A0/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://example.com%A0/)"},
    {R"(about:blank)", R"(http://127.0.0.1:10100/relative_import.html)", R"(1 1 0 6 4 7 7 7 16 23 43 43 1 http://127.0.0.1:10100/relative_import.html)"},
    {R"(about:blank)", R"(http://facebook.com/?foo=%7B%22abc%22)", R"(1 1 0 0 4 7 7 7 19 20 20 37 1 http://facebook.com/?foo=%7B%22abc%22)"},
    {R"(about:blank)", R"(https://localhost:3000/jqueryui@1.2.3)", R"(1 1 0 5 5 8 8 8 17 23 37 37 1 https://localhost:3000/jqueryui@1.2.3)"},
    {R"(about:blank)", R"(h\u0009t\u000At\u000Dp://h\u0009o\u000As\u000Dt:9\u00090\u000A0\u000D0/p\u0009a\u000At\u000Dh?q\u0009u\u000Ae\u000Dry#f\u0009r\u000Aa\u000Dg)", R"(1 1 0 5 4 7 7 7 11 17 21 27 1 http://host:9000/path?query#frag)"},
    {R"(http://example.org/foo/bar)", R"(?a=b&c=d)", R"(1 1 0 0 4 7 7 7 18 23 26 34 1 http://example.org/foo/bar?a=b&c=d)"},
    {R"(http://example.org/foo/bar)", R"(??a=b&c=d)", R"(1 1 0 0 4 7 7 7 18 23 26 35 1 http://example.org/foo/bar??a=b&c=d)"},
    {R"(http://example.org/foo/bar)", R"(http:)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar)"},
    {R"(https://example.org/foo/bar)", R"(http:)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http:)"},
    {R"(https://example.org/foo/bar)", R"(sc:)", R"(1 0 1 0 2 3 3 3 3 3 3 3 1 sc:)"},
    {R"(about:blank)", R"(http://foo.bar/baz?qux#foo\u0008bar)", R"(1 1 0 0 4 7 7 7 14 15 18 22 1 http://foo.bar/baz?qux#foo%08bar)"},
    {R"(about:blank)", R"(http://foo.bar/baz?qux#foo"bar)", R"(1 1 0 0 4 7 7 7 14 15 18 22 1 http://foo.bar/baz?qux#foo%22bar)"},
    {R"(about:blank)", R"(http://foo.bar/baz?qux#foo<bar)", R"(1 1 0 0 4 7 7 7 14 15 18 22 1 http://foo.bar/baz?qux#foo%3Cbar)"},
    {R"(about:blank)", R"(http://foo.bar/baz?qux#foo>bar)", R"(1 1 0 0 4 7 7 7 14 15 18 22 1 http://foo.bar/baz?qux#foo%3Ebar)"},
    {R"(about:blank)", R"(http://foo.bar/baz?qux#foo`bar)", R"(1 1 0 0 4 7 7 7 14 15 18 22 1 http://foo.bar/baz?qux#foo%60bar)"},
    {R"(http://other.com/)", R"(http://192.168.257)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://192.168.1.1/)"},
    {R"(http://other.com/)", R"(http://192.168.257.com)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://192.168.257.com/)"},
    {R"(http://other.com/)", R"(http://256)", R"(1 1 0 0 4 7 7 7 14 15 15 15 1 http://0.0.1.0/)"},
    {R"(http://other.com/)", R"(http://256.com)", R"(1 1 0 0 4 7 7 7 14 15 15 15 1 http://256.com/)"},
    {R"(http://other.com/)", R"(http://999999999)", R"(1 1 0 0 4 7 7 7 21 22 22 22 1 http://59.154.201.255/)"},
    {R"(http://other.com/)", R"(http://999999999.com)", R"(1 1 0 0 4 7 7 7 20 21 21 21 1 http://999999999.com/)"},
    {R"(http://other.com/)", R"(http://10000000000)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://10000000000)"},
    {R"(http://other.com/)", R"(http://10000000000.com)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://10000000000.com/)"},
    {R"(http://other.com/)", R"(http://4294967295)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://255.255.255.255/)"},
    {R"(http://other.com/)", R"(http://4294967296)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://4294967296)"},
    {R"(http://other.com/)", R"(http://0xffffffff)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://255.255.255.255/)"},
    {R"(http://other.com/)", R"(http://0xffffffff1)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://0xffffffff1)"},
    {R"(http://other.com/)", R"(http://256.256.256.256)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://256.256.256.256)"},
    {R"(http://other.com/)", R"(http://256.256.256.256.256)", R"(1 1 0 0 4 7 7 7 26 27 27 27 1 http://256.256.256.256.256/)"},
    {R"(about:blank)", R"(https://0x.0x.0)", R"(1 1 0 0 5 8 8 8 15 16 16 16 1 https://0.0.0.0/)"},
    {R"(about:blank)", R"(https://0x100000000/test)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://0x100000000/test)"},
    {R"(about:blank)", R"(https://256.0.0.1/test)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://256.0.0.1/test)"},
    {R"(about:blank)", R"(file:///C%3A/)", R"(1 0 0 0 4 7 7 7 7 13 13 13 1 file:///C%3A/)"},
    {R"(about:blank)", R"(file:///C%7C/)", R"(1 0 0 0 4 7 7 7 7 13 13 13 1 file:///C%7C/)"},
    {R"(about:blank)", R"(file://%43%3A)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://%43%3A)"},
    {R"(about:blank)", R"(file://%43%7C)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://%43%7C)"},
    {R"(about:blank)", R"(file://%43|)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://%43|)"},
    {R"(about:blank)", R"(file://C%7C)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://C%7C)"},
    {R"(about:blank)", R"(file://%43%7C/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 file://%43%7C/)"},
    {R"(about:blank)", R"(https://%43%7C/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://%43%7C/)"},
    {R"(about:blank)", R"(asdf://%43|/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 asdf://%43|/)"},
    {R"(about:blank)", R"(asdf://%43%7C/)", R"(1 0 0 0 4 7 7 7 13 14 14 14 1 asdf://%43%7C/)"},
    {R"(file:///C:/Users/Domenic/Dropbox/GitHub/tmpvar/jsdom/test/level2/html/files/anchor.html)", R"(pix/submit.gif)", R"(1 0 0 0 4 7 7 7 7 80 90 90 1 file:///C:/Users/Domenic/Dropbox/GitHub/tmpvar/jsdom/test/level2/html/files/pix/submit.gif)"},
    {R"(file:///C:/)", R"(..)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file:///C:/)"},
    {R"(file:///)", R"(..)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file:///C:/a/b)", R"(/)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file:///C:/)"},
    {R"(file://h/C:/a/b)", R"(/)", R"(1 0 0 0 4 7 7 7 8 12 12 12 1 file://h/C:/)"},
    {R"(file://h/a/b)", R"(/)", R"(1 0 0 0 4 7 7 7 8 9 9 9 1 file://h/)"},
    {R"(file:///C:/a/b)", R"(//d:)", R"(1 0 0 0 4 7 7 7 7 10 10 10 1 file:///d:)"},
    {R"(file:///C:/a/b)", R"(//d:/..)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file:///d:/)"},
    {R"(file:///ab:/)", R"(..)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file:///1:/)", R"(..)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file:///test?test#test)", R"()", R"(1 0 0 0 4 7 7 7 7 8 12 17 1 file:///test?test)"},
    {R"(file:///test?test#test)", R"(file:)", R"(1 0 0 0 4 7 7 7 7 8 12 17 1 file:///test?test)"},
    {R"(file:///test?test#test)", R"(?x)", R"(1 0 0 0 4 7 7 7 7 8 12 14 1 file:///test?x)"},
    {R"(file:///test?test#test)", R"(file:?x)", R"(1 0 0 0 4 7 7 7 7 8 12 14 1 file:///test?x)"},
    {R"(file:///test?test#test)", R"(#x)", R"(1 0 0 0 4 7 7 7 7 8 12 17 1 file:///test?test#x)"},
    {R"(file:///test?test#test)", R"(file:#x)", R"(1 0 0 0 4 7 7 7 7 8 12 17 1 file:///test?test#x)"},
    {R"(about:blank)", R"(file:\u005C\u005C//)", R"(1 0 0 0 4 7 7 7 7 9 9 9 1 file:////)"},
    {R"(about:blank)", R"(file:\u005C\u005C\u005C\u005C)", R"(1 0 0 0 4 7 7 7 7 9 9 9 1 file:////)"},
    {R"(about:blank)", R"(file:\u005C\u005C\u005C\u005C?fox)", R"(1 0 0 0 4 7 7 7 7 9 9 13 1 file:////?fox)"},
    {R"(about:blank)", R"(file:\u005C\u005C\u005C\u005C#guppy)", R"(1 0 0 0 4 7 7 7 7 9 9 9 1 file:////#guppy)"},
    {R"(about:blank)", R"(file://spider///)", R"(1 0 0 0 4 7 7 7 13 16 16 16 1 file://spider///)"},
    {R"(about:blank)", R"(file:\u005C\u005Clocalhost//)", R"(1 0 0 0 4 7 7 7 7 9 9 9 1 file:////)"},
    {R"(about:blank)", R"(file:///localhost//cat)", R"(1 0 0 0 4 7 7 7 7 19 22 22 1 file:///localhost//cat)"},
    {R"(about:blank)", R"(file://\u005C/localhost//cat)", R"(1 0 0 0 4 7 7 7 7 20 23 23 1 file:////localhost//cat)"},
    {R"(about:blank)", R"(file://localhost//a//../..//)", R"(1 0 0 0 4 7 7 7 7 10 10 10 1 file://///)"},
    {R"(file:///elephant)", R"(/////mouse)", R"(1 0 0 0 4 7 7 7 7 10 15 15 1 file://///mouse)"},
    {R"(file://lion/)", R"(\u005C//pig)", R"(1 0 0 0 4 7 7 7 7 8 11 11 1 file:///pig)"},
    {R"(file://lion/)", R"(\u005C/localhost//pig)", R"(1 0 0 0 4 7 7 7 7 9 12 12 1 file:////pig)"},
    {R"(file://lion/)", R"(//localhost//pig)", R"(1 0 0 0 4 7 7 7 7 9 12 12 1 file:////pig)"},
    {R"(file://lion/)", R"(/..//localhost//pig)", R"(1 0 0 0 4 7 7 7 11 24 27 27 1 file://lion//localhost//pig)"},
    {R"(file://ape/)", R"(file://)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(file://tea/)", R"(/rooibos)", R"(1 0 0 0 4 7 7 7 10 11 18 18 1 file://tea/rooibos)"},
    {R"(file://tea/)", R"(/?chai)", R"(1 0 0 0 4 7 7 7 10 11 11 16 1 file://tea/?chai)"},
    {R"(file://host/dir/file)", R"(C|)", R"(1 0 0 0 4 7 7 7 11 12 14 14 1 file://host/C:)"},
    {R"(file://host/D:/dir1/dir2/file)", R"(C|)", R"(1 0 0 0 4 7 7 7 11 12 14 14 1 file://host/C:)"},
    {R"(file://host/dir/file)", R"(C|#)", R"(1 0 0 0 4 7 7 7 11 12 14 14 1 file://host/C:#)"},
    {R"(file://host/dir/file)", R"(C|?)", R"(1 0 0 0 4 7 7 7 11 12 14 15 1 file://host/C:?)"},
    {R"(file://host/dir/file)", R"(C|/)", R"(1 0 0 0 4 7 7 7 11 15 15 15 1 file://host/C:/)"},
    {R"(file://host/dir/file)", R"(C|\u000A/)", R"(1 0 0 0 4 7 7 7 11 15 15 15 1 file://host/C:/)"},
    {R"(file://host/dir/file)", R"(C|\u005C)", R"(1 0 0 0 4 7 7 7 11 15 15 15 1 file://host/C:/)"},
    {R"(file://host/dir/file)", R"(C)", R"(1 0 0 0 4 7 7 7 11 16 17 17 1 file://host/dir/C)"},
    {R"(file://host/dir/file)", R"(C|a)", R"(1 0 0 0 4 7 7 7 11 16 19 19 1 file://host/dir/C|a)"},
    {R"(file:///c:/baz/qux)", R"(/c:/foo/bar)", R"(1 0 0 0 4 7 7 7 7 15 18 18 1 file:///c:/foo/bar)"},
    {R"(file:///c:/baz/qux)", R"(/c|/foo/bar)", R"(1 0 0 0 4 7 7 7 7 15 18 18 1 file:///c:/foo/bar)"},
    {R"(file:///c:/baz/qux)", R"(file:\u005Cc:\u005Cfoo\u005Cbar)", R"(1 0 0 0 4 7 7 7 7 15 18 18 1 file:///c:/foo/bar)"},
    {R"(file://host/path)", R"(/c:/foo/bar)", R"(1 0 0 0 4 7 7 7 11 19 22 22 1 file://host/c:/foo/bar)"},
    {R"(about:blank)", R"(file://example.net/C:/)", R"(1 0 0 0 4 7 7 7 18 22 22 22 1 file://example.net/C:/)"},
    {R"(about:blank)", R"(file://1.2.3.4/C:/)", R"(1 0 0 0 4 7 7 7 14 18 18 18 1 file://1.2.3.4/C:/)"},
    {R"(about:blank)", R"(file://[1::8]/C:/)", R"(1 0 0 0 4 7 7 7 13 17 17 17 1 file://[1::8]/C:/)"},
    {R"(about:blank)", R"(file:/C|/)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file:///C:/)"},
    {R"(about:blank)", R"(file://C|/)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file:///C:/)"},
    {R"(about:blank)", R"(file:)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(about:blank)", R"(file:?q=v)", R"(1 0 0 0 4 7 7 7 7 8 8 12 1 file:///?q=v)"},
    {R"(about:blank)", R"(file:#frag)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///#frag)"},
    {R"(about:blank)", R"(file:///Y:)", R"(1 0 0 0 4 7 7 7 7 8 10 10 1 file:///Y:)"},
    {R"(about:blank)", R"(file:///Y:/)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file:///Y:/)"},
    {R"(about:blank)", R"(file:///./Y)", R"(1 0 0 0 4 7 7 7 7 8 9 9 1 file:///Y)"},
    {R"(about:blank)", R"(file:///./Y:)", R"(1 0 0 0 4 7 7 7 7 8 10 10 1 file:///Y:)"},
    {R"(about:blank)", R"(\u005C\u005C\u005C.\u005CY:)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 \u005C\u005C\u005C.\u005CY:)"},
    {R"(about:blank)", R"(file:///y:)", R"(1 0 0 0 4 7 7 7 7 8 10 10 1 file:///y:)"},
    {R"(about:blank)", R"(file:///y:/)", R"(1 0 0 0 4 7 7 7 7 11 11 11 1 file:///y:/)"},
    {R"(about:blank)", R"(file:///./y)", R"(1 0 0 0 4 7 7 7 7 8 9 9 1 file:///y)"},
    {R"(about:blank)", R"(file:///./y:)", R"(1 0 0 0 4 7 7 7 7 8 10 10 1 file:///y:)"},
    {R"(about:blank)", R"(\u005C\u005C\u005C.\u005Cy:)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 \u005C\u005C\u005C.\u005Cy:)"},
    {R"(about:blank)", R"(file://localhost//a//../..//foo)", R"(1 0 0 0 4 7 7 7 7 10 13 13 1 file://///foo)"},
    {R"(about:blank)", R"(file://localhost////foo)", R"(1 0 0 0 4 7 7 7 7 11 14 14 1 file://////foo)"},
    {R"(about:blank)", R"(file:////foo)", R"(1 0 0 0 4 7 7 7 7 9 12 12 1 file:////foo)"},
    {R"(file:///)", R"(file:///one/two)", R"(1 0 0 0 4 7 7 7 7 12 15 15 1 file:///one/two)"},
    {R"(file:///)", R"(file:////one/two)", R"(1 0 0 0 4 7 7 7 7 13 16 16 1 file:////one/two)"},
    {R"(file:///)", R"(//one/two)", R"(1 0 0 0 4 7 7 7 10 11 14 14 1 file://one/two)"},
    {R"(file:///)", R"(///one/two)", R"(1 0 0 0 4 7 7 7 7 12 15 15 1 file:///one/two)"},
    {R"(file:///)", R"(////one/two)", R"(1 0 0 0 4 7 7 7 7 13 16 16 1 file:////one/two)"},
    {R"(file:////)", R"(file:///.//)", R"(1 0 0 0 4 7 7 7 7 9 9 9 1 file:////)"},
    {R"(about:blank)", R"(file:.//p)", R"(1 0 0 0 4 7 7 7 7 9 10 10 1 file:////p)"},
    {R"(about:blank)", R"(file:/.//p)", R"(1 0 0 0 4 7 7 7 7 9 10 10 1 file:////p)"},
    {R"(http://example.net/)", R"(http://[1:0::])", R"(1 1 0 0 4 7 7 7 12 13 13 13 1 http://[1::]/)"},
    {R"(http://example.net/)", R"(http://[0:1:2:3:4:5:6:7:8])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[0:1:2:3:4:5:6:7:8])"},
    {R"(about:blank)", R"(https://[0::0::0])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://[0::0::0])"},
    {R"(about:blank)", R"(https://[0:.0])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://[0:.0])"},
    {R"(about:blank)", R"(https://[0:0:])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://[0:0:])"},
    {R"(about:blank)", R"(https://[0:1:2:3:4:5:6:7.0.0.0.1])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://[0:1:2:3:4:5:6:7.0.0.0.1])"},
    {R"(about:blank)", R"(https://[0:1.00.0.0.0])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://[0:1.00.0.0.0])"},
    {R"(about:blank)", R"(https://[0:1.290.0.0.0])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://[0:1.290.0.0.0])"},
    {R"(about:blank)", R"(https://[0:1.23.23])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 https://[0:1.23.23])"},
    {R"(about:blank)", R"(http://?)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://?)"},
    {R"(about:blank)", R"(http://#)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://#)"},
    {R"(http://example.org/)", R"(http://f:4294967377/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:4294967377/c)"},
    {R"(http://example.org/)", R"(http://f:18446744073709551697/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:18446744073709551697/c)"},
    {R"(http://example.org/)", R"(http://f:340282366920938463463374607431768211537/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:340282366920938463463374607431768211537/c)"},
    {R"(about:blank)", R"(sc://\u00F1)", R"(1 0 0 0 2 5 5 5 11 11 11 11 1 sc://%C3%B1)"},
    {R"(about:blank)", R"(sc://\u00F1?x)", R"(1 0 0 0 2 5 5 5 11 11 11 13 1 sc://%C3%B1?x)"},
    {R"(about:blank)", R"(sc://\u00F1#x)", R"(1 0 0 0 2 5 5 5 11 11 11 11 1 sc://%C3%B1#x)"},
    {R"(sc://\u00F1)", R"(#x)", R"(1 0 0 0 2 5 5 5 11 11 11 11 1 sc://%C3%B1#x)"},
    {R"(sc://\u00F1)", R"(?x)", R"(1 0 0 0 2 5 5 5 11 11 11 13 1 sc://%C3%B1?x)"},
    {R"(about:blank)", R"(sc://?)", R"(1 0 0 0 2 5 5 5 5 5 5 6 1 sc://?)"},
    {R"(about:blank)", R"(sc://#)", R"(1 0 0 0 2 5 5 5 5 5 5 5 1 sc://#)"},
    {R"(sc://x/)", R"(///)", R"(1 0 0 0 2 5 5 5 5 6 6 6 1 sc:///)"},
    {R"(sc://x/)", R"(////)", R"(1 0 0 0 2 5 5 5 5 7 7 7 1 sc:////)"},
    {R"(sc://x/)", R"(////x/)", R"(1 0 0 0 2 5 5 5 5 9 9 9 1 sc:////x/)"},
    {R"(about:blank)", R"(tftp://foobar.com/someconfig;mode=netascii)", R"(1 0 0 0 4 7 7 7 17 18 42 42 1 tftp://foobar.com/someconfig;mode=netascii)"},
    {R"(about:blank)", R"(telnet://user:pass@foobar.com:23/)", R"(1 0 0 3 6 9 13 18 29 33 33 33 1 telnet://user:pass@foobar.com:23/)"},
    {R"(about:blank)", R"(ut2004://10.10.10.10:7777/Index.ut2)", R"(1 0 0 5 6 9 9 9 20 26 35 35 1 ut2004://10.10.10.10:7777/Index.ut2)"},
    {R"(about:blank)", R"(redis://foo:bar@somehost:6379/0?baz=bam&qux=baz)", R"(1 0 0 5 5 8 11 15 24 30 31 47 1 redis://foo:bar@somehost:6379/0?baz=bam&qux=baz)"},
    {R"(about:blank)", R"(rsync://foo@host:911/sup)", R"(1 0 0 4 5 8 11 11 16 21 24 24 1 rsync://foo@host:911/sup)"},
    {R"(about:blank)", R"(git://github.com/foo/bar.git)", R"(1 0 0 0 3 6 6 6 16 21 28 28 1 git://github.com/foo/bar.git)"},
    {R"(about:blank)", R"(irc://myserver.com:6999/channel?passwd)", R"(1 0 0 5 3 6 6 6 18 24 31 38 1 irc://myserver.com:6999/channel?passwd)"},
    {R"(about:blank)", R"(dns://fw.example.org:9999/foo.bar.org?type=TXT)", R"(1 0 0 5 3 6 6 6 20 26 37 46 1 dns://fw.example.org:9999/foo.bar.org?type=TXT)"},
    {R"(about:blank)", R"(ldap://localhost:389/ou=People,o=JNDITutorial)", R"(1 0 0 4 4 7 7 7 16 21 45 45 1 ldap://localhost:389/ou=People,o=JNDITutorial)"},
    {R"(about:blank)", R"(git+https://github.com/foo/bar)", R"(1 0 0 0 9 12 12 12 22 27 30 30 1 git+https://github.com/foo/bar)"},
    {R"(about:blank)", R"(urn:ietf:rfc:2648)", R"(1 0 1 0 3 4 4 4 4 4 17 17 1 urn:ietf:rfc:2648)"},
    {R"(about:blank)", R"(tag:joe@example.org,2001:foo/bar)", R"(1 0 1 0 3 4 4 4 4 29 32 32 1 tag:joe@example.org,2001:foo/bar)"},
    {R"(about:blank)", R"(non-spec:/.//)", R"(1 0 0 0 8 9 9 9 9 13 13 13 1 non-spec:/.//)"},
    {R"(about:blank)", R"(non-spec:/..//)", R"(1 0 0 0 8 9 9 9 9 13 13 13 1 non-spec:/.//)"},
    {R"(about:blank)", R"(non-spec:/a/..//)", R"(1 0 0 0 8 9 9 9 9 13 13 13 1 non-spec:/.//)"},
    {R"(about:blank)", R"(non-spec:/.//path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(about:blank)", R"(non-spec:/..//path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(about:blank)", R"(non-spec:/a/..//path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(non-spec:/p)", R"(/.//path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(non-spec:/p)", R"(/..//path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(non-spec:/p)", R"(..//path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(non-spec:/p)", R"(a/..//path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(non-spec:/..//p)", R"()", R"(1 0 0 0 8 9 9 9 9 13 14 14 1 non-spec:/.//p)"},
    {R"(non-spec:/..//p)", R"(path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(non-spec:/.//p)", R"(../path)", R"(1 0 0 0 8 9 9 9 9 10 14 14 1 non-spec:/path)"},
    {R"(about:blank)", R"(non-special://%E2%80%A0/)", R"(1 0 0 0 11 14 14 14 23 24 24 24 1 non-special://%E2%80%A0/)"},
    {R"(about:blank)", R"(non-special://H%4fSt/path)", R"(1 0 0 0 11 14 14 14 20 21 25 25 1 non-special://H%4fSt/path)"},
    {R"(about:blank)", R"(non-special://[1:2:0:0:5:0:0:0]/)", R"(1 0 0 0 11 14 14 14 27 28 28 28 1 non-special://[1:2:0:0:5::]/)"},
    {R"(about:blank)", R"(non-special://[1:2:0:0:0:0:0:3]/)", R"(1 0 0 0 11 14 14 14 22 23 23 23 1 non-special://[1:2::3]/)"},
    {R"(about:blank)", R"(non-special://[1:2::3]:80/)", R"(1 0 0 3 11 14 14 14 22 26 26 26 1 non-special://[1:2::3]:80/)"},
    {R"(about:blank)", R"(non-special://[:80/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 non-special://[:80/)"},
    {R"(about:blank)", R"(blob:https://example.com:443/)", R"(1 0 1 0 4 5 5 5 5 29 29 29 1 blob:https://example.com:443/)"},
    {R"(about:blank)", R"(blob:d3958f5c-0777-0845-9dcf-2cb28783acaf)", R"(1 0 1 0 4 5 5 5 5 5 41 41 1 blob:d3958f5c-0777-0845-9dcf-2cb28783acaf)"},
    {R"(about:blank)", R"(http://0177.0.0.0189)", R"(1 1 0 0 4 7 7 7 20 21 21 21 1 http://0177.0.0.0189/)"},
    {R"(about:blank)", R"(http://0x7f.0.0.0x7g)", R"(1 1 0 0 4 7 7 7 20 21 21 21 1 http://0x7f.0.0.0x7g/)"},
    {R"(about:blank)", R"(http://0X7F.0.0.0X7G)", R"(1 1 0 0 4 7 7 7 20 21 21 21 1 http://0x7f.0.0.0x7g/)"},
    {R"(about:blank)", R"(http://[::127.0.0.0.1])", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://[::127.0.0.0.1])"},
    {R"(about:blank)", R"(http://[0:1:0:1:0:1:0:1])", R"(1 1 0 0 4 7 7 7 24 25 25 25 1 http://[0:1:0:1:0:1:0:1]/)"},
    {R"(about:blank)", R"(http://[1:0:1:0:1:0:1:0])", R"(1 1 0 0 4 7 7 7 24 25 25 25 1 http://[1:0:1:0:1:0:1:0]/)"},
    {R"(about:blank)", R"(http://example.org/test?")", R"(1 1 0 0 4 7 7 7 18 19 23 27 1 http://example.org/test?%22)"},
    {R"(about:blank)", R"(http://example.org/test?#)", R"(1 1 0 0 4 7 7 7 18 19 23 24 1 http://example.org/test?#)"},
    {R"(about:blank)", R"(http://example.org/test?<)", R"(1 1 0 0 4 7 7 7 18 19 23 27 1 http://example.org/test?%3C)"},
    {R"(about:blank)", R"(http://example.org/test?>)", R"(1 1 0 0 4 7 7 7 18 19 23 27 1 http://example.org/test?%3E)"},
    {R"(about:blank)", R"(http://example.org/test?\u2323)", R"(1 1 0 0 4 7 7 7 18 19 23 33 1 http://example.org/test?%E2%8C%A3)"},
    {R"(about:blank)", R"(http://example.org/test?%23%23)", R"(1 1 0 0 4 7 7 7 18 19 23 30 1 http://example.org/test?%23%23)"},
    {R"(about:blank)", R"(http://example.org/test?%GH)", R"(1 1 0 0 4 7 7 7 18 19 23 27 1 http://example.org/test?%GH)"},
    {R"(about:blank)", R"(http://example.org/test?a#%EF)", R"(1 1 0 0 4 7 7 7 18 19 23 25 1 http://example.org/test?a#%EF)"},
    {R"(about:blank)", R"(http://example.org/test?a#%GH)", R"(1 1 0 0 4 7 7 7 18 19 23 25 1 http://example.org/test?a#%GH)"},
    {R"(about:blank)", R"(a)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 a)"},
    {R"(about:blank)", R"(a/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 a/)"},
    {R"(about:blank)", R"(a//)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 a//)"},
    {R"(a:)", R"(test-a-colon.html)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 test-a-colon.html)"},
    {R"(a:b)", R"(test-a-colon-b.html)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 test-a-colon-b.html)"},
    {R"(a:/)", R"(test-a-colon-slash.html)", R"(1 0 0 0 1 2 2 2 2 3 26 26 1 a:/test-a-colon-slash.html)"},
    {R"(a://)", R"(test-a-colon-slash-slash.html)", R"(1 0 0 0 1 4 4 4 4 5 34 34 1 a:///test-a-colon-slash-slash.html)"},
    {R"(a:/b)", R"(test-a-colon-slash-b.html)", R"(1 0 0 0 1 2 2 2 2 3 28 28 1 a:/test-a-colon-slash-b.html)"},
    {R"(a://b)", R"(test-a-colon-slash-slash-b.html)", R"(1 0 0 0 1 4 4 4 5 6 37 37 1 a://b/test-a-colon-slash-slash-b.html)"},
    {R"(about:blank)", R"(http://example.org/test?a#b\u0000c)", R"(1 1 0 0 4 7 7 7 18 19 23 25 1 http://example.org/test?a#b%00c)"},
    {R"(about:blank)", R"(non-spec://example.org/test?a#b\u0000c)", R"(1 0 0 0 8 11 11 11 22 23 27 29 1 non-spec://example.org/test?a#b%00c)"},
    {R"(about:blank)", R"(non-spec:/test?a#b\u0000c)", R"(1 0 0 0 8 9 9 9 9 10 14 16 1 non-spec:/test?a#b%00c)"},
    {R"(file:///some/dir/bar.html)", R"(10.0.0.7:8080/foo.html)", R"(1 0 0 0 4 7 7 7 7 31 39 39 1 file:///some/dir/10.0.0.7:8080/foo.html)"},
    {R"(file:///some/dir/bar.html)", R"(a!@$*=/foo.html)", R"(1 0 0 0 4 7 7 7 7 24 32 32 1 file:///some/dir/a!@$*=/foo.html)"},
    {R"(http://example.com/dir/file)", R"(a1234567890-+.:foo/bar)", R"(1 0 1 0 14 15 15 15 15 19 22 22 1 a1234567890-+.:foo/bar)"},
    {R"(https://example.org/##link)", R"(#link)", R"(1 1 0 0 5 8 8 8 19 20 20 20 1 https://example.org/#link)"},
    {R"(about:blank)", R"(non-special:cannot-be-a-base-url-\u0000\u0001\u001F\u001E~\u007F\u0080)", R"(1 0 1 0 11 12 12 12 12 12 55 55 1 non-special:cannot-be-a-base-url-%00%01%1F%1E~%7F%C2%80)"},
    {R"(about:blank)", R"(https://www.example.com/path{\u007Fpath.html?query'\u007F=query#fragment<\u007Ffragment)", R"(1 1 0 0 5 8 8 8 23 24 43 61 1 https://www.example.com/path%7B%7Fpath.html?query%27%7F=query#fragment%3C%7Ffragment)"},
    {R"(http://example.org)", R"(https://user:pass[\u007F@foo/bar)", R"(1 1 0 0 5 8 12 23 27 28 31 31 1 https://user:pass%5B%7F@foo/bar)"},
    {R"(http://example.org/foo/bar)", R"(\Whttp://example\u0009.\u000Aorg)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.org/)"},
    {R"(http://example.org/foo/bar)", R"(\W foo.com  )", R"(1 1 0 0 4 7 7 7 18 23 30 30 1 http://example.org/foo/foo.com)"},
    {R"(http://example.org/foo/bar)", R"(\Whttp://f:b/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://f:b/c)"},
    {R"(http://example.org/foo/bar)", R"(\W  \u0009)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar)"},
    {R"(http://example.org/foo/bar)", R"(\W#)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar#)"},
    {R"(http://example.org/foo/bar)", R"(\W::)", R"(1 1 0 0 4 7 7 7 18 23 25 25 1 http://example.org/foo/::)"},
    {R"(http://example.org/foo/bar)", R"(\Whttp://foo.com/\u005C@)", R"(1 1 0 0 4 7 7 7 14 16 17 17 1 http://foo.com//@)"},
    {R"(http://example.org/foo/bar)", R"(\Wc:/foo)", R"(1 0 0 0 1 2 2 2 2 3 6 6 0 c:/foo)"},
    {R"(http://example.org/foo/bar)", R"(\Whttp://2001::1)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://2001::1)"},
    {R"(http://example.org/foo/bar)", R"(\Wftp:/example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 ftp://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(\Wftps:/example.com/)", R"(1 0 0 0 4 5 5 5 5 18 18 18 0 ftps:/example.com/)"},
    {R"(http://example.org/foo/bar)", R"(\Wftp:example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 ftp://example.com/)"},
    {R"(http://example.org/foo/bar)", R"(\Wjavascript:example.com/)", R"(1 0 1 0 10 11 11 11 11 23 23 23 0 javascript:example.com/)"},
    {R"(http://example.org/foo/bar)", R"(\Wtel:1234567890)", R"(1 0 1 0 3 4 4 4 4 4 14 14 0 tel:1234567890)"},
    {R"(file:///tmp/mock/path)", R"(\W\u005C\u005Cserver\u005Cfile)", R"(1 0 0 0 4 7 7 7 13 14 18 18 1 file://server/file)"},
    {R"(file:///tmp/mock/path)", R"(\Wfile://localhost)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///)"},
    {R"(about:blank)", R"(\Whttp://example.com/foo/./)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.com/foo/)"},
    {R"(about:blank)", R"(\Whttp://example.com/foo/%2e)", R"(1 1 0 0 4 7 7 7 18 23 23 23 1 http://example.com/foo/)"},
    {R"(about:blank)", R"(\Whttp://example.com/foo%)", R"(1 1 0 0 4 7 7 7 18 19 23 23 0 http://example.com/foo%)"},
    {R"(about:blank)", R"(\Whttp://example.com/%3A%3a%3C%3c)", R"(1 1 0 0 4 7 7 7 18 19 31 31 0 http://example.com/%3A%3a%3C%3c)"},
    {R"(about:blank)", R"(\Whttp://example.com/\u202E/foo/\u202D/bar)", R"(1 1 0 0 4 7 7 7 18 43 46 46 1 http://example.com/%E2%80%AE/foo/%E2%80%AD/bar)"},
    {R"(about:blank)", R"(\Whttp://user:pass@/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://user:pass@/)"},
    {R"(about:blank)", R"(\Whttps://foo:80/)", R"(1 1 0 3 5 8 8 8 11 15 15 15 0 https://foo:80/)"},
    {R"(about:blank)", R"(\Wws://foo:815/)", R"(1 0 0 4 2 5 5 5 8 13 13 13 0 ws://foo:815/)"},
    {R"(about:blank)", R"(\Wmadeupscheme:/example.com/)", R"(1 0 0 0 12 13 13 13 13 26 26 26 0 madeupscheme:/example.com/)"},
    {R"(about:blank)", R"(\Wmailto:/example.com/)", R"(1 0 0 0 6 7 7 7 7 20 20 20 0 mailto:/example.com/)"},
    {R"(about:blank)", R"(\Wwss:example.com/)", R"(1 0 0 0 3 6 6 6 17 18 18 18 1 wss://example.com/)"},
    {R"(about:blank)", R"(\Whttp:/a:b@www.example.com)", R"(1 1 0 0 4 7 8 10 26 27 27 27 1 http://a:b@www.example.com/)"},
    {R"(about:blank)", R"(\Whttp:@/www.example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http:@/www.example.com)"},
    {R"(about:blank)", R"(\Whttp:a:@www.example.com)", R"(1 1 0 0 4 7 8 8 24 25 25 25 1 http://a@www.example.com/)"},
    {R"(http://www.example.com/test)", R"(\W/)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://www.example.com/)"},
    {R"(http://www.example.com/test)", R"(\W../../test.txt)", R"(1 1 0 0 4 7 7 7 22 23 31 31 1 http://www.example.com/test.txt)"},
    {R"(http://other.com/)", R"(\Whttp://example example.com)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://example example.com)"},
    {R"(http://other.com/)", R"(\Whttp://%25)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://%25)"},
    {R"(about:blank)", R"(\Whttp://./)", R"(1 1 0 0 4 7 7 7 8 9 9 9 0 http://./)"},
    {R"(http://other.com/)", R"(\Whttp://[::1.])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://[::1.])"},
    {R"(http://doesnotmatter/)", R"(\Whttps://@@@example)", R"(1 1 0 0 5 8 14 14 22 23 23 23 1 https://%40%40@example/)"},
    {R"(sc:sd/sd)", R"(\Wi)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 i)"},
    {R"(sc:///pa/pa)", R"(\W../i)", R"(1 0 0 0 2 5 5 5 5 6 7 7 1 sc:///i)"},
    {R"(sc:/pa/pa)", R"(\W?i)", R"(1 0 0 0 2 3 3 3 3 7 9 11 1 sc:/pa/pa?i)"},
    {R"(about:blank)", R"(\Wabout:/../)", R"(1 0 0 0 5 6 6 6 6 7 7 7 1 about:/)"},
    {R"(about:blank)", R"(\Wsc://@/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 sc://@/)"},
    {R"(about:blank)", R"(\Wsc:\u005C../)", R"(1 0 1 0 2 3 3 3 3 7 7 7 0 sc:\u005C../)"},
    {R"(about:blank)", R"(\Whttp://a^b)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://a^b)"},
    {R"(about:blank)", R"(\Whttps://example.com%80/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 https://example.com%80/)"},
    {R"(http://example.org/foo/bar)", R"(\Whttp:)", R"(1 1 0 0 4 7 7 7 18 23 26 26 1 http://example.org/foo/bar)"},
    {R"(http://other.com/)", R"(\Whttp://192.168.257)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://192.168.1.1/)"},
    {R"(http://other.com/)", R"(\Whttp://4294967295)", R"(1 1 0 0 4 7 7 7 22 23 23 23 1 http://255.255.255.255/)"},
    {R"(about:blank)", R"(\Whttps://256.0.0.1/test)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 https://256.0.0.1/test)"},
    {R"(about:blank)", R"(\Whttps://%43%7C/)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 https://%43%7C/)"},
    {R"(file://h/a/b)", R"(\W/)", R"(1 0 0 0 4 7 7 7 8 9 9 9 1 file://h/)"},
    {R"(file:///test?test#test)", R"(\Wfile:?x)", R"(1 0 0 0 4 7 7 7 7 8 12 14 1 file:///test?x)"},
    {R"(about:blank)", R"(\Wfile:\u005C\u005Clocalhost//)", R"(1 0 0 0 4 7 7 7 7 9 9 9 1 file:////)"},
    {R"(file://lion/)", R"(\W/..//localhost//pig)", R"(1 0 0 0 4 7 7 7 11 24 27 27 1 file://lion//localhost//pig)"},
    {R"(file://host/dir/file)", R"(\WC|/)", R"(1 0 0 0 4 7 7 7 11 15 15 15 1 file://host/C:/)"},
    {R"(file://host/path)", R"(\W/c:/foo/bar)", R"(1 0 0 0 4 7 7 7 11 19 22 22 1 file://host/c:/foo/bar)"},
    {R"(about:blank)", R"(\Wfile:#frag)", R"(1 0 0 0 4 7 7 7 7 8 8 8 1 file:///#frag)"},
    {R"(about:blank)", R"(\Wfile:///./y)", R"(1 0 0 0 4 7 7 7 7 8 9 9 1 file:///y)"},
    {R"(file:///)", R"(\W//one/two)", R"(1 0 0 0 4 7 7 7 10 11 14 14 1 file://one/two)"},
    {R"(about:blank)", R"(\Whttps://[0::0::0])", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 https://[0::0::0])"},
    {R"(about:blank)", R"(\Whttp://#)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 http://#)"},
    {R"(sc://\u00F1)", R"(\W?x)", R"(1 0 0 0 2 5 5 5 11 11 11 13 1 sc://%C3%B1?x)"},
    {R"(about:blank)", R"(\Wut2004://10.10.10.10:7777/Index.ut2)", R"(1 0 0 5 6 9 9 9 20 26 35 35 0 ut2004://10.10.10.10:7777/Index.ut2)"},
    {R"(about:blank)", R"(\Wurn:ietf:rfc:2648)", R"(1 0 1 0 3 4 4 4 4 4 17 17 0 urn:ietf:rfc:2648)"},
    {R"(non-spec:/p)", R"(\W/.//path)", R"(1 0 0 0 8 9 9 9 9 13 17 17 1 non-spec:/.//path)"},
    {R"(about:blank)", R"(\Wnon-special://H%4fSt/path)", R"(1 0 0 0 11 14 14 14 20 21 25 25 0 non-special://H%4fSt/path)"},
    {R"(about:blank)", R"(\Whttp://0x7f.0.0.0x7g)", R"(1 1 0 0 4 7 7 7 20 21 21 21 1 http://0x7f.0.0.0x7g/)"},
    {R"(about:blank)", R"(\Whttp://example.org/test?>)", R"(1 1 0 0 4 7 7 7 18 19 23 27 1 http://example.org/test?%3E)"},
    {R"(about:blank)", R"(\Wa//)", R"(0 0 0 0 0 0 0 0 0 0 0 0 0 a//)"},
    {R"(about:blank)", R"(\Wnon-spec://example.org/test?a#b\u0000c)", R"(1 0 0 0 8 11 11 11 22 23 27 29 1 non-spec://example.org/test?a#b%00c)"},
    {R"(http://example.org)", R"(\Whttps://user:pass[\u007F@foo/bar)", R"(1 1 0 0 5 8 12 23 27 28 31 31 1 https://user:pass%5B%7F@foo/bar)"},
    {R"(\N)", R"(\Ehttp://h/p?)", R"(1 1 0 0 4 7 7 7 8 9 10 11 1 http://h/p?)"},
    {R"(\N)", R"(\E\Whttp://h/p?)", R"(1 1 0 0 4 7 7 7 8 9 10 11 0 http://h/p?)"},
    {R"(\N)", R"(\Ehttp://h/p?a=é)", R"(1 1 0 0 4 7 7 7 8 9 10 19 1 http://h/p?a=%C3%A9)"},
    {R"(\N)", R"(\E\Whttp://h/p?a=é)", R"(1 1 0 0 4 7 7 7 8 9 10 19 1 http://h/p?a=%C3%A9)"},
    {R"(\N)", R"(\Ehttp://h/p?\u20AC=x)", R"(1 1 0 0 4 7 7 7 8 9 10 14 1 http://h/p??=x)"},
    {R"(\N)", R"(\E\Whttp://h/p?\u20AC=x)", R"(1 1 0 0 4 7 7 7 8 9 10 14 1 http://h/p??=x)"},
    {R"(\N)", R"(\Ehttp://h/p?%%41\u3000)", R"(1 1 0 0 4 7 7 7 8 9 10 16 1 http://h/p?%%41?)"},
    {R"(\N)", R"(\E\Whttp://h/p?%%41\u3000)", R"(1 1 0 0 4 7 7 7 8 9 10 16 1 http://h/p?%%41?)"},
    {R"(\N)", R"(\Ehttp://h/p?q=\uD83D\uDE00)", R"(1 1 0 0 4 7 7 7 8 9 10 15 1 http://h/p?q=??)"},
    {R"(\N)", R"(\E\Whttp://h/p?q=\uD83D\uDE00)", R"(1 1 0 0 4 7 7 7 8 9 10 15 1 http://h/p?q=??)"},
    {R"(\N)", R"(\Ehttp://h/p?x y)", R"(1 1 0 0 4 7 7 7 8 9 10 16 1 http://h/p?x%20y)"},
    {R"(\N)", R"(\E\Whttp://h/p?x y)", R"(1 1 0 0 4 7 7 7 8 9 10 16 1 http://h/p?x%20y)"},
    {R"(\N)", R"(\Ehttps://h/?)", R"(1 1 0 0 5 8 8 8 9 10 10 11 1 https://h/?)"},
    {R"(\N)", R"(\E\Whttps://h/?)", R"(1 1 0 0 5 8 8 8 9 10 10 11 0 https://h/?)"},
    {R"(\N)", R"(\Ehttps://h/?a=é)", R"(1 1 0 0 5 8 8 8 9 10 10 19 1 https://h/?a=%C3%A9)"},
    {R"(\N)", R"(\E\Whttps://h/?a=é)", R"(1 1 0 0 5 8 8 8 9 10 10 19 1 https://h/?a=%C3%A9)"},
    {R"(\N)", R"(\Ehttps://h/?\u20AC=x)", R"(1 1 0 0 5 8 8 8 9 10 10 14 1 https://h/??=x)"},
    {R"(\N)", R"(\E\Whttps://h/?\u20AC=x)", R"(1 1 0 0 5 8 8 8 9 10 10 14 1 https://h/??=x)"},
    {R"(\N)", R"(\Ehttps://h/?%%41\u3000)", R"(1 1 0 0 5 8 8 8 9 10 10 16 1 https://h/?%%41?)"},
    {R"(\N)", R"(\E\Whttps://h/?%%41\u3000)", R"(1 1 0 0 5 8 8 8 9 10 10 16 1 https://h/?%%41?)"},
    {R"(\N)", R"(\Ehttps://h/?q=\uD83D\uDE00)", R"(1 1 0 0 5 8 8 8 9 10 10 15 1 https://h/?q=??)"},
    {R"(\N)", R"(\E\Whttps://h/?q=\uD83D\uDE00)", R"(1 1 0 0 5 8 8 8 9 10 10 15 1 https://h/?q=??)"},
    {R"(\N)", R"(\Ehttps://h/?x y)", R"(1 1 0 0 5 8 8 8 9 10 10 16 1 https://h/?x%20y)"},
    {R"(\N)", R"(\E\Whttps://h/?x y)", R"(1 1 0 0 5 8 8 8 9 10 10 16 1 https://h/?x%20y)"},
    {R"(\N)", R"(\Ews://h/?)", R"(1 0 0 0 2 5 5 5 6 7 7 8 1 ws://h/?)"},
    {R"(\N)", R"(\E\Wws://h/?)", R"(1 0 0 0 2 5 5 5 6 7 7 8 0 ws://h/?)"},
    {R"(\N)", R"(\Ews://h/?a=é)", R"(1 0 0 0 2 5 5 5 6 7 7 22 1 ws://h/?a=%C3%83%C2%A9)"},
    {R"(\N)", R"(\E\Wws://h/?a=é)", R"(1 0 0 0 2 5 5 5 6 7 7 22 1 ws://h/?a=%C3%83%C2%A9)"},
    {R"(\N)", R"(\Ews://h/?\u20AC=x)", R"(1 0 0 0 2 5 5 5 6 7 7 19 1 ws://h/?%E2%82%AC=x)"},
    {R"(\N)", R"(\E\Wws://h/?\u20AC=x)", R"(1 0 0 0 2 5 5 5 6 7 7 19 1 ws://h/?%E2%82%AC=x)"},
    {R"(\N)", R"(\Ews://h/?%%41\u3000)", R"(1 0 0 0 2 5 5 5 6 7 7 21 1 ws://h/?%%41%E3%80%80)"},
    {R"(\N)", R"(\E\Wws://h/?%%41\u3000)", R"(1 0 0 0 2 5 5 5 6 7 7 21 1 ws://h/?%%41%E3%80%80)"},
    {R"(\N)", R"(\Ews://h/?q=\uD83D\uDE00)", R"(1 0 0 0 2 5 5 5 6 7 7 22 1 ws://h/?q=%F0%9F%98%80)"},
    {R"(\N)", R"(\E\Wws://h/?q=\uD83D\uDE00)", R"(1 0 0 0 2 5 5 5 6 7 7 22 1 ws://h/?q=%F0%9F%98%80)"},
    {R"(\N)", R"(\Ews://h/?x y)", R"(1 0 0 0 2 5 5 5 6 7 7 13 1 ws://h/?x%20y)"},
    {R"(\N)", R"(\E\Wws://h/?x y)", R"(1 0 0 0 2 5 5 5 6 7 7 13 1 ws://h/?x%20y)"},
    {R"(\N)", R"(\Esc://h/p?)", R"(1 0 0 0 2 5 5 5 6 7 8 9 1 sc://h/p?)"},
    {R"(\N)", R"(\E\Wsc://h/p?)", R"(1 0 0 0 2 5 5 5 6 7 8 9 0 sc://h/p?)"},
    {R"(\N)", R"(\Esc://h/p?a=é)", R"(1 0 0 0 2 5 5 5 6 7 8 23 1 sc://h/p?a=%C3%83%C2%A9)"},
    {R"(\N)", R"(\E\Wsc://h/p?a=é)", R"(1 0 0 0 2 5 5 5 6 7 8 23 1 sc://h/p?a=%C3%83%C2%A9)"},
    {R"(\N)", R"(\Esc://h/p?\u20AC=x)", R"(1 0 0 0 2 5 5 5 6 7 8 20 1 sc://h/p?%E2%82%AC=x)"},
    {R"(\N)", R"(\E\Wsc://h/p?\u20AC=x)", R"(1 0 0 0 2 5 5 5 6 7 8 20 1 sc://h/p?%E2%82%AC=x)"},
    {R"(\N)", R"(\Esc://h/p?%%41\u3000)", R"(1 0 0 0 2 5 5 5 6 7 8 22 1 sc://h/p?%%41%E3%80%80)"},
    {R"(\N)", R"(\E\Wsc://h/p?%%41\u3000)", R"(1 0 0 0 2 5 5 5 6 7 8 22 1 sc://h/p?%%41%E3%80%80)"},
    {R"(\N)", R"(\Esc://h/p?q=\uD83D\uDE00)", R"(1 0 0 0 2 5 5 5 6 7 8 23 1 sc://h/p?q=%F0%9F%98%80)"},
    {R"(\N)", R"(\E\Wsc://h/p?q=\uD83D\uDE00)", R"(1 0 0 0 2 5 5 5 6 7 8 23 1 sc://h/p?q=%F0%9F%98%80)"},
    {R"(\N)", R"(\Esc://h/p?x y)", R"(1 0 0 0 2 5 5 5 6 7 8 14 1 sc://h/p?x%20y)"},
    {R"(\N)", R"(\E\Wsc://h/p?x y)", R"(1 0 0 0 2 5 5 5 6 7 8 14 1 sc://h/p?x%20y)"},
    {R"(\N)", R"(\Efile:///x?)", R"(1 0 0 0 4 7 7 7 7 8 9 10 1 file:///x?)"},
    {R"(\N)", R"(\E\Wfile:///x?)", R"(1 0 0 0 4 7 7 7 7 8 9 10 0 file:///x?)"},
    {R"(\N)", R"(\Efile:///x?a=é)", R"(1 0 0 0 4 7 7 7 7 8 9 18 1 file:///x?a=%C3%A9)"},
    {R"(\N)", R"(\E\Wfile:///x?a=é)", R"(1 0 0 0 4 7 7 7 7 8 9 18 1 file:///x?a=%C3%A9)"},
    {R"(\N)", R"(\Efile:///x?\u20AC=x)", R"(1 0 0 0 4 7 7 7 7 8 9 13 1 file:///x??=x)"},
    {R"(\N)", R"(\E\Wfile:///x?\u20AC=x)", R"(1 0 0 0 4 7 7 7 7 8 9 13 1 file:///x??=x)"},
    {R"(\N)", R"(\Efile:///x?%%41\u3000)", R"(1 0 0 0 4 7 7 7 7 8 9 15 1 file:///x?%%41?)"},
    {R"(\N)", R"(\E\Wfile:///x?%%41\u3000)", R"(1 0 0 0 4 7 7 7 7 8 9 15 1 file:///x?%%41?)"},
    {R"(\N)", R"(\Efile:///x?q=\uD83D\uDE00)", R"(1 0 0 0 4 7 7 7 7 8 9 14 1 file:///x?q=??)"},
    {R"(\N)", R"(\E\Wfile:///x?q=\uD83D\uDE00)", R"(1 0 0 0 4 7 7 7 7 8 9 14 1 file:///x?q=??)"},
    {R"(\N)", R"(\Efile:///x?x y)", R"(1 0 0 0 4 7 7 7 7 8 9 15 1 file:///x?x%20y)"},
    {R"(\N)", R"(\E\Wfile:///x?x y)", R"(1 0 0 0 4 7 7 7 7 8 9 15 1 file:///x?x%20y)"},
    {R"(\N)", R"(\Efoo:bar?)", R"(1 0 1 0 3 4 4 4 4 4 7 8 1 foo:bar?)"},
    {R"(\N)", R"(\E\Wfoo:bar?)", R"(1 0 1 0 3 4 4 4 4 4 7 8 0 foo:bar?)"},
    {R"(\N)", R"(\Efoo:bar?a=é)", R"(1 0 1 0 3 4 4 4 4 4 7 22 1 foo:bar?a=%C3%83%C2%A9)"},
    {R"(\N)", R"(\E\Wfoo:bar?a=é)", R"(1 0 1 0 3 4 4 4 4 4 7 22 1 foo:bar?a=%C3%83%C2%A9)"},
    {R"(\N)", R"(\Efoo:bar?\u20AC=x)", R"(1 0 1 0 3 4 4 4 4 4 7 19 1 foo:bar?%E2%82%AC=x)"},
    {R"(\N)", R"(\E\Wfoo:bar?\u20AC=x)", R"(1 0 1 0 3 4 4 4 4 4 7 19 1 foo:bar?%E2%82%AC=x)"},
    {R"(\N)", R"(\Efoo:bar?%%41\u3000)", R"(1 0 1 0 3 4 4 4 4 4 7 21 1 foo:bar?%%41%E3%80%80)"},
    {R"(\N)", R"(\E\Wfoo:bar?%%41\u3000)", R"(1 0 1 0 3 4 4 4 4 4 7 21 1 foo:bar?%%41%E3%80%80)"},
    {R"(\N)", R"(\Efoo:bar?q=\uD83D\uDE00)", R"(1 0 1 0 3 4 4 4 4 4 7 22 1 foo:bar?q=%F0%9F%98%80)"},
    {R"(\N)", R"(\E\Wfoo:bar?q=\uD83D\uDE00)", R"(1 0 1 0 3 4 4 4 4 4 7 22 1 foo:bar?q=%F0%9F%98%80)"},
    {R"(\N)", R"(\Efoo:bar?x y)", R"(1 0 1 0 3 4 4 4 4 4 7 13 1 foo:bar?x%20y)"},
    {R"(\N)", R"(\E\Wfoo:bar?x y)", R"(1 0 1 0 3 4 4 4 4 4 7 13 1 foo:bar?x%20y)"},
    {R"(\N)", R"(\Ehttp://h/p#f?)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?)"},
    {R"(\N)", R"(\E\Whttp://h/p#f?)", R"(1 1 0 0 4 7 7 7 8 9 10 10 0 http://h/p#f?)"},
    {R"(\N)", R"(\Ehttp://h/p#f?a=é)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?a=%C3%83%C2%A9)"},
    {R"(\N)", R"(\E\Whttp://h/p#f?a=é)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?a=%C3%83%C2%A9)"},
    {R"(\N)", R"(\Ehttp://h/p#f?\u20AC=x)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?%E2%82%AC=x)"},
    {R"(\N)", R"(\E\Whttp://h/p#f?\u20AC=x)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?%E2%82%AC=x)"},
    {R"(\N)", R"(\Ehttp://h/p#f?%%41\u3000)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?%%41%E3%80%80)"},
    {R"(\N)", R"(\E\Whttp://h/p#f?%%41\u3000)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?%%41%E3%80%80)"},
    {R"(\N)", R"(\Ehttp://h/p#f?q=\uD83D\uDE00)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?q=%F0%9F%98%80)"},
    {R"(\N)", R"(\E\Whttp://h/p#f?q=\uD83D\uDE00)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?q=%F0%9F%98%80)"},
    {R"(\N)", R"(\Ehttp://h/p#f?x y)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?x%20y)"},
    {R"(\N)", R"(\E\Whttp://h/p#f?x y)", R"(1 1 0 0 4 7 7 7 8 9 10 10 1 http://h/p#f?x%20y)"},
};

std::u16string Units(const std::string& text) {
    std::u16string units;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 5 < text.size() && text[i + 1] == 'u') {
            units.push_back(static_cast<char16_t>(std::stoul(text.substr(i + 2, 4), nullptr, 16)));
            i += 5;
        } else
            units.push_back(static_cast<unsigned char>(text[i]));
    }
    return units;
}

String Decode(std::string text) {
    if (text == "\\N") return String();
    bool wide = text.rfind("\\W", 0) == 0;
    if (wide) text = text.substr(2);
    std::u16string units = Units(text);
    bool latin1 = !wide;
    for (char16_t unit : units) latin1 &= unit <= 0xFF;
    if (latin1) {
        std::string bytes(units.begin(), units.end());
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<unsigned>(bytes.size()));
    }
    return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
}

std::string Encode(const String& value) {
    if (!value.impl) return "\\N";
    std::string out;
    char buffer[8];
    const bool is8Bit = value.impl->hashAndFlags & 4;
    for (unsigned i = 0; i < value.impl->length; ++i) {
        const char16_t unit = is8Bit ? static_cast<const unsigned char*>(value.impl->data)[i] : static_cast<const char16_t*>(value.impl->data)[i];
        if (unit >= 0x20 && unit < 0x7F && unit != '\\') out.push_back(static_cast<char>(unit));
        else {
            std::snprintf(buffer, sizeof buffer, "\\u%04X", unit);
            out += buffer;
        }
    }
    return out;
}

std::string Describe(const URL& url) {
    const String string(url.impl ? (url.impl->refCount += 2, url.impl) : nullptr);
    char fields[160];
    const unsigned bits = url.bits;
    std::snprintf(fields, sizeof fields, "%u %u %u %u %u %u %u %u %u %u %u %u %d ", bits & 1, (bits >> 1) & 1, (bits >> 2) & 1, (bits >> 3) & 7, bits >> 6,
        url.userStart, url.userEnd, url.passwordEnd, url.hostEnd, url.pathAfterLastSlash, url.pathEnd, url.queryEnd,
        string.impl ? static_cast<int>(!!(string.impl->hashAndFlags & 4)) : -1);
    return fields + Encode(string);
}

struct EncodedBytes {
    unsigned char* buffer;
    unsigned capacity;
    unsigned size;
};

void APS5_VABI Latin1OrQuestionMark(EncodedBytes* result, const void*, NKWebKit::WtfAbi::StringView view) {
    result->buffer = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(view.length ? view.length : 1));
    result->capacity = view.length;
    result->size = view.length;
    for (unsigned i = 0; i < view.length; ++i) {
        const char16_t unit = view.is8Bit ? static_cast<const unsigned char*>(view.data)[i] : static_cast<const char16_t*>(view.data)[i];
        result->buffer[i] = unit <= 0xFF ? static_cast<unsigned char>(unit) : '?';
    }
}

const void* const EncodingVtable[] = {reinterpret_cast<const void*>(&Latin1OrQuestionMark), nullptr, nullptr};
const struct {
    const void* const* vtable;
} Encoding {EncodingVtable};

URL Parse(const URL& base, const String& input, const void* encoding = nullptr) {
    URL url;
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, encoding);
    return url;
}

void TestCases() {
    for (const Case& test : Cases) {
        const String baseText = Decode(test.base);
        const URL empty {};
        URL base {};
        if (baseText.impl) base = Parse(empty, baseText);
        std::string input = test.input;
        const bool encoded = input.rfind("\\E", 0) == 0;
        if (encoded) input = input.substr(2);
        const String relative = Decode(input);
        URL url = Parse(base, relative, encoded ? &Encoding : nullptr);
        const std::string actual = Describe(url);
        if (actual != test.expected) {
            std::fprintf(stderr, "base <%s> input <%s>\n  expected %s\n  actual   %s\n", test.base, test.input, test.expected, actual.c_str());
            std::abort();
        }
        String owner(url.impl);
        String baseOwner(base.impl);
    }
}

void TestReferences() {
    const URL empty {};
    const String canonical = Decode("https://example.com/a?b#c");
    const unsigned before = canonical.impl->refCount;
    {
        URL url = Parse(empty, canonical);
        Require(url.impl == canonical.impl);
        Require(canonical.impl->refCount == before + 2);
        String owner(url.impl);
    }
    Require(canonical.impl->refCount == before);

    const String changed = Decode("HTTPS://Example.com:443/./a");
    URL url = Parse(empty, changed);
    Require(url.impl != changed.impl);
    Require(Describe(url) == "1 1 0 0 5 8 8 8 19 20 21 21 1 https://example.com/a");
    String owner(url.impl);
    Require(changed.impl->refCount == 2);

    URL base = Parse(empty, Decode("http://h/p?q#f"));
    String baseOwner(base.impl);
    const unsigned baseReferences = base.impl->refCount;
    URL fromNull = Parse(base, String());
    Require(Describe(fromNull) == "1 1 0 0 4 7 7 7 8 9 10 12 1 http://h/p?q");
    String fromNullOwner(fromNull.impl);
    Require(base.impl->refCount == baseReferences);
}

void TestNullArguments() {
    const URL base {};
    const String input = Decode("http://h/");
    URL url {};
    RequireInvalidArgument([&] { _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(nullptr, &base, &input, nullptr); });
    RequireInvalidArgument([&] { _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, nullptr, &input, nullptr); });
    RequireInvalidArgument([&] { _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, nullptr, nullptr); });
    const struct {
        const void* const* vtable;
    } noVtable {nullptr};
    RequireInvalidArgument([&] { _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, &noVtable); });
}

}

int main() {
    TestCases();
    TestReferences();
    TestNullArguments();
    std::puts("URL parser tests passed");
    return 0;
}
