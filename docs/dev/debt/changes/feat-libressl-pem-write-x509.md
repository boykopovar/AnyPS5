# libSceLibreSsl

## add Functional

- [PEM_write_bio_X509](../../../core/libs/prx/libSceLibreSsl/src/PemWriteX509.cpp) (libSceLibreSsl) - nothing is checked on the console. Follows LibreSSL 2.5.4 `pem_lib.c`/`pem_all.c`: it writes the certificate's cached DER encoding as a `-----BEGIN CERTIFICATE-----` line, 64-character base64 lines and the matching footer, and returns 1 when every byte was written. A certificate without a cached, unmodified encoding throws because the parent library has no X509 encoder yet. Closes when: compared with the console.
