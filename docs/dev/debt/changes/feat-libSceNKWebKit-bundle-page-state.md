# libSceNKWebKit

## add Functional

- [WKBundlePage state getters](../../../core/libs/prx/libSceNKWebKit/src/BundlePageState.cpp) (libSceNKWebKit) - `WKBundlePageGetBackingScaleFactor`, `WKBundlePageIsControlledByAutomation`, `WKBundlePageIsSuspended`, `WKBundlePageIsUsingEphemeralSession`, `WKBundlePageGetRenderTreeSize` and `WKBundlePageHasComposition` read host `BundlePage` state. No export creates a `WKBundlePage` or drives a web page behind it yet, so they return the defaults (scale factor 1, not controlled by automation, not suspended, persistent session, render tree size 0, no composition) until page loading is connected. The defaults follow the pinned WebKit's `WKBundlePage.cpp` and `WebPage`; they are not compared with a console module. Closes when: compared with a console module.

## remove `WKBundlePageGetBackingScaleFactor` | `PS5n3xxFDXQ`

## remove `WKBundlePageIsControlledByAutomation` | `PqTXHcj8G7U`

## remove `WKBundlePageIsSuspended` | `NVjEy07nN2o`

## remove `WKBundlePageIsUsingEphemeralSession` | `glsQJ5HCC-U`

## remove `WKBundlePageGetRenderTreeSize` | `U2LcSWAxv6I`

## remove `WKBundlePageHasComposition` | `v08CAVZPz-0`
