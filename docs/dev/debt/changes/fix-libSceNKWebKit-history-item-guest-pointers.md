# libSceNKWebKit

## add Functional

- [WebCore::HistoryItem guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/HistoryItem.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::HistoryItem` exports is unmeasured; getters validate guest readability and copy nested fields via `GuestCopyIn_nid_no_patch`, while constructors and setters validate writable guest storage under active `GuestArena::HostWrite` guards, plan alias-aware `WTF::StringImpl` retains and releases (rejecting terminating flags and cyclic/truncated substrings) before publishing via `GuestCopyOut_nid_no_patch`, and call `NotImplementedAt_nid_no_patch` on invalid guest memory. Closes when: console behavior for invalid guest pointers in `HistoryItem` exports is verified on hardware.
