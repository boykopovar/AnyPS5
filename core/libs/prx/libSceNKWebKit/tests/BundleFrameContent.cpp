#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/FrameHandle.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

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
    auto frameObj = NKWebKit::Make<NKWebKit::BundleFrame>();
    const void* frame = frameObj.Get();
    auto* f = frameObj.Get();

    f->frameID = 12345;
    const void* handle = WKBundleFrameCreateFrameHandle(frame);
    Require(handle != nullptr);
    Require(WKGetTypeID(handle) == WKFrameHandleGetTypeID());
    Require(WKFrameHandleGetFrameID(handle) == 12345);
    WKRelease(handle);

    const void* counter = WKBundleFrameCopyCounterValue(frame, nullptr);
    Require(counter != nullptr);
    Require(WKGetTypeID(counter) == WKStringGetTypeID());
    Require(WKStringIsEqualToUTF8CString(counter, ""));
    WKRelease(counter);

    const void* emptyText = WKBundleFrameCopyInnerText(frame);
    Require(WKStringIsEqualToUTF8CString(emptyText, ""));
    WKRelease(emptyText);

    f->innerText = u"hello world";
    const void* text = WKBundleFrameCopyInnerText(frame);
    Require(WKStringIsEqualToUTF8CString(text, "hello world"));
    WKRelease(text);

    const void* emptyLayer = WKBundleFrameCopyLayerTreeAsText(frame);
    Require(WKStringIsEqualToUTF8CString(emptyLayer, ""));
    WKRelease(emptyLayer);

    f->layerTreeAsText = u"layer tree root";
    const void* layer = WKBundleFrameCopyLayerTreeAsText(frame);
    Require(WKStringIsEqualToUTF8CString(layer, "layer tree root"));
    WKRelease(layer);

    const void* testUrl = WKURLCreateWithUTF8CString("https://example.com/asset.png");

    f->mimeType = "image/png";
    const void* mimeWithUrl = WKBundleFrameCopyMIMETypeForResourceWithURL(frame, testUrl);
    Require(WKStringIsEqualToUTF8CString(mimeWithUrl, "image/png"));
    WKRelease(mimeWithUrl);

    const void* mimeNullUrl = WKBundleFrameCopyMIMETypeForResourceWithURL(frame, nullptr);
    Require(WKStringIsEqualToUTF8CString(mimeNullUrl, "image/png"));
    WKRelease(mimeNullUrl);

    f->suggestedFilename = "asset.png";
    const void* nameWithUrl = WKBundleFrameCopySuggestedFilenameForResourceWithURL(frame, testUrl);
    Require(WKStringIsEqualToUTF8CString(nameWithUrl, "asset.png"));
    WKRelease(nameWithUrl);

    const void* nameNullUrl = WKBundleFrameCopySuggestedFilenameForResourceWithURL(frame, nullptr);
    Require(WKStringIsEqualToUTF8CString(nameNullUrl, "asset.png"));
    WKRelease(nameNullUrl);

    double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
    bool hasBg = WKBundleFrameGetDocumentBackgroundColor(frame, &r, &g, &b, &a);
    Require(!hasBg);
    Require(r == 1.0 && g == 1.0 && b == 1.0 && a == 1.0);

    Require(!WKBundleFrameGetDocumentBackgroundColor(frame, nullptr, &g, &b, &a));
    Require(!WKBundleFrameGetDocumentBackgroundColor(frame, &r, nullptr, &b, &a));
    Require(!WKBundleFrameGetDocumentBackgroundColor(frame, &r, &g, nullptr, &a));
    Require(!WKBundleFrameGetDocumentBackgroundColor(frame, &r, &g, &b, nullptr));

    f->backgroundColor = NKWebKit::BundleFrame::BackgroundColor{0.25, 0.5, 0.75, 0.5};
    hasBg = WKBundleFrameGetDocumentBackgroundColor(frame, &r, &g, &b, &a);
    Require(hasBg);
    Require(r == 0.25 && g == 0.5 && b == 0.75 && a == 0.5);

    RequireThrows([&] { WKBundleFrameCreateFrameHandle(nullptr); });
    RequireThrows([&] { WKBundleFrameCopyCounterValue(nullptr, nullptr); });
    RequireThrows([&] { WKBundleFrameCopyInnerText(nullptr); });
    RequireThrows([&] { WKBundleFrameCopyLayerTreeAsText(nullptr); });
    RequireThrows([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(nullptr, testUrl); });
    RequireThrows([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(nullptr, testUrl); });
    RequireThrows([&] { WKBundleFrameGetDocumentBackgroundColor(nullptr, &r, &g, &b, &a); });

    auto dummyStr = NKWebKit::Make<NKWebKit::String>();
    RequireThrows([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(frame, dummyStr.Get()); });
    RequireThrows([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(frame, dummyStr.Get()); });

    WKRelease(testUrl);

    std::printf("All libSceNKWebKit BundleFrameContent tests passed!\n");
    return 0;
}
