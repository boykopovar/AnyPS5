# libSceIcu

## add Functional

- [Bidi guest pointer checks](../../../core/libs/prx/libSceIcu/Bidi.cpp) (libSceIcu) - `error`, output, and `ubidi_reorderVisual` level/map guest pointers are checked with `GuestCopyIn_nid_no_patch`, `GuestWritable_nid_no_patch`, and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting faulted or under-mapped pointers via `NotImplemented_nid_no_patch(__func__)`; console behavior on faulted pointers is unverified.
