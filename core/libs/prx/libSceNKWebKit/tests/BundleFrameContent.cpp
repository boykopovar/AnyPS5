#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKBundleFrame content check failed at line %u\n", location.line());
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

std::string ToString(const void* ref) {
    if (!ref) {
        return {};
    }
    const auto* str = static_cast<const NKWebKit::String*>(ref);
    return str->Utf8();
}

}

int main() {
    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    auto nodeHandle = NKWebKit::Make<NKWebKit::BundleNodeHandle>();
    auto url = NKWebKit::Make<NKWebKit::URL>("https://example.com/asset.png");

    const void* frameRef = frame.Get();
    const void* nodeHandleRef = nodeHandle.Get();
    const void* urlRef = url.Get();

    Require(ToString(WKBundleFrameCopyCounterValue(frameRef, nodeHandleRef)).empty());
    Require(ToString(WKBundleFrameCopyInnerText(frameRef)).empty());
    Require(ToString(WKBundleFrameCopyLayerTreeAsText(frameRef)).empty());
    Require(ToString(WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, urlRef)).empty());
    Require(ToString(WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, urlRef)).empty());

    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 0.0;
    Require(!WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, &a));
    Require(r == 1.0);
    Require(g == 1.0);
    Require(b == 1.0);
    Require(a == 1.0);

    frame->counterValue = NKWebKit::String::FromUtf8("42");
    frame->innerText = NKWebKit::String::FromUtf8("Sample Inner Text");
    frame->layerTreeAsText = NKWebKit::String::FromUtf8("LayerTree(Root)");
    frame->mimeTypeForResource = NKWebKit::String::FromUtf8("image/png");
    frame->suggestedFilenameForResource = NKWebKit::String::FromUtf8("asset.png");
    frame->hasDocumentBackgroundColor = true;
    frame->documentBackgroundColor[0] = 0.1;
    frame->documentBackgroundColor[1] = 0.2;
    frame->documentBackgroundColor[2] = 0.3;
    frame->documentBackgroundColor[3] = 0.4;

    Require(ToString(WKBundleFrameCopyCounterValue(frameRef, nodeHandleRef)) == "42");
    Require(ToString(WKBundleFrameCopyInnerText(frameRef)) == "Sample Inner Text");
    Require(ToString(WKBundleFrameCopyLayerTreeAsText(frameRef)) == "LayerTree(Root)");
    Require(ToString(WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, urlRef)) == "image/png");
    Require(ToString(WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, urlRef)) == "asset.png");

    Require(WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, &a));
    Require(r == 0.1);
    Require(g == 0.2);
    Require(b == 0.3);
    Require(a == 0.4);

    RequireInvalidArgument([] { WKBundleFrameCopyCounterValue(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopyCounterValue(frameRef, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopyCounterValue(nullptr, nodeHandleRef); });
    RequireInvalidArgument([] { WKBundleFrameCopyInnerText(nullptr); });
    RequireInvalidArgument([] { WKBundleFrameCopyLayerTreeAsText(nullptr); });
    RequireInvalidArgument([] { WKBundleFrameCopyMIMETypeForResourceWithURL(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(nullptr, urlRef); });
    RequireInvalidArgument([] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(nullptr, urlRef); });

    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(nullptr, &r, &g, &b, &a); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, nullptr, &g, &b, &a); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, nullptr, &b, &a); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, nullptr, &a); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, nullptr); });

    auto dummy = NKWebKit::Make<NKWebKit::BundleScriptWorld>();
    RequireInvalidArgument([&] { WKBundleFrameCopyCounterValue(dummy.Get(), nodeHandleRef); });
    RequireInvalidArgument([&] { WKBundleFrameCopyCounterValue(frameRef, dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameCopyInnerText(dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameCopyLayerTreeAsText(dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(dummy.Get(), urlRef); });
    RequireInvalidArgument([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(dummy.Get(), urlRef); });
    RequireInvalidArgument([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(dummy.Get(), &r, &g, &b, &a); });

    return 0;
}
