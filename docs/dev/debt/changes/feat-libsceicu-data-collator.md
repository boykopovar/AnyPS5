# libSceIcu

## change [ICU collator exports]

- [ICU collator exports](../../../core/libs/prx/libSceIcu/Collator.cpp) are not compared with a console. Implemented exports call ICU 78.1 ([source](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/ucol.cpp)) over root data and all locale tailorings from `3rdparty/icu`. Accessors and comparisons accept owned `ucol_open` handles and borrowed `usearch_getCollator` handles; `ucol_close` accepts only owned handles. Other or closed handles, unreadable guest memory, a null `UErrorCode` and locale IDs without a terminator in 4096 bytes throw. `ucol_getReorderCodes` and `ucol_getTailoredSet` implement only ICU 68.2's early returns ([source](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/ucol.cpp) 372-382, 602-612). Closes when: compared with the console for the same locales.
