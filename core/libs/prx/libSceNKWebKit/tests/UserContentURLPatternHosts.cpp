#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <cstdio>

int main() {
    struct Case {
        const char* pattern;
        const char* url;
        bool expected;
    };
    const Case cases[] = {
        {"file://server-a/path/*", "file://server-a/path/x", true},
        {"file://server-a/path/*", "file://server-b/path/x", false},
        {"file://server-a/path/*", "file:///path/x", false},
        {"file://server-a/path/*", "file://localhost/path/x", false},
        {"file://server-a/path/*", "FILE://SeRvEr-A/path/x", true},
        {"FiLe://SERVER-A/path/*", "file://server-a/path/x", true},
        {"file:///path/*", "file:///path/x", true},
        {"file:///path/*", "file://localhost/path/x", true},
        {"file:///path/*", "FILE://LoCaLhOsT/path/x", true},
        {"file:///path/*", "file://server-a/path/x", false},
        {"file://localhost/path/*", "file:///path/x", true},
        {"file://LoCaLhOsT/path/*", "file://LOCALHOST/path/x", true},
        {"file://localhost/path/*", "file://server-a/path/x", false},
        {"file://*/path/*", "file://server-a/path/x", true},
        {"file://*/path/*", "file:///path/x", true},
        {"file://*/path/*", "file://LOCALHOST/path/x", true},
        {"file://*.example.com/path/*", "file://example.com/path/x", true},
        {"file://*.example.com/path/*", "file://sub.example.com/path/x", true},
        {"file://*.example.com/path/*", "file://a.b.EXAMPLE.COM/path/x", true},
        {"file://*.example.com/path/*", "file://.example.com/path/x", true},
        {"file://*.example.com/path/*", "file://other.com/path/x", false},
        {"file://*.example.com/path/*", "file://sub.example.org/path/x", false},
        {"file://*.example.com/path/*", "file://exbmple.com/path/x", false},
        {"file://*.example.com/path/*", "file://badexample.com/path/x", false},
        {"file://*.example.com/path/*", "file://example.com.other/path/x", false},
        {"file://*.example.com/path/*", "file:///path/x", false},
        {"file://*.localhost/path/*", "file://LOCALHOST/path/x", false},
        {"file://*.localhost/path/*", "file://sub.localhost/path/x", true},
        {"http://server-a/path/*", "http://server-a/path/x", true},
        {"http://server-a/path/*", "http://server-b/path/x", false},
        {"http://server-a/path/*", "https://server-a/path/x", false},
        {"http://server-a/path/*", "HTTP://SERVER-A/path/x", true},
        {"http://localhost/path/*", "http://localhost/path/x", true},
        {"http://*/path/*", "http://server-a/path/x", true},
        {"http://*.example.com/path/*", "http://example.com/path/x", true},
        {"http://*.example.com/path/*", "http://sub.example.com/path/x", true},
        {"http://*.example.com/path/*", "http://.example.com/path/x", true},
        {"http://*.example.com/path/*", "http://badexample.com/path/x", false},
        {"http://*.example.com/path/*", "http://sub.example.org/path/x", false},
        {"*://server-a/path/*", "http://server-a/path/x", true},
        {"*://server-a/path/*", "https://server-a/path/x", true},
        {"*://server-a/path/*", "file://server-a/path/x", false},
        {"file://server-a/path/*", "file://server-a/path/x?query#fragment", true},
        {"file://server-a/path/*", "file://server-b/path/x?query#fragment", false},
        {"file://server-a/path/*", "file://server-a/other/x", false},
        {"file://*/path/*", "file://server-a/other/x", false},
        {"file:///path/*", "file:///other/x", false},
        {"http://server-a/path/*", "http://server-a/other/x", false},
    };
    int failures = 0;
    for (const auto& test : cases) {
        const auto* string = WKStringCreateWithUTF8CString(test.pattern);
        const auto* pattern = WKUserContentURLPatternCreate(string);
        const auto* url = WKURLCreateWithUTF8CString(test.url);
        const bool valid = WKUserContentURLPatternIsValid(pattern);
        const bool actual = WKUserContentURLPatternMatchesURL(pattern, url);
        if (!valid || actual != test.expected) {
            std::fprintf(stderr, "%s against %s: valid=%d actual=%d expected=%d\n",
                         test.pattern, test.url, valid, actual, test.expected);
            ++failures;
        }
        WKRelease(url);
        WKRelease(pattern);
        WKRelease(string);
    }
    std::printf("%zu host cases, %d failures\n", sizeof(cases) / sizeof(cases[0]), failures);
    return failures ? 1 : 0;
}
