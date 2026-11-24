#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Printing test failed at line %u\n", location.line());
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

struct ComputePagesResult {
    bool invoked = false;
    std::uint32_t pageCount = 0;
    double scaleFactor = 0.0;
    NKWebKit::RectValue rect{};
    void* context = nullptr;
};

void APS5_VABI TestComputePagesCallback(NKWebKit::RectValue* pageRects, std::uint32_t pageCount, double resultPageScaleFactor, const void* error, void* functionContext) {
    auto* r = static_cast<ComputePagesResult*>(functionContext);
    r->invoked = true;
    r->pageCount = pageCount;
    r->scaleFactor = resultPageScaleFactor;
    if (pageRects && pageCount > 0) {
        r->rect = pageRects[0];
    }
    r->context = functionContext;
    (void)error;
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();
    auto frame = NKWebKit::Make<NKWebKit::Frame>();
    const void* frameRef = frame.Get();
    auto bundlePage = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* bundlePageRef = bundlePage.Get();

    WKPrintInfo printInfo{1.5f, 210.0f, 297.0f};

    RequireInvalidArgument([&] {
        WKPageBeginPrinting(nullptr, frameRef, printInfo);
    });
    RequireInvalidArgument([&] {
        WKPageBeginPrinting(bundlePageRef, frameRef, printInfo);
    });
    RequireInvalidArgument([&] {
        WKPageBeginPrinting(pageRef, bundlePageRef, printInfo);
    });

    RequireInvalidArgument([&] {
        WKPageBeginPrinting(pageRef, nullptr, printInfo);
    });

    Require(!page->isInPrintingMode);
    WKPageBeginPrinting(pageRef, frameRef, printInfo);
    Require(page->isInPrintingMode);
    Require(page->printInfo.pageSetupScaleFactor == 1.5f);
    Require(page->printInfo.availablePaperWidth == 210.0f);
    Require(page->printInfo.availablePaperHeight == 297.0f);

    WKPrintInfo printInfo2{2.0f, 100.0f, 100.0f};
    WKPageBeginPrinting(pageRef, frameRef, printInfo2);
    Require(page->printInfo.pageSetupScaleFactor == 1.5f);

    RequireInvalidArgument([] {
        WKPageEndPrinting(nullptr);
    });
    RequireInvalidArgument([&] {
        WKPageEndPrinting(bundlePageRef);
    });

    WKPageEndPrinting(pageRef);
    Require(!page->isInPrintingMode);

    WKPageEndPrinting(pageRef);
    Require(!page->isInPrintingMode);

    auto closedPage = NKWebKit::Make<NKWebKit::Page>();
    const void* closedPageRef = closedPage.Get();
    closedPage->isClosed = true;
    WKPageBeginPrinting(closedPageRef, frameRef, printInfo);
    Require(!closedPage->isInPrintingMode);
    WKPageEndPrinting(closedPageRef);
    Require(!closedPage->isInPrintingMode);

    RequireInvalidArgument([&] {
        WKPageComputePagesForPrinting(nullptr, frameRef, printInfo, TestComputePagesCallback, nullptr);
    });
    RequireInvalidArgument([&] {
        WKPageComputePagesForPrinting(bundlePageRef, frameRef, printInfo, TestComputePagesCallback, nullptr);
    });
    RequireInvalidArgument([&] {
        WKPageComputePagesForPrinting(pageRef, bundlePageRef, printInfo, TestComputePagesCallback, nullptr);
    });

    RequireInvalidArgument([&] {
        WKPageComputePagesForPrinting(pageRef, nullptr, printInfo, TestComputePagesCallback, nullptr);
    });

    WKPageComputePagesForPrinting(pageRef, frameRef, printInfo, nullptr, nullptr);

    ComputePagesResult res1{};
    WKPageComputePagesForPrinting(pageRef, frameRef, printInfo, TestComputePagesCallback, &res1);
    Require(res1.invoked);
    Require(res1.pageCount == 1);
    Require(res1.scaleFactor == 1.5);
    Require(res1.rect.origin.x == 0.0);
    Require(res1.rect.origin.y == 0.0);
    Require(res1.rect.size.width == 210.0);
    Require(res1.rect.size.height == 297.0);
    Require(page->isInPrintingMode);

    ComputePagesResult resClosed{};
    WKPageComputePagesForPrinting(closedPageRef, frameRef, printInfo, TestComputePagesCallback, &resClosed);
    Require(resClosed.invoked);
    Require(resClosed.pageCount == 0);

    RequireInvalidArgument([] {
        WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(nullptr);
    });
    RequireInvalidArgument([&] {
        WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(pageRef);
    });

    const void* emptyRep = WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(bundlePageRef);
    Require(emptyRep != nullptr);
    Require(WKStringGetLength(emptyRep) == 0);
    WKRelease(emptyRep);

    bundlePage->renderTreeExternalRepresentationForPrinting = u"Mock Render Tree";
    const void* rep = WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(bundlePageRef);
    Require(rep != nullptr);
    Require(WKStringIsEqualToUTF8CString(rep, "Mock Render Tree"));
    WKRelease(rep);

    return 0;
}
