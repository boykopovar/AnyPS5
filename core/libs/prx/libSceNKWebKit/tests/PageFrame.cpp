#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/FrameHandle.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage frame check failed at line %u\n", location.line());
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

static void APS5_VABI SourceCallback(const void* str, const void* error, void* ctx) {
    Require(str != nullptr);
    Require(error == nullptr);
    Require(WKGetTypeID(str) == WKStringGetTypeID());
    Require(WKStringGetLength(str) == 39);
    *static_cast<bool*>(ctx) = true;
}

static void APS5_VABI ResourceCallback(const void* data, const void* error, void* ctx) {
    Require(data != nullptr);
    Require(error == nullptr);
    Require(WKGetTypeID(data) == WKDataGetTypeID());
    Require(WKDataGetSize(data) == 4);
    *static_cast<bool*>(ctx) = true;
}

static void APS5_VABI NullCallback(const void* val, const void* error, void* ctx) {
    Require(val == nullptr);
    Require(error == nullptr);
    *static_cast<bool*>(ctx) = true;
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();

    Require(!page->frames.empty());
    auto mainFrame = page->frames.front();
    Require(mainFrame->isMainFrame);
    const void* mainHandle = WKFrameCreateFrameHandle(mainFrame.Get());
    Require(WKPageLookUpFrameFromHandle(pageRef, mainHandle) == mainFrame.Get());
    WKRelease(mainHandle);

    auto frame1 = NKWebKit::Make<NKWebKit::Frame>();
    frame1->frameID = 101;
    frame1->source = u"<html><body>Hello Frame 1</body></html>";
    frame1->resourceData["https://example.com/asset.png"] = {0x01, 0x02, 0x03, 0x04};
    const void* frame1Ref = frame1.Get();

    auto frame2 = NKWebKit::Make<NKWebKit::Frame>();
    frame2->frameID = 102;
    frame2->source = u"<html><body>Frame 2</body></html>";
    const void* frame2Ref = frame2.Get();

    page->frames.push_back(frame1);
    page->frames.push_back(frame2);

    auto handle1 = NKWebKit::Make<NKWebKit::FrameHandle>(101);
    auto handle2 = NKWebKit::Make<NKWebKit::FrameHandle>(102);
    auto handleUnknown = NKWebKit::Make<NKWebKit::FrameHandle>(999);

    Require(WKPageLookUpFrameFromHandle(pageRef, handle1.Get()) == frame1Ref);
    Require(WKPageLookUpFrameFromHandle(pageRef, handle2.Get()) == frame2Ref);
    Require(WKPageLookUpFrameFromHandle(pageRef, handleUnknown.Get()) == nullptr);

    bool sourceCalled = false;
    WKPageGetSourceForFrame(pageRef, frame1Ref, &sourceCalled, SourceCallback);
    Require(sourceCalled);

    WKPageGetSourceForFrame(pageRef, frame1Ref, nullptr, nullptr);

    auto otherFrame = NKWebKit::Make<NKWebKit::Frame>();
    otherFrame->source = u"<html><body>Foreign Frame</body></html>";
    bool foreignSourceCalled = false;
    WKPageGetSourceForFrame(pageRef, otherFrame.Get(), &foreignSourceCalled, NullCallback);
    Require(foreignSourceCalled);

    auto testURL = NKWebKit::Make<NKWebKit::URL>("https://example.com/asset.png");
    bool resourceCalled = false;
    WKFrameGetResourceData(frame1Ref, testURL.Get(), &resourceCalled, ResourceCallback);
    Require(resourceCalled);

    auto missingURL = NKWebKit::Make<NKWebKit::URL>("https://example.com/missing.png");
    bool missingResourceCalled = false;
    WKFrameGetResourceData(frame1Ref, missingURL.Get(), &missingResourceCalled, NullCallback);
    Require(missingResourceCalled);

    WKFrameGetResourceData(frame1Ref, testURL.Get(), nullptr, nullptr);

    page->isClosed = true;

    Require(WKPageLookUpFrameFromHandle(pageRef, handle1.Get()) == nullptr);

    bool closedSourceCalled = false;
    WKPageGetSourceForFrame(pageRef, frame1Ref, &closedSourceCalled, NullCallback);
    Require(closedSourceCalled);

    page->isClosed = false;

    auto notFrame = NKWebKit::Make<NKWebKit::Array>();

    RequireInvalidArgument([] { WKPageLookUpFrameFromHandle(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageLookUpFrameFromHandle(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKPageLookUpFrameFromHandle(nullptr, handle1.Get()); });
    RequireInvalidArgument([&] { WKPageLookUpFrameFromHandle(notFrame.Get(), handle1.Get()); });
    RequireInvalidArgument([&] { WKPageLookUpFrameFromHandle(pageRef, notFrame.Get()); });

    RequireInvalidArgument([] { WKPageGetSourceForFrame(nullptr, nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGetSourceForFrame(pageRef, nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGetSourceForFrame(nullptr, frame1Ref, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGetSourceForFrame(notFrame.Get(), frame1Ref, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGetSourceForFrame(pageRef, notFrame.Get(), nullptr, nullptr); });

    RequireInvalidArgument([] { WKFrameGetResourceData(nullptr, nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKFrameGetResourceData(frame1Ref, nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKFrameGetResourceData(nullptr, testURL.Get(), nullptr, nullptr); });
    RequireInvalidArgument([&] { WKFrameGetResourceData(notFrame.Get(), testURL.Get(), nullptr, nullptr); });
    RequireInvalidArgument([&] { WKFrameGetResourceData(frame1Ref, notFrame.Get(), nullptr, nullptr); });

    return 0;
}
