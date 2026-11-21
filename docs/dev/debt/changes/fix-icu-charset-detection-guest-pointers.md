# libSceIcu

## add Functional

- [CharsetDetection guest pointer checks](../../../core/libs/prx/libSceIcu/CharsetDetection.cpp) (libSceIcu) - `error` and `found` guest pointers are checked with `GuestCopyIn_nid_no_patch`, `GuestWritable_nid_no_patch`, and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting faulted pointers via `NotImplemented_nid_no_patch(__func__)`; console behavior on faulted pointers is unverified.
