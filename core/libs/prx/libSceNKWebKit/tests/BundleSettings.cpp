#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
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
    const auto* b = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);

    Require(!b->allowFileAccessFromFileURLs);
    Require(!b->allowStorageAccessFromFileURLs);
    Require(!b->allowUniversalAccessFromFileURLs);
    Require(!b->asynchronousSpellCheckingEnabled);
    Require(b->authorAndUserStylesEnabled);
    Require(b->databaseQuota == 0);
    Require(!b->frameFlatteningEnabled);
    Require(!b->javaScriptCanAccessClipboard);
    Require(b->minimumLogicalFontSize == 0);
    Require(b->popupBlockingEnabled);
    Require(!b->spatialNavigationEnabled);

    WKBundleSetAllowFileAccessFromFileURLs(bundleRef, true);
    Require(b->allowFileAccessFromFileURLs);
    WKBundleSetAllowFileAccessFromFileURLs(bundleRef, false);
    Require(!b->allowFileAccessFromFileURLs);

    WKBundleSetAllowStorageAccessFromFileURLS(bundleRef, true);
    Require(b->allowStorageAccessFromFileURLs);
    WKBundleSetAllowStorageAccessFromFileURLS(bundleRef, false);
    Require(!b->allowStorageAccessFromFileURLs);

    WKBundleSetAllowUniversalAccessFromFileURLs(bundleRef, true);
    Require(b->allowUniversalAccessFromFileURLs);
    WKBundleSetAllowUniversalAccessFromFileURLs(bundleRef, false);
    Require(!b->allowUniversalAccessFromFileURLs);

    WKBundleSetAsynchronousSpellCheckingEnabled(bundleRef, true);
    Require(b->asynchronousSpellCheckingEnabled);
    WKBundleSetAsynchronousSpellCheckingEnabled(bundleRef, false);
    Require(!b->asynchronousSpellCheckingEnabled);

    WKBundleSetAuthorAndUserStylesEnabled(bundleRef, false);
    Require(!b->authorAndUserStylesEnabled);
    WKBundleSetAuthorAndUserStylesEnabled(bundleRef, true);
    Require(b->authorAndUserStylesEnabled);

    WKBundleSetDatabaseQuota(bundleRef, 1024 * 1024);
    Require(b->databaseQuota == 1024 * 1024);

    WKBundleSetFrameFlatteningEnabled(bundleRef, true);
    Require(b->frameFlatteningEnabled);
    WKBundleSetFrameFlatteningEnabled(bundleRef, false);
    Require(!b->frameFlatteningEnabled);

    WKBundleSetJavaScriptCanAccessClipboard(bundleRef, true);
    Require(b->javaScriptCanAccessClipboard);
    WKBundleSetJavaScriptCanAccessClipboard(bundleRef, false);
    Require(!b->javaScriptCanAccessClipboard);

    WKBundleSetMinimumLogicalFontSize(bundleRef, 12);
    Require(b->minimumLogicalFontSize == 12);

    WKBundleSetPopupBlockingEnabled(bundleRef, false);
    Require(!b->popupBlockingEnabled);
    WKBundleSetPopupBlockingEnabled(bundleRef, true);
    Require(b->popupBlockingEnabled);

    WKBundleSetSpatialNavigationEnabled(bundleRef, true);
    Require(b->spatialNavigationEnabled);
    WKBundleSetSpatialNavigationEnabled(bundleRef, false);
    Require(!b->spatialNavigationEnabled);

    const void* dummyString = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKBundleSetAllowFileAccessFromFileURLs(nullptr, true); });
    RequireThrows([&] { WKBundleSetAllowFileAccessFromFileURLs(dummyString, true); });

    RequireThrows([&] { WKBundleSetAllowStorageAccessFromFileURLS(nullptr, true); });
    RequireThrows([&] { WKBundleSetAllowStorageAccessFromFileURLS(dummyString, true); });

    RequireThrows([&] { WKBundleSetAllowUniversalAccessFromFileURLs(nullptr, true); });
    RequireThrows([&] { WKBundleSetAllowUniversalAccessFromFileURLs(dummyString, true); });

    RequireThrows([&] { WKBundleSetAsynchronousSpellCheckingEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetAsynchronousSpellCheckingEnabled(dummyString, true); });

    RequireThrows([&] { WKBundleSetAuthorAndUserStylesEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetAuthorAndUserStylesEnabled(dummyString, true); });

    RequireThrows([&] { WKBundleSetDatabaseQuota(nullptr, 100); });
    RequireThrows([&] { WKBundleSetDatabaseQuota(dummyString, 100); });

    RequireThrows([&] { WKBundleSetFrameFlatteningEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetFrameFlatteningEnabled(dummyString, true); });

    RequireThrows([&] { WKBundleSetJavaScriptCanAccessClipboard(nullptr, true); });
    RequireThrows([&] { WKBundleSetJavaScriptCanAccessClipboard(dummyString, true); });

    RequireThrows([&] { WKBundleSetMinimumLogicalFontSize(nullptr, 10); });
    RequireThrows([&] { WKBundleSetMinimumLogicalFontSize(dummyString, 10); });

    RequireThrows([&] { WKBundleSetPopupBlockingEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetPopupBlockingEnabled(dummyString, true); });

    RequireThrows([&] { WKBundleSetSpatialNavigationEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleSetSpatialNavigationEnabled(dummyString, true); });

    WKRelease(dummyString);
    return 0;
}
