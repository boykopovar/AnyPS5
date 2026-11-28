# libSceNKWebKit

## add Functional

- [CustomHeaderFields guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/CustomHeaderFields.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::CustomHeaderFields` constructors, copy assignment, destructor, and `thirdPartyDomainsMatch` is unmeasured; the exports preflight readable and writable guest storage under `GuestArena::HostWrite`, plan aliased string refcount releases (including `SharedSubstring` owners), copy guest `CustomHeaderFields`, vector elements, `WTF::URL`, `WTF::StringImpl`, and character buffers through `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`, roll back cloned retains and allocations on error, and call `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or unwritable guest memory. Closes when: console behavior for invalid guest pointers in `WebCore::CustomHeaderFields` exports is verified on hardware.
