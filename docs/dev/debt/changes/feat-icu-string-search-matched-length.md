# libSceIcu

## add ICU string search matched length

- [usearch_getMatchedLength](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and delegates registered search handles to pinned ICU 78.1. Null handles return `USEARCH_DONE`; foreign handles throw. Console behavior is unverified. Closes when: compared with the console.
