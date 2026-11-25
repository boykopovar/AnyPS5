# libSceIcu

## change [ICU string search exports]

- [ICU string search exports](../../../core/libs/prx/libSceIcu/StringSearch.cpp) `usearch_open`, `usearch_getMatchedLength`, `usearch_reset`, `usearch_setOffset`, `usearch_setAttribute`, `usearch_setPattern`, `usearch_setText`, `usearch_next` and `usearch_getCollator` are firmware 13.42 exports implemented over pinned ICU 78.1. Handles and retained UTF-16 buffers are registered under locks; foreign handles, unreadable guest memory and null status pointers throw. The console close export has not been identified, so handles live until process teardown. Console behavior is unverified. Closes when: compared with the console, the close export is identified and borrowed collators interoperate with the guest collator API.
