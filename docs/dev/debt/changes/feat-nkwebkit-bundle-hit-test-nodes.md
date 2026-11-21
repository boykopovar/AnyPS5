# libSceNKWebKit

## change [WKBundleHitTestResult property getters]

- [WKBundleHitTestResult property getters and hit testing](../../../core/libs/prx/libSceNKWebKit/src/BundleHitTestResult.cpp) (libSceNKWebKit) - WKBundleFrameCreateHitTestResult, WKBundleHitTestResultCopyAbsoluteImageURL, WKBundleHitTestResultCopyAbsoluteLinkURL, WKBundleHitTestResultCopyAbsoluteMediaURL, WKBundleHitTestResultCopyAbsolutePDFURL, WKBundleHitTestResultCopyLinkLabel, WKBundleHitTestResultCopyLinkTitle, WKBundleHitTestResultCopyLinkSuggestedFilename, WKBundleHitTestResultCopyNodeHandle, WKBundleHitTestResultCopyURLElementHandle, WKBundleHitTestResultGetImageRect, WKBundleHitTestResultGetIsSelected, WKBundleHitTestResultGetMediaType, WKBundleHitTestResultIsDownloadableMedia, WKBundleHitTestResultMediaHasAudio and WKBundleHitTestResultMediaIsInFullscreen create and inspect hit test results, associating the frame's node handle and coordinates in host memory; live layout tree hit testing remains unmodeled.

## remove `WKBundleFrameCreateHitTestResult`

## remove `WKBundleHitTestResultCopyNodeHandle`

## remove `WKBundleHitTestResultCopyURLElementHandle`

## remove `WKBundleHitTestResultGetImageRect`
