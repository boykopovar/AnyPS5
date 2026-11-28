# libSceSaveData.native

## add Functional

- [sceSaveDataUmount2](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - the console effect of bit 0 (`SAVE_DATA_UMOUNT_MODE_BACKUP_FORCE`) is not known: it is accepted with or without bit 16 and ignored, as [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/a1fc5ae438ea343480c9850f585d8c0afed06241/src/libs/libSaveData.cpp#L973-L997) acts on bit 16 only. PPSA08668 sets it when it unmounts a save it mounted with `SAVE_DATA_MOUNT_MODE_CREATE2`; PPSA15552 passes `0x1`. Bit 16 (`SAVE_DATA_UMOUNT_MODE_BACKUP`) copies the backup synchronously and delivers `SAVE_DATA_EVENT_TYPE_UMOUNT_BACKUP` as `sceSaveDataUmountWithBackup` does, unmodified saves included; any other bit throws. Unlike `sceSaveDataUmountWithBackup`, it does not require `sceSaveDataInitialize3` before returning `SAVE_DATA_ERROR_NOT_MOUNTED`. Closes when: measured on a PS5. Labels: console, tested.
