# libSceLibreSsl

## change [OCSP_response_get1_basic](../../../core/libs/prx/libSceLibreSsl/Export.cpp#L294)

- [OCSP_response_get1_basic and OCSP_BASICRESP_free](../../../core/libs/prx/libSceLibreSsl/src/OcspBasicResponseDer.cpp) (libSceLibreSsl) - nothing is checked on the console, and LibreSSL's `Field=..., Type=...` error data is not kept. They follow LibreSSL 2.5.4 [ocsp_cl.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/ocsp/ocsp_cl.c#L197-L213), [asn_pack.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/asn1/asn_pack.c#L205-L215), the `OCSP_BASICRESP` items of `ocsp_asn.c`, `x_algor.c`, `x_exten.c`, and `tasn_dec.c`, `tasn_new.c`, `tasn_fre.c`, `a_bitstr.c` and `a_type.c`. A null response or response octets, null octet data with a non-zero length, contents over `INT_MAX` bytes and a BIT STRING with 32 or more unused bits (an undefined shift in LibreSSL) throw. A decoded list element whose push fails is freed where LibreSSL leaks it.

## change [OCSP_BASICRESP_free](../../../core/libs/prx/libSceLibreSsl/Export.cpp#L261)

- [OCSP_response_get1_basic responder ID by name](../../../core/libs/prx/libSceLibreSsl/src/OcspBasicResponseDer.cpp) (libSceLibreSsl) - a responder ID by name throws, because `X509_NAME` decoding ([x_name.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/asn1/x_name.c)) is not implemented; adding it is the follow-up. Responder IDs by key decode. `OCSP_BASICRESP_free` releases a name the caller stored with the `X509_NAME` release `GENERAL_NAME_free` uses.

## add Functional

- [OCSP_response_get1_basic and OCSP_BASICRESP_free embedded certificates](../../../core/libs/prx/libSceLibreSsl/src/OcspBasicResponseDer.cpp) (libSceLibreSsl) - a non-empty `certs` field throws when decoded, and freeing a basic response whose `certs` holds a certificate (for example one added with `OCSP_basic_add1_cert`) throws, because `d2i_X509` and `X509_free` are not implemented; [#13266](https://github.com/boykopovar/AnyPS5/pull/13266) implements them. An absent or empty `certs` field decodes and frees.

## change [OCSP_response_status, OCSP_single_get0_status](../../../core/libs/prx/libSceLibreSsl/src/OcspResponse.cpp)

- [OCSP_response_status, OCSP_single_get0_status](../../../core/libs/prx/libSceLibreSsl/src/OcspResponse.cpp) (libSceLibreSsl) - nothing is checked on the console. Layouts and results follow [LibreSSL 2.5.4](https://github.com/libressl/openbsd/tree/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto) `ocsp.h`, `ocsp_cl.c` and `a_enum.c`, the version in the [firmware 13.40 measurements](https://github.com/boykopovar/AnyPS5/discussions/8981#discussioncomment-19111214). `d2i_OCSP_RESPONSE` creates the responses `OCSP_response_status` reads; `OCSP_response_get1_basic` creates the single responses `OCSP_single_get0_status` reads. Enumerated values follow `ASN1_ENUMERATED_get` (0 for a null value or null data, -1 for another type or more than 8 bytes), truncated to `int`. A null response, a null certificate status, and a null revocation record when its time or reason is requested, which LibreSSL dereferences, throw
