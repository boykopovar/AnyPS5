#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
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
    auto bundle = NKWebKit::Make<NKWebKit::Bundle>();
    const void* bundleRef = bundle.Get();
    Require(bundleRef != nullptr);
    Require(WKGetTypeID(bundleRef) == WKBundleGetTypeID());

    Require(bundle->client == nullptr);
    Require(!bundle->frameFlatteningEnabled);
    Require(!bundle->spatialNavigationEnabled);
    Require(!bundle->allowFileAccessFromFileURLs);
    Require(!bundle->javaScriptCanAccessClipboard);
    Require(!bundle->allowStorageAccessFromFileURLs);
    Require(!bundle->allowUniversalAccessFromFileURLs);
    Require(!bundle->popupBlockingEnabled);
    Require(!bundle->asynchronousSpellCheckingEnabled);
    Require(bundle->authorAndUserStylesEnabled);
    Require(bundle->minimumLogicalFontSize == 0);
    Require(bundle->databaseQuota == 0);
    Require(!bundle->userStyleSheetLocation);

    const int dummyClient = 42;
    WKBundleSetClient(bundleRef, &dummyClient);
    Require(bundle->client == &dummyClient);

    WKBundleSetFrameFlatteningEnabled(bundleRef, true);
    Require(bundle->frameFlatteningEnabled);
    WKBundleSetFrameFlatteningEnabled(bundleRef, false);
    Require(!bundle->frameFlatteningEnabled);

    WKBundleSetSpatialNavigationEnabled(bundleRef, true);
    Require(bundle->spatialNavigationEnabled);
    WKBundleSetSpatialNavigationEnabled(bundleRef, false);
    Require(!bundle->spatialNavigationEnabled);

    WKBundleSetAllowFileAccessFromFileURLs(bundleRef, true);
    Require(bundle->allowFileAccessFromFileURLs);
    WKBundleSetAllowFileAccessFromFileURLs(bundleRef, false);
    Require(!bundle->allowFileAccessFromFileURLs);

    WKBundleSetJavaScriptCanAccessClipboard(bundleRef, true);
    Require(bundle->javaScriptCanAccessClipboard);
    WKBundleSetJavaScriptCanAccessClipboard(bundleRef, false);
    Require(!bundle->javaScriptCanAccessClipboard);

    WKBundleSetAllowStorageAccessFromFileURLS(bundleRef, true);
    Require(bundle->allowStorageAccessFromFileURLs);
    WKBundleSetAllowStorageAccessFromFileURLS(bundleRef, false);
    Require(!bundle->allowStorageAccessFromFileURLs);

    WKBundleSetAllowUniversalAccessFromFileURLs(bundleRef, true);
    Require(bundle->allowUniversalAccessFromFileURLs);
    WKBundleSetAllowUniversalAccessFromFileURLs(bundleRef, false);
    Require(!bundle->allowUniversalAccessFromFileURLs);

    WKBundleSetPopupBlockingEnabled(bundleRef, true);
    Require(bundle->popupBlockingEnabled);
    WKBundleSetPopupBlockingEnabled(bundleRef, false);
    Require(!bundle->popupBlockingEnabled);

    WKBundleSetAsynchronousSpellCheckingEnabled(bundleRef, true);
    Require(bundle->asynchronousSpellCheckingEnabled);
    WKBundleSetAsynchronousSpellCheckingEnabled(bundleRef, false);
    Require(!bundle->asynchronousSpellCheckingEnabled);

    WKBundleSetAuthorAndUserStylesEnabled(bundleRef, false);
    Require(!bundle->authorAndUserStylesEnabled);
    WKBundleSetAuthorAndUserStylesEnabled(bundleRef, true);
    Require(bundle->authorAndUserStylesEnabled);

    WKBundleSetMinimumLogicalFontSize(bundleRef, 12);
    Require(bundle->minimumLogicalFontSize == 12);
    WKBundleSetMinimumLogicalFontSize(bundleRef, 18);
    Require(bundle->minimumLogicalFontSize == 18);

    WKBundleSetDatabaseQuota(bundleRef, 10485760ull);
    Require(bundle->databaseQuota == 10485760ull);

    const void* location = WKStringCreateWithUTF8CString("file:///styles/custom.css");
    WKBundleSetUserStyleSheetLocation(bundleRef, location);
    Require(bundle->userStyleSheetLocation.Get() != nullptr);
    Require(WKStringIsEqualToUTF8CString(bundle->userStyleSheetLocation.Get(), "file:///styles/custom.css"));

    WKBundleSetUserStyleSheetLocation(bundleRef, nullptr);
    Require(bundle->userStyleSheetLocation.Get() == nullptr);

    const void* dummyObj = location;
    RequireThrows([&] { WKBundleSetClient(nullptr, &dummyClient); });
    RequireThrows([&] { WKBundleSetFrameFlatteningEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetSpatialNavigationEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetAllowFileAccessFromFileURLs(nullptr, true); });
    RequireThrows([&] { WKBundleSetJavaScriptCanAccessClipboard(nullptr, true); });
    RequireThrows([&] { WKBundleSetAllowStorageAccessFromFileURLS(nullptr, true); });
    RequireThrows([&] { WKBundleSetAllowUniversalAccessFromFileURLs(nullptr, true); });
    RequireThrows([&] { WKBundleSetPopupBlockingEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetAsynchronousSpellCheckingEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetAuthorAndUserStylesEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetMinimumLogicalFontSize(nullptr, 10); });
    RequireThrows([&] { WKBundleSetDatabaseQuota(nullptr, 100); });
    RequireThrows([&] { WKBundleSetUserStyleSheetLocation(nullptr, location); });

    RequireThrows([&] { WKBundleSetClient(dummyObj, &dummyClient); });
    RequireThrows([&] { WKBundleSetFrameFlatteningEnabled(dummyObj, true); });
    RequireThrows([&] { WKBundleSetSpatialNavigationEnabled(dummyObj, true); });
    RequireThrows([&] { WKBundleSetAllowFileAccessFromFileURLs(dummyObj, true); });
    RequireThrows([&] { WKBundleSetJavaScriptCanAccessClipboard(dummyObj, true); });
    RequireThrows([&] { WKBundleSetAllowStorageAccessFromFileURLS(dummyObj, true); });
    RequireThrows([&] { WKBundleSetAllowUniversalAccessFromFileURLs(dummyObj, true); });
    RequireThrows([&] { WKBundleSetPopupBlockingEnabled(dummyObj, true); });
    RequireThrows([&] { WKBundleSetAsynchronousSpellCheckingEnabled(dummyObj, true); });
    RequireThrows([&] { WKBundleSetAuthorAndUserStylesEnabled(dummyObj, true); });
    RequireThrows([&] { WKBundleSetMinimumLogicalFontSize(dummyObj, 10); });
    RequireThrows([&] { WKBundleSetDatabaseQuota(dummyObj, 100); });
    RequireThrows([&] { WKBundleSetUserStyleSheetLocation(dummyObj, location); });
    RequireThrows([&] { WKBundleSetUserStyleSheetLocation(bundleRef, bundleRef); });

    WKRelease(location);
    std::printf("All libSceNKWebKit Bundle tests passed!\n");
    return 0;
}
