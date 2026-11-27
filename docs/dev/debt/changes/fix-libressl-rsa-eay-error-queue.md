# libSceLibreSsl

## add Functional

- [RSA_private_encrypt, RSA_private_decrypt](../../../core/libs/prx/libSceLibreSsl/src/Rsa.cpp) (libSceLibreSsl) - nothing is checked on the console. The default method slots follow [LibreSSL 2.5.4](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/rsa/rsa_eay.c) `rsa_eay.c` and its padding files, reason codes included. Two failures return -1 with an empty queue where LibreSSL queues errors: a blinding that cannot be set up or arithmetic that fails (`RSA_R_NO_PUBLIC_EXPONENT` or `ERR_R_BN_LIB`, the `BN` errors and `ERR_R_INTERNAL_ERROR`), and a negative length given to `RSA_private_decrypt` (`BN_R_BIGNUM_TOO_LONG` from `BN_bin2bn`)

## change [RSA_public_encrypt, RSA_public_decrypt]

- [RSA_public_encrypt, RSA_public_decrypt](../../../core/libs/prx/libSceLibreSsl/src/Rsa.cpp) (libSceLibreSsl) - nothing is checked on the console. They call the key method's slots; the default ones follow [LibreSSL 2.5.4](https://github.com/libressl/openbsd/tree/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/rsa) `rsa_eay.c`, `rsa_pk1.c`, `rsa_oaep.c`, `rsa_ssl.c`, `rsa_x931.c` and `rsa_none.c`: the modulus and exponent limits, encryption with PKCS#1 v1.5, SSLv23, OAEP (SHA-1, empty label) or no padding, decryption checking PKCS#1 v1.5 type 1, X9.31 or no padding, and -1 for other paddings; both queue the reason codes of those files. Random padding comes from host entropy and the arithmetic from mbedtls, whose 8192-bit limit makes a larger modulus throw; the Montgomery context is never cached. A method with its own `bn_mod_exp`, a negative `BIGNUM` and a null key, method or slot throw
