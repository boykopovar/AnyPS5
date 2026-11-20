# libSceIcu

## change [ICU string search exports]

- [ICU string search exports](../../../core/libs/prx/libSceIcu/StringSearch.cpp) implement only the early returns of ICU 68.2 [usearch.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/usearch.cpp) for `usearch_open`, `usearch_next`, `usearch_reset`, `usearch_getCollator`, `usearch_getMatchedLength`, `usearch_setAttribute`, `usearch_setOffset`, `usearch_setPattern` and `usearch_setText`: a failing `UErrorCode` (above zero) or a null `UStringSearch` returns as ICU does, and the argument checks that precede object use set `U_ILLEGAL_ARGUMENT_ERROR`. Valid calls, a null `UErrorCode` and every path that needs `UStringSearch` objects and collation data throw `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.
