# libSceLibreSsl3

## add Functional

- [ASN.1 key parser](../../../core/libs/prx/libSceLibreSsl3/src/KeyDer.cpp) - negative INTEGERs, constructed primitive tags, optional context-tag errors and outer-SEQUENCE error wrapping still differ from [LibreSSL 4.0.0](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/asn1/tasn_dec.c#L604-L639). The custom DER reader is not a general BER decoder; console error-queue parity is unverified. Regression coverage of traditional-key fallback checks missing fields, wrong tags, empty magnitudes and 64-bit version decoding. Closes when: the remaining parser paths are compared with the pinned source and the console.
