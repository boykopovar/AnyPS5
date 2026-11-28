# libSceLibreSsl3

## add Unknown function info

- [EVP_PKEY_sign, EVP_PKEY_encrypt and EVP_PKEY_decrypt output length](../../../core/libs/prx/libSceLibreSsl3/src/PkeyContext.cpp) (libSceLibreSsl3) - the guest output length is read once with `GuestCopyIn_nid_no_patch` and written back with `GuestCopyOut_nid_no_patch` only when the operation changes it; an unreadable or unwritable length throws the invalid-argument exception a null length throws, with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. The input and output buffers are still accessed in place, and `EVP_PKEY_derive` still reads its length in place.
