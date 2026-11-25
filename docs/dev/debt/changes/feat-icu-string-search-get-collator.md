# libSceIcu

## add ICU string search collator

- [usearch_getCollator](../../../core/libs/prx/libSceIcu/StringSearch.cpp) is exported by firmware 13.42 and returns the pinned ICU 78.1 collator owned by a registered search. Null handles return null and foreign handles throw. The borrowed collator is not accepted by the separately registered `ucol_*` wrappers. Console behavior is unverified. Closes when: compared with the console and borrowed collators interoperate with the guest collator API.
