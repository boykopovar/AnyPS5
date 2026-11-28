# libSceIcu

## add ICU string search next

- [usearch_next](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and advances registered searches through pinned ICU 78.1, returning UTF-16 offsets and `USEARCH_DONE` with ICU's `UErrorCode` behavior. Console behavior is unverified. Closes when: compared with the console.
