# libSceNKWebKit

## add Functional

- [WebCore::Region::Shape and Region stream guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/Region.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::Region`, `WebCore::Region::Shape::translate`, and `WebCore::operator<<(WTF::TextStream&, const WebCore::Region&)` is unmeasured; exports validate required guest reads (`GuestArenaPrepareHostRead_nid_no_patch` + `GuestReadable_nid_no_patch`), copy guest `Region`, `Shape`, and vector storage via `GuestCopyIn_nid_no_patch`, guard guest writes with active `GuestArena::HostWrite` scopes before `GuestCopyOut_nid_no_patch`, and report failed required guest-memory accesses through `NotImplementedAt_nid_no_patch`. Closes when: console behavior for invalid guest pointers in `Region` exports, shape translation, and stream output is verified on hardware.
