# libSceIcu

## add Functional

- [BiDi reverse writing, case-map configuration and locale objects](../../../core/libs/prx/libSceIcu/CaseMapping.cpp) (libSceIcu) - `ubidi_writeReverse`, `ucasemap_close`, `ucasemap_getLocale`, `ucasemap_getOptions`, `ucasemap_setLocale`, `ucasemap_setOptions`, `ucasemap_utf8ToLower`, `ulocale_openForLocaleID`, `ulocale_close`, `ulocale_getLocaleID` and `ulocale_isBogus` delegate to bundled ICU 78.1. The object APIs expose bundled ICU objects through the guest ABI; null case-map handles and null status pointers throw `std::invalid_argument`. Console behavior and binary compatibility with ICU 68.2 are unverified. Closes when: compared with the console.
