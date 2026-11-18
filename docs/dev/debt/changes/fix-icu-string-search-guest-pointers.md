# libSceIcu

## add Functional

- [usearch_open guest status checks](../../../core/libs/prx/libSceIcu/StringSearch.cpp) (libSceIcu) - reads and updates guest `UErrorCode` through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting faulted guest status pointers via `NotImplemented_nid_no_patch(__func__)`; console behavior on faulted pointers is unverified.
