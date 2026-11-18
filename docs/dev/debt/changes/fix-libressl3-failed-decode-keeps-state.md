# libSceLibreSsl3

## change [d2i_ASN1_INTEGER, d2i_ECParameters, o2i_ECPublicKey]

- [d2i_ASN1_INTEGER, d2i_ECParameters, o2i_ECPublicKey](../../../core/libs/prx/libSceLibreSsl3/src/EcAsn1.cpp) - a failed or faulting decode leaves the caller's cursor, output integer, key, group and point unchanged, and a new object is attached only after the input has been checked. Upstream LibreSSL frees and clears an existing output integer when the decode fails after the header, and keeps the coordinates of a point whose encoding was rejected. The console behaviour on those failures is unmeasured, so a title that relies on either would see the old object instead. Two cases keep the upstream result: `d2i_ECParameters` still advances the cursor when it decoded a valid curve name or NULL that it cannot turn into a group, and `ASN1_get_object` still stores the header it parsed when the content is longer than the input.
