# libSceLibreSsl3

## change [Additional exports]

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 50 still-unimplemented callable exports: 49 named and 1 known only by NID. These entries throw. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. The fourteen libtls lifecycle/config exports are implemented in `LibTls.cpp`; title compatibility beyond preconnection behavior remains unverified.

## change `4pEl-5DmrrM`

| `ERR_print_errors_fp` | `4pEl-5DmrrM` |

## change `qGsc6uT6p1E`

| `ECDSA_do_verify` | `qGsc6uT6p1E` |

## change [libSceLibreSsl3 pending exports]

- [libSceLibreSsl3 pending exports](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) - 6 named callable exports, excluding `EVP_PKEY_new_mac_key`, `PKCS7_free`, `d2i_ECParameters`, `ERR_print_errors_cb`, `OCSP_resp_find_status`, `ASN1_OBJECT_free`, `CRYPTO_memcmp`, `EVP_PKEY_CTX_ctrl`, `EVP_PKEY_CTX_new`, `EVP_PKEY_decrypt`, `EVP_PKEY_decrypt_init`, `EVP_PKEY_encrypt`, `EVP_PKEY_encrypt_init`, `RSA_new`, `RSA_check_key`, `RSA_padding_add_PKCS1_PSS_mgf1`, `EVP_DigestSignInit`, `EVP_DigestSignFinal`, `EVP_SignFinal`, `EVP_VerifyFinal`, `MD5`, `SHA1`, `d2i_ASN1_INTEGER`, `EVP_PKEY_up_ref`, `EC_KEY_up_ref`, `RSA_up_ref`, `SSL_CTX_up_ref`, `SSL_up_ref`, `SSL_SESSION_up_ref`, `SSL_CTX_set_min_proto_version`, `SSL_CTX_set_max_proto_version`, `SSL_get_client_random`, `X509_up_ref`, `X509_get_version`, `EVP_PKEY_get0_RSA`, `EVP_PKEY_get0_DSA`, `EVP_PKEY_get0_DH`, `EVP_MD_CTX_new`, `EVP_MD_CTX_free`, `EVP_DigestInit`, `EVP_sha1`, `EVP_sha224`, `ERR_load_crypto_strings`, `ERR_peek_last_error`, `OpenSSL_version`, `X509_PUBKEY_get0_param`, `OBJ_cmp`, `ASN1_STRING_get0_data`, `ASN1_INTEGER_free`, `X509_get0_extensions`, `X509_get_ext_count`, `X509_get_ext`, `EVP_PKEY_size`, `X509_getm_notBefore`, `X509_getm_notAfter`, `X509_get0_notBefore`, `X509_get0_notAfter`, `X509_get0_signature`, `RSA_get0_key`, `DH_get0_pqg`, `DH_get0_key`, `DSA_get0_pqg`, `DSA_get0_key`, `BIO_up_ref`, `X509_STORE_CTX_free`, `X509_STORE_CTX_new`, `X509_STORE_up_ref`, `OCSP_CERTID_free`, `OCSP_cert_to_id`, `d2i_RSA_PUBKEY`, `PEM_read_RSAPrivateKey` and `d2i_PKCS7`, remain unimplemented from the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibreSsl3.c), cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

## change [Native PKEY forwarding gaps]

- [Native PKEY forwarding gaps](../../../core/libs/prx/libSceLibreSsl3/ParentExports.cpp) - `X509_get_pubkey` still forwards to a legacy throwing stub. `SSL_get_privatekey` forwards to the parent library and returns its legacy PKEY. The consumers `EVP_PKEY_get1_EC_KEY`, `EVP_DigestVerifyInit`, `SSL_CTX_use_PrivateKey`, `SSL_CTX_use_PrivateKey_ASN1`, `SSL_CTX_use_PrivateKey_file` and `SSL_use_PrivateKey_ASN1` do too. Each needs a native wrapper before its legacy implementation can run, to prevent mixing the incompatible PKEY layouts.

## change [Parent export forwarding]

- [Parent export forwarding](../../../core/libs/prx/libSceLibreSsl3/ParentExports.cpp) exposes 382 parent-library exports on Windows through IAT tail jumps without changing their ABI. Non-Windows hosts use their existing dependency lookup. Static COFF inspection verifies names, parent imports and tail jumps; Windows loading and runtime behavior remain unverified. Parent functions retain their existing implementation limitations.
