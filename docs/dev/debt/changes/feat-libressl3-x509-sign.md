# libSceLibreSsl3

## remove `TpBvY92mrSM`

## add Functional

- [X509_sign](../../../core/libs/prx/libSceLibreSsl3/src/X509Sign.cpp) (libSceLibreSsl3) - no console check. Follows LibreSSL 4.0.0 `x_all.c` and `asn1_item.c`: it signs the cached certificate-info encoding, the TBS that `i2d_X509` reads, and sets both algorithm identifiers from the first object-table entry whose `OBJ_find_sigid_algs` digest and key NIDs match the digest and `EVP_PKEY_base_id`, each with its own NULL parameter for RSA keys; the signature goes into the certificate bit string with the no-unused-bits flag and is returned, signed through `EVP_SignFinal`. LibreSSL re-encodes the TBS rather than reading the cache, so this covers re-signing an already encoded certificate only: a modified, absent or differently signed certificate info throws `NotImplemented_nid_no_patch`, and no title is measured; RSA-PSS, EC, Ed25519, HMAC, a null digest and normalised key aliases throw or pick the aliased NID. Null arguments return 0. Closes when: compared with the console.
