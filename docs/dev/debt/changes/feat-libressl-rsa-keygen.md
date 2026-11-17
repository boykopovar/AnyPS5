# libSceLibreSsl

## add Unknown function info

- [RSA_generate_key_ex](../../../core/libs/prx/libSceLibreSsl/src/RsaKeygen.cpp) (libSceLibreSsl) - nothing is checked on the console. A non-null `BN_GENCB`, an even or negative exponent (LibreSSL never returns for even ones) and primes over 8192 bits (mbedtls limit) throw. A BIGNUM that cannot grow, or a failed `BN_new`, queues only the RSA `ERR_LIB_BN` reason, without LibreSSL's preceding `bn_lib.c` one. Miller-Rabin runs in mbedtls with host entropy. The rest follows [rsa_gen.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/rsa/rsa_gen.c) and `probable_prime` in [bn_prime.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/bn/bn_prime.c), including the sieve, round counts, the three-equal-primes error and the `p > q` pointer swap.
