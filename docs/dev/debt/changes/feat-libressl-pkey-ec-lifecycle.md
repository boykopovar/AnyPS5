# libSceLibreSsl

## change EVP_PKEY_free

- [EVP_PKEY_free](../../../core/libs/prx/libSceLibreSsl/src/PkeyLifecycle.cpp#L88-L100) - not measured on PS5. Releasing the last reference of a key with an unknown `ameth`, a non-null payload without a supported method, an `engine` or `attributes` throws and restores the reference; real producers bind a supported RSA or EC method, releasing it via `RSA_free` or `EC_KEY_free`. Refcounting and release follow LibreSSL 2.5.4 [`p_lib.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/p_lib.c#L402-L433).

## change EVP_PKEY_get1_EC_KEY

- [EVP_PKEY_get1_EC_KEY](../../../core/libs/prx/libSceLibreSsl/src/PkeyLifecycle.cpp#L133-L144) - not measured on PS5; null keys or EC keys with null `EcKey*` throw where source dereferences them. Non-EC keys queue (6,0xfff,142) and return null; valid EC keys add a reference and return `EcKey*`, per LibreSSL 2.5.4 [`p_lib.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/p_lib.c#L336-L345), [`evp.h` struct](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/evp.h#L124-L143), [NID](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/objects/obj_mac.num#L408), [reason](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/evp.h#L1453) and [`err.h`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/err/err.h#L163-L244).
