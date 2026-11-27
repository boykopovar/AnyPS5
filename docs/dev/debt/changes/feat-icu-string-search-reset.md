# libSceIcu

## add ICU string search reset

- [usearch_reset](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and resets registered search handles through pinned ICU 78.1. Null handles are ignored; foreign handles throw. If allocation fails while rebuilding after a borrowed collator changes, the wrapper preserves the prior collation-element tables and `usearch_next` reports `U_MEMORY_ALLOCATION_ERROR` until a later reset or pattern replacement succeeds. Console behavior is unverified. Closes when: compared with the console.
