# libSceLibreSsl3

## change [Additional exports]

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 50 still-unimplemented callable exports: 49 named and 1 known only by NID. These entries throw. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. The fourteen libtls lifecycle/config exports are implemented in `LibTls.cpp`; title compatibility beyond preconnection behavior remains unverified.

## remove `4pEl-5DmrrM`

## change `qGsc6uT6p1E`

| `ECDSA_do_verify` | `qGsc6uT6p1E` |

## remove [libSceLibreSsl3 pending exports]

## change [Native PKEY forwarding gaps]

- [Native PKEY forwarding gaps](../../../core/libs/prx/libSceLibreSsl3/ParentExports.cpp) - `X509_get_pubkey` still forwards to a legacy throwing stub. `SSL_get_privatekey` forwards to the parent library and returns its legacy PKEY. The consumers `EVP_PKEY_get1_EC_KEY`, `EVP_DigestVerifyInit`, `SSL_CTX_use_PrivateKey`, `SSL_CTX_use_PrivateKey_ASN1`, `SSL_CTX_use_PrivateKey_file` and `SSL_use_PrivateKey_ASN1` do too. Each needs a native wrapper before its legacy implementation can run, to prevent mixing the incompatible PKEY layouts.

## change [Parent export forwarding]

- [Parent export forwarding](../../../core/libs/prx/libSceLibreSsl3/ParentExports.cpp) exposes 382 parent-library exports on Windows through IAT tail jumps without changing their ABI. Non-Windows hosts use their existing dependency lookup. Static COFF inspection verifies names, parent imports and tail jumps; Windows loading and runtime behavior remain unverified. Parent functions retain their existing implementation limitations.
