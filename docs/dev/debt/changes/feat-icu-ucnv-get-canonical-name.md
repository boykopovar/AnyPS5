# libSceIcu

## add Functional

- [`ucnv_getCanonicalName`](../../../core/libs/prx/libSceIcu/ConverterCanonicalName.cpp) uses the pinned ICU 78.1 alias-table implementation with guest-safe strings and status access. Lookups use the `cnvalias` resource of the bundled ICU data package, built from ICU 78.1's `convrtrs.txt`. The console uses ICU 68 and its alias data, and no hardware comparison is available. Closes when: ICU 68 converter alias data is built and the results are compared with the console.
