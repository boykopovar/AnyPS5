# libSceNKWebKit

## add Functional

- [WKBundlePage zoom factor controls](../../../core/libs/prx/libSceNKWebKit/src/PageZoom.cpp) (libSceNKWebKit) - `WKBundlePageGetPageZoomFactor`, `WKBundlePageGetTextZoomFactor`, `WKBundlePageSetPageZoomFactor`, and `WKBundlePageSetTextZoomFactor` query and update page and text zoom factors on `BundlePage` instances, ignoring non-positive and non-finite values. Null or invalid object references throw `std::invalid_argument`.
