# libSceNKWebKit

## add Functional

- [WKBundleFrame content guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/BundleFrameContent.cpp) - console behaviour on invalid, unmapped, or read-only guest pointers passed to `WKBundleFrameCopyCounterValue`, `WKBundleFrameCopyInnerText`, `WKBundleFrameCopyLayerTreeAsText`, `WKBundleFrameCopyMIMETypeForResourceWithURL`, `WKBundleFrameCopySuggestedFilenameForResourceWithURL`, and `WKBundleFrameGetDocumentBackgroundColor` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null/wrong-type references) without mutating guest color outputs. Closes when: console behaviour on invalid guest pointers in these exports is measured.
