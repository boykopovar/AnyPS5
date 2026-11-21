# libSceIcu

## remove | `uloc_toLanguageTag` | `zmMWRKwedmw` |

## change [BCP 47 language tag parsing](../../../core/libs/prx/libSceIcu/LanguageTag.cpp)

- [BCP 47 language tag parsing](../../../core/libs/prx/libSceIcu/LanguageTag.cpp) is unverified on a console. `uloc_forLanguageTag` follows ICU 68.2, the version of the 3.20 module (its C++ names are in `icu_68`; firmware 13.42 reports ICU 78.1 and whether it exports this NID is unverified), [uloc_tag.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uloc_tag.cpp), including its `_POSIX` suffix after the variants when a non-`u` extension or private use follows a variant, which ICU 74 no longer writes. The keys and types of a `-u-` extension are mapped with this module's `uloc_toLegacyKey` and `uloc_toLegacyType`, whose tables are ICU 78.1's, newer than the 68.2 the parser follows. Closes when: compared with the console, including the `-u-` keyword mapping.

## add Functional

- [BCP 47 language tag writing](../../../core/libs/prx/libSceIcu/LanguageTag.cpp) is unverified on a console. `uloc_toLanguageTag` follows ICU 68.2 [uloc_tag.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uloc_tag.cpp#L2588-L2722) over this module's `uloc_canonicalize`, `uloc_getLanguage`, `uloc_getScript` and `uloc_getCountry`, so their gaps listed above apply to it. An ID without `@` whose shortest subtag has one character is first read with `uloc_forLanguageTag`, as ICU 68.2 canonicalization does, at any length; a tag that conversion rejects returns `U_ILLEGAL_ARGUMENT_ERROR`. A keyword other than `attribute` or a one-character extension is mapped with this module's `uloc_toUnicodeLocaleKey` and `uloc_toUnicodeLocaleType`, whose tables are ICU 78.1's; a `POSIX` variant becomes `-u-va-posix`. `strict` is read as one signed byte. Closes when: compared with the console, including the keyword mapping.
