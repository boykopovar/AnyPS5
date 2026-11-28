# libSceLibreSsl3

## add Functional

- [PEM_read_RSAPrivateKey](../../../core/libs/prx/libSceLibreSsl3/src/PemKeys.cpp) has no console verification and inherits unsupported encrypted PEM, DSA and RSA cleanup paths. It follows [LibreSSL 4.0.0 pem_all.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/pem/pem_all.c#L280-L305): a non-closing BIO wraps the guest FILE, the native PEM reader decodes the key, and EVP_PKEY_get1_RSA obtains an independent RSA reference. A failed BIO allocation queues ERR_R_BUF_LIB. A null FILE throws invalid_argument; an unreadable or unwritable output slot throws. Replacing an output uses native RSA_free and transfers the decoded reference to the caller.
