# libSceIcu

## add Functional

- [ureldatefmt_format and ureldatefmt_open](../../../core/libs/prx/libSceIcu/RelativeDateTimeFormatter.cpp) read and update guest `UErrorCode* status` pointers via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable status pointers via `NotImplemented_nid_no_patch(__func__)`.
