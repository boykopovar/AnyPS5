# libSceIcu

## add Functional

- [`ucnv_getCanonicalName`](../../../core/libs/prx/libSceIcu/ConverterCanonicalName.cpp) uses the pinned ICU 78.1 alias-table implementation with guest-safe strings and status access. The reduced bundled data has no `cnvalias` resource, so it reports the same missing-data error for every lookup until converter alias data is built. The console uses ICU 68 and its alias data, and no hardware comparison is available. Closes when: ICU 68 converter alias data is built and the results are compared with the console.
