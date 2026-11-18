# libSceSaveData.native

## add Functional

- [sceSaveDataGetSaveDataMemory2, sceSaveDataGetAllSize, sceSaveDataGetMountedSaveDataCount, sceSaveDataGetSaveDataCount](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - validates guest output and parameter pointers upfront with `GuestWritable_nid_no_patch` and `GuestCopyIn_nid_no_patch` while preserving caller errno, returning `SAVE_DATA_ERROR_PARAMETER` on unmapped or unwritable addresses before performing state mutation or disk I/O.
