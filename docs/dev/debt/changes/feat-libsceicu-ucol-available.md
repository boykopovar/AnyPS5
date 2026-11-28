# libSceIcu

## add Functional

- [Collator locale availability](../../../core/libs/prx/libSceIcu/CollatorAvailability.cpp) (libSceIcu) - what the console's ICU 68.2 answers is not measured. `ucol_countAvailable` and `ucol_getAvailable` follow ICU 78.1 ([coll.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/coll.cpp#L224-L252), [ucol_res.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/ucol_res.cpp#L550-L567)) and read `InstalledLocales` of the `coll` tree's `res_index` once per process, under the ICU 78.1 data name. They answer the 145 locales of the bundled ICU data package's `coll` tree unless a guest loads data of that name through `udata_setCommonData` before the first call; a title's ICU 68 data does not provide it. `u_cleanup` does not discard the list, where ICU reads it again. A negative index with a list present reads before the list in ICU and throws here.
