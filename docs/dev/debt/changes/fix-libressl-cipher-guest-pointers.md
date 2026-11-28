# libSceLibreSsl

## add Functional

- [SSL_CIPHER_get_bits, SSL_CIPHER_get_name and SSL_CIPHER_get_version](../../../core/libs/prx/libSceLibreSsl/src/Cipher.cpp) (libSceLibreSsl) validate guest `cipher` and `algorithmBits` pointers with `GuestCopyIn`/`GuestCopyOut` and throw `NotImplemented` via `NotImplemented_nid_no_patch` while preserving `errno` on invalid guest addresses instead of dereferencing them on the host.
