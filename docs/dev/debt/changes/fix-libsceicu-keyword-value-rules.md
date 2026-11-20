# libSceIcu

## add Functional locale parsing and metadata

- [BCP 47 IDs in uloc_getKeywordValue](../../../core/libs/prx/libSceIcu/LocaleCore.cpp) - an ID with a `-u-` key throws. An ID with no `@` whose shortest subtag, the last one not counted, has one character goes through `uloc_forLanguageTag` first, as in ICU ([uloc.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uloc.cpp#L783-L788)), so that function's gaps apply. Its conversion is ICU 68.2's while the keyword rules are 78.1's, so results can differ from 78.1, for example for a `-u-` attribute list that does not fit 100 bytes, which fails with `U_ILLEGAL_ARGUMENT_ERROR`; 68.2's 157-byte limit on the converted ID is not applied. Closes when: compared with the console, including the `-u-` keyword mapping.
