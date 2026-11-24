#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKPageBeginPrinting(const void* pageRef, const void* frameRef, WKPrintInfo printInfo) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || page->isInPrintingMode) {
        return;
    }
    NKWebKit::Cast<NKWebKit::Frame>(frameRef, __func__);
    page->isInPrintingMode = true;
    page->printInfo = {printInfo.pageSetupScaleFactor, printInfo.availablePaperWidth, printInfo.availablePaperHeight};
}

void APS5_VABI WKPageComputePagesForPrinting(const void* pageRef, const void* frameRef, WKPrintInfo printInfo, WKPageComputePagesForPrintingFunction callback, void* context) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    NKWebKit::Cast<NKWebKit::Frame>(frameRef, __func__);
    if (!callback) {
        return;
    }
    if (page->isClosed) {
        callback(nullptr, 0, 1.0, nullptr, context);
        return;
    }
    page->isInPrintingMode = true;
    page->printInfo = {printInfo.pageSetupScaleFactor, printInfo.availablePaperWidth, printInfo.availablePaperHeight};
    NKWebKit::RectValue pageRect{{0.0, 0.0}, {static_cast<double>(printInfo.availablePaperWidth), static_cast<double>(printInfo.availablePaperHeight)}};
    callback(&pageRect, 1, static_cast<double>(printInfo.pageSetupScaleFactor), nullptr, context);
}

void APS5_VABI WKPageEndPrinting(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !page->isInPrintingMode) {
        return;
    }
    page->isInPrintingMode = false;
}

const void* APS5_VABI WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto result = NKWebKit::Make<NKWebKit::String>(page->renderTreeExternalRepresentationForPrinting);
    return NKWebKit::ToRef(result);
}

}
