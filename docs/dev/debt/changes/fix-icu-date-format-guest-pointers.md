# libSceIcu

## add Functional

- [udat_format, udat_formatForFields, udat_open, and udat_toPattern](../../../core/libs/prx/libSceIcu/DateFormat.cpp) read and update guest `UErrorCode* status` values via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable status pointers via `NotImplemented_nid_no_patch(__func__)`.
