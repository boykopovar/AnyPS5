# libSceIcu

## add Functional

- [uldn_openForContext](../../../core/libs/prx/libSceIcu/LocaleDisplayNames.cpp) reads guest `UErrorCode* status` values via `GuestCopyIn_nid_no_patch` while preserving `errno`, rejecting unreadable status pointers via `NotImplemented_nid_no_patch(__func__)`.
