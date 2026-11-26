# libSceLibreSsl

## add Functional

- [X509_get_pubkey](../../../core/libs/prx/libSceLibreSsl/src/X509PubkeyGet.cpp) (libSceLibreSsl) - nothing is checked on the console. Returns the certificate's decoded `EVP_PKEY` with one added reference, as LibreSSL 4.0.0 `X509_PUBKEY_get` does, and null when the certificate or its public-key slot is empty. A public key that is present but not decoded throws, because the parent library has no SubjectPublicKeyInfo decoder. Closes when: compared with the console.
