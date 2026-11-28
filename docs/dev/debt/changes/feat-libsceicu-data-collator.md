# libSceIcu

## change [ICU collator exports]

- [ICU collator exports](../../../core/libs/prx/libSceIcu/Collator.cpp) are not compared with a console. `ucol_open`, `ucol_close`, `ucol_getAttribute`, `ucol_setAttribute`, `ucol_getStrength`, `ucol_setStrength`, `ucol_strcoll` and `ucol_strcollUTF8` call ICU 78.1 ([ucol.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/ucol.cpp)) over the root data and every locale tailoring built from `3rdparty/icu`. A handle `ucol_open` did not return or already closed, unreadable guest memory, a null `UErrorCode` and a locale ID without a terminator in 4096 bytes throw. `ucol_getReorderCodes` and `ucol_getTailoredSet` implement only ICU 68.2's early returns ([ucol.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/ucol.cpp) 372-382, 602-612). Closes when: compared with the console for the same locales.
