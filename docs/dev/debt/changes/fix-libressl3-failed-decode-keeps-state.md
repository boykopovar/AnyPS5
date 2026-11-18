# libSceLibreSsl3

## change [o2i_ECPublicKey]

- [o2i_ECPublicKey](../../../core/libs/prx/libSceLibreSsl3/src/EcAsn1.cpp) (libSceLibreSsl3) - nothing is checked on the console. A failed decode queues only `ERR_R_EC_LIB`, because libSceLibreSsl's `EC_POINT_oct2point` queues no reason of its own where [LibreSSL 4.0.0](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/ec/ec_asn1.c) first queues the decoder's; a compressed point on a curve whose prime is not 3 mod 4 throws there. Otherwise it follows `ec_asn1.c`: a missing key or group queues `ERR_R_PASSED_NULL_PARAMETER`, and success stores the first octet without its low bit as the conversion form and advances `*in` by `len`. Unlike LibreSSL, which keeps a new point after a failed decode and lets a rejected off-curve point overwrite the old one, a failed or faulting decode leaves the key, its point and `*in` unchanged, and a point of another curve is replaced. A null `in` throws. It works only on libSceLibreSsl keys.

## add Functional

- [d2i_ASN1_INTEGER](../../../core/libs/prx/libSceLibreSsl3/src/IntegerDecoder.cpp) (libSceLibreSsl3) - nothing is checked on the console. It follows LibreSSL `a_int.c` and `tasn_dec.c`, with the existing parent-library error queue. Unlike LibreSSL, which frees and clears an existing `*output` when the decode fails after the header, a failed or faulting decode leaves `*input` and `*output` unchanged; a valid decode stores the new integer first and frees the old one last, and an `*output` that is not a readable, writable integer throws. Content over `INT_MAX` bytes fails without an error, and the host buffer for the content grows only as far as the guest memory can be read.
