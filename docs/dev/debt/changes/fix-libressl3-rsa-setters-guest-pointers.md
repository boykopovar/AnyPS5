# libSceLibreSsl3

## add Functional

- [BN_clear_free, RSA_set0_key, RSA_set0_factors, and RSA_set0_crt_params guest pointer checks](../../../core/libs/prx/libSceLibreSsl3/src/RsaSetters.cpp) (libSceLibreSsl3) - non-null guest `BIGNUM`, `RSA`, and non-static limb buffer accesses go through `GuestCopyIn_nid_no_patch`, `GuestWritable_nid_no_patch`, and `GuestCopyOut_nid_no_patch` while preserving `errno`, validating all replaced slots and key writability before freeing or updating any field and rejecting faulted guest pointers via `NotImplemented_nid_no_patch(__func__)`; console behavior on faulted pointers is unverified.
