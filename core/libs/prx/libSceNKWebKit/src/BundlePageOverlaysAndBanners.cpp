#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <algorithm>

extern "C" {

void APS5_VABI WKBundlePageInstallPageOverlay(const void* pageRef, const void* pageOverlayRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto* overlay = NKWebKit::Cast<NKWebKit::BundlePageOverlay>(pageOverlayRef, __func__);
    if (std::find_if(page->pageOverlays.begin(), page->pageOverlays.end(), [&](const auto& ptr) { return ptr.Get() == overlay; }) == page->pageOverlays.end()) {
        page->pageOverlays.push_back(NKWebKit::Retained<NKWebKit::BundlePageOverlay>::Share(overlay));
    }
}

void APS5_VABI WKBundlePageInstallPageOverlayWithAnimation(const void* pageRef, const void* pageOverlayRef) {
    WKBundlePageInstallPageOverlay(pageRef, pageOverlayRef);
}

void APS5_VABI WKBundlePageUninstallPageOverlay(const void* pageRef, const void* pageOverlayRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto* overlay = NKWebKit::Cast<NKWebKit::BundlePageOverlay>(pageOverlayRef, __func__);
    auto it = std::remove_if(page->pageOverlays.begin(), page->pageOverlays.end(), [&](const auto& ptr) { return ptr.Get() == overlay; });
    page->pageOverlays.erase(it, page->pageOverlays.end());
}

void APS5_VABI WKBundlePageUninstallPageOverlayWithAnimation(const void* pageRef, const void* pageOverlayRef) {
    WKBundlePageUninstallPageOverlay(pageRef, pageOverlayRef);
}

void APS5_VABI WKBundlePageSetHeaderBanner(const void* pageRef, const void* bannerRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->headerBanner = bannerRef ? NKWebKit::Retained<NKWebKit::BundlePageBanner>::Share(NKWebKit::Cast<NKWebKit::BundlePageBanner>(bannerRef, __func__)) : nullptr;
}

void APS5_VABI WKBundlePageSetFooterBanner(const void* pageRef, const void* bannerRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->footerBanner = bannerRef ? NKWebKit::Retained<NKWebKit::BundlePageBanner>::Share(NKWebKit::Cast<NKWebKit::BundlePageBanner>(bannerRef, __func__)) : nullptr;
}

const void* APS5_VABI WKBundlePageOverlayCreate(const void* client) {
    auto overlay = NKWebKit::Make<NKWebKit::BundlePageOverlay>();
    overlay->client = client;
    return NKWebKit::ToRef(overlay);
}

double APS5_VABI WKBundlePageOverlayFractionFadedIn(const void* pageOverlayRef) {
    return NKWebKit::Cast<NKWebKit::BundlePageOverlay>(pageOverlayRef, __func__)->fractionFadedIn;
}

void APS5_VABI WKBundlePageOverlaySetNeedsDisplay(const void* pageOverlayRef, NKWebKit::RectValue rect) {
    auto* overlay = NKWebKit::Cast<NKWebKit::BundlePageOverlay>(pageOverlayRef, __func__);
    overlay->lastNeedsDisplayRect = rect;
    ++overlay->needsDisplayCount;
}

}
