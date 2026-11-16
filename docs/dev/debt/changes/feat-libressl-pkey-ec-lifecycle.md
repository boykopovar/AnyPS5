# libSceLibreSsl

## change EVP_PKEY_free

- [EVP_PKEY_free](../../../core/libs/prx/libSceLibreSsl/src/PkeyLifecycle.cpp) - PS5 behavior has not been independently measured. A key that holds non-RSA/EC algorithm data, an engine or attributes throws and keeps its last reference, since no export can attach them yet. An RSA key is released with `RSA_free` and an EC key is released with `EC_KEY_free`. Reference counting and release follow LibreSSL 2.5.4 [p_lib.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/p_lib.c).

## change EVP_PKEY_get1_EC_KEY

- [EVP_PKEY_get1_EC_KEY](../../../core/libs/prx/libSceLibreSsl/src/PkeyLifecycle.cpp#L111) not measured on a PS5: null keys throw on source dereference; non-EC types return null and queue error (6,0xfff,142); an EC key with null EC_KEY throws; a valid EC key increments its reference count and returns the `EcKey*`, following [LibreSSL 2.5.4 `p_lib.c`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/p_lib.c#L336-L345), [`evp.h` struct](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/evp.h#L124-L129), [NID](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/objects/obj_mac.num#L408), [reason](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/evp.h#L1453), and [`err.h`](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/err/err.h#L163-L244).
