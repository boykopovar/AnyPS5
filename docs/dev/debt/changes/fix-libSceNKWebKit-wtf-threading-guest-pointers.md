# libSceNKWebKit

## add Functional

- [WTF::Mutex and WTF::ThreadCondition guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/WtfThreading.cpp) (libSceNKWebKit) - Console fault behavior for invalid or misaligned guest pointers in `WTF::Mutex` and `WTF::ThreadCondition` exports is unmeasured; mutating exports validate alignment, readability, and writability under an active `GuestArena::HostWrite` guard (and `Mutex` construction publishes the initialized handle via `GuestCopyOut_nid_no_patch`), while read-only handle reads copy in via `GuestCopyIn_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on invalid guest memory. Closes when: console behavior for invalid guest pointers in `WtfThreading` exports is verified on hardware.
