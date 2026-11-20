# libSceIcu

## add Functional

- [BCP 47 IDs in `uloc_getBaseName` and `uloc_canonicalize`](../../../core/libs/prx/libSceIcu/LocaleCore.cpp) - an ID without `@` whose shortest subtag has one character is first read with `uloc_forLanguageTag`, as ICU 68.2 does ([uloc.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uloc.cpp#L477-L489)), so a `-u-` extension with a key throws here too. A conversion that does not fit ICU's 157-byte buffer returns `U_BUFFER_OVERFLOW_ERROR` with what ICU 68.2 leaves in the result, the ID after its language in upper case; ICU 78 converts at any length, and which one a console runs is unmeasured. Closes when: `uloc_forLanguageTag` maps `-u-` keywords and the long-ID result is compared with the console.
