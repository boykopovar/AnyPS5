# libSceNKWebKit

## add Functional

- [CrossThreadTaskHandler guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/CrossThreadTaskHandler.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WTF::CrossThreadTaskHandler` constructors, destructors, `postTask`, `postTaskReply`, and `kill` is unmeasured; the exports preflight readable and writable guest storage under `GuestArena::HostWrite`, copy handler fields and `VoidFunction` wrappers through `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` while preserving `unused[96]`, verify live internal state registry membership and callable vtables before mutating state or queueing work, and call `NotImplementedAt_nid_no_patch` on unmapped, unreadable, unwritable, forged-state, or invalid-callable guest pointers. Closes when: console behavior for invalid guest pointers in `WTF::CrossThreadTaskHandler` exports is verified on hardware.
