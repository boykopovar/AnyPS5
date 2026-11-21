# libSceIcu

## change [Three-letter language and region codes]

- [Three-letter language and region codes](../../../core/libs/prx/libSceIcu/LocaleCore.cpp) - the locale getters shorten a three-letter language or region subtag to its two-letter code with the `LANGUAGES_3` and `COUNTRIES_3` tables of [ICU 68.2](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/uloc.cpp#L1141-L1250), the version of the 3.20 module; only the pairs whose codes differ are kept. ICU 78.1, which firmware 13.42 reports, also shortens the regions `CRQ`, `DGA`, `XEA`, `XIC` and `XKK`; which tables a console uses is unmeasured.
