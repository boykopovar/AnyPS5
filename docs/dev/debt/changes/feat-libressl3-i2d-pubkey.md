# libSceLibreSsl3

## add Functional

- [i2d_PUBKEY](../../../core/libs/prx/libSceLibreSsl3/src/I2dPubkey.cpp) (libSceLibreSsl3) - nothing is checked on the console; the DER is built by hand and the return values follow LibreSSL 4.0.0 (`ce488b78e0`), where a failed `X509_PUBKEY_set` returns -1: a key without an ASN.1 method, a failed encoder and a key whose method has no public-key encoder all return -1, and the HMAC key queues `X509_R_METHOD_NOT_SUPPORTED`. RSA-PSS, DSA and curves other than NIST P-256 and P-384 throw `NotImplemented_nid_no_patch`, and every EC key is encoded with a named-curve algorithm identifier. Closes when: compared with the console.

## remove `A-Hs-8mau+8`
