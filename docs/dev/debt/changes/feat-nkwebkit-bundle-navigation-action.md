# libSceNKWebKit

## add Functional

- [Injected bundle navigation action accessors](../../../core/libs/prx/libSceNKWebKit/src/BundleNavigationAction.cpp) (libSceNKWebKit) - `WKBundleNavigationActionGetTypeID`, `WKBundleNavigationActionGetNavigationType`, `WKBundleNavigationActionGetEventModifiers`, `WKBundleNavigationActionGetEventMouseButton`, `WKBundleNavigationActionCopyHitTestResult`, `WKBundleNavigationActionCopyFormElement`, `WKBundleNavigationActionCopyDownloadAttribute`, `WKBundleNavigationActionGetShouldOpenExternalURLs` and `WKBundleNavigationActionGetShouldTryAppLinks` inspect navigation action properties and retained subobjects. Unset handles return null; unset download attribute returns empty string. Null or mismatched references throw `std::invalid_argument`.
