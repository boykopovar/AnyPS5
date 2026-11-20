# libSceSaveData.native

## add Functional

- [sceSaveDataUmount2](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - modes `0x10000` (`SAVE_DATA_UMOUNT_MODE_BACKUP`) and `0x10001` (`SAVE_DATA_UMOUNT_MODE_BACKUP | SAVE_DATA_UMOUNT_MODE_BACKUP_FORCE`) synchronously copy the save backup and queue and deliver a `SAVE_DATA_EVENT_TYPE_UMOUNT_BACKUP` (`1`) event identically to `sceSaveDataUmountWithBackup`, without distinguishing forced backups from ordinary backup unmounts or skipping unmodified saves. Mode `0` unmounts without backing up or queuing an event, and any other mode throws `std::runtime_error`. Unlike `sceSaveDataUmountWithBackup`, `sceSaveDataUmount2` does not require `sceSaveDataInitialize3` before returning `SAVE_DATA_ERROR_NOT_MOUNTED`. Closes when: measured on a PS5. Labels: console, tested.
