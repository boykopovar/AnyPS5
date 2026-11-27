# libSceNKWebKit

## add Functional

- [WebCoreColorSerialization guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/WebCoreColorSerialization.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::serializationForHTML` is unmeasured; the export reads guest `WebCore::Color` storage through `GuestCopyIn_nid_no_patch` and calls `NotImplementedAt_nid_no_patch` on unmapped or unreadable guest memory. Closes when: console behavior for invalid guest pointers in `WebCore::serializationForHTML` is verified on hardware.
