# libSceLibreSsl3

## add Functional

- [d2i_ECParameters](../../../core/libs/prx/libSceLibreSsl3/src/EcAsn1.cpp) (libSceLibreSsl3) - nothing is checked on the console. Explicit curve parameters throw, as libSceLibreSsl groups hold only named curves, and so do LibreSSL 4.0.0 named curves that mbedtls lacks. Errors are queued without file, line or data. Otherwise it follows [LibreSSL 4.0.0](https://github.com/libressl/openbsd/tree/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto) `ec_asn1.c`, `tasn_dec.c` and `a_object.c`: BER header, CHOICE and OID errors queue the same reasons, a named curve's group gets `OPENSSL_EC_NAMED_CURVE`, and an OID outside the 4.0.0 curve list or implicitlyCA fails after advancing `*in`. It works only on libSceLibreSsl keys.
