# libSceLibreSsl3

## add Functional

- [ASN1_TIME_diff](../../../core/libs/prx/libSceLibreSsl3/src/Time.cpp) copies non-null guest `Asn1String` inputs, their byte slices, and `days`/`seconds` outputs through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, throwing via `NotImplemented_nid_no_patch` on unreadable or unwritable pointers (restoring `days` if writing `seconds` faults) rather than dereferencing faulted guest pointers on the host; console behavior on faulted pointers is unverified.
