# libSceIcu

## add ICU string search pattern

- [usearch_setPattern](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and replaces the pattern on registered handles through pinned ICU 78.1. Guest UTF-16 is copied into handle-owned storage because ICU retains the buffer; null, empty, unreadable and invalid-length patterns are rejected. Console behavior is unverified. Closes when: compared with the console.
