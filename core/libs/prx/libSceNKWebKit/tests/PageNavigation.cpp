#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/BackForwardList.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage navigation check failed at line %u\n", location.line());
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
    auto item = NKWebKit::Make<NKWebKit::BackForwardListItem>(
        std::string("https://example.com"),
        NKWebKit::Make<NKWebKit::String>(std::u16string(u"Example")),
        std::string("https://example.com/orig"));
    const void* itemRef = NKWebKit::ToRef(item);
    auto frame = NKWebKit::Make<NKWebKit::Frame>();

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

    Require(!page->navigatedBack);
    WKPageGoBack(pageRef);
    Require(page->navigatedBack);

    Require(!page->navigatedForward);
    WKPageGoForward(pageRef);
    Require(page->navigatedForward);

    Require(page->currentBackForwardListItem.Get() == nullptr);
    WKPageGoToBackForwardListItem(pageRef, itemRef);
    Require(page->currentBackForwardListItem.Get() == item.Get());

    page->isClosed = true;
    page->navigatedBack = false;
    page->navigatedForward = false;
    page->currentBackForwardListItem = nullptr;

    WKPageGoBack(pageRef);
    Require(!page->navigatedBack);

    WKPageGoForward(pageRef);
    Require(!page->navigatedForward);

    WKPageGoToBackForwardListItem(pageRef, itemRef);
    Require(page->currentBackForwardListItem.Get() == nullptr);

    page->isClosed = false;

    RequireInvalidArgument([] { WKPageCanGoBack(nullptr); });
    RequireInvalidArgument([] { WKPageCanGoForward(nullptr); });
    RequireInvalidArgument([] { WKPageIsPinnedToLeftSide(nullptr); });
    RequireInvalidArgument([] { WKPageIsPinnedToRightSide(nullptr); });
    RequireInvalidArgument([] { WKPageIsPinnedToTopSide(nullptr); });
    RequireInvalidArgument([] { WKPageIsPinnedToBottomSide(nullptr); });

    RequireInvalidArgument([] { WKPageGoBack(nullptr); });
    RequireInvalidArgument([&] { WKPageGoBack(frame.Get()); });

    RequireInvalidArgument([] { WKPageGoForward(nullptr); });
    RequireInvalidArgument([&] { WKPageGoForward(frame.Get()); });

    RequireInvalidArgument([] { WKPageGoToBackForwardListItem(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGoToBackForwardListItem(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKPageGoToBackForwardListItem(nullptr, itemRef); });
    RequireInvalidArgument([&] { WKPageGoToBackForwardListItem(frame.Get(), itemRef); });
    RequireInvalidArgument([&] { WKPageGoToBackForwardListItem(pageRef, frame.Get()); });

    WKRelease(itemRef);
    WKRelease(pageRef);
    return 0;
}
