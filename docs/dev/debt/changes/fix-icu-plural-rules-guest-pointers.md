# libSceIcu

## add Functional

- [uplrules_openForType](../../../core/libs/prx/libSceIcu/PluralRules.cpp) reads and updates guest `UErrorCode* status` pointers via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable status pointers via `NotImplemented_nid_no_patch(__func__)`.
