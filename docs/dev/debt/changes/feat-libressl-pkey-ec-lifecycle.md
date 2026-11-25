# libSceLibreSsl

## change EVP_PKEY_free

- [EVP_PKEY_free](../../../core/libs/prx/libSceLibreSsl/src/PkeyLifecycle.cpp) - PS5 behavior has not been independently measured. A key that holds non-RSA/EC algorithm data, an engine or attributes throws and keeps its last reference, since no export can attach them yet. An RSA key is released with `RSA_free`. An EC key without an algorithm method is released with `EC_KEY_free`; an EC key with a method throws, since no export here assigns one. Reference counting and release follow LibreSSL 2.5.4 [p_lib.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/p_lib.c).

## change EVP_PKEY_get1_EC_KEY

- [EVP_PKEY_get1_EC_KEY](../../../core/libs/prx/libSceLibreSsl/src/PkeyLifecycle.cpp#L114) - not measured on a PS5. Null keys throw on dereference; non-EC types return null and queue `(6,0xfff,142)`; an EC key with null `EC_KEY` throws; a valid EC key increments its refcount and returns `EcKey*`, following LibreSSL 2.5.4 [p_lib.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/p_lib.c#L336-L345), [evp.h](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/evp/evp.h#L124-L129), [obj_mac.num](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/objects/obj_mac.num#L408) and [err.h](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/err/err.h#L163-L244).
