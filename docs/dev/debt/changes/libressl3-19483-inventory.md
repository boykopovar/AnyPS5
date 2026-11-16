# libSceLibreSsl3

## change [Additional exports]

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 49 still-unimplemented callable exports: 48 named and 1 known only by NID. These entries throw. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. The fourteen libtls lifecycle/config exports are implemented in `LibTls.cpp` and four client transport exports (`tls_connect_socket`, `tls_handshake`, `tls_read`, `tls_write`) in `LibTlsTransport.cpp`; title compatibility beyond tested client transport remains unverified.

## change `4pEl-5DmrrM`

| `ERR_print_errors_fp` | `4pEl-5DmrrM` |

## change `qGsc6uT6p1E`

| `ECDSA_do_verify` | `qGsc6uT6p1E` |

## change [libtls CA memory and verification settings]

- [libtls CA memory and verification settings](../../../core/libs/prx/libSceLibreSsl3/src/LibTls.cpp) - Five additional exports copy owned CA and certificate bytes, assign a protocol bitmask, and set client-certificate and hostname-verification flags. They follow [OpenBSD libtls `tls_config.c`](https://github.com/openbsd/src/blob/master/lib/libtls/tls_config.c). `tls_config_set_cert_mem` copies the bytes but does not derive the public key hash or parse the certificate as [tls_keypair.c](https://github.com/openbsd/src/blob/master/lib/libtls/tls_keypair.c) does, so malformed certificates are accepted and the error string stays null. Server transport (`tls_accept_socket`), client certificate parsing and cipher-list validation remain unimplemented; `tls_config_set_ciphers` still throws; `tls_config_parse_protocols` is implemented in `TlsProtocols.cpp`. Native PS5 ABI and option interoperability are unverified.

## change [libtls context and configuration lifecycle]

- [libtls context, configuration and client transport](../../../core/libs/prx/libSceLibreSsl3/src/LibTlsTransport.cpp) - Client transport (`tls_connect_socket`, `tls_handshake`, `tls_read`, `tls_write`, `tls_close`) runs over guest sockets via `mbedtls` using config/CA/hostname flags from `LibTls.cpp`, falling back to `CA_LIST.cer` when no CA memory is set. No PS5 ABI, error strings or `mbedtls` parity are console-verified. TLS 1.3 is disabled because `MbedConfig.hpp` undefines `MBEDTLS_SSL_PROTO_TLS1_3` (TLS 1.3-only config fails; default negotiates TLS 1.2), and `tls_accept_socket`, client certs/keys, cipher-list checks (`tls_config_set_ciphers`) and OCSP stapling remain unimplemented.

## remove `27Fuliv3Om8`

## remove `7RvDjfy4gUA`

## remove `ZERQN8ewpgk`

## remove `fZJr2OF+KGw`

## change [libSceLibreSsl3 pending exports]

- [libSceLibreSsl3 pending exports](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) - 13 named callable exports, excluding `d2i_ECParameters`, `ERR_print_errors_cb`, `OCSP_resp_find_status`, `ASN1_OBJECT_free`, `CRYPTO_memcmp`, `EVP_PKEY_CTX_ctrl`, `EVP_PKEY_CTX_new`, `EVP_PKEY_decrypt`, `EVP_PKEY_decrypt_init`, `EVP_PKEY_encrypt`, `EVP_PKEY_encrypt_init`, `RSA_new`, `RSA_check_key`, `RSA_padding_add_PKCS1_PSS_mgf1`, `EVP_DigestSignInit`, `EVP_DigestSignFinal`, `EVP_SignFinal`, `EVP_VerifyFinal`, `MD5`, `SHA1`, `d2i_ASN1_INTEGER`, `EVP_PKEY_up_ref`, `EC_KEY_up_ref`, `RSA_up_ref`, `SSL_CTX_up_ref`, `SSL_up_ref`, `SSL_SESSION_up_ref`, `SSL_CTX_set_min_proto_version`, `SSL_CTX_set_max_proto_version`, `SSL_get_client_random`, `X509_up_ref`, `X509_get_version`, `EVP_PKEY_get0_RSA`, `EVP_PKEY_get0_DSA`, `EVP_PKEY_get0_DH`, `EVP_MD_CTX_new`, `EVP_MD_CTX_free`, `EVP_DigestInit`, `EVP_sha1`, `EVP_sha224`, `ERR_load_crypto_strings`, `ERR_peek_last_error`, `OpenSSL_version`, `X509_PUBKEY_get0_param`, `OBJ_cmp`, `ASN1_STRING_get0_data`, `ASN1_INTEGER_free`, `X509_get0_extensions`, `X509_get_ext_count`, `X509_get_ext`, `EVP_PKEY_size`, `X509_getm_notBefore`, `X509_getm_notAfter`, `X509_get0_notBefore`, `X509_get0_notAfter`, `X509_get0_signature`, `RSA_get0_key`, `DH_get0_pqg`, `DH_get0_key`, `DSA_get0_pqg`, `DSA_get0_key`, `BIO_up_ref`, `X509_STORE_CTX_free`, `X509_STORE_CTX_new`, `X509_STORE_up_ref` and `OCSP_CERTID_free`, remain unimplemented from the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibreSsl3.c), cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.
