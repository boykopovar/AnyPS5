# libSceIcu

## add ICU string search text

- [usearch_setText](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and replaces the searched text on registered handles through pinned ICU 78.1. Guest UTF-16 is copied into handle-owned storage because ICU retains the buffer; null, empty, unreadable and invalid-length text is rejected. Console behavior is unverified. Closes when: compared with the console.
