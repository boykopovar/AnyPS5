# libSceNKWebKit

## add Functional

- [WebCore::Region::Shape and Region stream guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/Region.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::Region`, `WebCore::Region::Shape::translate`, and `WebCore::operator<<(WTF::TextStream&, const WebCore::Region&)` is unmeasured; exports validate guest readability (`GuestArenaPrepareHostRead_nid_no_patch` + `GuestReadable_nid_no_patch`), copy guest `Region`, `Shape`, and vector storage via `GuestCopyIn_nid_no_patch`, guard guest writes with active `GuestArena::HostWrite` scopes before `GuestCopyOut_nid_no_patch`, and call `NotImplementedAt_nid_no_patch` on invalid guest memory. Closes when: console behavior for invalid guest pointers in `Region` exports is verified on hardware.
