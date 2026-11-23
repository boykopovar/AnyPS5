# libSceIcu

## add Functional

- [u_getBinaryPropertySet](../../../core/libs/prx/libSceIcu/Set.cpp) reads and updates the guest `UErrorCode* error` pointer via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting an unreadable or unwritable error pointer via `NotImplemented_nid_no_patch(__func__)`.
