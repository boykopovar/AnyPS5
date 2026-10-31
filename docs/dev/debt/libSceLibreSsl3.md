# libSceLibreSsl3 technical debt

### Unknown function info

- [libSceLibreSsl3 pending exports](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) - 80 named callable exports, excluding `CRYPTO_memcmp`, `MD5`, `SHA1`, `d2i_ASN1_INTEGER`, `EVP_PKEY_up_ref`, `EC_KEY_up_ref` and `RSA_up_ref`, remain unimplemented from the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibreSsl3.c), cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

### Functional

- [ASN.1 integer decoding](../../../core/libs/prx/libSceLibreSsl3/src/IntegerDecoder.cpp) throws on rejected encodings/allocation failures instead of matching native NULL/error handling. Redundant-sign/empty-content rejection retains old output that the module frees and clears. The decoder does not populate the legacy queue; remaining LibreSSL3 ERR exports throw. [13.42 native-PC evidence](https://github.com/boykopovar/AnyPS5/pull/10948#issuecomment-6412137728), using a recording allocator, confirms tested replacement/free order, allocation cleanup and negative-length NULL returns. Nonnegative null input faults natively and throws here. Console/application-heap ABI and untested error-precedence/overflow combinations remain unverified. Contracts follow [LibreSSL 4.0.0](https://github.com/libressl/openbsd/tree/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1). Closes when: error/replacement semantics and console/allocator interoperability are verified.
- [CRYPTO_memcmp](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) follows the public [LibreSSL signature and equality contract](https://man.openbsd.org/CRYPTO_memcmp.3), without PS5 verification. The exact nonzero result for unequal buffers is unspecified by that contract. Content-independent execution of its volatile byte reads still depends on the compiler; inspect optimized code when changing toolchains. Closes when: measured on a PS5 and the supported toolchains' generated code is checked.
- [MD5 and SHA1](../../../core/libs/prx/libSceLibreSsl3/src/Digest.cpp) (libSceLibreSsl3) - follow LibreSSL's [md5.c, sha1.c](https://github.com/libressl/openbsd/tree/bc9483725c5815efb64b6d9acc52fc5fba139212/src/lib/libcrypto) and hash through libSceLibreSsl's `MD5_*` and `SHA1_*`. A null digest buffer, which LibreSSL 4.0.0 does not check, throws. Nothing is checked on the console
- [EVP_PKEY_up_ref, EC_KEY_up_ref and RSA_up_ref](../../../core/libs/prx/libSceLibreSsl3/src/Key.cpp) - console layouts and retain/release interoperability are unverified. The 32-byte EVP, 64-byte EC and 152-byte RSA layouts, with counters at offsets 4, 48 and 104, follow [LibreSSL 4.0.0](https://github.com/libressl/openbsd/tree/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto) `evp/evp_local.h`, `evp/p_lib.c`, `ec/ec_local.h`, `ec/ec_key.c`, `rsa/rsa_local.h` and `rsa/rsa_lib.c`; the RSA layout has no `engine` field, unlike libSceLibreSsl's `LibreSslRsa`. Atomic retains replace its internal mutex; future releases must synchronize with the same counters. With no key constructors/destructors in this module, tests use caller-owned objects without validating allocator or destruction compatibility. Null keys and signed-counter overflow, undefined upstream, throw without mutation. Closes when: console layouts and lifetime interoperability are measured.

- [Parent export forwarding](../../../core/libs/prx/libSceLibreSsl3/ParentExports.cpp) exposes 395 parent-library exports on Windows through IAT tail jumps without changing their ABI. Non-Windows hosts use their existing dependency lookup. Static COFF inspection verifies names, parent imports and tail jumps; Windows loading and runtime behavior remain unverified. Parent functions retain their existing implementation limitations.
- [sk_new, sk_push and sk_free](../../../core/libs/prx/libSceLibreSsl3/src/Stack.cpp) (libSceLibreSsl3) - follow [LibreSSL 4.0.0](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/stack/stack.c) `stack.c` with libSceLibreSsl's 32-byte `_STACK` layout, which `stack_local.h` keeps there. Stacks and element arrays come from the guest heap, so the forwarded `sk_pop_free` can release them. Nothing is checked on the console. Where LibreSSL faults or overflows, they throw: `sk_push` on a null stack, on a negative element count, on a null element array it does not regrow, or when the element array cannot double without overflowing `int`

### Additional callable export inventory

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 180 callable exports: 54 named and 126 known only by NID. Every entry throws. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. No functional implementation or title unblock is claimed.

| Name | NID |
| --- | --- |
| `EC_KEY_set_asn1_flag` | `++b0FizBrJk` |
| `BIO_set_init` | `+STC2BUY9vY` |
| `EC_POINT_get_affine_coordinates_GFp` | `+Tu0gXSas9s` |
| `tls_config_set_cert_mem` | `+bQfpiUHRVk` |
| `SSL_SESSION_get_protocol_version` | `0BW+8jTsY54` |
| `X509_NAME_add_entry_by_txt` | `0IxWzLyU8PI` |
| `SSL_SESSION_get_timeout` | `0JcHkrKJ9iw` |
| `BIO_meth_free` | `13BHMSblVyY` |
| `tls_handshake` | `27Fuliv3Om8` |
| `AUTHORITY_KEYID_free` | `2bIZM-OfE8Y` |
| `X509V3_EXT_d2i` | `2l+JidqxyZQ` |
| `TLS_method` | `31eWdro1jqs` |
| `RSA_bits` | `3KZ-L4y1gak` |
| `EVP_chacha20_poly1305` | `3m4CmAyOXwQ` |
| `BIO_clear_flags` | `3sypCuBDdQs` |
| `EVP_chacha20` | `3tGgkjBQ82A` |
| `ASN1_INTEGER_set` | `3v2Omd4sqcU` |
| `RSA_set0_factors` | `3wlK0tLONvk` |
| `X509_set_pubkey` | `409ieWBMPUk` |
| `BIO_get_shutdown` | `44M+SEbpNdQ` |
| `tls_accept_socket` | `4Q3bvOy4LY8` |
| `unknown` | `4pEl-5DmrrM` |
| `EVP_CIPHER_iv_length` | `4ryyfC3TknU` |
| `SSL_set_accept_state` | `5BoCLZK9mGk` |
| `BIO_meth_set_write` | `5DhOJmgra8E` |
| `BIO_meth_set_create` | `5NBoRJYLH0U` |
| `X509_CRL_free` | `5PWK0cYOpHs` |
| `i2d_SSL_SESSION` | `5aSlVeVSKVc` |
| `tls_config_error` | `5ex+694ri90` |
| `tls_read` | `7RvDjfy4gUA` |
| `BIO_meth_set_read` | `8A-gU2xQfjY` |
| `EC_POINT_set_affine_coordinates_GFp` | `8AGfaSvdDq4` |
| `SSL_CTX_set_max_early_data` | `8kct4CSWwOA` |
| `tls_configure` | `8pwP-7j5UZI` |
| `X509_STORE_CTX_get0_param` | `8wroYNJAQpo` |
| `X509_VERIFY_PARAM_set_flags` | `9E9Gr4FLIE4` |
| `unknown` | `A-Hs-8mau+8` |
| `ERR_load_X509_strings` | `A0lZtFjhcTw` |
| `tls_config_set_key_mem` | `A65b+ZhOt9k` |
| `BN_clear_free` | `ABcJv8S1KEM` |
| `BN_is_negative` | `AiNud8LR8B4` |
| `X509_cmp_current_time` | `Ax+Uu++rnrA` |
| `SSL_CTX_set_timeout` | `BBOgwZLvXHA` |
| `tls_config_new` | `BQNyLN2nA9w` |
| `X509_get_ext_by_NID` | `BRjfzlGULmU` |
| `EVP_PKEY_security_bits` | `BS5yWv6X-hg` |
| `X509_check_akid` | `C7eFtWPfLdk` |
| `SSL_set_quic_use_legacy_codepoint` | `CFRaBB2+R+s` |
| `SSL_CTX_set_default_verify_paths` | `DZ6fywqpaik` |
| `TLS_server_method` | `EtM5Jwx4EFo` |
| `ERR_lib_error_string` | `F7jknWesBTY` |
| `tls_close` | `Fqc0-3YCXjo` |
| `tls_client` | `GEK3LmjM9Q4` |
| `BIO_meth_set_ctrl` | `GFEaREp1UFw` |
| `EVP_PKEY_get0_EC_KEY` | `GY2X+VHs568` |
| `unknown` | `GjO5Ap3E-t4` |
| `EVP_MD_CTX_md` | `H1dRCduUy+o` |
| `unknown` | `HE-vihq-KKw` |
| `EVP_PKEY_verify` | `HEuvmk+aHs4` |
| `X509_set_version` | `IOutZPRNNPo` |
| `d2i_SSL_SESSION` | `IiG4HLr+cOw` |
| `DES_set_key_unchecked` | `J437jVn9pjI` |
| `EVP_PKEY_CTX_new_id` | `J5ZLHxWkT6k` |
| `RSAPublicKey_dup` | `JSqFTYGaWhU` |
| `AES_unwrap_key` | `JvoQz7GYOMo` |
| `ASN1_OCTET_STRING_cmp` | `Jzebx477Q3M` |
| `SSL_CTX_add1_chain_cert` | `L897DuAuXKk` |
| `X509_print` | `LA1MlgDx80o` |
| `tls_config_free` | `LL8N+X6DA-4` |
| `HMAC_CTX_new` | `LcxutSyn11o` |
| `X509_CRL_get0_by_cert` | `LdGe2IfIlpI` |
| `ERR_load_PEM_strings` | `M7ze0mz2jQM` |
| `tls_error` | `N+G3Gxequt0` |
| `OpenSSL_add_all_digests` | `O9Thca3enw8` |
| `EVP_PKEY_base_id` | `OqtHJ7QUZg8` |
| `unknown` | `PBYz0kpXwg8` |
| `X509_new` | `PNfwr3s6dEs` |
| `RSA_get0_factors` | `PcUZS7XHwHk` |
| `SSL_get0_verified_chain` | `QKIOxHfZLh8` |
| `RSA_set0_crt_params` | `QTV9Nyi7ryg` |
| `EVP_PKEY_derive` | `QYEjNnn6PT4` |
| `SSL_CIPHER_get_id` | `QaiPrM-RrrM` |
| `EVP_PKEY_sign` | `QhYzE+Jdvuk` |
| `X509_get_signature_nid` | `QnjD8S6Rgk4` |
| `ECDSA_SIG_new` | `R3NyckUcClU` |
| `BN_dup` | `RMhEgJs6KLI` |
| `EVP_sha512_256` | `Sea+FOS2GOI` |
| `tls_config_set_ca_mem` | `Sqn+7yQmQWc` |
| `EVP_PKEY_derive_init` | `Ssfu1yw4Em4` |
| `X509V3_EXT_conf_nid` | `TKGJOgbeyuA` |
| `X509_sign` | `TpBvY92mrSM` |
| `SSL_get_peer_quic_transport_params` | `U1dQZ1rXHdA` |
| `BIO_get_data` | `U3AsZ-9e2MI` |
| `SSL_CTX_set0_chain` | `UHkYqkyfwOs` |
| `EVP_PKEY2PKCS8` | `UT6E1YgI8mw` |
| `ASN1_TIME_new` | `UmAPG5bxYYk` |
| `X509_STORE_load_locations` | `V3JtebYui1A` |
| `ERR_func_error_string` | `VXuysR7ySzw` |
| `EVP_get_digestbyname` | `Wm24hhTLqWM` |
| `SSL_SESSION_set_time` | `XG3hIzt9VXY` |
| `X509_set_issuer_name` | `XK3FmaTS6pg` |
| `HMAC_CTX_free` | `YAxGqtnKQFY` |
| `X509_add_ext` | `YJ7bfxMHniE` |
| `tls_connect_socket` | `ZERQN8ewpgk` |
| `SSL_CTX_set_quic_method` | `ZFeTGo4ybcw` |
| `X509_gmtime_adj` | `ZQ3b6SakHkc` |
| `SSL_process_quic_post_handshake` | `aKGUCnoiE+k` |
| `EVP_CIPHER_nid` | `aLdgrI2jAF0` |
| `EC_POINT_clear_free` | `aSO3Svp7umY` |
| `tls_config_set_protocols` | `agQxMaXVE8o` |
| `d2i_PKCS8_PRIV_KEY_INFO` | `as+t2ZOg5fw` |
| `ECDSA_do_sign` | `ba4o0Ih6kSs` |
| `SSL_CTX_set_ciphersuites` | `bpOQdoGE2es` |
| `SSL_get_version` | `dL2V0os4TZ4` |
| `PEM_write_bio_PrivateKey` | `dQE7FIbNsUE` |
| `i2d_X509` | `e2ReciVG21w` |
| `RSA_get0_crt_params` | `eyrIagsax0w` |
| `BIO_set_data` | `f6Pa2rYH-dw` |
| `SSL_CTX_set_keylog_callback` | `fR2Wat3ssHs` |
| `d2i_X509_CRL` | `fVos2kAqfMI` |
| `tls_write` | `fZJr2OF+KGw` |
| `X509_ALGOR_get0` | `fdvZNUHm-dE` |
| `BIO_set_shutdown` | `fq1vAGKJNrI` |
| `ASN1_STRING_set` | `gavhu7SX2FI` |
| `PEM_read_bio_PUBKEY` | `gn7mRF71J4k` |
| `EVP_PKEY_assign` | `gp5dGS14-pI` |
| `OPENSSL_init_ssl` | `hGiyDq-l4AE` |
| `i2d_PKCS8_PRIV_KEY_INFO` | `iFlkTGEbE9k` |
| `BN_div` | `iHetf873n+4` |
| `X509_CRL_get_ext_d2i` | `je-ydGAbgFg` |
| `X509_digest` | `k1AHUWet-oU` |
| `X509V3_set_ctx` | `kO-jQ3HS9kE` |
| `BIO_meth_set_destroy` | `kPCJUQuguzE` |
| `BIO_set_flags` | `lLHlEb1nt5U` |
| `tls_config_set_ciphers` | `lTdt9OsPAV0` |
| `SSL_set_alpn_protos` | `lyk0UDADPAc` |
| `RSA_pkey_ctx_ctrl` | `mKlCMgWMU84` |
| `SSL_set_info_callback` | `mOQe5RQATT4` |
| `ASN1_get_object` | `mXrusLhI3CI` |
| `SSL_provide_quic_data` | `mhAgol4ETgY` |
| `tls_server` | `nCOh70mg3K0` |
| `BIO_meth_new` | `o0dpw6ZZ6K0` |
| `d2i_PUBKEY` | `o3Q8yQAMT-k` |
| `EVP_PKEY_derive_set_peer` | `ovH4WphWIQQ` |
| `OBJ_find_sigid_algs` | `pW329hq95Hc` |
| `unknown` | `pdxCm9+CCc4` |
| `EVP_CIPHER_key_length` | `qAG7o03bkkM` |
| `unknown` | `qGsc6uT6p1E` |
| `EVP_PKEY_verify_init` | `qseLIVbdvMQ` |
| `ERR_reason_error_string` | `rJZ-aiNIYCA` |
| `X509_add1_ext_i2d` | `s8WMaKNjwvE` |
| `EC_GROUP_cmp` | `sENJFZ7WbLU` |
| `EVP_PKEY_bits` | `ssruelYxprY` |
| `SSL_do_handshake` | `tJsbVTdWjT4` |
| `EVP_PKEY_sign_init` | `tZG2VIefbX0` |
| `EC_POINT_dup` | `uITUdhDDAg8` |
| `HKDF` | `uJ2ZXq4z3As` |
| `EVP_aes_128_ccm` | `uM9UnC8KFjY` |
| `RSA_set0_key` | `uihDBp74nLI` |
| `EVP_PKEY_CTX_free` | `ulzAPWCIBkw` |
| `tls_free` | `uz3bWkbHZu4` |
| `tls_config_verify_client` | `v1vHv4jOJxQ` |
| `EVP_PKEY_cmp` | `vLuIwBfrxrk` |
| `d2i_ASN1_SEQUENCE_ANY` | `vR0po3nBkBs` |
| `PKCS5_PBKDF2_HMAC` | `vnwZHU5FvBQ` |
| `ASN1_OCTET_STRING_free` | `vo-cv5gnPUA` |
| `tls_config_parse_protocols` | `vu4FgwGayHY` |
| `SSL_CTX_set_post_handshake_auth` | `wHdymWL7CbE` |
| `tls_config_insecure_noverifyname` | `wNQT6Zn4UG4` |
| `X509_EXTENSION_free` | `wbJDMMsje6s` |
| `ASN1_TYPE_free` | `whL1IRBxFZA` |
| `ASN1_TIME_diff` | `xBkcXV1OqWQ` |
| `ECDSA_SIG_set0` | `xVv3qlSRldc` |
| `OBJ_obj2txt` | `xZ1BME5qDv0` |
| `SSL_set_quic_transport_params` | `xf6anoZpWRM` |
| `X509_get0_pubkey` | `xj7cFYOV4+Y` |
| `SSL_is_quic` | `xpB+BNkO2Z0` |
| `X509_get_X509_PUBKEY` | `yLm956jsJpc` |
| `ECDSA_SIG_get0` | `z8fsXF8JuL0` |
| `EVP_MD_block_size` | `zCa75KLWWn4` |

- 119 of the 126 exports the 13.60 table lists only by NID carry, in the table above and in [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp), the LibreSSL names whose `tools/nid_names.py` hash equals the NID; the names were taken from the identifiers of the LibreSSL 3.8.4 and 4.0.0 `include`, `crypto`, `ssl` and `tls` sources. A name match does not show that the console function has LibreSSL's prototype or behavior; each keeps the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. `AES_wrap_key`, `ECDSA_SIG_free`, `ECDSA_do_verify`, `ERR_load_ERR_strings`, `ERR_print_errors_fp` and `i2d_PUBKEY` also hash to the NIDs of six `unknown` rows, which keep their NID names; `PBYz0kpXwg8` hashes to no identifier of those sources.
