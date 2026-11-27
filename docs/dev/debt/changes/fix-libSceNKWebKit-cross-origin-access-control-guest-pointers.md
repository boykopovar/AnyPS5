# libSceNKWebKit

## add Functional

- [CrossOriginAccessControl guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginAccessControl.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::CrossOriginAccessControlCheckDisabler`, `WebCore::passesAccessControlCheck`, and `WebCore::validatePreflightResponse` exports is unmeasured; the exports validate readable and writable guest storage under `GuestArena::HostWrite` and copy guest `CrossOriginAccessControlCheckDisabler`, vtable, `ResourceResponseStatusLayout`, `SecurityOrigin`, and `LegacyResourceRequestBase` fields through `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or unwritable guest memory. Closes when: console behavior for invalid guest pointers in `CrossOriginAccessControl` exports is verified on hardware.
