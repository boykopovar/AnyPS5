# libSceLibreSsl3

## add Functional

- [RSA_size, RSA_flags, RSA_public_encrypt, RSA_public_decrypt, RSA_private_encrypt, RSA_private_decrypt, RSA_sign, RSA_verify, RSA_padding_add_PKCS1_PSS and RSA_verify_PKCS1_PSS](../../../core/libs/prx/libSceLibreSsl3/src/RsaCrypto.cpp) (libSceLibreSsl3) - not measured on a PS5. A key whose method pointer is set goes to libSceLibreSsl through its `_nid_no_patch` helpers, because only that library's `RSA_new` sets one; keys from this module's `RSA_new`, which leaves it null, use this module's RSA code, and an unreadable key throws. `RSA_flags` returns 0 for a native key, as libSceLibreSsl's default method has no flags. The PSS exports pass a null mask digest, so MGF1 uses the message digest. `RSA_generate_key_ex` still forwards to libSceLibreSsl, which throws for a native key's null method.
