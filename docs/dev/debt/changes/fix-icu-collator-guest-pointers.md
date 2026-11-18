# libSceIcu

## add Functional

- [ucol_getAttribute, ucol_getReorderCodes, ucol_getTailoredSet, ucol_open, and ucol_setAttribute](../../../core/libs/prx/libSceIcu/Collator.cpp) read guest `UErrorCode* status` values via `GuestCopyIn_nid_no_patch` while preserving `errno`, rejecting unreadable status pointers via `NotImplemented_nid_no_patch(__func__)`.
