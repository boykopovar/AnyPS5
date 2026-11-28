# libSceIcu

## add ICU string search reset

- [usearch_reset](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and resets registered search handles through pinned ICU 78.1. Null handles are ignored; foreign handles throw. Console behavior is unverified. Closes when: compared with the console.
