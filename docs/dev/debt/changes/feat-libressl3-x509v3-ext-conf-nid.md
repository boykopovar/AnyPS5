# libSceLibreSsl3

## add Functional

- [X509V3_EXT_conf_nid](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtConfNid.cpp) (libSceLibreSsl3) - nothing is checked on the console. It follows LibreSSL 4.0.0 `x509_conf.c`, `v3_utl.c` and `asn1_gen.c`: `DER:` hex becomes the extension octet value; `ASN1:` encodes BOOLEAN, INTEGER, ENUMERATED, NULL, OBJECT, OCTET STRING, BIT STRING, IA5String and UTF8String with the `ASCII`, `UTF8`, `HEX` and `BITLIST` formats; plain values encode the keyUsage, nsCertType and extendedKeyUsage bit and OID lists. Other `ASN1:` tags, the tagging modifiers, a non-null `conf` holding an `@section` value and plain values for other extension NIDs throw, while a plain value for a non-extension NID returns null. The critical flag is stored as 0 or 1, and X509V3 errors are queued without file, line or data. Closes when: compared with the console.

## remove `TKGJOgbeyuA`
