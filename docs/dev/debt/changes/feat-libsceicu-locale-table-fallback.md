# libSceIcu

## add Functional

- [Locale table string fallback](../../../core/libs/prx/libSceIcu/LocaleTableFallback.cpp) - no console ABI or behavior verification; the seven-argument prototype follows [ICU 78.1 ulocimp.h](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ulocimp.h) and is not taken from a title. `uloc_getTableStringWithFallback` forwards to `locresdata.cpp`, built into `icu_common`, with checked guest reads of the path, locale and keys and checked writes of the length and status; the result is ICU's borrowed resource string. Table-level fallback, deprecated code replacement and `Fallback` entries follow upstream with this build's data; a nonwritable status or length throws. `ures_open` sees only this host library's data, so console bundles can differ. Closes when: compared with the console.
