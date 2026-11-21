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
        std::fprintf(stderr, "WKBundlePage MIME check failed at line %u\n", location.line());
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
    auto page = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* pageRef = page.Get();

    auto* htmlMime = WKStringCreateWithUTF8CString("text/html");
    Require(WKBundlePageCanShowMIMEType(pageRef, htmlMime));
    WKRelease(htmlMime);

    auto* pngMime = WKStringCreateWithUTF8CString("image/png");
    Require(WKBundlePageCanShowMIMEType(pageRef, pngMime));
    WKRelease(pngMime);

    auto* unsupportedMime = WKStringCreateWithUTF8CString("text/rtf");
    Require(!WKBundlePageCanShowMIMEType(pageRef, unsupportedMime));
    WKRelease(unsupportedMime);

    auto* emptyMime = WKStringCreateWithUTF8CString("");
    Require(!WKBundlePageCanShowMIMEType(pageRef, emptyMime));
    WKRelease(emptyMime);

    auto* nullMime = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    Require(!WKBundlePageCanShowMIMEType(pageRef, nullMime));
    WKRelease(nullMime);

    auto* testMime = WKStringCreateWithUTF8CString("text/html");
    RequireInvalidArgument([&] { WKBundlePageCanShowMIMEType(nullptr, testMime); });
    RequireInvalidArgument([&] { WKBundlePageCanShowMIMEType(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageCanShowMIMEType(pageRef, pageRef); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKBundlePageCanShowMIMEType(frame.Get(), testMime); });
    WKRelease(testMime);

    return 0;
}
