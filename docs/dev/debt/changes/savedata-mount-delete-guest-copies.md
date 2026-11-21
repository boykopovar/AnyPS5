# libSceSaveData.native

## add Functional

- [sceSaveDataMount, sceSaveDataMount2, sceSaveDataMount3, sceSaveDataDelete](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - reading the mount, delete and directory name structures through `GuestCopyIn_nid_no_patch` and probing and writing the mount result structure through `GuestCopyOut_nid_no_patch`, returning `SAVE_DATA_ERROR_PARAMETER` on inaccessible or unwritable guest memory as host-safety fallbacks; console fault handling is unmeasured.
