# libSceLibreSsl3

## add Functional

- [X509V3_EXT_conf_nid](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtConfNid.cpp) (libSceLibreSsl3) - nothing is checked on the console, and `SEQWRAP`, `SETWRAP`, `BITWRAP` and `OCTWRAP` without a `:` fail where LibreSSL wraps. It follows LibreSSL 4.0.0: `DER:` hex is the value; `ASN1:` encodes BOOLEAN, INTEGER, ENUMERATED, NULL, OBJECT, OCTET STRING, BIT STRING, IA5String and UTF8String in the four formats; plain values encode keyUsage, nsCertType and extendedKeyUsage lists. With a `ctx`, `SEQUENCE` and `SET` encode an empty value; a section fails with `OPERATION_NOT_DEFINED` without `ctx->db`, else throws. Other `ASN1:` tags not missing a value before a comma, any modifier with a `:`, a non-null `conf` with an `@section` value and plain values for other extension NIDs throw; a tagging modifier without a `:` and plain values for a non-extension NID return null. The critical flag is 0 or 1; errors are queued without file, line or data. Closes when: compared with the console.

## remove `TKGJOgbeyuA`
