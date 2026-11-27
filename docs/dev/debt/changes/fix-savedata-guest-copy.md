# libSceSaveData.native

## add Functional

- [sceSaveDataDelete, sceSaveDataMount3, sceSaveDataMount2, sceSaveDataTransferringMount, sceSaveDataTransferringMountPs4, sceSaveDataDirNameSearchPs4, sceSaveDataCheckBackupData, sceSaveDataRestoreBackupData](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - reading and writing guest parameter, directory name, title ID, and result structs via `GuestCopyIn_nid_no_patch`/`GuestCopyOut_nid_no_patch` while preserving `errno`, and returning `SAVE_DATA_ERROR_PARAMETER` (`0x809F0003`) on unreadable or unwritable non-null pointers, is a host-safety fallback; console behaviour on unmapped pointers is unmeasured.
