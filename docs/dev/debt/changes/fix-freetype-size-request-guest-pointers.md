# libSceFreeType

## add Functional

- [FT_Request_Size guest pointer checks](../../../core/libs/prx/libSceFreeType/src/SizeRequest.cpp) (libSceFreeType) - non-null guest `face` internal pointer and `request` struct are read through `GuestCopyIn_nid_no_patch` while preserving `errno`, rejecting unreadable guest pointers via `NotImplemented_nid_no_patch(__func__)`; console behavior on faulted pointers is unverified.
