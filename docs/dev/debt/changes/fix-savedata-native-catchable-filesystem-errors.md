# libSceSaveData.native

## add Functional

- [sceSaveDataDirNameSearch, sceSaveDataDirNameSearch2, sceSaveDataConvert, sceSaveDataGetParam, sceSaveDataSetParam, sceSaveDataGetSaveDataMemory, sceSaveDataGetSaveDataMemory2](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - a host status, listing or `.param` read error other than a missing path leaves the export as a `filesystem_error` instead of an error code, because search, convert and the param calls run these checks outside any catch. The error code the console returns there is not measured. Closes when: measured on a PS5
- [sceSaveDataUmountWithBackup, sceSaveDataUmount2, sceSaveDataRestoreBackupData, sceSaveDataCheckBackupData, sceSaveDataDelete](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - on Windows, a `std::random_device`, `std::mutex`, string length or range error, or an allocation failure on these paths is still raised inside `libstdc++-6.dll`, where the module's catch sites cannot receive it, so it ends the process instead of returning `SAVE_DATA_ERROR_INTERNAL`. Only the `std::filesystem` throws are raised from the module. Labels: code, windows
