# libSceLibreSsl3

## add Functional

- [X509_cmp_current_time guest pointer checks](../../../core/libs/prx/libSceLibreSsl3/src/X509CmpTime.cpp) (libSceLibreSsl3) - `ASN1_STRING` struct and timestamp bytes are copied through `GuestCopyIn_nid_no_patch` while preserving `errno`, rejecting faulted guest pointers via `NotImplemented_nid_no_patch(__func__)`; console behavior on faulted pointers is unverified.
