# libSceIcu

## change [ICU string search exports](../../../core/libs/prx/libSceIcu/StringSearch.cpp)

- [ICU string search exports](../../../core/libs/prx/libSceIcu/StringSearch.cpp) implement only the early returns of ICU 68.2 ([usearch.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/i18n/usearch.cpp): `usearch_open` 2593-2631, `usearch_setOffset` 2806-2821, `usearch_setAttribute` 2835-2865, `usearch_getMatchedLength` 2937-2944, `usearch_setText` 2972-3001, `usearch_getCollator` 3067-3073, `usearch_setPattern` 3075-3097, `usearch_next` 3190-3296, `usearch_reset` 3386-3442). Non-null handles, valid calls, a null `UErrorCode` and every path that needs `UStringSearch` objects and collation data throw `NotImplemented_nid_no_patch`. Null handles, failing statuses and the argument errors return as in ICU. Unverified on a console. Closes when: those are implemented and compared with the console.
