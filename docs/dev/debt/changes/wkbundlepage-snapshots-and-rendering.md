# libSceNKWebKit

## add Silent stubs

- [WKBundlePage snapshots and rendering controls](../../../core/libs/prx/libSceNKWebKit/src/BundlePageSnapshotsAndRendering.cpp) (libSceNKWebKit) - `WKBundlePageCreateSnapshotInViewCoordinates`, `WKBundlePageCreateSnapshotInDocumentCoordinates`, `WKBundlePageCreateScaledSnapshotInDocumentCoordinates` and `WKBundlePageCreateSnapshotWithOptions` construct `Image` objects scaled by the requested rect dimensions, `WKBundlePageCopyRenderTreeExternalRepresentation` and `WKBundlePageCopyTrackedRepaintRects` return the configured representation and repaint rects, `WKBundlePageForceRepaint` increments repaint operations, and `WKBundlePageExtendIncrementalRenderingSuppression` and `WKBundlePageStopExtendingIncrementalRenderingSuppression` manage suppression tokens; none of these trigger live WebCore compositing or Cairo/GPU snapshot rendering until a backing surface pipeline is wired.

## remove `cZrLCe6EKEA`

## remove `NkqH32IMG2A`

## remove `RQ3j4p7RBcw`

## remove `Kqv-zZ9xSIU`

## remove `TNbQMd1lZxU`

## remove `GEUAFDNuhBc`

## remove `jaQ3h3J4zd4`

## remove `QzaA1qde-1c`

## remove `Xzpfd5uJNNk`
