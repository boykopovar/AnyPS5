# libSceNKWebKit

## add Functional

- [CrossThreadTaskHandler guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/CrossThreadTaskHandler.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WTF::CrossThreadTaskHandler` exports is unmeasured; constructors and destructors preflight writable `CrossThreadTaskHandler` storage under `GuestArena::HostWrite` while preserving `unused[96]`, `postTask`/`postTaskReply` preflight writable `VoidFunction` storage and readable callable vtables, and all methods copy handler fields through `GuestCopyIn_nid_no_patch`/`GuestCopyOut_nid_no_patch` and check owner-bound live state membership before throwing `NotImplementedAt_nid_no_patch`. Closes when: console behavior for invalid guest pointers in `WTF::CrossThreadTaskHandler` exports is verified on hardware.
