# libSceNKWebKit

## add Functional

- [TrackPrivateBase constructor guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/TrackPrivateBase.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::TrackPrivateBase::TrackPrivateBase()` is unmeasured; the constructor validates writable storage for the 12-byte vptr and refCount prefix under `GuestArena::HostWrite` and writes it through `GuestCopyOut_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped or unwritable guest memory without modifying trailing padding bytes or partially writing split pages. Closes when: console behavior for invalid guest pointers in `WebCore::TrackPrivateBase::TrackPrivateBase()` is verified on hardware.
