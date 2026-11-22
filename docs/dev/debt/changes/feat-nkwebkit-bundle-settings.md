# libSceNKWebKit

## add Functional

- [Injected bundle configuration accessors](../../../core/libs/prx/libSceNKWebKit/src/Bundle.cpp) (libSceNKWebKit) - `WKBundleSetClient`, `WKBundleSetFrameFlatteningEnabled`, `WKBundleSetSpatialNavigationEnabled`, `WKBundleSetAllowFileAccessFromFileURLs`, `WKBundleSetJavaScriptCanAccessClipboard`, `WKBundleSetAllowStorageAccessFromFileURLS`, `WKBundleSetAllowUniversalAccessFromFileURLs`, `WKBundleSetPopupBlockingEnabled`, `WKBundleSetAsynchronousSpellCheckingEnabled`, `WKBundleSetAuthorAndUserStylesEnabled`, `WKBundleSetMinimumLogicalFontSize`, `WKBundleSetDatabaseQuota`, and `WKBundleSetUserStyleSheetLocation` configure injected bundle properties and retained stylesheet location in memory. Null or mismatched bundle references throw `std::invalid_argument`.
