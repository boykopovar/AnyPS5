# libSceIcu

## add Functional

- [ULocale queries, keyword enumeration and locale-builder allocation](../../../core/libs/prx/libSceIcu/ULocale.cpp) (libSceIcu) - `ulocale_openForLanguageTag`, `ulocale_getBaseName`, `ulocale_getKeywordValue`, `ulocale_getKeywords`, `ulocale_getLanguage`, `ulocale_getRegion`, `ulocale_getScript`, `ulocale_getUnicodeKeywordValue`, `ulocale_getUnicodeKeywords`, `ulocale_getVariant` and `ulocbld_open` delegate to bundled ICU 78.1. Native keyword enumerations are adapted to the guest System V callback layout. `ulocbld_open` requires the dependent follow-up's `ulocbld_close` to release its result. Console behavior and binary compatibility with ICU 68.2 are unverified. Closes when: compared with the console.
