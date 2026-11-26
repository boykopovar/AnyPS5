# libSceLibreSsl3

## add Functional

- [d2i_X509_bio](../../../core/libs/prx/libSceLibreSsl3/src/KeyDer.cpp) (libSceLibreSsl3) - nothing is checked on the console. Reads one definite-length DER object from the BIO through the key decoder's reader, decodes it with the same certificate decoder the PKCS#7 path uses, and replaces the caller's output, freeing the previous certificate. A truncated or malformed certificate returns null, and trailing bytes stay in the BIO for the next read. Closes when: compared with the console.
