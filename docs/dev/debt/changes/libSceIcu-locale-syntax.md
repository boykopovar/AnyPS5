# libSceIcu

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceIcu/Export.cpp) - The remaining throwing functions match NIDs in the public 3.20 catalogue and both version-labelled images; neither image label authenticates retail firmware provenance. Their prototypes and behavior are unverified and retain `NotImplemented_nid_no_patch`.

## add Functional

- [Locale variant extraction](../../../core/libs/prx/libSceIcu/LocaleSyntax.cpp) has no console ABI or behavior verification. `uloc_getVariant` follows pinned ICU 78.1 [uloc.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uloc.cpp), including partial writes and termination warnings. A null locale names the guest default locale from `uloc_getDefault`, as in ICU. A null status or null destination with positive capacity throws `std::invalid_argument` where ICU dereferences it. ICU 78.1 may differ from the ICU 68 symbols in the PS5 3.20 inventory. Closes when compared with the console.

- [Locale keyword editing](../../../core/libs/prx/libSceIcu/LocaleSyntax.cpp) has no console ABI or behavior verification. `uloc_setKeywordValue` follows the same pinned ICU `uloc.cpp`: null/empty values remove keywords, and overflow preserves the buffer while returning the required length. Null status pointers and null buffers after key/capacity validation throw `std::invalid_argument` where ICU dereferences them. Other invalid memory and unterminated strings retain native input requirements. Closes when compared with console behavior.

- [Locale name and language-tag conversion](../../../core/libs/prx/libSceIcu/Export.cpp) exports `uloc_getName` and `uloc_toLanguageTag` remain throwing. Their BCP 47 mappings require `keyTypeData` absent from this build; ICU's fallback can silently omit mappings. Closes when resource-backed mappings are available and verified.
