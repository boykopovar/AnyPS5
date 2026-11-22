# libSceNKWebKit

## add Silent stubs

- [WKBundleFrame content, resource and style queries](../../../core/libs/prx/libSceNKWebKit/src/BundleFrameContent.cpp) (libSceNKWebKit) - `WKBundleFrameCopyCounterValue`, `WKBundleFrameCopyInnerText`, `WKBundleFrameCopyLayerTreeAsText`, `WKBundleFrameCopyMIMETypeForResourceWithURL`, `WKBundleFrameCopySuggestedFilenameForResourceWithURL` and `WKBundleFrameGetDocumentBackgroundColor` read optional strings and a color stored on `BundleFrame`; no export or host code populates them yet because no frame document, renderer or resource loader exists, so they return an empty string, or false with white (1,1,1,1) for the background color, until frames model document contents.

## remove `rF5BMbyEszI`

## remove `ZD2MP6Vzxu4`

## remove `lDHhVVXL4z8`

## remove `9j3jntJ9Nfw`

## remove `kGAitTwtXq0`

## remove `qXfTToOHVgs`
