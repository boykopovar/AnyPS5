# libSceSaveData.native

## add Functional

- [sceSaveDataDelete, sceSaveDataMount, sceSaveDataMount2, sceSaveDataMount3, sceSaveDataCheckBackupData, sceSaveDataRestoreBackupData](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - probes guest parameters and directory names using `GuestCopyIn_nid_no_patch` and logs from validated local copies instead of raw guest pointers; an unmapped pointer returns `SAVE_DATA_ERROR_PARAMETER` while preserving caller errno.
