# libSceLibreSsl3

## change [`X509V3_EXT_d2i`]

- [`X509V3_EXT_d2i`](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtD2i.cpp) (libSceLibreSsl3) - console ABI and allocator unverified. Prototype and NID dispatch follow LibreSSL 4.0.0 `x509_lib.c`, `v3_lib.c` and `ext_dat.h`. Bit/octet string, integer, enumerated, IA5, time, object, OCSP nonce, AKI (90, 24-byte `AUTHORITY_KEYID`), general names (85, 86), info access (177, 398) and CRL distribution points (103, 857) decode; otherName, ediPartyName, constructed keyid/time strings and non-string indefinite lengths throw. Standard methods 89, 290, 291, 367, 368, 371, 401, 666, 747, 770, 771, 1018 and 1021 throw `NotImplemented_nid_no_patch`. Malformed values return null with 4.0.0's queue for tested vectors; directoryName keeps shorter nesting and unverified allocation queues. Null inputs throw where upstream dereferences them; negative lengths return null. Closes when: compared with the console.
