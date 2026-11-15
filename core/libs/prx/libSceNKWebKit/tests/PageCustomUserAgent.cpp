#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage custom user agent controls check failed at line %u\n", location.line());
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

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    {
        auto* empty = WKPageCopyCustomUserAgent(pageRef);
        Require(empty != nullptr && WKStringGetLength(empty) == 0);
        WKRelease(empty);
    }

    auto testStr = NKWebKit::String::FromUtf8("TestCustomUA/1.0");
    const void* testStrRef = NKWebKit::ToRef(testStr);

    WKPageSetCustomUserAgent(pageRef, testStrRef);
    auto* copied = WKPageCopyCustomUserAgent(pageRef);
    Require(copied != nullptr);
    Require(WKStringIsEqualToUTF8CString(copied, "TestCustomUA/1.0"));
    WKRelease(copied);

    WKPageSetCustomUserAgent(pageRef, nullptr);
    {
        auto* empty = WKPageCopyCustomUserAgent(pageRef);
        Require(empty != nullptr && WKStringGetLength(empty) == 0);
        WKRelease(empty);
    }

    RequireInvalidArgument([] { WKPageCopyCustomUserAgent(nullptr); });
    RequireInvalidArgument([] { WKPageSetCustomUserAgent(nullptr, nullptr); });

    WKRelease(testStrRef);
    WKRelease(pageRef);
    return 0;
}
