# libSceIcu

## add ICU string search offset

- [usearch_setOffset](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and delegates offsets on registered handles to pinned ICU 78.1, including its bounds validation and `UErrorCode` result. Console behavior is unverified. Closes when: compared with the console.
