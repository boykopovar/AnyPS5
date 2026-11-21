# libSceLibreSsl3

## add Functional

- [ASN1_TIME_diff guest pointer checks](../../../core/libs/prx/libSceLibreSsl3/src/Time.cpp) (libSceLibreSsl3) - non-null guest `Asn1String` inputs, their byte slices, and `days`/`seconds` outputs are checked with `GuestWritable_nid_no_patch` and copied through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting faulted pointers via `NotImplemented_nid_no_patch(__func__)` before mutating outputs; console behavior on faulted pointers is unverified.
