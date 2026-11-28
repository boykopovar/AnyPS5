# libSceIcu

## add ICU string search attributes

- [usearch_setAttribute](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and delegates all three search attributes and their values to pinned ICU 78.1. Invalid attribute and sentinel values retain ICU's status behavior. Console behavior is unverified. Closes when: compared with the console.
