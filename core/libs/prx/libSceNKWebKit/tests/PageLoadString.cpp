#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

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
    auto pageObj = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = pageObj.Get();
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);

    Require(!page->loadedHTMLString.has_value());
    Require(!page->loadedPlainTextString.has_value());
    Require(!page->loadedBaseURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);
    Require(page->loadUserData == nullptr);

    const void* htmlString = WKStringCreateWithUTF8CString("<h1>Hello</h1>");
    const void* plainTextString = WKStringCreateWithUTF8CString("Plain text content");
    const void* baseURL = WKURLCreateWithUTF8CString("https://example.com/base/");

    auto notPage = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* notPageRef = notPage.Get();

    RequireThrows([&] {
        WKPageLoadHTMLString(nullptr, htmlString, baseURL);
    });
    RequireThrows([&] {
        WKPageLoadHTMLString(notPageRef, htmlString, baseURL);
    });
    RequireThrows([&] {
        WKPageLoadHTMLString(pageRef, nullptr, baseURL);
    });
    RequireThrows([&] {
        WKPageLoadHTMLString(pageRef, pageRef, baseURL);
    });
    RequireThrows([&] {
        WKPageLoadHTMLString(pageRef, htmlString, pageRef);
    });

    RequireThrows([&] {
        WKPageLoadHTMLStringWithUserData(nullptr, htmlString, baseURL, nullptr);
    });
    RequireThrows([&] {
        WKPageLoadHTMLStringWithUserData(notPageRef, htmlString, baseURL, nullptr);
    });
    RequireThrows([&] {
        WKPageLoadHTMLStringWithUserData(pageRef, nullptr, baseURL, nullptr);
    });

    RequireThrows([&] {
        WKPageLoadPlainTextString(nullptr, plainTextString);
    });
    RequireThrows([&] {
        WKPageLoadPlainTextString(notPageRef, plainTextString);
    });
    RequireThrows([&] {
        WKPageLoadPlainTextString(pageRef, nullptr);
    });

    RequireThrows([&] {
        WKPageLoadPlainTextStringWithUserData(nullptr, plainTextString, nullptr);
    });
    RequireThrows([&] {
        WKPageLoadPlainTextStringWithUserData(notPageRef, plainTextString, nullptr);
    });
    RequireThrows([&] {
        WKPageLoadPlainTextStringWithUserData(pageRef, nullptr, nullptr);
    });

    WKPageLoadHTMLString(pageRef, htmlString, baseURL);
    Require(page->loadedHTMLString.has_value());
    Require(page->loadedBaseURL.Get() != nullptr);
    Require(page->loadedBaseURL->text == "https://example.com/base/");
    Require(page->pendingAPIRequestURL.Get() == page->loadedBaseURL.Get());
    Require(page->provisionalURL.Get() == page->loadedBaseURL.Get());
    Require(page->activeURL.Get() == page->provisionalURL.Get());
    Require(page->loadUserData == nullptr);
    const void* pendingURL1 = WKPageCopyPendingAPIRequestURL(pageRef);
    Require(pendingURL1 != nullptr);
    WKRelease(pendingURL1);

    WKPageLoadHTMLString(pageRef, htmlString, nullptr);
    Require(page->loadedHTMLString.has_value());
    Require(page->loadedBaseURL.Get() != nullptr);
    Require(page->loadedBaseURL->text == "about:blank");
    Require(page->pendingAPIRequestURL.Get() == page->loadedBaseURL.Get());
    Require(page->provisionalURL.Get() == page->loadedBaseURL.Get());
    Require(page->activeURL.Get() == page->provisionalURL.Get());
    Require(page->loadUserData == nullptr);
    const void* pendingURL2 = WKPageCopyPendingAPIRequestURL(pageRef);
    Require(pendingURL2 != nullptr);
    WKRelease(pendingURL2);

    const void* emptyBaseURL = WKURLCreateWithUTF8CString("");
    WKPageLoadHTMLString(pageRef, htmlString, emptyBaseURL);
    Require(page->loadedHTMLString.has_value());
    Require(page->loadedBaseURL.Get() != nullptr);
    Require(page->loadedBaseURL->text == "about:blank");
    Require(page->pendingAPIRequestURL.Get() == page->loadedBaseURL.Get());
    Require(page->provisionalURL.Get() == page->loadedBaseURL.Get());
    Require(page->activeURL.Get() == page->provisionalURL.Get());
    const void* pendingURL3 = WKPageCopyPendingAPIRequestURL(pageRef);
    Require(pendingURL3 != nullptr);
    WKRelease(pendingURL3);
    WKRelease(emptyBaseURL);

    int testUserData = 42;
    WKPageLoadHTMLStringWithUserData(pageRef, htmlString, baseURL, &testUserData);
    Require(page->loadedHTMLString.has_value());
    Require(page->loadedBaseURL.Get() != nullptr);
    Require(page->loadedBaseURL->text == "https://example.com/base/");
    Require(page->pendingAPIRequestURL.Get() == page->loadedBaseURL.Get());
    Require(page->loadUserData == &testUserData);

    WKPageLoadPlainTextString(pageRef, plainTextString);
    Require(page->loadedPlainTextString.has_value());
    Require(page->loadedBaseURL.Get() != nullptr);
    Require(page->loadedBaseURL->text == "about:blank");
    Require(page->pendingAPIRequestURL.Get() == page->loadedBaseURL.Get());
    Require(page->provisionalURL.Get() == page->loadedBaseURL.Get());
    Require(page->activeURL.Get() == page->provisionalURL.Get());
    Require(page->loadUserData == nullptr);
    const void* pendingURL4 = WKPageCopyPendingAPIRequestURL(pageRef);
    Require(pendingURL4 != nullptr);
    WKRelease(pendingURL4);

    int testPlainUserData = 100;
    WKPageLoadPlainTextStringWithUserData(pageRef, plainTextString, &testPlainUserData);
    Require(page->loadedPlainTextString.has_value());
    Require(page->loadedBaseURL.Get() != nullptr);
    Require(page->loadedBaseURL->text == "about:blank");
    Require(page->pendingAPIRequestURL.Get() == page->loadedBaseURL.Get());
    Require(page->loadUserData == &testPlainUserData);

    auto closedPageObj = NKWebKit::Make<NKWebKit::Page>();
    const void* closedPageRef = closedPageObj.Get();
    auto* closedPage = NKWebKit::Cast<NKWebKit::Page>(closedPageRef, __func__);
    closedPage->isClosed = true;

    WKPageLoadHTMLString(closedPageRef, htmlString, baseURL);
    Require(!closedPage->loadedHTMLString.has_value());
    Require(!closedPage->loadedBaseURL);

    WKPageLoadHTMLStringWithUserData(closedPageRef, htmlString, baseURL, &testUserData);
    Require(!closedPage->loadedHTMLString.has_value());
    Require(closedPage->loadUserData == nullptr);

    WKPageLoadPlainTextString(closedPageRef, plainTextString);
    Require(!closedPage->loadedPlainTextString.has_value());
    Require(!closedPage->loadedBaseURL);

    WKPageLoadPlainTextStringWithUserData(closedPageRef, plainTextString, &testPlainUserData);
    Require(!closedPage->loadedPlainTextString.has_value());
    Require(closedPage->loadUserData == nullptr);

    WKRelease(htmlString);
    WKRelease(plainTextString);
    WKRelease(baseURL);

    return 0;
}
