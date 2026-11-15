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
        std::fprintf(stderr, "WKPage application name user agent controls check failed at line %u\n", location.line());
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
    auto* unset = WKPageCopyApplicationNameForUserAgent(pageRef);
    Require(unset != nullptr && WKStringGetLength(unset) == 0);
    WKRelease(unset);

    auto testStr = NKWebKit::String::FromUtf8("TestApp/2.0");
    const void* testStrRef = NKWebKit::ToRef(testStr);

    WKPageSetApplicationNameForUserAgent(pageRef, testStrRef);
    auto* copied = WKPageCopyApplicationNameForUserAgent(pageRef);
    Require(copied != nullptr);
    Require(WKStringIsEqualToUTF8CString(copied, "TestApp/2.0"));
    WKRelease(copied);

    WKPageSetApplicationNameForUserAgent(pageRef, nullptr);
    auto* cleared = WKPageCopyApplicationNameForUserAgent(pageRef);
    Require(cleared != nullptr && WKStringGetLength(cleared) == 0);
    WKRelease(cleared);

    RequireInvalidArgument([] { WKPageCopyApplicationNameForUserAgent(nullptr); });
    RequireInvalidArgument([] { WKPageSetApplicationNameForUserAgent(nullptr, nullptr); });

    WKRelease(testStrRef);
    WKRelease(pageRef);
    return 0;
}
