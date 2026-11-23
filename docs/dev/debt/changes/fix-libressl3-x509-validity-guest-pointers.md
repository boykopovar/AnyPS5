# libSceLibreSsl3

## add Functional

- [X509_getm_notBefore, X509_getm_notAfter, X509_get0_notBefore, X509_get0_notAfter, and X509_get0_signature guest pointer checks](../../../core/libs/prx/libSceLibreSsl3/src/X509Validity.cpp) (libSceLibreSsl3) - non-null guest `X509`, `X509_CINF`, `X509_VAL`, and output pointer accesses go through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rolling back partial output writes and rejecting faulted guest pointers via `NotImplemented_nid_no_patch(__func__)`; console behavior on faulted pointers is unverified.
