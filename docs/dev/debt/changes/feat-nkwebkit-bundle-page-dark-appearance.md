# libSceNKWebKit

## remove WKBundlePageSetUseDarkAppearance

## add Functional

- [WKBundlePage dark appearance accessors](../../../core/libs/prx/libSceNKWebKit/src/BundlePageState.cpp) (libSceNKWebKit) - `WKBundlePageIsUsingDarkAppearance` and `WKBundlePageSetUseDarkAppearance` read and set `BundlePage::isUsingDarkAppearance` (default false), matching WebKit C API semantics and throwing `std::invalid_argument` on null or invalid references. Unverified on console.
