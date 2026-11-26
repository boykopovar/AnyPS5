# libSceLibreSsl

## add Functional

- [i2d_X509_PUBKEY](../../../core/libs/prx/libSceLibreSsl/src/X509PubkeyEncode.cpp) (libSceLibreSsl) - nothing is checked on the console. Encodes the modelled `X509_PUBKEY` (the algorithm object with its optional parameter, then the public-key BIT STRING) as DER, returning its length and either allocating the output or advancing the caller's pointer as the `i2d_*` functions do. Parameters are encoded for NULL, OBJECT, BOOLEAN, INTEGER, BIT STRING and OCTET STRING; another type throws, as does an algorithm without its encoded object. Closes when: compared with the console.
