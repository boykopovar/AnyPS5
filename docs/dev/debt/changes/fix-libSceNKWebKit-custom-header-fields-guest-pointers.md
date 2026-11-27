# libSceNKWebKit

## add Functional

- [CustomHeaderFields guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/CustomHeaderFields.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::CustomHeaderFields` constructors, copy assignment, destructor, and `thirdPartyDomainsMatch` is unmeasured; the exports validate readable and writable guest storage under `GuestArena::HostWrite` and copy guest `CustomHeaderFields`, vector elements, `WTF::URL`, `WTF::StringImpl`, and character buffers through `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or unwritable guest memory without partial writes or leaked references. Closes when: console behavior for invalid guest pointers in `WebCore::CustomHeaderFields` exports is verified on hardware.
