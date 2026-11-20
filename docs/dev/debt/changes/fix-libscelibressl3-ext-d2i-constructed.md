# libSceLibreSsl3

## add Functional

- [X509V3_EXT_d2i constructed strings](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtD2i.cpp) (libSceLibreSsl3) - not compared with the console. A length field of more than 8 octets is still rejected, where LibreSSL accepts it; this is the only known difference from 4.0.0. The rest follows LibreSSL 4.0.0 [`asn1_collect`](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/tasn_dec.c#L180-L231), read but not run; 3.7.3 and the LibreSSL main branch read the same on this path. It differs from the measured 3.4.1 submodule: a definite length does not bound the fragments read, constructed ENUMERATED and OBJECT values give `TYPE_NOT_PRIMITIVE` (231) where 3.4.1 accepts them, and constructed GeneralizedTime content is validated where 3.4.1 validates no GeneralizedTime. Closes when: compared with the console.
