# libSceIcu

## add Functional

- [POSIX variant after `@`](../../../core/libs/prx/libSceIcu/LocaleCore.cpp) - when the text after `@` has no `=`, `uloc_canonicalize` appends it as written and `uloc_getBaseName` drops it. ICU 68.2 reads it as a POSIX variant ([uloc.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uloc.cpp#L1578-L1611)): `uloc_canonicalize` turns `no@ny` into `no__NY` and `en_GB@EURO` into `en_GB_EURO`, and `uloc_getBaseName` keeps `mr@FOO` whole. Closes when: the variant is written as ICU does.
