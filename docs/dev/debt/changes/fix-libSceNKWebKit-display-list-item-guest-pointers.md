# libSceNKWebKit

## add Functional

- [DisplayList::Item and DrawingItem guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/DisplayListItem.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::DisplayList::Item` and `WebCore::DisplayList::DrawingItem` constructors and destructors is unmeasured; the exports validate writable object storage under `GuestArena::HostWrite` and write initialized fields through `GuestCopyOut_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped or unwritable guest memory without modifying untouched padding bytes or freeing invalid pointers. Closes when: console behavior for invalid guest pointers in `DisplayList::Item` and `DisplayList::DrawingItem` exports is verified on hardware.
