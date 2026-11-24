# libSceNKWebKit

## add Silent stubs

- [WKBundlePage overhang image controls](../../../core/libs/prx/libSceNKWebKit/src/BundlePageOverhang.cpp) (libSceNKWebKit) - `WKBundlePageSetTopOverhangImage` and `WKBundlePageSetBottomOverhangImage` store the retained image reference on the host `BundlePage` object without forwarding to layer rendering or compositor trees; null clears the image.
