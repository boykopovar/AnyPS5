# libSceNKWebKit

## add Functional

- [Bundle back/forward list and items](../../../core/libs/prx/libSceNKWebKit/src/BundleBackForwardList.cpp) (libSceNKWebKit) - `WKBundleBackForwardListClear`, `WKBundleBackForwardListCopyItemAtIndex`, `WKBundleBackForwardListGetBackListCount`, `WKBundleBackForwardListGetForwardListCount`, `WKBundleBackForwardListItemCopyChildren`, `WKBundleBackForwardListItemCopyOriginalURL`, `WKBundleBackForwardListItemCopyTarget`, `WKBundleBackForwardListItemCopyTitle`, `WKBundleBackForwardListItemCopyURL`, `WKBundleBackForwardListItemHasCachedPageExpired`, `WKBundleBackForwardListItemIsInBackForwardCache`, `WKBundleBackForwardListItemIsSame` and `WKBundleBackForwardListItemIsTargetItem` follow WebKit injected bundle API. Accessors throw `std::invalid_argument` on null or wrong object type. Console ABI is unverified.
