# libSceIcu

## add Functional

- [Resource helpers](../../../core/libs/prx/libSceIcu/ResourceHelpers.cpp) (libSceIcu) - `res_countArrayItems` follows ICU 68.2 `uresdata.cpp`: strings, binary, aliases, integers and integer vectors report 1, a zero offset and an unknown type report 0, and arrays, tables and their 16-bit and 32-bit forms read the count from the bundle's own `pRoot`/`p16BitUnits`. `res_getStringNoTrace`, `res_getBinaryNoTrace`, `res_getAlias`, `res_getIntVectorNoTrace`, `res_getResource`, `res_getArrayItem`, `res_getTableItemByIndex` and `res_getTableItemByKey` stay stubs: their payloads are pointers and lengths into pool bundles and table16/table32 data, and this build ships no ICU data to validate them against, so they throw instead of returning invented pointers. The `ResourceData` prefix read here (pRoot at 8, p16BitUnits at 16) is the ICU 68.2 x86-64 layout.
