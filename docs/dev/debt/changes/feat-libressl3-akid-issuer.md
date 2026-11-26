# libSceLibreSsl3

## add Functional

- [Authority Key Identifier issuer names](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtD2i.cpp) are unverified on PS5. The implicit issuer stack and explicit directory name follow [LibreSSL's AUTHORITY_KEYID](https://github.com/libressl/openbsd/blob/078b3d1df8ca2011c1605cd40418c58fa0f5b143/src/lib/libcrypto/x509/x509_akeya.c) and [GENERAL_NAME](https://github.com/libressl/openbsd/blob/078b3d1df8ca2011c1605cd40418c58fa0f5b143/src/lib/libcrypto/x509/x509_genn.c) layouts. OtherName, x400Address, ediPartyName, constructed strings and indefinite-length issuer fields remain unsupported. The exact malformed-input error queue is unverified against the console.
