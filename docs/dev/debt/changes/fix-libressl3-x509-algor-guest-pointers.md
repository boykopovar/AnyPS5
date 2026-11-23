# libSceLibreSsl3

## add Functional

- [X509_ALGOR_get0](../../../core/libs/prx/libSceLibreSsl3/src/X509Algor.cpp) copies guest `LibreSsl3X509Algor` and `LibreSsl3Asn1Type` structures and writes `outObject`, `outType` and `outValue` through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, throwing via `NotImplemented_nid_no_patch` (and restoring previously written outputs if a later output pointer faults) rather than dereferencing faulted guest pointers on the host; console behavior on faulted pointers is unverified.
