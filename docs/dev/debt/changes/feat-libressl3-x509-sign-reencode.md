# libSceLibreSsl3

## add Functional

- [X509_sign](../../../core/libs/prx/libSceLibreSsl3/src/X509Sign.cpp) (libSceLibreSsl3) - not checked on a console. It re-encodes `X509_CINF` DER from its fields when structured fields are present, or updates only the cached signature `AlgorithmIdentifier` when absent. Follows LibreSSL 4.0.0 `x_all.c`, `asn1_item.c`, `x_x509.c`, `a_int.c`, `a_bitstr.c`, `x_name.c`, `x_val.c`, `x_pubkey.c` and `x_exten.c`; omits version only when null, emits empty extension sequences, strips trailing zero bits in bit strings, sorts multi-entry RDNs, checks guest writability before mutating, and refreshes `enc.bytes` with `modified=0`. Ed25519 (null md), unmodelled `X509_NAME` value types, unmodelled `ASN1_TYPE` parameters, and unsupported key/digest pairs throw `NotImplemented_nid_no_patch`. Closes when: compared with the console.
