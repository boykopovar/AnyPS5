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
        std::fprintf(stderr, "BundleFrameAttributes test failed at line %u\n", location.line());
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
    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    const void* frameRef = frame.Get();
    auto bundle = NKWebKit::Make<NKWebKit::Bundle>();
    const void* bundleRef = bundle.Get();
    const void* url = WKURLCreateWithUTF8CString("https://example.com");
    const void* name = WKStringCreateWithUTF8CString("MainFrameName");
    const void* dir = WKStringCreateWithUTF8CString("ltr");

    RequireInvalidArgument([&] {
        WKBundleFrameAllowsFollowingLink(nullptr, url);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameAllowsFollowingLink(bundleRef, url);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameAllowsFollowingLink(frameRef, nullptr);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameAllowsFollowingLink(frameRef, name);
    });

    Require(WKBundleFrameAllowsFollowingLink(frameRef, url));
    frame->allowsFollowingLink = false;
    Require(!WKBundleFrameAllowsFollowingLink(frameRef, url));

    RequireInvalidArgument([] {
        WKBundleFrameCallShouldCloseOnWebView(nullptr);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameCallShouldCloseOnWebView(bundleRef);
    });

    Require(WKBundleFrameCallShouldCloseOnWebView(frameRef));
    frame->shouldCloseOnWebView = false;
    Require(!WKBundleFrameCallShouldCloseOnWebView(frameRef));

    RequireInvalidArgument([&] {
        WKBundleFrameSetAccessibleName(nullptr, name);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameSetAccessibleName(bundleRef, name);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameSetAccessibleName(frameRef, nullptr);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameSetAccessibleName(frameRef, url);
    });

    Require(!frame->accessibleName.has_value());
    WKBundleFrameSetAccessibleName(frameRef, name);
    Require(frame->accessibleName.has_value());
    Require(frame->accessibleName.value() == u"MainFrameName");

    RequireInvalidArgument([&] {
        WKBundleFrameSetTextDirection(nullptr, dir);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameSetTextDirection(bundleRef, dir);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameSetTextDirection(frameRef, nullptr);
    });
    RequireInvalidArgument([&] {
        WKBundleFrameSetTextDirection(frameRef, url);
    });

    Require(!frame->textDirection.has_value());
    WKBundleFrameSetTextDirection(frameRef, dir);
    Require(frame->textDirection.has_value());
    Require(frame->textDirection.value() == u"ltr");

    WKRelease(dir);
    WKRelease(name);
    WKRelease(url);
    return 0;
}
