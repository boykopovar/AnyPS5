#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    Require(pageRef != nullptr);

    Require(!WKPageIsClosed(pageRef));
    Require(!page->isClosed);
    Require(!page->activeDOMObjectsAndAnimationsSuspended);

    WKPageSuspendActiveDOMObjectsAndAnimations(pageRef);
    Require(page->activeDOMObjectsAndAnimationsSuspended);

    WKPageResumeActiveDOMObjectsAndAnimations(pageRef);
    Require(!page->activeDOMObjectsAndAnimationsSuspended);

    WKPageClose(pageRef);
    Require(WKPageIsClosed(pageRef));
    Require(page->isClosed);

    const void* dummyString = WKStringCreateWithUTF8CString("not-a-page");

    RequireThrows([&] { WKPageClose(nullptr); });
    RequireThrows([&] { WKPageClose(dummyString); });

    RequireThrows([&] { WKPageSuspendActiveDOMObjectsAndAnimations(nullptr); });
    RequireThrows([&] { WKPageSuspendActiveDOMObjectsAndAnimations(dummyString); });

    RequireThrows([&] { WKPageResumeActiveDOMObjectsAndAnimations(nullptr); });
    RequireThrows([&] { WKPageResumeActiveDOMObjectsAndAnimations(dummyString); });

    WKRelease(dummyString);
    WKRelease(pageRef);
    return 0;
}
