# libSceIcu

## add Functional

- [Data memory accessors](../../../core/libs/prx/libSceIcu/DataInfo.cpp) (libSceIcu) - `udata_getMemory`, `udata_getRawMemory` and `udata_getLength` forward to ICU 78.1 the way `udata_getInfo` in the same file already does, so a `UDataMemory` opened by `udata_open` reports the pointer and length the console would; the `UDataMemory` and `DataHeader` layouts are asserted at compile time in that file. They cannot be exercised here: opening a data handle needs `icudt` data, which this build does not ship, so nothing in the test suite reaches them. The remaining `udata_*` exports still answer `NotImplemented_nid_no_patch`.
