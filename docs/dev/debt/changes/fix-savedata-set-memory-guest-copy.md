# libSceSaveData.native

## add Functional

- [sceSaveDataSetSaveDataMemory2](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - probes the save-data set parameter, data array and parameter structure using `GuestCopyIn_nid_no_patch`; an unmapped guest pointer returns `SAVE_DATA_ERROR_PARAMETER` (`0x809F0000`) while preserving guest errno, matching the parameter rejection behavior of other save-data exports.
