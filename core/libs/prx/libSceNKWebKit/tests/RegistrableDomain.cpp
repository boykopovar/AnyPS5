#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKRegistrableDomainCreate check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

template<class Function>
void RequireRuntimeError(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void CheckDomain(const char* urlString, const char* expectedDomain) {
    const void* url = WKURLCreateWithUTF8CString(urlString);
    Require(url != nullptr);
    const void* domain = WKRegistrableDomainCreate(url);
    Require(domain != nullptr);
    Require(WKGetTypeID(domain) == WKStringGetTypeID());
    Require(WKStringIsEqualToUTF8CString(domain, expectedDomain));
    WKRelease(domain);
    WKRelease(url);
}

}

int main() {
    CheckDomain("https://www.example.com/path", "example.com");
    CheckDomain("http://sub.domain.example.co.uk:8080/test", "example.co.uk");
    CheckDomain("https://example.com/", "example.com");
    CheckDomain("http://localhost:3000", "localhost");
    CheckDomain("http://192.168.1.1:80/", "192.168.1.1");
    CheckDomain("about:blank", "nullOrigin");
    CheckDomain("file:///path/to/file", "nullOrigin");
    CheckDomain("https://com/", "com");
    CheckDomain("HTTP://SUB.EXAMPLE.COM/page", "example.com");
    CheckDomain("https://www.xn--mnchen-3ya.de/", "xn--mnchen-3ya.de");
    CheckDomain("https://www.ex%41mple.com/", "example.com");
    CheckDomain("http://0x7f.1/", "127.0.0.1");

    RequireRuntimeError([] {
        const void* url = WKURLCreateWithUTF8CString("https://m\xC3\xBCnchen.de/path");
        Require(url != nullptr);
        try {
            WKRegistrableDomainCreate(url);
            WKRelease(url);
        } catch (...) {
            WKRelease(url);
            throw;
        }
    });
    RequireRuntimeError([] {
        const void* url = WKURLCreateWithUTF8CString("https://\xD0\xBF\xD1\x80\xD0\xB8\xD0\xBC\xD0\xB5\xD1\x80.\xD0\xB8\xD1\x81\xD0\xBF\xD1\x8B\xD1\x82\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5/path");
        Require(url != nullptr);
        try {
            WKRegistrableDomainCreate(url);
            WKRelease(url);
        } catch (...) {
            WKRelease(url);
            throw;
        }
    });

    RequireInvalidArgument([] { WKRegistrableDomainCreate(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKRegistrableDomainCreate(frame.Get()); });

    return 0;
}
