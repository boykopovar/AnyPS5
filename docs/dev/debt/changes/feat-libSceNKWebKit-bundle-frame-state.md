# libSceNKWebKit

## add Functional

- [WKBundleFrame state getters](../../../core/libs/prx/libSceNKWebKit/src/BundleFrameState.cpp) (libSceNKWebKit) - `WKBundleFrameIsMainFrame`, `WKBundleFrameGetParentFrame`, `WKBundleFrameCopyChildFrames`, `WKBundleFrameCopyName`, `WKBundleFrameCopyURL`, `WKBundleFrameCopyProvisionalURL`, `WKBundleFrameCopySecurityOrigin` and `WKBundleFrameGetFrameLoadState` read host `BundleFrame` state. No export creates a `WKBundleFrame` or fills it from a loaded document yet, so they return the defaults (not the main frame, no parent, no children, an empty name, null URLs and security origin, load state 2) until frame loading is connected. `parentFrame` and `childFrames` both retain strongly, so filling both forms a retain cycle where WebKit's parent link is weak; the frame-loading code that fills them has to break it. Signatures follow the pinned WebKit's `WKBundleFrame.h`; they are not compared with a console module. Closes when: compared with a console module.

## remove `WKBundleFrameIsMainFrame` | `GJ+VEmIdxUQ`

## remove `WKBundleFrameGetParentFrame` | `v2hk1-AZ06A`

## remove `WKBundleFrameCopyChildFrames` | `OfwgHjJZyGo`

## remove `WKBundleFrameCopyName` | `HfwSYBY9ybg`

## remove `WKBundleFrameCopyURL` | `xoaQMJQ9JFg`

## remove `WKBundleFrameCopyProvisionalURL` | `pK81BVjswyQ`

## remove `WKBundleFrameCopySecurityOrigin` | `-s98d979pPQ`

## remove `WKBundleFrameGetFrameLoadState` | `jB0PrHhQOZ8`
