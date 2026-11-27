# libSceIcu

## add ICU string search open

- [usearch_open](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and delegates valid searches to pinned ICU 78.1 [usearch.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/usearch.cpp). Pattern and text are copied from guest memory into handle-owned buffers because ICU retains them. A registered break iterator is cloned so its binding and lifetime remain caller-owned. Locale IDs must terminate within 4096 bytes. Invalid, unreadable or oversized inputs throw or set `U_ILLEGAL_ARGUMENT_ERROR` according to the existing guest-pointer contract. Search handles remain registered until `u_cleanup` closes them because the console close export has not been identified. Console behavior is unverified. Closes when: compared with the console and the close export is identified.
