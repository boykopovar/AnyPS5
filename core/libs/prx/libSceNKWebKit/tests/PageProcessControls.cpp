#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage process controls check failed at line %u\n", location.line());
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
    const void* pageRef = page.Get();
    auto otherPage = NKWebKit::Make<NKWebKit::Page>();
    const void* otherPageRef = otherPage.Get();
    auto closedRelatedPage = NKWebKit::Make<NKWebKit::Page>();
    closedRelatedPage->isClosed = true;
    auto frame = NKWebKit::Make<NKWebKit::Frame>();

    Require(WKPageGetProcessIdentifier(pageRef) == 1000);
    Require(!page->isTerminated);
    Require(!page->isClosed);

    {
        const void* relatedArray = WKPageCopyRelatedPages(pageRef);
        Require(relatedArray != nullptr);
        Require(WKArrayGetSize(relatedArray) == 0);
        WKRelease(relatedArray);
    }

    page->relatedPages.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(page.Get()));
    page->relatedPages.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(otherPage.Get()));
    page->relatedPages.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(closedRelatedPage.Get()));

    {
        const void* relatedArray = WKPageCopyRelatedPages(pageRef);
        Require(relatedArray != nullptr);
        Require(WKArrayGetSize(relatedArray) == 1);
        WKRelease(relatedArray);
    }

    Require(page->layoutMilestones == 0);
    WKPageListenForLayoutMilestones(pageRef, 0x1F);
    Require(page->layoutMilestones == 0x1F);

    Require(page->mockCameraOrientation == 0);
    WKPageSetMockCameraOrientation(pageRef, 180);
    Require(page->mockCameraOrientation == 180);

    const void* policies = WKWebsitePoliciesCreate();
    Require(policies != nullptr);
    Require(page->websitePolicies.Get() == nullptr);
    WKPageUpdateWebsitePolicies(pageRef, policies);
    Require(page->websitePolicies.Get() != nullptr);
    Require(page->websitePolicies.Get() == NKWebKit::ToObject(policies, "test"));

    WKPageTerminate(pageRef);
    Require(page->isTerminated);
    Require(!page->isClosed);
    Require(!WKPageIsClosed(pageRef));
    Require(WKPageGetProcessIdentifier(pageRef) == 0);

    WKPageTerminate(pageRef);
    Require(page->isTerminated);
    Require(!page->isClosed);

    {
        const void* relatedArray = WKPageCopyRelatedPages(pageRef);
        Require(relatedArray != nullptr);
        Require(WKArrayGetSize(relatedArray) == 0);
        WKRelease(relatedArray);
    }

    WKPageListenForLayoutMilestones(pageRef, 0x2A);
    Require(page->layoutMilestones == 0x1F);

    WKPageSetMockCameraOrientation(pageRef, 270);
    Require(page->mockCameraOrientation == 180);

    const void* otherPolicies = WKWebsitePoliciesCreate();
    WKPageUpdateWebsitePolicies(pageRef, otherPolicies);
    Require(page->websitePolicies.Get() == NKWebKit::ToObject(policies, "test"));
    WKRelease(otherPolicies);
    WKRelease(policies);

    auto ordinaryClosedPage = NKWebKit::Make<NKWebKit::Page>();
    const void* ordinaryClosedPageRef = ordinaryClosedPage.Get();
    ordinaryClosedPage->isClosed = true;
    Require(ordinaryClosedPage->isClosed);
    Require(!ordinaryClosedPage->isTerminated);
    Require(WKPageIsClosed(ordinaryClosedPageRef));

    RequireInvalidArgument([] { WKPageTerminate(nullptr); });
    RequireInvalidArgument([&] { WKPageTerminate(frame.Get()); });

    RequireInvalidArgument([] { WKPageGetProcessIdentifier(nullptr); });
    RequireInvalidArgument([&] { WKPageGetProcessIdentifier(frame.Get()); });

    RequireInvalidArgument([] { WKPageCopyRelatedPages(nullptr); });
    RequireInvalidArgument([&] { WKPageCopyRelatedPages(frame.Get()); });

    RequireInvalidArgument([] { WKPageListenForLayoutMilestones(nullptr, 1); });
    RequireInvalidArgument([&] { WKPageListenForLayoutMilestones(frame.Get(), 1); });

    RequireInvalidArgument([] { WKPageSetMockCameraOrientation(nullptr, 90); });
    RequireInvalidArgument([&] { WKPageSetMockCameraOrientation(frame.Get(), 90); });

    RequireInvalidArgument([] { WKPageUpdateWebsitePolicies(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageUpdateWebsitePolicies(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKPageUpdateWebsitePolicies(nullptr, policies); });
    RequireInvalidArgument([&] { WKPageUpdateWebsitePolicies(frame.Get(), policies); });
    RequireInvalidArgument([&] { WKPageUpdateWebsitePolicies(pageRef, frame.Get()); });

    return 0;
}
