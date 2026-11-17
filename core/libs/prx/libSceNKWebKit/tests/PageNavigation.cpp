#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>


int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);
    Require(!WKPageCanGoBack(pageRef));
    Require(!WKPageCanGoForward(pageRef));
    Require(WKPageIsPinnedToLeftSide(pageRef));
    Require(WKPageIsPinnedToRightSide(pageRef));
    Require(WKPageIsPinnedToTopSide(pageRef));
    Require(WKPageIsPinnedToBottomSide(pageRef));

    page->canGoBack = true;
    page->canGoForward = true;
    page->isPinnedToLeftSide = false;
    page->isPinnedToRightSide = false;
    page->isPinnedToTopSide = false;
    page->isPinnedToBottomSide = false;
    Require(WKPageCanGoBack(pageRef));
    Require(WKPageCanGoForward(pageRef));
    Require(!WKPageIsPinnedToLeftSide(pageRef));
    Require(!WKPageIsPinnedToRightSide(pageRef));
    Require(!WKPageIsPinnedToTopSide(pageRef));
    Require(!WKPageIsPinnedToBottomSide(pageRef));

    ExpectErr<std::invalid_argument>([] { WKPageCanGoBack(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageCanGoForward(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageIsPinnedToLeftSide(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageIsPinnedToRightSide(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageIsPinnedToTopSide(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageIsPinnedToBottomSide(nullptr); });

    WKRelease(pageRef);
    return 0;
}
