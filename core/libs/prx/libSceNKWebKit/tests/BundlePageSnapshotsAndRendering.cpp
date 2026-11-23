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
        std::fprintf(stderr, "WKBundlePage snapshots/rendering check failed at line %u\n", location.line());
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
    auto page = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* pageRef = page.Get();

    NKWebKit::RectValue rect{{0.0, 0.0}, {120.0, 80.0}};

    const void* viewSnapRef = WKBundlePageCreateSnapshotInViewCoordinates(pageRef, rect, 0);
    auto* viewImage = NKWebKit::Cast<NKWebKit::Image>(viewSnapRef, "main");
    Require(viewImage->width == 120);
    Require(viewImage->height == 80);

    const void* docSnapRef = WKBundlePageCreateSnapshotInDocumentCoordinates(pageRef, rect, 0);
    auto* docImage = NKWebKit::Cast<NKWebKit::Image>(docSnapRef, "main");
    Require(docImage->width == 120);
    Require(docImage->height == 80);

    const void* scaledSnapRef = WKBundlePageCreateScaledSnapshotInDocumentCoordinates(pageRef, rect, 2.5, 0);
    auto* scaledImage = NKWebKit::Cast<NKWebKit::Image>(scaledSnapRef, "main");
    Require(scaledImage->width == 300);
    Require(scaledImage->height == 200);

    const void* optSnapRef = WKBundlePageCreateSnapshotWithOptions(pageRef, rect, 1);
    auto* optImage = NKWebKit::Cast<NKWebKit::Image>(optSnapRef, "main");
    Require(optImage->width == 120);
    Require(optImage->height == 80);

    NKWebKit::RectValue negativeRect{{0.0, 0.0}, {-10.0, -20.0}};
    const void* clampedSnapRef = WKBundlePageCreateSnapshotInViewCoordinates(pageRef, negativeRect, 0);
    auto* clampedImage = NKWebKit::Cast<NKWebKit::Image>(clampedSnapRef, "main");
    Require(clampedImage->width == 0);
    Require(clampedImage->height == 0);

    const void* defaultTreeRef = WKBundlePageCopyRenderTreeExternalRepresentation(pageRef, 0);
    auto* defaultTreeStr = NKWebKit::Cast<NKWebKit::String>(defaultTreeRef, "main");
    Require(defaultTreeStr->characters.empty());

    page->renderTreeExternalRepresentation = "layer (RenderView 0x1234)";
    const void* customTreeRef = WKBundlePageCopyRenderTreeExternalRepresentation(pageRef, 1);
    auto* customTreeStr = NKWebKit::Cast<NKWebKit::String>(customTreeRef, "main");
    Require(customTreeStr->Utf8() == "layer (RenderView 0x1234)");

    const void* defaultRectsRef = WKBundlePageCopyTrackedRepaintRects(pageRef);
    auto* defaultRectsArray = NKWebKit::Cast<NKWebKit::Array>(defaultRectsRef, "main");
    Require(defaultRectsArray->elements.empty());

    auto customRects = NKWebKit::Make<NKWebKit::Array>();
    customRects->elements.push_back(NKWebKit::Make<NKWebKit::String>(std::u16string(u"rect1")));
    page->trackedRepaintRects = customRects;
    const void* customRectsRef = WKBundlePageCopyTrackedRepaintRects(pageRef);
    auto* customRectsArray = NKWebKit::Cast<NKWebKit::Array>(customRectsRef, "main");
    Require(customRectsArray->elements.size() == 1);

    Require(page->forceRepaintCount == 0);
    WKBundlePageForceRepaint(pageRef);
    Require(page->forceRepaintCount == 1);
    WKBundlePageForceRepaint(pageRef);
    Require(page->forceRepaintCount == 2);

    std::uint32_t token1 = WKBundlePageExtendIncrementalRenderingSuppression(pageRef);
    Require(token1 == 1);
    Require(page->nextRenderingSuppressionToken == 1);

    std::uint32_t token2 = WKBundlePageExtendIncrementalRenderingSuppression(pageRef);
    Require(token2 == 2);
    Require(page->nextRenderingSuppressionToken == 2);

    WKBundlePageStopExtendingIncrementalRenderingSuppression(pageRef, token1);
    Require(page->lastStoppedRenderingSuppressionToken == 1);

    WKBundlePageStopExtendingIncrementalRenderingSuppression(pageRef, token2);
    Require(page->lastStoppedRenderingSuppressionToken == 2);

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();

    RequireInvalidArgument([&] { WKBundlePageCreateSnapshotInViewCoordinates(nullptr, rect, 0); });
    RequireInvalidArgument([&] { WKBundlePageCreateSnapshotInViewCoordinates(frame.Get(), rect, 0); });

    RequireInvalidArgument([&] { WKBundlePageCreateSnapshotInDocumentCoordinates(nullptr, rect, 0); });
    RequireInvalidArgument([&] { WKBundlePageCreateSnapshotInDocumentCoordinates(frame.Get(), rect, 0); });

    RequireInvalidArgument([&] { WKBundlePageCreateScaledSnapshotInDocumentCoordinates(nullptr, rect, 1.0, 0); });
    RequireInvalidArgument([&] { WKBundlePageCreateScaledSnapshotInDocumentCoordinates(frame.Get(), rect, 1.0, 0); });

    RequireInvalidArgument([&] { WKBundlePageCreateSnapshotWithOptions(nullptr, rect, 0); });
    RequireInvalidArgument([&] { WKBundlePageCreateSnapshotWithOptions(frame.Get(), rect, 0); });

    RequireInvalidArgument([&] { WKBundlePageCopyRenderTreeExternalRepresentation(nullptr, 0); });
    RequireInvalidArgument([&] { WKBundlePageCopyRenderTreeExternalRepresentation(frame.Get(), 0); });

    RequireInvalidArgument([&] { WKBundlePageCopyTrackedRepaintRects(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageCopyTrackedRepaintRects(frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageForceRepaint(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageForceRepaint(frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageExtendIncrementalRenderingSuppression(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageExtendIncrementalRenderingSuppression(frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageStopExtendingIncrementalRenderingSuppression(nullptr, 1); });
    RequireInvalidArgument([&] { WKBundlePageStopExtendingIncrementalRenderingSuppression(frame.Get(), 1); });

    return 0;
}
