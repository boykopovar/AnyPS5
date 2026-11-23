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
        std::fprintf(stderr, "BundlePageOverlaysAndBanners check failed at line %u\n", location.line());
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

    int dummyClient = 42;
    const void* overlayRef = WKBundlePageOverlayCreate(&dummyClient);
    Require(overlayRef != nullptr);
    Require(WKGetTypeID(overlayRef) == WKBundlePageOverlayGetTypeID());

    auto* overlay = NKWebKit::Cast<NKWebKit::BundlePageOverlay>(overlayRef, "TestOverlay");
    Require(overlay->client == &dummyClient);
    Require(WKBundlePageOverlayFractionFadedIn(overlayRef) == 1.0);

    overlay->fractionFadedIn = 0.75;
    Require(WKBundlePageOverlayFractionFadedIn(overlayRef) == 0.75);

    NKWebKit::RectValue rect{{10.0, 20.0}, {300.0, 200.0}};
    Require(overlay->needsDisplayCount == 0);
    WKBundlePageOverlaySetNeedsDisplay(overlayRef, rect);
    Require(overlay->needsDisplayCount == 1);
    Require(overlay->lastNeedsDisplayRect.origin.x == 10.0 && overlay->lastNeedsDisplayRect.origin.y == 20.0);
    Require(overlay->lastNeedsDisplayRect.size.width == 300.0 && overlay->lastNeedsDisplayRect.size.height == 200.0);

    Require(page->pageOverlays.empty());
    WKBundlePageInstallPageOverlay(pageRef, overlayRef);
    Require(page->pageOverlays.size() == 1);
    Require(page->pageOverlays[0].Get() == overlay);

    WKBundlePageInstallPageOverlay(pageRef, overlayRef);
    Require(page->pageOverlays.size() == 1);

    const void* overlayRef2 = WKBundlePageOverlayCreate(nullptr);
    WKBundlePageInstallPageOverlayWithAnimation(pageRef, overlayRef2);
    Require(page->pageOverlays.size() == 2);

    WKBundlePageUninstallPageOverlay(pageRef, overlayRef);
    Require(page->pageOverlays.size() == 1);
    Require(page->pageOverlays[0].Get() == NKWebKit::Cast<NKWebKit::BundlePageOverlay>(overlayRef2, "TestOverlay2"));

    WKBundlePageUninstallPageOverlayWithAnimation(pageRef, overlayRef2);
    Require(page->pageOverlays.empty());

    Require(!page->headerBanner);
    Require(!page->footerBanner);

    auto header = NKWebKit::Make<NKWebKit::BundlePageBanner>();
    auto footer = NKWebKit::Make<NKWebKit::BundlePageBanner>();

    WKBundlePageSetHeaderBanner(pageRef, header.Get());
    Require(page->headerBanner.Get() == header.Get());

    WKBundlePageSetFooterBanner(pageRef, footer.Get());
    Require(page->footerBanner.Get() == footer.Get());

    WKBundlePageSetHeaderBanner(pageRef, nullptr);
    Require(!page->headerBanner);

    WKBundlePageSetFooterBanner(pageRef, nullptr);
    Require(!page->footerBanner);

    RequireInvalidArgument([&] { WKBundlePageInstallPageOverlay(nullptr, overlayRef); });
    RequireInvalidArgument([&] { WKBundlePageInstallPageOverlay(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageInstallPageOverlayWithAnimation(nullptr, overlayRef); });
    RequireInvalidArgument([&] { WKBundlePageInstallPageOverlayWithAnimation(pageRef, nullptr); });

    RequireInvalidArgument([&] { WKBundlePageUninstallPageOverlay(nullptr, overlayRef); });
    RequireInvalidArgument([&] { WKBundlePageUninstallPageOverlay(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageUninstallPageOverlayWithAnimation(nullptr, overlayRef); });
    RequireInvalidArgument([&] { WKBundlePageUninstallPageOverlayWithAnimation(pageRef, nullptr); });

    RequireInvalidArgument([&] { WKBundlePageSetHeaderBanner(nullptr, header.Get()); });
    RequireInvalidArgument([&] { WKBundlePageSetFooterBanner(nullptr, footer.Get()); });

    RequireInvalidArgument([&] { WKBundlePageOverlayFractionFadedIn(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageOverlaySetNeedsDisplay(nullptr, rect); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKBundlePageInstallPageOverlay(frame.Get(), overlayRef); });
    RequireInvalidArgument([&] { WKBundlePageInstallPageOverlay(pageRef, frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageInstallPageOverlayWithAnimation(frame.Get(), overlayRef); });
    RequireInvalidArgument([&] { WKBundlePageInstallPageOverlayWithAnimation(pageRef, frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageUninstallPageOverlay(frame.Get(), overlayRef); });
    RequireInvalidArgument([&] { WKBundlePageUninstallPageOverlay(pageRef, frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageUninstallPageOverlayWithAnimation(frame.Get(), overlayRef); });
    RequireInvalidArgument([&] { WKBundlePageUninstallPageOverlayWithAnimation(pageRef, frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageSetHeaderBanner(frame.Get(), header.Get()); });
    RequireInvalidArgument([&] { WKBundlePageSetHeaderBanner(pageRef, frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageSetFooterBanner(frame.Get(), footer.Get()); });
    RequireInvalidArgument([&] { WKBundlePageSetFooterBanner(pageRef, frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageOverlayFractionFadedIn(frame.Get()); });
    RequireInvalidArgument([&] { WKBundlePageOverlaySetNeedsDisplay(frame.Get(), rect); });

    return 0;
}
